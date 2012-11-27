SET FOREIGN_KEY_CHECKS=0;

-- ----------------------------
-- Table structure for `Player_DATA`
-- ----------------------------
CREATE TABLE `Player_DATA` (
  `PlayerUID` varchar(32) NOT NULL,
  `PlayerName` varchar(128) CHARACTER SET utf8 NOT NULL,
  `PlayerMorality` int(11) NOT NULL DEFAULT '0',
  `PlayerSex` tinyint(3) UNSIGNED NOT NULL DEFAULT '0',
  PRIMARY KEY (`PlayerUID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- ----------------------------
-- Table structure for `Character_DATA`
-- ----------------------------
CREATE TABLE `Character_DATA` (
  `CharacterID` int(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `PlayerUID` varchar(32) NOT NULL,
  `InstanceID` int(11) NOT NULL,
  `Datestamp` datetime DEFAULT NULL,
  `LastLogin` datetime NOT NULL,
  `Inventory` text,
  `Backpack` text,
  `Worldspace` varchar(128) NOT NULL DEFAULT '[]',
  `Medical` varchar(256) NOT NULL DEFAULT '[]',
  `Alive` tinyint(3) UNSIGNED NOT NULL DEFAULT '1',
  `Generation` int(11) UNSIGNED NOT NULL DEFAULT '1',
  `LastAte` datetime NOT NULL,
  `LastDrank` datetime NOT NULL,
  `KillsZ` int(11) UNSIGNED NOT NULL DEFAULT '0',
  `HeadshotsZ` int(11) UNSIGNED NOT NULL DEFAULT '0',
  `DistanceFoot` int(11) NOT NULL DEFAULT '0',
  `Duration` int(11) UNSIGNED NOT NULL DEFAULT '0',
  `CurrentState` varchar(128) NOT NULL DEFAULT '[]',
  `KillsH` int(11) UNSIGNED NOT NULL DEFAULT '0',
  `Model` varchar(64) NOT NULL DEFAULT '"Survivor2_DZ"',
  `KillsB` int(11) UNSIGNED NOT NULL DEFAULT '0',
  `Humanity` int(11) NOT NULL DEFAULT '2500',
  PRIMARY KEY (`CharacterID`),
  KEY `CharFetch` (`PlayerUID`,`Alive`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;

-- ----------------------------
-- Table structure for `Player_LOGIN`
-- ----------------------------
CREATE TABLE `Player_LOGIN` (
  `LoginID` int(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `PlayerUID` varchar(32) NOT NULL,
  `CharacterID` int(11) UNSIGNED NOT NULL,
  `Datestamp` datetime NOT NULL,
  `Action` tinyint(3) NOT NULL,
  PRIMARY KEY (`LoginID`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;
