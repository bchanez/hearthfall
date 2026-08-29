-- Level-up boons: on every level-up a player is offered 3 of these and picks
-- one. They STACK (take the same one twice → double the effect), layering a
-- survivor-like build on top of the persistent RPG stats — the road to feeling
-- like a god and chaining waves.
--
-- `effect` is one of:
--   "damage"      -- +magnitude% attack damage
--   "attackSpeed" -- +magnitude% attack speed (faster)
--   "moveSpeed"   -- +magnitude% move speed
--   "maxHp"       -- +magnitude% max HP (and heals the difference on pick)
--   "crit"        -- +magnitude% crit chance
--   "lifesteal"   -- +magnitude% of damage dealt returned as HP
--   "multiShot"   -- +magnitude extra ranged bolts per shot (fanned)
--   "pierce"      -- ranged bolts pass through +magnitude more enemies
--   "regen"       -- +magnitude passive HP regenerated per second
--   "dodge"       -- +magnitude% chance to avoid an incoming hit (capped 60%)
-- `magnitude` is granted per pick. This pool is the HOME for the huge variety of
-- characteristics (the manual stat menu is gone) — add your own rows and relaunch.

return {
  { name = "Sharpened Blade",  desc = "+15% damage",          effect = "damage",      magnitude = 15 },
  { name = "Frenzy",           desc = "+12% attack speed",    effect = "attackSpeed", magnitude = 12 },
  { name = "Fleet Feet",       desc = "+12% move speed",      effect = "moveSpeed",   magnitude = 12 },
  { name = "Vitality",         desc = "+20% max HP",          effect = "maxHp",       magnitude = 20 },
  { name = "Bulwark",          desc = "+15% max HP",          effect = "maxHp",       magnitude = 15 },
  { name = "Deadeye",          desc = "+8% crit chance",      effect = "crit",        magnitude = 8 },
  { name = "Vampiric",         desc = "+6% lifesteal",        effect = "lifesteal",   magnitude = 6 },
  { name = "Regeneration",     desc = "+3 HP per second",     effect = "regen",       magnitude = 3 },
  { name = "Evasion",          desc = "+8% dodge chance",     effect = "dodge",       magnitude = 8 },
  { name = "Split Shot",       desc = "+1 bolt per shot",     effect = "multiShot",   magnitude = 1 },
  { name = "Piercing Rounds",  desc = "bolts pierce +1",      effect = "pierce",      magnitude = 1 },
  { name = "Arcane Power",     desc = "+15% spell power",     effect = "spellPower",  magnitude = 15 },
  { name = "Ironhide",         desc = "+12% damage reduction", effect = "armor",      magnitude = 12 },
}
