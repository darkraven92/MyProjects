CREATE TABLE creature_template (`entry` int, `patch` int, `name` text, `npc_flags` int, `trainer_type` int, `trainer_class` int, `trainer_id` int, `faction` int);
INSERT INTO creature_template VALUES (60001,0,'Test Trainer',16,0,8,20,35),(60002,0,'Profession',16,2,0,20,35);
CREATE TABLE npc_trainer (`entry` int, `spell` int, `spellcost` int, `reqskill` int, `reqskillvalue` int, `reqlevel` int, `build_min` int, `build_max` int);
CREATE TABLE npc_trainer_template (`entry` int, `spell` int, `spellcost` int, `reqskill` int, `reqskillvalue` int, `reqlevel` int, `build_min` int, `build_max` int);
INSERT INTO npc_trainer_template VALUES (20,70001,10,0,0,1,0,5875),(20,70002,20,0,0,2,0,5875);
CREATE TABLE spell_template (`entry` int, `build` int, `name` text, `nameSubtext` text, `effect1` int, `effect2` int, `effect3` int, `effectTriggerSpell1` int, `spellLevel` int);
INSERT INTO spell_template VALUES
 (70001,1234,'Test Ability','Rank 1',36,0,0,50001,1),
 (70002,1234,'Test Ability','Rank 2',36,0,0,50002,2),
 (50001,1234,'Old Name','Rank 1',2,0,0,0,1),
 (50001,5875,'Test Ability','Rank 1',2,0,0,0,1),
 (50002,1234,'Test Ability','Rank 2',2,0,0,0,2),
 (50002,9999,'Future Name','Rank 2',2,0,0,0,2);
CREATE TABLE spell_chain (`spell_id` int, `prev_spell` int, `first_spell` int, `rank` int, `req_spell` int, `build_min` int, `build_max` int);
INSERT INTO spell_chain VALUES (50001,0,50001,1,0,0,5875),(50002,50001,50001,2,0,0,5875);
CREATE TABLE faction_template (`id` int, `build` int, `faction_id` int, `our_mask` int, `friendly_mask` int, `hostile_mask` int, `enemy_faction1` int, `enemy_faction2` int, `enemy_faction3` int, `enemy_faction4` int);
INSERT INTO faction_template VALUES (35,1234,35,1,1,0,0,0,0,0);
