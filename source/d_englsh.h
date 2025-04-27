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
// Purpose: Printed strings for translation.
//  English languages support (default).
//  See dstrings.h for suggestions about foreign language BEX support.
//
// Authors: James Haley
//

#ifndef D_ENGLSH_H__
#define D_ENGLSH_H__

//
//  Printed strings for translation
//

//
// D_Main.C
//
constexpr const char D_DEVSTR[] = "Development mode ON.\n";
constexpr const char D_CDROM[]  = "CD-ROM Version: default.cfg from c:\\doomdata\n";

//
//  M_Menu.C
//
#define PRESSKEY "press a key."
#define PRESSYN  "press y or n."
constexpr const char QUITMSG[]   = "are you sure you want to\nquit this great game?";
constexpr const char LOADNET[]   = "you can't do load while in a net game!\n\n" PRESSKEY;
constexpr const char QLOADNET[]  = "you can't quickload during a netgame!\n\n" PRESSKEY;
constexpr const char QSAVESPOT[] = "you haven't picked a quicksave slot yet!\n\n" PRESSKEY;
constexpr const char SAVEDEAD[]  = "you can't save if you aren't playing!\n\n" PRESSKEY;
constexpr const char QSPROMPT[]  = "quicksave over your game named\n\n'%s'?\n\n" PRESSYN;
constexpr const char QLPROMPT[]  = "do you want to quickload the game named\n\n'%s'?\n\n" PRESSYN;

constexpr const char NEWGAME[] = "you can't start a new game\n"
                                 "while in a network game.\n\n" PRESSKEY;

constexpr const char NIGHTMARE[] = "are you sure? this skill level\n"
                                   "isn't even remotely fair.\n\n" PRESSYN;

constexpr const char SWSTRING[] = "this is the shareware version of doom.\n\n"
                                  "you need to order the entire trilogy.\n\n" PRESSKEY;

constexpr const char MSGOFF[]  = "Messages OFF";
constexpr const char MSGON[]   = "Messages ON";
constexpr const char NETEND[]  = "you can't end a netgame!\n\n" PRESSKEY;
constexpr const char ENDGAME[] = "are you sure you want to end the game?\n\n" PRESSYN;

constexpr const char DOSY[] = "(press y to quit)";

constexpr const char DETAILHI[]    = "High detail";
constexpr const char DETAILLO[]    = "Low detail";
constexpr const char GAMMALVL0[]   = "Gamma correction OFF";
constexpr const char GAMMALVL1[]   = "Gamma correction level 1";
constexpr const char GAMMALVL2[]   = "Gamma correction level 2";
constexpr const char GAMMALVL3[]   = "Gamma correction level 3";
constexpr const char GAMMALVL4[]   = "Gamma correction level 4";
constexpr const char EMPTYSTRING[] = "empty slot";

//
//  P_inter.C
//
constexpr const char GOTARMOR[]    = "Picked up the armor.";
constexpr const char GOTMEGA[]     = "Picked up the MegaArmor!";
constexpr const char GOTHTHBONUS[] = "Picked up a health bonus.";
constexpr const char GOTARMBONUS[] = "Picked up an armor bonus.";
constexpr const char GOTSTIM[]     = "Picked up a stimpack.";
constexpr const char GOTMEDINEED[] = "Picked up a medikit that you REALLY need!";
constexpr const char GOTMEDIKIT[]  = "Picked up a medikit.";
constexpr const char GOTSUPER[]    = "Supercharge!";

constexpr const char GOTBLUECARD[] = "Picked up a blue keycard.";
constexpr const char GOTYELWCARD[] = "Picked up a yellow keycard.";
constexpr const char GOTREDCARD[]  = "Picked up a red keycard.";
constexpr const char GOTBLUESKUL[] = "Picked up a blue skull key.";
constexpr const char GOTYELWSKUL[] = "Picked up a yellow skull key.";
constexpr const char GOTREDSKULL[] = "Picked up a red skull key.";

constexpr const char GOTINVUL[]   = "Invulnerability!";
constexpr const char GOTBERSERK[] = "Berserk!";
constexpr const char GOTINVIS[]   = "Partial Invisibility";
constexpr const char GOTSUIT[]    = "Radiation Shielding Suit";
constexpr const char GOTMAP[]     = "Computer Area Map";

constexpr const char GOTVISOR[] = "Light Amplification Visor";

constexpr const char GOTMSPHERE[] = "MegaSphere!";

