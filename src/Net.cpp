#include "Net.hpp"

#include "PlayerStats.hpp"  // xpForLevel (snapshot next-level threshold)

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

namespace game::net {

namespace {

// Writing to a socket whose peer has gone raises SIGPIPE, which by default kills
// the process. Ignore it once — we detect the dead peer via send/recv return
// values instead (and reconnect).
void ignoreSigpipeOnce() {
    static bool done = false;
    if (!done) {
        std::signal(SIGPIPE, SIG_IGN);
        done = true;
    }
}

// --- little serialization helpers (native-endian; LAN same-arch prototype) ---

void putBytes(std::vector<uint8_t>& b, const void* p, std::size_t n) {
    const auto* c = static_cast<const uint8_t*>(p);
    b.insert(b.end(), c, c + n);
}
void putI32(std::vector<uint8_t>& b, int32_t v) { putBytes(b, &v, 4); }
void putF32(std::vector<uint8_t>& b, float v) { putBytes(b, &v, 4); }
void putStr(std::vector<uint8_t>& b, const std::string& s) {
    putI32(b, static_cast<int32_t>(s.size()));
    putBytes(b, s.data(), s.size());
}

struct Reader {
    const uint8_t* p;
    std::size_t n;
    std::size_t off = 0;
    bool ok = true;

    void get(void* dst, std::size_t len) {
        if (off + len > n) {
            ok = false;
            return;
        }
        std::memcpy(dst, p + off, len);
        off += len;
    }
    int32_t i32() {
        int32_t v = 0;
        get(&v, 4);
        return v;
    }
    float f32() {
        float v = 0;
        get(&v, 4);
        return v;
    }
    std::string str() {
        int32_t len = i32();
        if (!ok || len < 0 || off + static_cast<std::size_t>(len) > n) {
            ok = false;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(p + off), len);
        off += len;
        return s;
    }
};

// --- non-blocking socket I/O ---

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Frame = [uint32 length][uint8 type][payload]; length counts type + payload.
std::vector<uint8_t> frame(MsgType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    const uint32_t len = static_cast<uint32_t>(payload.size() + 1);
    putBytes(out, &len, 4);
    out.push_back(static_cast<uint8_t>(type));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

void sendAll(int fd, const std::vector<uint8_t>& bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const ssize_t n = ::send(fd, bytes.data() + sent, bytes.size() - sent, 0);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
        } else if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;  // LAN, small messages: just spin briefly
        } else {
            return;  // peer gone; caller detects on read
        }
    }
}

// Pull whatever bytes are available into conn.in. Returns false if the peer
// closed the connection.
bool recvInto(Connection& conn) {
    uint8_t tmp[4096];
    for (;;) {
        const ssize_t n = ::recv(conn.fd, tmp, sizeof(tmp), MSG_DONTWAIT);
        if (n > 0) {
            conn.in.insert(conn.in.end(), tmp, tmp + n);
        } else if (n == 0) {
            return false;  // orderly close
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            if (errno == EINTR) continue;
            return false;  // real error
        }
    }
}

// Extract the next complete message from conn.in, if any.
bool nextMessage(Connection& conn, MsgType& type, std::vector<uint8_t>& payload) {
    if (conn.in.size() < 4) return false;
    uint32_t len = 0;
    std::memcpy(&len, conn.in.data(), 4);
    if (conn.in.size() < 4 + len || len < 1) return false;
    type = static_cast<MsgType>(conn.in[4]);
    payload.assign(conn.in.begin() + 5, conn.in.begin() + 4 + len);
    conn.in.erase(conn.in.begin(), conn.in.begin() + 4 + len);
    return true;
}

}  // namespace

// --- encode / decode ---------------------------------------------------------

std::vector<uint8_t> encodeWelcome(int playerId) {
    std::vector<uint8_t> b;
    putI32(b, playerId);
    return b;
}

bool decodeWelcome(const std::vector<uint8_t>& p, int& playerId) {
    Reader r{p.data(), p.size()};
    playerId = r.i32();
    return r.ok;
}

