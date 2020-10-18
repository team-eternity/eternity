// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// EDF DECORATE States Implementation
//
// 100% original GPL code by James Haley
//
// I have implemented DECORATE states into EDF for several reasons. First and
// foremost is that their compact syntax is vastly superior even to EDF cmp
// frames. Second, they offer improved encapsulation of states within the
// objects that use them. Third, they will be useful for weapons and 
// inventory definitions as well as mobjinfo.
//
// And finally, they offer a vastly simpler method of both adding and removing
// metastates from objects.
//
//----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomtype.h"
#include "e_actions.h"
#include "e_lib.h"
#include "e_edf.h"
#include "e_args.h"
#include "e_sprite.h"
#include "e_states.h"
#include "e_dstate.h"
#include "m_dllist.h"
#include "m_qstr.h"
#include "p_pspr.h"

//==============================================================================
//
// DECORATE State Management
//
// haleyjd 04/03/10: This code manages the creation of DECORATE state sets and
// is used by the parser below.
//

// enumeration for estatebuf types
enum
{
   BUF_LABEL,
   BUF_STATE,
   BUF_KEYWORD,
   BUF_GOTO
};

//
// estatebuf
//
// This is a temporary structure used to cache data about DECORATE state
// sequences.
//
struct estatebuf_t
{
   DLListItem<estatebuf_t> links; // linked list links

   int type;            // type of entry (label, state, etc.)
   int linenum;         // origin line number
   char *name;          // name when needed

   // goto-specific info
   char *gotodest;      // goto destination
   int  gotooffset;     // goto offset
};

//
// internal goto relocation list
//
// gotos which are not able to be relocated within this module will be sent up
// to the caller in the DSO's goto list, where they can be checked for super
// state references or references to inherited or native state names
//
struct internalgoto_t
{
   estatebuf_t *gotoInfo; // buffered state object with goto information
   int state;             // index into states[] that was assigned to this goto
};

//
// Parser state globals
//
struct edecparser_t
{
   // buffered states - accumulated in the principals pass and
   // iterated over during the main pass to drive generation
   DLListItem<estatebuf_t> *statebuffer; // temporary data cache
   DLListItem<estatebuf_t> *curbufstate; // current state during main pass

   estatebuf_t *neweststate; // newest buffered state generated
   

   // counts - incremented during the principals pass and used
   // to pre-allocate a sufficient number of entities for the main pass
   int numdeclabels;   // number of labels counted
   int numdecstates;   // number of states counted
   int numkeywords;    // number of keywords counted
   int numgotos;       // number of goto's counted
   int numgotostates;  // number of goto's that are also independent states
   int numstops;       // number of states being removed by stop keywords

   // state_t indices - these are relative to states[].
   // only valid during the main pass.
   int firststate;     // index of first state_t added to states[]
   int currentstate;   // index of current state_t being processed
   int lastlabelstate; // index of last state_t assigned to a label

   // DSO - Decorate State Output object which accretes all data that must
   // be returned to the caller of E_ParseDecorateStates.
   edecstateout_t *pDSO; // output object for main pass

   // Internal goto table - tracks gotos for internal resolution. Any gotos
   // not resolvable here are returned as DSO gotos for external resolution.
   internalgoto_t *internalgotos; // array of goto entries
   int numinternalgotos;          // current number of gotos
   int numinternalgotosalloc;     // number of gotos allocated
};

// The one and only Decorate State Parser object
static edecparser_t DSP;

//
// E_AddBufferedState
//
// Caches data in the statebuffer list.
//
static void E_AddBufferedState(int type, const char *name, int linenum)
{
   estatebuf_t *newbuf = estructalloc(estatebuf_t, 1);

   newbuf->type = type;

   if(name)
      newbuf->name = estrdup(name);

   newbuf->linenum = linenum;

   // use tail insertion to keep objects in order generated
   newbuf->links.insert(newbuf,
                        DSP.neweststate ? &(DSP.neweststate->links.dllNext) :
                                          &(DSP.statebuffer));

   DSP.neweststate = newbuf;
}

//==============================================================================
//
// DECORATE State Parser
//
// haleyjd 06/16/09: EDF now supports DECORATE-format state definitions inside
// thingtype "states" fields, which accept heredoc strings. The processing
// of those strings is done here, where they are turned into states.
//

/*
  Grammar

  <labeledunit> := <labelblock><frameblock><labeledunit> | nil
    <labelblock> := <label><eol><labelblock>
      <label> := [A-Za-z0-9_]+('.'[A-Za-z0-9_]+)?':'
      <eol>   := '\n'
    <frameblock> := <frame><frameblock> | <frame>
      <frame> := <keyword><eol> | <frame_token_list><eol>
        <keyword> := "stop" | "wait" | "loop" | "goto" <jumplabel>
          <jumplabel> := <jlabel> | <jlabel> '+' number
            <jlabel> := [A-Za-z0-9_:]+('.'[A-Za-z0-9_]+)?
        <frame_token_list> := <sprite><frameletters><tics><flagslist><action>
          <sprite> := [A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]
          <frameletters> := [A-Z\[\\\]]+
          <tics> := [0-9]+
          <flagslist> := <flag><flagslist>
            <flag> := "bright" | "fast" | "offset" '(' <x> ',' <y> [',' "interpolate"] ')' | nil
          <action> := <name>
                    | <name> '(' <arglist> ')'
                    | nil
            <name> := [A-Za-z0-9_]+
            <arglist> := <arg> ',' <arglist> | <arg> | nil
              <arg> := "string" | number
*/

/*
   State Transition Diagram
   ----------------------------------
   NEEDLABEL: 
      <label> : NEEDLABELORKWORSTATE
      EOL     : loop
   NEEDLABELORKWORSTATE:
      <label>   : NEEDLABELORKWORSTATE
      "goto"    : NEEDGOTOLABEL
      <keyword> : NEEDKWEOL
      <text>    : NEEDSTATEFRAMES
      EOL       : loop ***
   NEEDGOTOLABEL:
      <label> : NEEDGOTOEOLORPLUS
   NEEDGOTOEOLORPLUS:
      '+' : NEEDGOTOOFFSET
      EOL : NEEDLABEL
   NEEDGOTOOFFSET:
      <number> : NEEDKWEOL
   NEEDKWEOL:
      EOL : NEEDLABEL
   NEEDSTATEFRAMES:
      <text> : NEEDSTATETICS
   NEEDSTATETICS:
      <number> : NEEDFLAGORACTION
   NEEDFLAGORACTION:
      "bright" : NEEDFLAGORACTION
      "fast"   : NEEDFLAGORACTION
      "offset" '(' <number> ',' <number> ')' : NEEDFLAGORACTION
      <text>   : NEEDSTATEEOLORPAREN
      EOL      : NEEDLABELORKWORSTATE
   NEEDSTATEEOLORPAREN:
      EOL : NEEDLABELORKWORSTATE
      '(' : NEEDSTATEARGORPAREN
   NEEDSTATEARGORPAREN:
      <number> | <string> : NEEDSTATECOMMAORPAREN
      ')' : NEEDSTATEEOL
   NEEDSTATECOMMAORPAREN:
      ',' : NEEDSTATEARG
      ')' : NEEDSTATEEOL
   NEEDSTATEARG:
      <number> | <string> : NEEDSTATECOMMAORPAREN
   NEEDSTATEEOL:
      EOL : NEEDLABELORKWORSTATE

  *** EOL is allowed in PSTATE_NEEDLABELORKWORSTATE in a manner more flexible than
      that specified by the above grammar for matters of convenience.
*/

