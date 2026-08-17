-- Dungeon Crawl Stone Soup (CC0) PNG art → in-game sprite names.
--
-- This is the game's PRIMARY art now: every name below replaces the procedural
-- sprite of the same base name (see ScriptEngine::loadSprites). Sprites are the
-- real 32x32 CC0 tiles from OpenGameArt, loaded straight from PNG.
--
--   root  : asset folder, resolved relative to THIS file's folder (data/).
--   grade : when true, each PNG is pushed through the game's colour grade so the
--           external art shares the procedural art's mood. Off = raw DCSS look.
--   anim  : characters (heroes/monsters) — emitted as a single idle frame for
--           now; real animation is a later pass. Flat entries (tiles, loot) omit it.
--
-- Every 'file' below has been checked to exist in the pack. To add art, drop a
-- new { name, file } line — no C++ changes needed.

return {
  root = "../assets/dungeon_crawl/full/Dungeon Crawl Stone Soup Full",
  grade = false,

  sprites = {
    -- Ground tiles (flat, opaque, tiled across the world) --------------------
    { name = "grass", file = "dungeon/floor/grass/grass_0_new.png" },
    { name = "dirt",  file = "dungeon/floor/dirt_0_new.png" },
    { name = "rock",  file = "dungeon/floor/cobble_blood_1_new.png" },
    { name = "stone", file = "dungeon/floor/sandstone_floor_0.png" },

    -- Loot / weapons (flat) --------------------------------------------------
    { name = "loot_gold",   file = "item/gold/gold_pile.png" },
    { name = "loot_potion", file = "item/potion/brilliant_blue_new.png" },
    { name = "loot_armor",  file = "item/armor/back/cloak_1_leather.png" },
    { name = "loot_relic",  file = "item/misc/misc_orb.png" },
    { name = "wpn_sword",   file = "item/weapon/golden_sword.png" },
    { name = "wpn_bow",     file = "item/weapon/hand_crossbow.png" },

    -- Map dressing: scattered props that make the world feel alive -----------
    { name = "prop_bush",     file = "monster/fungi_plants/bush_2.png" },
    { name = "prop_flower",   file = "monster/fungi_plants/plant.png" },
    { name = "prop_mushroom", file = "monster/fungi_plants/deathcap.png" },
    { name = "prop_torch",    file = "dungeon/wall/torches/torch_1.png" },
    { name = "prop_rock",     file = "dungeon/floor/floor_sand_rock_0.png" },

    -- Hero (character; single static frame until we wire animation) ----------
    { name = "human", file = "player/base/human_male.png", anim = true },

    -- Enemies (characters; single static frame for now) ---------------------
    { name = "grunt",        file = "monster/orc_new.png",                anim = true },
    { name = "swarmer",      file = "monster/goblin_new.png",             anim = true },
    { name = "brute",        file = "monster/ogre_new.png",               anim = true },
    { name = "boss",         file = "monster/orc_warlord.png",            anim = true },
    { name = "slime",        file = "monster/jelly.png",                  anim = true },
    { name = "enemy_archer", file = "monster/deep_elf_master_archer.png", anim = true },
    { name = "spitter",      file = "monster/brown_ooze.png",             anim = true },
    { name = "sorcerer",     file = "monster/orc_sorcerer_new.png",       anim = true },
  },
}