constexpr const char GOTCLIP[]     = "Picked up a clip.";
constexpr const char GOTCLIPBOX[]  = "Picked up a box of bullets.";
constexpr const char GOTROCKET[]   = "Picked up a rocket.";
constexpr const char GOTROCKBOX[]  = "Picked up a box of rockets.";
constexpr const char GOTCELL[]     = "Picked up an energy cell.";
constexpr const char GOTCELLBOX[]  = "Picked up an energy cell pack.";
constexpr const char GOTSHELLS[]   = "Picked up 4 shotgun shells.";
constexpr const char GOTSHELLBOX[] = "Picked up a box of shotgun shells.";

constexpr const char GOTBACKPACK[] = "Picked up a backpack full of ammo!";

constexpr const char GOTBFG9000[]  = "You got the BFG9000!  Oh, yes.";
constexpr const char GOTCHAINGUN[] = "You got the chaingun!";
constexpr const char GOTCHAINSAW[] = "A chainsaw!  Find some meat!";
constexpr const char GOTLAUNCHER[] = "You got the rocket launcher!";
constexpr const char GOTPLASMA[]   = "You got the plasma gun!";
constexpr const char GOTSHOTGUN[]  = "You got the shotgun!";
constexpr const char GOTSHOTGUN2[] = "You got the super shotgun!";

constexpr const char BETA_BONUS3[] = "Picked up an evil sceptre";
constexpr const char BETA_BONUS4[] = "Picked up an unholy bible";

//
// P_Doors.C
//
constexpr const char PD_BLUEO[]   = "You need a blue key to activate this object";
constexpr const char PD_REDO[]    = "You need a red key to activate this object";
constexpr const char PD_YELLOWO[] = "You need a yellow key to activate this object";
constexpr const char PD_BLUEK[]   = "You need a blue key to open this door";
constexpr const char PD_REDK[]    = "You need a red key to open this door";
constexpr const char PD_YELLOWK[] = "You need a yellow key to open this door";
// jff 02/05/98 Create messages specific to card and skull keys
#define BOOM_KEY_STRINGS
constexpr const char PD_BLUEC[]   = "You need a blue card to open this door";
constexpr const char PD_REDC[]    = "You need a red card to open this door";
constexpr const char PD_YELLOWC[] = "You need a yellow card to open this door";
constexpr const char PD_BLUES[]   = "You need a blue skull to open this door";
constexpr const char PD_REDS[]    = "You need a red skull to open this door";
constexpr const char PD_YELLOWS[] = "You need a yellow skull to open this door";
constexpr const char PD_ANY[]     = "Any key will open this door";
constexpr const char PD_ALL3[]    = "You need all three keys to open this door";
constexpr const char PD_ALL6[]    = "You need all six keys to open this door";

//
//  G_game.C
//
constexpr const char GGSAVED[] = "game saved.";

//
//  HU_stuff.C
//
constexpr const char HUSTR_MSGU[] = "[Message unsent]";

constexpr const char HUSTR_E1M1[] = "E1M1: Hangar";
constexpr const char HUSTR_E1M2[] = "E1M2: Nuclear Plant";
constexpr const char HUSTR_E1M3[] = "E1M3: Toxin Refinery";
constexpr const char HUSTR_E1M4[] = "E1M4: Command Control";
constexpr const char HUSTR_E1M5[] = "E1M5: Phobos Lab";
constexpr const char HUSTR_E1M6[] = "E1M6: Central Processing";
constexpr const char HUSTR_E1M7[] = "E1M7: Computer Station";
constexpr const char HUSTR_E1M8[] = "E1M8: Phobos Anomaly";
constexpr const char HUSTR_E1M9[] = "E1M9: Military Base";

constexpr const char HUSTR_E2M1[] = "E2M1: Deimos Anomaly";
constexpr const char HUSTR_E2M2[] = "E2M2: Containment Area";
constexpr const char HUSTR_E2M3[] = "E2M3: Refinery";
constexpr const char HUSTR_E2M4[] = "E2M4: Deimos Lab";
constexpr const char HUSTR_E2M5[] = "E2M5: Command Center";
constexpr const char HUSTR_E2M6[] = "E2M6: Halls of the Damned";
constexpr const char HUSTR_E2M7[] = "E2M7: Spawning Vats";
constexpr const char HUSTR_E2M8[] = "E2M8: Tower of Babel";
constexpr const char HUSTR_E2M9[] = "E2M9: Fortress of Mystery";