// parser state enumeration
enum
{
   PSTATE_NEEDLABEL,             // starting state, first token must be a label
   PSTATE_NEEDLABELORKWORSTATE,  // after label, need label or keyword or state
   PSTATE_NEEDGOTOLABEL,         // after goto, need goto label
   PSTATE_NEEDGOTOEOLORPLUS,     // after goto label, need EOL or '+'
   PSTATE_NEEDGOTOOFFSET,        // after '+', need goto offset number
   PSTATE_NEEDKWEOL,             // need kw EOL
   PSTATE_NEEDSTATEFRAMES,       // after state sprite, need frames
   PSTATE_NEEDSTATETICS,         // after state frames, need tics
   PSTATE_NEEDFLAGORACTION,      // after tics, need "bright" or action name
   PSTATE_NEEDSTATEEOLORPAREN,   // after action name, need EOL or '('
   PSTATE_NEEDSTATEARGORPAREN,   // after '(', need argument or ')'
   PSTATE_NEEDSTATECOMMAORPAREN, // after argument, need ',' or ')'
   PSTATE_NEEDSTATEARG,          // after ',', need argument (loop state)
   PSTATE_NEEDSTATEEOL,          // after ')', need EOL
   PSTATE_NUMSTATES
};

// parser state structure
struct pstate_t
{
   int state; // state of the parser, as defined by the above enumeration
   qstring *linebuffer;  // qstring to use as line buffer
   qstring *tokenbuffer; // qstring to use as token buffer
   int index;            // current index into line buffer for tokenization
   int linenum;          // line number (relative to start of state block)
   bool needline;        // if true, feed a line from the input
   bool principals;      // parsing for principals only
   bool error;           // if true, an error has occurred.

   int tokentype;        // current token type, once decided upon
   int tokenerror;       // current token error code
};

// tokenization

// token types
enum
{
   TOKEN_LABEL,   // [A-Za-z0-9_]+('.'[A-Za-z0-9_]+)?':'
   TOKEN_KEYWORD, // loop, stop, wait, goto
   TOKEN_PLUS,    // '+'
   TOKEN_LPAREN,  // '('
   TOKEN_COMMA,   // ','
   TOKEN_RPAREN,  // ')'
   TOKEN_TEXT,    // anything else (numbers, strings, words, etc.)
   TOKEN_EOL,     // end of line
   TOKEN_ERROR,   // an error token
   TOKEN_NUMTOKENS
};

// token type names for display in error messages
static const char *ds_token_names[TOKEN_NUMTOKENS] =
{
   "label",
   "keyword",
   "character",
   "character",
   "character",
   "character",
   "text token",
   "end of line",
   "error"        // not actually displayed; see below
};

// token errors
enum
{
   TERR_NONE,           // not an error
   TERR_BADSTRING,      // malformed string constant
   TERR_UNEXPECTEDCHAR, // weird character
   TERR_NUMERRORS
};

// token error strings
static const char *ds_token_errors[TERR_NUMERRORS] =
{
   "not an error",
   "unterminated string constant",
   "unexpected character"
};

// tokenizer states
enum
{
   TSTATE_SCAN,    // scanning for start of a token
   TSTATE_SLASH,   // scanning after a '/'
   TSTATE_COMMENT, // consume up to next "\n"
   TSTATE_TEXT,    // scanning in a label, keyword, or text token
   TSTATE_COLON,   // scanning after ':'
   TSTATE_LABEL,   // scanning after '.' in a label
   TSTATE_STRING,  // scanning in a string literal
   TSTATE_DONE     // finished; return token to parser
};

// tokenizer state structure
struct tkstate_t
{
   int state;        // current state of tokenizer
   int i;            // index into string
   int tokentype;    // token type, once decided upon
   int tokenerror;   // token error code
   qstring *line;    // line text
   qstring *token;   // token text
};

//=============================================================================
//
// Tokenizer Callbacks
//

//
// DoTokenStateScan
//
// Scanning for the start of a token.
//
static void DoTokenStateScan(tkstate_t *tks)
{
   const char *str = tks->line->constPtr();
   int i           = tks->i;
   qstring *token  = tks->token;

   // allow A-Za-z0-9, underscore, and leading - for numbers
   if(ectype::isAlnum(str[i]) || str[i] == '_' || str[i] == '-')
   {
      // start a text token - we'll determine the more specific type, if any,
      // later.
      *token += str[i];
      tks->tokentype = TOKEN_TEXT;
      tks->state     = TSTATE_TEXT;
   }
   else
   {
      switch(str[i])
      {
      case ' ':
      case '\t': // whitespace
         break;  // keep scanning
      case '\0': // end of line, which is significant in DECORATE
         tks->tokentype = TOKEN_EOL;
         tks->state     = TSTATE_DONE;
         break;
      case '"':  // quoted string
         tks->tokentype = TOKEN_TEXT;
         tks->state     = TSTATE_STRING;
         break;
      case '+':  // plus - used in relative goto statement
         *token += '+';
         tks->tokentype = TOKEN_PLUS;
         tks->state     = TSTATE_DONE;
         break;
      case '(':  // lparen - for action argument lists
         *token += '(';
         tks->tokentype = TOKEN_LPAREN;
         tks->state     = TSTATE_DONE;
         break;
      case ',':  // comma - for action argument lists
         *token += ',';
         tks->tokentype = TOKEN_COMMA;
         tks->state     = TSTATE_DONE;
         break;
      case ')':  // rparen - for action argument lists
         *token += ')';
         tks->tokentype = TOKEN_RPAREN;
         tks->state     = TSTATE_DONE;
         break;
      case '/':  // forward slash - start of single-line comment?
         tks->state     = TSTATE_SLASH;
         break;
      default: // whatever it is, we don't care for it.
         *token += str[i];
         tks->tokentype  = TOKEN_ERROR;
         tks->tokenerror = TERR_UNEXPECTEDCHAR;
         tks->state      = TSTATE_DONE;
         break;
      }
   }
}

//
// DoTokenStateSlash
//
static void DoTokenStateSlash(tkstate_t *tks)
{
   const char *str = tks->line->constPtr();
   int i           = tks->i;
   qstring *token  = tks->token;

   if(str[i] == '/')
      tks->state = TSTATE_COMMENT;
   else
   {
      *token += str[i];
      tks->tokentype  = TOKEN_ERROR;
      tks->tokenerror = TERR_UNEXPECTEDCHAR;
      tks->state      = TSTATE_DONE;
   }
}

//
// DoTokenStateComment
//
static void DoTokenStateComment(tkstate_t *tks)
{
   const char *str  = tks->line->constPtr();
   int i            = tks->i;

   // eol?
   if(str[i] == '\0')
   {
      tks->tokentype = TOKEN_EOL;
      tks->state     = TSTATE_DONE;
   }
   // else keep consuming characters
}

// there are only four keywords, so hashing is pointless.
static const char *decorate_kwds[] =
{
   "loop",
   "wait",
   "stop",
   "goto",
};

enum
{
   KWD_LOOP,
   KWD_WAIT,
   KWD_STOP,
   KWD_GOTO
};

#define NUMDECKWDS (sizeof(decorate_kwds) / sizeof(const char *))

//
// isDecorateKeyword
//
// Returns true if the string matches a DECORATE state keyword, and false
// otherwise.
//
static bool isDecorateKeyword(const char *text)
{
   return (E_StrToNumLinear(decorate_kwds, NUMDECKWDS, text) != NUMDECKWDS);
}

