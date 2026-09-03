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
#include <variant>

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

using DefaultValue  = std::variant<int, const char *, double, bool>;
using DefaultTarget = std::variant<int *, char **, double *, bool *>;

struct default_t
{
    enum wad_e
    {
        wad_no,
        wad_game,    // read from OPTIONS when gameplay is started (G_InitNew)
        wad_startup, // read from OPTIONS during program init
    };

    bool writeHelp(FILE *f) const;            // write help message
    bool writeOpt(FILE *f) const;             // write option key and value
    void setValue(void *, bool);              // set value
    bool readOpt(char *, bool);               // read option from string
    void setDefault();                        // set to hardcoded default
    bool checkCVar(const variable_t *) const; // check against a cvar

    const char *const name; // name

    const DefaultTarget location; // default variable
    void *const         current;  // possible nondefault variable

    DefaultValue defaultvalue; // built-in default value

    struct
    {
        int min, max;
    } const limit; // numerical limits

    const wad_e       wad_allowed; // whether it's allowed in wads & when it's read
    const char *const help;        // description of parameter

    // internal fields (initialized implicitly to 0) follow

    default_t *first, *next; // hash table pointers
    int        modified;     // Whether it's been modified

    DefaultValue orig_default; // Original default, if modified

    // struct setup_menu_s *setup_menu;          // Xref to setup menu item, if any
};

// haleyjd 07/27/09: Macros for defining configuration values.
// (ioanch) functions by now

constexpr default_t DEFAULT_END()
{
    return {
        nullptr, static_cast<int *>(nullptr), nullptr, 0, { 0, 0 },
             default_t::wad_no, nullptr, nullptr, nullptr, 0, 0
    };
}

constexpr default_t DEFAULT_INT(const char *const name, void *const loc, void *const cur, int def, const int min,
                                const int max, const default_t::wad_e wad, const char *const help)
{
    return {
        .name         = name,
        .location     = static_cast<int *>(loc),
        .current      = cur,
        .defaultvalue = def,
        .limit        = { min, max },
        .wad_allowed  = wad,
        .help         = help,
        .first        = nullptr,
        .next         = nullptr,
        .modified     = 0,
        .orig_default = 0
    };
}

constexpr default_t DEFAULT_STR(const char *const name, void *const loc, void *const cur, const char *def,
                                const default_t::wad_e wad, const char *const help)
{
    return {
        .name         = name,
        .location     = static_cast<char **>(loc),
        .current      = cur,
        .defaultvalue = def,
        .limit        = { 0, 0 },
        .wad_allowed  = wad,
        .help         = help,
        .first        = nullptr,
        .next         = nullptr,
        .modified     = 0,
        .orig_default = (const char *)nullptr
    };
}

constexpr default_t DEFAULT_FLOAT(const char *const name, void *const loc, void *const cur, double def, const int min,
                                  const int max, const default_t::wad_e wad, const char *const help)
{
    return {
        .name         = name,
        .location     = static_cast<double *>(loc),
        .current      = cur,
        .defaultvalue = def,
        .limit        = { min, max },
        .wad_allowed  = wad,
        .help         = help,
        .first        = nullptr,
        .next         = nullptr,
        .modified     = 0,
        .orig_default = 0.0
    };
}

constexpr default_t DEFAULT_BOOL(const char *const name, void *const loc, void *const cur, bool def,
                                 const default_t::wad_e wad, const char *const help)
{
    return {
        .name         = name,
        .location     = static_cast<bool *>(loc),
        .current      = cur,
        .defaultvalue = def,
        .limit        = { 0, 1 },
        .wad_allowed  = wad,
        .help         = help,
        .first        = nullptr,
        .next         = nullptr,
        .modified     = 0,
        .orig_default = false
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
