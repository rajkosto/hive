SET FOREIGN_KEY_CHECKS=0;

-- ----------------------------
-- Table structure for `Player_DATA`
-- ----------------------------
CREATE TABLE `Player_DATA` (
  `PlayerUID` varchar(32) NOT NULL,
  `PlayerName` varchar(128) CHARACTER SET utf8 NOT NULL,
  `PlayerMorality` int(11) NOT NULL DEFAULT '0',
  `PlayerSex` tinyint(3) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`PlayerUID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- ----------------------------
-- Table structure for `Character_DATA`
-- ----------------------------
CREATE TABLE `Character_DATA` (
  `CharacterID` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `PlayerUID` varchar(32) NOT NULL,
  `InstanceID` int(11) NOT NULL,
  `MapID` int(11) unsigned NOT NULL DEFAULT '0',
  `Datestamp` datetime DEFAULT NULL,
  `LastLogin` datetime NOT NULL,
  `Inventory` text,
  `Backpack` text,
  `Worldspace` varchar(128) NOT NULL DEFAULT '[]',
  `Medical` varchar(256) NOT NULL DEFAULT '[]',
  `Alive` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `Generation` int(11) unsigned NOT NULL DEFAULT '1',
  `LastAte` datetime NOT NULL,
  `LastDrank` datetime NOT NULL,
  `KillsZ` int(11) unsigned NOT NULL DEFAULT '0',
  `HeadshotsZ` int(11) unsigned NOT NULL DEFAULT '0',
  `DistanceFoot` int(11) NOT NULL DEFAULT '0',
  `Duration` int(11) unsigned NOT NULL DEFAULT '0',
  `CurrentState` varchar(128) NOT NULL DEFAULT '[]',
  `KillsH` int(11) unsigned NOT NULL DEFAULT '0',
  `Model` varchar(64) NOT NULL DEFAULT '',
  `KillsB` int(11) unsigned NOT NULL DEFAULT '0',
  `Humanity` int(11) NOT NULL DEFAULT '2500',
  PRIMARY KEY (`CharacterID`),
  KEY `CharFetch` (`PlayerUID`,`Alive`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;

-- ----------------------------
-- Table structure for `Character_DEAD`
-- ----------------------------
CREATE TABLE `Character_DEAD` (
  `CharacterID` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `PlayerUID` varchar(32) NOT NULL,
  `InstanceID` int(11) NOT NULL,
  `MapID` int(11) unsigned NOT NULL DEFAULT '0',
  `Datestamp` datetime DEFAULT NULL,
  `LastLogin` datetime NOT NULL,
  `Inventory` text,
  `Backpack` text,
  `Worldspace` varchar(128) NOT NULL DEFAULT '[]',
  `Medical` varchar(256) NOT NULL DEFAULT '[]',
  `Alive` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `Generation` int(11) unsigned NOT NULL DEFAULT '1',
  `LastAte` datetime NOT NULL,
  `LastDrank` datetime NOT NULL,
  `KillsZ` int(11) unsigned NOT NULL DEFAULT '0',
  `HeadshotsZ` int(11) unsigned NOT NULL DEFAULT '0',
  `DistanceFoot` int(11) NOT NULL DEFAULT '0',
  `Duration` int(11) unsigned NOT NULL DEFAULT '0',
  `CurrentState` varchar(128) NOT NULL DEFAULT '[]',
  `KillsH` int(11) unsigned NOT NULL DEFAULT '0',
  `Model` varchar(64) NOT NULL DEFAULT '',
  `KillsB` int(11) unsigned NOT NULL DEFAULT '0',
  `Humanity` int(11) NOT NULL DEFAULT '2500',
  PRIMARY KEY (`CharacterID`),
  KEY `CharFetch` (`PlayerUID`,`Alive`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;

-- ----------------------------
-- Table structure for `Player_LOGIN`
-- ----------------------------
CREATE TABLE `Player_LOGIN` (
  `LoginID` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `PlayerUID` varchar(32) NOT NULL,
  `CharacterID` int(11) unsigned NOT NULL,
  `Datestamp` datetime NOT NULL,
  `Action` tinyint(3) NOT NULL,
  PRIMARY KEY (`LoginID`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;

-- ----------------------------
-- Procedure structure for `LoadCharacter`
-- ----------------------------
DROP PROCEDURE IF EXISTS `LoadCharacter`;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `LoadCharacter`(IN `instId_` int,IN `mapId_` int(11) UNSIGNED,IN `playerUid_` varchar(32),IN `playerName_` varchar(128))
BEGIN
	DECLARE _oldName VARCHAR(128) DEFAULT NULL;
	DECLARE _playerStatus VARCHAR(8) DEFAULT 'ERROR';
	DECLARE _charStatus VARCHAR(8) DEFAULT 'ERROR';

	-- actual values from the db
	DECLARE _charId INT(11) UNSIGNED DEFAULT NULL;
	DECLARE _ws VARCHAR(128);
	DECLARE _inv TEXT;
	DECLARE _backpack TEXT;
	DECLARE _survTime INT(11) DEFAULT 0;
	DECLARE _minsLastAte INT(11) DEFAULT 0;
	DECLARE _minsLastDrank INT(11) DEFAULT 0;
	DECLARE _duration INT(11) UNSIGNED DEFAULT 0;
	DECLARE _distance INT(11) DEFAULT 0;
	DECLARE _killsZ INT(11) UNSIGNED DEFAULT 0;
	DECLARE _killsH INT(11) UNSIGNED DEFAULT 0;
	DECLARE _killsB INT(11) UNSIGNED DEFAULT 0;
	DECLARE _headsZ INT(11) UNSIGNED DEFAULT 0;
	DECLARE _model VARCHAR(64) DEFAULT '';
	DECLARE _medical VARCHAR(256) DEFAULT '';
	DECLARE _currState VARCHAR(128) DEFAULT '';
	DECLARE _gen INT(11) UNSIGNED DEFAULT NULL;
	DECLARE _hum INT(11) DEFAULT NULL;
	

	-- This also checks if the player already exists
	SELECT `PlayerName` INTO `_oldName` FROM `Player_DATA` WHERE `PlayerUID`=`playerUid_` LIMIT 1;

	IF `_oldName` IS NULL THEN -- player didn't exist before
		SET `_playerStatus` := 'NEW';
		INSERT INTO `Player_DATA`(`PlayerUID`,`PlayerName`) VALUES(`playerUid_`,`playerName_`);
	ELSEIF `playerName_` <> `_oldName` THEN -- player existed but with a different name
		SET `_playerStatus` := 'CHNAME';
		UPDATE `Player_DATA` SET `PlayerName`=`playerName_` WHERE `PlayerUID`=`playerUid_`;
	ELSE
		SET `_oldName` := NULL;
		SET `_playerStatus` := 'EXIST';
	END IF;
	
	SELECT `CharacterID`,`Worldspace`,`Inventory`,`Backpack`,
				TIMESTAMPDIFF(MINUTE,`Datestamp`,`LastLogin`) as `SurvivalTime`,
				TIMESTAMPDIFF(MINUTE,`LastAte`,NOW()) as `MinsLastAte`,
				TIMESTAMPDIFF(MINUTE,`LastDrank`,NOW()) as `MinsLastDrank`,
				`Duration`,`DistanceFoot`,`KillsZ`,`KillsH`,`KillsB`,`HeadshotsZ`,
				`Model`,`Medical`,`CurrentState`,`Generation`,`Humanity`
				FROM `Character_DATA` WHERE `PlayerUID`=`playerUid_`
				AND `MapID`=`mapId_` AND `Alive`=1 ORDER BY `CharacterID` DESC LIMIT 1 
				INTO `_charId`,`_ws`,`_inv`,`_backpack`,
				`_survTime`,`_minsLastAte`,`_minsLastDrank`,
				`_duration`,`_distance`,`_killsZ`,`_killsH`,`_killsB`,`_headsZ`,
				`_model`,`_medical`,`_currState`,`_gen`,`_hum`;

	IF `_charId` IS NULL THEN -- make a new character (nobody is alive)
		-- not specifying MapID or Alive as a condition here because humanity is shared between characters
		-- and maybe we are dead on one map but alive on another, just use the most recent char for humanity
		SELECT `Generation`,`Humanity`,`Model` FROM `Character_DATA` WHERE `PlayerUID`=`playerUid_` 
		ORDER BY `CharacterID` DESC LIMIT 1 INTO `_gen`,`_hum`,`_model`;

		IF `_gen` IS NULL THEN -- nobody existed before us
			SET `_charStatus` := 'NEW1ST';
			SET `_model` := '';
			SET `_gen` := NULL;
			SET `_hum` := NULL;
		ELSE -- we aren't the first generation
			SET `_charStatus` := 'NEWFROM';
			SET `_gen` := `_gen`+1;

			-- since we got all the info we need, move dead characters to their archive table
			INSERT INTO `Character_DEAD` SELECT * FROM `Character_DATA` WHERE `PlayerUID`=`playerUid_` AND `Alive`=0;
			DELETE FROM `Character_DATA` WHERE `PlayerUID`=`playerUid_` AND `Alive`=0;
		END IF;

		-- insert the new character
		IF `_charStatus`='NEW1ST' THEN -- don't supply generation and humanity
			INSERT INTO `Character_DATA`(`PlayerUID`,`InstanceID`,`MapID`,
																		`Datestamp`,`LastLogin`,`LastAte`,`LastDrank`) 
			VALUES(`playerUid_`,`instId_`,`mapId_`,
							CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP);			
		ELSEIF `_charStatus`='NEWFROM' THEN -- supply generation and humanity
			INSERT INTO `Character_DATA`(`PlayerUID`,`InstanceID`,`MapID`,`Generation`,`Humanity`,
																		`Datestamp`,`LastLogin`,`LastAte`,`LastDrank`) 
			VALUES(`playerUid_`,`instId_`,`mapId_`,`_gen`,`_hum`,
							CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP);
		END IF;

		-- get the characterID and other defaults
		SELECT `CharacterID`,`Worldspace`,`Model`,`Medical`,`CurrentState`,`Generation`,`Humanity`
			FROM `Character_DATA` WHERE `PlayerUID`=`playerUid_` AND `MapID`=`mapId_` 
			AND `Alive`=1 ORDER BY `CharacterID` DESC LIMIT 1 
			INTO `_charId`,`_ws`,`_model`,`_medical`,`_currState`,`_gen`,`_hum`;

		IF `_charId` IS NULL THEN
			SET `_charStatus` := 'ERROR';
		END IF;
	ELSE -- got a character from the DB
		UPDATE `Character_DATA` SET `LastLogin` = CURRENT_TIMESTAMP WHERE `CharacterID`=`_charId`;
		SET `_charStatus` := 'EXIST';
	END IF;

	-- output status to client
	SELECT `_playerStatus` AS `PlayerStatus`,`_oldName` AS `OldName`,`_charStatus` AS `CharacterStatus`,`_charId` AS `CharacterID`;
	IF (`_playerStatus` <> 'ERROR') AND (`_charStatus` <> 'ERROR') THEN
	-- output character row to client
		SELECT `_ws` AS `Worldspace`, `_inv` AS `Inventory`, `_backpack` AS `Backpack`,
			`_survTime` AS `SurvivalTime`, `_minsLastAte` AS `MinsLastAte`, `_minsLastDrank` AS `MinsLastDrank`,
			`_duration` AS `Duration`, `_distance` AS `DistanceFoot`, 
			`_killsZ` AS `KillsZ`, `_killsH` AS `KillsH`, `_killsB` AS `KillsB`, `_headsZ` AS `HeadshotsZ`,
			`_model` AS `Model`, `_medical` AS `Medical`, `_currState` AS `CurrentState`,
			`_gen` AS `Generation`, `_hum` AS `Humanity`;
	END IF;
END
;;
DELIMITER ;
