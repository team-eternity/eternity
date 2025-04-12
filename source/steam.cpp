//
// ironwail
// Copyright (C) 2022 A. Drexler
//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//------------------------------------------------------------------------------
//
// Purpose: Steam config parsing.
// Authors: A. Drexler, Max Waine
//

#include "z_zone.h"

#include "d_dwfile.h"
#include "m_ctype.h"
#include "steam.h"

#ifdef EE_FEATURE_REGISTRYSCAN

#include "Win32/i_registry.h"

// Steam Install Path Registry Key
static const registry_value_t steamInstallValue =
{
   HKEY_LOCAL_MACHINE,
   SOFTWARE_KEY "\\Valve\\Steam",
   "InstallPath"
};

bool Steam_GetDir(qstring &dirout)
{
   return I_GetRegistryString(steamInstallValue, dirout);
}

#elif (EE_CURRENT_PLATFORM == EE_PLATFORM_LINUX) || (EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX)
bool Steam_GetDir(qstring &dirout)
{
   // TODO: Take Sys_GetSteamDir from ironwail and allow this for Linux and Mac too
   // See https://github.com/andrei-drexler/ironwail/blob/eeec028/Quake/sys_sdl_unix.c#L249
   return false;
}
#else
bool Steam_GetDir(qstring &dirout)
{
   return false;
}
#endif // defined(EE_FEATURE_REGISTRYSCAN)

struct vdbcontext_t;

using vdbcallback_t = void (*)(vdbcontext_t *ctx, const char *key, const char *value);

struct vdbcontext_t
{
   void          *userdata;
   vdbcallback_t  callback;
   int            depth;
   const char    *path[256];
};

//
// Parses a quoted string (potentially with escape sequences)
//
static char *VDB_ParseString(char **buf)
{
   char *ret, *write;
   while(ectype::isSpace(**buf))
      ++*buf;

   if(**buf != '"')
      return nullptr;

   write = ret = ++*buf;
   for(;;)
   {
      if(!**buf) // premature end of buffer
         return nullptr;

      if(**buf == '\\') // escape sequence
      {
         ++*buf;
         switch(**buf)
         {
            case '\'':  *write++ = '\''; break;
            case '"':   *write++ = '"';  break;
            case '?':   *write++ = '\?'; break;
            case '\\':  *write++ = '\\'; break;
            case 'a':   *write++ = '\a'; break;
            case 'b':   *write++ = '\b'; break;
            case 'f':   *write++ = '\f'; break;
            case 'n':   *write++ = '\n'; break;
            case 'r':   *write++ = '\r'; break;
            case 't':   *write++ = '\t'; break;
            case 'v':   *write++ = '\v'; break;
            default:   // unsupported sequence
               return nullptr;
         }
         ++*buf;
         continue;
      }

      if(**buf == '"') // end of quoted string
      {
         ++*buf;
         *write = '\0';
         break;
      }

      *write++ = **buf;
      ++*buf;
   }

   return ret;
}

//
// Parses either a simple key/value pair or a node
//
static bool VDB_ParseEntry(char **buf, vdbcontext_t *ctx)
{
   char *name;

   while(ectype::isSpace(**buf))
      ++*buf;
   if(!**buf) // end of buffer
      return true;

   name = VDB_ParseString(buf);
   if(!name)
      return false;

   while(ectype::isSpace(**buf))
      ++*buf;

   if(**buf == '"') // key-value pair
   {
      char *value = VDB_ParseString(buf);
      if(!value)
         return false;
      ctx->callback(ctx, name, value);
      return true;
   }

   if(**buf == '{') // node
   {
      ++*buf;
      if(ctx->depth == earrlen(ctx->path))
         return false;
      ctx->path[ctx->depth++] = name;

      while(**buf)
      {
         while(ectype::isSpace(**buf))
            ++*buf;

         if(**buf == '}')
         {
            ++*buf;
            --ctx->depth;
            break;
         }

         if(**buf == '"')
         {
            if(!VDB_ParseEntry(buf, ctx))
               return false;
            else
               continue;
         }

         if(**buf)
            return false;
      }

      return true;
   }

   return false;
}

//
// Parses the given buffer in-place, calling the specified callback for each key/value pair
//
static bool VDB_Parse(char *buf, vdbcallback_t callback, void *userdata)
{
   vdbcontext_t ctx{};
   ctx.userdata = userdata;
   ctx.callback = callback;
   ctx.depth = 0;

   while(*buf)
      if(!VDB_ParseEntry(&buf, &ctx))
         return false;

   return true;
}

