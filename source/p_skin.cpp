//--------------------------------------------------------------------------
//
// Copyright(C) 2006 Simon Howard, James Haley, et al.
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
//                Copyright(C) 1999 Simon Howard 'Fraggle'
//
// Skins (Doom Legacy)
//
// Skins are a set of sprites which replace the normal player sprites, so
// in multiplayer the players can look like whatever they want.
//
// NETCODE_FIXME -- CONFIG_FIXME: Need to be able to save skin values more
// effectively.
//
//--------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "c_runcmd.h"
#include "c_io.h"
#include "c_net.h"
#include "d_player.h"
#include "doomstat.h"
#include "d_main.h"
#include "p_info.h"
#include "p_skin.h"
#include "r_things.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "d_io.h" // SoM 3/13/02: Get rid of strncasecmp warnings in VC++
#include "e_edf.h"
#include "e_sound.h"
#include "e_player.h"
#include "d_gi.h"

// haleyjd: default skin is now mapped through the player class and is defined
// in EDF

static int numskins = 0;      // haleyjd 03/22/03
static int numskinsalloc = 0; // haleyjd 03/22/03
static int numedfskins;       // haleyjd 11/14/06
static skin_t **skins = NULL;

static skin_t **monster_skins = NULL; // haleyjd 09/26/04

char **spritelist = NULL;
char *default_skin = NULL;     // name of currently selected skin

static const char *skinsoundnames[NUMSKINSOUNDS] =
{
   "dsplpain",
   "dspdiehi",
   "dsoof",
   "dsslop",
   "dspunch",
   "dsradio",
   "dspldeth",
   "dsplfall",
   "dsplfeet",
   "dsfallht",
   "dsplwdth",
   "dsnoway",
};

// forward prototypes
static void P_AddSkin(skin_t *newskin);
static void P_AddEDFSkins(void);
static void P_CacheFaces(skin_t *skin);
static void P_InitMonsterSkins(void);
static skin_t *P_SkinForName(const char *s);

//
// P_ResolveSkinSounds
//
// Resolve all missing sounds for skins.
//
static void P_ResolveSkinSounds(skin_t *skin)
{
   int i;
   const char **skinsounddefs = GameModeInfo->skinSounds;

   for(i = 0; i < NUMSKINSOUNDS; ++i)
   {
      if(!skin->sounds[i])
         skin->sounds[i] = estrdup(skinsounddefs[i]);
   }
}

//
// P_InitSkins
//
// Allocates the combined sprite list and creates the default
// skin for the current game mode
//
// haleyjd 03/22/03: significant rewriting, safety increased
// haleyjd 09/26/04: initialize the monster skins list here
//
void P_InitSkins(void)
{
   int i;
   char **currentsprite;
   int numskinsprites;

   // haleyjd 09/26/04: initialize monster skins list
   P_InitMonsterSkins();

   // FIXME: problem here with preferences
   if(default_skin == NULL) 
   {
      playerclass_t *dpc = E_PlayerClassForName(GameModeInfo->defPClassName);
      default_skin = estrdup(dpc->defaultskin->skinname);
   }

   // haleyjd 11/14/06: add EDF skins
   P_AddEDFSkins();

   // allocate spritelist
   if(spritelist)
      Z_Free(spritelist);
   
   // don't count any skin with an already-existing sprite
   if((numskinsprites = numskins - numedfskins) < 0)
      I_Error("P_InitSkins: numedfskins > numskins\n");

   spritelist = ecalloc(char **, numskinsprites + NUMSPRITES + 1, sizeof(char *));

   // add the normal sprites
   currentsprite = spritelist;
   for(i = 0; i < NUMSPRITES + 1; ++i)
   {
      if(!sprnames[i])
         break;
      *currentsprite = sprnames[i];
      currentsprite++;
   }

   // add skin sprites
   for(i = 0; i < numskins; ++i)
   {
      // haleyjd 11/14/06: don't add EDF skin sprites
      if(!skins[i]->edfskin)
      {
         *currentsprite   = skins[i]->spritename;
         skins[i]->sprite = static_cast<spritenum_t>(currentsprite - spritelist);
         currentsprite++;
      }
      P_ResolveSkinSounds(skins[i]); // haleyjd 10/17/05: resolve sounds
      P_CacheFaces(skins[i]);
   }

   *currentsprite = NULL;     // end in null
}


