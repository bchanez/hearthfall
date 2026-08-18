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

Two horizons, decided **feel-first** (2026-08-17):

1. **Prove the hook first — a couch game on the TV.** 2–4 friends on one screen,
   pick-up-and-play, *fun in 10 minutes*, with hidden depth for those who want
   it. The question this answers: **"ça donne envie ?"** Depth systems don't
   answer that — *feel* does (movement, juice, sound, playing together).
2. **Then the depth — an addictive open world with a farm loop**, co-op and
   D3/D4-style endless modes. Insight: a **hub + instanced maps** system gives
   farm, dungeons, rifts and survival as four doors in the same building.

Core addiction loop we're serving: **kill → loot → get stronger → kill harder →
better loot**, with rewards at three time-scales (this fight / this session /
this week). But horizon 1 comes first — no more depth until the couch hook lands.

---

## Now — feel-first couch sprint (do these BEFORE the depth backlog)

> Goal: a 10-minute couch session that **feels alive**. Sit two people in front
> of the TV and watch their faces — that's the real test of "ça donne envie".
> Ordered by *couch-fun per hour of work*. All of it is render/command work the
> architecture already supports (juice runs on remote snapshots too).

- [x] **A. Dash / dodge roll** — *shipped* (see "Later" below). The movement-feel
  win is already banked — Shift / RB, i-frames, ~1.1s cooldown, networked.

- [ ] **B. Sound (miniaudio)** — *M.* Hits, deaths, pickups, level-up, rare drop.
  **Why:** our own note — silence is the biggest thing making it read as a
  prototype. Highest feel-per-effort item on the whole list. *(Same as the
  "Sound" line under Tech/infra — pulled up here as a sprint priority.)*

- [x] **C. Damage popups + reward juice** — *shipped.* Floating damage numbers
  spawned render-side by diffing enemy HP per stable id (works on remote
  snapshots too); big hits skew red, all rise and fade. The screen-shake now also
  punches on **level-up** and **fresh rare+ drops**. **Why:** cheap dopamine,
  makes "numbers go up" visible.