//
// DoTokenStateText
//
// At this point we are in a label, a keyword, or a plain text token,
// but we don't know which yet.
//
static void DoTokenStateText(tkstate_t *tks)
{
   const char *str = tks->line->constPtr();
   int i           = tks->i;
   qstring *token  = tks->token;

   if(ectype::isAlnum(str[i]) || str[i] == '_')
   {
      // continuing in label, keyword, or text
      *token += str[i];
   }
   else if(str[i] == '.')
   {
      char *endpos = nullptr;

      // we see a '.' which could either be the decimal point in a float
      // value, or the dot name separator in a label. If the token is only
      // numbers up to this point, we will consider it a number, but the
      // parser can sort this out if the wrong token type appears when it
      // is expecting TOKEN_LABEL.
      
      token->toLong(&endpos, 10);
      
      if(*endpos != '\0')
      {      
         // it's not just a number, so assume it's a label
         tks->tokentype = TOKEN_LABEL;
         tks->state     = TSTATE_LABEL;
      }
      // otherwise, proceed as normal
      
      // add the dot character to the token
      *token += '.';
   }
   else if(str[i] == ':')
   {
      // colon either means:
      // A. this is a label, and we are at the end of it.
      // B. this is a namespace declarator in a goto label - next char is ':'
      // We must use another state to determine which is which.
      tks->state = TSTATE_COLON;
   }
   else // anything else ends this token, and steps back
   {
      // see if it is a keyword
      if(isDecorateKeyword(token->constPtr()))
         tks->tokentype = TOKEN_KEYWORD;
      tks->state = TSTATE_DONE;
      --tks->i; // the char we're on is the start of a new token, so back up.
   }
}

//
// DoTokenStateColon
//
// When a ':' is found in a text token we must defer the decision on whether
// to call that the end of the token or not until we see the next character.
//
static void DoTokenStateColon(tkstate_t *tks)
{
   const char *str = tks->line->constPtr();
   int i           = tks->i;
   qstring *token  = tks->token;

   if(str[i] == ':')
   {
      // Two colons in a row means we've found the namespace declarator.
      // Add two colons to the token buffer, and return to TSTATE_TEXT to
      // parse out the rest of the token.
      token->concat("::");
      tks->state = TSTATE_TEXT;
   }
   else // anything else means this was a label
   {
      tks->tokentype = TOKEN_LABEL;
      tks->state     = TSTATE_DONE;
      --tks->i; // the char we're on is the start of a new token, so back up
   }
}

//
// DoTokenStateLabel
//
// Scans inside a label after encountering a dot, which is used for custom
// death and pain state specification.
//
static void DoTokenStateLabel(tkstate_t *tks)
{
   const char *str = tks->line->constPtr();
   int i           = tks->i;
   qstring *token  = tks->token;

   if(ectype::isAlnum(str[i]) || str[i] == '_')
   {
      // continuing in label
      *token += str[i];
   }
   else if(str[i] == ':')
   {
      // colon at the end means this is a label, and we are at the end of it.
      tks->state = TSTATE_DONE;
   }
   else
   {
      // anything else means we may have been mistaken about this being a 
      // label after all, or this is a label in the context of a goto statement;
      // in that case, we mark the token as text type and the parser can sort 
      // out the damage by watching out for it.
      tks->tokentype = TOKEN_TEXT;
      tks->state     = TSTATE_DONE;
      --tks->i; // back up one char since we ate something unrelated
   }
}

//
// DoTokenStateString
//
// Parsing inside a string literal.
//
static void DoTokenStateString(tkstate_t *tks)
{
   const char *str = tks->line->constPtr();
   int i           = tks->i;
   qstring *token  = tks->token;

   // note: DECORATE does not support escapes in strings?!?
   switch(str[i])
   {
   case '"': // end of string
      tks->tokentype = TOKEN_TEXT;
      tks->state     = TSTATE_DONE;
      break;
   case '\0': // EOL - error
      tks->tokentype  = TOKEN_ERROR;
      tks->tokenerror = TERR_BADSTRING;
      tks->state      = TSTATE_DONE;
      break;
   default:
      // add character and continue scanning
      *token += str[i];
      break;
   }
}

// tokenizer callback type
typedef void (*tksfunc_t)(tkstate_t *);

// tokenizer state function table
static tksfunc_t tstatefuncs[] =
{
   DoTokenStateScan,    // scanning for start of a token
   DoTokenStateSlash,   // scanning inside a single-line comment?
   DoTokenStateComment, // scanning inside a comment; consume to next EOL
   DoTokenStateText,    // scanning inside a label, keyword, or text
   DoTokenStateColon,   // scanning after a ':' encountered in a text token
   DoTokenStateLabel,   // scanning inside a label after a dot
   DoTokenStateString,  // scanning inside a string literal
};

//
// E_GetDSToken
//
// Gets the next token from the DECORATE states string.
//
static int E_GetDSToken(pstate_t *ps)
{
   tkstate_t tks;

   // set up tokenizer state - transfer in parser state details
   tks.state      = TSTATE_SCAN;
   tks.tokentype  = -1;
   tks.tokenerror = TERR_NONE;
   tks.i          = ps->index;
   tks.line       = ps->linebuffer;
   tks.token      = ps->tokenbuffer;

   tks.token->clear();

   while(tks.state != TSTATE_DONE)
   {
      I_Assert(tks.state >= 0 && tks.state < TSTATE_DONE,
               "E_GetDSToken: Internal error: undefined state\n");

      tstatefuncs[tks.state](&tks);

      // step to next character
      ++tks.i;
   }

   // write back info to parser state
   ps->index      = tks.i;
   ps->tokentype  = tks.tokentype;
   ps->tokenerror = tks.tokenerror;

   return tks.tokentype;
}

//=============================================================================
//
// Parser Callbacks
//

// utilities

// for unnecessarily precise grammatical correctness ;)
#define hasvowel(str) \
   (*str == 'a' || *str == 'e' || *str == 'i' || *str == 'o' || *str == 'u')

//
// PSExpectedErr
//
// Called to issue a verbose log message indicating an error consisting of an
// unexpected token type.
//
static void PSExpectedErr(pstate_t *ps, const char *expected)
{
   const char *tokenname = ds_token_names[ps->tokentype];

   // if error is due to a tokenizer error, get the more specific error message
   // pertaining to that malformed token
   if(ps->tokenerror)
      tokenname = ds_token_errors[ps->tokenerror];

   E_EDFLoggedWarning(2, 
                      "Error on line %d of DECORATE state block:\n\t\t"
                      "Expected %s %s but found %s %s with value '%s'\n",
                      ps->linenum,
                      hasvowel(expected)  ? "an" : "a", expected,
                      hasvowel(tokenname) ? "an" : "a", tokenname,
                      ps->tokentype != TOKEN_EOL ? ps->tokenbuffer->constPtr() : "\\n");
   
   ps->error = true;
}

//
// PSWrapStates
//
// haleyjd 06/23/10: shared logic to handle the end of a set of one or more 
// DECORATE state definitions. This is invoked from the following states:
// * PSTATE_NEEDFLAGORACTION
// * PSTATE_NEEDSTATEEOLORPAREN
// * PSTATE_NEEDSTATEEOL
//
static void PSWrapStates(pstate_t *ps)
{
   if(!ps->principals)
   {
      // Increment curbufstate and currentstate until end of list is reached,
      // a non-state object is encountered, or the line number changes
      DLListItem<estatebuf_t> *link = DSP.curbufstate;
      int curlinenum = (*link)->linenum;

      while(link && (*link)->linenum == curlinenum && 
            (*link)->type == BUF_STATE)
      {
         link = link->dllNext;
         DSP.currentstate++;
      }
      DSP.curbufstate = link;
   }
}

//
// DoPSNeedLabel
//
// Initial state of the parser; we expect a state label, but we'll accept
// EOL, which means to loop.
//
static void DoPSNeedLabel(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      // loop until something meaningful appears
      break;

   case TOKEN_LABEL:
      // record label to associate with next state generated
      if(ps->principals) 
      {
         // count a label
         DSP.numdeclabels++; 
         // make a label object
         E_AddBufferedState(BUF_LABEL, ps->tokenbuffer->constPtr(), ps->linenum);
      }
      // Outside of principals, we don't do anything here. When we hit a 
      // non-label parsing state, we will run down the labels until we reach
      // the first non-label buffered object and create output labels for all
      // of them.
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;

   default:
      // anything else is an error
      PSExpectedErr(ps, "label");
      break;
   }
}

