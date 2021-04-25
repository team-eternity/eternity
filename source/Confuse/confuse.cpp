/* Configuration file parser -*- tab-width: 4; -*-
 *
 * Copyright (c) 2002-2003, Martin Hedenfalk <mhe@home.se>
 * Modifications for Eternity Copyright (c) 2012 James Haley
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 */

#include <limits.h>
#include <errno.h>
#include <sys/types.h>

// haleyjd: Use zone memory and other assets such as wad/file io
// emulation, since this is now being made Eternity-specific by
// necessity.

#include "../z_zone.h"
#include "../d_io.h"
#include "../d_dwfile.h"
#include "../i_system.h"
#include "../w_wad.h"

#include "confuse.h"
#include "lexer.h"

//=============================================================================
//
// Globals
//

// haleyjd 03/08/03: Modifications for Eternity
// #define PACKAGE_VERSION "Eternity version"
// #define PACKAGE_STRING  "libConfuse"

// haleyjd: removed ENABLE_NLS

const char *confuse_version   = "Eternity version";
const char *confuse_copyright = "libConfuse by Martin Hedenfalk <mhe@home.se>";
const char *confuse_author    = "Martin Hedenfalk <mhe@home.se>";

//=============================================================================
//
// Utilities
//

#if defined(NDEBUG)
#define cfg_assert(test) ((void)0)
#else
#define cfg_assert(test) \
  ((void)((test)||(my_assert(#test, __FILE__, __LINE__),0)))
#endif

#define STATE_CONTINUE 0
#define STATE_EOF -1
#define STATE_ERROR 1

#ifndef NDEBUG
//
// my_assert
//
// haleyjd 04/03/03: Eternity cannot use assert, as it aborts the
// program, leaving everything messy. So this has replaced assert,
// and calls I_Error instead.
//
static void my_assert(const char *msg, const char *file, int line)
{
   I_Error("Assertion failed at %s line %d: %s\n", file, line, msg);
}
#endif

//
// cfg_strndup
//
// haleyjd 11/08/09: just use this unconditionally, because I cannot be
// bothered to build GCC version detection into the makefiles for Linux
// now that GCC 4.4.x has this function.
//
static char *cfg_strndup(const char *s, size_t n)
{
   char *r;
   
   if(s == nullptr)
      return nullptr;
   
   r = ecalloc(char *, 1, n + 1);
   strncpy(r, s, n);
   r[n] = 0;
   return r;
}

//=============================================================================
//
// Option Retrieval
//

cfg_opt_t *cfg_getopt(const cfg_t *const cfg, const char *name)
{
   int i;
   const cfg_t *sec = cfg;
   
   cfg_assert(cfg && cfg->name && name);

   // haleyjd 07/11/03: from CVS, traverses subsections
   while(name && *name)
   {
      char *secname;
      size_t len = strcspn(name, "|");

      if(name[len] == 0) /* no more subsections */
         break;
      if(len)
      {
         secname = cfg_strndup(name, len);
         sec = cfg_getsec(sec, secname);
         if(sec == nullptr)
            cfg_error(cfg, "no such option '%s'\n", secname);
         efree(secname);
         if(sec == nullptr)
            return nullptr;
      }
      name += len;
      name += strspn(name, "|");
   }

   // haleyjd 05/25/10: check for +/- prefixes for flag items
   if(name[0] == '+' || name[0] == '-')
      ++name; // skip past it for lookup
   
   for(i = 0; sec->opts[i].name; i++)
   {
      if(is_set(CFGF_NOCASE, sec->flags))
      {
         if(strcasecmp(sec->opts[i].name, name) == 0)
            return &sec->opts[i];
      } 
      else 
      {
         if(strcmp(sec->opts[i].name, name) == 0)
            return &sec->opts[i];
      }
   }
   cfg_error(cfg, "no such option '%s'\n", name);
   return nullptr;
}

//
// cfg_gettitleopt
//
// haleyjd 09/19/12
//
cfg_opt_t *cfg_gettitleopt(cfg_t *cfg)
{
   // search for the first MVPROP which is flagged as TITLEPROPS
   for(int i = 0; cfg->opts[i].name; i++)
   {
      if(is_set(CFGF_TITLEPROPS, cfg->opts[i].flags) &&
         cfg->opts[i].type == CFGT_MVPROP)
         return &cfg->opts[i];
   }
   
   cfg_error(cfg, "section '%s' does not define title properties\n", cfg->name);
   return nullptr;
}

const char *cfg_title(cfg_t *cfg)
{
   return cfg->title;
}

unsigned int cfg_size(const cfg_t *const cfg, const char *name)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   if(opt)
      return opt->nvalues;
   return 0;
}

cfg_t *cfg_displaced(cfg_t *cfg)
{
   // haleyjd 01/02/12: for getting the displaced cfg_t
   return cfg->displaced;
}

signed int cfg_getnint(cfg_t *cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt)
   {
      cfg_assert(opt->type == CFGT_INT);
      if(opt->nvalues == 0)
         return opt->idef;
      else
      {
         cfg_assert(index < opt->nvalues);
         return opt->values[index]->number;
      }
   }
   else
      return 0;
}

signed int cfg_getint(cfg_t *cfg, const char *name)
{
   return cfg_getnint(cfg, name, 0);
}

double cfg_getnfloat(cfg_t *cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt) 
   {
      cfg_assert(opt->type == CFGT_FLOAT);
      if(opt->nvalues == 0)
         return opt->fpdef;
      else
      {
         cfg_assert(index < opt->nvalues);
         return opt->values[index]->fpnumber;
      }
   }
   else
      return 0;
}

double cfg_getfloat(cfg_t *cfg, const char *name)
{
   return cfg_getnfloat(cfg, name, 0);
}

bool cfg_getnbool(cfg_t *cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt)
   {
      cfg_assert(opt->type == CFGT_BOOL);
      if(opt->nvalues == 0)
         return opt->bdef;
      else
      {
         cfg_assert(index < opt->nvalues);
         return opt->values[index]->boolean;
      }
   }
   else
      return false;
}

bool cfg_getbool(cfg_t *cfg, const char *name)
{
   return cfg_getnbool(cfg, name, 0);
}

