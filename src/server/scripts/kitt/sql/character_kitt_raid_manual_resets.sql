CREATE TABLE `character_kitt_raid_manual_resets` (
  `guid` int unsigned NOT NULL COMMENT 'GUID-ul jucatorului',
  `mode` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Dificultatea instantei',
  `resets` int unsigned NOT NULL DEFAULT '0' COMMENT 'Numarul de resetari',
  `reset_time` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Timestamp-ul ultimei resetari',
  PRIMARY KEY (`guid`,`mode`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
