/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This is where scripts' loading functions should be declared:
void AddSC_kitt_go();
void AddSC_kitt_vendor_go();
void AddSC_kitt_p_start_guild();
void AddSC_kitt_first_lvl80();
void AddSC_kitt_quest_complet();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddkittScripts()
{
    AddSC_kitt_go();
    AddSC_kitt_vendor_go();
    AddSC_kitt_p_start_guild();
    AddSC_kitt_first_lvl80();
    AddSC_kitt_quest_complet();
}
