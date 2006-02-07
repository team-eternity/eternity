/*
    Eternity Engine Small Header
    Native functions for basic game stuff
*/

#if defined _game_included
  #endinput
#endif
#define _game_included

// call to exit the current level normally
// * must be called only while in a level
native _ExitLevel();

// call to use a secret exit for the current level
// * must be called only while in a level
native _ExitSecret();

enum _gameskills
{
   _SKILL_ITYTD = 1,
   _SKILL_HNTR,
   _SKILL_HMP,
   _SKILL_UV,
   _SKILL_NM
}

// call to start a new game on the given skill level and level name
// * skill should be a number between 1 and 5, 1 being ITYTD, 5 being 
//   Nightmare!
// * levelname should be a valid map name amongst loaded wad files
native _StartGame(_gameskills:skill, const mapname[]);

// returns the current game skill setting, between 1 and 5 as above
native _gameskills:_GameSkill();

enum _gametypes
{
   _GTYPE_SINGLE,
   _GTYPE_COOP,
   _GTYPE_DM
}

// returns the current game type using the definitions above
native _gametypes:_GameType();

// returns the version number of the Eternity Engine running this
// script multiplied by a factor of 100 -- ie, v3.31 == 331
native _EngineVersion();

// returns the name of the requested player (1-4), if the player is in the game
native _GetPlayerName(num, str[], bool:packed);