// haleyjd 12/27/10: return value must be explicitly const (was implicitly
// considered that way anyway)
const char *cfg_getnstr(cfg_t *cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt)
   {
      cfg_assert(opt->type == CFGT_STR || opt->type == CFGT_STRFUNC); // haleyjd
      if(opt->nvalues == 0) 
         return opt->sdef;
      else 
      {
         cfg_assert(index < opt->nvalues);
         return opt->values[index]->string;
      }
   }
   return nullptr;
}

const char *cfg_getstr(cfg_t *cfg, const char *name)
{
   return cfg_getnstr(cfg, name, 0);
}

char *cfg_getstrdup(cfg_t *cfg, const char *name)
{
   // haleyjd 12/31/11: get a dynamic copy of a string
   const char *value = cfg_getstr(cfg, name);

   return value ? estrdup(value) : nullptr;
}

cfg_t *cfg_getnsec(const cfg_t *const cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt) 
   {
      cfg_assert(opt->type == CFGT_SEC);
      cfg_assert(opt->values);
      cfg_assert(index < opt->nvalues);
      return opt->values[index]->section;
   }
   return nullptr;
}

cfg_t *cfg_gettsec(cfg_t *cfg, const char *name, const char *title)
{
   unsigned int i, n;
   
   n = cfg_size(cfg, name);
   
   for(i = 0; i < n; i++) 
   {
      cfg_t *sec = cfg_getnsec(cfg, name, i);
      cfg_assert(sec && sec->title);
      
      if(is_set(CFGF_NOCASE, cfg->flags))
      {
         if(strcasecmp(title, sec->title) == 0)
            return sec;
      } 
      else 
      {
         if(strcmp(title, sec->title) == 0)
            return sec;
      }
   }
   return nullptr;
}

cfg_t *cfg_getsec(const cfg_t *const cfg, const char *name)
{
   return cfg_getnsec(cfg, name, 0);
}

//
// cfg_getnmvprop
//
// haleyjd 09/26/09: multi-valued properties, which are in effect
// fixed-order sections, or lists of values with expected types.
//
cfg_t *cfg_getnmvprop(cfg_t *cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt) 
   {
      cfg_assert(opt->type == CFGT_MVPROP);
      cfg_assert(opt->values);
      cfg_assert(index < opt->nvalues);
      return opt->values[index]->section;
   }
   return nullptr;
}

//
// cfg_getmvprop
//
// haleyjd 09/26/09
//
cfg_t *cfg_getmvprop(cfg_t *cfg, const char *name)
{
   return cfg_getnmvprop(cfg, name, 0);
}

//
// cfg_getnflag
//
// haleyjd 05/25/10
//
signed int cfg_getnflag(cfg_t *cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt)
   {
      cfg_assert(opt->type == CFGT_FLAG);
      if(opt->nvalues == 0)
         return opt->idef;
      else
      {
         cfg_assert(index < opt->nvalues);
         return opt->values[index]->number;
      }
   }
   else
      return 0;
}

//
// cfg_getflag
//
// haleyjd 05/25/10
//
signed int cfg_getflag(cfg_t *cfg, const char *name)
{
   return cfg_getnflag(cfg, name, 0);
}

//
// cfg_gettitleprops
//
// haleyjd 09/30/12: Retrieve title properties for a section, if it has such.
// The title properties are an effectively anonymous MVPROP which is specified
// in the header of the section's definition following the title, as such:
//
//    secoptname title : opt, opt, opt, ...
//    {
//       normal section opts
//    }
//
cfg_t *cfg_gettitleprops(cfg_t *cfg)
{
   cfg_opt_t *opt = cfg_gettitleopt(cfg);

   if(opt)
   {
      cfg_assert(opt->type == CFGT_MVPROP);
      cfg_assert(opt->values);
      return opt->values[0]->section;
   }
   return nullptr;
}

//=============================================================================
//
// Internal Value Maintenance
//

static cfg_value_t *cfg_addval(cfg_opt_t *opt)
{
   opt->values = erealloc(cfg_value_t **, opt->values,
                          (opt->nvalues+1) * sizeof(cfg_value_t *));
   cfg_assert(opt->values);
   opt->values[opt->nvalues] = estructalloc(cfg_value_t, 1);
   return opt->values[opt->nvalues++];
}

static cfg_opt_t *cfg_dupopts(cfg_opt_t *opts)
{
   int n;
   cfg_opt_t *dupopts;
   
   for(n = 0; opts[n].name; n++) /* do nothing */ ;

   ++n;
   dupopts = estructalloc(cfg_opt_t, n);
   memcpy(dupopts, opts, n * sizeof(cfg_opt_t));
   return dupopts;
}

int cfg_parse_boolean(const char *s)
{
   if(strcasecmp(s, "true") == 0 || 
      strcasecmp(s, "on")   == 0 || 
      strcasecmp(s, "yes")  == 0)
      return 1;
   else if(strcasecmp(s, "false") == 0 || 
           strcasecmp(s, "off")   == 0 || 
           strcasecmp(s, "no")    == 0)
      return 0;
   return -1;
}

