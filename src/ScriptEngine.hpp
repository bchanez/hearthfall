#pragma once

#include <string>
#include <vector>

#include "GameContent.hpp"
#include "Sprite.hpp"

namespace game {

// Loads data-driven content from Lua files. This is the ONLY translation unit
// that includes sol2/Lua — everything downstream works with plain structs, so
// the Simulation stays script-free and unit-testable.
// The persistent shared bank (gold + stashed items), saved between sessions.
struct SaveState {
    int gold = 0;
    std::vector<Item> items;
};

class ScriptEngine {
public:
    // Reads `<dataDir>/classes.lua` and `<dataDir>/loot.lua`. On any problem
    // (missing file, parse error, unknown enum value) it logs a warning and
    // falls back to the built-in defaultContent(), so the game always runs.
    static GameContent loadContent(const std::string& dataDir);

    // Reads `<dataDir>/sprites.lua` into decoded RGBA bitmaps (see Sprite.hpp).
    // Purely render-side, so it's kept off GameContent/the Simulation. Returns an
    // empty list (and logs) if the file is missing or malformed — the renderer
    // then just falls back to solid squares.
    static std::vector<SpritePixels> loadSprites(const std::string& dataDir);

    // Persistence for the shared bank. Saved as a Lua file so it's inspectable
    // and reuses the same loading path. loadState returns an empty state if the
    // file is missing or unreadable.
    static void saveState(const std::string& path, const SaveState& state);
    static SaveState loadState(const std::string& path);
};

}  // namespace game
