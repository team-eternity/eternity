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
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: EDF sprites module.
// Authors: James Haley, Max Waine
//

#include "z_zone.h"
#include "info.h"
#include "m_dllist.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_edf.h"
#include "e_lib.h"
#include "e_hash.h"
#include "e_sprite.h"
#include "i_system.h"

// Data

static int numspritesalloc; // number of sprites allocated

// Sprite hashing

struct esprite_t
{
    DLListItem<esprite_t> namelinks;   // name hash links
    DLListItem<esprite_t> dsdnumlinks; // dsdnumnum hash links

    char *nameptr;   // name hash key
    int   num;       // sprite number
    int   adddehnum; // additive dehacked sprite number
    char  name[5];   // sprite name
    bool  adddeh;    // made by additive dehacked
};

// sprite name hash table
static EHashTable<esprite_t, ENCStringHashKey, &esprite_t::nameptr, &esprite_t::namelinks> spritenamehash(257);

// sprite additive dehacked num hash table
static EHashTable<esprite_t, EIntHashKey, &esprite_t::adddehnum, &esprite_t::dsdnumlinks> spritedsdnumhash(257);

//
// E_SpriteNumForName
//
// Sprite hashing function. Returns the index of "name" in the sprnames array,
// if found. If not, returns -1.
//
int E_SpriteNumForName(const char *name)
{
    esprite_t *obj;
    int        spritenum = -1;

    if((obj = spritenamehash.objectForKey(name)))
        spritenum = obj->num;

    return spritenum;
}

//
// As above but for additive dehacked
//
int E_SpriteNumForDEHNum(const int num)
{
    esprite_t *obj;
    int        spritenum = -1;

    if((obj = spritedsdnumhash.objectForKey(num)))
        spritenum = obj->num;

    return spritenum;
}

//
// Updates the name of a given sprite, overwriting the old name
//
void E_UpdateSpriteName(const char *oldname, const char *newname, const int newlen)
{
    constexpr int SPRITE_NAME_LENGTH = int(earrlen(esprite_t::name) - 1);

    if(estrnonempty(oldname) && estrnonempty(newname) && !strcmp(oldname, newname))
        return;

    esprite_t *obj = spritenamehash.objectForKey(oldname);
    if(!obj)
        return;

    if(newlen > SPRITE_NAME_LENGTH)
    {
        I_Error("E_UpdateSpriteName: Attempted to set sprite name to %s, which is longer than the maximum of %d "
                "characters\n",
                newname, SPRITE_NAME_LENGTH);
    }

    spritenamehash.removeObject(obj);
    strncpy(obj->name, estrnonempty(newname) ? newname : "", newlen);
    if(estrnonempty(newname))
        spritenamehash.addObject(obj);
}

//
// E_AddSprite
//
// haleyjd 03/23/10: Add a sprite name to sprnames, if such is not already
// present. Returns true if successful and false otherwise.
//
static bool E_AddSprite(const char *name, esprite_t *sprite, int num = -1)
{
    // initialize the esprite object
    strncpy(sprite->name, name ? name : "", 4);
    sprite->num       = NUMSPRITES;
    sprite->adddehnum = num < 0 ? NUMSPRITES : num;
    sprite->nameptr   = sprite->name;

    if(name && spritenamehash.objectForKey(name))
        return false; // don't add the same sprite name twice

    E_EDFLogPrintf("\t\tAdding spritename %s\n", name ? name : "NAMELESS");

    // add esprite to hashes
    if(name)
        spritenamehash.addObject(sprite);
    spritedsdnumhash.addObject(sprite);

    // reallocate sprnames if necessary
    if(NUMSPRITES + 1 >= numspritesalloc)
    {
        numspritesalloc = numspritesalloc ? numspritesalloc + 128 : 256;
        sprnames        = erealloc(char **, sprnames, numspritesalloc * sizeof(char *));
    }

    // set the new sprnames entry, and make the next one nullptr
    sprnames[NUMSPRITES]     = sprite->name;
    sprnames[NUMSPRITES + 1] = nullptr;

    ++NUMSPRITES;

    return true;
}

//
// Updates a sprite's name for an according number, or creates that sprite
// If the newname is nullptr then it will only clear the name if the name update is forced
//
void E_UpdateAddSpriteNameForNum(const int num, const char *newname, const int newlen, bool forceupdate)
{
    esprite_t *obj;
    int        spritenum = -1;

    if(!forceupdate && !newname)
        return;

    if(obj = spritedsdnumhash.objectForKey(num); obj && (!forceupdate || obj->adddeh))
    {
        spritenum = obj->num;
        if(estrnonempty(obj->name))
            E_UpdateSpriteName(obj->name, newname, newlen);
        else if(estrnonempty(newname))
        {
            strncpy(obj->name, newname, newlen);
            spritenamehash.addObject(obj);
        }
    }
    else
    {
        // we need to create a new sprite

        // remove the old sprite for this dsdnum from the hash if applicable
        if(obj)
            spritedsdnumhash.removeObject(obj);

        esprite_t *spr;

        // must be exactly 4 characters
        if(newname && strlen(newname) != 4)
            return;

        // allocate separate storage for implicit sprites
        spr = estructalloc(esprite_t, 1);

        // try adding it; if this fails, we need to free spr
        if(!E_AddSprite(newname, spr, num))
        {
            efree(spr);
            return;
        }

        spr->adddeh = true;

        // re-add the old sprite at the end of the sprite list
        if(obj)
        {
            obj->adddehnum = NUMSPRITES;
            spritedsdnumhash.addObject(obj);
        }

        return;
    }
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
    esprite_t *sprites;
    int        numarraysprites;
    int        i;

    E_EDFLogPuts("\t* Processing spritenames array\n");

    // get number of sprites in the spritenames array
    numarraysprites = cfg_size(cfg, SEC_SPRITE);

    E_EDFLogPrintf("\t\t%d sprite name(s) defined\n", numarraysprites);

    // At least one sprite is required to be defined through the
    // spritenames array, but only when no sprites have been defined
    // already.
    if(!numarraysprites)
    {
        if(!NUMSPRITES)
            E_EDFLoggedErr(2, "E_ProcessSprites: no sprite names defined.\n");
        return;
    }

    // 10/17/03: allocate a single array of sprite objects to save a lot of
    // memory and some time.
    sprites = ecalloc(esprite_t *, numarraysprites, sizeof(*sprites));

    // process each spritename
    for(i = 0; i < numarraysprites; ++i)
    {
        const char *sprname = cfg_getnstr(cfg, SEC_SPRITE, i);

        // spritenames must be exactly 4 characters long
        if(strlen(sprname) != 4)
        {
            E_EDFLoggedErr(2, "E_ProcessSprites: invalid sprite name '%s'\n", sprname);
        }

        // add the sprite
        E_AddSprite(sprname, &sprites[i]);
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
bool E_ProcessSingleSprite(const char *sprname)
{
    esprite_t *spr;

    // must be exactly 4 characters
    if(strlen(sprname) != 4)
        return false;

    // allocate separate storage for implicit sprites
    spr = estructalloc(esprite_t, 1);

    // try adding it; if this fails, we need to free spr
    if(!E_AddSprite(sprname, spr))
    {
        efree(spr);
        return false;
    }

    return true;
}

// EOF