cfg_value_t *cfg_setopt(cfg_t *cfg, cfg_opt_t *opt, const char *value)
{
   cfg_value_t *val = nullptr;
   int b;
   char *s;
   double f;
   int i;
   char *endptr;
   cfg_t *oldsection;
   
   cfg_assert(cfg && opt);
   
   if(opt->simple_value)
   {
      cfg_assert(opt->type != CFGT_SEC);
      val = (cfg_value_t *)opt->simple_value;
   } 
   else 
   {
      // haleyjd: code from 2.0: check for sections with same name

      if(opt->nvalues == 0 || is_set(CFGF_MULTI, opt->flags) ||
         is_set(CFGF_LIST, opt->flags))
      {
         val = nullptr;
         if(opt->type == CFGT_SEC && is_set(CFGF_TITLE, opt->flags))
         {
            unsigned int ii;

            /* check if there is already a section with the same title */
            cfg_assert(value);
            for(ii = 0; ii < opt->nvalues; ii++)
            {
               cfg_t *sec = opt->values[ii]->section;
               if(is_set(CFGF_NOCASE, cfg->flags))
               {
                  if(strcasecmp(value, sec->title) == 0)
                     val = opt->values[ii];
               }
               else
               {
                  if(strcmp(value, sec->title) == 0)
                     val = opt->values[ii];
               }
            }
         }
         if(val == nullptr)
            val = cfg_addval(opt);
      }
      else
         val = opt->values[0];
   }
   
   switch(opt->type)
   {
   case CFGT_INT:
      if(opt->cb)
      {
         if((*opt->cb)(cfg, opt, value, &i) != 0)
            return nullptr;
         val->number = i;
      } 
      else 
      {
         // haleyjd 11/10/08: modified to print invalid values
         val->number = static_cast<int>(strtol(value, &endptr, 0));
         if(*endptr != '\0')
         {            
            cfg_error(cfg, "invalid integer value '%s' for option '%s'\n",
                      value, opt->name);
            return nullptr;
         }
         if(errno == ERANGE) 
         {
            cfg_error(cfg,
               "integer value '%s' for option '%s' is out of range\n",
               value, opt->name);
            return nullptr;
         }
      }
      break;
   case CFGT_FLOAT:
      if(opt->cb)
      {
         if((*opt->cb)(cfg, opt, value, &f) != 0)
            return nullptr;
         val->fpnumber = f;
      } 
      else 
      {
         val->fpnumber = strtod(value, &endptr);
         if(*endptr != '\0')
         {
            cfg_error(cfg,
               "invalid floating point value for option '%s'\n",
               opt->name);
            return nullptr;
         }
         if(errno == ERANGE)
         {
            cfg_error(cfg,
               "floating point value for option '%s' is out of range\n",
               opt->name);
            return nullptr;
         }
      }
      break;
   case CFGT_STR:
   case CFGT_STRFUNC: // haleyjd
      if(val->string)
         efree(val->string);
      if(opt->cb)
      {
         s = nullptr;
         if((*opt->cb)(cfg, opt, value, &s) != 0)
            return nullptr;
         val->string = estrdup(s);
      } else
         val->string = estrdup(value);
      break;
   case CFGT_SEC:
   case CFGT_MVPROP: // haleyjd
      oldsection = val->section;
      val->section = estructalloc(cfg_t, 1);
      cfg_assert(val->section);
      val->section->namealloc = estrdup(opt->name); // haleyjd 04/14/11
      val->section->name      = val->section->namealloc;
      val->section->opts      = cfg_dupopts(opt->subopts);
      val->section->flags     = cfg->flags;
      val->section->flags    |= CFGF_ALLOCATED;
      val->section->filename  = cfg->filename;
      val->section->line      = cfg->line;
      val->section->errfunc   = cfg->errfunc;
      val->section->title     = value ? estrdup(value) : nullptr;
      // haleyjd 01/02/12: make the old section a displaced version of the
      // new one, so that it can remain accessible
      val->section->displaced = oldsection;
      break;
   case CFGT_BOOL:
      if(opt->cb)
      {
         if((*opt->cb)(cfg, opt, value, &b) != 0)
            return nullptr;
      } 
      else
      {
         b = cfg_parse_boolean(value);
         if(b == -1)
         {
            cfg_error(cfg, "invalid boolean value for option '%s'\n",
               opt->name);
            return nullptr;
         }
      }
      val->boolean = !!b;
      break;
   case CFGT_FLAG: // haleyjd
      if(opt->cb)
      {
         if((*opt->cb)(cfg, opt, value, &i) != 0)
            return nullptr;
         val->number = i;
      }
      else
      {
         // If this flag item has the SIGNPREFIX flag, it is expected to begin
         // with a + or - character. + means to set the value to 1, and - means
         // to set it to 0.
         // Otherwise, presence of the option in the input is expected to invert 
         // the default value of the option.
         if(is_set(CFGF_SIGNPREFIX, opt->flags))
            val->number = (value[0] == '+');
         else
            val->number = !(opt->bdef);
      }
      break;
   default:
      cfg_error(cfg, "internal error in cfg_setopt(%s, %s)\n",
                opt->name, value);
      cfg_assert(0);
      break;
   }
   return val;
}

void cfg_free_value(cfg_opt_t *opt)
{
   unsigned int i;
   
   if(opt == nullptr)
      return;
   
   for(i = 0; i < opt->nvalues; i++)
   {
      if(opt->type == CFGT_STR || opt->type == CFGT_STRFUNC) // haleyjd
         efree(opt->values[i]->string);
      else if(opt->type == CFGT_SEC || opt->type == CFGT_MVPROP) // haleyjd
         cfg_free(opt->values[i]->section);
      efree(opt->values[i]);
   }
   efree(opt->values);
   opt->values = nullptr;
   opt->nvalues = 0;
}

//=============================================================================
//
// Callbacks
//

cfg_errfunc_t cfg_set_error_function(cfg_t *cfg, cfg_errfunc_t errfunc)
{
   cfg_errfunc_t old;
   
   cfg_assert(cfg);   
   old = cfg->errfunc;
   cfg->errfunc = errfunc;
   return old;
}

//
// cfg_set_lexer_callback
//
// haleyjd 03/21/10
//
cfg_lexfunc_t cfg_set_lexer_callback(cfg_t *cfg, cfg_lexfunc_t lexfunc)
{
   cfg_lexfunc_t old;

   cfg_assert(cfg);
   old = cfg->lexfunc;
   cfg->lexfunc = lexfunc;
   return old;
}

void cfg_error(const cfg_t *const cfg, E_FORMAT_STRING(const char *fmt), ...)
{
   va_list ap;
   
   va_start(ap, fmt);
   
   if(cfg->errfunc)
   {
      (*cfg->errfunc)(cfg, fmt, ap);
   }
   else 
   {
      if(cfg && cfg->filename && cfg->line)
         fprintf(stderr, "%s:%d: ", cfg->filename, cfg->line);
      else if(cfg && cfg->filename)
         fprintf(stderr, "%s: ", cfg->filename);
      vfprintf(stderr, fmt, ap);
      fprintf(stderr, "\n");
   }
   
   va_end(ap);
}

