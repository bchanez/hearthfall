-- Affix pool: modifiers gear can roll. Higher-rarity items roll more of them AND
-- bigger magnitudes (rarity scales the roll — see Simulation::rollAffixes).
--
-- Affixes are filtered by the item's SLOT so gear stays coherent:
--   offense (damage, attackSpeed, crit, lifesteal, spellPower) → weapons
--   defense (maxHp, moveSpeed)                                 → body armour
--   either                                                     → off-hand + jewellery
--
-- `stat` is one of: "maxHp" | "damage" | "attackSpeed" | "moveSpeed" | "crit"
--                 | "lifesteal" | "spellPower".
-- maxHp/damage are flat; the rest are percentages.
-- `min`/`max` bound the rolled magnitude (before the rarity multiplier). Edit and relaunch.

return {
  { stat = "maxHp",       min = 10, max = 40 },
  { stat = "damage",      min = 3,  max = 10 },
  { stat = "attackSpeed", min = 5,  max = 20 },
  { stat = "moveSpeed",   min = 5,  max = 15 },
  { stat = "crit",        min = 3,  max = 12 },
  { stat = "lifesteal",   min = 3,  max = 10 },
  { stat = "spellPower",  min = 6,  max = 18 },
}
