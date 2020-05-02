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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Resource archive file formats
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_iwad.h"
#include "m_qstr.h"
#include "m_utils.h"
#include "w_formats.h"
#include "w_wad.h"

enum
{
   EXT_WAD,
   EXT_PKE,
   EXT_PK3,
   EXT_ZIP,
   EXT_LMP,
   EXT_NUMEXTENSIONS
};

// Default extensions to try when a file is not found, in order.
static const char *W_defaultExtensions[] =
{
   ".wad",
   ".pke", // Recommended extension for EE-specific mods
   ".pk3",
   ".zip",
   ".lmp"
};

//
// Tries to open a file using the given filename first. If that fails, it
// will try all of the available default extensions. Returns the open file
// if it was found (nullptr if not), and sets filename equal to the filename
// that was successfully opened, with slashes normalized and the default
// extension added if one was needed.
//
FILE *W_TryOpenFile(qstring &filename, bool allowInexact)
{
   FILE *f = nullptr;
   qstring basefn;

   filename.normalizeSlashes();
   basefn = filename;

   // Try opening without an added extension first
   if((f = fopen(basefn.constPtr(), "rb")))
      return f;

   if(!allowInexact)
      return nullptr;

   // Try default extensions
   for(int i = 0; i < EXT_NUMEXTENSIONS; i++)
   {
      basefn = filename;
      basefn.addDefaultExtension(W_defaultExtensions[i]);
      if((f = fopen(basefn.constPtr(), "rb")))
      {
         filename = basefn;
         return f;
      }
   }

   // Try searching the dirs in DOOMWADPATH
   const size_t numpaths = D_GetNumDoomWadPaths();
   for(size_t i = 0; i < numpaths; i++)
   {
      const char *path = D_GetDoomWadPath(i);
      qstring pathandfn = qstring(path).pathConcatenate(filename.constPtr());

      if((f = fopen(pathandfn.constPtr(), "rb")))
      {
         filename = pathandfn;
         return f;
      }
      else
      {
         // Try default extensions
         for(int j = 0; j < EXT_NUMEXTENSIONS; j++)
         {
            basefn = pathandfn;
            basefn.addDefaultExtension(W_defaultExtensions[j]);
            if((f = fopen(basefn.constPtr(), "rb")))
            {
               filename = basefn;
               return f;
            }
         }
      }
   }

   return nullptr;
}

typedef bool (*FormatFunc)(FILE *, long);

//
// W_isWadFile
//
// Detect an id IWAD or PWAD file, as used in DOOM itself and its direct spawn,
// Heretic, Hexen, and Strife, as well as in numerous other games directly,
// such as RoTT, Amulets and Armor, Bloodmasters, etc. It also formed the 
// basis of the GRP (Duke3D) and BSP (Quake, Quake 2) formats.
//
static bool W_isWadFile(FILE *f, long baseoffset)
{
   bool result = false;
   char header[12];

   if(!fseek(f, baseoffset, SEEK_SET) && fread(header, 1, 12, f) == 12)
   {
      if(!memcmp(header, "IWAD", 4) || !memcmp(header, "PWAD", 4))
         result = true;
   }
   fseek(f, baseoffset, SEEK_SET);

   return result;
}

//
// W_isZipFile
//
// Detect a ZIP archive, originally defined by the PKZip archive tool
// and now used pretty much everywhere. Forms the basis of id Tech 3 and up
// "pack" formats PK3 and PK4, as well as being the underlying format of
// many other domain-specific archives such as Java JARs.
//
static bool W_isZipFile(FILE *f, long baseoffset)
{
   bool result = false;
   char header[30];

   if(!fseek(f, baseoffset, SEEK_SET) && fread(header, 1, 30, f) == 30)
   {
      if(!memcmp(header, "PK\x3\x4", 4))
         result = true;
   }
   fseek(f, baseoffset, SEEK_SET);

   return result;
}

//
// W_isFile
//
// This one is a dummy which always returns true; if no other format could be
// positively detected first, we treat the file as an ordinary flat physical
// file. It's the lowest priority predicate as a result.
//
static bool W_isFile(FILE *f, long baseoffset)
{
   return true;
}

static FormatFunc formatFuncs[W_FORMAT_MAX] = 
{
   W_isWadFile, // W_FORMAT_WAD
   W_isZipFile, // W_FORMAT_ZIP
   W_isFile,    // W_FORMAT_FILE
   nullptr,     // W_FORMAT_DIR: not used here, guarded by W_isFile (ioanch)
};

//
// W_DetermineFileFormat
//
// Determine what format a file added with -iwad, -file, etc. is in according
// to the file's internal metadata (we do not trust file extensions for this
// purpose).
//
WResourceFmt W_DetermineFileFormat(FILE *f, long baseoffset)
{
   int fmt = W_FORMAT_WAD;

   while(!formatFuncs[fmt](f, baseoffset))
      ++fmt;

   return static_cast<WResourceFmt>(fmt);      
}

//
// W_LumpNameFromFilePath
//
// Creates a lump name given a filepath.
//
void W_LumpNameFromFilePath(const char *input, char output[9], int li_namespace)
{
   // Strip off the path, and remove any extension
   M_ExtractFileBase(input, output);

   // Change '^' to '\' for benefit of sprite frames
   if(li_namespace == lumpinfo_t::ns_sprites)
   {
      for(int i = 0; i < 8; i++)
      {
         if(output[i] == '^')
            output[i] = '\\';
      }
   }
}

struct namespace_matcher_t
{
   const char *prefix;
   int li_namespace;
};

static namespace_matcher_t matchers[] =
{
   { "acs/",          lumpinfo_t::ns_acs          },
   { "colormaps/",    lumpinfo_t::ns_colormaps    },
   { "demos/",        lumpinfo_t::ns_demos        }, // EE extension
   { "flats/",        lumpinfo_t::ns_flats        },
   //{ "graphics/",     lumpinfo_t::ns_graphics     }, FIXME - as soon as VImage is done!
   { "graphics/",     lumpinfo_t::ns_global       },
   { "hires/",        lumpinfo_t::ns_hires        }, // TODO: Implement
   { "music/",        lumpinfo_t::ns_global       }, // Treated as global in EE
   { "sounds/",       lumpinfo_t::ns_sounds       },
   { "sprites/",      lumpinfo_t::ns_sprites      },
   { "translations/", lumpinfo_t::ns_translations }, // EE extension
   { "gamepads/",     lumpinfo_t::ns_pads         }, // EE extension
   { "textures/",     lumpinfo_t::ns_textures     },

   { NULL,            -1                          }  // keep this last

   // TODO ??
   /*
   { "patches/",      lumpinfo_t::ns_patches      },
   { "voices/",       lumpinfo_t::ns_voices       },
   { "voxels/",       lumpinfo_t::ns_voxels       },
   */
};

//
// W_NamespaceForFilePath
//
// Given a relative filepath (such as from a zip), determine a lump namespace.
// Returns -1 if the path should not be exposed with a short lump name.
//
int W_NamespaceForFilePath(const char *path)
{
   int li_namespace = -1;
   namespace_matcher_t *matcher = matchers;

   while(matcher->prefix)
   {
      if(!strncmp(path, matcher->prefix, strlen(matcher->prefix)))
      {
         li_namespace = matcher->li_namespace;
         break;
      }
      ++matcher;
   }

   // if the file is in the top-level directory, it is global
   if(li_namespace == -1 && !strchr(path, '/'))
      li_namespace = lumpinfo_t::ns_global;

   return li_namespace;
}

// EOF