//
// Service routines for DoPSNeedLabelOrKWOrState
//

//
// doLabel
//
// A label token has been encountered. During parsing for principals, we create
// a buffered state object that stores the label token. During the main pass,
// nothing is done, because we sit with curbufstate pointing to the first label
// we've seen until we hit a state or keyword. At that point we process down
// the list of estatebuf objects until a non-label is hit.
//
static void doLabel(pstate_t *ps)
{
   // record label to associate with next state generated.
   // remain in the current state
   if(ps->principals)
   {
      // count a label
      DSP.numdeclabels++;
      // make a label object
      E_AddBufferedState(BUF_LABEL, ps->tokenbuffer->constPtr(), ps->linenum);
   }
   // Again, just keep going outside of principals.
}

//
// doGoto
//
// Gotos are ridiculously complicated:
//
// * During principals, we count it and generate a buffered goto object.
//   In addition, if this goto immediately follows a label, we must count
//   it specially because it will generate an invisible state that 
//   immediately transfers to the goto's destination label.
//
// * During the main pass, if we are sitting with curbufstate on a label,
//   we know we're dealing with such a state-generating goto, and we
//   reserve one of our previously allocated state_t's for use with it.
//   Otherwise, this goto record is meant to modify the nextstate of the
//   previous state_t that was processed. Note the previous state_t processed
//   while dealing with a keyword is *always* currentstate - 1.
//   
// * Because label resolution is not complete, we must store ALL gotos into
//   the temporary "internalgotos" list. This list is run down after the
//   main parsing pass. Any gotos that can be resolved to labels in the
//   current scope will be resolved at that point. Any that cannot are passed
//   back to the caller within the DSO object.
//
static void doGoto(pstate_t *ps)
{
   // handle goto specifics
   if(ps->principals)
   {
      // count a goto
      DSP.numgotos++;

      // also counts as a keyword
      DSP.numkeywords++;

      // if the previous buffered state is a label, this is a state-generating
      // goto label
      if(DSP.neweststate && DSP.neweststate->type == BUF_LABEL)
         DSP.numgotostates++;

      // make a goto object
      E_AddBufferedState(BUF_GOTO, nullptr, ps->linenum);
   }
   else
   {
      // main parsing pass
      if((*DSP.curbufstate)->type == BUF_LABEL)
      {
         DLListItem<estatebuf_t> *link = DSP.curbufstate;

         // last noticed state is a label; run down all labels and
         // create relocations that point to the current state.
         while((*link)->type != BUF_GOTO)
         {
            edecstateout_t *dso = DSP.pDSO;
            edecstate_t *s = &(dso->states[dso->numstates]);
            
            s->label = estrdup((*link)->name);
            s->state = states[DSP.currentstate];
            
            dso->numstates++;
            
            link = link->dllNext;
         }
         DSP.curbufstate = link;

         // setup the current state as a goto state
         DSP.internalgotos[DSP.numinternalgotos].gotoInfo = *DSP.curbufstate;
         DSP.internalgotos[DSP.numinternalgotos].state    =  DSP.currentstate;

         DSP.numinternalgotos++; // move forward one internal goto
         DSP.currentstate++;     // move forward to the next state
      }
      else
      {
         // this goto determines the nextstate field for the previous state
         DSP.internalgotos[DSP.numinternalgotos].gotoInfo = *DSP.curbufstate;
         DSP.internalgotos[DSP.numinternalgotos].state    =  DSP.currentstate - 1;

         DSP.numinternalgotos++; // move forward one internal goto
      }
      
      // move to next buffered state object
      DSP.curbufstate = DSP.curbufstate->dllNext;
   }
   ps->state = PSTATE_NEEDGOTOLABEL;
}

//
// doKeyword
//
// Other keywords are not quite as horrible as goto.
// 
// * During principals we just record a keyword object and what type of keyword
//   it is. "stop" keywords that immediately follow one or more labels are "kill
//   states," however, which must eventually be returned to the caller in the 
//   DSO object for removing states from whatever object contains this state 
//   block.
//
// * During the main pass, the loop, wait, and stop keywords are resolved to
//   their jump targets immediately. The wait keyword keeps an object in its 
//   current state, whereas the loop keyword returns it to the previous state
//   to which a label was resolved, which is kept in the variable 
//   "lastlabelstate" (see doText below). "stop" sends a state to the nullptr
//   state. stop keywords that follow labels are added to the killstates list
//   in the DSO object immediately, once for each consecutive label that 
//   points to that stop keyword.
//
static void doKeyword(pstate_t *ps)
{
   // handle other keyword specifics
   if(ps->principals)
   {
      // count a keyword
      DSP.numkeywords++;

      // if the keyword is "stop" and the previous buffered object is a label, 
      // this is a "kill state"
      if(DSP.neweststate && DSP.neweststate->type == BUF_LABEL &&
         !ps->tokenbuffer->strCaseCmp("stop"))
         DSP.numstops++;

      // make a keyword object
      E_AddBufferedState(BUF_KEYWORD, ps->tokenbuffer->constPtr(), ps->linenum);
   }
   else
   {
      state_t *state = states[DSP.currentstate - 1];
      int kwdcode = E_StrToNumLinear(decorate_kwds, NUMDECKWDS, 
                                     ps->tokenbuffer->constPtr());

      switch(kwdcode)
      {
      case KWD_WAIT:
         if(state->flags & STATEFI_DECORATE)
         {
            state->nextstate = DSP.currentstate - 1; // self-referential
            if(state->tics == 0)
               state->tics = 1; // eliminate unsafe tics value
         }
         break;
      case KWD_LOOP:
         if(state->flags & STATEFI_DECORATE)
            state->nextstate = DSP.lastlabelstate;
         break;
      case KWD_STOP:
         if((*DSP.curbufstate)->type == BUF_LABEL)
         {
            DLListItem<estatebuf_t> *link = DSP.curbufstate;

            // if we're still on a label and this is a stop keyword, run 
            // forward through the labels and make kill states for each one
            while((*link)->type != BUF_KEYWORD)
            {
               edecstateout_t *dso = DSP.pDSO;
               ekillstate_t   *ks  = &(dso->killstates[dso->numkillstates]);
               
               ks->killname = estrdup((*link)->name);               
               dso->numkillstates++;

               link = link->dllNext;
            }
            DSP.curbufstate = link;
         }
         else if(state->flags & STATEFI_DECORATE)
            state->nextstate = NullStateNum; // next state is null state
         break;
      default:
         break;
      }

      // move to next buffered state object
      DSP.curbufstate = DSP.curbufstate->dllNext;
   }
   ps->state = PSTATE_NEEDKWEOL;
}

//
// doText
//
// A normal textual token (phew!) It should be a sprite name, and during the
// first pass, the sprite name will be added if it doesn't exist already.
//
// During the main pass, we start processing the current set of states which
// are now being defined. First the currentstate is associated with any
// pending set of labels that have been seen (and we are still sitting at
// via the curbufstate pointer). Once the pointer hits a state, we should be
// caught up.
//
// Then we can walk down the list with a temporary estatebuf pointer to fill
// in the sprite in all frames that share the current buffered object's line
// number and type.
//
static void doText(pstate_t *ps)
{
   // verify sprite name; generate new state
   if(ps->principals)
   {
      // make a new sprite if this one doesn't already exist
      if(E_SpriteNumForName(ps->tokenbuffer->constPtr()) == -1)
         E_ProcessSingleSprite(ps->tokenbuffer->constPtr());

      // do not count or create a state yet; this will be done when we parse
      // the frame token, whose strlen is the number of states to count
   }
   else
   {
      int sprnum = E_SpriteNumForName(ps->tokenbuffer->constPtr());
      DLListItem<estatebuf_t> *link;
      int statenum;

      link = DSP.curbufstate;

      // if we are on a label, all the labels until we reach the next state
      // object should be associated with the current state
      while((*link)->type == BUF_LABEL)
      {
         edecstateout_t *dso = DSP.pDSO;

         dso->states[dso->numstates].label = estrdup((*link)->name);
         dso->states[dso->numstates].state = states[DSP.currentstate];
         dso->numstates++;

         // set lastlabelstate for loop keyword
         DSP.lastlabelstate = DSP.currentstate;

         link = link->dllNext;
      }
      DSP.curbufstate = link;
      
      statenum = DSP.currentstate;

      // set sprite number of all frame objects on the list with the same
      // linenumber as the current buffered state object
      while(link && 
            (*link)->linenum == (*DSP.curbufstate)->linenum &&
            (*link)->type == BUF_STATE)
      {
         if(states[statenum]->flags & STATEFI_DECORATE)
            states[statenum]->sprite = sprnum;
         link = link->dllNext;
         ++statenum;
      }
   }
   ps->state = PSTATE_NEEDSTATEFRAMES;
}

