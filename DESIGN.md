# Jeu — design & decisions (codename TBD)

> A 2D co-op ARPG in the Rucoy / Diablo spirit, built in C++ for the craft.
> Desktop-first, role-based teamwork, shared loot, persistent characters.
> This doc is the cold-start reference: read it to remember *why* every choice
> was made.

## The pitch

Top-down 2D action-RPG. You control a character, kill mobs, gain XP, level up,
use abilities, loot gear. Tougher mobs need a **party**. Distinct **classes with
real roles** (tank / healer / DPS) make fighting *together* feel like teamwork,
not four people mashing attack. Persistent characters and a **shared household
bank** mean your progress is always there — and friends are never too low-level
to play with you.

**North star:** [Rucoy Online](https://www.rucoyonline.com) — proof that one
person can build a profitable pixel MMORPG. Simple art, tight scope, years of
iteration. We study it, we don't clone it.

## Honest framing (read this before dreaming)

- **This is the craft / passion project.** The money project is batchmeal.
  Games are a lottery genre; the goal here is *learn C++ deeply, build something
  fun*. Any revenue is a bonus, not the plan.
- **What looks small is not small in total effort.** The hard parts are netcode,
  content (mobs/items/maps/balance), and — eventually — a playerbase. Rucoy took
  *years*, mostly solo.
- **Ship a fun single-player loop first.** If 5 minutes of solo play isn't fun,
  nothing downstream matters. Don't build the login screen before the game.

## Pillars

1. **We fight together, and everyone matters.** Roles (the trinity) make co-op
   feel like teamwork.
2. **Nobody is left behind.** Friends level-match on join and share the loot
   pool — no "you're too low-level to play with me".
3. **Loot is fluid and shared.** A household bank, not siloed characters.
4. **Learn low-level C++.** We own the engine (loop, rendering, collision,
   netcode); gameplay content is iterated in Lua.

## Decisions locked in

| Decision | Choice | Why |
|---|---|---|
| Language | **C++ (C++20)** | The craft/learning is the point; own the engine. |
| Framework | **SDL3** | Low-level 2D + input + audio; you see the machinery (chosen over raylib for control, and over SFML for its C-level API + best-in-class gamepad support). |
| Scripting | **Lua 5.4 (C API)** | Content (classes/loot, later mobs/abilities) as data files, added without recompiling. Wired in at step 6. (sol2 was tried but doesn't build on the current toolchain; the C API is enough and dependency-free.) |
| Perspective | **Top-down** | Iso is cooler but much harder (depth sort, coords). A *later* upgrade, not a starting choice. |
| Platform | **Desktop-first (PC)** | Matches "several players on PC". Browser/mobile is a trap for a first game. |
| Players | **Solo / 2p / 4p+ co-op** | Small authoritative-server model, not a full MMO. |
| Persistence | Persistent world + characters | Server owns the truth; clients render it. |
| Monetization | Cosmetics **later** | Only earns once a playerbase exists. Not v1. |

> **Framework note:** an earlier version of this doc locked in raylib. Superseded
> by SDL3 (2026-08) after weighing control, gamepad support and ecosystem.

## Classes & roles (the trinity)

Target roster (start with 3, grow to all):

| Class     | Role       | Hook                                        |
|-----------|------------|---------------------------------------------|
| Tank      | Tank       | Holds aggro, soaks damage, protects team    |
| Healer    | Support    | Heals, buffs, sustains the group            |
| Archer    | Ranged DPS | Sustained ranged damage, kiting             |
| Mage      | Ranged DPS | Burst / area damage, control                |
| Berserker | Melee DPS  | High-risk melee, damage ramps up            |
| Summoner  | Pet DPS    | Commands minions that fight for you         |

**Trinity requires enemy AGGRO/THREAT** (who does the monster chase?) — that's
what makes tanking/healing meaningful. **MVP: Tank, Healer, Archer** (covers the
whole trinity with 3). Currently implemented with placeholder stats + one
ability each; they fully shine once there are allies (co-op) and aggro.

## Progression & ownership model ⭐ (the distinctive part)

- **Each player owns a persistent character** — own class, own level, keeps
  progressing.
- **Shared household bank/stash** — loot goes into a common pool across all of
  your characters.
- **Fluid gear:** gear not equipped by a currently-played character flows back to
  the shared pool, so idle characters never lock away useful items.
  - Rule to handle: an item can't be duplicated — if two characters want it, only
    one gets it (ownership/locking on the bank).
- **Level-matching:** guests are scaled so nobody is useless (direction TBD).
- **Loot & weight:** items have weight → encumbrance limits carrying (real
  decisions). The solo `Inventory` today is the seed of this bank.

## Architecture principle — network-*shaped* from line 1

The #1 thing that kills these projects: building solo the naive way, then trying
to "add networking" later → half a rewrite. We avoid that.

**Build solo, but structure the code as if a server already exists — even when
the "server" is just a function call in the same program.**

Two rules, from day 1 (now implemented):

1. **Split simulation from rendering/input.**
   - `Simulation` (`src/Simulation.*`) owns all state (`World`) and rules
     (positions, HP, mob AI, damage, loot). It never draws, never touches SDL.
   - `Renderer` (`src/Renderer.*`) draws the world's state. It never mutates it.
   - `Game` (`src/Game.*`) is the host: window + loop; turns input into commands.
2. **All player actions are commands** (`src/Command.hpp`: `Move`, `Attack`,
   `Ability`, `SelectClass`…). A command is applied by the sim.
   - Solo: the command goes straight into the sim (local function call).
   - Online later: the exact same command just travels over the network first.
     Nothing else changes.

This is the Factorio lesson, scaled down: heavy sim in fast compiled code, and a
clean seam where the network will slot in.

### Migration path (each step reuses the last, no rewrite)

1. **Solo** — sim + renderer in one process. Make the loop fun. ✓
2. **Local co-op** — the sim is already authoritative; multiple command sources
   (keyboard + gamepads) feed one world. ✓
3. **Online (LAN)** — remote clients' input arrives as the same Commands; the
   host broadcasts snapshots. Client-side **interpolation** + auto-**reconnect**
   done. ✓ ← we are here. Still to harden: lag compensation (server rewind),
   delta-compressed snapshots, stable enemy ids for enemy interpolation,
   anti-cheat.

## Build order (each step is a playable game)

- [x] **1 — Foundation.** SDL3 window, fixed-timestep loop, player (keyboard +
  gamepad), chasing enemies, circle collision.
- [x] **2 — Loot & weight.** Melee attack, enemy HP/death, loot drops, weight-
  limited inventory, respawning waves, HUD.
- [x] **3 — Classes + architecture.** Sim/renderer split + command pattern.
  Tank/Archer/Healer with distinct stats, projectiles, one ability each, enemy
  contact damage, player HP/down-and-reset.
- [x] **4 — Local co-op.** Multiple players on one machine: keyboard = P1, each
  gamepad joins as its own player (drop-in / leave). Shared bank; enemies target
  the nearest player. Roles start to matter.
- [x] **5 — Enemy aggro/threat.** Each enemy has a per-player threat table:
  damage builds threat, the tank builds 5x + has *Taunt* (rips aggro) and a
  half-damage passive, healing draws aggro. Enemies chase the highest-threat
  player. Tanking & healing now matter — the trinity works.
- [x] **6 — Content to Lua.** Classes and loot tables live in `data/*.lua`,
  loaded at startup by `ScriptEngine` into plain structs (`GameContent`) — the
  sim stays script-free. Add/tune a class or item by editing Lua, no recompile
  (a data-only **Mage** ships as proof). Uses the Lua C API directly (sol2 was
  dropped — it doesn't build on the current toolchain). Mobs & ability
  *behaviours* to Lua still to come.
- [x] **7 — Progression + shared bank + level-matching.** Per-character XP and
  levels (scaling HP/damage); a joining player is level-matched to the party;
  class-switch keeps your level; a Tab **bank overlay** shows the shared stash.
  The bank **persists to disk** (`save/bank.lua`, host/local) across sessions.
- [x] **Equipment + fluid loot.** Items carry stat bonuses (weapon → damage,
  armor → max HP). Players **equip the best gear from the shared bank** (F / pad
  Y); when a player leaves, their gear **flows back to the pool** for others —
  the "fluid loot" rule. Fully wired through co-op and online (equip/unequip are
  commands; equipped bonuses ride in snapshots).
- [x] **8 — Online (LAN) + combined with local co-op.** `--host` runs the
  authoritative sim and broadcasts snapshots; `--join <ip>` sends input and
  renders. Built on the command seam — remote input becomes the same Commands as
  local. **Both modes mix:** the host keeps its own local players (keyboard +
  gamepads) *and* accepts network clients; a joining client opens one connection
  per local input source, so a joining PC can also have couch co-op. So "one PC
  with 2 players + another PC joining" just works. *Prototype:* same-arch LAN,
  trusts clients, no lag compensation/interpolation yet.
- [x] **9 — Make the solo loop fun.** Enemy **variety**: Grunt / Swarmer / Brute
  archetypes (distinct HP, speed, size, contact damage), colour-coded and mixed
  per wave. **Tension:** a **Boss** every 5th wave (huge, hard-hitting,
  knockback-resistant, flagged in the HUD) plus per-wave HP scaling, so
  difficulty ramps instead of "more of the same". **Juice:** enemies flash white
  and get knocked back when hit, plus a light screen-shake on kills/pickups
  (render-only, so it also works on remote snapshots). Higher-tier gear appears
  deeper in the loot cycle so equipment keeps paying off as waves escalate.
- [x] **10 — Item rarity.** Gear rolls a rarity (Common/Uncommon/Rare/Epic) on
  drop, biased by the killer's enemy type (bosses drop Epic/Rare). Rarity scales
  the item's bonuses + value and prefixes its name; the UI colours loot by
  rarity. Deterministic PRNG (fixed seed) so it stays reproducible/testable.
  Persists in the save and rides in snapshots.
- [x] **11 — Netcode hardening.** Client keeps a short snapshot buffer and
  **interpolates player positions** ~100 ms in the past for smooth motion;
  clients **auto-reconnect** after a dropped link (survives a host restart).
  SIGPIPE ignored so a dead peer never kills the process.
- [x] **12 — Affixes.** Gear rolls random affixes by rarity (Common 0 → Epic 3)
  from a Lua-defined pool (`data/affixes.lua`): **+HP, +damage, +attack speed,
  +move speed, +crit**. Equipped affixes aggregate into derived stats (faster
  attacks, faster movement, crit doubles a hit, bigger HP/damage). Shown in the
  bank overlay, persisted, and networked. *Still open:* multiple affixes of the
  same stat could stack oddly; a drag-and-drop inventory UI; skill trees.

**Success test (from the first milestone):** is 5 minutes fun to play? If yes →
grow it. If no → better to know now.

## Tech stack

| Concern            | Choice                          |
|--------------------|---------------------------------|
| Language / engine  | C++20, SDL3                     |
| Build / presets    | CMake (+ CMakePresets), Makefile wrapper |
| Tests              | GoogleTest (via FetchContent)   |
| Scripting          | Lua 5.4 via its C API (data files) |
| Audio (soon)       | miniaudio                       |
| UI / debug (soon)  | Dear ImGui                      |
| Networking         | POSIX TCP sockets (LAN prototype) |

## Explicitly parked (do NOT build yet)

- Networking / servers (architecture-ready, but stays *off* until the loop is fun)
- Isometric perspective
- Browser / mobile / "play everywhere"
- Monetization, cash shop, cosmetics
- Accounts, login, matchmaking
- Big content: many maps, deep item system, skill trees

## Open questions

- Project / game name (codename for now).
- Level-matching direction (match up to host? cap? scale stats only or abilities?).
- Bank item locking when multiple characters want the same gear.
- Persistent world structure: hub + dungeons vs open zones.
- Art direction (pixel art vs simple shapes for now).
