/*
    Eternity Engine Small Header
    Native functions for invocation model interaction / callbacks
*/

#if defined _invoke_included
  #endinput
#endif
#define _invoke_included

// invocation model types
enum _invoketypes
{
   _INVOKE_NONE,      // no invocation data
   _INVOKE_CCMD,      // started by console command
   _INVOKE_THING,     // started from thing frame
   _INVOKE_PLAYER,    // started from player gun frame
   _INVOKE_LINE,      // started by linedef
   _INVOKE_TERRAIN,   // started by scripted TerrainType
   _INVOKE_CALLBACK,  // scheduled callback
   _INVOKE_SPECIAL,   // ExtraData thing special
   _INVOKE_DIALOGUE,  // started by dialogue
}

// returns the current invocation model type, indicating how the current
// script was started
native _invoketypes:_GetInvokeType();

// If invocation model is INVOKE_CCMD, returns player number from 1 to 4 
// indicating what player ran the command. Otherwise an error will occur.
native _GetCcmdSrc();

// If invocation model is INVOKE_CCMD, INVOKE_PLAYER, or INVOKE_DIALOGUE, the
// player number of the player that started the script is returned. If invocation
// model is INVOKE_THING, INVOKE_TERRAIN, or INVOKE_LINE, the player number will
// be returned if a player started the script, otherwise -1 will be returned. If
// the invocation model is not one of the above, an error will occur.
native _GetPlayerSrc();

// If a script trigger object exists:
// 1) If the object is a player, a TID from -1 to -4 will be returned.
// 2) If the object has its own unique TID, that TID will be returned.
// 3) If the object doesn't have a unique TID, TID_TRIGGER will be returned.
//
// If no script trigger object exists, zero is returned. This can be used to test
// if a script trigger object exists.
native _GetThingSrc();

// script callback wait types
enum _waittypes
{
   _SWAIT_NONE,
   _SWAIT_DELAY,
   _SWAIT_TAG,
}

// callback flags (optional)
enum
{
   _CB_PAUSABLE = 0x00000001, // callback will wait if game is paused
   _CB_MPAUSE   = 0x00000002  // callback will wait when menus are up in non-netgames
}

// Schedules a callback for the named script. The waittype should be one of the
// above enum values. waitdata's meaning varies for each wait type. For WAIT_NONE,
// it has no meaning. For SWAIT_DELAY, it is the number of gametics to wait before
// executing the callback. For SWAIT_TAG, it is a sector tag on which to wait for
// all thinker actions to finish.
native _SetCallback(const name[], _waittypes:waittype = _SWAIT_NONE, waitdata = 0, flags = _CB_PAUSABLE | _CB_MPAUSE);

// Allows game scripts to be started from within level scripts.
native _ExecGS(const name[], ...);

// Allows level scripts to be started from within game scripts.
native _ExecLS(const name[], ...);

// Functions to read/write Levelscript public variables from anywhere
native _GetLevelVar(const name[]);
native _SetLevelVar(const name[], value);

// Functions to read/write Gamescript public variables from anywhere
native _GetGameVar(const name[]);
native _SetGameVar(const name[], value);
