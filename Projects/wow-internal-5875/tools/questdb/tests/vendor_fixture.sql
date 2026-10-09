CREATE TABLE creature_template (`entry` int, `patch` int, `name` text, `npc_flags` int, `faction` int);
INSERT INTO creature_template VALUES (60001,0,'Merchant',4,35),(60002,0,'Repair Merchant',16388,35),(60003,0,'Not Merchant',16,35),(60004,0,'Removed Merchant',4,35),(60004,10,'Removed Merchant',0,35),(60001,11,'Future',0,35);
CREATE TABLE faction_template (`id` int, `build` int, `faction_id` int, `our_mask` int, `friendly_mask` int, `hostile_mask` int, `enemy_faction1` int, `enemy_faction2` int, `enemy_faction3` int, `enemy_faction4` int);
INSERT INTO faction_template VALUES (35,1234,35,1,1,0,0,0,0,0),(35,9999,35,1,0,1,0,0,0,0);
