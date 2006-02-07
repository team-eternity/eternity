/*
    Eternity Engine Small Header
    Native functions for map specials
*/

#if defined _spec_included
  #endinput
#endif
#define _spec_included

#include <fixed>

// floor changer types
// NOTE: Trigger change types can ONLY be used if special mode is _SPEC_PASS!

enum
{
   _FNoChg,
   _FChgZeroTrig,
   _FChgZeroNum,
   _FChgTxtTrig,
   _FChgTxtNum,
   _FChgTypTrig,
   _FChgTypNum
}

// Constants

// standard floor speed (used by slow floors; 1 unit per tic)
#define _FLOORSPEED 8

// standard door speed (2 units per tic)
#define _VDOORSPEED 16

// standard door delay time (150 tics)
#define _VDOORWAIT 150

// Control Functions:

enum _spec_modes
{
   _SPEC_NULL,  // NULL mode: no thing or line is ever passed on to a special
   _SPEC_PASS   // PASS mode: for _INVOKE_LINE model, line and thing are passed to specials
}

// Note: The special mode will be reset to _SPEC_NULL after each line special execution.

native _SpecialMode(_spec_modes:mode);

// Param Door Specials:

native _Door_Raise(tag, speed, delay, lighttag = 0);                // 300
native _Door_Open(tag, speed, lighttag = 0);                        // 301
native _Door_Close(tag, speed, lighttag = 0);                       // 302
native _Door_CloseWaitOpen(tag, speed, delay, lighttag = 0);        // 303
native _Door_WaitRaise(tag, speed, delay, countdown, lighttag = 0); // 304
native _Door_WaitClose(tag, speed, countdown, lighttag = 0);        // 305

// Param Floor Specials:

native _Floor_RaiseToHighest(tag, speed, change = _FNoChg, crush = -1);       // 306
native _Floor_LowerToHighest(tag, speed, change = _FNoChg);                   // 307
native _Floor_RaiseToLowest(tag, change = _FNoChg, crush = -1);               // 308
native _Floor_LowerToLowest(tag, speed, change = _FNoChg);                    // 309
native _Floor_RaiseToNearest(tag, speed, change = _FNoChg, crush = -1);       // 310
native _Floor_LowerToNearest(tag, speed, change = _FNoChg);                   // 311
native _Floor_RaiseToLowestCeiling(tag, speed, change = _FNoChg, crush = -1); // 312
native _Floor_LowerToLowestCeiling(tag, speed, change = _FNoChg);             // 313
native _Floor_RaiseToCeiling(tag, speed, change = _FNoChg, crush = -1);       // 314
native _Floor_RaiseByTexture(tag, speed, change = _FNoChg, crush = -1);       // 315
native _Floor_LowerByTexture(tag, speed, change = _FNoChg);                   // 316
native _Floor_RaiseByValue(tag, speed, height, change = _FNoChg, crush = -1); // 317
native _Floor_LowerByValue(tag, speed, height, change = _FNoChg);             // 318
native _Floor_MoveToValue(tag, speed, height, change = _FNoChg, crush = -1);  // 319
native _Floor_RaiseInstant(tag, height, change = _FNoChg, crush = -1);        // 320
native _Floor_LowerInstant(tag, height, change = _FNoChg);                    // 321

// Allows setting the special of a sector.
// * must be called only while in a level
native _SectorSpecial(special, tag);

// colormap spaces
enum _cmap_spaces
{
   _cmap_middle,  // normal space
   _cmap_bottom,  // below 242 floor
   _cmap_top,     // above 242 ceiling
   _cmap_all      // set all 3 regions
}

// Can set the colormap of any partition of a sector (partitions are determined
// as for 242 deep water -- if a sector is already affected by deep water, its
// deep water control sector will be affected by this function instead).
// * must be called only while in a level
native _SectorColormap(const lump[], _cmap_spaces:pos, tag);

// Call to force a lightning strike.
// * only works if lightning is enabled via MapInfo
// * must be called only while in a level
native _ForceLightning();
