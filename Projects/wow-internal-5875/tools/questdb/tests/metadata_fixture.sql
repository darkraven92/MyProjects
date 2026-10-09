CREATE TABLE `quest_template` (`entry` int, `patch` int, `Title` text, `Method` int, `QuestFlags` int, `SpecialFlags` int, `ExclusiveGroup` int, `RequiredSkill` int, `RequiredSkillValue` int, `RequiredClasses` int, `RequiredRaces` int, `RequiredCondition` int, `PrevQuestId` int, `NextQuestId` int, `StartScript` int, `CompleteScript` int, `ZoneOrSort` int, `MinLevel` int, `QuestLevel` int);
INSERT INTO `quest_template` VALUES
(10001,10,'Metadata fixture A',2,0,0,-7,0,0,1,2,0,0,10003,0,0,14,1,2),
(10002,10,'Metadata fixture B',2,0,0,-7,0,0,1,2,12,0,0,0,0,14,1,2),
(10003,10,'Metadata fixture C',2,0,2,0,0,0,1,2,0,10001,0,0,0,17,1,2),
(10004,10,'Metadata fixture D',2,0,0,8,0,0,1,2,0,-10001,0,0,0,17,1,2),
(10005,10,'Metadata fixture E',2,0,0,8,0,0,1,2,0,-99999,0,0,0,17,1,2);
CREATE TABLE `areatrigger_involvedrelation` (`id` int, `quest` int);
INSERT INTO `areatrigger_involvedrelation` VALUES (90001,10003);
CREATE TABLE `areatrigger_template` (`id` int, `build` int, `map_id` int, `x` float, `y` float, `z` float, `radius` float, `box_x` float, `box_y` float, `box_z` float, `box_orientation` float);
INSERT INTO `areatrigger_template` VALUES (90001,5875,1,1,2,3,4,0,0,0,0),(90001,8606,1,99,99,99,4,0,0,0,0);
CREATE TABLE `conditions` (`condition_entry` int, `type` int, `value1` int, `value2` int, `value3` int, `value4` int, `flags` int);
INSERT INTO `conditions` VALUES (12,8,10001,0,0,0,0);