constexpr const char HUSTR_E3M1[] = "E3M1: Hell Keep";
constexpr const char HUSTR_E3M2[] = "E3M2: Slough of Despair";
constexpr const char HUSTR_E3M3[] = "E3M3: Pandemonium";
constexpr const char HUSTR_E3M4[] = "E3M4: House of Pain";
constexpr const char HUSTR_E3M5[] = "E3M5: Unholy Cathedral";
constexpr const char HUSTR_E3M6[] = "E3M6: Mt. Erebus";
constexpr const char HUSTR_E3M7[] = "E3M7: Limbo";
constexpr const char HUSTR_E3M8[] = "E3M8: Dis";
constexpr const char HUSTR_E3M9[] = "E3M9: Warrens";

constexpr const char HUSTR_E4M1[] = "E4M1: Hell Beneath";
constexpr const char HUSTR_E4M2[] = "E4M2: Perfect Hatred";
constexpr const char HUSTR_E4M3[] = "E4M3: Sever The Wicked";
constexpr const char HUSTR_E4M4[] = "E4M4: Unruly Evil";
constexpr const char HUSTR_E4M5[] = "E4M5: They Will Repent";
constexpr const char HUSTR_E4M6[] = "E4M6: Against Thee Wickedly";
constexpr const char HUSTR_E4M7[] = "E4M7: And Hell Followed";
constexpr const char HUSTR_E4M8[] = "E4M8: Unto The Cruel";
constexpr const char HUSTR_E4M9[] = "E4M9: Fear";

constexpr const char HUSTR_1[]  = "level 1: entryway";
constexpr const char HUSTR_2[]  = "level 2: underhalls";
constexpr const char HUSTR_3[]  = "level 3: the gantlet";
constexpr const char HUSTR_4[]  = "level 4: the focus";
constexpr const char HUSTR_5[]  = "level 5: the waste tunnels";
constexpr const char HUSTR_6[]  = "level 6: the crusher";
constexpr const char HUSTR_7[]  = "level 7: dead simple";
constexpr const char HUSTR_8[]  = "level 8: tricks and traps";
constexpr const char HUSTR_9[]  = "level 9: the pit";
constexpr const char HUSTR_10[] = "level 10: refueling base";
constexpr const char HUSTR_11[] = "level 11: 'o' of destruction!";

constexpr const char HUSTR_12[] = "level 12: the factory";
constexpr const char HUSTR_13[] = "level 13: downtown";
constexpr const char HUSTR_14[] = "level 14: the inmost dens";
constexpr const char HUSTR_15[] = "level 15: industrial zone";
constexpr const char HUSTR_16[] = "level 16: suburbs";
constexpr const char HUSTR_17[] = "level 17: tenements";
constexpr const char HUSTR_18[] = "level 18: the courtyard";
constexpr const char HUSTR_19[] = "level 19: the citadel";
constexpr const char HUSTR_20[] = "level 20: gotcha!";

constexpr const char HUSTR_21[] = "level 21: nirvana";
constexpr const char HUSTR_22[] = "level 22: the catacombs";
constexpr const char HUSTR_23[] = "level 23: barrels o' fun";
constexpr const char HUSTR_24[] = "level 24: the chasm";
constexpr const char HUSTR_25[] = "level 25: bloodfalls";
constexpr const char HUSTR_26[] = "level 26: the abandoned mines";
constexpr const char HUSTR_27[] = "level 27: monster condo";
constexpr const char HUSTR_28[] = "level 28: the spirit world";
constexpr const char HUSTR_29[] = "level 29: the living end";
constexpr const char HUSTR_30[] = "level 30: icon of sin";

constexpr const char HUSTR_31[] = "level 31: wolfenstein";
constexpr const char HUSTR_32[] = "level 32: grosse";

// haleyjd 10/03/12: only used for pack_disk (BFG Edition)
constexpr const char HUSTR_33[] = "level 33: betray";

// haleyjd 04/29/13: only used for pack_disk, and if the originals
// have not been modified.
constexpr const char BFGHUSTR_31[] = "level 31: idkfa";
constexpr const char BFGHUSTR_32[] = "level 32: keen";

constexpr const char PHUSTR_1[]  = "level 1: congo";
constexpr const char PHUSTR_2[]  = "level 2: well of souls";
constexpr const char PHUSTR_3[]  = "level 3: aztec";
constexpr const char PHUSTR_4[]  = "level 4: caged";
constexpr const char PHUSTR_5[]  = "level 5: ghost town";
constexpr const char PHUSTR_6[]  = "level 6: baron's lair";
constexpr const char PHUSTR_7[]  = "level 7: caughtyard";
constexpr const char PHUSTR_8[]  = "level 8: realm";
constexpr const char PHUSTR_9[]  = "level 9: abattoire";
constexpr const char PHUSTR_10[] = "level 10: onslaught";
constexpr const char PHUSTR_11[] = "level 11: hunted";

