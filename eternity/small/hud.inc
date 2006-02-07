/*
    Eternity Engine Small Header
    Heads-Up Display Interface
*/

#if defined _hud_included
  #endinput
#endif
#define _hud_included

#include <fixed.inc>

// patch colors
enum
{
   _PCR_BRICK,
   _PCR_TAN,
   _PCR_GRAY,
   _PCR_GREEN,
   _PCR_BROWN,
   _PCR_GOLD,
   _PCR_RED,
   _PCR_BLUE,
   _PCR_ORANGE,
   _PCR_YELLOW  
};

// Creates a new widget that displays a graphic patch.
native _NewPatchWidget(const name[], const patch[], x, y, color = -1, tl = FRACUNIT);

// Sets a widget to a new patch. If the indicated widget doesn't exist or isn't a patch
// widget, nothing will happen. The name in patch MUST be a valid patch graphic lump.
native _SetWidgetPatch(const name[], const patch[]);

// Sets color/tl properties of a patch widget. If the indicated widget doesn't exist or isn't
// a patch widget, nothing will happen. The default values for both of these parameters mean
// that value won't be changed. So, if you want to, for instance, only change the color, leave
// tl set to -1 and it will not change. A color value of -1 will clear the translation completely.
native _PatchWidgetColor(const name[], color = -2, tl = -1);

// built-in fonts
enum _fonts
{
   _FONT_NORMAL,
   _FONT_HUD,
   _FONT_BIG,
   _FONT_BIG_NUM
}

// text widget behavior flags
enum
{
   _TW_AUTOMAP_ONLY = 0x00000001, // appears in automap only
   _TW_NOCLEAR      = 0x00000002, // is not cleared at level start or view change
   _TW_BOXED        = 0x00000004, // is surrounded by a box
}

// Creates a new widget that displays a text message.
native _NewTextWidget(const name[], const msg[], x, y, _fonts:font = _FONT_NORMAL, cleartics = 0, flags = 0);

// Retrieves as much of a widget's text message as will fit into dest, up to the number of
// characters in "size", which must be less than or equal to the length of the array. If the
// widget indicated by name is not a text widget or doesn't exist, nothing will be changed. If
// the widget is a text widget but has no text, an empty string is returned.
native _GetWidgetText(const name[], dest[], size, bool:packed = false);

// Sets the message of a text widget. If the indicated widget doesn't exist or isn't a text
// widget, nothing will happen.
native _SetWidgetText(const name[], const value[], cleartics = 0);

// Toggles the disable state of a widget. Set disable to true to turn the widget off.
native _ToggleWidget(const name[], bool:disable);

// Moves the indicated widget, if it exists.
native _MoveWidget(const name[], x, y);
