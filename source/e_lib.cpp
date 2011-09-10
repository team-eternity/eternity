// Emacs style mode select   -*- C -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
//
// DESCRIPTION:  
//    Common utilities for "Extended Feature" modules.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomtype.h"
#include "d_io.h"
#include "d_dwfile.h"
#include "m_hash.h"
#include "m_misc.h"
#include "w_wad.h"
#include "psnprntf.h"
#include "d_main.h"
#include "i_system.h"
#include "d_dehtbl.h"
#include "v_video.h"

#include "Confuse/confuse.h"
#include "Confuse/lexer.h"

#include <errno.h>

#include "e_lib.h"
#include "e_edf.h"

//=============================================================================
//
// Basic Functionality
//

//
// Default Error Callback
//
void E_ErrorCB(cfg_t *cfg, const char *fmt, va_list ap)
{
   I_ErrorVA(fmt, ap);
}

//=============================================================================
//
// Include Tracking
//
// haleyjd 3/17/10: Under the new architecture for single-pass parsing and
// processing of all EDF data sources, it becomes necessary to make sure that
// no given data source is parsed/processed more than once. To that end, we 
// calculate the SHA-1 checksum of each EDF data source and record it here.
//

static hashdata_t *eincludes;
static int numincludes;
static int numincludesalloc;

//
// E_CheckInclude
//
// Pass a pointer to some cached data and the size of that data. The SHA-1
// hash will be calculated and compared against the SHA-1 hashes of all other
// data sources that have been sent into this function. Returns true if the
// data should be included, and false otherwise (ie. there was a match).
//
static boolean E_CheckInclude(const char *data, size_t size)
{
   int i;
   hashdata_t newhash;
   char *digest;

   // calculate the SHA-1 hash of the data
   M_HashInitialize(&newhash, HASH_SHA1);
   M_HashData(&newhash, (const uint8_t *)data, (uint32_t)size);
   M_HashWrapUp(&newhash);

   // output digest string
   digest = M_HashDigestToStr(&newhash);

   E_EDFLogPrintf("\t\t  SHA-1 = %s\n", digest);

   free(digest);

   // compare against existing includes
   for(i = 0; i < numincludes; ++i)
   {
      // found a match?
      if(M_HashCompare(&newhash, &eincludes[i]))
      {
         E_EDFLogPuts("\t\t\tDeclined, SHA-1 match detected.\n");
         return false;
      }
   }

   // this source has not been processed before, so add its hash to the list
   if(numincludes == numincludesalloc)
   {
      numincludesalloc = numincludesalloc ? 2 * numincludesalloc : 8;
      eincludes = (hashdata_t *)(realloc(eincludes, numincludesalloc * sizeof(*eincludes)));
   }
   eincludes[numincludes++] = newhash;

   return true;
}

//
// E_OpenAndCheckInclude
//
// Performs the following actions:
// 1. Caches the indicated file or lump (lump only if lumpnum >= 0).
//    If the file cannot be opened, the libConfuse error code 1 is
//    returned.
// 2. Calls E_CheckInclude on the cached data.
// 3. If there is a hash collision, the data is freed. The libConfuse 
//    "ok" code 0 will be returned.
// 4. If there is not a hash collision, the data is included through a
//    call to cfg_lexer_include. The return value of that function will
//    be propagated.
//
static int E_OpenAndCheckInclude(cfg_t *cfg, const char *fn, int lumpnum)
{
   size_t len;
   char *data;
   int code = 1;

   E_EDFLogPrintf("\t\t* Including %s\n", fn);

   // must open the data source
   if((data = cfg_lexer_mustopen(cfg, fn, lumpnum, &len)))
   {
      // see if we already parsed this data source
      if(E_CheckInclude(data, len))
         code = cfg_lexer_include(cfg, data, fn, lumpnum);
      else
      {
         // we already parsed it, but this is not an error;
         // we ignore the include and return 0 to indicate success.
         free(data);
         code = 0;
      }
   }

   return code;
}

//
// E_FindLumpInclude
//
// Finds a lump from the same data source as the including lump.
// Returns -1 if no such lump can be found.
// 
static int E_FindLumpInclude(cfg_t *src, const char *name)
{
   lumpinfo_t *lump, *inclump;
   int includinglumpnum;
   int i;

   // this is not for files
   if((includinglumpnum = cfg_lexer_source_type(src)) < 0)
      return -1;

   // get a pointer to the including lump's lumpinfo
   inclump = w_GlobalDir.lumpinfo[includinglumpnum];

   // get a pointer to the hash chain for this lump name
   lump = W_GetLumpNameChain(name);

   // walk down the hash chain
   for(i = lump->index; i >= 0; i = lump->next)
   {
      lump = w_GlobalDir.lumpinfo[i];

      if(!strncasecmp(lump->name, name, 8) &&           // name matches specified
         lump->li_namespace == lumpinfo_t::ns_global && // is in global namespace
         lump->source == inclump->source)               // is from same source
      {
         return i; // haleyjd 07/26/10: i, not lump->index!!!
      }
   }

   return -1; // not found
}