static int call_function(cfg_t *cfg, cfg_opt_t *opt, cfg_opt_t *funcopt)
{
   int ret;
   const char **argv;
   unsigned int i;

   /* create a regular argv string vector and call
    * the registered function
    */

   argv = ecalloc(const char **, funcopt->nvalues, sizeof(char *));

   for(i = 0; i < funcopt->nvalues; i++)
      argv[i] = funcopt->values[i]->string;
   ret = (*opt->func)(cfg, opt, funcopt->nvalues, argv);
   cfg_free_value(funcopt); // haleyjd: CVS fix
   efree((void *)argv);
   return ret;
}

//=============================================================================
//
// Parser
//

// haleyjd 04/03/08: state enumeration
enum
{
   STATE_EXPECT_OPTION,    // state 0
   STATE_EXPECT_ASSIGN,    // state 1
   STATE_EXPECT_VALUE,     // state 2
   STATE_EXPECT_LISTBRACE, // state 3
   STATE_EXPECT_LISTNEXT,  // state 4
   STATE_EXPECT_SECBRACE,  // state 5
   STATE_EXPECT_TITLE,     // state 6
   STATE_EXPECT_PAREN,     // state 7
   STATE_EXPECT_ARGUMENT,  // state 8
   STATE_EXPECT_ARGNEXT,   // state 9
   STATE_EXPECT_LOOKFOR,   // state 10
   STATE_EXPECT_PROPNEXT,  // state 11

   CFG_NUM_PSTATES
};

// haleyjd: parser state structure
struct cfg_pstate_t
{
   int   state;          // current state
   int   next_state;     // next state to visit, in some circumstances
   char *opttitle;       // cached option title, for sections
   bool  append_value;   // += has been used for assignment
   bool  found_func;     // found "lookfor" function
   bool  list_nobraces;  // list has been defined without braces
   int   tok;            // current token value
   int   skip_token;     // skip the next token if > 0
   int   propindex;      // index within current mvprop option

   // Data needed to parse title options
   cfg_value_t *titleParentVal; // remembered parent section value
   cfg_opt_t   *backupOpt;      // remembered option
   int          mv_exit_state;  // state to return to from MVPROPS loop
   
   cfg_opt_t   *opt;     // current option
   cfg_value_t *val;     // current value
   cfg_opt_t    funcopt; // dummy option, for function call parsing

   ~cfg_pstate_t()
   {
      if(opttitle)
      {
         efree(opttitle);
         opttitle = nullptr;
      }
   }
};

// Forward declaration required for recursion.
static int cfg_parse_internal(cfg_t *cfg, int level);

//
// cfg_init_pstate
//
// Initialize parser state variables.
//
static void cfg_init_pstate(cfg_pstate_t *pstate)
{
   memset(pstate, 0, sizeof(*pstate));
   pstate->funcopt.type = CFGT_STR;
}

// haleyjd: parser state handler function signature
typedef int (*cfg_pstate_func)(cfg_t *, int, cfg_pstate_t &);

//
// haleyjd: parser states
//
// Because cfg_parse_internal was too long and messy, I have broken it up into
// discrete state handlers as with EE's other FSA parsers.
//

//
// cfg_pstate_expectoption
//
// Expecting an option.
//
static int cfg_pstate_expectoption(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   if(pstate.tok == '}')
   {
      if(level == 0)
      {
         cfg_error(cfg, "unexpected closing brace\n");
         return STATE_ERROR;
      }
      return STATE_EOF; // End of a list or section
   }

   if(pstate.tok != CFGT_STR)
   {
      cfg_error(cfg, "unexpected token '%s'\n", mytext);
      return STATE_ERROR;
   }         

   if((pstate.opt = cfg_getopt(cfg, mytext)) == nullptr) // haleyjd
      return STATE_ERROR;

   switch(pstate.opt->type)
   {
   case CFGT_SEC: // Section
      if(is_set(CFGF_TITLE, pstate.opt->flags)) // requires title?
         pstate.state = STATE_EXPECT_TITLE;
      else
         pstate.state = STATE_EXPECT_SECBRACE;
      break;

   case CFGT_FUNC: // Function call
      pstate.state = STATE_EXPECT_PAREN;
      break;

   case CFGT_FLAG: // haleyjd: flag options, which are simple keywords
      if(cfg_setopt(cfg, pstate.opt, mytext) == nullptr)
         return STATE_ERROR;
      // remain in STATE_EXPECT_OPTION
      break;

   default: // List or ordinary scalar option
      pstate.state = STATE_EXPECT_ASSIGN;
      break;
   }

   return STATE_CONTINUE;
}

//
// cfg_pstate_expectassign
//
// Expecting an assignment: '=', '+=', or, if in "Alfheim" dialect or 
// higher, ':' (such are not treated as tokens otherwise).
// '=' and ':' are strictly optional.
//
static int cfg_pstate_expectassign(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   pstate.append_value = false;

   if(pstate.tok == '+') // Append to list?
   {
      if(!is_set(CFGF_LIST, pstate.opt->flags))
      {
         cfg_error(cfg, "attempt to append to non-list option %s\n", 
                   pstate.opt->name);
         return STATE_ERROR;
      }
      pstate.append_value = true;
   } 
   else if(pstate.tok != '=' && pstate.tok != ':')
   {
      // haleyjd 09/26/09: move forward anyway, and don't read a token.
      // This makes ='s optional!
      pstate.skip_token = 1;
   }

   if(pstate.opt->type == CFGT_MVPROP) // haleyjd: multi-valued properties
   {
      pstate.val = cfg_setopt(cfg, pstate.opt, pstate.opttitle);
      efree(pstate.opttitle);
      pstate.opttitle = nullptr;
      if(!pstate.val || !pstate.val->section || !pstate.val->section->opts)
         return STATE_ERROR;

      // start at opts[0]
      pstate.propindex = 0;

      // set the new option
      pstate.opt = &(pstate.val->section->opts[pstate.propindex++]);

      // must have at least one valid option
      cfg_assert(pstate.opt->type != CFGT_NONE);

      pstate.state         = STATE_EXPECT_VALUE;
      pstate.next_state    = STATE_EXPECT_PROPNEXT;
      pstate.mv_exit_state = STATE_EXPECT_OPTION;
   }
   else if(is_set(CFGF_LIST, pstate.opt->flags))
   {
      // Start list
      if(!pstate.append_value)
         cfg_free_value(pstate.opt);
      pstate.state = STATE_EXPECT_LISTBRACE;
   }
   else
   {
      // Ordinary scalar option
      pstate.state      = STATE_EXPECT_VALUE;
      pstate.next_state = STATE_EXPECT_OPTION;
   }

   return STATE_CONTINUE;
}

