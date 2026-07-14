INSERT INTO `rbac_permissions`(`id`, `name`) VALUES (50001, 'Kitt Command for player');
INSERT INTO `rbac_permissions`(`id`, `name`) VALUES (50002, 'Kitt command for gm rank 5');
INSERT INTO `rbac_permissions`(`id`, `name`) VALUES (50003, 'Kitt command for gm rank 9');

INSERT INTO `rbac_linked_permissions`(`id`, `linkedId`) VALUES (195, 50001);
INSERT INTO `rbac_linked_permissions`(`id`, `linkedId`) VALUES (9900, 50002);
INSERT INTO `rbac_linked_permissions`(`id`, `linkedId`) VALUES (192, 50002);
INSERT INTO `rbac_linked_permissions`(`id`, `linkedId`) VALUES (192, 50003);

