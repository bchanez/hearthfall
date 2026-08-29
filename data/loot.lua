-- Loot table. The base item is picked at random on each drop, weighted by
-- `dropWeight` (relative frequency; default 1.0). Rarity + affixes are then
-- rolled on top (see Simulation). Picks use a seedable PRNG, so tests stay
-- reproducible.
--
-- `kind`   is "gold" | "potion" | "weapon" | "armor". Weapons live in the main
--          hand; everything else you wear (shields, focuses, armour, jewellery)
--          is "armor" plus a `slot` — the kind just decides drink/currency and
--          which items roll affixes (weapon + armor do).
-- `slot`   is the paperdoll slot: "mainhand" | "offhand" | "head" | "chest" |
--          "hands" | "legs" | "feet" | "amulet" | "ring".
-- `hands`  (weapons) 1 = one-handed (frees the off-hand for a shield / focus / a
--          second weapon to dual-wield), 2 = two-handed (claims both hands).
-- `style`  (weapons) "melee" (STR) | "ranged" (DEX) | "magic" (INT) — the weapon
--          decides how you fight AND which characteristic powers it.
-- `weight` feeds the shared bank's encumbrance limit (heavy plate slows you).
-- `bonusDamage` (weapons) / `bonusMaxHp` (armour) apply when equipped.

