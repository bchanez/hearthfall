#include "ScriptEngine.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

// Single-header PNG decoder (public domain). This translation unit owns the
// implementation; it's the only place external image files enter the engine.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

namespace game {

namespace {

const char* kindToString(ItemKind kind) {
    switch (kind) {
        case ItemKind::Gold:   return "gold";
        case ItemKind::Potion: return "potion";
        case ItemKind::Weapon: return "weapon";
        case ItemKind::Armor:  return "armor";
    }
    return "potion";
}

const char* rarityToString(Rarity r) {
    switch (r) {
        case Rarity::Common:   return "common";
        case Rarity::Uncommon: return "uncommon";
        case Rarity::Rare:     return "rare";
        case Rarity::Epic:     return "epic";
    }
    return "common";
}

Rarity rarityFromString(const std::string& s) {
    if (s == "uncommon") return Rarity::Uncommon;
    if (s == "rare") return Rarity::Rare;
    if (s == "epic") return Rarity::Epic;
    return Rarity::Common;
}

const char* affixTypeToString(AffixType t) {
    switch (t) {
        case AffixType::MaxHp:       return "maxHp";
        case AffixType::Damage:      return "damage";
        case AffixType::AttackSpeed: return "attackSpeed";
        case AffixType::MoveSpeed:   return "moveSpeed";
        case AffixType::Crit:        return "crit";
        case AffixType::Lifesteal:   return "lifesteal";
    }
    return "maxHp";
}

AffixType affixTypeFromString(const std::string& s) {
    if (s == "damage") return AffixType::Damage;
    if (s == "attackSpeed") return AffixType::AttackSpeed;
    if (s == "moveSpeed") return AffixType::MoveSpeed;
    if (s == "crit") return AffixType::Crit;
    if (s == "lifesteal") return AffixType::Lifesteal;
    return AffixType::MaxHp;
}

// --- small helpers: read a field from the Lua table currently on the stack top

std::string fieldStr(lua_State* L, const char* key, const char* fallback) {
    lua_getfield(L, -1, key);
    std::string v = lua_isstring(L, -1) ? lua_tostring(L, -1) : fallback;
    lua_pop(L, 1);
    return v;
}

double fieldNum(lua_State* L, const char* key, double fallback) {
    lua_getfield(L, -1, key);
    double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : fallback;
    lua_pop(L, 1);
    return v;
}

ClassId classIdFromString(const std::string& s) {
    if (s == "tank") return ClassId::Tank;
    if (s == "healer") return ClassId::Healer;
    return ClassId::Archer;
}

AttackStyle attackStyleFromString(const std::string& s) {
    return s == "ranged" ? AttackStyle::Ranged : AttackStyle::Melee;
}

const char* attackStyleToString(AttackStyle s) {
    return s == AttackStyle::Ranged ? "ranged" : "melee";
}

ItemKind itemKindFromString(const std::string& s) {
    if (s == "gold") return ItemKind::Gold;
    if (s == "weapon") return ItemKind::Weapon;
    if (s == "armor") return ItemKind::Armor;
    return ItemKind::Potion;
}

AffixSpec readAffixSpec(lua_State* L) {  // spec table at stack top
    AffixSpec s;
    s.type = affixTypeFromString(fieldStr(L, "stat", "maxHp"));
    s.minMag = static_cast<int>(fieldNum(L, "min", 1));
    s.maxMag = static_cast<int>(fieldNum(L, "max", 1));
    return s;
}

PlayerClass readClass(lua_State* L) {  // entry table at stack top
    PlayerClass c;
    c.name = fieldStr(L, "name", "?");
    c.id = classIdFromString(fieldStr(L, "id", "archer"));
    c.maxHp = static_cast<int>(fieldNum(L, "maxHp", 100));
    c.speed = static_cast<float>(fieldNum(L, "speed", 260.0));
    c.attackStyle = attackStyleFromString(fieldStr(L, "attackStyle", "melee"));
    c.attackDamage = static_cast<int>(fieldNum(L, "attackDamage", 20));
    c.attackRange = static_cast<float>(fieldNum(L, "attackRange", 80.0));
    c.attackCooldown = static_cast<float>(fieldNum(L, "attackCooldown", 0.30));
    c.abilityName = fieldStr(L, "abilityName", "Ability");
    c.abilityCooldown = static_cast<float>(fieldNum(L, "abilityCooldown", 6.0));
    return c;
}

Item readItem(lua_State* L) {  // entry table at stack top
    Item item;
    item.name = fieldStr(L, "name", "?");
    item.weight = static_cast<float>(fieldNum(L, "weight", 0.0));
    item.kind = itemKindFromString(fieldStr(L, "kind", "potion"));
    item.value = static_cast<int>(fieldNum(L, "value", 0));
    item.bonusDamage = static_cast<int>(fieldNum(L, "bonusDamage", 0));
    item.bonusMaxHp = static_cast<int>(fieldNum(L, "bonusMaxHp", 0));
    item.style = attackStyleFromString(fieldStr(L, "style", "melee"));  // weapons
    item.rarity = rarityFromString(fieldStr(L, "rarity", "common"));
    item.dropWeight = static_cast<float>(fieldNum(L, "dropWeight", 1.0));

    lua_getfield(L, -1, "affixes");
    if (lua_istable(L, -1)) {
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_geti(L, -1, i);
            if (lua_istable(L, -1)) {
                Affix a;
                a.type = affixTypeFromString(fieldStr(L, "stat", "maxHp"));
                a.magnitude = static_cast<int>(fieldNum(L, "magnitude", 0));
                item.affixes.push_back(a);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return item;
}

// Parse "#rrggbb" (or "rrggbb") into three bytes. Unparseable channels stay 0.
struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
};
// ---- Global colour grade --------------------------------------------------
// Every colour in the game (tiles, sprites, effects, loot) is decoded through
// parseHex below, so this one transform IS the whole game's "look". The brief:
// deepen + cool the shadows, warm the highlights, and lift saturation so actors
// and FX pop against the dark dungeon — a classic split-tone grade. Tune these
// constants and relaunch; no other file needs to change. Set kSaturation = 1 and
// the two tints to 0 to disable.
// Tuned in the sprite viewer's "Ref-match (Epic/FarVale)" preset: punchier
// saturation/contrast and a warm/neutral split-tone (only a whisper of cool in
// the darks) so the redrawn, outlined monsters read vivid instead of muddy.
namespace grade {
constexpr float kSaturation = 1.42f;  // >1 = more colourful actors/FX
constexpr float kContrast = 1.12f;    // >1 = deeper shadows, brighter lights
// Split-tone: signed RGB pushes, weighted toward the shadows / highlights.
constexpr float kShadowR = -0.010f, kShadowG = -0.005f, kShadowB = 0.020f;  // faint cool in darks
constexpr float kLightR = 0.050f, kLightG = 0.030f, kLightB = -0.020f;      // warm the lights
constexpr float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
}  // namespace grade

Rgb applyGrade(Rgb c) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    const float lum = 0.299f * r + 0.587f * g + 0.114f * b;  // 0 shadow .. 1 light

    // 1) Saturation: push each channel away from its grey (the pixel's luminance).
    r = lum + (r - lum) * grade::kSaturation;
    g = lum + (g - lum) * grade::kSaturation;
    b = lum + (b - lum) * grade::kSaturation;

    // 2) Contrast around mid-grey: deepens the shadows, keeps the lights bright.
    r = (r - 0.5f) * grade::kContrast + 0.5f;
    g = (g - 0.5f) * grade::kContrast + 0.5f;
    b = (b - 0.5f) * grade::kContrast + 0.5f;

    // 3) Split-tone: cool the darks, warm the lights (each weighted by luminance).
    const float shadow = 1.0f - lum, light = lum;
    r += grade::kShadowR * shadow + grade::kLightR * light;
    g += grade::kShadowG * shadow + grade::kLightG * light;
    b += grade::kShadowB * shadow + grade::kLightB * light;

    return {static_cast<std::uint8_t>(grade::clamp01(r) * 255.0f + 0.5f),
            static_cast<std::uint8_t>(grade::clamp01(g) * 255.0f + 0.5f),
            static_cast<std::uint8_t>(grade::clamp01(b) * 255.0f + 0.5f)};
}

Rgb parseHex(const std::string& s) {
    const std::size_t off = (!s.empty() && s[0] == '#') ? 1 : 0;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    Rgb c;
    if (s.size() >= off + 6) {
        c.r = static_cast<std::uint8_t>(nib(s[off]) * 16 + nib(s[off + 1]));
        c.g = static_cast<std::uint8_t>(nib(s[off + 2]) * 16 + nib(s[off + 3]));
        c.b = static_cast<std::uint8_t>(nib(s[off + 4]) * 16 + nib(s[off + 5]));
    }
    return applyGrade(c);  // grade every colour as it enters the engine
}

// Decode a grid of row-strings + palette into an RGBA bitmap. A '.', ' ' or '0'
// is transparent; digits 1..9 index the palette. Ragged rows are padded to the
// widest with transparency, so counting columns exactly is not required.
SpritePixels buildPixels(const std::string& name, const std::vector<Rgb>& palette,
                         const std::vector<std::string>& rows) {
    SpritePixels sp;
    sp.name = name;

    std::size_t w = 0;
    for (const auto& row : rows) w = std::max(w, row.size());
    sp.width = static_cast<int>(w);
    sp.height = static_cast<int>(rows.size());
    sp.rgba.assign(w * rows.size() * 4, 0);  // transparent by default

    for (std::size_t y = 0; y < rows.size(); ++y) {
        const std::string& row = rows[y];
        for (std::size_t x = 0; x < row.size(); ++x) {
            const char ch = row[x];
            if (ch < '1' || ch > '9') continue;  // '.', ' ', '0' → transparent
            const std::size_t idx = static_cast<std::size_t>(ch - '1');
            const Rgb c = idx < palette.size() ? palette[idx] : Rgb{255, 0, 255};  // magenta = missing
            std::uint8_t* px = &sp.rgba[(y * w + x) * 4];
            px[0] = c.r;
            px[1] = c.g;
            px[2] = c.b;
            px[3] = 255;
        }
    }
    return sp;
}

// Read the "palette" field of the table at stack top into hex-decoded colours.
std::vector<Rgb> readPaletteField(lua_State* L) {
    std::vector<Rgb> palette;
    lua_getfield(L, -1, "palette");
    if (lua_istable(L, -1)) {
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_geti(L, -1, i);
            palette.push_back(parseHex(lua_isstring(L, -1) ? lua_tostring(L, -1) : ""));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return palette;
}

// Read an array-of-strings table at stack top into row strings.
std::vector<std::string> readRowsAtTop(lua_State* L) {
    std::vector<std::string> rows;
    const lua_Integer n = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= n; ++i) {
        lua_geti(L, -1, i);
        rows.emplace_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : "");
        lua_pop(L, 1);
    }
    return rows;
}

// Read a flat sprite entry (table at stack top): { name, palette, rows } — used
// for tiles/props, which are single static frames.
SpritePixels readSprite(lua_State* L) {
    const std::string name = fieldStr(L, "name", "?");
    const std::vector<Rgb> palette = readPaletteField(L);
    std::vector<std::string> rows;
    lua_getfield(L, -1, "rows");
    if (lua_istable(L, -1)) rows = readRowsAtTop(L);
    lua_pop(L, 1);
    return buildPixels(name, palette, rows);
}

// Load an animated character sheet:
//   { name = "tank", palette = {..}, states = { idle = { {frame} },
//                                               walk = { {frameA}, {frameB} },
//                                               attack = { {frame} } } }
// Each frame is expanded into a SpritePixels named "<base>.<state>.<index>", so
// the renderer groups them back into animations by that naming convention.
bool loadCharacterSheet(const std::string& path, std::vector<SpritePixels>& out) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    bool ok = false;
    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        std::fprintf(stderr, "[ScriptEngine] %s: %s\n", path.c_str(), lua_tostring(L, -1));
    } else if (!lua_istable(L, -1)) {
        std::fprintf(stderr, "[ScriptEngine] %s did not return a table\n", path.c_str());
    } else {
        const char* kStates[] = {"idle", "walk", "attack"};
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_geti(L, -1, i);  // entry
            if (lua_istable(L, -1)) {
                const std::string base = fieldStr(L, "name", "?");
                const std::vector<Rgb> palette = readPaletteField(L);
                lua_getfield(L, -1, "states");
                if (lua_istable(L, -1)) {
                    for (const char* st : kStates) {
                        lua_getfield(L, -1, st);
                        if (lua_istable(L, -1)) {
                            const lua_Integer nf = luaL_len(L, -1);
                            for (lua_Integer f = 1; f <= nf; ++f) {
                                lua_geti(L, -1, f);  // frame = array of rows
                                if (lua_istable(L, -1)) {
                                    out.push_back(buildPixels(
                                        base + "." + st + "." + std::to_string(f - 1), palette,
                                        readRowsAtTop(L)));
                                }
                                lua_pop(L, 1);  // frame
                            }
                        }
                        lua_pop(L, 1);  // state list
                    }
                }
                lua_pop(L, 1);  // states
            }
            lua_pop(L, 1);  // entry
        }
        ok = true;
    }

    lua_close(L);
    return ok;
}