std::vector<uint8_t> encodeInput(const InputState& in) {
    std::vector<uint8_t> b;
    putF32(b, in.move.x);
    putF32(b, in.move.y);
    putF32(b, in.aim.x);
    putF32(b, in.aim.y);
    b.push_back(in.attack ? 1 : 0);
    b.push_back(in.ability ? 1 : 0);
    b.push_back(in.equip ? 1 : 0);
    b.push_back(in.unequip ? 1 : 0);
    b.push_back(in.usePotion ? 1 : 0);
    b.push_back(in.dash ? 1 : 0);
    putI32(b, in.classSelect);
    putI32(b, in.allocStat);
    return b;
}

bool decodeInput(const std::vector<uint8_t>& p, InputState& out) {
    Reader r{p.data(), p.size()};
    out.move.x = r.f32();
    out.move.y = r.f32();
    out.aim.x = r.f32();
    out.aim.y = r.f32();
    uint8_t a = 0, ab = 0, eq = 0, un = 0, pot = 0, dsh = 0;
    r.get(&a, 1);
    r.get(&ab, 1);
    r.get(&eq, 1);
    r.get(&un, 1);
    r.get(&pot, 1);
    r.get(&dsh, 1);
    out.attack = a != 0;
    out.ability = ab != 0;
    out.equip = eq != 0;
    out.unequip = un != 0;
    out.usePotion = pot != 0;
    out.dash = dsh != 0;
    out.classSelect = r.i32();
    out.allocStat = r.i32();
    return r.ok;
}

std::vector<uint8_t> encodeSnapshot(const World& w) {
    std::vector<uint8_t> b;
    putI32(b, w.width);
    putI32(b, w.height);
    putI32(b, w.wave);
    putI32(b, w.gold);

    const auto& items = w.inventory.items();
    putI32(b, static_cast<int32_t>(items.size()));
    for (const auto& it : items) {
        putStr(b, it.name);
        putF32(b, it.weight);
        putI32(b, static_cast<int32_t>(it.kind));
        putI32(b, static_cast<int32_t>(it.rarity));
        putI32(b, static_cast<int32_t>(it.affixes.size()));
        for (const auto& a : it.affixes) {
            putI32(b, static_cast<int32_t>(a.type));
            putI32(b, a.magnitude);
        }
    }

    putI32(b, static_cast<int32_t>(w.players.size()));
    for (const auto& p : w.players) {
        b.push_back(p.active ? 1 : 0);
        putF32(b, p.entity.position.x);
        putF32(b, p.entity.position.y);
        putF32(b, p.entity.radius);
        putI32(b, p.entity.hp);
        putI32(b, p.entity.maxHp);
        putI32(b, p.level);
        putI32(b, static_cast<int32_t>(p.cls.id));
        putF32(b, p.cls.attackRange);
        putStr(b, p.cls.abilityName);
        putF32(b, p.aim.x);
        putF32(b, p.aim.y);
        putF32(b, p.invuln);
        putF32(b, p.attackFlash);
        putF32(b, p.shieldTimer);
        putF32(b, p.healFlash);
        putF32(b, p.abilityCooldown);
        putI32(b, p.hasWeapon ? p.weapon.bonusDamage : 0);
        putI32(b, p.hasArmor ? p.armor.bonusMaxHp : 0);
        b.push_back(static_cast<uint8_t>(p.weapon.style));  // sword vs bow overlay
        // Characteristics + progression, so the client HUD/sheet can show them.
        putI32(b, p.stats.str);
        putI32(b, p.stats.dex);
        putI32(b, p.stats.intel);
        putI32(b, p.stats.vit);
        putI32(b, p.stats.agi);
        putI32(b, p.unspentPoints);
        putI32(b, p.xp);
        putI32(b, xpForLevel(p.level));  // next-level threshold, for the XP bar
        // Skill levels, so the client can show the emergent title + sheet.
        putI32(b, p.skills.melee.level);
        putI32(b, p.skills.ranged.level);
        putI32(b, p.skills.heal.level);
        putI32(b, p.skills.dodge.level);
    }

    int32_t aliveEnemies = 0;
    for (const auto& e : w.enemies)
        if (e.alive) ++aliveEnemies;
    putI32(b, aliveEnemies);
    for (const auto& e : w.enemies) {
        if (!e.alive) continue;
        putI32(b, e.id);
        putF32(b, e.position.x);
        putF32(b, e.position.y);
        putF32(b, e.radius);
        putI32(b, e.hp);
        putI32(b, e.maxHp);
        putI32(b, e.targetPlayer);
        putI32(b, static_cast<int32_t>(e.type));
        putF32(b, e.hitFlash);
        putI32(b, e.level);
        b.push_back(e.elite ? 1 : 0);
        putF32(b, e.windup);  // telegraph, so the caster's wind-up shows on clients
    }

    putI32(b, static_cast<int32_t>(w.loot.size()));
    for (const auto& g : w.loot) {
        putF32(b, g.position.x);
        putF32(b, g.position.y);
        putF32(b, g.radius);
        putI32(b, static_cast<int32_t>(g.item.kind));
        putI32(b, static_cast<int32_t>(g.item.rarity));
    }

    putI32(b, static_cast<int32_t>(w.projectiles.size()));
    for (const auto& pr : w.projectiles) {
        putF32(b, pr.position.x);
        putF32(b, pr.position.y);
        putF32(b, pr.radius);
    }
    return b;
}

