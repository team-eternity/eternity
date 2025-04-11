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
//--------------------------------------------------------------------------

#ifndef D_DEHTBL_H__
#define D_DEHTBL_H__

struct actionargs_t;

struct deh_bexptr
{
   void (*cptr)(actionargs_t *); // actual pointer to the subroutine
   const char *lookup;           // mnemonic lookup string to be specified in BEX
   int next;                     // haleyjd: for bex hash chaining   
};

extern deh_bexptr deh_bexptrs[]; // still needed in d_deh.c
extern int num_bexptrs;

struct dehstr_t
{
   const char **ppstr;   // doubly indirect pointer to string   
   const char *lookup;   // pointer to lookup string name
   const char *original; // haleyjd 10/08/06: original string
   size_t bnext;         // haleyjd: for bex hash chaining (by mnemonic)
   size_t dnext;         // haleyjd: for deh hash chaining (by value)
};

extern char **deh_spritenames;
extern char **deh_musicnames;

unsigned int D_HashTableKey(const char *str);
unsigned int D_HashTableKeyCase(const char *str);

dehstr_t *D_GetBEXStr(const char *string);
dehstr_t *D_GetDEHStr(const char *string);

// haleyjd 10/08/06: new string fetching functions
const char *DEH_String(const char *mnemonic);
bool DEH_StringChanged(const char *mnemonic);
void DEH_ReplaceString(const char *mnemonic, const char *newstr);

deh_bexptr *D_GetBexPtr(const char *mnemonic);

void D_BuildBEXHashChains();
void D_BuildBEXTables();

// haleyjd: flag field parsing stuff is now global for EDF and
// ExtraData usage
struct dehflags_t
{
   const char *name;
   unsigned int value;
   int index;
};

inline constexpr int MAXFLAGFIELDS = 5;

enum
{
   DEHFLAGS_MODE1,
   DEHFLAGS_MODE2,
   DEHFLAGS_MODE3,
   DEHFLAGS_MODE4,
   DEHFLAGS_MODE5,
   DEHFLAGS_MODE_ALL
};

struct dehflagset_t
{
   dehflags_t *flaglist;
   int mode;
   unsigned int results[MAXFLAGFIELDS];
};

dehflags_t   *deh_ParseFlag(const dehflagset_t *flagset, const char *name);
dehflags_t   *deh_ParseFlagCombined(const char *name);
void          deh_ParseFlags(dehflagset_t *dehflags, char **strval);
unsigned int  deh_ParseFlagsSingle(const char *strval, int mode);
unsigned int *deh_ParseFlagsCombined(const char *strval);
unsigned int *deh_RemapMBF21ThingTypeFlags(const unsigned int flags);

// deh queue stuff
void D_DEHQueueInit(void);
void D_QueueDEH(const char *filename, int lumpnum);
void D_ProcessDEHQueue(void);

#endif

// EOF

