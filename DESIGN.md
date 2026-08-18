# Jeu — design & decisions (codename TBD)

> A cosy **co-op monster-hunting** game in a **living, semi-procedural world**.
> You level up together, peacefully, until the world grows a predator worth
> hunting — then you track it and take it down as a group.
> Built in C++/SDL3 for the craft. This doc is the cold-start reference:
> read it to remember *why* every choice was made.

> **Direction reset (2026-08):** the earlier concept (trinity ARPG + wave
> survival, in the Rucoy/Diablo spirit) is superseded. The *engine* built for it
> is kept — see "Foundation already built". The *game* is now the hunt loop below.

## The pitch (one page)

**Fantasy:** "We're chilling in a world that lives its own life. Somewhere, a
predator is growing stronger. We pick up its trail, we get ready, and we hunt it
down together." A Monster Hunter feeling, cosy and drop-in, where **the prey
becomes the boss on its own**.

**The distinctive hook — the world grows its own bosses.** In Monster Hunter the
monsters are placed and scripted. Here the **living ecosystem produces the
targets**: a mob that survives, eats other mobs, and snowballs into an **alpha**.
Nobody scripts the boss — it *emerges*.

**The core loop (one session = one hunt):**
1. **Live the world** — explore / gather / level up together, peacefully, in a
   semi-procedural world.
2. **The world produces a threat** — the ecosystem grows an alpha. Visible signs:
   carcasses, fleeing mobs, a shifting territory. You don't get a quest marker;
   you read the world.
3. **Track** — follow the signs, Monster Hunter style, to locate it.
4. **The hunt** — the high point: skill combat, positioning, playable solo or as
   a group.
5. **Butcher & craft** — harvest **monster parts** → forge / upgrade gear.
   Progression is *crafting from parts* (Monster Hunter), **not** random Diablo
   loot: more thematic, far easier to balance.
6. **Next alpha, bigger** → up to a final **apex**. The game is *finished* when
   you bring it down.

**The tension that makes it ours — the alpha strengthens by killing.** Difficulty
is **not** a designer-set ramp. The longer an alpha is left alone, the more it
hunts and devours, the stronger it grows. Catch it early = easier fight; wait =
a nightmare. The choice "do we go now, or wait until we're geared?" is *player-
made*, and the danger is *emergent*, not scripted.

## Pillars (in priority order)

1. **The feel of the hunt.** Game-feel of combat, readability, sound & juice.
   If the fight doesn't feel great, nothing else matters. *(feel-first)*
2. **The living world as a target generator.** The ecosystem — mobs that hunt
   mobs, alphas that snowball — is what creates objectives, for almost free.
3. **Cosy drop-in co-op, play from anywhere.** Join at any time; PC or gamepad on
   the TV. A *technical* choice (netcode + input), handled apart — it does not
   change "what you do".

## Why this fits "small & finished"

- Content = **~5–6 monster archetypes**, each able to mutate into an alpha. The
  living world *recombines* them → lots of gameplay from few assets.
- The world is **generated** → no manual level design.
- Progression = **craft from parts** → a legible forge tree, not endless random
  loot tables to balance.
- A **clear finish line** (the apex) → it's *finishable*, unlike an endless
  sandbox.

## Cut on purpose (to actually finish)

No fixed classes / trinity, no big random-loot ARPG system, no narrative, no
base-building, no wave-survival mode. Just: **world → track → hunt → forge →
again, harder because *it* grew.**

## Foundation already built (the engine is an asset, not a rewrite)

The previous direction produced a solid, reusable engine. It carries over
unchanged; only the *game systems* on top change.

**Kept as-is:**
- **Sim / renderer / input split + command pattern** (`Simulation`, `Renderer`,
  `Game`, `Command`). Network-shaped from line 1.
- **Co-op, local + online (LAN).** `--host` runs the authoritative sim and
  broadcasts snapshots; `--join <ip>` sends input. Local couch co-op (keyboard +
  each gamepad = a player, drop-in/leave) mixes with networked clients. This is
  exactly the "play from anywhere, join anytime" plumbing.
- **Netcode hardening:** snapshot buffer + interpolation (~100 ms), stable enemy
  ids, auto-reconnect, SIGPIPE-safe.
- **Content in Lua** (`data/*.lua` via `ScriptEngine` → structs) — mobs, drops,
  affixes tuned without recompiling.