constexpr const char PHUSTR_12[] = "level 12: speed";
constexpr const char PHUSTR_13[] = "level 13: the crypt";
constexpr const char PHUSTR_14[] = "level 14: genesis";
constexpr const char PHUSTR_15[] = "level 15: the twilight";
constexpr const char PHUSTR_16[] = "level 16: the omen";
constexpr const char PHUSTR_17[] = "level 17: compound";
constexpr const char PHUSTR_18[] = "level 18: neurosphere";
constexpr const char PHUSTR_19[] = "level 19: nme";
constexpr const char PHUSTR_20[] = "level 20: the death domain";

constexpr const char PHUSTR_21[] = "level 21: slayer";
constexpr const char PHUSTR_22[] = "level 22: impossible mission";
constexpr const char PHUSTR_23[] = "level 23: tombstone";
constexpr const char PHUSTR_24[] = "level 24: the final frontier";
constexpr const char PHUSTR_25[] = "level 25: the temple of darkness";
constexpr const char PHUSTR_26[] = "level 26: bunker";
constexpr const char PHUSTR_27[] = "level 27: anti-christ";
constexpr const char PHUSTR_28[] = "level 28: the sewers";
constexpr const char PHUSTR_29[] = "level 29: odyssey of noises";
constexpr const char PHUSTR_30[] = "level 30: the gateway of hell";

constexpr const char PHUSTR_31[] = "level 31: cyberden";
constexpr const char PHUSTR_32[] = "level 32: go 2 it";

constexpr const char THUSTR_1[]  = "level 1: system control";
constexpr const char THUSTR_2[]  = "level 2: human bbq";
constexpr const char THUSTR_3[]  = "level 3: power control";
constexpr const char THUSTR_4[]  = "level 4: wormhole";
constexpr const char THUSTR_5[]  = "level 5: hanger";
constexpr const char THUSTR_6[]  = "level 6: open season";
constexpr const char THUSTR_7[]  = "level 7: prison";
constexpr const char THUSTR_8[]  = "level 8: metal";
constexpr const char THUSTR_9[]  = "level 9: stronghold";
constexpr const char THUSTR_10[] = "level 10: redemption";
constexpr const char THUSTR_11[] = "level 11: storage facility";

constexpr const char THUSTR_12[] = "level 12: crater";
constexpr const char THUSTR_13[] = "level 13: nukage processing";
constexpr const char THUSTR_14[] = "level 14: steel works";
constexpr const char THUSTR_15[] = "level 15: dead zone";
constexpr const char THUSTR_16[] = "level 16: deepest reaches";
constexpr const char THUSTR_17[] = "level 17: processing area";
constexpr const char THUSTR_18[] = "level 18: mill";
constexpr const char THUSTR_19[] = "level 19: shipping/respawning";
constexpr const char THUSTR_20[] = "level 20: central processing";

constexpr const char THUSTR_21[] = "level 21: administration center";
constexpr const char THUSTR_22[] = "level 22: habitat";
constexpr const char THUSTR_23[] = "level 23: lunar mining project";
constexpr const char THUSTR_24[] = "level 24: quarry";
constexpr const char THUSTR_25[] = "level 25: baron's den";
constexpr const char THUSTR_26[] = "level 26: ballistyx";
constexpr const char THUSTR_27[] = "level 27: mount pain";
constexpr const char THUSTR_28[] = "level 28: heck";
constexpr const char THUSTR_29[] = "level 29: river styx";
constexpr const char THUSTR_30[] = "level 30: last call";

constexpr const char THUSTR_31[] = "level 31: pharaoh";
constexpr const char THUSTR_32[] = "level 32: caribbean";

constexpr const char HUSTR_CHATMACRO1[] = "I'm ready to kick butt!";
constexpr const char HUSTR_CHATMACRO2[] = "I'm OK.";
constexpr const char HUSTR_CHATMACRO3[] = "I'm not looking too good!";
constexpr const char HUSTR_CHATMACRO4[] = "Help!";
constexpr const char HUSTR_CHATMACRO5[] = "You suck!";
constexpr const char HUSTR_CHATMACRO6[] = "Next time, scumbag...";
constexpr const char HUSTR_CHATMACRO7[] = "Come here!";
constexpr const char HUSTR_CHATMACRO8[] = "I'll take care of it.";
constexpr const char HUSTR_CHATMACRO9[] = "Yes";
constexpr const char HUSTR_CHATMACRO0[] = "No";