//
// DoPSNeedLabelOrKWOrState
//
// Main looping state. This is gone to after finding the first label,
// as well as returned to after processing a state.
//
static void DoPSNeedLabelOrKWOrState(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      // loop until something meaningful appears
      break;

   case TOKEN_LABEL:
      // handle the label
      doLabel(ps);
      break;

   case TOKEN_KEYWORD:
      // generate appropriate state for keyword
      if(!ps->tokenbuffer->strCaseCmp("goto"))
         doGoto(ps);
      else
         doKeyword(ps);
      break;

   case TOKEN_TEXT:
      // This should be a sprite name.
      doText(ps);
      break;

   default:
      // anything else is an error; eat rest of the line
      PSExpectedErr(ps, "label or keyword or sprite");
      break;
   }
}

//
// DoPSNeedGotoLabel
//
// Expecting the label for a "goto" keyword
//
static void DoPSNeedGotoLabel(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype == TOKEN_TEXT)
   {
      // record text into goto destination record for later
      // resolution pass.
      if(ps->principals)
      {
         if(DSP.neweststate->type != BUF_GOTO)
            I_Error("DoPSNeedGotoLabel: internal error - last state != GOTO\n");

         DSP.neweststate->gotodest = ps->tokenbuffer->duplicate(PU_STATIC);
      }
      // Nothing is required outside of principals
      ps->state = PSTATE_NEEDGOTOEOLORPLUS;
   }
   else
   {
      PSExpectedErr(ps, "goto label");
      ps->state = PSTATE_NEEDLABEL;          // return to initial state
   }
}

//
// DoPSNeedGotoEOLOrPlus
//
// Expecting EOL or '+' after a goto destination label.
//
static void DoPSNeedGotoEOLOrPlus(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      ps->state = PSTATE_NEEDLABEL;
      break;

   case TOKEN_PLUS:
      ps->state = PSTATE_NEEDGOTOOFFSET;
      break;

   default:
      // anything else is an error
      PSExpectedErr(ps, "end of line or +");
      ps->state = PSTATE_NEEDLABEL;
      break;
   }
}

//
// DoPSNeedGotoOffset
//
// Expecting a numeric offset value after the '+' in a goto statement.
//
static void DoPSNeedGotoOffset(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype == TOKEN_TEXT)
   {
      // record offset into goto destination record
      if(ps->principals)
      {
         if(DSP.neweststate->type != BUF_GOTO)
            I_Error("DoPSNeedGotoOffset: internal error - last state != GOTO\n");

         DSP.neweststate->gotooffset = ps->tokenbuffer->toInt();
      }
      // Nothing is required outside of principals
      ps->state = PSTATE_NEEDKWEOL;
   }
   else
   {
      // anything else is an error
      PSExpectedErr(ps, "goto offset");
      ps->state = PSTATE_NEEDLABEL;
   }
}

//
// DoPSNeedKWEOL
//
// Expecting the end of a line after a state transition keyword.
// The only thing which can come next is a new label.
//
static void DoPSNeedKWEOL(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_EOL)
      PSExpectedErr(ps, "end of line");

   // return to initial state
   ps->state = PSTATE_NEEDLABEL;
}

//
// DoPSNeedStateFrames
//
// Expecting a frames string for the current state.
// Frames strings are a series of A-Z and [\] chars.
// If this string is more than one character long, we generate additional
// states up to the number of characters in the string, and the rest of the
// properties parsed for the first state apply to those states. The states
// created this way have their next state fields automatically created to be 
// consecutive.
//
static void DoPSNeedStateFrames(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "frame string");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
   }
   else
   {
      if(ps->principals)
      {
         // During parsing for principals, we determine how many states should
         // be created, and add that many state objects. Each state object 
         // created here will have the same line number as this line in the 
         // DECORATE state block, and when parsing for values, we will apply the
         // values to all states from the current one to the last one found 
         // having the same line number.
         int i;
         int numstatestomake = (int)(ps->tokenbuffer->length());

         for(i = 0; i < numstatestomake; ++i)
            E_AddBufferedState(BUF_STATE, nullptr, ps->linenum);

         DSP.numdecstates += numstatestomake;
      }
      else
      {
         // Apply the frames to each state with the same line number.
         // We also set the nextstates here to be consecutive.
         DLListItem<estatebuf_t> *link = DSP.curbufstate;
         int statenum = DSP.currentstate;
         unsigned int stridx = 0;

         while(link && (*link)->type == BUF_STATE && 
               (*link)->linenum == (*DSP.curbufstate)->linenum)
         {
            char c = ectype::toUpper(ps->tokenbuffer->charAt(stridx));

            if(states[statenum]->flags & STATEFI_DECORATE)
            {
               states[statenum]->frame = c - 'A';

               if(states[statenum]->frame < 0 || states[statenum]->frame > 28)
               {
                  E_EDFLoggedWarning(2,
                     "DoPSNeedStateFrames: line %d: invalid DECORATE frame char %c\n",
                     (*DSP.curbufstate)->linenum, c);
                  ps->error = true;
                  return;
               }

               if(statenum != NUMSTATES - 1)
                  states[statenum]->nextstate = statenum + 1;
            }

            ++statenum; // move forward one state in states[]
            ++stridx;   // move forward one char in the qstring
            link = link->dllNext; // move forward one buffered state
         }
      }
      ps->state = PSTATE_NEEDSTATETICS;
   }
}

//
// DoPSNeedStateTics
//
// Expecting a tics amount for the current state.
//
static void DoPSNeedStateTics(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "tics value");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
   }
   else
   {      
      // Apply the tics to each state with the same line number.
      if(!ps->principals)
      {
         DLListItem<estatebuf_t> *link = DSP.curbufstate;
         int statenum = DSP.currentstate;
         int tics = ps->tokenbuffer->toInt();

         while(link && (*link)->type == BUF_STATE && 
               (*link)->linenum == (*DSP.curbufstate)->linenum)
         {
            if(states[statenum]->flags & STATEFI_DECORATE)
            {
               states[statenum]->tics = tics;

               if(states[statenum]->tics < -1)
                  states[statenum]->tics = -1;
            }

            ++statenum;             // move forward one state in states[]
            link = link->dllNext; // move forward one buffered state
         }
      }
      ps->state = PSTATE_NEEDFLAGORACTION;
   }
}

//
// doAction
//
// haleyjd 10/14/10: Factored common code for assigning state actions out of
// the two state handlers below.
//
static void doAction(pstate_t *ps, const char *fn)
{
   // Verify is valid codepointer name and apply action to all
   // states in the current range.
   if(!ps->principals)
   {
      DLListItem<estatebuf_t> *link = DSP.curbufstate;
      int statenum = DSP.currentstate;
      action_t *action = E_GetAction(ps->tokenbuffer->constPtr());

      if(!action)
      {
         E_EDFLoggedWarning(2, "%s: unknown action %s\n",
                            fn, ps->tokenbuffer->constPtr());
         ps->error = true;
         return;
      }

      while(link && (*link)->type == BUF_STATE &&
            (*link)->linenum == (*DSP.curbufstate)->linenum)
      {
         if(states[statenum]->flags & STATEFI_DECORATE)
            states[statenum]->action = action;

         ++statenum;           // move forward one state in states[]
         link = link->dllNext; // move forward one buffered state
      }
   }
   ps->state = PSTATE_NEEDSTATEEOLORPAREN;
}

