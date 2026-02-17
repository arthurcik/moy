CREATE TABLE IF NOT EXISTS `character_kitt_custom_rate` (
  `player_guid` INT UNSIGNED NOT NULL DEFAULT '0',
  `player_name` VARCHAR(12) NOT NULL DEFAULT '',
  `xp_rate` INT UNSIGNED NOT NULL DEFAULT '0' COMMENT '0 = use config rate',
  PRIMARY KEY (`player_guid`),
  CONSTRAINT `FK_kitt_custom_rate_characters` 
    FOREIGN KEY (`player_guid`) 
    REFERENCES `characters` (`guid`) 
    ON DELETE CASCADE 
    ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci ROW_FORMAT=DYNAMIC COMMENT='kitt custom rates per character';