//
// E_CheckRoot
//
// haleyjd 03/21/10: Checks a root data source to see if it has already been
// processed. This is installed as a lexer file open callback in the cfg_t.
// The convention is to return 0 if the file should be parsed.
//
int E_CheckRoot(cfg_t *cfg, const char *data, int size)
{
   return !E_CheckInclude(data, (size_t)size);
}

//=============================================================================
//
// Parser File/Lump Include Callback Functions
//

//
// E_Include
//
// The normal include function. cfg_include is insufficient since it 
// looks in the current working directory unless provided a full path.
// This function interprets paths relative to the  current file when 
// called from a physical input file, and uses the argument as a lump 
// name otherwise.
//
int E_Include(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   char  *currentpath = NULL;
   char  *filename    = NULL;
   size_t len         =  0;
   int    lumpnum     = -1;

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to include()\n");
      return 1;
   }
   if(!cfg->filename)
   {
      cfg_error(cfg, "include: cfg_t filename is undefined\n");
      return 1;
   }

   // 02/09/05: support both files and lumps in this function, but
   // only one or the other depending on the calling context
   switch(cfg_lexer_source_type(cfg))
   {
   case -1: // physical file
      len = M_StringAlloca(&currentpath, 1, 2, cfg->filename);
      M_GetFilePath(cfg->filename, currentpath, len);
      len = M_StringAlloca(&filename, 2, 2, currentpath, argv[0]);
      psnprintf(filename, len, "%s/%s", currentpath, argv[0]);
      M_NormalizeSlashes(filename);
      
      return E_OpenAndCheckInclude(cfg, filename, -1);
   
   default: // data source
      if(strlen(argv[0]) > 8)
      {
         cfg_error(cfg, "include: %s is not a valid lump name\n", argv[0]);
         return 1;
      }

      // haleyjd 03/19/10:
      // find a lump of the requested name in the same data source only
      if((lumpnum = E_FindLumpInclude(cfg, argv[0])) < 0)
      {
         cfg_error(cfg, "include: %s not found\n", argv[0]);
         return 1;
      }

      return E_OpenAndCheckInclude(cfg, argv[0], lumpnum);
   }
}

//
// E_LumpInclude
//
// Includes a WAD lump. Useful if you need to include a lump from a 
// file, since include() cannot do that.
//
int E_LumpInclude(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   int lumpnum = -1;

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to lumpinclude()\n");
      return 1;
   }
   if(strlen(argv[0]) > 8)
   {
      cfg_error(cfg, "lumpinclude: %s is not a valid lump name\n", argv[0]);
      return 1;
   }

   switch(cfg_lexer_source_type(cfg))
   {
   case -1: // from a file - include the newest lump
      return E_OpenAndCheckInclude(cfg, argv[0], W_GetNumForName(argv[0]));
   default: // lump
      if((lumpnum = E_FindLumpInclude(cfg, argv[0])) < 0)
      {
         cfg_error(cfg, "lumpinclude: %s not found\n", argv[0]);
         return 1;
      }
      return E_OpenAndCheckInclude(cfg, argv[0], lumpnum);
   }
}

//
// E_IncludePrev
//
// Includes the next WAD lump on the lumpinfo hash chain of the same 
// name as the current lump being processed (to the user, this is
// the "previous" lump of that name). Enables recursive inclusion
// of like-named lumps to enable cascading behavior.
//
int E_IncludePrev(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   int i;

   // haleyjd 03/18/10: deprecation warning
   E_EDFLoggedWarning(0, "Warning: include_prev is deprecated\n");

   if(argc != 0)
   {
      cfg_error(cfg, "wrong number of args to include_prev()\n");
      return 1;
   }
   if(!cfg->filename)
   {
      cfg_error(cfg, "include_prev: cfg_t filename is undefined\n");
      return 1;
   }
   if((i = cfg_lexer_source_type(cfg)) < 0)
   {
      cfg_error(cfg, "include_prev: cannot call from file\n");
      return 1;
   }

   // Go down the hash chain and look for the next lump of the same
   // name within the global namespace.
   while((i = w_GlobalDir.lumpinfo[i]->next) >= 0)
   {
      if(w_GlobalDir.lumpinfo[i]->li_namespace == lumpinfo_t::ns_global &&
         !strncasecmp(w_GlobalDir.lumpinfo[i]->name, cfg->filename, 8))
      {
         return E_OpenAndCheckInclude(cfg, cfg->filename, i);
      }
   }

   // it is not an error if no such lump is found
   return 0;
}