//
// cfg_pstate_expectvalue
//
// Expecting a value for the option.
//
static int cfg_pstate_expectvalue(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   // End of list?
   if(pstate.tok == '}' && is_set(CFGF_LIST, pstate.opt->flags))
   {
      if(!pstate.list_nobraces)
      {
         lexer_set_unquoted_spaces(false); /* haleyjd */
         pstate.state = STATE_EXPECT_OPTION;
         return STATE_CONTINUE;
      }
   }

   if(pstate.tok != CFGT_STR)
   {
      cfg_error(cfg, "unexpected token '%s'\n", mytext);
      return STATE_ERROR;
   }

   // haleyjd 04/03/08: function-valued options: the name of the
   // function being called will become the value of the option.
   // The arguments are provided to the callback function as usual.
   if(pstate.opt->type == CFGT_STRFUNC)
      pstate.next_state = STATE_EXPECT_PAREN;

   if(cfg_setopt(cfg, pstate.opt, mytext) == nullptr)
      return STATE_ERROR;

   pstate.state = pstate.next_state;
   return STATE_CONTINUE;
}

//
// cfg_pstate_expectlistbrace
//
// Expecting an optional curly brace delimiting the beginning of a list.
//
static int cfg_pstate_expectlistbrace(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   if(pstate.tok != '{')
   {
      pstate.list_nobraces = true;
      pstate.skip_token = 1;
   }

   // haleyjd 12/23/06: set unquoted string state
   if(is_set(CFGF_STRSPACE, pstate.opt->flags))
      lexer_set_unquoted_spaces(true);
   
   pstate.state      = STATE_EXPECT_VALUE;
   pstate.next_state = STATE_EXPECT_LISTNEXT;
   return STATE_CONTINUE;
}

//
// cfg_pstate_expectlistnext
//
// Expecting ',' or the end of the list.
//
static int cfg_pstate_expectlistnext(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   if(pstate.tok == ',')
   {
      pstate.state      = STATE_EXPECT_VALUE;
      pstate.next_state = STATE_EXPECT_LISTNEXT;
   } 
   else if(!pstate.list_nobraces && pstate.tok == '}')
   {
      lexer_set_unquoted_spaces(false);   // haleyjd
      pstate.state = STATE_EXPECT_OPTION;
   }
   else if(pstate.list_nobraces)
   {
      // haleyjd 05/30/12: allow lists with no braces
      pstate.list_nobraces = false;
      pstate.skip_token    = 1;
      pstate.state         = STATE_EXPECT_OPTION;
   }
   else
   {
      cfg_error(cfg, "unexpected token '%s'\n", mytext);
      return STATE_ERROR;
   }

   return STATE_CONTINUE;
}

//
// cfg_pstate_expectsecbrace
//
// Expecting a curly brace delimiting the start of a section.
//
static int cfg_pstate_expectsecbrace(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   // Found title options indicator?
   if(pstate.tok == ':')
   {
      cfg_t     *titleParentCfg;
      cfg_opt_t *titlePropsOpt;

      // Must *not* be returning from parsing a previous title option sequence
      if(pstate.titleParentVal)
      {
         cfg_error(cfg, "multiple title option sets are not allowed\n");
         return STATE_ERROR;
      }

      // Remember the current option
      pstate.backupOpt = pstate.opt;

      // Get the parent section (the one we were about to parse)
      pstate.titleParentVal = cfg_setopt(cfg, pstate.opt, pstate.opttitle);
      if(!pstate.titleParentVal)
         return STATE_ERROR;

      titleParentCfg = pstate.titleParentVal->section;

      // Find an MVPROP option in the parent section that is CFGF_TITLEPROPS
      if(!(titlePropsOpt = cfg_gettitleopt(titleParentCfg)))
         return STATE_ERROR;

      // Add its value to the parent section
      pstate.val = cfg_setopt(titleParentCfg, titlePropsOpt, titlePropsOpt->name);
      if(!pstate.val || !pstate.val->section || !pstate.val->section->opts)
         return STATE_ERROR;

      // Parse the MVPROP that follows
      pstate.propindex = 0;
      pstate.opt = &(pstate.val->section->opts[pstate.propindex++]);
      pstate.state         = STATE_EXPECT_VALUE;
      pstate.next_state    = STATE_EXPECT_PROPNEXT;
      pstate.mv_exit_state = STATE_EXPECT_SECBRACE; // Return here when finished
   }
   else
   {
      // Restore backed up option if needed
      if(pstate.backupOpt)
      {
         pstate.opt = pstate.backupOpt;
         pstate.backupOpt = nullptr;
      }

      if(pstate.tok != '{')
      {
         cfg_error(cfg, "missing opening brace for section '%s'\n",
            pstate.opt->name);
         return STATE_ERROR;
      }

      // Did we just return from parsing title options?
      if(pstate.titleParentVal)
      {
         pstate.val = pstate.titleParentVal;
         pstate.titleParentVal = nullptr;
      }
      else
      {
         // Get new value
         pstate.val = cfg_setopt(cfg, pstate.opt, pstate.opttitle);
         efree(pstate.opttitle);
         pstate.opttitle = nullptr;
         if(!pstate.val)
            return STATE_ERROR;
      }

      // Recursively parse the interior of the section
      if(cfg_parse_internal(pstate.val->section, level+1) != STATE_EOF)
         return STATE_ERROR;

      cfg->line = pstate.val->section->line;
      pstate.state = STATE_EXPECT_OPTION;
   }

   return STATE_CONTINUE;
}

