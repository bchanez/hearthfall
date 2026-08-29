-- Auto-casting abilities, drafted on level-up alongside the passive boons. Once
-- acquired, each fires on its own cooldown (no key to press) — stacking several
-- is the survivor-like god fantasy. Re-picking one you already own RANKS it up
-- (stronger, and a touch faster).
--
-- Spell DISCOVERY is gated by the weapon you're WIELDING at level-up, and by how
-- much you've MASTERED that weapon family (Mabinogi-style — play the weapon to
-- unlock its deeper spells). A spell is only offered when:
--   `weapon`   matches your equipped weapon: a class ("sword", "axe", "mace",
--                "greatsword", "greataxe", "greatmace", "bow", "crossbow", "wand",
--                "staff"), a style ("melee"/"ranged"/"magic"), or "any".
--   `minSkill` your mastery of that weapon family has reached this level.
--
-- `effect` — the delivery shape:
--   "nova"   -- AoE burst around you; magnitude = damage
--   "volley" -- fire bolts in a full ring; magnitude = bolt count
--   "bolt"   -- auto-fire a bolt at the nearest enemy; magnitude = damage
--   "chain"  -- lightning that leaps from the nearest foe to nearby others
-- `status` — an on-hit RIDER layered on the delivery, so the same shape feels
--            different per weapon: "burn"/"bleed" (damage over time), "slow"/
--            "chill", "stun" (frozen), "knock" (shoved back). `statusDur` seconds,
--            `statusPower` = burn damage/sec (ignored by slow/stun/knock).
-- `cooldown` seconds between auto-casts. `magnitude` per the effect above.
-- Edit freely and relaunch.