//
// E_BuildDefaultFn
//
// Constructs the absolute file name for a default file. Don't cache 
// the returned pointer, since it points to a static buffer.
//
const char *E_BuildDefaultFn(const char *filename)
{
   char *buffer = NULL;
   size_t len = 0;

   len = M_StringAlloca(&buffer, 2, 2, basepath, filename);

   psnprintf(buffer, len, "%s/%s", basepath, filename);
   M_NormalizeSlashes(buffer);

   return buffer;
}

//
// E_StdInclude
//
// An include function that looks for files in the EXE's directory, as 
// opposed to the current directory.
//
int E_StdInclude(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   const char *filename;

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to stdinclude()\n");
      return 1;
   }

   // haleyjd 03/15/2010: Using stdinclude on anything other than root.edf is
   // now considered deprecated because of problems it creates with forward
   // compatibility of EDF mods when new EDF modules are added.
   if(!strstr(argv[0], "root.edf"))
   {
      E_EDFLoggedWarning(0, "Warning: stdinclude() is deprecated except for "
                            "the inclusion of file 'root.edf'.\n");
   }

   filename = E_BuildDefaultFn(argv[0]);

   return E_OpenAndCheckInclude(cfg, filename, -1);
}

//
// E_UserInclude
//
// Like stdinclude, but checks to see if the file exists before parsing it.
// If it doesn't exist, it's not an error.
// haleyjd 03/14/06
//
int E_UserInclude(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   const char *filename;

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to userinclude()\n");
      return 1;
   }

   filename = E_BuildDefaultFn(argv[0]);

   return !access(filename, R_OK) ? E_OpenAndCheckInclude(cfg, filename, -1) : 0;
}

//=============================================================================
//
// Enables
//

//
// E_EnableNumForName
//
// Gets the index of an enable value. Linear search on a small fixed 
// set. Note: Enable sets must be terminated with an all-zeroes entry.
//
int E_EnableNumForName(const char *name, E_Enable_t *enables)
{
   int i = 0;

   while(enables[i].name)
   {
      if(!strcasecmp(enables[i].name, name))
         return i;

      ++i;
   }

   return -1;
}

//
// E_Endif
//
// 01/14/04: Returns the parser to normal after a conditional function.
//
int E_Endif(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   cfg->flags &= ~CFGF_LOOKFORFUNC;
   cfg->lookfor = NULL;

   return 0;
}

//=============================================================================
//
// Resolution Functions
//

//
// E_StrToNumLinear
//
// Looks through an array of strings until a case-insensitive match
// is found, and then returns the index the match is found at. If no
// match is found, the array size is returned.
//
int E_StrToNumLinear(const char **strings, int numstrings, const char *value)
{
   int index = 0;

   while(index != numstrings && strcasecmp(strings[index], value))
      ++index;

   return index;
}

//
// E_ParseFlags
//
// Parses a single-word options/flags field.
//
int E_ParseFlags(const char *str, dehflagset_t *flagset)
{
   char *buffer, *bufptr;

   bufptr = buffer = strdup(str);

   deh_ParseFlags(flagset, &bufptr);

   free(buffer);

   return flagset->results[0];
}

//
// Value-Parsing Callbacks
//

//
// E_SpriteFrameCB
//
// libConfuse value-parsing callback function for sprite frame values. 
// Allows use of characters A through ], corresponding to the actual 
// sprite lump names (implemented by popular demand ;)
//
// This function is also called explicitly by E_ProcessCmpState.
// When this is done, the cfg and opt parameters are set to NULL,
// and will not be used.
//
int E_SpriteFrameCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, 
                    void *result)
{
   if(strlen(value) == 1 && value[0] >= 'A' && value[0] <= ']')
   {
      *(int *)result = value[0] - 'A';
   }
   else
   {
      char *endptr;

      *(int *)result = strtol(value, &endptr, 0);
      
      if(*endptr != '\0')
      {
         if(cfg)
         {
            cfg_error(cfg, "invalid integer value for option '%s'\n",
                      opt->name);
         }
         return -1;
      }
      if(errno == ERANGE) 
      {
         if(cfg)
         {
            cfg_error(cfg,
               "integer value for option '%s' is out of range\n",
               opt->name);
         }
         return -1;
      }
   }

   return 0;
}