constexpr const char HUSTR_TALKTOSELF1[] = "You mumble to yourself";
constexpr const char HUSTR_TALKTOSELF2[] = "Who's there?";
constexpr const char HUSTR_TALKTOSELF3[] = "You scare yourself";
constexpr const char HUSTR_TALKTOSELF4[] = "You start to rave";
constexpr const char HUSTR_TALKTOSELF5[] = "You've lost it...";

constexpr const char HUSTR_MESSAGESENT[] = "[Message Sent]";

// The following should NOT be changed unless it seems
// just AWFULLY necessary

constexpr const char HUSTR_PLRGREEN[]  = "Green";
constexpr const char HUSTR_PLRINDIGO[] = "Indigo";
constexpr const char HUSTR_PLRBROWN[]  = "Brown";
constexpr const char HUSTR_PLRRED[]    = "Red";

constexpr const char HUSTR_KEYGREEN  = 'g';
constexpr const char HUSTR_KEYINDIGO = 'i';
constexpr const char HUSTR_KEYBROWN  = 'b';
constexpr const char HUSTR_KEYRED    = 'r';

//
//  AM_map.C
//

constexpr const char AMSTR_FOLLOWON[]  = "Follow Mode ON";
constexpr const char AMSTR_FOLLOWOFF[] = "Follow Mode OFF";

constexpr const char AMSTR_GRIDON[]  = "Grid ON";
constexpr const char AMSTR_GRIDOFF[] = "Grid OFF";

constexpr const char AMSTR_MARKEDSPOT[]   = "Marked Spot";
constexpr const char AMSTR_MARKSCLEARED[] = "All Marks Cleared";

//
//  ST_stuff.C
//

constexpr const char STSTR_MUS[]    = "Music Change";
constexpr const char STSTR_NOMUS[]  = "IMPOSSIBLE SELECTION";
constexpr const char STSTR_DQDON[]  = "Degreelessness Mode On";
constexpr const char STSTR_DQDOFF[] = "Degreelessness Mode Off";

constexpr const char STSTR_KFAADDED[] = "Very Happy Ammo Added";
constexpr const char STSTR_FAADDED[]  = "Ammo (no keys) Added";

constexpr const char STSTR_NCON[]  = "No Clipping Mode ON";
constexpr const char STSTR_NCOFF[] = "No Clipping Mode OFF";

constexpr const char STSTR_BEHOLD[]  = "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp";
constexpr const char STSTR_BEHOLDX[] = "Power-up Toggled";

constexpr const char STSTR_CHOPPERS[] = "... doesn't suck - GM";
constexpr const char STSTR_CLEV[]     = "Changing Level...";

constexpr const char STSTR_COMPON[]  = "Compatibility Mode On";  // phares
constexpr const char STSTR_COMPOFF[] = "Compatibility Mode Off"; // phares

//
//  F_Finale.C
//
constexpr const char E1TEXT[] = //
    "Once you beat the big badasses and\n"
    "clean out the moon base you're supposed\n"
    "to win, aren't you? Aren't you? Where's\n"
    "your fat reward and ticket home? What\n"
    "the hell is this? It's not supposed to\n"
    "end this way!\n"
    "\n"
    "It stinks like rotten meat, but looks\n"
    "like the lost Deimos base.  Looks like\n"
    "you're stuck on The Shores of Hell.\n"
    "The only way out is through.\n"
    "\n"
    "To continue the DOOM experience, play\n"
    "The Shores of Hell and its amazing\n"
    "sequel, Inferno!\n";

constexpr const char E2TEXT[] = //
    "You've done it! The hideous cyber-\n"
    "demon lord that ruled the lost Deimos\n"
    "moon base has been slain and you\n"
    "are triumphant! But ... where are\n"
    "you? You clamber to the edge of the\n"
    "moon and look down to see the awful\n"
    "truth.\n"
    "\n"
    "Deimos floats above Hell itself!\n"
    "You've never heard of anyone escaping\n"
    "from Hell, but you'll make the bastards\n"
    "sorry they ever heard of you! Quickly,\n"
    "you rappel down to  the surface of\n"
    "Hell.\n"
    "\n"
    "Now, it's on to the final chapter of\n"
    "DOOM! -- Inferno.";

