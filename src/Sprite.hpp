#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace game {

// A tiny hand-authored sprite, decoded from data/sprites.lua into a flat RGBA
// bitmap. Purely render-side data — the Simulation never sees it. The Renderer
// bakes each one into an SDL_Texture at startup (see Renderer::loadSprites).
//
// Kept SDL-free so ScriptEngine (the only Lua-aware translation unit) can build
// it without pulling in rendering headers. `rgba` is width*height*4 bytes in
// R,G,B,A order; an alpha of 0 marks a transparent pixel.
struct SpritePixels {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

}  // namespace game
