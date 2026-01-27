CREATE TABLE `character_kitt_playtime_rewards` (
  `player_guid` INT UNSIGNED NOT NULL DEFAULT '0',
  `player_name` VARCHAR(12) NOT NULL DEFAULT '',
  `last_reward_level` INT UNSIGNED NOT NULL DEFAULT '0',
  `last_periodic_reward_time` INT UNSIGNED NOT NULL DEFAULT '0',
  PRIMARY KEY (`player_guid`),
  CONSTRAINT `FK_kitt_playtime_characters` 
    FOREIGN KEY (`player_guid`) 
    REFERENCES `characters` (`guid`) 
    ON DELETE CASCADE 
    ON UPDATE CASCADE
) ENGINE=InnoDB 
DEFAULT CHARSET=utf8mb4 
COLLATE=utf8mb4_unicode_ci 
ROW_FORMAT=DYNAMIC 
COMMENT='kitt play time Reward';