//
// cfg_pstate_expecttitle
//
// Expecting a title for a section which carries the CFGF_TITLE flag.
//
static int cfg_pstate_expecttitle(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   if(pstate.tok != CFGT_STR)
   {
      cfg_error(cfg, "missing title for section '%s'\n", pstate.opt->name);
      return STATE_ERROR;
   }
   else
      pstate.opttitle = estrdup(mytext);
   
   pstate.state = STATE_EXPECT_SECBRACE;   
   return STATE_CONTINUE;
}

//
// cfg_pstate_expectparen
//
// Expecting an opening parenthesis for a function call.
//
static int cfg_pstate_expectparen(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   if(pstate.tok != '(')
   {
      // haleyjd: function-valued options may have no parameter list.
      // If so, however, we must skip reading the next token of input,
      // since we already ate it.
      if(pstate.opt->type == CFGT_STRFUNC)
      {
         int ret = call_function(cfg, pstate.opt, &pstate.funcopt);
         if(ret != 0)
            return STATE_ERROR;

         pstate.state = STATE_EXPECT_OPTION;
         pstate.skip_token = 1; // reprocess the current token
         return STATE_CONTINUE;
      }
      else
      {
         cfg_error(cfg, "missing parenthesis for function '%s'\n", pstate.opt->name);
         return STATE_ERROR;
      }
   }

   pstate.state = STATE_EXPECT_ARGUMENT;
   return STATE_CONTINUE;
}

//
// cfg_pstate_expectargument
//
// Expecting the next argument, or the end of the argument list.
//
static int cfg_pstate_expectargument(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   if(pstate.tok == ')')
   {
      int ret = call_function(cfg, pstate.opt, &pstate.funcopt);
      if(ret != 0)
         return STATE_ERROR;
      
      // haleyjd 09/30/05: check for LOOKFORFUNC flag
      if(is_set(CFGF_LOOKFORFUNC, cfg->flags))
      {
         pstate.found_func = false;
         pstate.state = STATE_EXPECT_LOOKFOR; // go to new "lookfor" state
      }
      else
         pstate.state = STATE_EXPECT_OPTION;
   }
   else if(pstate.tok == CFGT_STR)
   {
      pstate.val = cfg_addval(&pstate.funcopt);
      pstate.val->string = estrdup(mytext);
      pstate.state = STATE_EXPECT_ARGNEXT;
   } 
   else 
   {
      cfg_error(cfg, "syntax error in call of function '%s'\n",
         pstate.opt->name);
      return STATE_ERROR;
   }

   return STATE_CONTINUE;
}

//
// cfg_pstate_expectargnext
//
// Expecting a comma or the end of the argument list.
//
static int cfg_pstate_expectargnext(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   if(pstate.tok == ')')
   {
      int ret = call_function(cfg, pstate.opt, &pstate.funcopt);
      if(ret != 0)
         return STATE_ERROR;

      // haleyjd 01/14/04: check for LOOKFORFUNC flag
      if(is_set(CFGF_LOOKFORFUNC, cfg->flags))
      {
         pstate.found_func = false;
         pstate.state = STATE_EXPECT_LOOKFOR; // go to new "lookfor" state
      }
      else
         pstate.state = STATE_EXPECT_OPTION;
   }
   else if(pstate.tok == ',')
   {
      pstate.state = STATE_EXPECT_ARGUMENT;
   }
   else 
   {
      cfg_error(cfg, "syntax error in call of function '%s'\n", pstate.opt->name);
      return STATE_ERROR;
   }

   return STATE_CONTINUE;
}

//
// cfg_pstate_expectlookfor
//
// Eternity extension. Enables #ifdef/#endif-style function pairs by causing
// the parser to skip ahead through any number and type of tokens until the
// "lookfor" function is identified in the token stream.
//
static int cfg_pstate_expectlookfor(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   // haleyjd 01/16/04: special state which looks for the "lookfor" function, 
   // allowing ifdef-type stuff which ignores everything until the function is 
   // encountered
   if(is_set(CFGF_NOCASE, cfg->flags))
   {
      if(!strcasecmp(mytext, cfg->lookfor))
         pstate.found_func = true;
   }
   else
   {
      if(!strcmp(mytext, cfg->lookfor))
         pstate.found_func = true;
   }

   // Found the "lookfor" function?
   if(pstate.found_func == true)
   {
      pstate.opt = cfg_getopt(cfg, mytext);
      if(pstate.opt == nullptr)
         return STATE_ERROR;

      if(pstate.opt->type == CFGT_FUNC)
         pstate.state = STATE_EXPECT_PAREN; // parse the function call
      else
      {
         cfg_error(cfg, "internal error\n");
         return STATE_ERROR;
      }
   }

   return STATE_CONTINUE;
}

//
// cfg_pstate_expectpropnext
//
// Eternity extension. Expecting ',' or the end of the property list within a
// multi-valued property, which is a form of braceless section.
//
static int cfg_pstate_expectpropnext(cfg_t *cfg, int level, cfg_pstate_t &pstate)
{
   // haleyjd 09/26/09: step to next value for a multi-value property
   if(pstate.tok == ',')
   {
      // step to next option in the section
      pstate.opt = &(pstate.val->section->opts[pstate.propindex++]);

      if(pstate.opt->type == CFGT_NONE) // reached CFG_END?
         pstate.state = pstate.mv_exit_state;
      else
      {
         pstate.state      = STATE_EXPECT_VALUE;
         pstate.next_state = STATE_EXPECT_PROPNEXT;
      }
   } 
   else
   {
      pstate.skip_token = 1; // reprocess the current token
      pstate.state = pstate.mv_exit_state;
   }
   
   return STATE_CONTINUE;
}

// haleyjd 09/19/12: parser state table
static cfg_pstate_func cfg_pstates[CFG_NUM_PSTATES] =
{
   cfg_pstate_expectoption,    // expecting an option name
   cfg_pstate_expectassign,    // expecting an equal or plus-equal sign
   cfg_pstate_expectvalue,     // expecting an option value
   cfg_pstate_expectlistbrace, // expecting an opening brace for a list option
   cfg_pstate_expectlistnext,  // expecting a separator for a list option, or closing (list) brace
   cfg_pstate_expectsecbrace,  // expecting an opening brace for a section
   cfg_pstate_expecttitle,     // expecting a title for a section 
   cfg_pstate_expectparen,     // expecting a opening parenthesis for a function
   cfg_pstate_expectargument,  // expecting a function parameter or closing paren
   cfg_pstate_expectargnext,   // expecting a comma in a function or a closing paren
   cfg_pstate_expectlookfor,   // expecting a "lookfor" function; everything else is ignored
   cfg_pstate_expectpropnext   // expecting next value in a multi-valued property
};

