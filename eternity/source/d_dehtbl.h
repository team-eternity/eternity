//--------------------------------------------------------------------
//
// Copyright(C) 2002 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------

#ifndef __D_DEHTBL_H__
#define __D_DEHTBL_H__

#include "sounds.h"
#include "info.h"
#include "d_think.h" // for actionf_t
#include "p_mobj.h"

typedef struct
{
   void (*cptr)(mobj_t *); // actual pointer to the subroutine
   char *lookup;           // mnemonic lookup string to be specified in BEX
   void *kwds;             // haleyjd: EDF keyword set
   int next;               // haleyjd: for bex hash chaining   
} deh_bexptr;

extern deh_bexptr deh_bexptrs[]; // still needed in d_deh.c
extern int num_bexptrs;

typedef struct dehstr_s
{
   char **ppstr;   // doubly indirect pointer to string   
   char *lookup;   // pointer to lookup string name
   char *original; // haleyjd 10/08/06: original string
   int bnext;      // haleyjd: for bex hash chaining (by mnemonic)
   int dnext;      // haleyjd: for deh hash chaining (by value)
} dehstr_t;

extern char **deh_spritenames;
extern char **deh_musicnames;

unsigned int D_HashTableKey(const char *str);
unsigned int D_HashTableKeyCase(const char *str);

dehstr_t *D_GetBEXStr(const char *string);
dehstr_t *D_GetDEHStr(const char *string);

// haleyjd 10/08/06: new string fetching functions
const char *DEH_String(const char *mnemonic);
boolean DEH_StringChanged(const char *mnemonic);

deh_bexptr *D_GetBexPtr(const char *mnemonic);

void D_BuildBEXHashChains(void);
void D_BuildBEXTables(void);

// haleyjd: flag field parsing stuff is now global for EDF and
// ExtraData usage
typedef struct
{
   char *name;
   int  value;
   int  index;
} dehflags_t;

#define MAXFLAGFIELDS 3

enum
{
   DEHFLAGS_MODE1,
   DEHFLAGS_MODE2,
   DEHFLAGS_MODE3,
   DEHFLAGS_MODE_ALL
};

typedef struct
{
   dehflags_t *flaglist;
   int mode;
   int results[MAXFLAGFIELDS];
} dehflagset_t;

void deh_ParseFlags(dehflagset_t *dehflags, char **strval);
int  deh_ParseFlagsSingle(const char *strval, int mode);
int *deh_ParseFlagsCombined(const char *strval);

// deh queue stuff
void D_DEHQueueInit(void);
void D_QueueDEH(const char *filename, int lumpnum);
void D_ProcessDEHQueue(void);

#endif

// EOF

