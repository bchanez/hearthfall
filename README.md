# jeu — 2D co-op fantasy ARPG

A 2D top-down co-op action RPG built around **role-based teamwork** (tank / healer /
DPS), a **shared household bank**, and **level-matched drop-in co-op** so friends are
never too low-level to play together. Solo / 2p / 4p+.

Built from scratch in **C++ / SDL3**, with gameplay content scripted in **Lua**.
The engine is hand-written on purpose — this project doubles as a way to learn
low-level game programming.

> Full design and roadmap: [`DESIGN.md`](DESIGN.md).

## Why this repo

A learning-first project: build the engine (loop, rendering, collision, netcode)
by hand in C++, iterate on the *fun* in Lua. The north star is a Diablo-like
co-op ARPG; we get there one playable step at a time.

## Status

**Latest:** **affixes** — gear rolls modifiers by rarity (+HP, +damage, +attack
speed, +move speed, +crit) from a Lua-defined pool, aggregated from equipped
gear and shown in the bank overlay. Builds on item **rarity** (Common→Epic;
bosses drop the good stuff) and **netcode hardening** (client-side interpolation
+ auto-reconnect).


**Steps 7 & 8 — progression, shared bank, and online (LAN).**

- **Progression:** characters gain **XP and levels** (scaling HP/damage). A
  friend who joins is **level-matched** to the party, so nobody's too low to
  help. Switching class keeps your level.
- **Shared bank:** press **Tab** for the household stash overlay (gold, items,
  weight) — all players deposit into the one shared bank, which **persists across
  sessions** (saved to `save/bank.lua` on the host/local machine).
- **Equipment + fluid loot:** items carry bonuses (weapons → damage, armor → max
  HP). Press **F** to equip the best gear from the shared bank, **G** to return
  it. When a player leaves, their gear **flows back to the pool** for the others.
- **Online (LAN):** run one instance as **`--host`** (authoritative sim) and
  others with **`--join <ip>`**. Remote input arrives as the same **commands**
  the local sim already uses; the host broadcasts world snapshots. *Prototype:*
  same-arch LAN, trusts clients, no lag compensation yet.

Earlier steps: sim/renderer/command architecture, three+ classes, local co-op
(keyboard + gamepads), **aggro/threat** (Tank *Taunt*, Healer draws aggro), and
**Lua-defined content** (`data/*.lua`; a data-only Mage ships as proof — press
`4`).

**Local co-op:** **keyboard = P1**, and **each gamepad joins as its own player**
(drop-in / leave). All players share the household **bank**.

Built on the **network-shaped** architecture (see [DESIGN.md](DESIGN.md)): a
`Simulation` owns all state/rules and never draws, a `Renderer` only draws, and
every player action flows as a **command** carrying a `playerId` — so online
co-op later is just "commands arrive over sockets", not a rewrite.

*Earlier:* step 1 foundation; step 2 loot & weight; step 3 classes +
sim/renderer/command refactor; step 4 local co-op; step 5 aggro/trinity.