enum
{
   DSFK_BRIGHT,
   DSFK_FAST,
   DSFK_OFFSET,
   DSFK_NUMFLAGS
};

static const char *decStateFlagKeywords[DSFK_NUMFLAGS] =
{
   "bright",
   "fast",
   "offset"
};

//
// applyStateBright
//
// Apply brightness to all the states in the current range.
//
static void applyStateBright()
{
   DLListItem<estatebuf_t> *link = DSP.curbufstate;
   int statenum = DSP.currentstate;

   // Apply fullbright to all states in the current range
   while(link && (*link)->type == BUF_STATE && 
      (*link)->linenum == (*DSP.curbufstate)->linenum)
   {
      if(states[statenum]->flags & STATEFI_DECORATE)
         states[statenum]->frame |= FF_FULLBRIGHT;

      ++statenum;           // move forward one state in states[]
      link = link->dllNext; // move forward one buffered state
   }
}

//
// applyStateFlag
//
// Apply a state flag to all the states in the current range.
//
static void applyStateFlag(unsigned int flag)
{
   DLListItem<estatebuf_t> *link = DSP.curbufstate;
   int statenum = DSP.currentstate;

   // Apply flag to all states in the current range
   while(link && (*link)->type == BUF_STATE &&
      (*link)->linenum == (*DSP.curbufstate)->linenum)
   {
      if(states[statenum]->flags & STATEFI_DECORATE)
         states[statenum]->flags |= flag;

      ++statenum;
      link = link->dllNext;
   }
}

//
// Apply a state offset (misc1 and misc2)
//
static void applyStateOffset(pstate_t *ps)
{
   E_GetDSToken(ps);
   if(ps->tokentype != TOKEN_LPAREN)
   {
      PSExpectedErr(ps, "'('");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   E_GetDSToken(ps);
   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "x value");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   int x = ps->tokenbuffer->toInt();

   E_GetDSToken(ps);
   if(ps->tokentype != TOKEN_COMMA)
   {
      PSExpectedErr(ps, "','");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   E_GetDSToken(ps);
   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "y value");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   int y = ps->tokenbuffer->toInt();
   bool interpolate = false;

   E_GetDSToken(ps);

   if(ps->tokentype == TOKEN_COMMA)
   {
      E_GetDSToken(ps);
      if(ps->tokentype != TOKEN_TEXT || ps->tokenbuffer->strCaseCmp("interpolate"))
      {
         PSExpectedErr(ps, "interpolate");
         ps->state = PSTATE_NEEDLABELORKWORSTATE;
         return;
      }
      interpolate = true;

      E_GetDSToken(ps);
      // fall through
   }

   if(ps->tokentype != TOKEN_RPAREN)
   {
      PSExpectedErr(ps, "')'");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   if(ps->principals)
      return;  // Principals only needs to check syntax here

   // Now apply the miscs
   DLListItem<estatebuf_t> *link = DSP.curbufstate;
   int statenum = DSP.currentstate;

   // Apply flag to all states in the current range
   while(link && (*link)->type == BUF_STATE && (*link)->linenum == (*DSP.curbufstate)->linenum)
   {
      if(states[statenum]->flags & STATEFI_DECORATE)
      {
         states[statenum]->misc1 = x;
         states[statenum]->misc2 = y;
         if(interpolate)
            states[statenum]->flags |= STATEF_INTERPOLATE;
         else
            states[statenum]->flags &= ~STATEF_INTERPOLATE;
      }

      ++statenum;
      link = link->dllNext;
   }
}

//
// DoPSNeedFlagOrAction
//
// Expecting either one or more state flag keywords or the action name.
//
// 06/23/10: We also accept EOL here so as not to require "Null" on frames 
//           that use the null action. (woops!)
//
static void DoPSNeedFlagOrAction(pstate_t *ps)
{
   E_GetDSToken(ps);

   // Allow EOL here to indicate null action
   if(ps->tokentype == TOKEN_EOL)
   {
      PSWrapStates(ps);
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "state flag keyword or action name");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      return;
   }

   int flagnum = E_StrToNumLinear(decStateFlagKeywords, DSFK_NUMFLAGS, 
                                  ps->tokenbuffer->constPtr());

   if(flagnum != DSFK_NUMFLAGS)
   {
      switch(flagnum)
      {
      case DSFK_BRIGHT:
         if(!ps->principals)
            applyStateBright();
         break;
      case DSFK_FAST:
         if(!ps->principals)
            applyStateFlag(STATEF_SKILL5FAST);
         break;
      case DSFK_OFFSET:
         applyStateOffset(ps);
         break;
      default:
         break;
      }
      // stay in the current state
   }
   else
      doAction(ps, "DoPSNeedFlagOrAction"); // otherwise verify & assign action
}

//
// DoPSNeedStateEOLOrParen
//
// After parsing the action name, expecting an EOL or '('.
//
static void DoPSNeedStateEOLOrParen(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_EOL:
      // Finalize state range
      PSWrapStates(ps);
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   case TOKEN_LPAREN:
      // Add argument object to all states
      if(!ps->principals)
      {
         DLListItem<estatebuf_t> *link = DSP.curbufstate;
         int statenum = DSP.currentstate;

         while(link && (*link)->type == BUF_STATE && 
               (*link)->linenum == (*DSP.curbufstate)->linenum)
         {
            if(states[statenum]->flags & STATEFI_DECORATE)
               E_CreateArgList(states[statenum]);

            ++statenum;           // move forward one state in states[]
            link = link->dllNext; // move forward one buffered state
         }
      }
      ps->state = PSTATE_NEEDSTATEARGORPAREN;
      break;
   default:
      // anything else is an error
      PSExpectedErr(ps, "end of line or (");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   }
}

//
// DoPSNeedStateArgOrParen
//
// After '(' we expect an argument or ')' (which is pointless to have but 
// is still allowed).
//
static void DoPSNeedStateArgOrParen(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_TEXT:
      // Parse and populate argument in state range
      if(!ps->principals)
      {
         DLListItem<estatebuf_t> *link = DSP.curbufstate;
         int statenum = DSP.currentstate;

         while(link && (*link)->type == BUF_STATE && 
               (*link)->linenum == (*DSP.curbufstate)->linenum)
         {
            if(states[statenum]->flags & STATEFI_DECORATE)
               E_AddArgToList(states[statenum]->args, ps->tokenbuffer->constPtr());

            ++statenum;           // move forward one state in states[]
            link = link->dllNext; // move forward one buffered state
         }
      }
      ps->state = PSTATE_NEEDSTATECOMMAORPAREN;
      break;
   case TOKEN_RPAREN:
      ps->state = PSTATE_NEEDSTATEEOL;
      break;
   default:
      // error
      PSExpectedErr(ps, "argument or )");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   }
}

//
// DoPSNeedStateCommaOrParen
//
// Expecting ',' or ')' after an action argument.
//
static void DoPSNeedStateCommaOrParen(pstate_t *ps)
{
   E_GetDSToken(ps);

   switch(ps->tokentype)
   {
   case TOKEN_COMMA:
      ps->state = PSTATE_NEEDSTATEARG;
      break;
   case TOKEN_RPAREN:
      ps->state = PSTATE_NEEDSTATEEOL;
      break;
   default:
      // error
      PSExpectedErr(ps, ", or )");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
      break;
   }
}

