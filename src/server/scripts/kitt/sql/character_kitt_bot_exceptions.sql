CREATE TABLE `character_kitt_bot_exceptions` (
  `account_id` int unsigned NOT NULL,
  `account_name` varchar(50) DEFAULT NULL COMMENT 'Optional',
  `date_added` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `duration_days` int unsigned NOT NULL DEFAULT '0' COMMENT '0 inseamna permanent',
  PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;