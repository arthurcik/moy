-- kitt npc menu
INSERT INTO `npc_text` (`id`, `text0_0`) VALUES 
(90014, 'Salut, calator! Ce doresti sa faci astazi?'),
(90015, 'Salut, aici poti sa iti resetezi instantele.');

UPDATE `npc_text` SET `text0_0` = 'Greetings, traveler! What would you like to do today?' WHERE `id` = 90014;
UPDATE `npc_text` SET `text0_0` = 'Greetings. Here you can reset your active instance cooldowns.' WHERE `id` = 90015;

-- ro
INSERT INTO `npc_text` (`id`, `text0_0`) VALUES 
(90016, 'Salut, |cff00ff00$n|r ! Eu sunt expertul în obiecte rare al regatului.
|nPrin mine poti afla exact cine detine un anumit obiect si ce sansa ai sa il obtii.
|n|cff66ccffFunctii disponibile:|r
|n- Verificare rata de drop (Grup/Referinta).
|n- Teleportare directa la locatie (doar în lume).
|n|cffff0000Nota:|r Daca un monstru nu poate fi gasit, probabil este invocat prin quest-uri sau evenimente.
|n|cffff0000Nota:|r Daca rata de drop este 0 inseamna ca este GrupLoot');

-- en
UPDATE `npc_text` SET `text0_0` = 
'Greetings, |cff00ff00$n|r! I am the realm''s expert on rare artifacts.
|nI can show you exactly who holds a specific item and what your chances are of obtaining it.
|n|cff66ccffAvailable Features:|r
|n- Check drop rates (Group/Reference).
|n- Direct teleport to location (World maps only).
|n|cffff0000Note:|r If a creature cannot be found, it is likely summoned via quests or events.
|n|cffff0000Note:|r A drop rate of 0% usually indicates Group Loot or special loot tables.' 
WHERE `id` = 90016;