//
// E_IntOrFixedCB
//
// libConfuse value-parsing callback for a thing speed field.
// Allows input of either integer or floating-point values, where
// the latter are converted to fixed-point for storage.
//
int E_IntOrFixedCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, 
                   void *result)
{
   char *endptr;
   const char *dotloc;

   // test for a decimal point
   dotloc = strchr(value, '.');

   if(dotloc)
   {
      // process a float and convert to fixed-point
      double tmp;

      tmp = strtod(value, &endptr);

      if(*endptr != '\0')
      {
         if(cfg)
         {
            cfg_error(cfg, "invalid floating point value for option '%s'\n",
                      opt->name);
         }
         return -1;
      }
      if(errno == ERANGE) 
      {
         if(cfg)
         {
            cfg_error(cfg,
               "floating point value for option '%s' is out of range\n",
               opt->name);
         }
         return -1;
      }

      *(int *)result = (int)(tmp * FRACUNIT);
   }
   else
   {
      // process an integer
      *(int *)result = (int)strtol(value, &endptr, 0);
      
      if(*endptr != '\0')
      {
         if(cfg)
         {
            cfg_error(cfg, "invalid integer value for option '%s'\n",
                      opt->name);
         }
         return -1;
      }
      if(errno == ERANGE) 
      {
         if(cfg)
         {
            cfg_error(cfg,
               "integer value for option '%s' is out of range\n",
               opt->name);
         }
         return -1;
      }
   }

   return 0;
}

//
// E_TranslucCB
//
// libConfuse value-parsing callback for translucency fields. Can accept 
// an integer value or a percentage.
//
int E_TranslucCB(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                 void *result)
{
   const char *endptr, *pctloc;

   // test for a percent sign (start looking at end)
   pctloc = strrchr(value, '%');

   if(pctloc)
   {
      int pctvalue;
      
      // get the percentage value (base 10 only)
      pctvalue = strtol(value, &endptr, 10);

      // strtol should stop at the percentage sign
      if(endptr != pctloc)
      {
         if(cfg)
         {
            cfg_error(cfg, "invalid percentage value for option '%s'\n",
                      opt->name);
         }
         return -1;
      }
      if(errno == ERANGE || pctvalue < 0 || pctvalue > 100) 
      {
         if(cfg)
         {
            cfg_error(cfg,
               "percentage value for option '%s' is out of range\n",
               opt->name);
         }
         return -1;
      }

      *(int *)result = (FRACUNIT * pctvalue) / 100;
   }
   else
   {
      // process an integer
      *(int *)result = (int)strtol(value, &endptr, 0);
      
      if(*endptr != '\0')
      {
         if(cfg)
            cfg_error(cfg, "invalid integer value for option '%s'\n", opt->name);
         return -1;
      }
      if(errno == ERANGE) 
      {
         if(cfg)
         {
            cfg_error(cfg,
                      "integer value for option '%s' is out of range\n",
                      opt->name);
         }
         return -1;
      }
   }

   return 0;
}

//
// E_ColorStrCB
//
// Accepts either a palette index or an RGB triplet, which will be
// matched to the closest color in the game palette.
//
int E_ColorStrCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, 
                 void *result)
{
   char *endptr;
   
   *(int *)result = (int)strtol(value, &endptr, 0);

   if(*endptr != '\0')
   {
      byte *palette;
      int r, g, b;

      if(sscanf(value, "%d %d %d", &r, &g, &b) != 3)
      {
         if(cfg)
         {
            cfg_error(cfg,
               "invalid color triplet for option '%s'\n",
               opt->name);
         }
         return -1;
      }

      palette = (byte *)(W_CacheLumpName("PLAYPAL", PU_STATIC));

      *(int *)result = V_FindBestColor(palette, r, g, b);

      Z_ChangeTag(palette, PU_CACHE);
   }
   else if(errno == ERANGE) 
   {
      if(cfg)
      {
         cfg_error(cfg,
            "integer value for option '%s' is out of range\n",
            opt->name);
      }
      return -1;
   }

   return 0;
}

//
// Prefix:Value Syntax Parsing
//

//
// E_ExtractPrefix
//
// Returns the result of strchr called on the string in value.
// If the return is non-NULL, you can expect to find the extracted
// prefix written into prefixbuf. If the return value is NULL,
// prefixbuf is unmodified.
//
char *E_ExtractPrefix(char *value, char *prefixbuf, int buflen)
{
   int i;
   char *colonloc, *rover, *strval;

   // look for a colon ending a possible prefix
   colonloc = strchr(value, ':');

   if(colonloc)
   {
      strval = colonloc + 1;
      rover = value;
      i = 0;
      
      // 01/10/09: initialize buffer
      memset(prefixbuf, 0, buflen);

      while(rover != colonloc && i < buflen - 1) // leave room for \0
      {
         prefixbuf[i] = *rover;
         ++rover;
         ++i;
      }
      
      // check validity of the string value location (could be end)
      if(!(*strval))
      {
         E_EDFLoggedErr(0,
            "E_ExtractPrefix: invalid prefix:value %s\n", value);
      }
   }

   return colonloc;
}

// EOF