- [x] **D. Shared-screen camera (couch decision)** — *shipped (v1: centroid;
  zoom-to-fit → follow-up).* A second `CameraMode::FrameParty` centres on the
  active party's **centroid**, so one TV frames everyone; `FollowLocal` stays for
  online/desktop. `Game` picks the mode by session type — **Local = FrameParty**,
  Host/Client = FollowLocal. **Follow-up:** zoom-to-fit so players who spread past
  a screen still stay framed (v1 keeps them centred but doesn't scale).

- [~] **E. Sprites over squares** — *v1 shipped (players + enemies).* Lua-defined
  pixel sprites (`data/sprites.lua`): tiny char-map grids + a hex palette, decoded
  by `ScriptEngine::loadSprites` and baked into nearest-neighbour textures by the
  `Renderer` at startup — **no PNGs, no image library**. Players draw their class
  sprite (tank/archer/healer) over a team-colour ground bar; enemies draw their
  archetype sprite (grunt/swarmer/brute/boss), both mirrored to face their
  direction, with the white hit-flash punched over the sprite. *Remaining:* loot
  & projectiles are still squares; per-frame animation; edit-art-live is already
  possible (relaunch, no recompile). *(See "Sprites" under Tech/infra.)*

- [ ] **F. Music / soundtrack** — *S/M.* A looping combat/ambient track (miniaudio,
  same dependency as SFX), volume-mixed under the SFX. **Why:** the biggest
  *mood* lever — SFX alone still reads as a tech demo; music is ~50% of the vibe
  of a couch game. Distinct from the "Sound" SFX item — do both.

- [ ] **G. Downed state + ally revive** — *M.* On lethal damage a player **goes
  down** (immobile, bleeding-out timer) instead of instantly resetting; a
  teammate stands near to **revive** them. Solo → the current reset. **Why:** the
  emotional core of pillar #1 ("we fight together") and *the* couch-co-op bonding
  moment ("hold on, I'm coming!"). Different from "Stakes on death" (which is
  about *losing* something). Wire the revive as a Command.

- [ ] **H. Loot beams / rarity pillars** — *S, render-only.* A vertical beam of
  light over dropped gear, coloured + taller by rarity; Epic gets a screen-noticed
  pillar. **Why:** the Diablo dopamine moment — "everyone runs for it". Cheap
  render work on top of the existing rarity colours; works on remote snapshots.

- [ ] **I. Hit-stop / impact freeze** — *S.* A 2–3 frame freeze on a big/crit hit
  (scale by damage). **Why:** what makes a hit feel like it *connects*; the next
  juice layer after popups + screenshake, tiny effort. Render/timing only.

**Couch v1 target:** shared-screen arena + dash (done) + sound + music + juice
(popups + beams + hit-stop) + downed/revive. **No** hub, rifts, or inventory UI.
Ship that, test it on the sofa, *then* decide.

> Dash, damage-popups/juice and the shared camera are already in — so the sprint
> is really **sound → music → loot beams → hit-stop → downed/revive**. That's the
> shortest path to sitting two people down and knowing.

---

## Now — the depth foundation (after the couch sprint lands)

- [x] **1. Camera + world bigger than the screen** — *shipped.*
  World extent decoupled from screen (default 2560×1440); `Renderer` holds a
  camera that centres on the *local* player (P1 local/host, `myId` client) and
  clamps to world edges. Mouse aim mapped back through the camera. HUD stays
  screen-fixed. **Why:** unlocks "se promener dans la map" — the enabler for
  everything below.

- [x] **2. Aggro behaviors + aggro radius** — *shipped.*
  Per-`EnemyType` `behavior` (Passive / Defensive / Aggressive / Pack) + an
  `aggroRadius`. Idle mobs only chase once a player enters range; Defensive
  leash back to `home`; Pack members wake nearby mates; Passive only fights when
  hit. Wave enemies spawn in a centred band already aggroed (a wave is an
  assault) with `home` = objective, so waves play as before while the system is
  ready for idle world/zone mobs. **Why:** the world stops sprinting at you.

- [x] **3. Zone-based enemy levels** — *shipped.*
  The map is the difficulty curve: `zoneLevel(pos)` maps distance from spawn to a
  level (near = 1–5, outer rings = 10/20/40). Idle, zone-scaled world mobs are
  scattered across the outer world (deterministic jittered grid, tougher
  archetypes deeper), HP/damage scaled per level, and XP reward scales with
  level. Level tags render above mobs (warmer = deadlier) and ride in snapshots.
  World mobs are excluded from wave-clear so the survival loop still ticks.
  **Why:** walking outward = voluntarily raising the stakes for almost free.

---

## Next — the addiction engine

- [x] **4. Rolled loot with random affixes** — *shipped.*
  Rarity (Common→Epic, biased by enemy type) + rolled affixes from a Lua pool
  (`+hp`, `+dmg`, `+attack speed`, `+move speed`, `+crit`), aggregated from
  equipped gear, coloured in the UI, persisted, networked. **Why:** the single
  biggest fun lever after the camera — unpredictable rewards create "one more
  run". *Base-item drops are now weighted-random too (see below).* Remaining:
  lifesteal / on-hit affixes → Later.

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

- [x] **8. Consumables that do something** — *shipped (heal; temp buff → Later).*
  Potions are usable: **Q** (keyboard) / **LB** (pad) consumes a Health Potion
  from the shared bank and heals 50% max HP, with a 1s cooldown and a "don't
  waste at full HP" guard. Wired through local, gamepad and networked input; HUD
  shows the potion count. **Why:** gives the bank a purpose beyond gear + an
  in-fight decision. *Remaining:* temp-buff potions (needs the status-effect
  substrate) — folded into "Status effects" / Later.

---

## Later — the spice (fun to add)

- [x] **Dash / dodge roll** — *shipped.* **Shift** (keyboard) / **RB** (pad)
  dashes a fast burst in the move direction (or aim when standing still) with
  i-frames through the roll and a ~1.1s cooldown. Wired through local, gamepad
  and networked input.
- [x] **Elite / rare mobs** — *shipped.* ~1 in 7 world mobs rolls **elite**: a
  golden aura (rendered + networked), ~2.2× HP, 1.5× contact damage, and **two
  guaranteed Rare+ drops** (so extra affixes come for free). Little targets of
  desire scattered through the zones.
- [x] **Predators / emergent alphas (living world)** — *shipped.* ~1 in 13 world
  mobs is a **predator**: a hyper-aggressive hunter that, when no player is near,
  stalks and **eats other mobs** — biting on a cadence, then absorbing the prey
  (heals to full, +18% HP / +12% damage / grows in size / +level each kill) and
  **scavenging its loot into a hoard** (which flows *up* the food chain when
  predators eat each other). Enough kills → an **alpha** (blood-red aura that
  thickens with each kill, pulsing once it's apex). Beat one and you inherit
  everything it accumulated: the hoard spills to the ground and its inflated
  level pays out big XP. The risk *is* the reward. *Next:* territoriality/leashing
  so the map thins gracefully, prey retaliation, packs, and network sync of the
  predator/alpha fields + aura.
- [ ] **Skill-expressive combat (the Mabinogi lever)** — *M/L, design pillar.*
  Combat where **player skill can beat level/gear** — reading the enemy and
  timing your input matters more than raw stats. Mabinogi's model: an active
  rock-paper-scissors of intents (Attack / Defend / Smash / Counter / Windmill)
  where the right *read* + *timing* wins, so a skilled low-level player can out-
  play an over-geared one. Our seeds already point here: **dash i-frames**
  (dodge the telegraph), **Taunt/threat** (positioning reads), **kiting**. Next
  concrete steps: (a) enemy **telegraphed wind-ups** you can dash/parry through
  — see the boss item below; (b) a **block / parry** with a timing window;
  (c) later, per-enemy attack *tells* so fights are read-and-react, not tank-and-
  spank. **Why:** this is exactly the "*simple to play, deep for those who want
  it*" promise of horizon 1 — the ceiling that keeps skilled players hooked
  without punishing casual ones. Depends on the **status-effect substrate** +
  telegraphs; a clean home for it is a small combat-intent state machine per
  actor. ⭐ Bastien-loved reference — keep it in view.

- [~] **Boss with a telegraphed attack** — *telegraph tech shipped; boss use → M.*
  The **Sorcerer** already telegraphs (wind-up ring → heavy blast you dash out
  of), proving the read-and-react loop. **Still to do:** give the wave-5 **Boss**
  its own telegraphed slam/AoE using the same `windup` mechanism.
- [ ] **Vendor + gold sink / affix reroll** — *M.* Loot you don't use still
  matters (sell / reroll). Closes the loop so nothing is wasted.
- [ ] **Damage popups + reward juice** — *S.* Make the numbers-go-up visible;
  extend existing hit-flash/screenshake to *reward* moments (level-up, rare drop).
- [ ] **Co-op shared goal** — *M.* World boss or an event ("defend the caravan")
  that needs a party. Reinforces pillar #1 (we fight together).
- [ ] **Set bonuses / build-defining items** — *L.* The long-term endgame chase.
- [ ] **Quests** — *M/L.* Simple objectives with rewards; gives direction to the
  open world beyond free farming.
- [x] **Lifesteal affix** — *shipped (burn/slow → needs status effects).*
  New `Lifesteal` affix (data-driven in `affixes.lua`): the attacker heals a
  capped % of damage dealt on every hit (melee or projectile). Aggregated in
  `PlayerStats`, rolls like any affix, rides in snapshots. On-hit burn/slow wait
  on the status-effect substrate.
- [~] **Status effects (burn / slow / stun)** — *substrate shipped; stun/on-hit → M.*
  `Entity` carries burn/poison/slow timers + DoT accumulator that tick each step
  on players *and* enemies (`tickStatus`). Slow scales movement; DoT chips HP. A
  **Spitter** enemy applies poison + slow on contact. **Still to do:** stun, and
  wiring player on-hit affixes (burn) onto enemies (substrate is ready).
- [x] **Ranged / caster enemies** — *shipped.* **Archer** (kites: holds distance,
  fires bolts on a cadence, backs off when crowded) and **Sorcerer** (channels a
  **telegraphed** heavy blast you can dash out of). Enemy projectiles are a
  `hostile` flag on `Projectile` that damages players (respecting i-frames). The
  telegraph wind-up rides in snapshots and renders as a pulsing red warning ring.
- [x] **Slime (splits on death)** — *shipped.* A slow blob that bursts into two
  smaller, angrier slimes on death (until too small to split). Spawns are queued
  and flushed after the enemy loops so we never realloc a vector mid-iteration.
- [~] **Classless progression (Mabinogi model)** — *Phase 1 shipped; 2–4 → L.*
  Big direction change (2026-08-17): drop fixed classes; everyone starts a blank
  human and **specializes by playing**. Identity comes from two time-scales —
  instant (the weapon you hold) + long (the stats you pump + skills that rise by
  use). Class becomes an emergent *title*, not a cage. Resolves the "why 4 persos
  / why Mage = archer" mess (Mage `id="archer"` → same sprite+IA as the Archer).
  **Phase 1 (done):** a `Stats` block (STR/DEX/INT/VIT/AGI) on the Player; HP/dmg/
  speed/atk-speed/crit derive from stats (class = level-1 baseline only); level-up
  **banks a point** you spend into a stat via a **non-pausing** character sheet
  (`C` / pad D-pad), so allocating mid-fight in the open world is a real risk;
  level-matched drop-ins auto-allocate along a class-preferred order; stats +
  points + XP ride in snapshots; INT boosts Mend. **Phase 2:** the equipped weapon
  drives attack style (bow→ranged, sword→melee), everyone spawns "Human". **Phase
  3:** skills rise by use + emergent title + ability unlocks at thresholds.
  **Phase 4:** remove `ClassId` entirely; sprite = body + weapon in hand.
  Supersedes the "Skill tree / talents per class" idea below (folded in) and the
  DESIGN.md "classes locked-in" decision.
- [ ] **Skill tree / talents per class** — *folded into classless progression above.*
  Was: spend a point per level on class perks. The stat-per-level choice now
  covers the "second progression axis"; deeper perk trees can layer on later.
- [x] **Minimap** — *shipped (off-screen ally arrows → follow-up).* A corner
  minimap shows the whole world: enemy dots (elites gold), ally dots in player
  colours, and the current camera-view box. Makes the bigger-than-screen world
  navigable and shows where allies are. *Follow-up:* directional arrows for allies
  that are off the minimap edge (they're already all on it, so low priority).
- [x] **Party-size scaling** — *shipped.* Wave enemy **count** (+2/extra player)
  and **HP** (+50%/extra player) scale with the active party, on top of the
  per-wave ramp, so a full group isn't trivially easy.
- [x] **Randomize base-item drops** — *shipped.* Base item is now a weighted
  random pick (`dropWeight` per loot entry, data-driven in `loot.lua`) off the
  seedable PRNG, replacing the `lootCounter` cycle. Common items (gold/potions)
  weighted up, top-end gear down. Reproducible (same seed → same drops).

---

## Couch UX & combat depth (what makes it *cool*, beyond the feel sprint)

> Gaps found analysing the backlog (2026-08-17): the feel sprint makes it *feel*
> good; these make it *play* cool and read as a real game on the sofa.

- [ ] **Ability kit per class (2–3 abilities + cooldowns)** — *M/L.* Each class
  has only **one** ability today. Give each a small kit with cooldowns so there's
  something to *press and combo*. **Why:** the baseline "each class is fun to
  pilot solo" — the biggest combat-depth hole, and the ground the skill tree
  builds on. Wire abilities as Commands; cooldowns in `PlayerStats`.

- [ ] **Controller-first lobby + pad-navigable menus** — *M.* A **"press A to
  join"** lobby, controller **class-pick**, and a pad-navigable bank/inventory
  (bank is **Tab / keyboard-only** today). **Why:** for a TV game where *chacun
  joue sur le pad*, a newcomer must grab a pad and go without touching the
  keyboard. Onboarding readability = half of "pick-up-and-play".

- [ ] **Player identity at join (name + colour)** — *S.* Pick a colour (and maybe
  a name) when you join. **Why:** for "chacun joue", people want to be *their*
  guy — "I'm the red one". Cheap, and distinct from the parked cosmetics.

- [ ] **Breakable world objects (barrels / crates / chests)** — *S/M.* Scatter
  destructible props that pop coins/loot and openable chests. **Why:** instant
  tactile satisfaction and a reason to explore — turns the empty arena into a
  *world*. Reuses hit/knockback + loot-drop code.

---

## Tech / infra & feel (foundations, not features)

- [ ] **Persist characters, not just the bank** — *M.* Today only the shared
  bank survives a restart; player level/xp/class don't. The "persistent
  characters" pillar isn't real until they do. Save a roster keyed by a stable
  character id.
- [x] **Stable enemy ids → enemy interpolation** — *shipped.* Each enemy carries
  a stable `id` (monotonic, assigned at spawn) that rides in snapshots. The
  client matches enemies across the two bracketing snapshots by id and lerps
  their positions — remote enemies (and world mobs) no longer stutter.
- [ ] **Sound (miniaudio)** — *M.* Hits, deaths, pickups, level-up, rare drops.
  Highest feel-per-effort item on the list — silence is the biggest thing making
  it read as a prototype.
- [~] **Sprites over squares** — *v2 shipped (detailed + animated chars); more → L.*
  Lua character sheets (`data/sprites.lua`) now carry `palette` + `states`
  (idle / walk / attack), each state a list of frames; a `frame(upper, lower)`
  helper glues a shared body to per-frame legs so a walk cycle only redraws the
  feet. Bigger, shaded 12×13 players (tank/archer/healer, sword/bow/staff attack
  poses) and 2-frame-walk enemies (grunt/swarmer/brute/boss). The `Renderer`
  groups `base.state.frame` textures into animations and picks the frame from a
  render clock: **walk when moving, attack when swinging, idle otherwise**, flipped
  to face travel/aim, with **soft drop-shadows** grounding every actor. Baked at
  startup, no image library, edit-art-and-relaunch. **Still squares:** loot,
  projectiles. **Still to do:** hit/death/dash anims, per-direction (up/down)
  frames, loot/bolt sprites.
- [~] **Living tile map** — *v3 shipped (32×32 layered terrain); biomes + coherence → L.*
  Rebuilt on a dark-fantasy 32×32 art direction, generated by `tools/build_tiles.py`
  (procedural, seamless/wrapping) → `data/tiles.lua`. Three render layers now:
  **base** (grass/dirt/rock/stone, seamless 32×32) → **overlay** (transparent
  decals: moss/mud/water/rubble/corruption + **directional fringe** transition
  tiles that tear the straight ring seams into organic edges, drawn block-sized so
  a patch spans several tiles) → **props**. `Renderer::drawGround` does Pass 1
  (base) / Pass 1.5 (fringe transitions + block detail) / Pass 2 (props). Still
  ring-keyed by distance from spawn. **Still to do (Bastien, 2026-08-17):**
  (a) **biomes** — forest, sand, dark forest, plains, dry/dead land — the ground
  should be biome-aware, not just difficulty rings; (b) **map-generation
  coherence** — how tiles get placed when a map is generated (contiguous regions,
  sensible adjacency), a separate concern from the assets themselves;
  (c) per-material ground **variants** (needs a small renderer change to pick among
  `grass_01..` per cell); collidable scenery; animated water/foliage; a proper
  tilemap format for hand-designed maps.
- [x] **Player separation / collision** — *shipped.* A light push-apart pass
  separates overlapping same-kind bodies (player-player, enemy-enemy) each step —
  half the overlap each, gentle factor, re-clamped to bounds. Bodies read solid
  and stop stacking on one tile. Player-vs-enemy is left to contact/melee.
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

- Camera in co-op: follow local player confirmed. Wandering-apart is partly
  answered — a **minimap** now shows all allies. Still open: a leash, or
  directional arrows for allies off the minimap edge, if it proves needed.
- Hub structure: single persistent hub vs. multiple town zones.
- Affix system: shipped as **one generic pool** for all gear. Open follow-up:
  do we want **per-kind pools** (e.g. crit only on weapons, HP only on armor)?
- Rift scoring: solo leaderboard now, or wait for online to be hardened.
