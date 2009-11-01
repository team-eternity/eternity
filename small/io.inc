/*
    Eternity Engine Small Header
    Native functions for input/output
*/

#if defined _io_included
  #endinput
#endif
#define _io_included

// print pre-formatted strings to the console
native _ConsolePrint(const string[], ...);

// print a <hr>-style divider bar -- a convenience method
native _ConsoleHR();

// emit a standard warning sound
native _ConsoleBeep();

// convert a number to a string
// * string should be 33 chars or more long!
native _Itoa(number, string[], base, bool:packed);

// Message types for formatted message functions
enum _msgtypes
{
   _MSG_CONSOLE, // console message
   _MSG_NORMAL,  // normal player message at top
   _MSG_CENTER   // Quake-style centered message
}

// Special message character codes
// These are an alternative to using escape sequences.
enum
{
   _CR_BRICK = 128,
   _CR_TAN,
   _CR_GRAY,
   _CR_GREEN,
   _CR_BROWN,
   _CR_GOLD,
   _CR_RED,
   _CR_BLUE,
   _CR_ORANGE,
   _CR_YELLOW,
   _CR_TRANS,
   _CR_NORMAL,
   _CR_HI,
   _CR_ERROR
}

// print a formatted message in various manners
native _Printf(_msgtypes:msgtype, const format[], ...);

// prints a center message that will stay on the screen for the
// given amount of time
native _CenterMsgTimed(const msg[], tics);
