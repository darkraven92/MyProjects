CREATE TABLE `quest_template` (
 `entry` int, `patch` int, `ZoneOrSort` int, `MinLevel` int, `MaxLevel` int, `QuestLevel` int,
 `RequiredClasses` int, `RequiredRaces` int, `RequiredCondition` int, `PrevQuestId` int, `NextQuestId` int,
 `BreadcrumbForQuestId` int, `NextQuestInChain` int, `SrcItemId` int, `SrcItemCount` int, `SrcSpell` int,
 `Title` text, `ObjectiveText1` text, `ObjectiveText2` text, `ObjectiveText3` text, `ObjectiveText4` text,
 `ReqItemId1` int, `ReqItemId2` int, `ReqItemId3` int, `ReqItemId4` int,
 `ReqItemCount1` int, `ReqItemCount2` int, `ReqItemCount3` int, `ReqItemCount4` int,
 `ReqCreatureOrGOId1` int, `ReqCreatureOrGOId2` int, `ReqCreatureOrGOId3` int, `ReqCreatureOrGOId4` int,
 `ReqCreatureOrGOCount1` int, `ReqCreatureOrGOCount2` int, `ReqCreatureOrGOCount3` int, `ReqCreatureOrGOCount4` int,
 `ReqSpellCast1` int, `ReqSpellCast2` int, `ReqSpellCast3` int, `ReqSpellCast4` int
);
INSERT INTO `quest_template` VALUES
(4402,10,14,1,0,1,0,2,0,0,0,0,0,0,0,0,'Galgar\'s Cactus Apple Surprise','Collect apples','','','',11583,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(790,10,14,1,0,1,0,2,0,0,804,0,804,0,0,0,'Sarkoth','Kill Sarkoth','','','',4905,0,0,0,1,0,0,0,3281,0,0,0,1,0,0,0,0,0,0,0),
(804,10,14,1,0,1,0,2,0,790,0,0,0,0,0,0,'Sarkoth','','','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
CREATE TABLE `creature_questrelation` (`id` int,`quest` int,`patch_min` int,`patch_max` int);
INSERT INTO `creature_questrelation` VALUES (9796,4402,0,10),(3287,790,0,10),(3287,804,0,10);
CREATE TABLE `creature_involvedrelation` (`id` int,`quest` int,`patch_min` int,`patch_max` int);
INSERT INTO `creature_involvedrelation` VALUES (9796,4402,0,10),(3287,790,0,10),(3143,804,0,10);
CREATE TABLE `creature_template` (`entry` int,`name` text,`loot_id` int);
INSERT INTO `creature_template` VALUES (9796,'Galgar',0),(3287,'Hana\'zua',0),(3143,'Gornek',0),(3281,'Sarkoth',3281);
CREATE TABLE `creature` (`guid` int,`id` int,`map` int,`position_x` float,`position_y` float,`position_z` float,`orientation` float,`patch_min` int,`patch_max` int);
INSERT INTO `creature` VALUES (1,9796,1,-561.63,-4221.80,41.67,0,0,10),(2,3287,1,-397.76,-4108.99,50.29,0,0,10),(3,3143,1,-600,-4200,42,0,0,10),(4,3281,1,-547.34,-4103.85,70.10,0,0,10);
CREATE TABLE `gameobject_template` (`entry` int,`type` int,`name` text,`data0` int,`data1` int);
INSERT INTO `gameobject_template` VALUES (171938,3,'Cactus Apple',0,171938);
CREATE TABLE `gameobject` (`guid` int,`id` int,`map` int,`position_x` float,`position_y` float,`position_z` float,`orientation` float,`patch_min` int,`patch_max` int);
INSERT INTO `gameobject` VALUES (10,171938,1,-489.09,-4301.17,42.87,0,0,10);
CREATE TABLE `gameobject_loot_template` (`entry` int,`item` int,`ChanceOrQuestChance` float,`mincountOrRef` int,`maxcount` int);
INSERT INTO `gameobject_loot_template` VALUES (171938,11583,-100,1,1);
CREATE TABLE `creature_loot_template` (`entry` int,`item` int,`ChanceOrQuestChance` float,`mincountOrRef` int,`maxcount` int);
INSERT INTO `creature_loot_template` VALUES (3281,4905,-100,1,1);
