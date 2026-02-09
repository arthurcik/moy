CREATE TABLE IF NOT EXISTS `instance_veteran_status` (
  `id` int(10) unsigned NOT NULL COMMENT 'Instance ID',
  `isVeteran` tinyint(3) unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`),
  CONSTRAINT `FK_instance_veteran_status_instance` 
    FOREIGN KEY (`id`) 
    REFERENCES `instance` (`id`) 
    ON DELETE CASCADE 
    ON UPDATE CASCADE
) ENGINE=InnoDB 
DEFAULT CHARSET=utf8mb4 
COLLATE=utf8mb4_unicode_ci 
COMMENT='Status Veteran persistent pentru instante';