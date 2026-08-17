# Backlog

> Living list of what we want to build, ordered by priority. Companion to
> `DESIGN.md` (the *why*) — this is the *what next*.
>
> **How we use it:** we move items between sections as priorities change.
> Each item has a size guess and a one-line "why it matters". Check the box
> when done and add a line to DESIGN.md's build order for anything shipped.
>
> Legend — size: **S** (hours) · **M** (a day-ish) · **L** (multi-session).
> Status: `[ ]` todo · `[~]` in progress · `[x]` done.

---

## North star for this backlog

Turn the current **wave-survival arena** into an **addictive open world with a
farm loop**, that also hosts co-op and D3/D4-style endless modes. The insight:
a **hub + instanced maps** system gives us farm, dungeons, rifts and survival
as four doors in the same building.

Core addiction loop we're serving: **kill → loot → get stronger → kill harder →
better loot**, with rewards at three time-scales (this fight / this session /
this week).

---

## Now — the foundation (do these first, in order)

- [ ] **1. Camera + world bigger than the screen** — *L, renderer-mostly.*
  Decouple world extent from screen size; camera follows the local player.
  Sim already works in world coords, so this is mostly `Renderer` + a camera
  offset. **Why:** unlocks "se promener dans la map" — the enabler for
  everything below. Co-op decision: camera follows the *local* player.

- [ ] **2. Aggro behaviors + aggro radius** — *M.*
  Per-`EnemyType`: `aggroRadius` + `behavior` (Passive / Defensive /
  Aggressive / Pack). Idle enemies only chase once a player enters range;
  Defensive ones return home. **Why:** the world stops sprinting at you — it
  feels alive, and passive mobs create safe zones for low-level players.

- [ ] **3. Zone-based enemy levels** — *M.*
  The map is the difficulty curve: near spawn = lvl 1–5, further out = 10/20/40.
  Reuse the per-wave HP/damage scaling, driven by a zone level instead.
  **Why:** walking outward = voluntarily raising the stakes (risk/reward
  exploration) for almost free.

---

## Next — the addiction engine

- [x] **4. Rolled loot with random affixes** — *shipped.*
  Rarity (Common→Epic, biased by enemy type) + rolled affixes from a Lua pool
  (`+hp`, `+dmg`, `+attack speed`, `+move speed`, `+crit`), aggregated from
  equipped gear, coloured in the UI, persisted, networked. **Why:** the single
  biggest fun lever after the camera — unpredictable rewards create "one more
  run". *Remaining:* the *base item* is still picked from a deterministic cycle
  (see "randomize base-item drops" below); lifesteal / on-hit affixes → Later.

- [ ] **5. Hub + one instanced dungeon** — *L.*
  A hub map with a portal to an instanced `World` that has an objective and a
  guaranteed reward chest at the end. **Why:** proves the portal system that
  farm / dungeons / rifts / survival all share.

- [ ] **6. Rift / pit mode (D3/D4)** — *M, on top of #5.*
  Endless instanced dungeon with a rising floor counter: each floor scales
  harder, push until you die, deepest floor = score. **Why:** high-intensity
  endgame chase; nearly free once #3 + #5 exist.

- [ ] **7. Inventory / stash UI (manual equip + compare)** — *L, adds Dear ImGui.*
  Right now gear is **auto-equip-best** (press F), which hides the decision. A
  real inventory: hover to compare, equip a specific piece, drop/keep. **Why:**
  with affixes live, *choosing* between a crit dagger and a beefy axe **is** the
  RPG — auto-equip throws that fun away. Also the natural home for consumables.

- [ ] **8. Consumables that do something** — *S/M.*
  Potions currently drop as dead weight. Make them usable (heal / temp buff),
  with a quick-use key. **Why:** gives the bank a purpose beyond gear, and adds
  an in-fight decision ("do I pop a potion now?").

---

## Later — the spice (fun to add)

- [ ] **Dash / dodge roll** — *S/M.* Movement that feels good = half of "ça donne
  envie" in an action game.
- [ ] **Elite / rare mobs** — *S.* Random mob with a golden aura, extra affixes,
  guaranteed rare drop. Little targets of desire scattered in the world.
