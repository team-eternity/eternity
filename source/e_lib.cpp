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

#include "z_zone.h"
#include "i_system.h"

#include "Confuse/confuse.h"
#include "Confuse/lexer.h"

#include "e_lib.h"
#include "e_edf.h"

#include "autopalette.h"
#include "d_dehtbl.h"
#include "d_dwfile.h"
#include "d_io.h"
#include "d_main.h"
#include "doomstat.h"
#include "doomtype.h"
#include "m_collection.h"
#include "m_compare.h"
#include "m_hash.h"
#include "m_qstr.h"
#include "m_utils.h"
#include "metaapi.h"
#include "psnprntf.h"
#include "v_video.h"
#include "w_wad.h"

// need wad iterators
#include "w_iterator.h"

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

static Collection<HashData> eincludes;

//
// E_CheckInclude
//
// Pass a pointer to some cached data and the size of that data. The SHA-1
// hash will be calculated and compared against the SHA-1 hashes of all other
// data sources that have been sent into this function. Returns true if the
// data should be included, and false otherwise (ie. there was a match).
//
bool E_CheckInclude(const char *data, size_t size)
{
   size_t numincludes;
   char *digest;
   
   // calculate the SHA-1 hash of the data   
   HashData newHash(HashData::SHA1, (const uint8_t *)data, (uint32_t)size);

   // output digest string
   digest = newHash.digestToString();

   E_EDFLogPrintf("\t\t  SHA-1 = %s\n", digest);

   efree(digest);

   numincludes = eincludes.getLength();

   // compare against existing includes
   for(size_t i = 0; i < numincludes; i++)
   {
      // found a match?
      if(newHash == eincludes[i])
      {
         E_EDFLogPuts("\t\t\tDeclined, SHA-1 match detected.\n");
         return false;
      }
   }

   // this source has not been processed before, so add its hash to the list
   eincludes.add(newHash);

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
         efree(data);
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
   lumpinfo_t  *inclump;
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();
   int includinglumpnum;

   // this is not for files
   if((includinglumpnum = cfg_lexer_source_type(src)) < 0)
      return -1;

   // get a pointer to the including lump's lumpinfo
   inclump = lumpinfo[includinglumpnum];

   WadChainIterator wci(wGlobalDir, name);

   // walk down the hash chain
   for(wci.begin(); wci.current(); wci.next())
   {
      if(wci.testLump(lumpinfo_t::ns_global) && // name matches, is global
         (*wci)->source == inclump->source)     // is from same source
      {
         return (*wci)->selfindex;
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

// External names for dialects
static const char *dialectNames[CFG_NUMDIALECTS] =
{
   "DELTA",  // Original dialect
   "ALFHEIM" // Adds ':' as an alternate assignment operator (like CSS and JSON)
};

//
// E_SetDialect
//
// Changes the parser dialect. This setting applies upward on the include
// stack, so including a file that does not explicitly set its own dialect will
// cause it to be treated as the same dialect as the including file.
//
int E_SetDialect(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   int dialect;

   if(argc != 1)
   {
      cfg_error(cfg, "wrong number of args to setdialect()\n");
      return 1;
   }

   dialect = E_StrToNumLinear(dialectNames, CFG_NUMDIALECTS, argv[0]);
   if(dialect == CFG_NUMDIALECTS)
   {
      cfg_error(cfg, "invalid libConfuse dialect %s\n", argv[0]);
      return 1;
   }

   cfg_lexer_set_dialect((cfg_dialect_t)dialect);
   return 0;
}

//
// E_Include
//
// The normal include function. cfg_include is insufficient since it 
// looks in the current working directory unless provided a full path.
// This function interprets paths relative to the current file when 
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
      
      filename = M_SafeFilePath(currentpath, argv[0]);
      
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
   lumpinfo_t **lumpinfo = wGlobalDir.getLumpInfo();

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
   while((i = lumpinfo[i]->namehash.next) >= 0)
   {
      if(lumpinfo[i]->li_namespace == lumpinfo_t::ns_global &&
         !strncasecmp(lumpinfo[i]->name, cfg->filename, 8))
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
   return M_SafeFilePath(basepath, filename);
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
unsigned int E_ParseFlags(const char *str, dehflagset_t *flagset)
{
   char *buffer, *bufptr;

   bufptr = buffer = estrdup(str);

   deh_ParseFlags(flagset, &bufptr);

   efree(buffer);

   return flagset->results[0];
}

//=============================================================================
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

      *(int *)result = static_cast<int>(strtol(value, &endptr, 0));
      
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
   char *endptr;
   const char *pctloc;

   // test for a percent sign (start looking at end)
   pctloc = strrchr(value, '%');

   if(pctloc)
   {
      int pctvalue;
      
      // get the percentage value (base 10 only)
      pctvalue = static_cast<int>(strtol(value, &endptr, 10));

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
// E_TranslucCB2
//
// libConfuse value-parsing callback for translucency fields. Can accept 
// an integer value or a percentage. This one expects an integer from 0
// to 255 when percentages are not used, and does not convert to fixed point.
//
int E_TranslucCB2(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                  void *result)
{
   char *endptr;
   const char *pctloc;

   // test for a percent sign (start looking at end)
   pctloc = strrchr(value, '%');

   if(pctloc)
   {
      int pctvalue;
      
      // get the percentage value (base 10 only)
      pctvalue = static_cast<int>(strtol(value, &endptr, 10));

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

      *(int *)result = (255 * pctvalue) / 100;
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
      AutoPalette pal(wGlobalDir);
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

      *(int *)result = V_FindBestColor(pal.get(), r, g, b);
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

//=============================================================================
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
const char *E_ExtractPrefix(const char *value, char *prefixbuf, int buflen)
{
   const char *colonloc;

   // look for a colon ending a possible prefix
   colonloc = strchr(value, ':');

   if(colonloc)
   {
      const char *strval = colonloc + 1;
      const char *rover = value;
      int i = 0;
      
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

//=============================================================================
//
// Other String Utilities
//

//
// E_ReplaceString
//
// haleyjd 12/31/11: Free a string if it's non-null and then give it the 
// new value.
//
void E_ReplaceString(char *&dest, char *newvalue)
{
   if(dest)
      efree(dest);

   dest = newvalue;
}

//
// E_MetaStringFromCfgString
//
// Utility function.
// Adds a MetaString property to the passed-in table with the same name and
// value as the cfg_t string property.
//
void E_MetaStringFromCfgString(MetaTable *meta, cfg_t *cfg, const char *prop, int n, bool list)
{
   if(list)
      meta->addString(prop, cfg_getnstr(cfg, prop, n));
   else
      meta->setString(prop, cfg_getstr(cfg, prop));
}

//
// E_MetaIntFromCfgInt
//
// Utility function.
// Adds a MetaInteger property to the passed-in table with the same name
// and value as the cfg_t integer property.
//
void E_MetaIntFromCfgInt(MetaTable *meta, cfg_t *cfg, const char *prop, int n, bool list)
{
   if(list && meta->getObjectKeyAndTypeEx<MetaInteger>(prop))
      meta->addInt(prop, cfg_getnint(cfg, prop, n));
   else
      meta->setInt(prop, cfg_getint(cfg, prop));
}

//
// Get MetaFloat from cfg float
//
static void E_metaDoubleFromCfgFloat(MetaTable *meta, cfg_t *cfg,
                                     const char *prop, int n, bool list)
{
   if(list && meta->getObjectKeyAndTypeEx<MetaDouble>(prop))
      meta->addDouble(prop, cfg_getnfloat(cfg, prop, n));
   else
      meta->setDouble(prop, cfg_getfloat(cfg, prop));
}

//
// E_MetaIntFromCfgBool
//
// Utility function.
// Adds a MetaInteger property to the passed-in table with the same name
// and value as the cfg_t bool property.
//
void E_MetaIntFromCfgBool(MetaTable *meta, cfg_t *cfg, const char *prop, int n, bool list)
{
   if(list && meta->getObjectKeyAndTypeEx<MetaInteger>(prop))
      meta->addInt(prop, cfg_getnbool(cfg, prop, n));
   else
      meta->setInt(prop, cfg_getbool(cfg, prop));
}

//
// E_MetaIntFromCfgFlag
//
// Utility function.
// Adds a MetaInteger property to the passed-in table with the same name
// and value as the cfg_t flag property.
//
void E_MetaIntFromCfgFlag(MetaTable *meta, cfg_t *cfg, const char *prop, int n, bool list)
{
   if(list && meta->getObjectKeyAndTypeEx<MetaInteger>(prop))
      meta->addInt(prop, cfg_getnflag(cfg, prop, n));
   else
      meta->setInt(prop, cfg_getflag(cfg, prop));
}

void E_MetaTableFromCfgMvprop(MetaTable *meta, cfg_t *cfg, const char *prop, bool allowmulti)
{
   int numprop = cfg_size(cfg, prop);

   for(int i = 0; i < numprop; i++)
   {
      cfg_t *currcfg = cfg_getnmvprop(cfg, prop, i);

      MetaTable *table = new MetaTable(prop);

      for(auto opt = currcfg->opts; opt->type != CFGT_NONE; opt++)
      {
         int n = cfg_size(currcfg, opt->name);
         if(n == 0)
            continue;
         bool list = opt->flags & CFGF_LIST;
         if(!list)
            n = 1;

         for(int j = 0; j < n; j++)
         {
            switch(opt->type)
            {
            case CFGT_INT:
               E_MetaIntFromCfgInt(table, currcfg, opt->name, j, list);
               break;
            case CFGT_STR:
               E_MetaStringFromCfgString(table, currcfg, opt->name, j, list);
               break;
            case CFGT_BOOL:
               E_MetaIntFromCfgBool(table, currcfg, opt->name, j, list);
               break;
            case CFGT_FLAG:
               E_MetaIntFromCfgFlag(table, currcfg, opt->name, j, list);
               break;
            case CFGT_MVPROP:
               E_MetaTableFromCfgMvprop(table, currcfg, opt->name, opt->flags & CFGF_MULTI);
               break;
            default:
               break;
            }
         }
      }

      if(allowmulti)
         meta->addMetaTable(prop, table);
      else
         meta->setMetaTable(prop, table);
   }

}

//
// E_MetaTableFromCfg
//
// haleyjd 06/30/13: Convert a cfg_t to fields in a MetaTable, with optional
// inheritance from a prototype table.
//
void E_MetaTableFromCfg(cfg_t *cfg, MetaTable *table, MetaTable *prototype)
{
   table->clearTable();

   if(prototype)
   {
      table->copyTableFrom(prototype);
      table->setString("super", prototype->getKey());
   }

   for(auto opt = cfg->opts; opt->type != CFGT_NONE; opt++)
   {
      int n = cfg_size(cfg, opt->name);
      if(n == 0)
         continue;
      bool list = opt->flags & CFGF_LIST;
      if(!list)
         n = 1;

      for(int i = n; i --> 0;)
      {
         switch(opt->type)
         {
         case CFGT_INT:
            E_MetaIntFromCfgInt(table, cfg, opt->name, i, list);
            break;
         case CFGT_FLOAT:
            E_metaDoubleFromCfgFloat(table, cfg, opt->name, i, list);
            break;
         case CFGT_STR:
            E_MetaStringFromCfgString(table, cfg, opt->name, i, list);
            break;
         case CFGT_BOOL:
            E_MetaIntFromCfgBool(table, cfg, opt->name, i, list);
            break;
         case CFGT_FLAG:
            E_MetaIntFromCfgFlag(table, cfg, opt->name, i, list);
            break;
         case CFGT_STRFUNC:
            E_MetaStringFromCfgString(table, cfg, opt->name, i, list);
            break;
         case CFGT_MVPROP:
            E_MetaTableFromCfgMvprop(table, cfg, opt->name, opt->flags & CFGF_MULTI);
            break;
         default:
            break;
         }
      }
   }
}

//
// Sets flags by looking for + and - prefixed values
//
void E_SetFlagsFromPrefixCfg(cfg_t *cfg, unsigned &flags, const dehflags_t *set)
{
   for(auto opt = cfg->opts; opt->type != CFGT_NONE; opt++)
   {
      if(!cfg_size(cfg, opt->name) || opt->type != CFGT_FLAG)
         continue;
      // Look for the name in the set
      for(const dehflags_t *item = set; item->name; item++)
      {
         if(strcasecmp(opt->name, item->name))
            continue;
         int v = cfg_getflag(cfg, opt->name);
         if(v)
            flags |= item->value;
         else
            flags &= ~item->value;
         break;
      }
   }
}

//
// E_GetHeredocLine
//
// Finds the start of the next line in the string, and modifies the string with
// a \0 to terminate the current line. Returns the start of the current line, or
// NULL if input is exhausted. Based loosely on E_GetDSLine from the DECORATE 
// state parser.
//
char *E_GetHeredocLine(char **src)
{
   char *srctxt    = *src;
   char *linestart = srctxt;

   if(!*srctxt) // at end?
      linestart = NULL;
   else
   {
      char c;

      while((c = *srctxt))
      {
         if(c == '\n')
         {
            // modify with a \0 to terminate the line, and step to the next char
            *srctxt = '\0';
            ++srctxt;
            break;
         }
         ++srctxt;
      }

      // parse out spaces at the start of the line
      while(ectype::isSpace(*linestart))
         ++linestart;
   }

   *src = srctxt;

   return linestart;
}

//============================================================================
//
// Basic simple text line tokenizer - abstracted out of SndSeq parser
//

// states for command mini-parser below.
enum
{
   STATE_LOOKFORCMD,
   STATE_INCMD,
};

//
// E_ParseTextLine
//
// Tokenizes a script command string. Basically, breaks it up into
// 0 to E_MAXCMDTOKENS tokens. Anything beyond the max token count is totally 
// ignored.
//
tempcmd_t E_ParseTextLine(char *str)
{
   tempcmd_t retcmd;
   char *tokenstart = NULL;
   int state = STATE_LOOKFORCMD, strnum = 0;

   memset(&retcmd, 0, sizeof(retcmd));

   while(1)
   {
      switch(state)
      {
      case STATE_LOOKFORCMD: // looking for a command -- skip whitespace
         switch(*str)
         {
         case '\0': // end of string -- done
            return retcmd;
         case ' ':
         case '\t':
         case '\r':
            break;
         default:
            tokenstart = str;
            state = STATE_INCMD;
            break;
         }
         break;
      case STATE_INCMD: // inside a command -- whitespace ends
         switch(*str)
         {
         case '\0': // end of string
         case ' ':
         case '\t':
         case '\r':
            retcmd.strs[strnum] = tokenstart;
            ++strnum;
            if(*str == '\0' || strnum >= E_MAXCMDTOKENS) // are we done?
               return retcmd;
            tokenstart = NULL;
            state = STATE_LOOKFORCMD;
            *str = '\0'; // modify string to terminate tokens
            break;
         default:
            break;
         }
         break;
      }

      ++str;
   }

   return retcmd; // should be unreachable
}

//=============================================================================
//
// Translation Parsing
//
// haleyjd 02/02/11: Support ZDoom-format translation strings (to a point)
//

// Translation token types
enum
{
   TR_TOKEN_NUM,
   TR_TOKEN_COLON,
   TR_TOKEN_EQUALS,
   TR_TOKEN_COMMA,
   TR_TOKEN_END,
   TR_TOKEN_ERROR
};

// Parser States:
enum
{
   TR_PSTATE_SRCBEGIN,
   TR_PSTATE_COLON,
   TR_PSTATE_SRCEND,
   TR_PSTATE_EQUALS,
   TR_PSTATE_DSTBEGIN,
   TR_PSTATE_DSTEND,
   TR_PSTATE_COMMAOREND,
   TR_PSTATE_NUMSTATES
};

struct tr_range_t
{
   int srcbegin;
   int srcend;
   int dstbegin;
   int dstend;

   tr_range_t *next;
};

struct tr_pstate_t
{
   int state;
   int prevstate;
   qstring *token;
   const char *input;
   int inputpos;
   bool error;
   bool done;
   bool singlecolor;
   tr_range_t *ranges;
   
   // data
   int srcbegin;
   int srcend;
   int dstbegin;
   int dstend;
};

//
// E_GetTranslationToken
//
// Get the next token from the translation string
//
static int E_GetTranslationToken(tr_pstate_t *pstate)
{
   const char *str = pstate->input;
   int strpos = pstate->inputpos;

   pstate->token->clear();

   // skip whitespace
   while(str[strpos] == ' ' || str[strpos] == '\t')
      ++strpos;

   // On a number?
   if(ectype::isDigit(str[strpos]))
   {
      while(ectype::isDigit(str[strpos]))
      {
         *pstate->token += str[strpos];
         ++strpos;
      }
      pstate->inputpos = strpos;
      return TR_TOKEN_NUM;
   }
   else
   {
      // Misc character
      switch(str[strpos])
      {
      case ':':
         *pstate->token += str[strpos];
         ++pstate->inputpos;
         return TR_TOKEN_COLON;
      case '=':
         *pstate->token += str[strpos];
         ++pstate->inputpos;
         return TR_TOKEN_EQUALS;
      case ',':
         *pstate->token += str[strpos];
         ++pstate->inputpos;
         return TR_TOKEN_COMMA;
      case '\0':
         return TR_TOKEN_END;
      default:
         return TR_TOKEN_ERROR; // woops?
      }
   }
}

//#define COLOR_CLAMP(c) ((c) > 255 ? 255 : ((c) < 0 ? 0 : (c)))

//
// PushRange
//
// Pushes the current palette translation range into the parser state object's
// list of ranges.
//
static void PushRange(tr_pstate_t *pstate)
{
   tr_range_t *newrange = estructalloc(tr_range_t, 1);

   newrange->srcbegin = eclamp(pstate->srcbegin, 0, 255);
   newrange->srcend   = eclamp(pstate->srcend,   0, 255);
   newrange->dstbegin = eclamp(pstate->dstbegin, 0, 255);
   newrange->dstend   = eclamp(pstate->dstend,   0, 255);

   // normalize ranges
   if(newrange->srcbegin > newrange->srcend)
   {
      int temp = newrange->srcbegin;
      newrange->srcbegin = newrange->srcend;
      newrange->srcend   = temp;
   }

   if(newrange->dstbegin > newrange->dstend)
   {
      int temp = newrange->dstbegin;
      newrange->dstbegin = newrange->dstend;
      newrange->dstend   = temp;
   }

   newrange->next = pstate->ranges;
   pstate->ranges = newrange;
};

//
// DoPStateSrcBegin
//
// Expecting the beginning number of a source range
//
static void DoPStateSrcBegin(tr_pstate_t *pstate)
{
   if(E_GetTranslationToken(pstate) == TR_TOKEN_NUM)
   {
      pstate->srcbegin = pstate->token->toInt();
      pstate->prevstate = pstate->state;
      pstate->state = TR_PSTATE_COLON;
   }
   else
      pstate->error = true;
}

//
// DoPStateColon
//
// Expecting a colon between range ends.
// An =, comma, or end-of-string may also be acceptable when coming here
// from different states, as they would then indicate a single-color range.
//
static void DoPStateColon(tr_pstate_t *pstate)
{
   int tokentype = E_GetTranslationToken(pstate);

   if(tokentype == TR_TOKEN_COLON)
   {
      switch(pstate->prevstate)
      {
      case TR_PSTATE_SRCBEGIN:
         pstate->state = TR_PSTATE_SRCEND;
         break;
      case TR_PSTATE_DSTBEGIN:
         pstate->state = TR_PSTATE_DSTEND;
         break;
      default:
         pstate->error = true;
      }
   }
   else if(tokentype == TR_TOKEN_EQUALS && pstate->prevstate == TR_PSTATE_SRCBEGIN)
   {
      // An equals sign here forwards processing to the destination end state
      pstate->srcend = pstate->srcbegin; // single color, begin = end
      pstate->state = TR_PSTATE_DSTEND;
      pstate->singlecolor = true;        // remember to duplicate end of range too...
   }
   else if((tokentype == TR_TOKEN_COMMA || tokentype == TR_TOKEN_END) &&
           pstate->prevstate == TR_PSTATE_DSTBEGIN)
   {
      // , or end-of-string here means the destination range is a single color;
      // duplicate the color, back up one character, and go to the end state.
      pstate->dstend = pstate->dstbegin;
      if(pstate->inputpos > 0 && tokentype != TR_TOKEN_END)
         --pstate->inputpos;
      pstate->state = TR_PSTATE_COMMAOREND;
   }
   else
      pstate->error = true;
}

//
// DoPStateSrcEnd
//
// Expecting the end number of a source range
//
static void DoPStateSrcEnd(tr_pstate_t *pstate)
{
   if(E_GetTranslationToken(pstate) == TR_TOKEN_NUM)
   {
      pstate->srcend = pstate->token->toInt();
      pstate->state = TR_PSTATE_EQUALS;
   }
   else
      pstate->error = true;
}

//
// DoPStateEquals
//
// Expecting an = between src and dest ranges
//
static void DoPStateEquals(tr_pstate_t *pstate)
{
   if(E_GetTranslationToken(pstate) == TR_TOKEN_EQUALS)
      pstate->state = TR_PSTATE_DSTBEGIN;
   else
      pstate->error = true;
}

//
// DoPStateDestBegin
//
// Expecting the beginning number of a destination range
//
static void DoPStateDestBegin(tr_pstate_t *pstate)
{
   if(E_GetTranslationToken(pstate) == TR_TOKEN_NUM)
   {
      pstate->dstbegin = pstate->token->toInt();
      pstate->prevstate = pstate->state;
      pstate->state = TR_PSTATE_COLON;
   }
   else
      pstate->error = true;
}

// 
// DoPStateDestEnd
//
// Expecting the ending number of a destination range
//
static void DoPStateDestEnd(tr_pstate_t *pstate)
{
   if(E_GetTranslationToken(pstate) == TR_TOKEN_NUM)
   {
      pstate->dstend = pstate->token->toInt();
      
      if(pstate->singlecolor)
      {
         // If this was a single-color source range, duplicate the end color
         // to the beginning color for the destination range.
         pstate->dstbegin = pstate->dstend;
         pstate->singlecolor = false;
      }
      
      pstate->state = TR_PSTATE_COMMAOREND;
   }
   else
      pstate->error = true;
}

//
// DoPStateCommaOrEnd
//
// Need either a comma, which will start a new range, or the end of the string.
//
static void DoPStateCommaOrEnd(tr_pstate_t *pstate)
{
   int tokentype = E_GetTranslationToken(pstate);
   
   // push range
   PushRange(pstate);
   
   switch(tokentype)
   {
   case TR_TOKEN_END:
      pstate->done = true;
      break;
   case TR_TOKEN_COMMA:
      pstate->state = TR_PSTATE_SRCBEGIN;
      break;
   default:
      pstate->error = true;
      break;
   }
}

// Parser callback type
typedef void (*tr_pfunc)(tr_pstate_t *);

// Parser state table
static tr_pfunc trpfuncs[TR_PSTATE_NUMSTATES] = 
{
   DoPStateSrcBegin,   // TR_PSTATE_SRCBEGIN
   DoPStateColon,      // TR_PSTATE_COLON
   DoPStateSrcEnd,     // TR_PSTATE_SRCEND
   DoPStateEquals,     // TR_PSTATE_EQUALS
   DoPStateDestBegin,  // TR_PSTATE_DSTBEGIN
   DoPStateDestEnd,    // TR_PSTATE_DSTEND
   DoPStateCommaOrEnd  // TR_PSTATE_COMMAOREND
};

#define RANGE_CLAMP(c, end) ((c) <= (end) ? (c) : (end))

//
// E_ParseTranslation
//
byte *E_ParseTranslation(const char *str, int tag)
{
   int i;
   qstring tokenbuf;
   byte *translation = ecalloctag(byte *, 1, 256, tag, NULL);
   tr_pstate_t parserstate;

   // initialize to monotonically increasing sequence (identity translation)
   for(i = 0; i < 256; i++)
      translation[i] = i;

   // setup the parser
   parserstate.state       = TR_PSTATE_SRCBEGIN;
   parserstate.prevstate   = TR_PSTATE_SRCBEGIN;
   parserstate.token       = &tokenbuf;
   parserstate.input       = str;
   parserstate.inputpos    = 0;
   parserstate.error       = false;
   parserstate.done        = false;
   parserstate.singlecolor = false;
   parserstate.ranges      = NULL;

   while(!(parserstate.done || parserstate.error))
      trpfuncs[parserstate.state](&parserstate);

   // If no error occurred, apply all translation ranges
   if(!parserstate.error)
   {
      tr_range_t *range = parserstate.ranges;

      while(range)
      {
         tr_range_t *next = range->next;
         int numsrccolors = range->srcend - range->srcbegin;
         int numdstcolors = range->dstend - range->dstbegin;

         if(numsrccolors == 0)
            translation[range->srcbegin] = range->dstbegin;
         else
         {
            fixed_t dst      = range->dstbegin * FRACUNIT;
            fixed_t deststep = (numdstcolors * FRACUNIT) / numsrccolors;

            // populate source indices with destination colors
            for(int src = range->srcbegin; src <= range->srcend; src++)
            {
               translation[src] = RANGE_CLAMP(dst / FRACUNIT, range->dstend);
               dst += deststep;
            }
         }

         // done with this range
         efree(range);

         // step to next range
         range = next;
      }
   }

   return translation;
}

//
// E_CfgListToCommaString
//
// Concatenates all values in a CFGF_LIST CFG_STR option into a single
// string of comma-delimited values.
//
void E_CfgListToCommaString(cfg_t *sec, const char *optname, qstring &output)
{
   unsigned int numopts = cfg_size(sec, optname);
   
   output.clear();

   for(unsigned int i = 0; i < numopts; i++)
   {
      const char *str = cfg_getnstr(sec, optname, i);

      if(str)
         output += str;

      size_t len = output.length();
      if(i != numopts - 1 && len && output[len - 1] != ',')
         output += ',';
   }
}

// EOF