//
// DoPSNeedStateArg
//
// Expecting an argument, after having seen ','.
// This state exists because ')' is not allowed here.
//
static void DoPSNeedStateArg(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_TEXT)
   {
      PSExpectedErr(ps, "argument");
      ps->state = PSTATE_NEEDLABELORKWORSTATE;
   }
   else
   {
      // Parse and populate argument in state range
      if(!ps->principals)
      {
         DLListItem<estatebuf_t> *link = DSP.curbufstate;
         int statenum = DSP.currentstate;

         while(link && (*link)->type == BUF_STATE && 
               (*link)->linenum == (*DSP.curbufstate)->linenum)
         {
            if(states[statenum]->flags & STATEFI_DECORATE)
               E_AddArgToList(states[statenum]->args, ps->tokenbuffer->constPtr());

            ++statenum;           // move forward one state in states[]
            link = link->dllNext; // move forward one buffered state
         }
      }
      ps->state = PSTATE_NEEDSTATECOMMAORPAREN;
   }
}

//
// DoPSNeedStateEOL
//
// Expecting end of line after completing state parsing.
//
static void DoPSNeedStateEOL(pstate_t *ps)
{
   E_GetDSToken(ps);

   if(ps->tokentype != TOKEN_EOL)
   {
      PSExpectedErr(ps, "end of line");
      return;
   }

   // Finalize state range
   PSWrapStates(ps);

   ps->state = PSTATE_NEEDLABELORKWORSTATE;
}

// parser callback type
typedef void (*psfunc_t)(pstate_t *);

// parser state function table
static psfunc_t pstatefuncs[PSTATE_NUMSTATES] =
{
   DoPSNeedLabel,
   DoPSNeedLabelOrKWOrState,
   DoPSNeedGotoLabel,
   DoPSNeedGotoEOLOrPlus,
   DoPSNeedGotoOffset,
   DoPSNeedKWEOL,
   DoPSNeedStateFrames,
   DoPSNeedStateTics,
   DoPSNeedFlagOrAction,
   DoPSNeedStateEOLOrParen,
   DoPSNeedStateArgOrParen,
   DoPSNeedStateCommaOrParen,
   DoPSNeedStateArg,
   DoPSNeedStateEOL,
};

//=============================================================================
//
// Parser Core
//

//
// E_GetDSLine
//
// Gets the next line of DECORATE state input.
//
static bool E_GetDSLine(const char **src, pstate_t *ps)
{
   bool isdone = false;
   const char *srctxt = *src;

   ps->linebuffer->clear();
   ps->index = 0;

   if(!*srctxt) // at end?
      isdone = true;
   else
   {
      char c;

      while((c = *srctxt))
      {
         // do not step past the end of the string by doing this in the loop
         // condition above...
         ++srctxt;

         if(c == '\n')
            break;
         
         *ps->linebuffer += c;
      }

      // track line numbers
      ps->linenum++;
   }

   *src = srctxt;

   return isdone;
}

//
// E_parseDecorateInternal
//
// Main driver routine for parsing of DECORATE state blocks.
// Can be called either to collect principals or to run the final collection
// of data.
//
static bool E_parseDecorateInternal(const char *input, bool principals)
{
   pstate_t ps;
   qstring linebuffer;
   qstring tokenbuffer;
   const char *inputstr = input;

   // initialize pstate structure
   ps.index       = 0;
   ps.linenum     = 0;
   ps.tokentype   = 0;
   ps.tokenerror  = TERR_NONE;   
   ps.linebuffer  = &linebuffer;
   ps.tokenbuffer = &tokenbuffer;
   ps.principals  = principals;
   ps.error       = false;

   // set initial state
   ps.state = PSTATE_NEEDLABEL;

   // need a line to start
   ps.needline = true;

   while(1)
   {
      // need a new line of input?
      if(ps.needline)
      {
         if(E_GetDSLine(&inputstr, &ps))
            break; // ran out of lines

         ps.needline = false;
      }

      I_Assert(ps.state >= 0 && ps.state < PSTATE_NUMSTATES,
               "E_parseDecorateInternal: internal error - undefined state\n");

      pstatefuncs[ps.state](&ps);

      // if an error occured, end the loop
      if(ps.error)
         break;

      // if last token processed was an EOL, we need a new line of input
      if(ps.tokentype == TOKEN_EOL)
         ps.needline = true;
   }

   return !ps.error;
}

//
// E_checkPrincipalSemantics
//
// Looks for simple errors in the principals.
//
static bool E_checkPrincipalSemantics(const char *firststate)
{
   DLListItem<estatebuf_t> *link = DSP.statebuffer; 
   estatebuf_t *prev = nullptr;

   // Trying to use a pre-defined set of states. Certain conditions must be met:
   // * The state being named must exist.
   // * firststatenum + numdecstates should be <= NUMSTATES.
   // * numgotostates must be zero.
   if(firststate)
   {
      int firststatenum = E_StateNumForName(firststate);

      if(firststatenum < 0)
      {
         E_EDFLoggedWarning(2, "E_checkPrincipalSemantics: firstdecoratestate "
                               "of parent object is invalid\n");
         return false;
      }
      if(firststatenum + DSP.numdecstates > NUMSTATES)
      {
         E_EDFLoggedWarning(2, "E_checkPrincipalSemantics: not enough states "
                               "reserved after firstdecoratestate of parent "
                               "object\n");
         return false;
      }
      if(DSP.numgotostates > 0)
      {
         E_EDFLoggedWarning(2, "E_checkPrincipalSemantics: implicit goto states "
                               "are incompatible with firstdecoratestate "
                               "specification in parent object\n");
         return false;
      }
   }

   // Empty? No way bub. Not gonna deal with it.
   if(!link)
   {
      E_EDFLoggedWarning(2, "E_checkPrincipalSemantics: illegal empty DECORATE "
                            "state block\n");
      return false;
   }

   // At least one label must be defined.
   if(!DSP.numdeclabels)
   {
      E_EDFLoggedWarning(2, "E_checkPrincipalSemantics: no labels defined in "
                            "DECORATE state block\n");
      return false;
   }

   // At least one keyword or one state must be defined.
   // Note that implicit goto states will be included by the count of keywords.
   if(!DSP.numkeywords && !DSP.numdecstates)
   {
      E_EDFLoggedWarning(2, "E_checkPrincipalSemantics: no keywords or states "
                            "defined in DECORATE state block\n");
      return false;
   }

   while(link)
   {
      estatebuf_t *bstate = *link;

      // Look for orphaned loop and wait keywords immediately after a label,
      // as this is not allowed.
      if(prev && prev->type == BUF_LABEL && bstate->type == BUF_KEYWORD)
      {
         if(!strcasecmp(bstate->name, "loop") || 
            !strcasecmp(bstate->name, "wait"))
         {
            E_EDFLoggedWarning(2, 
                               "E_checkPrincipalSemantics: illegal keyword in "
                               "DECORATE states: line %d: %s\n", 
                               bstate->linenum, bstate->name);
            return false;
         }
      }

      prev = bstate;
      link = link->dllNext;
   }

   // Orphaned label at the end?
   if(prev && prev->type == BUF_LABEL)
   {
      E_EDFLoggedWarning(2,
                         "E_checkPrincipalSemantics: orphaned label in "
                         "DECORATE states: line %d: %s\n",
                         prev->linenum, prev->name);
      return false;
   }

   return true;
}

