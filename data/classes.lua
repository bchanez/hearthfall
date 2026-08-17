-- Playable classes. Edit the numbers and relaunch — no recompile needed.
--
-- `id` selects the engine behaviour: "tank" | "archer" | "healer".
-- `attackStyle` is "melee" or "ranged".
-- Note: distinct *ability* behaviours (a real summoner, a real mage) arrive
-- when abilities themselves move to Lua; for now a class reuses the behaviour
-- of its `id` (e.g. Mage below is a glassier, harder-hitting archer).

return {
  { name = "Tank",   id = "tank",   maxHp = 220, speed = 200, attackStyle = "melee",
    attackDamage = 18, attackRange = 70,  attackCooldown = 0.45, abilityName = "Taunt",       abilityCooldown = 7 },

  { name = "Archer", id = "archer", maxHp = 90,  speed = 280, attackStyle = "ranged",
    attackDamage = 22, attackRange = 520, attackCooldown = 0.28, abilityName = "Power Shot",  abilityCooldown = 5 },

  { name = "Healer", id = "healer", maxHp = 120, speed = 250, attackStyle = "ranged",
    attackDamage = 12, attackRange = 380, attackCooldown = 0.50, abilityName = "Mend",        abilityCooldown = 6 },

  { name = "Mage",   id = "archer", maxHp = 80,  speed = 260, attackStyle = "ranged",
    attackDamage = 30, attackRange = 560, attackCooldown = 0.40, abilityName = "Arcane Bolt", abilityCooldown = 5 },
}