//
// P_AddEDFSkins
//
// haleyjd 11/14/06: Replaces P_CreateMarine. Adds all EDF player skins to the
// main skin list.
//
static void P_AddEDFSkins(void)
{
   // go down every hash chain
   for(skin_t *chain : edf_skins)
   {
      while(chain)
      {
         // add the skin only if one of this name doesn't already exist
         if(!P_SkinForName(chain->skinname))
         {
            P_AddSkin(chain);
            ++numedfskins;
         }

         chain = chain->ehashnext;
      }
   }
}

//
// P_AddSkin
//
// Add a new skin to the skins list
//
// haleyjd 03/22/03: rewritten
//
static void P_AddSkin(skin_t *newskin)
{
   // FIXME: needs to use hash table instead of array
   if(numskins >= numskinsalloc)
   {
      numskinsalloc = numskinsalloc ? numskinsalloc*2 : 32;
      skins = erealloc(skin_t **, skins, numskinsalloc*sizeof(skin_t *));
   }
   
   skins[numskins] = newskin;
   numskins++;
}

//
// Checks if it's a valid sprite name, to mitigate false finds
//
static bool P_isValidSpriteName(const char *name)
{
   size_t len = strlen(name);
   if(len != 6 && len != 8)
      return false;
   if(name[4] - 'A' >= MAX_SPRITE_FRAMES || name[5] - '0' > 8)
      return false;
   if(len == 8 && (name[6] - 'A' >= MAX_SPRITE_FRAMES || name[7] - '0' > 8))
      return false;
   return true;
}

static void P_AddSpriteLumps(const char *named)
{
   int i, n = static_cast<int>(strlen(named));
   int numlumps = wGlobalDir.getNumLumps();
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();
   
   for(i = 0; i < numlumps; i++)
   {
      if(!strncasecmp(lumpinfo[i]->name, named, n) &&
         lumpinfo[i]->li_namespace == lumpinfo_t::ns_global &&
         P_isValidSpriteName(lumpinfo[i]->name))
      {
         // mark as sprites so that W_CoalesceMarkedResource
         // will group them as sprites
         lumpinfo[i]->li_namespace = lumpinfo_t::ns_sprites;
      }
   }
}

static skin_t *newskin;

static void P_ParseSkinCmd(const char *line)
{
   int i;
   
   while(*line==' ') 
      line++;
   if(!*line) 
      return;      // maybe nothing left now
   
   if(!strncasecmp(line, "name", 4))
   {
      const char *skinname = line+4;
      while(*skinname == ' ') 
         skinname++;
      efree(newskin->skinname);
      newskin->skinname = estrdup(skinname);
   }
   else if(!strncasecmp(line, "sprite", 6))
   {
      const char *spritename = line+6;
      while(*spritename == ' ')
         spritename++;
      strncpy(newskin->spritename, spritename, 4);
      newskin->spritename[0] = ectype::toUpper(newskin->spritename[0]);
      newskin->spritename[1] = ectype::toUpper(newskin->spritename[1]);
      newskin->spritename[2] = ectype::toUpper(newskin->spritename[2]);
      newskin->spritename[3] = ectype::toUpper(newskin->spritename[3]);
      newskin->spritename[4] = 0;
   }
   else if(!strncasecmp(line, "face", 4))
   {
      const char *facename = line+4;
      while(*facename == ' ')
         facename++;
      efree(newskin->facename);
      newskin->facename = estrdup(facename);
      if(strlen(newskin->facename) > 3)
         newskin->facename[3] = 0;
   }

   // is it a sound?
   
   for(i = 0; i < NUMSKINSOUNDS; i++)
   {
      if(!strncasecmp(line, skinsoundnames[i], strlen(skinsoundnames[i])))
      {                    // yes!
         const char *newsoundname = line + strlen(skinsoundnames[i]);
         while(*newsoundname == ' ')
            newsoundname++;
         
         if(ectype::toUpper(newsoundname[0]) == 'D' && ectype::toUpper(newsoundname[1]) == 'S')
            newsoundname += 2;        // ds
         
         newskin->sounds[i] = estrdup(newsoundname);
      }
   }
}

