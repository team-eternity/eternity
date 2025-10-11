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
// Purpose: Config file.
// Authors: James Haley, Max Waine
//

#ifndef M_MISC_H__
#define M_MISC_H__

#include "doomtype.h"

//
// MISC
//

extern int config_help;

// haleyjd 07/27/09: default file portability fix - separate types for config
// variables

enum defaulttype_e
{
    dt_integer,
    dt_string,
    dt_float,
    dt_boolean,
    dt_numtypes
};

// phares 4/21/98:
// Moved from m_misc.c so m_menu.c could see it.
//
// killough 11/98: totally restructured

// Forward declarations for interface
struct default_t;
struct variable_t;

// clang-format off

// haleyjd 07/03/10: interface object for defaults
struct default_i
{
    bool (*writeHelp) (default_t *, FILE *);       // write help message
    bool (*writeOpt)  (default_t *, FILE *);       // write option key and value
    void (*setValue)  (default_t *, void *, bool); // set value
    bool (*readOpt)   (default_t *, char *, bool); // read option from string
    void (*setDefault)(default_t *);               // set to hardcoded default
    bool (*checkCVar) (default_t *, variable_t *); // check against a cvar
    void (*getDefault)(default_t *, void *);       // get the default externally
};

// clang-format on

struct default_t
{
    enum wad_e
    {
        wad_no,
        wad_game,    // read from OPTIONS when gameplay is started (G_InitNew)
        wad_startup, // read from OPTIONS during program init
    };

    const char *const   name; // name
    const defaulttype_e type; // type

    void *const location; // default variable
    void *const current;  // possible nondefault variable

    int         defaultvalue_i; // built-in default value
    const char *defaultvalue_s;
    double      defaultvalue_f;
    bool        defaultvalue_b;

    struct
    {
        int min, max;
    } const limit; // numerical limits

    const wad_e       wad_allowed; // whether it's allowed in wads & when it's read
    const char *const help;        // description of parameter

    // internal fields (initialized implicitly to 0) follow

    default_t *first, *next; // hash table pointers
    int        modified;     // Whether it's been modified

    int         orig_default_i; // Original default, if modified
    const char *orig_default_s;
    double      orig_default_f;
    bool        orig_default_b;

    default_i *methods;

    // struct setup_menu_s *setup_menu;          // Xref to setup menu item, if any
};

// haleyjd 07/27/09: Macros for defining configuration values.

#define DEFAULT_ARGS(type) const char *const name, void *const loc, void *const cur, type def, \
    const default_t::wad_e wad, const char *const help

#define DEFAULT_NUM_ARGS(type) const char *const name, void *const loc, void *const cur, type def, \
    const int min, const int max, const default_t::wad_e wad, const char *const help

constexpr default_t DEFAULT_END()
{
    return {
        nullptr, dt_integer, nullptr, nullptr, 0, nullptr, 0.0, false, { 0, 0 },
                default_t::wad_no,
        nullptr, nullptr,    nullptr, 0,       0, nullptr, 0.0, false, nullptr
    };
}

constexpr default_t DEFAULT_INT(DEFAULT_NUM_ARGS(const int))
{
    return {
        name, dt_integer, loc,     cur, def, nullptr, 0.0, false, { min, max },
                    wad,
        help, nullptr,    nullptr, 0,   0,   nullptr, 0.0, false, nullptr
    };
}

constexpr default_t DEFAULT_STR(DEFAULT_ARGS(const char *))
{
    return {
        name, dt_string, loc,     cur, 0, def,     0.0, false, { 0, 0 },
                        wad,
        help, nullptr,   nullptr, 0,   0, nullptr, 0.0, false, nullptr
    };
}

constexpr default_t DEFAULT_FLOAT(DEFAULT_NUM_ARGS(const double))
{
    return {
        name, dt_float, loc,     cur, 0, nullptr, def, false, { min, max },
                    wad,
        help, nullptr,  nullptr, 0,   0, nullptr, 0.0, false, nullptr
    };
}

constexpr default_t DEFAULT_BOOL(DEFAULT_ARGS(const bool))
{
    return {
        name, dt_boolean, loc,     cur, 0, nullptr, 0.0, def,   { 0, 1 },
                      wad,
        help, nullptr,    nullptr, 0,   0, nullptr, 0.0, false, nullptr
    };
}

// haleyjd 03/14/09: defaultfile_t structure
struct defaultfile_t
{
    default_t *defaults;    // array of defaults
    size_t     numdefaults; // length of defaults array
    bool       hashInit;    // if true, this default file's hash table is setup
    char      *fileName;    // name of corresponding file
    bool       loaded;      // if true, defaults are loaded
};

// haleyjd 06/29/09: default overrides
struct default_or_t
{
    const char *name;
    int         defaultvalue;
};

void M_LoadOptions(const default_t::wad_e minimum_allowed); // killough 11/98

// killough 11/98:
void       M_LoadDefaultFile(defaultfile_t *df);
void       M_SaveDefaultFile(defaultfile_t *df);
void       M_LoadDefaults(void);
void       M_SaveDefaults(void);
default_t *M_FindDefaultForCVar(variable_t *var);

static constexpr int UL = -123456789; /* magic number for no min or max for parameter */

// clang-format off

// haleyjd 06/24/02: platform-dependent macros for sound/music defaults
#if defined(_SDL_VER)
    constexpr int        SND_DEFAULT = -1;
    constexpr int        SND_MIN     = -1;
    constexpr int        SND_MAX     =  1;
    constexpr const char SND_DESCR[] = "code to select digital sound; -1 is SDL sound, 0 is no sound, 1 is PC speaker emulation";
    constexpr int        MUS_DEFAULT = -1;
    constexpr int        MUS_MIN     = -1;
    constexpr int        MUS_MAX     =  0;
    constexpr const char MUS_DESCR[] = "code to select music device; -1 is SDL_mixer, 0 is no music";
#else
    constexpr int        SND_DEFAULT = 0;
    constexpr int        SND_MIN     = 0;
    constexpr int        SND_MAX     = 0;
    constexpr const char SND_DESCR[] = "no sound driver available for this platform";
    constexpr int        MUS_DEFAULT = 0;
    constexpr int        MUS_MIN     = 0;
    constexpr int        MUS_MAX     = 0;
    constexpr const char MUS_DESCR[] = "no midi driver available for this platform";
#endif

// clang-format on

#ifdef HAVE_ADLMIDILIB
extern const int BANKS_MAX;
#endif

#endif

//----------------------------------------------------------------------------
//
// $Log: m_misc.h,v $
// Revision 1.4  1998/05/05  19:56:06  phares
// Formatting and Doc changes
//
// Revision 1.3  1998/04/22  13:46:17  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.2  1998/01/26  19:27:12  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