- [ ] **Boss with a telegraphed attack** — *M.* A wind-up you can dodge, not just
  a big HP bar. One good boss fight sells the game.
- [ ] **Vendor + gold sink / affix reroll** — *M.* Loot you don't use still
  matters (sell / reroll). Closes the loop so nothing is wasted.
- [ ] **Damage popups + reward juice** — *S.* Make the numbers-go-up visible;
  extend existing hit-flash/screenshake to *reward* moments (level-up, rare drop).
- [ ] **Co-op shared goal** — *M.* World boss or an event ("defend the caravan")
  that needs a party. Reinforces pillar #1 (we fight together).
- [ ] **Set bonuses / build-defining items** — *L.* The long-term endgame chase.
- [ ] **Quests** — *M/L.* Simple objectives with rewards; gives direction to the
  open world beyond free farming.
- [ ] **Lifesteal & on-hit affixes** — *S.* The affix system already exists;
  lifesteal / "chance to burn/slow on hit" are cheap adds with huge build
  variety. Cheapest depth-per-effort lever we have now.
- [ ] **Status effects (burn / slow / stun)** — *M.* One shared substrate that
  powers on-hit affixes, elite mobs, boss telegraphs and future abilities.
  Build it once, reuse everywhere.
- [ ] **Ranged / caster enemies** — *M.* Everything melees you today, so kiting
  is a free win. Enemies that shoot force real positioning and make the trinity
  (peel, LoS, tank soak) matter more.
- [ ] **Skill tree / talents per class** — *L.* Spend a point per level on class
  perks. **Why:** build identity beyond gear; a second progression axis that
  makes leveling exciting, not just a stat bump.
- [ ] **Minimap + off-screen ally markers** — *S/M, after camera (#1).* Answers
  the co-op "players wander apart" open question and makes a bigger-than-screen
  world navigable.
- [ ] **Party-size scaling** — *S.* Enemy HP/count scales with active players so
  co-op isn't trivially easy. Reuses the wave/zone scaling already in place.
- [ ] **Randomize base-item drops** — *S.* Finish #4: pick the base item from a
  weighted table instead of the deterministic `lootCounter` cycle, so drops feel
  genuinely random (keep a seedable PRNG for reproducible tests).

---

## Tech / infra & feel (foundations, not features)

- [ ] **Persist characters, not just the bank** — *M.* Today only the shared
  bank survives a restart; player level/xp/class don't. The "persistent
  characters" pillar isn't real until they do. Save a roster keyed by a stable
  character id.
- [ ] **Stable enemy ids → enemy interpolation** — *M.* Enemies have no stable id,
  so the client can't interpolate them (only players are smoothed). Give each
  enemy an id and remote enemies stop stuttering.
- [ ] **Sound (miniaudio)** — *M.* Hits, deaths, pickups, level-up, rare drops.
  Highest feel-per-effort item on the list — silence is the biggest thing making
  it read as a prototype.
- [ ] **Sprites over squares** — *L.* Placeholder squares are fine for now, but
  simple sprites/animations are what turn "tech demo" into "a game". Do after the
  loop is proven fun.
- [ ] **Player separation / collision** — *S.* Players and enemies overlap freely;
  light push-apart makes bodies feel solid and stops stacking on one tile.
- [ ] **Stakes on death** — *M.* Death currently just refills and recentres.
  Something to lose (drop a bit of gold? lose the current run's progress in
  rifts?) makes the risk/reward of pushing deeper real.

---

## Parked (from DESIGN.md — do NOT build yet)

Isometric perspective · browser/mobile · monetization/cosmetics · accounts/login/
matchmaking · server-side lag compensation / rewind & anti-cheat.

*(Client-side interpolation + auto-reconnect are **done** — no longer parked.)*

---

## Open questions (decide as we reach them)

- Camera in co-op: follow local player confirmed — how do we handle players who
  wander far apart (leash? off-screen indicators?).
- Hub structure: single persistent hub vs. multiple town zones.
- Affix system: shipped as **one generic pool** for all gear. Open follow-up:
  do we want **per-kind pools** (e.g. crit only on weapons, HP only on armor)?
- Rift scoring: solo leaderboard now, or wait for online to be hardened.