void P_ParseSkin(int lumpnum)
{
   lumpinfo_t *const *lumpinfo = wGlobalDir.getLumpInfo();

   // FIXME: revise to use finite-state-automaton parser and qstring buffers

   newskin = estructalloc(skin_t, 1);

   newskin->spritename = (char *)(Z_Malloc(5, PU_STATIC, 0));
   if(lumpnum + 1 < wGlobalDir.getNumLumps())
      strncpy(newskin->spritename, lumpinfo[lumpnum + 1]->name, 4);
   else
      newskin->spritename[0] = 0;
   newskin->spritename[4] = 0;

   newskin->facename = estrdup("STF");      // default status bar face
   newskin->faces    = NULL;

   newskin->type    = SKIN_PLAYER; // haleyjd: it's a player skin
   newskin->edfskin = false;       // haleyjd: it's not an EDF skin

   // set sounds to defaults
   // haleyjd 10/17/05: nope, can't do it here now, see top of file

   auto lump = (char *)(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC));  // get the lump
   if(!lump)
   {
      efree(newskin->facename);
      efree(newskin->spritename);
      efree(newskin);
      return;
   }
   
   const char *rover = lump;
   bool comment = false;

   char inputline[256] = {};
   const char *lumpend = lump + lumpinfo[lumpnum]->size;
   while(rover < lumpend)
   {
      if((*rover == '/' && rover + 1 < lumpend && *(rover + 1) == '/') ||        // '//'
         *rover == ';' || *rover == '#')           // ';', '#'
      {
         comment = true;
      }
      if(*rover > 31 && !comment)
      {
         psnprintf(inputline, sizeof(inputline), "%s%c", 
                   inputline, (*rover == '=') ? ' ' : *rover);
      }
      if(*rover=='\n') // end of line
      {
         P_ParseSkinCmd(inputline);    // parse the line
         memset(inputline, 0, sizeof(inputline));
         comment = false;
      }
      rover++;
   }
   P_ParseSkinCmd(inputline);    // parse the last line
   
   Z_ChangeTag(lump, PU_CACHE); // mark lump purgable

   if(!newskin->skinname || !*newskin->spritename)
   {
      // Reject unnamed or last skin in file without mention
      for(int i = 0; i < earrlen(newskin->sounds); ++i)
         efree(newskin->sounds[i]);
      efree(newskin->facename);
      efree(newskin->spritename);
      efree(newskin);
      return;
   }
   
   P_AddSkin(newskin);
   P_AddSpriteLumps(newskin->spritename);
}

static void P_CacheFaces(skin_t *skin)
{
   if(skin->faces) 
      return; // already cached
   
   if(!strcasecmp(skin->facename, "STF"))
   {
      skin->faces = default_faces;
   }
   else
   {
      skin->faces = (patch_t **)(Z_Malloc(ST_NUMFACES * sizeof(patch_t *), PU_STATIC, 0));
      ST_CacheFaces(skin->faces, skin->facename);
   }
}

// this could be done with a hash table for speed.
// i cant be bothered tho, its not something likely to be
// being done constantly, only now and again

static skin_t *P_SkinForName(const char *s)
{
   int i;

   // FIXME: needs to use a chained hash table with front insertion
   // now to properly support EDF skins

   while(*s==' ')
      s++;
   
   if(!skins)
      return NULL;

   for(i = 0; i < numskins; i++)
   {
      if(!strcasecmp(s, skins[i]->skinname))
         return skins[i];
   }

   return NULL;
}

//
// P_GetDefaultSkin
//
// Gets the default skin for a player.
//
// PCLASS_TODO: what about preferences?
//
skin_t *P_GetDefaultSkin(player_t *player)
{
#ifdef RANGECHECK
   if(!player->pclass)
      I_Error("P_GetDefaultSkin: NULL playerclass\n");
#endif

   return player->pclass->defaultskin;
}

