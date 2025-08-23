//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//------------------------------------------------------------------------------
//
// Purpose: Version ID constants.
// Authors: James Haley, Ioan Chera, Max Waine
//

#include "version.h"

// sf: made int from define
int version = 405;

// haleyjd: subversion -- range from 0 to 255
unsigned char subversion = 3;

const char version_date[] = __DATE__;
const char version_time[] = __TIME__; // haleyjd

// sf: version name -- at the suggestion of mystican
const char version_name[] = "Citadel";

// haleyjd: caption for SDL window
#ifdef _SDL_VER
const char ee_wmCaption[] = "Eternity Engine v4.05.03-pre \"Citadel\"";
#endif

// haleyjd: Eternity release history
// private alpha       -- 3.29 private       09/14/00
// public beta         -- 3.29 public beta 1 01/08/01
// public beta         -- 3.29 public beta 2 01/09/01
// public beta         -- 3.29 public beta 3 05/10/01
// public beta         -- 3.29 public beta 4 06/30/01
// development beta    -- 3.29 dev beta 5    10/02/01
// final v3.29 release -- 3.29 gamma         07/04/02
// public beta         -- 3.31 public beta 1 09/11/02
// public beta         -- 3.31 public beta 2 03/05/03
// public beta         -- 3.31 public beta 3 08/08/03
// public beta         -- 3.31 public beta 4 11/29/03
// public beta         -- 3.31 public beta 5 12/17/03
// public beta         -- 3.31 public beta 6 02/29/04
// public beta         -- 3.31 public beta 7 04/11/04

// 3.31.10 'Delta'            -- 01/19/05
// 3.33.00 'Genesis'          -- 05/26/05
// 3.33.01 'Outcast'          -- 06/24/05
// 3.33.02 'Warrior'          -- 10/01/05
// 3.33.33 'Paladin'          -- 05/17/06
// 3.33.50 'Phoenix'          -- 10/23/06
// 3.35.90 'Simorgh'          -- 01/11/09
// 3.35.92 'Nekhbet'          -- 03/22/09
// 3.37.00 'Sekhmet'          -- 01/01/10
// 3.39.20 'Resheph'          -- 10/10/10
// 3.40.00 'Rebirth'          -- 01/08/11
// 3.40.10 'Aasgard'          -- 05/01/11
// 3.40.11 'Aasgard'          -- 05/02/11
// 3.40.15 'Wodanaz'          -- 06/21/11
// 3.40.20 'Mjolnir'          -- 12/25/11
// 3.40.25 'Midgard'          -- 08/26/12
// 3.40.30 'Alfheim'          -- 11/04/12
// 3.40.37 'Gungnir'          -- 05/27/13
// 3.40.46 'Bifrost'          -- 01/19/14
// 3.42.02 'Heimdal'          -- 05/07/17
// 3.42.03 'Heimdal 2'        -- 08/02/17
// 4.00.00 'Völuspá'          -- 03/17/18
// 4.01.00 'Tyrfing'          -- 10/13/20
// 4.02.00 'Forseti'          -- 01/24/21
// 4.04.00 'Glitnir'          -- 10/13/24
// 4.04.01 'Glitnir Update 1' -- 10/13/24
// 4.04.02 'Glitnir Update 2' -- 11/19/25
// 4.05.03 'Citadel'          -- 08/23/25

// auxilliary releases
// Caverns of Darkness -- 3.29 dev beta 5 joel-2 04/24/02

//----------------------------------------------------------------------------
//
// $Log: version.c,v $
// Revision 1.2  1998/05/03  22:59:31  killough
// beautification
//
// Revision 1.1  1998/02/02  13:21:58  killough
// version information files
//
//----------------------------------------------------------------------------

