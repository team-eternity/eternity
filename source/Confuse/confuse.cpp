/* Configuration file parser -*- tab-width: 4; -*-
 *
 * Copyright (c) 2002-2003, Martin Hedenfalk <mhe@home.se>
 * Modifications for Eternity Copyright (c) 2005 James Haley
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
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

// haleyjd 03/08/03: Modifications for Eternity
// #define PACKAGE_VERSION "Eternity version"
// #define PACKAGE_STRING  "libConfuse"

// haleyjd: removed ENABLE_NLS
#define _(str) str
#define N_(str) str

const char *confuse_version   = "Eternity version";
const char *confuse_copyright = "libConfuse by Martin Hedenfalk <mhe@home.se>";
const char *confuse_author    = "Martin Hedenfalk <mhe@home.se>";

#if defined(NDEBUG)
#define cfg_assert(test) ((void)0)
#else
#define cfg_assert(test) \
  ((void)((test)||(my_assert(#test, __FILE__, __LINE__),0)))
#endif

#define STATE_CONTINUE 0
#define STATE_EOF -1
#define STATE_ERROR 1

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
   
   if(s == 0)
      return 0;
   
   r = ecalloc(char *, 1, n + 1);
   strncpy(r, s, n);
   r[n] = 0;
   return r;
}

cfg_opt_t *cfg_getopt(cfg_t *cfg, const char *name)
{
   int i;
   cfg_t *sec = cfg;
   
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
         if(sec == 0)
            cfg_error(cfg, _("no such option '%s'\n"), secname);
         efree(secname);
         if(sec == 0)
            return 0;
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
   cfg_error(cfg, _("no such option '%s'\n"), name);
   return 0;
}

const char *cfg_title(cfg_t *cfg)
{
   return cfg->title;
}

unsigned int cfg_size(cfg_t *cfg, const char *name)
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

cfg_bool_t cfg_getnbool(cfg_t *cfg, const char *name, unsigned int index)
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
      return cfg_false;
}

cfg_bool_t cfg_getbool(cfg_t *cfg, const char *name)
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
   return 0;
}

const char *cfg_getstr(cfg_t *cfg, const char *name)
{
   return cfg_getnstr(cfg, name, 0);
}

char *cfg_getstrdup(cfg_t *cfg, const char *name)
{
   // haleyjd 12/31/11: get a dynamic copy of a string
   const char *value = cfg_getstr(cfg, name);

   return value ? estrdup(value) : NULL;
}

cfg_t *cfg_getnsec(cfg_t *cfg, const char *name, unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);
   
   if(opt) 
   {
      cfg_assert(opt->type == CFGT_SEC);
      cfg_assert(opt->values);
      cfg_assert(index < opt->nvalues);
      return opt->values[index]->section;
   }
   return 0;
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
   return 0;
}

cfg_t *cfg_getsec(cfg_t *cfg, const char *name)
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
   return 0;
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

cfg_value_t *cfg_setopt(cfg_t *cfg, cfg_opt_t *opt, char *value)
{
   cfg_value_t *val = 0;
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
         val = 0;
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
         if(val == 0)
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
            return 0;
         val->number = i;
      } 
      else 
      {
         // haleyjd 11/10/08: modified to print invalid values
         val->number = strtol(value, &endptr, 0);
         if(*endptr != '\0')
         {            
            cfg_error(cfg, _("invalid integer value '%s' for option '%s'\n"),
                      value, opt->name);
            return 0;
         }
         if(errno == ERANGE) 
         {
            cfg_error(cfg,
               _("integer value '%s' for option '%s' is out of range\n"),
               value, opt->name);
            return 0;
         }
      }
      break;
   case CFGT_FLOAT:
      if(opt->cb)
      {
         if((*opt->cb)(cfg, opt, value, &f) != 0)
            return 0;
         val->fpnumber = f;
      } 
      else 
      {
         val->fpnumber = strtod(value, &endptr);
         if(*endptr != '\0')
         {
            cfg_error(cfg,
               _("invalid floating point value for option '%s'\n"),
               opt->name);
            return 0;
         }
         if(errno == ERANGE)
         {
            cfg_error(cfg,
               _("floating point value for option '%s' is out of range\n"),
               opt->name);
            return 0;
         }
      }
      break;
   case CFGT_STR:
   case CFGT_STRFUNC: // haleyjd
      if(val->string)
         efree(val->string);
      if(opt->cb)
      {
         s = 0;
         if((*opt->cb)(cfg, opt, value, &s) != 0)
            return 0;
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
      val->section->title     = value;
      // haleyjd 01/02/12: make the old section a displaced version of the
      // new one, so that it can remain accessible
      val->section->displaced = oldsection;
      break;
   case CFGT_BOOL:
      if(opt->cb)
      {
         if((*opt->cb)(cfg, opt, value, &b) != 0)
            return 0;
      } 
      else
      {
         b = cfg_parse_boolean(value);
         if(b == -1)
         {
            cfg_error(cfg, _("invalid boolean value for option '%s'\n"),
               opt->name);
            return 0;
         }
      }
      val->boolean = (cfg_bool_t)b;
      break;
   case CFGT_FLAG: // haleyjd
      if(opt->cb)
      {
         if((*opt->cb)(cfg, opt, value, &i) != 0)
            return 0;
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
   
   if(opt == 0)
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
   opt->values = 0;
   opt->nvalues = 0;
}

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

void cfg_error(cfg_t *cfg, const char *fmt, ...)
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
};

static int cfg_parse_internal(cfg_t *cfg, int level)
{
   int state = STATE_EXPECT_OPTION, next_state = STATE_EXPECT_OPTION;
   char *opttitle = 0;
   cfg_opt_t *opt = 0;
   cfg_value_t *val;
   cfg_bool_t append_value;
   cfg_opt_t funcopt = CFG_STR(0,0,0);
   cfg_bool_t found_func = cfg_false; // haleyjd
   int tok;
   int skip_token = 0;     // haleyjd
   int propindex = 0;      // haleyjd: for multi-value properties
   
   while(1)
   {
      // haleyjd: possibly skip reading new tokens
      if(skip_token == 0)
         tok = mylex(cfg); // haleyjd: use custom lexer
      else
         --skip_token;
      
      if(tok == 0)
      {
         /* lexer.l should have called cfg_error */
         return STATE_ERROR;
      }
      
      if(tok == EOF)
      {
         if(state != STATE_EXPECT_OPTION)
         {
            // haleyjd: catch EOF while in lookfor state
            if(state == STATE_EXPECT_LOOKFOR) 
               cfg_error(cfg, "missing closing conditional function\n");
            else
               cfg_error(cfg, _("premature end of file\n"));
            return STATE_ERROR;
         }
         return STATE_EOF;
      }

      switch(state)
      {
      case STATE_EXPECT_OPTION: /* expecting an option name */
         if(tok == '}')
         {
            if(level == 0)
            {
               cfg_error(cfg, _("unexpected closing brace\n"));
               return STATE_ERROR;
            }
            return STATE_EOF;
         }
         if(tok != CFGT_STR)
         {
            cfg_error(cfg, _("unexpected token '%s'\n"), mytext);
            return STATE_ERROR;
         }         
         
         if((opt = cfg_getopt(cfg, mytext)) == 0) // haleyjd
            return STATE_ERROR;
         
         switch(opt->type)
         {
         case CFGT_SEC:
            if(is_set(CFGF_TITLE, opt->flags))
               state = STATE_EXPECT_TITLE;
            else
               state = STATE_EXPECT_SECBRACE;
            break;
         case CFGT_FUNC:
            state = STATE_EXPECT_PAREN;
            break;
         case CFGT_FLAG: // haleyjd: flag options, which are simple keywords
            if(cfg_setopt(cfg, opt, mytext) == 0)
               return STATE_ERROR;
            // remain in STATE_EXPECT_OPTION
            break;
         default:
            state = STATE_EXPECT_ASSIGN;
         }
         break;

      case STATE_EXPECT_ASSIGN: /* expecting an equal sign or plus-equal sign */
         append_value = cfg_false;
         if(tok == '+')
         {
            if(!is_set(CFGF_LIST, opt->flags))
            {
               cfg_error(cfg,
                  _("attempt to append to non-list option %s\n"),
                  opt->name);
               return STATE_ERROR;
            }
            append_value = cfg_true;
         } 
         else if(tok != '=')
         {
            // haleyjd 09/26/09: move forward anyway, and don't read a token.
            // This makes ='s optional!
            skip_token = 1;
         }

         if(opt->type == CFGT_MVPROP) // haleyjd
         {
            val = cfg_setopt(cfg, opt, opttitle);
            opttitle = 0;
            if(!val || !val->section || !val->section->opts)
               return STATE_ERROR;

            // start at opts[0]
            propindex = 0;

            // set the new option
            opt = &(val->section->opts[propindex++]);

            // must have at least one valid option
            cfg_assert(opt->type != CFGT_NONE);

            state = STATE_EXPECT_VALUE;
            next_state = STATE_EXPECT_PROPNEXT;
         }
         else if(is_set(CFGF_LIST, opt->flags))
         {
            if(!append_value)
               cfg_free_value(opt);
            state = STATE_EXPECT_LISTBRACE;
         }
         else
         {
            state = STATE_EXPECT_VALUE;
            next_state = STATE_EXPECT_OPTION;
         }
         break;

      case STATE_EXPECT_VALUE: /* expecting an option value */
         if(tok == '}' && is_set(CFGF_LIST, opt->flags))
         {
            lexer_set_unquoted_spaces(cfg_false); /* haleyjd */
            state = 0;
            break;
         }
         
         if(tok != CFGT_STR)
         {
            cfg_error(cfg, _("unexpected token '%s'\n"), mytext);
            return STATE_ERROR;
         }

         // haleyjd 04/03/08: function-valued options: the name of the
         // function being called will become the value of the option.
         // The arguments are provided to the callback function as usual.
         if(opt->type == CFGT_STRFUNC)
            next_state = STATE_EXPECT_PAREN;
         
         if(cfg_setopt(cfg, opt, mytext) == 0)
            return STATE_ERROR;

         state = next_state;
         break;

      case STATE_EXPECT_LISTBRACE: /* expecting an opening brace for a list option */
         if(tok != '{')
         {
            cfg_error(cfg, _("missing opening brace for option '%s'\n"),
               opt->name);
            return STATE_ERROR;
         }
         /* haleyjd 12/23/06: set unquoted string state */
         if(is_set(CFGF_STRSPACE, opt->flags))
            lexer_set_unquoted_spaces(cfg_true);
         state = STATE_EXPECT_VALUE;
         next_state = STATE_EXPECT_LISTNEXT;
         break;
         
      case STATE_EXPECT_LISTNEXT: /* expecting a separator for a list option, or
                                   * closing (list) brace */
         if(tok == ',')
         {
            state = STATE_EXPECT_VALUE;
            next_state = STATE_EXPECT_LISTNEXT;
         } 
         else if(tok == '}')
         {
            lexer_set_unquoted_spaces(cfg_false); /* haleyjd */
            state = STATE_EXPECT_OPTION;
         }
         else
         {
            cfg_error(cfg, _("unexpected token '%s'\n"), mytext);
            return STATE_ERROR;
         }
         break;

      case STATE_EXPECT_SECBRACE: /* expecting an opening brace for a section */
         if(tok != '{')
         {
            cfg_error(cfg, _("missing opening brace for section '%s'\n"),
               opt->name);
            return STATE_ERROR;
         }
         
         val = cfg_setopt(cfg, opt, opttitle);
         opttitle = 0;
         if(!val)
            return STATE_ERROR;
         
         if(cfg_parse_internal(val->section, level+1) != STATE_EOF)
            return STATE_ERROR;
         cfg->line = val->section->line;
         state = STATE_EXPECT_OPTION;
         break;

      case STATE_EXPECT_TITLE: /* expecting a title for a section */
         if(tok != CFGT_STR)
         {
            cfg_error(cfg, _("missing title for section '%s'\n"),
                      opt->name);
            return STATE_ERROR;
         }
         else
            opttitle = estrdup(mytext);
         state = STATE_EXPECT_SECBRACE;
         break;

      case STATE_EXPECT_PAREN: /* expecting a opening parenthesis for a function */
         if(tok != '(')
         {
            // haleyjd: function-valued options may have no parameter list.
            // If so, however, we must skip reading the next token of input,
            // since we already ate it.
            if(opt->type == CFGT_STRFUNC)
            {
               int ret = call_function(cfg, opt, &funcopt);
               if(ret != 0)
                  return STATE_ERROR;

               state = STATE_EXPECT_OPTION;
               skip_token = 1; // reprocess the current token
               break;
            }
            else
            {
               cfg_error(cfg, _("missing parenthesis for function '%s'\n"),
                         opt->name);
               return STATE_ERROR;
            }
         }
         state = STATE_EXPECT_ARGUMENT;
         break;

      case STATE_EXPECT_ARGUMENT: /* expecting a function parameter or closing paren*/
         if(tok == ')')
         {
            int ret = call_function(cfg, opt, &funcopt);
            if(ret != 0)
               return STATE_ERROR;
            // haleyjd 09/30/05: check for LOOKFORFUNC flag
            if(is_set(CFGF_LOOKFORFUNC, cfg->flags))
            {
               found_func = cfg_false;
               state = STATE_EXPECT_LOOKFOR; // go to new "lookfor" state
            }
            else
               state = STATE_EXPECT_OPTION;
         }
         else if(tok == CFGT_STR)
         {
            val = cfg_addval(&funcopt);
            val->string = estrdup(mytext);
            state = STATE_EXPECT_ARGNEXT;
         } 
         else 
         {
            cfg_error(cfg, _("syntax error in call of function '%s'\n"),
                      opt->name);
            return STATE_ERROR;
         }
         break;

      case STATE_EXPECT_ARGNEXT: /* expecting a comma in a function or a closing paren */
         if(tok == ')')
         {
            int ret = call_function(cfg, opt, &funcopt);
            if(ret != 0)
               return STATE_ERROR;
            // haleyjd 01/14/04: check for LOOKFORFUNC flag
            if(is_set(CFGF_LOOKFORFUNC, cfg->flags))
            {
               found_func = cfg_false;
               state = STATE_EXPECT_LOOKFOR; // go to new "lookfor" state
            }
            else
               state = STATE_EXPECT_OPTION;
         }
         else if(tok == ',')
         {
            state = STATE_EXPECT_ARGUMENT;
         }
         else 
         {
            cfg_error(cfg, _("syntax error in call of function '%s'\n"),
                      opt->name);
            return STATE_ERROR;
         }
         break;

      case STATE_EXPECT_LOOKFOR:
         // haleyjd 01/16/04: special state which looks for the
         // "lookfor" function, allowing ifdef-type stuff which
         // ignores everything until the function is encountered
         if(is_set(CFGF_NOCASE, cfg->flags))
         {
            if(!strcasecmp(mytext, cfg->lookfor))
               found_func = cfg_true;
         }
         else
         {
            if(!strcmp(mytext, cfg->lookfor))
               found_func = cfg_true;
         }
         if(found_func == cfg_true)
         {
            opt = cfg_getopt(cfg, mytext);
            if(opt == 0)
               return STATE_ERROR;
            if(opt->type == CFGT_FUNC)
               state = STATE_EXPECT_PAREN; // parse the function call
            else
            {
               cfg_error(cfg, "internal error\n");
               return STATE_ERROR;
            }
         }
         break;

      case STATE_EXPECT_PROPNEXT:
         // haleyjd 09/26/09: step to next value for a multi-value property
         if(tok == ',')
         {
            // step to next option in the section
            opt = &(val->section->opts[propindex++]);

            if(opt->type == CFGT_NONE) // reached CFG_END?
               state = STATE_EXPECT_OPTION;
            else
            {
               state = STATE_EXPECT_VALUE;
               next_state = STATE_EXPECT_PROPNEXT;
            }
         } 
         else
         {
            skip_token = 1; // reprocess the current token
            state = STATE_EXPECT_OPTION;
         }
         break;

      default:
         /* missing state, internal error, abort */
         cfg_assert(0);
      }
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
   cfg->filename = 0;
   cfg->line     = 0;
   cfg->lumpnum  = -1;   // haleyjd
   cfg->errfunc  = 0;
   cfg->lexfunc  = 0;    // haleyjd
   cfg->lookfor  = NULL; // haleyjd

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
   if(cfg->filename == 0)
   {
      cfg_error(cfg, _("%s: can't expand home directory\n"), filename);
      return CFG_FILE_ERROR;
   }

   cfg->line = 1;

   cfg->lumpnum = file->lumpnum;

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
   int ret;

   D_OpenFile(&dwfile, filename, "rb");
   
   if(!D_IsOpen(&dwfile))
      return CFG_FILE_ERROR;

   ret = cfg_parse_dwfile(cfg, filename, &dwfile);

   // haleyjd: wow, should probably close the file huh?
   D_Fclose(&dwfile);

   return ret;
}

