-- Loot table. The base item is picked at random on each drop, weighted by
-- `dropWeight` (relative frequency; default 1.0). Rarity + affixes are then
-- rolled on top (see Simulation). Picks use a seedable PRNG, so tests stay
-- reproducible.
--
-- `kind` is "gold" | "potion" | "weapon" | "armor".
-- `weight` feeds the shared bank's encumbrance limit.
-- `dropWeight` is how often this base item is chosen relative to the others.
-- `bonusDamage` (weapons) / `bonusMaxHp` (armor) apply when equipped.
-- `style` (weapons) is "melee" | "ranged" — the WEAPON now decides how you fight,
--   so finding a bow turns you into an archer on the spot.

return {
  { name = "Gold Coins",    weight = 0.0, kind = "gold",   value = 15, dropWeight = 6.0 },
  { name = "Health Potion", weight = 0.5, kind = "potion", value = 25, dropWeight = 4.0 },
  { name = "Rusty Sword",   weight = 3.0, kind = "weapon", value = 8,  bonusDamage = 6,  style = "melee",  dropWeight = 3.0 },
  { name = "Short Bow",     weight = 2.0, kind = "weapon", value = 10, bonusDamage = 7,  style = "ranged", dropWeight = 3.0 },
  { name = "Leather Armor", weight = 5.0, kind = "armor",  value = 12, bonusMaxHp = 20, dropWeight = 3.0 },
  { name = "Iron Shield",   weight = 8.0, kind = "armor",  value = 20, bonusMaxHp = 40, dropWeight = 2.0 },
  { name = "Ancient Relic", weight = 1.0, kind = "potion", value = 50, dropWeight = 1.0 },
  { name = "Great Axe",     weight = 6.0, kind = "weapon", value = 30, bonusDamage = 14, style = "melee",  dropWeight = 1.5 },
  { name = "Hunter Bow",    weight = 3.0, kind = "weapon", value = 34, bonusDamage = 16, style = "ranged", dropWeight = 1.2 },
  { name = "Runeblade",     weight = 5.0, kind = "weapon", value = 60, bonusDamage = 26, style = "melee",  dropWeight = 0.6 },
  { name = "Plate Armor",   weight = 10.0, kind = "armor", value = 45, bonusMaxHp = 70, dropWeight = 0.8 },
}