See the [build order](DESIGN.md#build-order-each-step-is-a-playable-game)
for what comes next (shared bank + level-matching → online).

## Prerequisites

- macOS with [Homebrew](https://brew.sh/)
- A C++20 compiler (Apple Clang, already on macOS via Xcode Command Line Tools)

Install the toolchain and SDL3:

```sh
xcode-select --install         # if you don't already have clang
brew install cmake sdl3 lua@5.4
```

> Lua **5.4** specifically (`lua@5.4`, keg-only) — the content files target the
> stable 5.4 line. CMake finds it under `/opt/homebrew/opt/lua@5.4`.

## Quickstart

A `Makefile` wraps the CMake commands, so day-to-day it's just:

```sh
make        # configure (once) + build
make run    # build + launch the game
make test   # build + run unit tests
make format # clang-format all sources
make clean  # remove the build directory
```

<details>
<summary>Raw CMake commands (what the Makefile runs)</summary>

```sh
cmake --preset debug          # configure (Debug, tests enabled)
cmake --build build/debug      # build
./build/debug/jeu              # run
ctest --test-dir build/debug --output-on-failure   # test

# Release build (no tests):
cmake --preset release && cmake --build build/release && ./build/release/jeu
```
</details>

### Controls

Player 1 is the keyboard + mouse. Plug in a gamepad and it joins as its own
player (up to 4 shown distinctly).

| Action  | Keyboard (P1)   | Gamepad (P2+)      |
|---------|-----------------|--------------------|
| Move    | WASD / arrows   | Left stick         |
| Aim     | Mouse           | Right stick        |
| Attack  | Space           | South (A/✕)        |
| Ability | E               | West (X/□)         |
| Equip   | F               | North (Y/△)        |
| Unequip | G               | East (B/○)         |
| Class   | 1/2/3/4         | D-pad ◄ / ▲ / ►    |
| Bank    | Tab             | —                  |
| Quit    | Esc / window ✕  | —                  |

### Multiplayer

**Local:** just run `make run` and plug in gamepads — each pad joins as a player.

**Online (LAN):** one machine hosts, others join.

```sh
./build/debug/jeu --host 7777            # host on port 7777
./build/debug/jeu --join 192.168.1.42 7777   # join that host
```

**Local and online mix.** The host keeps its own local players (keyboard +
gamepads) *and* accepts network clients. A joining machine can *also* have local
co-op: its keyboard and each gamepad each join as a separate player. So "one PC
with 2 people on the couch + a friend joining from another PC" works out of the
box.

The host runs the authoritative simulation; clients send input and render the
host's snapshots. LAN prototype — same-architecture machines, no lag
compensation yet.

## Project layout

```
jeu/
├── CMakeLists.txt        # build definition
├── CMakePresets.json     # debug / release presets
├── DESIGN.md             # design doc + build roadmap (north star)
├── data/                 # Lua content — edit + relaunch, no recompile
│   ├── classes.lua       # playable classes (stats + ability)
│   └── loot.lua          # loot table
├── src/
│   ├── main.cpp             # entry point
│   ├── Game.{hpp,cpp}       # app host: window, loop, input -> commands
│   ├── Simulation.{hpp,cpp} # owns all state + rules (never draws)
│   ├── Renderer.{hpp,cpp}   # draws the world (never mutates it)
│   ├── ScriptEngine.{hpp,cpp} # loads data/*.lua (only Lua-aware file)
│   ├── Net.{hpp,cpp}        # LAN host/client + snapshot serialization
│   ├── GameContent.hpp      # classes + loot table (+ built-in defaults)
│   ├── World.hpp            # the complete game state (+ Player, GroundItem)
│   ├── Command.hpp          # player intentions (Move/Attack/Ability/…)
│   ├── PlayerClass.hpp      # class stat block + ability
│   ├── Entity.hpp           # actor (player/enemy): pos, velocity, hp
│   ├── Projectile.hpp       # flying bolts
│   ├── Item.hpp             # item + deterministic loot table
│   ├── Inventory.hpp        # weight-limited carrying (seed of the shared bank)
│   └── Vec2.hpp             # minimal 2D vector math
└── tests/
    ├── vec2_test.cpp        # vector math
    ├── inventory_test.cpp   # weight / carrying rules
    ├── simulation_test.cpp  # sim behaviour (movement, attack, classes, aggro, xp)
    └── net_test.cpp         # snapshot / input serialization round-trips
```

## Conventions

- **C++20**, warnings on (`-Wall -Wextra -Wpedantic`), formatted with
  `clang-format` (`.clang-format`).
- **English in code** (identifiers, comments). Comments explain *why*, not *what*.
- **Tests** named `should_<outcome>_when_<condition>`, with `// given / when / then`.
- Gameplay content (enemies, abilities, classes, loot) will live in **Lua**;
  the C++ side stays the engine.

## Stack

| Concern            | Choice                          |
|--------------------|---------------------------------|
| Language / engine  | C++20, SDL3                     |
| Build / presets    | CMake (+ CMakePresets)          |
| Tests              | GoogleTest (via FetchContent)   |
| Scripting          | Lua 5.4 (C API), data files     |
| Networking         | POSIX TCP sockets (LAN)         |
| Audio (soon)       | miniaudio                       |
| UI / debug (soon)  | Dear ImGui                      |

## Notes

- SDL3 comes from Homebrew here (`find_package(SDL3 CONFIG)`). For a
  self-contained / cross-platform build later, switch to FetchContent or vcpkg.