//
// Examines all steam library folders and returns the path of the one containing the game with the given appid
//
struct libsparser_t
{
   const char   *appid;
   const char   *current;
   const char   *result;
};

static void VDB_OnLibFolderProperty(vdbcontext_t *ctx, const char *key, const char *value)
{
   libsparser_t *parser = static_cast<libsparser_t *>(ctx->userdata);
   int idx;

   if(ctx->depth >= 2 && !strcmp(ctx->path[0], "libraryfolders") && sscanf(ctx->path[1], "%d", &idx) == 1)
   {
      if(ctx->depth == 2)
      {
         if(!strcmp(key, "path"))
            parser->current = value;
      }
      else if(ctx->depth == 3 && !strcmp(key, parser->appid) && !strcmp(ctx->path[2], "apps"))
      {
         parser->result = parser->current;
      }
   }
}

//
// Finds the path relative to the library folder
//
struct acfparser_t
{
   const char *result;
};

static void ACF_OnManifestProperty(vdbcontext_t *ctx, const char *key, const char *value)
{
   acfparser_t *parser = static_cast<acfparser_t *>(ctx->userdata);
   if(ctx->depth == 1 && !strcmp(key, "installdir") && !strcmp(ctx->path[0], "AppState"))
      parser->result = value;
}

//
// Returns malloc'ed buffer for a given file path
//
static bool LoadFile_TextMode(qstring &outstr, const char *path)
{
   DWFILE  dwfile;
   qstring tempText;

   dwfile.openFile(path, "rt");
   if(!dwfile.isOpen())
      return false;

   tempText.initCreateSize(dwfile.fileLength() + 1);
   dwfile.read(tempText.getBuffer(), dwfile.fileLength(), 1);
   outstr = tempText.constPtr();
   return true;
}

//
// Returns malloc'ed buffer with Steam library folders config
//
static bool Steam_ReadLibFolders(qstring &steamcfg)
{
   qstring path;

   if(!Steam_GetDir(path))
      return false;
   path.pathConcatenate("config/libraryfolders.vdf");
   if(path.length() > PATH_MAX)
      return false;

   LoadFile_TextMode(steamcfg, path.constPtr());
   return true;
}

//
// Finds the Steam library and subdirectory for the given appid
//
bool Steam_FindGame(steamgame_t *game, int appid)
{
   char         appidstr[32];
   qstring      path;
   qstring      steamcfg, manifest;
   libsparser_t libparser;
   acfparser_t  acfparser;
   size_t       liblen, sublen;

   *game       = {};
   game->appid = appid;

   if(!Steam_ReadLibFolders(steamcfg))
      return false;

   snprintf(appidstr, sizeof(appidstr), "%d", appid);
   libparser = {};
   libparser.appid = appidstr;
   if(!VDB_Parse(steamcfg.getBuffer(), VDB_OnLibFolderProperty, &libparser))
      return false;

   if(path.Printf(PATH_MAX, "%s/steamapps/appmanifest_%s.acf", libparser.result, appidstr) < 0)
      return false;
   path.normalizeSlashes();

   if(!LoadFile_TextMode(manifest, path.constPtr()))
      return false;

   memset(&acfparser, 0, sizeof(acfparser));
   if(!VDB_Parse(manifest.getBuffer(), ACF_OnManifestProperty, &acfparser))
      return false;

   liblen = strlen(libparser.result);
   sublen = strlen(acfparser.result);

   if(liblen + 1 + sublen + 1 > earrlen(game->library))
      return false;

   memcpy(game->library, libparser.result, liblen + 1);
   game->subdir = game->library + liblen + 1;
   memcpy(game->subdir, acfparser.result, sublen + 1);

   return true;
}

//
// Fills in the OS path where the game is installed
//
bool Steam_ResolvePath(qstring &path, const steamgame_t *game)
{
   if(game->subdir)
   {
      path.Printf(0, "%s/steamapps/common/%s", game->library, game->subdir);
      path.normalizeSlashes();

      return path.length() < PATH_MAX;
   }

   return false;
}

// TODO: Allow loading from `rerelease\DOOM II_Data\StreamingAssets`?
// If so, bring back Steam_ChooseQuakeVersion from ironwail.

// EOF