constexpr const char E3TEXT[] = //
    "The loathsome spiderdemon that\n"
    "masterminded the invasion of the moon\n"
    "bases and caused so much death has had\n"
    "its ass kicked for all time.\n"
    "\n"
    "A hidden doorway opens and you enter.\n"
    "You've proven too tough for Hell to\n"
    "contain, and now Hell at last plays\n"
    "fair -- for you emerge from the door\n"
    "to see the green fields of Earth!\n"
    "Home at last.\n"
    "\n"
    "You wonder what's been happening on\n"
    "Earth while you were battling evil\n"
    "unleashed. It's good that no Hell-\n"
    "spawn could have come through that\n"
    "door with you ...";

constexpr const char E4TEXT[] = //
    "the spider mastermind must have sent forth\n"
    "its legions of hellspawn before your\n"
    "final confrontation with that terrible\n"
    "beast from hell.  but you stepped forward\n"
    "and brought forth eternal damnation and\n"
    "suffering upon the horde as a true hero\n"
    "would in the face of something so evil.\n"
    "\n"
    "besides, someone was gonna pay for what\n"
    "happened to daisy, your pet rabbit.\n"
    "\n"
    "but now, you see spread before you more\n"
    "potential pain and gibbitude as a nation\n"
    "of demons run amok among our cities.\n"
    "\n"
    "next stop, hell on earth!";

// after level 6, put this:

constexpr const char C1TEXT[] = //
    "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\n"
    "STARPORT. BUT SOMETHING IS WRONG. THE\n"
    "MONSTERS HAVE BROUGHT THEIR OWN REALITY\n"
    "WITH THEM, AND THE STARPORT'S TECHNOLOGY\n"
    "IS BEING SUBVERTED BY THEIR PRESENCE.\n"
    "\n"
    "AHEAD, YOU SEE AN OUTPOST OF HELL, A\n"
    "FORTIFIED ZONE. IF YOU CAN GET PAST IT,\n"
    "YOU CAN PENETRATE INTO THE HAUNTED HEART\n"
    "OF THE STARBASE AND FIND THE CONTROLLING\n"
    "SWITCH WHICH HOLDS EARTH'S POPULATION\n"
    "HOSTAGE.";

// After level 11, put this:

constexpr const char C2TEXT[] = //
    "YOU HAVE WON! YOUR VICTORY HAS ENABLED\n"
    "HUMANKIND TO EVACUATE EARTH AND ESCAPE\n"
    "THE NIGHTMARE.  NOW YOU ARE THE ONLY\n"
    "HUMAN LEFT ON THE FACE OF THE PLANET.\n"
    "CANNIBAL MUTATIONS, CARNIVOROUS ALIENS,\n"
    "AND EVIL SPIRITS ARE YOUR ONLY NEIGHBORS.\n"
    "YOU SIT BACK AND WAIT FOR DEATH, CONTENT\n"
    "THAT YOU HAVE SAVED YOUR SPECIES.\n"
    "\n"
    "BUT THEN, EARTH CONTROL BEAMS DOWN A\n"
    "MESSAGE FROM SPACE: \"SENSORS HAVE LOCATED\n"
    "THE SOURCE OF THE ALIEN INVASION. IF YOU\n"
    "GO THERE, YOU MAY BE ABLE TO BLOCK THEIR\n"
    "ENTRY.  THE ALIEN BASE IS IN THE HEART OF\n"
    "YOUR OWN HOME CITY, NOT FAR FROM THE\n"
    "STARPORT.\" SLOWLY AND PAINFULLY YOU GET\n"
    "UP AND RETURN TO THE FRAY.";

// After level 20, put this:

constexpr const char C3TEXT[] = //
    "YOU ARE AT THE CORRUPT HEART OF THE CITY,\n"
    "SURROUNDED BY THE CORPSES OF YOUR ENEMIES.\n"
    "YOU SEE NO WAY TO DESTROY THE CREATURES'\n"
    "ENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\n"
    "TEETH AND PLUNGE THROUGH IT.\n"
    "\n"
    "THERE MUST BE A WAY TO CLOSE IT ON THE\n"
    "OTHER SIDE. WHAT DO YOU CARE IF YOU'VE\n"
    "GOT TO GO THROUGH HELL TO GET TO IT?";

// After level 29, put this:

