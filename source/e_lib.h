// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:  
//    Common utilities for "Extended Feature" modules.
//
//-----------------------------------------------------------------------------

#ifndef E_LIB_H__
#define E_LIB_H__

#include "doomtype.h"

struct dehflags_t;
struct dehflagset_t;

struct E_Enable_t
{
   const char *name;
   int enabled;
};

#ifdef NEED_EDF_DEFINITIONS

// basic stuff
void E_ErrorCB(const cfg_t *const cfg, const char *fmt, va_list ap);

// include tracking
int E_CheckRoot(cfg_t *cfg, const char *data, int size);

// function callbacks
int E_SetDialect (cfg_t *, cfg_opt_t *, int, const char **);
int E_Include    (cfg_t *, cfg_opt_t *, int, const char **);
int E_LumpInclude(cfg_t *, cfg_opt_t *, int, const char **);
int E_IncludePrev(cfg_t *, cfg_opt_t *, int, const char **);
int E_StdInclude (cfg_t *, cfg_opt_t *, int, const char **);
int E_UserInclude(cfg_t *, cfg_opt_t *, int, const char **);
int E_Endif      (cfg_t *, cfg_opt_t *, int, const char **);

// value-parsing callbacks
int E_SpriteFrameCB(cfg_t *, cfg_opt_t *, const char *, void *);
int E_IntOrFixedCB (cfg_t *, cfg_opt_t *, const char *, void *);
int E_TranslucCB   (cfg_t *, cfg_opt_t *, const char *, void *);
int E_TranslucCB2  (cfg_t *, cfg_opt_t *, const char *, void *);
int E_ColorStrCB   (cfg_t *, cfg_opt_t *, const char *, void *);

// MetaTable adapter utilities
class MetaTable;
void E_MetaStringFromCfgString(MetaTable *meta, cfg_t *cfg, const char *prop);
void E_MetaIntFromCfgInt(MetaTable *meta, cfg_t *cfg, const char *prop);
void E_MetaIntFromCfgBool(MetaTable *meta, cfg_t *cfg, const char *prop);
void E_MetaIntFromCfgFlag(MetaTable *meta, cfg_t *cfg, const char *prop);
void E_MetaTableFromCfg(cfg_t *cfg, MetaTable *table, MetaTable *prototype = nullptr);

// Prefix flag stuff
void E_SetFlagsFromPrefixCfg(cfg_t *cfg, unsigned &flags, const dehflags_t *set);

// Advanced libConfuse utilities
class qstring;
void E_CfgListToCommaString(cfg_t *sec, const char *optname, qstring &output);

#endif

bool E_CheckInclude(const char *data, size_t size);

const char *E_BuildDefaultFn(const char *filename);

// misc utilities
int E_EnableNumForName(const char *name, E_Enable_t *enables);
int E_StrToNumLinear(const char **strings, int numstrings, const char *value);
unsigned int E_ParseFlags(const char *str, dehflagset_t *flagset);
const char *E_ExtractPrefix(const char *value, char *prefixbuf, int buflen);
void E_ReplaceString(char *&dest, char *newvalue);
char *E_GetHeredocLine(char **src);
byte *E_ParseTranslation(const char *str, int tag);

#define E_MAXCMDTOKENS 8

//
// This structure is returned by E_ParseTextLine and is used to hold pointers
// to the tokens inside the command string. The pointers may be nullptr if the
// corresponding tokens do not exist.
//
struct tempcmd_t
{
   const char *strs[E_MAXCMDTOKENS]; // command and up to 2 arguments
};

tempcmd_t E_ParseTextLine(char *str);

#endif

// EOF


