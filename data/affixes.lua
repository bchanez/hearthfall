-- Affix pool: modifiers gear can roll. Higher-rarity items roll more of them.
--
-- `stat` is one of: "maxHp" | "damage" | "attackSpeed" | "moveSpeed" | "crit"
--                 | "lifesteal".
-- maxHp/damage are flat; attackSpeed/moveSpeed/crit/lifesteal are percentages.
-- `min`/`max` bound the rolled magnitude. Edit freely and relaunch.

return {
  { stat = "maxHp",       min = 10, max = 40 },
  { stat = "damage",      min = 3,  max = 10 },
  { stat = "attackSpeed", min = 5,  max = 20 },
  { stat = "moveSpeed",   min = 5,  max = 15 },
  { stat = "crit",        min = 3,  max = 12 },
  { stat = "lifesteal",   min = 3,  max = 10 },
}