// 
// cfg_parselump
// 
// haleyjd 04/03/03: allow input from WAD lumps
//
int cfg_parselump(cfg_t *cfg, const char *lumpname, int lumpnum)
{
   DWFILE dwfile; // haleyjd
   int ret; // haleyjd

   D_OpenLump(&dwfile, lumpnum);
   
   if(!D_IsOpen(&dwfile))
      return CFG_FILE_ERROR;

   ret = cfg_parse_dwfile(cfg, lumpname, &dwfile);

   // haleyjd: wow, should probably close the file huh?
   D_Fclose(&dwfile);

   return ret;
}

void cfg_free(cfg_t *cfg)
{
   int i;
   
   if(cfg == 0)
      return;

   // haleyjd 01/02/12: free any displaced section(s) recursively
   if(cfg->displaced)
   {
      cfg_free(cfg->displaced);
      cfg->displaced = 0;
   }
   
   for(i = 0; cfg->opts[i].name; i++)
      cfg_free_value(&cfg->opts[i]);

   if(is_set(CFGF_ALLOCATED, cfg->flags))
   {
      efree(cfg->namealloc);
      efree(cfg->opts);
      efree(cfg->title);
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
      cfg_error(cfg, _("wrong number of arguments to cfg_include()\n"));
      return 1;
   }

   if(!(data = cfg_lexer_mustopen(cfg, argv[0], -1, NULL)))
      return 1;

   return cfg_lexer_include(cfg, data, argv[0], -1);
}

