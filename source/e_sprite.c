//----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//----------------------------------------------------------------------------
//
// EDF Sprites Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "info.h"
#include "m_dllist.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_lib.h"
#include "e_hash.h"
#include "e_sprite.h"

// Sprite hashing
#define NUMSPRCHAINS 257
static int *sprchains = NULL;
static int *sprnext = NULL;

// utility functions

//
// E_SpriteNumForName
//
// Sprite hashing function. Returns the index of "name" in the sprnames array,
// if found. If not, returns -1.
//
int E_SpriteNumForName(const char *name)
{
   int sprnum;
   unsigned int sprkey = D_HashTableKey(name) % NUMSPRCHAINS;

   sprnum = sprchains[sprkey];
   while(sprnum != -1 && strcasecmp(name, sprnames[sprnum]))
      sprnum = sprnext[sprnum];

   return sprnum;
}

//
// E_ProcessSprites
//
// Sets NUMSPRITES, allocates sprnames, sprchains, and sprnext,
// initializes the sprite hash table, and reads in all sprite names,
// adding each to the hash table as it is read.
//
void E_ProcessSprites(cfg_t *cfg)
{
   char *spritestr;
   int i;

   E_EDFLogPuts("\t* Processing sprites\n");

   // set NUMSPRITES and allocate tables
   NUMSPRITES = cfg_size(cfg, SEC_SPRITE);

   E_EDFLogPrintf("\t\t%d sprite name(s) defined\n", NUMSPRITES);

   // At least one sprite is required
   if(!NUMSPRITES)
      E_EDFLoggedErr(2, "E_ProcessSprites: no sprite names defined.\n");

   // 10/17/03: allocate a single sprite string instead of a bunch
   // of separate ones to save tons of memory and some time
   spritestr = calloc(NUMSPRITES, 8);

   // allocate with size+1 for the NULL terminator
   sprnames  = malloc((NUMSPRITES + 1) * sizeof(char *));
   sprchains = malloc(NUMSPRCHAINS * sizeof(int));
   sprnext   = malloc((NUMSPRITES + 1) * sizeof(int));

   // initialize the sprite hash table
   for(i = 0; i < NUMSPRCHAINS; ++i)
      sprchains[i] = -1;
   for(i = 0; i < NUMSPRITES + 1; ++i)
      sprnext[i] = -1;

   for(i = 0; i < NUMSPRITES; ++i)
   {
      unsigned int key;
      // read in all sprite names
      const char *sprname = cfg_getnstr(cfg, SEC_SPRITE, i);

      if(strlen(sprname) != 4)
      {
         E_EDFLoggedErr(2, 
            "E_ProcessSprites: invalid sprite mnemonic '%s'\n", sprname);
      }

      // initialize sprnames[i] to point into the single string
      // allocation above, then copy the EDF value into that location
      sprnames[i] = spritestr + i * 8;
      
      strncpy(sprnames[i], sprname, 4);
      
      // build sprite name hash table
      key = D_HashTableKey(sprnames[i]) % NUMSPRCHAINS;
      sprnext[i] = sprchains[key];
      sprchains[key] = i;
   }

   // set the pointer at index NUMSPRITES to NULL (used by the
   // renderer when it iterates over the sprites)
   sprnames[NUMSPRITES] = NULL;

   E_EDFLogPrintf("\t\tFirst sprite = %s\n\t\tLast sprite = %s\n",
                  sprnames[0], sprnames[NUMSPRITES-1]);
}

// EOF

