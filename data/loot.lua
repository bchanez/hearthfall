-- Loot table. Items drop in order as enemies die (deterministic for now).
--
-- `kind` is "gold" | "potion" | "weapon" | "armor".
-- `weight` feeds the shared bank's encumbrance limit.
-- `bonusDamage` (weapons) / `bonusMaxHp` (armor) apply when equipped.

return {
  { name = "Gold Coins",    weight = 0.0, kind = "gold",   value = 15 },
  { name = "Health Potion", weight = 0.5, kind = "potion", value = 25 },
  { name = "Rusty Sword",   weight = 3.0, kind = "weapon", value = 8,  bonusDamage = 6 },
  { name = "Leather Armor", weight = 5.0, kind = "armor",  value = 12, bonusMaxHp = 20 },
  { name = "Iron Shield",   weight = 8.0, kind = "armor",  value = 20, bonusMaxHp = 40 },
  { name = "Ancient Relic", weight = 1.0, kind = "potion", value = 50 },
  { name = "Great Axe",     weight = 6.0, kind = "weapon", value = 30, bonusDamage = 14 },
  { name = "Runeblade",     weight = 5.0, kind = "weapon", value = 60, bonusDamage = 26 },
  { name = "Plate Armor",   weight = 10.0, kind = "armor", value = 45, bonusMaxHp = 70 },
}