// haleyjd 04/03/08: added cfg_t value-setting functions from libConfuse 2.0

static cfg_value_t *cfg_getval(cfg_opt_t *opt, unsigned int index)
{
   cfg_value_t *val = 0;

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

void cfg_opt_setnbool(cfg_t *cfg, cfg_opt_t *opt, cfg_bool_t value,
                      unsigned int index)
{
   cfg_value_t *val;

   cfg_assert(cfg && opt && opt->type == CFGT_BOOL);
   val = cfg_getval(opt, index);
   val->boolean = value;
}

void cfg_setnbool(cfg_t *cfg, const char *name, cfg_bool_t value, 
                  unsigned int index)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
      cfg_opt_setnbool(cfg, opt, value, index);
}

void cfg_setbool(cfg_t *cfg, const char *name, cfg_bool_t value)
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
   val->string = value ? estrdup(value) : 0;
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
         cfg_opt_setnbool(cfg, opt, (cfg_bool_t)va_arg(ap, int), opt->nvalues);
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
                                    unsigned int nvalues, void *array)
{
   const cfg_type_t type = opt->type; // haleyjd: a little compiler sugar ;)
   unsigned int i;
   int        *intptr    = 0;
   double     *doubleptr = 0;
   cfg_bool_t *boolptr   = 0;
   const char **strptr   = 0;

   switch(type)
   {
   case CFGT_INT:
      intptr = (int *)array;
      break;
   case CFGT_FLOAT:
      doubleptr = (double *)array;
      break;
   case CFGT_BOOL:
      boolptr = (cfg_bool_t *)array;
      break;
   case CFGT_STR:
      strptr = (const char **)array;
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
                    void *array)
{
   cfg_opt_t *opt = cfg_getopt(cfg, name);

   if(opt)
   {
      cfg_free_value(opt);
      cfg_addlistptr_internal(cfg, opt, nvalues, array);
   }
}

// EOF

