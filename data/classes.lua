-- Playable classes. Edit the numbers and relaunch — no recompile needed.
--
-- The game is moving classless (Mabinogi-style): everyone starts as **Human**
-- and specializes by the stats they pump (level-up) and the weapon they wield
-- (which now decides melee vs ranged). Tank/Archer/Healer remain here only as
-- starting stat *kits* until ClassId is retired entirely.
--
-- `id` selects the engine behaviour: "human" | "tank" | "archer" | "healer".
-- `attackStyle` is the UNARMED style; a held weapon overrides it.

return {
  { name = "Human",  id = "human",  maxHp = 130, speed = 240, attackStyle = "melee",
    attackDamage = 14, attackRange = 65,  attackCooldown = 0.40, abilityName = "Second Wind", abilityCooldown = 6 },

  { name = "Tank",   id = "tank",   maxHp = 220, speed = 200, attackStyle = "melee",
    attackDamage = 18, attackRange = 70,  attackCooldown = 0.45, abilityName = "Taunt",        abilityCooldown = 7 },

  { name = "Archer", id = "archer", maxHp = 90,  speed = 280, attackStyle = "ranged",
    attackDamage = 22, attackRange = 520, attackCooldown = 0.28, abilityName = "Power Shot",   abilityCooldown = 5 },

  { name = "Healer", id = "healer", maxHp = 120, speed = 250, attackStyle = "ranged",
    attackDamage = 12, attackRange = 380, attackCooldown = 0.50, abilityName = "Mend",         abilityCooldown = 6 },
}