constexpr const char C4TEXT[] = //
    "THE HORRENDOUS VISAGE OF THE BIGGEST\n"
    "DEMON YOU'VE EVER SEEN CRUMBLES BEFORE\n"
    "YOU, AFTER YOU PUMP YOUR ROCKETS INTO\n"
    "HIS EXPOSED BRAIN. THE MONSTER SHRIVELS\n"
    "UP AND DIES, ITS THRASHING LIMBS\n"
    "DEVASTATING UNTOLD MILES OF HELL'S\n"
    "SURFACE.\n"
    "\n"
    "YOU'VE DONE IT. THE INVASION IS OVER.\n"
    "EARTH IS SAVED. HELL IS A WRECK. YOU\n"
    "WONDER WHERE BAD FOLKS WILL GO WHEN THEY\n"
    "DIE, NOW. WIPING THE SWEAT FROM YOUR\n"
    "FOREHEAD YOU BEGIN THE LONG TREK BACK\n"
    "HOME. REBUILDING EARTH OUGHT TO BE A\n"
    "LOT MORE FUN THAN RUINING IT WAS.\n";

// Before level 31, put this:

constexpr const char C5TEXT[] = //
    "CONGRATULATIONS, YOU'VE FOUND THE SECRET\n"
    "LEVEL! LOOKS LIKE IT'S BEEN BUILT BY\n"
    "HUMANS, RATHER THAN DEMONS. YOU WONDER\n"
    "WHO THE INMATES OF THIS CORNER OF HELL\n"
    "WILL BE.";

// Before level 32, put this:

constexpr const char C6TEXT[] = //
    "CONGRATULATIONS, YOU'VE FOUND THE\n"
    "SUPER SECRET LEVEL!  YOU'D BETTER\n"
    "BLAZE THROUGH THIS ONE!\n";

// after map 06

constexpr const char P1TEXT[] = //
    "You gloat over the steaming carcass of the\n"
    "Guardian.  With its death, you've wrested\n"
    "the Accelerator from the stinking claws\n"
    "of Hell.  You relax and glance around the\n"
    "room.  Damn!  There was supposed to be at\n"
    "least one working prototype, but you can't\n"
    "see it. The demons must have taken it.\n"
    "\n"
    "You must find the prototype, or all your\n"
    "struggles will have been wasted. Keep\n"
    "moving, keep fighting, keep killing.\n"
    "Oh yes, keep living, too.";

// after map 11

constexpr const char P2TEXT[] = //
    "Even the deadly Arch-Vile labyrinth could\n"
    "not stop you, and you've gotten to the\n"
    "prototype Accelerator which is soon\n"
    "efficiently and permanently deactivated.\n"
    "\n"
    "You're good at that kind of thing.";

// after map 20

constexpr const char P3TEXT[] = //
    "You've bashed and battered your way into\n"
    "the heart of the devil-hive.  Time for a\n"
    "Search-and-Destroy mission, aimed at the\n"
    "Gatekeeper, whose foul offspring is\n"
    "cascading to Earth.  Yeah, he's bad. But\n"
    "you know who's worse!\n"
    "\n"
    "Grinning evilly, you check your gear, and\n"
    "get ready to give the bastard a little Hell\n"
    "of your own making!";

// after map 30

constexpr const char P4TEXT[] = //
    "The Gatekeeper's evil face is splattered\n"
    "all over the place.  As its tattered corpse\n"
    "collapses, an inverted Gate forms and\n"
    "sucks down the shards of the last\n"
    "prototype Accelerator, not to mention the\n"
    "few remaining demons.  You're done. Hell\n"
    "has gone back to pounding bad dead folks \n"
    "instead of good live ones.  Remember to\n"
    "tell your grandkids to put a rocket\n"
    "launcher in your coffin. If you go to Hell\n"
    "when you die, you'll need it for some\n"
    "final cleaning-up ...";

// before map 31

constexpr const char P5TEXT[] = //
    "You've found the second-hardest level we\n"
    "got. Hope you have a saved game a level or\n"
    "two previous.  If not, be prepared to die\n"
    "aplenty. For master marines only.";

// before map 32

constexpr const char P6TEXT[] = //
    "Betcha wondered just what WAS the hardest\n"
    "level we had ready for ya?  Now you know.\n"
    "No one gets out alive.";

constexpr const char T1TEXT[] = //
    "You've fought your way out of the infested\n"
    "experimental labs.   It seems that UAC has\n"
    "once again gulped it down.  With their\n"
    "high turnover, it must be hard for poor\n"
    "old UAC to buy corporate health insurance\n"
    "nowadays..\n"
    "\n"
    "Ahead lies the military complex, now\n"
    "swarming with diseased horrors hot to get\n"
    "their teeth into you. With luck, the\n"
    "complex still has some warlike ordnance\n"
    "laying around.";