// Runs a Lua file expected to return an array of tables, mapping each with
// `reader`. Returns false (leaving `out` untouched) on any failure.
template <typename T, typename Reader>
bool loadArray(const std::string& path, Reader reader, std::vector<T>& out) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    bool ok = false;
    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        std::fprintf(stderr, "[ScriptEngine] %s: %s\n", path.c_str(), lua_tostring(L, -1));
    } else if (!lua_istable(L, -1)) {
        std::fprintf(stderr, "[ScriptEngine] %s did not return a table\n", path.c_str());
    } else {
        std::vector<T> parsed;
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_geti(L, -1, i);  // push entry
            if (lua_istable(L, -1)) parsed.push_back(reader(L));
            lua_pop(L, 1);  // pop entry
        }
        if (parsed.empty()) {
            std::fprintf(stderr, "[ScriptEngine] %s had no entries\n", path.c_str());
        } else {
            out = std::move(parsed);
            ok = true;
        }
    }

    lua_close(L);
    return ok;
}

// Decode a PNG file into an RGBA SpritePixels — the bridge that lets external
// pixel-art packs (Dungeon Crawl 32x32, CC0) run through the same render path as
// the procedural sprites. Width/height stay 0 on failure so the renderer skips
// it. When `grade` is set, each opaque texel is pushed through the game's colour
// grade, so the external art shares the hand-authored art's mood.
SpritePixels loadPng(const std::string& path, const std::string& name, bool grade) {
    SpritePixels sp;
    sp.name = name;
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);  // force RGBA
    if (!data) {
        std::fprintf(stderr, "[ScriptEngine] png load failed: %s (%s)\n", path.c_str(),
                     stbi_failure_reason());
        return sp;
    }
    sp.width = w;
    sp.height = h;
    sp.rgba.assign(data, data + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(data);

    if (grade) {
        for (std::size_t i = 0; i + 3 < sp.rgba.size(); i += 4) {
            if (sp.rgba[i + 3] == 0) continue;  // leave fully-transparent texels alone
            const Rgb g = applyGrade({sp.rgba[i], sp.rgba[i + 1], sp.rgba[i + 2]});
            sp.rgba[i] = g.r;
            sp.rgba[i + 1] = g.g;
            sp.rgba[i + 2] = g.b;
        }
    }
    return sp;
}