bool decodeSnapshot(const std::vector<uint8_t>& p, World& out) {
    Reader r{p.data(), p.size()};
    World w;
    w.width = r.i32();
    w.height = r.i32();
    w.wave = r.i32();
    w.gold = r.i32();

    const int32_t itemCount = r.i32();
    for (int i = 0; i < itemCount && r.ok; ++i) {
        Item it;
        it.name = r.str();
        it.weight = r.f32();
        it.kind = static_cast<ItemKind>(r.i32());
        it.rarity = static_cast<Rarity>(r.i32());
        const int32_t affixCount = r.i32();
        for (int a = 0; a < affixCount && r.ok; ++a) {
            Affix af;
            af.type = static_cast<AffixType>(r.i32());
            af.magnitude = r.i32();
            it.affixes.push_back(af);
        }
        w.inventory.tryAdd(it);
    }

    const int32_t playerCount = r.i32();
    for (int i = 0; i < playerCount && r.ok; ++i) {
        Player pl;
        uint8_t active = 0;
        r.get(&active, 1);
        pl.active = active != 0;
        pl.entity.position.x = r.f32();
        pl.entity.position.y = r.f32();
        pl.entity.radius = r.f32();
        pl.entity.hp = r.i32();
        pl.entity.maxHp = r.i32();
        pl.level = r.i32();
        pl.cls.id = static_cast<ClassId>(r.i32());
        pl.cls.attackRange = r.f32();
        pl.cls.abilityName = r.str();
        pl.aim.x = r.f32();
        pl.aim.y = r.f32();
        pl.invuln = r.f32();
        pl.attackFlash = r.f32();
        pl.shieldTimer = r.f32();
        pl.healFlash = r.f32();
        pl.abilityCooldown = r.f32();
        const int wpnBonus = r.i32();
        const int armBonus = r.i32();
        pl.hasWeapon = wpnBonus > 0;
        pl.weapon.bonusDamage = wpnBonus;
        pl.hasArmor = armBonus > 0;
        pl.armor.bonusMaxHp = armBonus;
        uint8_t wstyle = 0;
        r.get(&wstyle, 1);
        pl.weapon.style = static_cast<AttackStyle>(wstyle);
        pl.stats.str = r.i32();
        pl.stats.dex = r.i32();
        pl.stats.intel = r.i32();
        pl.stats.vit = r.i32();
        pl.stats.agi = r.i32();
        pl.unspentPoints = r.i32();
        pl.xp = r.i32();
        const int xpNeeded = r.i32();
        (void)xpNeeded;  // the HUD derives it from level; kept only for wire symmetry
        pl.skills.melee.level = r.i32();
        pl.skills.ranged.level = r.i32();
        pl.skills.heal.level = r.i32();
        pl.skills.dodge.level = r.i32();
        w.players.push_back(std::move(pl));
    }

    const int32_t enemyCount = r.i32();
    for (int i = 0; i < enemyCount && r.ok; ++i) {
        Entity e;
        e.alive = true;
        e.id = r.i32();
        e.position.x = r.f32();
        e.position.y = r.f32();
        e.radius = r.f32();
        e.hp = r.i32();
        e.maxHp = r.i32();
        e.targetPlayer = r.i32();
        e.type = static_cast<EnemyType>(r.i32());
        e.hitFlash = r.f32();
        e.level = r.i32();
        uint8_t elite = 0;
        r.get(&elite, 1);
        e.elite = elite != 0;
        e.windup = r.f32();
        w.enemies.push_back(std::move(e));
    }

    const int32_t lootCount = r.i32();
    for (int i = 0; i < lootCount && r.ok; ++i) {
        GroundItem g;
        g.position.x = r.f32();
        g.position.y = r.f32();
        g.radius = r.f32();
        g.item.kind = static_cast<ItemKind>(r.i32());
        g.item.rarity = static_cast<Rarity>(r.i32());
        w.loot.push_back(std::move(g));
    }

    const int32_t projCount = r.i32();
    for (int i = 0; i < projCount && r.ok; ++i) {
        Projectile pr;
        pr.position.x = r.f32();
        pr.position.y = r.f32();
        pr.radius = r.f32();
        w.projectiles.push_back(std::move(pr));
    }

    if (!r.ok) return false;
    out = std::move(w);
    return true;
}