- **World bigger than the screen:** camera (follow-cam + party-frame), minimap,
  world coords throughout.
- **Aggro / behaviours:** per-enemy `AggroBehavior` (Passive/Defensive/
  Aggressive/Pack) + `aggroRadius`, leashing, pack-waking — the substrate a
  *living world* needs.
- **Zone-based levels:** `zoneLevel(pos)` makes distance-from-spawn the difficulty
  curve; idle, zone-scaled world mobs scattered across the map.
- **Feel:** hit flashes, knockback, screen-shake, floating damage numbers, dash /
  dodge roll with i-frames.

**Repurposed or dropped:**
- **Trinity classes (Tank/Healer/Archer…)** → dropped as fixed roles. Movement,
  attacks, abilities, dodge stay as the base of skill-combat (classless).
- **Wave survival + boss-every-5th-wave** → dropped. Replaced by *emergent*
  alphas grown by the living world.
- **Random loot / rarity / affixes** → demoted. Keep the item/stat plumbing, but
  the *source* becomes crafting from monster parts, not rarity rolls.
- **Shared household bank** → keep the shared-stash idea for co-op materials;
  drop the "fluid gear flows back" complexity for now.

## Architecture principle — network-*shaped* from line 1

The #1 thing that kills these projects: building solo the naive way, then trying
to "add networking" later → half a rewrite. We avoid that.

**Build solo, but structure the code as if a server already exists — even when
the "server" is just a function call in the same program.**

1. **Split simulation from rendering/input.** `Simulation` owns all state
   (`World`) and rules; never draws. `Renderer` draws; never mutates. `Game`
   hosts the loop and turns input into commands.
2. **All player actions are commands** (`Move`, `Attack`, `Ability`…). Solo: the
   command goes straight into the sim. Online: the same command travels over the
   network first. Nothing else changes.

This is the Factorio lesson, scaled down: heavy sim in fast compiled code, and a
clean seam where the network slots in.

## Decisions locked in

| Decision | Choice | Why |
|---|---|---|
| Language | **C++ (C++20)** | The craft/learning is the point; own the engine. |
| Framework | **SDL3** | Low-level 2D + input + audio; best-in-class gamepad support. |
| Scripting | **Lua 5.4 (C API)** | Content as data files, added without recompiling. |
| Perspective | **Top-down** (3/4 pixel art) | Iso is much harder (depth sort, coords); a later upgrade at most. |
| Platform | **Desktop + gamepad/TV, drop-in co-op** | "Play from anywhere, join anytime." Browser/mobile parked. |
| Players | **Solo / 2p / 4p+ co-op** | Small authoritative-server model, not a full MMO. |
| Progression | **Craft from monster parts** | Thematic (Monster Hunter), balanceable, few assets. |
| Difficulty | **Emergent (alpha grows by killing)** | Not a designer ramp; the world sets the stakes. |

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
| Art                | 3/4 pixel art, sprites via `tools/build_*.py` generators |

## Next milestone — the vertical slice (prove the fun first)

Smallest thing that tests whether the *hunt* is fun, before building the rest:

1. **One monster** that roams a small generated area and can **grow into an
   alpha** by killing other mobs (a visible power/size meter).
2. **One hunt:** track it (a couple of signs), fight it (existing skill-combat +
   dodge), bring it down solo or in co-op.
3. **Butcher:** it drops **parts**; one **forge** turns parts into a gear upgrade.

**Success test:** is one hunt fun? If yes → grow the archetype roster and the
world. If no → better to know now, cheaply.

## Explicitly parked (do NOT build yet)

- Big content: many biomes, deep forge tree, many archetypes (start with 1).
- Online hardening beyond LAN (lag comp, delta snapshots, anti-cheat).
- Isometric perspective; browser / mobile.
- Monetization, cosmetics, accounts, login, matchmaking.
- Narrative, base-building, wave-survival mode.

## Open questions

- Project / game name (codename for now).
- How the world **signals** a rising alpha (tracks? carcasses? map haze? a
  distant roar?) — the "tracking" UX is the heart of the loop.
- How fast an alpha grows, and whether it can wander into the safe/starting zone.
- Craft depth: how many parts per monster, how legible the forge tree stays.
- Semi-procedural world: fully generated vs a fixed safe hub + generated wilds.
- Classless combat: shared skill pool vs per-player loadout.