// Load the external-art manifest (data/dcss.lua): a table
//   { root = "...", grade = bool, sprites = { {name=, file=, anim=bool}, ... } }
// Each entry becomes one SpritePixels; characters (anim=true) are emitted as a
// single "<name>.idle.0" frame so the renderer's animation path still finds them
// (real animation is a later pass). `overridden` collects the base names DCSS
// provides, so the caller can drop the procedural sprites they replace.
bool loadDcssManifest(const std::string& dataDir, std::vector<SpritePixels>& out,
                      std::set<std::string>& overridden) {
    const std::string path = dataDir + "/dcss.lua";
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        std::fprintf(stderr, "[ScriptEngine] %s: %s\n", path.c_str(), lua_tostring(L, -1));
        lua_close(L);
        return false;
    }
    if (!lua_istable(L, -1)) {
        lua_close(L);
        return false;
    }

    const std::string root = fieldStr(L, "root", "");
    lua_getfield(L, -1, "grade");
    const bool grade = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    // The asset root is resolved relative to the manifest's own folder (dataDir),
    // so it works regardless of the process's working directory.
    const std::filesystem::path base = std::filesystem::path(dataDir) / root;

    int loaded = 0;
    lua_getfield(L, -1, "sprites");
    if (lua_istable(L, -1)) {
        const lua_Integer n = luaL_len(L, -1);
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_geti(L, -1, i);
            if (lua_istable(L, -1)) {
                const std::string name = fieldStr(L, "name", "");
                const std::string file = fieldStr(L, "file", "");
                lua_getfield(L, -1, "anim");
                const bool anim = lua_toboolean(L, -1) != 0;
                lua_pop(L, 1);
                if (!name.empty() && !file.empty()) {
                    const std::string full = (base / file).lexically_normal().string();
                    SpritePixels sp = loadPng(full, anim ? name + ".idle.0" : name, grade);
                    if (sp.width > 0 && sp.height > 0) {
                        out.push_back(std::move(sp));
                        overridden.insert(name);
                        ++loaded;
                    }
                }
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);  // sprites

    lua_close(L);
    std::fprintf(stderr, "[ScriptEngine] loaded %d DCSS PNG sprites\n", loaded);
    return loaded > 0;
}

}  // namespace