//
// cfg_parse_internal
//
static int cfg_parse_internal(cfg_t *cfg, int level)
{
   cfg_pstate_t pstate;

   // initialize parser state
   cfg_init_pstate(&pstate);
   
   while(1)
   {
      int ret;

      // haleyjd: possibly skip reading new tokens
      if(pstate.skip_token == 0)
         pstate.tok = mylex(cfg); // haleyjd: use custom lexer
      else
         --pstate.skip_token;
      
      if(pstate.tok == 0) // lexer should have called cfg_error
         return STATE_ERROR;
      
      if(pstate.tok == EOF)
      {
         if(pstate.state != STATE_EXPECT_OPTION)
         {
            // haleyjd: catch EOF while in lookfor state
            if(pstate.state == STATE_EXPECT_LOOKFOR) 
               cfg_error(cfg, "missing closing conditional function\n");
            else
               cfg_error(cfg, "premature end of file\n");
            return STATE_ERROR;
         }
         return STATE_EOF;
      }

#ifdef RANGECHECK
      // missing state, internal error, abort
      if(pstate.state < 0 || pstate.state >= CFG_NUM_PSTATES)
         cfg_assert(0);
#endif

      if((ret = cfg_pstates[pstate.state](cfg, level, pstate)) != STATE_CONTINUE)
         return ret;
   }

   return STATE_EOF;
}

cfg_t *cfg_init(cfg_opt_t *opts, cfg_flag_t flags)
{
   cfg_t *cfg;
   
   cfg = estructalloc(cfg_t, 1);
   cfg_assert(cfg);
   memset(cfg, 0, sizeof(cfg_t));

   cfg->name     = "root";
   cfg->opts     = opts;
   cfg->flags    = flags;
   cfg->filename = nullptr;
   cfg->line     = 0;
   cfg->lumpnum  = -1;   // haleyjd
   cfg->errfunc  = nullptr;
   cfg->lexfunc  = nullptr; // haleyjd
   cfg->lookfor  = nullptr; // haleyjd

   // haleyjd: removed ENABLE_NLS

   return cfg;
}

char *cfg_tilde_expand(const char *filename)
{   
   // haleyjd 03/08/03: removed tilde expansion for eternity
   return estrdup(filename);
}

int cfg_parse_dwfile(cfg_t *cfg, const char *filename, DWFILE *file)
{
   int ret = CFG_SUCCESS; // haleyjd

   cfg_assert(cfg && filename);
 
   // haleyjd 07/11/03: CVS fix
   if(cfg->filename)
      efree(cfg->filename);

   cfg->filename = cfg_tilde_expand(filename);
   if(cfg->filename == nullptr)
   {
      cfg_error(cfg, "%s: can't expand home directory\n", filename);
      return CFG_FILE_ERROR;
   }

   cfg->line = 1;

   cfg->lumpnum = file->getLumpNum();

   // haleyjd: initialize the lexer
   if(lexer_init(cfg, file) == 0)
      ret = cfg_parse_internal(cfg, 0);

   // haleyjd: reset the lexer state
   lexer_reset();

   if(ret == STATE_ERROR)
      return CFG_PARSE_ERROR;

   return CFG_SUCCESS;
}

int cfg_parse(cfg_t *cfg, const char *filename)
{
   DWFILE dwfile; // haleyjd

   dwfile.openFile(filename, "rb");
   
   if(!dwfile.isOpen())
      return CFG_FILE_ERROR;

   return cfg_parse_dwfile(cfg, filename, &dwfile);
}

// 
// cfg_parselump
// 
// haleyjd 04/03/03: allow input from WAD lumps
//
int cfg_parselump(cfg_t *cfg, const char *lumpname, int lumpnum)
{
   if(!wGlobalDir.getLumpInfo()[lumpnum]->size)
      return CFG_SUCCESS;  // just quit as if nothing happened

   DWFILE dwfile; // haleyjd

   dwfile.openLump(lumpnum);
   
   if(!dwfile.isOpen())
      return CFG_FILE_ERROR;

   return cfg_parse_dwfile(cfg, lumpname, &dwfile);
}

void cfg_free(cfg_t *cfg)
{
   int i;
   
   if(cfg == nullptr)
      return;

   // haleyjd 01/02/12: free any displaced section(s) recursively
   if(cfg->displaced)
   {
      cfg_free(cfg->displaced);
      cfg->displaced = nullptr;
   }
   
   for(i = 0; cfg->opts[i].name; i++)
      cfg_free_value(&cfg->opts[i]);

   if(is_set(CFGF_ALLOCATED, cfg->flags))
   {
      efree(cfg->namealloc);
      efree(cfg->opts);
      efree(const_cast<char *>(cfg->title));
   }
   else
      efree(cfg->filename);
   
   efree(cfg);
}

int cfg_include(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
   char *data;

   if(argc != 1)      
   {
      cfg_error(cfg, "wrong number of arguments to cfg_include()\n");
      return 1;
   }

   if(!(data = cfg_lexer_mustopen(cfg, argv[0], -1, nullptr)))
      return 1;

   return cfg_lexer_include(cfg, data, argv[0], -1);
}

//=============================================================================
//
// haleyjd 04/03/08: added cfg_t value-setting functions from libConfuse 2.0
//

static cfg_value_t *cfg_getval(cfg_opt_t *opt, unsigned int index)
{
   cfg_value_t *val = nullptr;

   cfg_assert(index == 0 || is_set(CFGF_LIST, opt->flags));

   if(opt->simple_value)
   {
      val = (cfg_value_t *)opt->simple_value;
   }
   else
   {
      /*
      if(is_set(CFGF_RESET, opt->flags)) 
      {
         cfg_free_value(opt);
         opt->flags &= ~CFGF_RESET;
      }
      */
      if(index >= opt->nvalues)
      {
         val = cfg_addval(opt);
      }
      else
         val = opt->values[index];
   }

   return val;
}

