CREATE TABLE IF NOT EXISTS `character_kitt_invite_friend` (
  `inv_accID` INT(10) UNSIGNED NOT NULL COMMENT 'Contul nou (invitatul)',
  `inv_by_accID` INT(10) UNSIGNED NOT NULL COMMENT 'Contul care a recomandat (recrutatorul)',
  `inv_by_name` VARCHAR(12) NOT NULL COMMENT 'Numele caracterului indicat',
  `inv_rewarded` TINYINT(1) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Daca invitatul si-a luat premiul',
  `inv_by_rewarded` TINYINT(1) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Daca recrutatorul si-a luat premiul',
  `date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`inv_accID`),
  KEY `idx_inv_by_accID` (`inv_by_accID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


CREATE TABLE IF NOT EXISTS `zkitt_site_points_linked` (
  `id` INT(11) NOT NULL COMMENT 'ID-ul contului',
  `vp` INT(11) NOT NULL DEFAULT '0' COMMENT 'Vote Points',
  `dp` INT(11) NOT NULL DEFAULT '0' COMMENT 'Donation Points',
  PRIMARY KEY (`id`)
) ENGINE=FEDERATED
DEFAULT CHARSET=utf8mb4
CONNECTION='mysql://USER:PASS@HOST:3306/db_name/account_data';