void P_SetSkin(skin_t *skin, int playernum)
{
   player_t *pl = &players[playernum];

   if(!playeringame[playernum])
      return;
   
   pl->skin = skin;
   
   if(gamestate == GS_LEVEL)
   {
      Mobj *mo = pl->mo;
      
      mo->skin = skin;

      if(mo->state && mo->state->sprite == mo->info->defsprite)
         mo->sprite = skin->sprite;
   }
   
   if(playernum == consoleplayer)
      default_skin = skin->skinname;
}

        // change to previous skin
static skin_t *P_PrevSkin(int player)
{
   int skinnum;
      
   // find the skin in the list first
   for(skinnum = 0; skinnum < numskins; skinnum++)
   {
      if(players[player].skin == skins[skinnum])
         break;
   }
         
   if(skinnum == numskins)
      return NULL;         // not found (?)

   --skinnum;      // previous skin
   
   if(skinnum < 0) 
      skinnum = numskins-1;   // loop around
   
   return skins[skinnum];
}

        // change to next skin
static skin_t *P_NextSkin(int player)
{
   int skinnum;
      
   // find the skin in the list first

   for(skinnum = 0; skinnum < numskins; skinnum++)
   {
      if(players[player].skin == skins[skinnum]) 
         break;
   }

   if(skinnum == numskins)
      return NULL;         // not found (?)
   
   ++skinnum;      // next skin
   
   if(skinnum >= numskins) skinnum = 0;    // loop around
   
   return skins[skinnum];
}

//
// P_InitMonsterSkins
//
// haleyjd 09/26/04:
// Allocates the skin_t pointer array for monster skins with size
// NUMSPRITES (only one monster skin is needed at max for each sprite).
// Must be called after EDF and before first use of P_GetMonsterSkin.
//
static void P_InitMonsterSkins(void)
{
   if(!monster_skins)
      monster_skins = (skin_t **)(Z_Calloc(NUMSPRITES, sizeof(skin_t *), PU_STATIC, 0));
}

//
// P_GetMonsterSkin
//
// haleyjd 09/26/04:
// If a monster skin doesn't exist for the requested sprite, one will
// be created. Otherwise, the existing skin is returned.
//
skin_t *P_GetMonsterSkin(spritenum_t sprnum)
{
#ifdef RANGECHECK
   if(sprnum < 0 || sprnum >= NUMSPRITES)
      I_Error("P_GetMonsterSkin: sprite %d out of range\n", sprnum);
#endif

   if(!monster_skins[sprnum])
   {
      monster_skins[sprnum] = estructalloc(skin_t, 1);

      monster_skins[sprnum]->type = SKIN_MONSTER;
      monster_skins[sprnum]->sprite = sprnum;
   }

   return monster_skins[sprnum];
}

/**** console stuff ******/

CONSOLE_COMMAND(listskins, 0)
{
   int i;

   for(i = 0; i < numskins; i++)
   {      
      C_Printf("%s\n", skins[i]->skinname);
   }
}

//      helper macro to ensure grammatical correctness :)

#define isvowel(c)              \
        ( (c)=='a' || (c)=='e' || (c)=='i' || (c)=='o' || (c)=='u' )

VARIABLE_STRING(default_skin, NULL, 256);

        // player skin
CONSOLE_NETVAR(skin, default_skin, cf_handlerset, netcmd_skin)
{
   skin_t *skin;
   
   if(!Console.argc)
   {
      if(consoleplayer == Console.cmdsrc)
         C_Printf("%s is %s %s\n", players[Console.cmdsrc].name,
                  isvowel(players[Console.cmdsrc].skin->skinname[0]) ? "an" : "a",
                  players[Console.cmdsrc].skin->skinname);
      return;
   }

   if(*Console.argv[0] == "+")
      skin = P_NextSkin(Console.cmdsrc);
   else if(*Console.argv[0] == "-")
      skin = P_PrevSkin(Console.cmdsrc);
   else if(!(skin = P_SkinForName(Console.argv[0]->constPtr())))
   {
      if(consoleplayer == Console.cmdsrc)
         C_Printf("skin not found: '%s'\n", Console.argv[0]->constPtr());
      return;
   }

   P_SetSkin(skin, Console.cmdsrc);
}

// EOF