//
// E_DecoratePrincipals
//
// Counts and allocates labels, states, etc.
//
static edecstateout_t *E_DecoratePrincipals(const char *input, const char *firststate)
{
   edecstateout_t *newdso = nullptr;
   int totalstates;
   
   // Parse for principals (basic grammatic acceptance/rejection,
   // counting objects, recording some basic information).
   if(!E_parseDecorateInternal(input, true))
      return nullptr;

   // Run basic semantic checks
   if(!E_checkPrincipalSemantics(firststate))
      return nullptr;

   // Create the DSO object
   newdso = estructalloc(edecstateout_t, 1);

   // number of states to allocate is the number of declared states plus the
   // number of gotos which generate their own blank state with an immediate
   // transfer to the jump destination
   totalstates = DSP.numdecstates + DSP.numgotostates;

   if(totalstates)
   {
      // Record the index of the first state added into globals
      if(firststate)
         DSP.firststate = DSP.currentstate = E_StateNumForName(firststate);
      else
         DSP.firststate = DSP.currentstate = NUMSTATES;

      // if not using reserved states, create new ones
      if(!firststate)
      {
         state_t *newstates;

         // Add the required number of pointers to the states array
         E_ReallocStates(totalstates);

         // Allocate the new states as a block
         newstates = estructalloc(state_t, totalstates);

         // Initialize states
         for(int i = DSP.firststate; i < NUMSTATES; i++)
         {
            states[i] = &newstates[i - DSP.firststate];
            states[i]->index = i;
            states[i]->name = ecalloc(char *, 1, 40);
            psnprintf(states[i]->name, 40, "{DS %d}", i);         
            states[i]->dehnum = -1;
            states[i]->sprite = blankSpriteNum;
            states[i]->nextstate = NullStateNum;
            states[i]->flags |= STATEFI_DECORATE;
         }
      }
   }

   // allocate arrays in the DSO object at worst-case sizes for efficiency

   // there can't be more labels to assign than labels that are defined
   newdso->states = estructalloc(edecstate_t, DSP.numdeclabels);
   newdso->numstatesalloc = DSP.numdeclabels;

   if(DSP.numgotos)
   {
      // there can't be more gotos to externally fixup than the total number
      // of gotos
      newdso->gotos = estructalloc(egoto_t, DSP.numgotos);
      newdso->numgotosalloc = DSP.numgotos;

      // also allocate the internal goto list for the internal relocation pass
      DSP.internalgotos = estructalloc(internalgoto_t, DSP.numgotos);
      DSP.numinternalgotos = 0;
      DSP.numinternalgotosalloc = DSP.numgotos;
   }

   // We have counted the number of stops after labels exactly.
   if(DSP.numstops)
   {
      newdso->killstates = estructalloc(ekillstate_t, DSP.numstops);
      newdso->numkillsalloc = DSP.numstops;
   }

   return newdso;
}

//
// E_DecorateMainPass
//
// Parses through the data again, using the generated principals to drive
// population of the states and DSO object.
//
static bool E_DecorateMainPass(const char *input, edecstateout_t *dso)
{
   // Set the global DSO for parsing
   DSP.pDSO = dso;

   // begin current buffered state at head of list
   DSP.curbufstate = DSP.statebuffer;

   // Parse for keeps this time
   return E_parseDecorateInternal(input, false);
}

//
// E_resolveGotos
//
// Resolves gotos for which labels can be found in the DSO label set.
// Any goto which cannot be resolved in such a way must be added 
// instead to the DSO goto set for external resolution.
//
static bool E_resolveGotos(edecstateout_t *dso)
{
   int gi;

   // check each internal goto record
   for(gi = 0; gi < DSP.numinternalgotos; ++gi)
   {
      bool foundmatch = false;
      internalgoto_t *igt = &(DSP.internalgotos[gi]);
      estatebuf_t *gotoInfo = igt->gotoInfo;
      int ri;

      // check each resolved state record in the dso
      for(ri = 0; ri < dso->numstates; ++ri)
      {
         edecstate_t *ds = &(dso->states[ri]);
         if(!strcasecmp(gotoInfo->gotodest, ds->label))
         {
            foundmatch = true;

            if(!(states[igt->state]->flags & STATEFI_DECORATE))
               continue;

            states[igt->state]->nextstate = ds->state->index;

            // apply offset if any
            if(gotoInfo->gotooffset)
            {
               int statenum = states[igt->state]->nextstate + gotoInfo->gotooffset;

               if(statenum >= 0 && statenum < NUMSTATES)
                  states[igt->state]->nextstate = statenum;
               else
               {
                  // invalid!
                  E_EDFLoggedWarning(2, 
                                     "E_resolveGotos: bad DECORATE goto offset %s+%d\n", 
                                     gotoInfo->gotodest, gotoInfo->gotooffset);
                  return false;
               }     
            } // end if
         } // end if
      } // end for

      // no match? generate a goto in the DSO
      if(!foundmatch && states[igt->state]->flags & STATEFI_DECORATE)
      {
         egoto_t *egoto   = &(dso->gotos[dso->numgotos]);
         egoto->label     = estrdup(gotoInfo->gotodest);
         egoto->offset    = gotoInfo->gotooffset;
         egoto->nextstate = &(states[igt->state]->nextstate);

         dso->numgotos++;
      }
   } // end for

   return true;
}

//
// E_freeDecorateData
//
// Frees all the dynamic structures created to support parsing of DECORATE
// states.
//
static void E_freeDecorateData()
{
   DLListItem<estatebuf_t> *obj = nullptr;

   // free the internalgotos list
   if(DSP.internalgotos)
      efree(DSP.internalgotos);
   DSP.internalgotos = nullptr;
   DSP.numinternalgotos = DSP.numinternalgotosalloc = 0;

   // free the buffered state list
   while((obj = DSP.statebuffer))
   {
      estatebuf_t *bs = *obj;

      // remove it
      obj->remove();

      // free any allocated strings inside
      if(bs->name)
         efree(bs->name);
      if(bs->gotodest)
         efree(bs->gotodest);

      // free the object itself
      efree(bs);
   }
   DSP.statebuffer = DSP.curbufstate = nullptr;
   DSP.neweststate = nullptr;
}

//=============================================================================
//
// Global interface for DECORATE state parsing
//

//
// E_ParseDecorateStates
//
// Main driver routine for parsing of DECORATE state blocks.
// Call this and it will return the DSO object containing the following:
//
// * A list of states and their associated labels for binding
// * A list of gotos that need external resolution
// * A list of states to remove from the object
//
edecstateout_t *E_ParseDecorateStates(const char *input, const char *firststate)
{
   edecstateout_t *dso = nullptr;
   bool isgood = false;

   // init variables
   memset(&DSP, 0, sizeof(DSP));
   DSP.firststate = DSP.currentstate = DSP.lastlabelstate = NullStateNum;

   // parse for principals
   if(!(dso = E_DecoratePrincipals(input, firststate)))
      return nullptr;

   // do main pass
   if(E_DecorateMainPass(input, dso))
   {
     // resolve goto labels
     if(E_resolveGotos(dso))
        isgood = true; // all processing checks out
   }

   // free temporary parsing structures
   E_freeDecorateData();

   // if an error occured during processing, destroy whatever was created and
   // return nullptr
   if(!isgood)
   {
      E_FreeDSO(dso);
      dso = nullptr;
   }

   // return the DSO!
   return dso;
}

//
// E_FreeDSO
//
// Call this when you are finished with the DSO object.
//
void E_FreeDSO(edecstateout_t *dso)
{
   int i;

   if(dso->states)
   {
      for(i = 0; i < dso->numstates; i++)
      {
         if(dso->states[i].label)
            efree(dso->states[i].label);
      }
      efree(dso->states);
      dso->states = nullptr;
   }

   if(dso->gotos)
   {
      for(i = 0; i < dso->numgotos; i++)
      {
         if(dso->gotos[i].label)
            efree(dso->gotos[i].label);
      }
      efree(dso->gotos);
      dso->gotos = nullptr;
   }

   if(dso->killstates)
   {
      for(i = 0; i < dso->numkillstates; i++)
      {
         if(dso->killstates[i].killname)
            efree(dso->killstates[i].killname);
      }
      efree(dso->killstates);
      dso->killstates = nullptr;
   }

   efree(dso);
}

// EOF


