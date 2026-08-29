#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "Vec2.hpp"
#include "World.hpp"

// Minimal LAN networking over TCP. The host runs the authoritative Simulation;
// clients send their input and render snapshots the host broadcasts. This is a
// prototype: same-architecture LAN only (native-endian payloads), trusts
// clients, no lag compensation. The point is that it plugs into the existing
// command seam — see DESIGN.md.
namespace game::net {

// A client's per-frame input, sent to the host and turned into Commands there.
struct InputState {
    Vec2 move{};
    Vec2 aim{1.0f, 0.0f};
    bool attack = false;
    bool ability = false;
    bool equip = false;
    bool unequip = false;
    bool usePotion = false;
    bool dash = false;
    int classSelect = -1;    // -1 == no change this frame
    int allocStat = -1;      // stat index to spend a point into this frame, or -1
    int chooseUpgrade = -1;  // level-up boon slot (0..2) picked this frame, or -1
    int equipItem = -1;      // bank item index to equip this frame, or -1
    int sellItem = -1;       // bank item index to sell this frame, or -1
};

enum MsgType : uint8_t { MSG_WELCOME = 1, MSG_SNAPSHOT = 2, MSG_INPUT = 3 };

// --- payload (de)serialization (no framing; see Host/Client for that) --------
std::vector<uint8_t> encodeWelcome(int playerId);
std::vector<uint8_t> encodeSnapshot(const World& world);
std::vector<uint8_t> encodeInput(const InputState& in);
bool decodeWelcome(const std::vector<uint8_t>& p, int& playerId);
bool decodeSnapshot(const std::vector<uint8_t>& p, World& out);
bool decodeInput(const std::vector<uint8_t>& p, InputState& out);

// A buffered TCP connection: accumulates bytes and yields whole framed messages.
struct Connection {
    int fd = -1;
    std::vector<uint8_t> in;  // receive accumulator
};

// Authoritative side. Non-blocking accept + reads.
class Host {
public:
    ~Host();
    bool start(uint16_t port);

    struct Event {
        enum Type { Join, Leave, Input } type;
        int conn = -1;  // connection id (its fd)
        InputState input;
    };
    std::vector<Event> poll();  // accept new clients, read available input

    void send(int conn, MsgType type, const std::vector<uint8_t>& payload);
    void broadcast(MsgType type, const std::vector<uint8_t>& payload);

private:
    int listenFd_ = -1;
    std::unordered_map<int, Connection> conns_;
};

// Client side. Sends input, receives welcome + snapshots, and interpolates
// between snapshots for smooth rendering. Auto-reconnects if the link drops.
class Client {
public:
    ~Client();
    bool connect(const std::string& ip, uint16_t port);

    void poll(uint64_t nowMs);        // process incoming messages (timestamped)
    void tryReconnect(uint64_t nowMs);  // re-dial the host if disconnected
    void sendInput(const InputState& in);

    bool connected() const { return conn_.fd >= 0; }
    bool hasSnapshot() const { return hasSnapshot_; }
    const World& world() const { return world_; }  // latest snapshot (for aim)
    const World& view(uint64_t renderMs);           // interpolated for rendering
    int myId() const { return myId_; }

private:
    struct TimedSnapshot {
        uint64_t tick = 0;
        World world;
    };

    Connection conn_;
    std::string ip_;
    uint16_t port_ = 0;
    World world_;                       // latest received snapshot
    World view_;                        // last interpolated result (returned by ref)
    std::deque<TimedSnapshot> buffer_;  // recent snapshots for interpolation
    int myId_ = -1;
    bool hasSnapshot_ = false;
    uint64_t lastReconnect_ = 0;
};

}  // namespace game::net
