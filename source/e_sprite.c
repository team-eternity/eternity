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

// Data

static int     numspritesalloc; // number of sprites allocated
static ehash_t spritehash;      // sprite hash table

// Sprite hashing

typedef struct esprite_s
{
   struct mdllistitem_s link; // hash links
   const char *name;          // sprite name
   int num;                   // sprite number
} esprite_t;

E_KEYFUNC(esprite_t, name)

//
// E_SpriteNumForName
//
// Sprite hashing function. Returns the index of "name" in the sprnames array,
// if found. If not, returns -1.
//
int E_SpriteNumForName(const char *name)
{
   void *obj;
   int spritenum = -1;

   if((obj = E_HashObjectForKey(&spritehash, &name)))
      spritenum = ((esprite_t *)obj)->num;

   return spritenum;
}

//
// E_AddSprite
//
// haleyjd 03/23/10: Add a sprite name to sprnames, if such is not already 
// present. Returns true if successful and false otherwise.
//
static boolean E_AddSprite(char *name)
{
   esprite_t *sprite;

   // initialize the sprite hash if it has not been initialized
   if(!spritehash.isinit)
      E_NCStrHashInit(&spritehash, 257, E_KEYFUNCNAME(esprite_t, name), NULL);
   else if(E_HashObjectForKey(&spritehash, &name))
      return false; // don't add the same sprite name twice

   E_EDFLogPrintf("\t\tAdding spritename %s\n", name);

   if(NUMSPRITES + 1 >= numspritesalloc)
   {
      numspritesalloc = numspritesalloc ? numspritesalloc + 128 : 256;
      sprnames = realloc(sprnames, numspritesalloc * sizeof(char *));
   }
   sprnames[NUMSPRITES]     = name;
   sprnames[NUMSPRITES + 1] = NULL;

   // create an esprite object
   sprite = calloc(1, sizeof(esprite_t));
   sprite->name = name;
   sprite->num  = NUMSPRITES;

   // add esprite to hash
   E_HashAddObject(&spritehash, sprite);

   ++NUMSPRITES;

   return true;
}

//
// E_ProcessSprites
//
// haleyjd 03/23/10: Processes the EDF spritenames array, which is now largely
// only provided for the purpose of giving DOOM's sprites their proper DeHackEd
// numbers. Sprites defined through DECORATE-format state definitions will be
// added separately.
//
// When loading sprites from wad lumps at runtime, they will be added to any that
// already exist, rather than overwriting them. This shouldn't make any practical
// difference.
//
void E_ProcessSprites(cfg_t *cfg)
{
   char *spritestr;
   int numarraysprites;
   int i;

   E_EDFLogPuts("\t* Processing spritenames array\n");

   // get number of sprites in the spritenames array
   numarraysprites = cfg_size(cfg, SEC_SPRITE);

   // At least one sprite is required to be defined through the 
   // spritenames array
   if(!numarraysprites)
      E_EDFLoggedErr(2, "E_ProcessSprites: no sprite names defined.\n");

   E_EDFLogPrintf("\t\t%d sprite name(s) defined\n", numarraysprites);

   // 10/17/03: allocate a single sprite string instead of a bunch
   // of separate ones to save tons of memory and some time
   spritestr = calloc(numarraysprites, 8);

   // process each spritename
   for(i = 0; i < numarraysprites; ++i)
   {
      char *dest;
      const char *sprname = cfg_getnstr(cfg, SEC_SPRITE, i);

      // spritenames must be exactly 4 characters long
      if(strlen(sprname) != 4)
      {
         E_EDFLoggedErr(2, 
            "E_ProcessSprites: invalid sprite name '%s'\n", sprname);
      }

      // copy the libConfuse value into the proper space, which is aligned
      // to an 8-byte boundary for efficiency.
      dest = spritestr + i * 8;
      strncpy(dest, sprname, 4);

      // add the sprite
      E_AddSprite(dest);
   }

   E_EDFLogPuts("\t\tFinished spritenames\n");
}

//
// E_ProcessSingleSprite
//
// haleyjd 03/24/10: Adds a single sprite, such as one being implicitly defined
// by an EDF frame or DECORATE state. Returns true if successful and false if
// there is any kind of problem during the process.
//
boolean E_ProcessSingleSprite(const char *sprname)
{
   char *dest;

   // must be exactly 4 characters
   if(strlen(sprname) != 4)
      return false;

   // allocate separate storage for implicit sprites
   dest = strdup(sprname);

   // try adding it; if this fails, we need to free dest
   if(!E_AddSprite(dest))
   {
      free(dest);
      return false;
   }
   
   return true;
}

// EOF