return {
  -- Currency & consumables --------------------------------------------------
  { name = "Gold Coins",    weight = 0.0, kind = "gold",   value = 15, dropWeight = 6.0 },
  { name = "Health Potion", weight = 0.5, kind = "potion", value = 25, dropWeight = 4.0 },
  { name = "Ancient Relic", weight = 1.0, kind = "potion", value = 50, dropWeight = 1.0 },

  -- Melee weapons (STR). `weaponClass` groups them for spell discovery (see
  -- abilities.lua): each class unlocks different auto-cast spells on level-up.
  -- One-handed --
  { name = "Rusty Sword", weight = 3.0, kind = "weapon", value = 8,  bonusDamage = 6,  style = "melee", weaponClass = "sword", slot = "mainhand", hands = 1, dropWeight = 3.0 },
  { name = "Iron Sword",  weight = 3.0, kind = "weapon", value = 18, bonusDamage = 12, style = "melee", weaponClass = "sword", slot = "mainhand", hands = 1, dropWeight = 1.6 },
  { name = "Fine Blade",  weight = 3.0, kind = "weapon", value = 26, bonusDamage = 15, style = "melee", weaponClass = "sword", slot = "mainhand", hands = 1, dropWeight = 1.2 },
  { name = "Hand Axe",    weight = 3.5, kind = "weapon", value = 14, bonusDamage = 11, style = "melee", weaponClass = "axe",   slot = "mainhand", hands = 1, dropWeight = 2.0 },
  { name = "War Axe",     weight = 4.0, kind = "weapon", value = 28, bonusDamage = 16, style = "melee", weaponClass = "axe",   slot = "mainhand", hands = 1, dropWeight = 1.1 },
  { name = "Iron Mace",   weight = 4.0, kind = "weapon", value = 16, bonusDamage = 10, style = "melee", weaponClass = "mace",  slot = "mainhand", hands = 1, dropWeight = 2.0 },
  { name = "Flanged Mace",weight = 4.5, kind = "weapon", value = 30, bonusDamage = 15, style = "melee", weaponClass = "mace",  slot = "mainhand", hands = 1, dropWeight = 1.1 },
  -- Two-handed --
  { name = "Greatsword",  weight = 6.5, kind = "weapon", value = 40, bonusDamage = 24, style = "melee", weaponClass = "greatsword", slot = "mainhand", hands = 2, dropWeight = 1.2 },
  { name = "Runeblade",   weight = 5.0, kind = "weapon", value = 60, bonusDamage = 30, style = "melee", weaponClass = "greatsword", slot = "mainhand", hands = 2, dropWeight = 0.6 },
  { name = "Great Axe",   weight = 6.0, kind = "weapon", value = 30, bonusDamage = 22, style = "melee", weaponClass = "greataxe",   slot = "mainhand", hands = 2, dropWeight = 1.5 },
  { name = "Great Maul",  weight = 8.0, kind = "weapon", value = 46, bonusDamage = 28, style = "melee", weaponClass = "greatmace",  slot = "mainhand", hands = 2, dropWeight = 0.9 },

  -- Ranged weapons (DEX) — all two-handed -----------------------------------
  { name = "Short Bow",       weight = 2.0, kind = "weapon", value = 10, bonusDamage = 7,  style = "ranged", weaponClass = "bow",      slot = "mainhand", hands = 2, dropWeight = 3.0 },
  { name = "Hunter Bow",      weight = 3.0, kind = "weapon", value = 34, bonusDamage = 16, style = "ranged", weaponClass = "bow",      slot = "mainhand", hands = 2, dropWeight = 1.2 },
  { name = "Master Crossbow", weight = 4.0, kind = "weapon", value = 58, bonusDamage = 26, style = "ranged", weaponClass = "crossbow", slot = "mainhand", hands = 2, dropWeight = 0.6 },

  -- Magic weapons (INT). Their inherent `spellPower` affix lifts your auto-cast
  -- spells above their base magnitude — a caster weapon is worth more than its
  -- bonusDamage suggests. (2-handed staves carry more than 1-handed wands.)
  { name = "Apprentice Wand", weight = 1.5, kind = "weapon", value = 12, bonusDamage = 7,  style = "magic", weaponClass = "wand",  slot = "mainhand", hands = 1, dropWeight = 2.0, affixes = { { stat = "spellPower", magnitude = 10 } } },
  { name = "Sorcerer Wand",   weight = 1.5, kind = "weapon", value = 36, bonusDamage = 14, style = "magic", weaponClass = "wand",  slot = "mainhand", hands = 1, dropWeight = 1.0, affixes = { { stat = "spellPower", magnitude = 20 } } },
  { name = "Oak Staff",       weight = 3.5, kind = "weapon", value = 24, bonusDamage = 12, style = "magic", weaponClass = "staff", slot = "mainhand", hands = 2, dropWeight = 1.5, affixes = { { stat = "spellPower", magnitude = 25 } } },
  { name = "Runewood Staff",  weight = 3.5, kind = "weapon", value = 60, bonusDamage = 24, style = "magic", weaponClass = "staff", slot = "mainhand", hands = 2, dropWeight = 0.6, affixes = { { stat = "spellPower", magnitude = 40 } } },

  -- Off-hand: shields (defensive) & focuses (caster spell power) ------------
  { name = "Iron Shield",  weight = 8.0,  kind = "armor", value = 20, bonusMaxHp = 40, slot = "offhand", dropWeight = 2.0 },
  { name = "Tower Shield", weight = 12.0, kind = "armor", value = 44, bonusMaxHp = 70, slot = "offhand", dropWeight = 0.8 },
  { name = "Arcane Tome",  weight = 1.0,  kind = "armor", value = 22, bonusMaxHp = 8,  slot = "offhand", dropWeight = 1.2, affixes = { { stat = "spellPower", magnitude = 20 } } },
  { name = "Spirit Orb",   weight = 1.0,  kind = "armor", value = 40, bonusMaxHp = 12, slot = "offhand", dropWeight = 0.7, affixes = { { stat = "spellPower", magnitude = 35 } } },

  -- Head --------------------------------------------------------------------
  { name = "Leather Cap", weight = 1.5, kind = "armor", value = 8,  bonusMaxHp = 8,  slot = "head", dropWeight = 2.0 },
  { name = "Iron Helm",   weight = 3.0, kind = "armor", value = 22, bonusMaxHp = 18, slot = "head", dropWeight = 1.2 },

  -- Chest -------------------------------------------------------------------
  { name = "Leather Armor",    weight = 5.0,  kind = "armor", value = 12, bonusMaxHp = 20, slot = "chest", dropWeight = 3.0 },
  { name = "Iron Breastplate", weight = 8.0,  kind = "armor", value = 30, bonusMaxHp = 40, slot = "chest", dropWeight = 1.5 },
  { name = "Plate Armor",      weight = 12.0, kind = "armor", value = 45, bonusMaxHp = 70, slot = "chest", dropWeight = 0.8 },

  -- Hands / Legs / Feet -----------------------------------------------------
  { name = "Leather Gloves",  weight = 1.0, kind = "armor", value = 8,  bonusMaxHp = 6,  slot = "hands", dropWeight = 2.0 },
  { name = "Gauntlets",       weight = 2.5, kind = "armor", value = 24, bonusMaxHp = 14, slot = "hands", dropWeight = 1.0 },
  { name = "Leather Greaves", weight = 2.0, kind = "armor", value = 10, bonusMaxHp = 8,  slot = "legs",  dropWeight = 2.0 },
  { name = "Plate Legs",      weight = 5.0, kind = "armor", value = 28, bonusMaxHp = 20, slot = "legs",  dropWeight = 1.0 },
  { name = "Worn Boots",      weight = 1.0, kind = "armor", value = 8,  bonusMaxHp = 6,  slot = "feet",  dropWeight = 2.0 },
  { name = "Iron Greaves",    weight = 2.5, kind = "armor", value = 22, bonusMaxHp = 12, slot = "feet",  dropWeight = 1.0 },

  -- Jewellery (the wildcard slots — real value comes from rolled affixes) ---
  { name = "Amulet of Vigor", weight = 0.2, kind = "armor", value = 20, bonusMaxHp = 10, slot = "amulet", dropWeight = 1.2 },
  { name = "Silver Pendant",  weight = 0.2, kind = "armor", value = 30, bonusMaxHp = 0,  slot = "amulet", dropWeight = 0.8 },
  { name = "Band of Might",   weight = 0.1, kind = "armor", value = 18, bonusMaxHp = 0,  slot = "ring",   dropWeight = 1.4 },
  { name = "Signet Ring",     weight = 0.1, kind = "armor", value = 28, bonusMaxHp = 0,  slot = "ring",   dropWeight = 0.9 },
}