// --- Host --------------------------------------------------------------------

Host::~Host() {
    for (auto& [fd, c] : conns_) ::close(c.fd);
    if (listenFd_ >= 0) ::close(listenFd_);
}

bool Host::start(uint16_t port) {
    ignoreSigpipeOnce();
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) return false;
    int yes = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::fprintf(stderr, "[net] bind failed: %s\n", std::strerror(errno));
        return false;
    }
    if (::listen(listenFd_, 8) < 0) return false;
    setNonBlocking(listenFd_);
    std::fprintf(stderr, "[net] hosting on port %u\n", port);
    return true;
}

std::vector<Host::Event> Host::poll() {
    std::vector<Event> events;

    // Accept any pending connections.
    for (;;) {
        const int fd = ::accept(listenFd_, nullptr, nullptr);
        if (fd < 0) break;  // EAGAIN when none pending
        int yes = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        conns_[fd] = Connection{fd, {}};
        events.push_back({Event::Join, fd, {}});
    }

    // Read input from existing connections.
    std::vector<int> dead;
    for (auto& [fd, conn] : conns_) {
        if (!recvInto(conn)) {
            dead.push_back(fd);
            continue;
        }
        MsgType type;
        std::vector<uint8_t> payload;
        while (nextMessage(conn, type, payload)) {
            if (type == MSG_INPUT) {
                InputState in;
                if (decodeInput(payload, in)) events.push_back({Event::Input, fd, in});
            }
        }
    }
    for (int fd : dead) {
        events.push_back({Event::Leave, fd, {}});
        ::close(fd);
        conns_.erase(fd);
    }
    return events;
}

void Host::send(int conn, MsgType type, const std::vector<uint8_t>& payload) {
    auto it = conns_.find(conn);
    if (it != conns_.end()) sendAll(it->second.fd, frame(type, payload));
}

void Host::broadcast(MsgType type, const std::vector<uint8_t>& payload) {
    const std::vector<uint8_t> bytes = frame(type, payload);
    for (auto& [fd, conn] : conns_) sendAll(conn.fd, bytes);
}

// --- Client ------------------------------------------------------------------

Client::~Client() {
    if (conn_.fd >= 0) ::close(conn_.fd);
}