GameContent ScriptEngine::loadContent(const std::string& dataDir) {
    GameContent content = defaultContent();  // start from safe defaults

    loadArray<PlayerClass>(dataDir + "/classes.lua", readClass, content.classes);
    loadArray<Item>(dataDir + "/loot.lua", readItem, content.lootTable);
    loadArray<AffixSpec>(dataDir + "/affixes.lua", readAffixSpec, content.affixPool);

    std::fprintf(stderr, "[ScriptEngine] loaded %zu classes, %zu loot, %zu affixes\n",
                 content.classes.size(), content.lootTable.size(), content.affixPool.size());
    return content;
}

std::vector<SpritePixels> ScriptEngine::loadSprites(const std::string& dataDir) {
    std::vector<SpritePixels> sprites;
    // Characters are animated sheets (states + frames); tiles/props are flat
    // single frames. Both decode to the same SpritePixels list — the renderer
    // keys everything by name and groups "base.state.frame" into animations.
    loadCharacterSheet(dataDir + "/sprites.lua", sprites);
    // Spell/projectile effects share the animated-sheet format (see effects.lua);
    // the renderer keys them by name too and cycles + rotates + glows them.
    loadCharacterSheet(dataDir + "/effects.lua", sprites);
    std::vector<SpritePixels> tiles;
    loadArray<SpritePixels>(dataDir + "/tiles.lua", readSprite, tiles);
    for (auto& t : tiles) sprites.push_back(std::move(t));

    // Overlay the external Dungeon Crawl (CC0) PNG art: it's the primary look
    // now, so anything it provides *replaces* the procedural sprite of the same
    // base name — including every "base.state.frame" frame of a character it
    // covers, so no half-procedural / half-DCSS animation leaks through.
    std::vector<SpritePixels> dcss;
    std::set<std::string> overridden;
    if (loadDcssManifest(dataDir, dcss, overridden)) {
        auto baseName = [](const std::string& n) {
            const auto dot = n.find('.');
            return dot == std::string::npos ? n : n.substr(0, dot);
        };
        sprites.erase(std::remove_if(sprites.begin(), sprites.end(),
                                     [&](const SpritePixels& s) {
                                         return overridden.count(baseName(s.name)) > 0;
                                     }),
                      sprites.end());
        for (auto& d : dcss) sprites.push_back(std::move(d));
    }

    std::fprintf(stderr, "[ScriptEngine] loaded %zu sprite frames + tiles\n", sprites.size());
    return sprites;
}