return {
  -- Universal (any weapon, incl. unarmed) -----------------------------------
  { name = "Nova Blast",   desc = "AoE burst around you",       weapon = "any", effect = "nova", cooldown = 3.0, magnitude = 16, minSkill = 1 },
  { name = "Spirit Bolt",  desc = "auto-fire at nearest foe",   weapon = "any", effect = "bolt", cooldown = 1.4, magnitude = 12, minSkill = 1 },

  -- Sword (1H) — agile, all-round ------------------------------------------
  { name = "Blade Cyclone", desc = "spin, cutting all around",  weapon = "sword", effect = "nova",   cooldown = 3.0, magnitude = 16, minSkill = 1 },
  { name = "Phantom Blades", desc = "loose a fan of blades",    weapon = "sword", effect = "volley", cooldown = 3.4, magnitude = 6,  minSkill = 4 },
  { name = "Blade Storm",   desc = "a relentless flurry",       weapon = "sword", effect = "nova",   cooldown = 2.2, magnitude = 24, minSkill = 8 },

  -- Axe (1H) — raw damage & bleeding ---------------------------------------
  { name = "Rending Cleave", desc = "a wide, bleeding swing",   weapon = "axe", effect = "nova", cooldown = 2.8, magnitude = 20, minSkill = 1, status = "bleed", statusDur = 3.0, statusPower = 6 },
  { name = "Whirling Rage",  desc = "faster, angrier spins",    weapon = "axe", effect = "nova", cooldown = 2.0, magnitude = 18, minSkill = 5, status = "bleed", statusDur = 3.0, statusPower = 8 },
  { name = "Executioner",    desc = "a felling blow, one foe",  weapon = "axe", effect = "bolt", cooldown = 2.5, magnitude = 30, minSkill = 9 },

  -- Mace (1H) — concussive control -----------------------------------------
  { name = "Crushing Blow", desc = "a stunning strike",         weapon = "mace", effect = "bolt",   cooldown = 2.4, magnitude = 22, minSkill = 1, status = "stun", statusDur = 0.8 },
  { name = "Shockwave",     desc = "a ring of shoving force",   weapon = "mace", effect = "volley", cooldown = 3.2, magnitude = 6,  minSkill = 5, status = "knock" },
  { name = "Cataclysm",     desc = "shatter and stun all near", weapon = "mace", effect = "nova",   cooldown = 3.6, magnitude = 28, minSkill = 9, status = "stun", statusDur = 1.3 },

  -- Greatsword (2H) — sweeping ---------------------------------------------
  { name = "Whirlwind",    desc = "a wide, ceaseless spin",     weapon = "greatsword", effect = "nova",   cooldown = 2.0, magnitude = 22, minSkill = 1 },
  { name = "Sweeping Arc", desc = "a long carving arc",         weapon = "greatsword", effect = "volley", cooldown = 3.0, magnitude = 8,  minSkill = 5 },
  { name = "Colossus Slam", desc = "a devastating overhead",    weapon = "greatsword", effect = "nova",   cooldown = 3.2, magnitude = 36, minSkill = 9, status = "knock" },

  -- Great Axe (2H) — frenzy & bleed ----------------------------------------
  { name = "Reckless Spin", desc = "a fast, wild whirl",        weapon = "greataxe", effect = "nova", cooldown = 1.8, magnitude = 20, minSkill = 1, status = "bleed", statusDur = 2.5, statusPower = 7 },
  { name = "Rampage",       desc = "spin without stopping",     weapon = "greataxe", effect = "nova", cooldown = 1.5, magnitude = 18, minSkill = 6, status = "bleed", statusDur = 2.5, statusPower = 10 },
  { name = "Decapitate",    desc = "an execution stroke",       weapon = "greataxe", effect = "bolt", cooldown = 2.4, magnitude = 40, minSkill = 10 },

  -- Great Mace (2H) — seismic, stunning ------------------------------------
  { name = "Ground Slam",   desc = "a quaking, stunning impact", weapon = "greatmace", effect = "nova",   cooldown = 3.0, magnitude = 30, minSkill = 1, status = "stun", statusDur = 1.0 },
  { name = "Seismic Ring",  desc = "a shockwave that shoves",    weapon = "greatmace", effect = "volley", cooldown = 3.6, magnitude = 8,  minSkill = 5, status = "knock" },
  { name = "Earthshatter",  desc = "split the ground, stunning", weapon = "greatmace", effect = "nova",   cooldown = 4.0, magnitude = 44, minSkill = 10, status = "stun", statusDur = 1.6 },

  -- Bow (2H) — volleys ------------------------------------------------------
  { name = "Arrow Volley", desc = "loose arrows all around",    weapon = "bow", effect = "volley", cooldown = 3.5, magnitude = 8,  minSkill = 1 },
  { name = "Snipe",        desc = "a precise, heavy shot",      weapon = "bow", effect = "bolt",   cooldown = 1.6, magnitude = 20, minSkill = 5 },
  { name = "Arrow Storm",  desc = "a sky-darkening volley",     weapon = "bow", effect = "volley", cooldown = 3.0, magnitude = 12, minSkill = 9 },

  -- Crossbow (2H) — rapid, piercing ----------------------------------------
  { name = "Rapid Fire",    desc = "fire bolt after bolt",      weapon = "crossbow", effect = "bolt",   cooldown = 0.9, magnitude = 12, minSkill = 1 },
  { name = "Piercing Bolt", desc = "a bolt that shoves a row",  weapon = "crossbow", effect = "bolt",   cooldown = 1.8, magnitude = 24, minSkill = 5, status = "knock" },
  { name = "Bolt Barrage",  desc = "a spray of bolts",          weapon = "crossbow", effect = "volley", cooldown = 3.2, magnitude = 10, minSkill = 9 },

  -- Wand (1H magic) — nimble, chilling arcana ------------------------------
  { name = "Arcane Missile", desc = "a homing arcane dart",     weapon = "wand", effect = "bolt", cooldown = 1.2, magnitude = 14, minSkill = 1 },
  { name = "Frost Burst",    desc = "a chilling, slowing nova", weapon = "wand", effect = "nova", cooldown = 2.6, magnitude = 18, minSkill = 5, status = "slow", statusDur = 2.5 },
  { name = "Arcane Barrage", desc = "darts in every direction", weapon = "wand", effect = "volley", cooldown = 3.2, magnitude = 8, minSkill = 9 },

  -- Staff (2H magic) — artillery & lightning -------------------------------
  { name = "Fireball",       desc = "hurl a burning bolt",      weapon = "staff", effect = "bolt",  cooldown = 1.6, magnitude = 22, minSkill = 1, status = "burn", statusDur = 3.0, statusPower = 8 },
  { name = "Chain Lightning", desc = "arcs between foes",       weapon = "staff", effect = "chain", cooldown = 2.4, magnitude = 20, minSkill = 5 },
  { name = "Meteor",         desc = "call down a burning rock", weapon = "staff", effect = "nova",  cooldown = 3.4, magnitude = 34, minSkill = 9, status = "burn", statusDur = 3.0, statusPower = 12 },
}