bool Client::connect(const std::string& ip, uint16_t port) {
    ignoreSigpipeOnce();
    ip_ = ip;  // remembered for auto-reconnect
    port_ = port;
    conn_.fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (conn_.fd < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        std::fprintf(stderr, "[net] bad address: %s\n", ip.c_str());
        return false;
    }
    if (::connect(conn_.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::fprintf(stderr, "[net] connect failed: %s\n", std::strerror(errno));
        ::close(conn_.fd);
        conn_.fd = -1;
        return false;
    }
    int yes = 1;
    ::setsockopt(conn_.fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    std::fprintf(stderr, "[net] connected to %s:%u\n", ip.c_str(), port);
    return true;
}

void Client::poll(uint64_t nowMs) {
    if (conn_.fd < 0) return;
    if (!recvInto(conn_)) {
        ::close(conn_.fd);
        conn_.fd = -1;
        std::fprintf(stderr, "[net] lost connection; will retry\n");
        return;
    }
    MsgType type;
    std::vector<uint8_t> payload;
    while (nextMessage(conn_, type, payload)) {
        if (type == MSG_WELCOME) {
            decodeWelcome(payload, myId_);
            std::fprintf(stderr, "[net] joined as player %d\n", myId_ + 1);
        } else if (type == MSG_SNAPSHOT) {
            if (decodeSnapshot(payload, world_)) {
                hasSnapshot_ = true;
                buffer_.push_back({nowMs, world_});
                // Keep ~1s of history; that's plenty for interpolation.
                while (buffer_.size() > 2 && buffer_.front().tick + 1000 < nowMs)
                    buffer_.pop_front();
            }
        }
    }
}

void Client::tryReconnect(uint64_t nowMs) {
    if (conn_.fd >= 0) return;
    if (nowMs - lastReconnect_ < 1000) return;  // retry at most once a second
    lastReconnect_ = nowMs;
    connect(ip_, port_);  // logs its own success/failure
}

const World& Client::view(uint64_t renderMs) {
    if (buffer_.empty()) return world_;
    if (buffer_.size() == 1) return buffer_.back().world;

    // Find the pair of snapshots that bracket renderMs.
    const TimedSnapshot* a = nullptr;
    const TimedSnapshot* b = nullptr;
    for (std::size_t i = 0; i + 1 < buffer_.size(); ++i) {
        if (buffer_[i].tick <= renderMs && renderMs <= buffer_[i + 1].tick) {
            a = &buffer_[i];
            b = &buffer_[i + 1];
            break;
        }
    }
    if (!a) return buffer_.back().world;  // renderMs is ahead of/behind buffer

    const uint64_t span = b->tick - a->tick;
    const float t = span > 0 ? static_cast<float>(renderMs - a->tick) / static_cast<float>(span)
                             : 1.0f;

    // Take the newer snapshot as the base (latest roster/loot), then smooth
    // positions between the two.
    view_ = b->world;
    const std::size_t n = std::min(a->world.players.size(), b->world.players.size());
    for (std::size_t i = 0; i < n && i < view_.players.size(); ++i) {
        const Vec2 pa = a->world.players[i].entity.position;
        const Vec2 pb = b->world.players[i].entity.position;
        view_.players[i].entity.position = pa + (pb - pa) * t;
    }

    // Enemies now carry a stable id, so match them across the two snapshots and
    // lerp — no more stutter on remote enemies (players + world mobs alike).
    std::unordered_map<int, Vec2> prev;
    prev.reserve(a->world.enemies.size());
    for (const auto& e : a->world.enemies) prev[e.id] = e.position;
    for (auto& e : view_.enemies) {
        const auto it = prev.find(e.id);
        if (it != prev.end()) e.position = it->second + (e.position - it->second) * t;
    }
    return view_;
}

void Client::sendInput(const InputState& in) {
    if (conn_.fd < 0) return;
    sendAll(conn_.fd, frame(MSG_INPUT, encodeInput(in)));
}

}  // namespace game::net