void ScriptEngine::saveState(const std::string& path, const SaveState& state) {
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);

    std::ofstream out(path);
    if (!out) {
        std::fprintf(stderr, "[ScriptEngine] could not write save %s\n", path.c_str());
        return;
    }
    out << "-- Auto-generated shared-bank save. Safe to edit or delete.\n";
    out << "return {\n";
    out << "  gold = " << state.gold << ",\n";
    out << "  items = {\n";
    for (const auto& it : state.items) {
        out << "    { name = \"" << it.name << "\", weight = " << it.weight << ", kind = \""
            << kindToString(it.kind) << "\", value = " << it.value
            << ", bonusDamage = " << it.bonusDamage << ", bonusMaxHp = " << it.bonusMaxHp
            << ", style = \"" << attackStyleToString(it.style) << "\", rarity = \""
            << rarityToString(it.rarity) << "\", affixes = {";
        for (const auto& a : it.affixes) {
            out << " { stat = \"" << affixTypeToString(a.type) << "\", magnitude = " << a.magnitude
                << " },";
        }
        out << " } },\n";
    }
    out << "  },\n";
    out << "}\n";
}

SaveState ScriptEngine::loadState(const std::string& path) {
    SaveState state;
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        // Missing/broken save is normal on first run — stay silent-ish.
        lua_close(L);
        return state;
    }
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "gold");
        if (lua_isnumber(L, -1)) state.gold = static_cast<int>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "items");
        if (lua_istable(L, -1)) {
            const lua_Integer n = luaL_len(L, -1);
            for (lua_Integer i = 1; i <= n; ++i) {
                lua_geti(L, -1, i);
                if (lua_istable(L, -1)) state.items.push_back(readItem(L));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }

    lua_close(L);
    return state;
}

}  // namespace game