void cfg_opt_setnint(cfg_t *cfg, cfg_opt_t *opt, int value, 
                     unsigned int index)
{
   cfg_value_t *val;

   cfg_assert(cfg && opt && opt->type == CFGT_INT);
   val = cfg_getval(opt, index);
   val->number = value;
}

void cfg_setnint(cfg_t *cfg, const char *name, int value,
                 unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
      cfg_opt_setnint(cfg, opt, value, index);
}

void cfg_setint(cfg_t *cfg, const char *name, int value)
{
   cfg_setnint(cfg, name, value, 0);
}

void cfg_opt_setnfloat(cfg_t *cfg, cfg_opt_t *opt, double value,
                       unsigned int index)
{
   cfg_value_t *val;

   cfg_assert(cfg && opt && opt->type == CFGT_FLOAT);
   val = cfg_getval(opt, index);
   val->fpnumber = value;
}

void cfg_setnfloat(cfg_t *cfg, const char *name, double value,
                   unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
      cfg_opt_setnfloat(cfg, opt, value, index);
}

void cfg_setfloat(cfg_t *cfg, const char *name, double value)
{
   cfg_setnfloat(cfg, name, value, 0);
}

void cfg_opt_setnbool(cfg_t *cfg, cfg_opt_t *opt, bool value,
                      unsigned int index)
{
   cfg_value_t *val;

   cfg_assert(cfg && opt && opt->type == CFGT_BOOL);
   val = cfg_getval(opt, index);
   val->boolean = value;
}

void cfg_setnbool(cfg_t *cfg, const char *name, bool value, 
                  unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
      cfg_opt_setnbool(cfg, opt, value, index);
}

void cfg_setbool(cfg_t *cfg, const char *name, bool value)
{
   cfg_setnbool(cfg, name, value, 0);
}

void cfg_opt_setnstr(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                     unsigned int index)
{
   cfg_value_t *val;

   cfg_assert(cfg && opt && opt->type == CFGT_STR);
   val = cfg_getval(opt, index);
   if(val->string) // haleyjd: !
      efree(val->string);
   val->string = value ? estrdup(value) : nullptr;
}

void cfg_setnstr(cfg_t *cfg, const char *name, const char *value,
                 unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
      cfg_opt_setnstr(cfg, opt, value, index);
}

void cfg_setstr(cfg_t *cfg, const char *name, const char *value)
{
   cfg_setnstr(cfg, name, value, 0);
}

static void cfg_addlist_internal(cfg_t *cfg, cfg_opt_t *opt,
                                 unsigned int nvalues, va_list ap)
{
   unsigned int i;

   for(i = 0; i < nvalues; ++i)
   {
      switch(opt->type)
      {
      case CFGT_INT:
         cfg_opt_setnint(cfg, opt, va_arg(ap, int), opt->nvalues);
         break;
      case CFGT_FLOAT:
         cfg_opt_setnfloat(cfg, opt, va_arg(ap, double), opt->nvalues);
         break;
      case CFGT_BOOL:
         cfg_opt_setnbool(cfg, opt, !!va_arg(ap, int), opt->nvalues);
         break;
      case CFGT_STR:
         cfg_opt_setnstr(cfg, opt, va_arg(ap, char *), opt->nvalues);
         break;
      case CFGT_FUNC:
      case CFGT_SEC:
      case CFGT_MVPROP:
      default:
         break;
      }
   }
}

void cfg_setlist(cfg_t *cfg, const char *name, unsigned int nvalues, ...)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
   {
      va_list ap;

      cfg_free_value(opt);
      va_start(ap, nvalues);
      cfg_addlist_internal(cfg, opt, nvalues, ap);
      va_end(ap);
   }
}

void cfg_addlist(cfg_t *cfg, const char *name, unsigned int nvalues, ...)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
   {
      va_list ap;

      va_start(ap, nvalues);
      cfg_addlist_internal(cfg, opt, nvalues, ap);
      va_end(ap);
   }
}

// haleyjd 04/03/08: added below to allow setting a list option with values taken
// from an array rather than varargs, since the latter do not work very well with
// libConfuse callback functions, which use an argc/argv system.

static void cfg_addlistptr_internal(cfg_t *cfg, cfg_opt_t *opt,
                                    unsigned int nvalues, const void *values)
{
   const cfg_type_t type = opt->type; // haleyjd: a little compiler sugar ;)
   unsigned int i;
   const int     *intptr    = nullptr;
   const double  *doubleptr = nullptr;
   const bool    *boolptr   = nullptr;
   const char   **strptr    = nullptr;

   switch(type)
   {
   case CFGT_INT:
      intptr = (const int *)values;
      break;
   case CFGT_FLOAT:
      doubleptr = (const double *)values;
      break;
   case CFGT_BOOL:
      boolptr = (const bool *)values;
      break;
   case CFGT_STR:
      strptr = (const char **)values;
      break;
   default:
      return;
   }

   for(i = 0; i < nvalues; ++i)
   {
      switch(type)
      {
      case CFGT_INT:
         cfg_opt_setnint(cfg, opt, *intptr, opt->nvalues);
         ++intptr;
         break;
      case CFGT_FLOAT:
         cfg_opt_setnfloat(cfg, opt, *doubleptr, opt->nvalues);
         ++doubleptr;
         break;
      case CFGT_BOOL:
         cfg_opt_setnbool(cfg, opt, *boolptr, opt->nvalues);
         ++boolptr;
         break;
      case CFGT_STR:
         cfg_opt_setnstr(cfg, opt, *strptr, opt->nvalues);
         ++strptr;
         break;
      default:
         break;
      }
   }
}

void cfg_setlistptr(cfg_t *cfg, const char *name, unsigned int nvalues, 
                    const void *valarray)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
   {
      cfg_free_value(opt);
      cfg_addlistptr_internal(cfg, opt, nvalues, valarray);
   }
}

// EOF