constexpr const char T2TEXT[] = //
    "You hear the grinding of heavy machinery\n"
    "ahead.  You sure hope they're not stamping\n"
    "out new hellspawn, but you're ready to\n"
    "ream out a whole herd if you have to.\n"
    "They might be planning a blood feast, but\n"
    "you feel about as mean as two thousand\n"
    "maniacs packed into one mad killer.\n"
    "\n"
    "You don't plan to go down easy.";

constexpr const char T3TEXT[] = //
    "The vista opening ahead looks real damn\n"
    "familiar. Smells familiar, too -- like\n"
    "fried excrement. You didn't like this\n"
    "place before, and you sure as hell ain't\n"
    "planning to like it now. The more you\n"
    "brood on it, the madder you get.\n"
    "Hefting your gun, an evil grin trickles\n"
    "onto your face. Time to take some names.";

constexpr const char T4TEXT[] = //
    "Suddenly, all is silent, from one horizon\n"
    "to the other. The agonizing echo of Hell\n"
    "fades away, the nightmare sky turns to\n"
    "blue, the heaps of monster corpses start \n"
    "to evaporate along with the evil stench \n"
    "that filled the air. Jeeze, maybe you've\n"
    "done it. Have you really won?\n"
    "\n"
    "Something rumbles in the distance.\n"
    "A blue light begins to glow inside the\n"
    "ruined skull of the demon-spitter.";

constexpr const char T5TEXT[] = //
    "What now? Looks totally different. Kind\n"
    "of like King Tut's condo. Well,\n"
    "whatever's here can't be any worse\n"
    "than usual. Can it?  Or maybe it's best\n"
    "to let sleeping gods lie..";

constexpr const char T6TEXT[] = //
    "Time for a vacation. You've burst the\n"
    "bowels of hell and by golly you're ready\n"
    "for a break. You mutter to yourself,\n"
    "Maybe someone else can kick Hell's ass\n"
    "next time around. Ahead lies a quiet town,\n"
    "with peaceful flowing water, quaint\n"
    "buildings, and presumably no Hellspawn.\n"
    "\n"
    "As you step off the transport, you hear\n"
    "the stomp of a cyberdemon's iron shoe.";

//
// Character cast strings F_FINALE.C
//
constexpr const char CC_ZOMBIE[]  = "ZOMBIEMAN";
constexpr const char CC_SHOTGUN[] = "SHOTGUN GUY";
constexpr const char CC_HEAVY[]   = "HEAVY WEAPON DUDE";
constexpr const char CC_IMP[]     = "IMP";
constexpr const char CC_DEMON[]   = "DEMON";
constexpr const char CC_LOST[]    = "LOST SOUL";
constexpr const char CC_CACO[]    = "CACODEMON";
constexpr const char CC_HELL[]    = "HELL KNIGHT";
constexpr const char CC_BARON[]   = "BARON OF HELL";
constexpr const char CC_ARACH[]   = "ARACHNOTRON";
constexpr const char CC_PAIN[]    = "PAIN ELEMENTAL";
constexpr const char CC_REVEN[]   = "REVENANT";
constexpr const char CC_MANCU[]   = "MANCUBUS";
constexpr const char CC_ARCH[]    = "ARCH-VILE";
constexpr const char CC_SPIDER[]  = "THE SPIDER MASTERMIND";
constexpr const char CC_CYBER[]   = "THE CYBERDEMON";
constexpr const char CC_HERO[]    = "OUR HERO";
constexpr const char CC_TITLE[]   = "";

// haleyjd 10/08/06 -- demo sequence strings
constexpr const char TITLEPIC[] = "TITLEPIC";
constexpr const char TITLE[]    = "TITLE";
constexpr const char DEMO1[]    = "DEMO1";
constexpr const char DEMO2[]    = "DEMO2";
constexpr const char DEMO3[]    = "DEMO3";
constexpr const char DEMO4[]    = "DEMO4";
constexpr const char CREDIT[]   = "CREDIT";
constexpr const char ORDER[]    = "ORDER";
constexpr const char HELP2[]    = "HELP2";

#endif

//----------------------------------------------------------------------------
//
// $Log: d_englsh.h,v $
// Revision 1.5  1998/05/04  21:33:57  thldrmn
// commenting and reformatting
//
// Revision 1.4  1998/02/08  15:26:40  jim
// New messages for keyed doors
//
// Revision 1.3  1998/01/28  12:23:02  phares
// TNTCOMP cheat code added
//
// Revision 1.2  1998/01/26  19:26:21  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
