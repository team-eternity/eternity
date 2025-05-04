/* Configuration file parser -*- tab-width: 4; -*-
 *
 * Copyright (c) 2002, Martin Hedenfalk <mhe@home.se>
 * Modifications for Eternity Copyright (c) 2003 James Haley
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 */

// haleyjd: Replacement lexer for libConfuse -- the flex lexer was
// not able to meet my needs and was in some respects too general.
// All of this code, with the exception of the include stuff, is
// brand spankin' new.

#include <errno.h>

#include "../z_zone.h"
#include "../d_io.h"
#include "../d_dwfile.h"
#include "../w_wad.h"
#include "../m_qstr.h"
#include "../i_system.h"
#include "../v_misc.h"

#include "confuse.h"

// file include stack

static constexpr int MAX_INCLUDE_DEPTH = 16;

static struct cfginclude_s
{
    char         *filename;
    int           lumpnum; // haleyjd
    unsigned int  line;
    char         *buffer; // haleyjd 03/16/08
    char         *pos;
    cfg_dialect_t dialect; // haleyjd 09/17/12
} include_stack[MAX_INCLUDE_DEPTH];

static int include_stack_ptr = 0;

// haleyjd 09/17/12: current parser dialect
static cfg_dialect_t currentDialect = CFG_DIALECT_DELTA;

const char *mytext; // haleyjd: equivalent to yytext

// haleyjd 07/11/03: dynamic string buffer solution from
// libConfuse v2.0; eliminates unsafe, overflowable array
// 02/12/04: replaced with generalized solution in m_qstr.c

static qstring qstr;
static qstring hexchar;

//
// lexer_error
//
// haleyjd 12/16/03:
// Calls cfg_error with a nice, uniform error message.
//
static void lexer_error(cfg_t *cfg, const char *msg)
{
    cfg_error(cfg, "lexer error @ %s:%d:\n\t%s\n", cfg->filename, cfg->line, msg);
}

// haleyjd 12/23/06: if true, unquoted strings can contain spaces
// at the moment. Defaults to false.

static bool unquoted_spaces = false;

//
// lexer_set_unquoted_spaces
//
// Toggles the behavior of spaces inside unquoted strings.
//
void lexer_set_unquoted_spaces(bool us)
{
    unquoted_spaces = us;
}

static char *lexbuffer; // current file buffer
static char *bufferpos; // position in buffer

static char *lexer_buffer_file(DWFILE *dwfile, size_t *len)
{
    size_t foo;
    size_t size;
    char  *buffer;

    size   = static_cast<size_t>(dwfile->fileLength());
    buffer = emalloc(char *, size + 1);

    if((foo = dwfile->read(buffer, 1, size)) != size)
    {
        I_Error("lexer_buffer_file: failed on file read (%d of %d bytes)\n", (int)foo, (int)size);
    }

    // null-terminate buffer
    buffer[size] = '\0';

    // write back size
    if(len)
        *len = size;

    return buffer;
}

static void lexer_free_buffer()
{
    if(lexbuffer)
        efree(lexbuffer);
    lexbuffer = bufferpos = nullptr;
}

//
// lexer_init
//
// Initializes the lexer.
//
int lexer_init(cfg_t *cfg, DWFILE *file)
{
    char  *buf;
    size_t len;
    int    code = 0;

    buf = lexer_buffer_file(file, &len);

    // haleyjd 03/21/10: optional cfg_t lexer callback
    if(cfg && cfg->lexfunc)
        code = cfg->lexfunc(cfg, buf, (int)len);

    if(!code)
    {
        qstr.create();
        bufferpos = lexbuffer = buf;
    }

    return code;
}

//
// lexer_reset
//
// Resets all variables in the lexer to their initial state. It is very
// important for this to be called by the libConfuse parser before parsing
// each independent cfg_t.
//
void lexer_reset()
{
    // clear include stack
    memset(include_stack, 0, MAX_INCLUDE_DEPTH * sizeof(struct cfginclude_s));
    include_stack_ptr = 0;

    // reset lexer variables
    mytext          = nullptr;
    unquoted_spaces = false;
    currentDialect  = CFG_DIALECT_DELTA;

    // free qstring buffer
    qstr.freeBuffer();

    // ensure that buffer state is reset
    lexer_free_buffer();
}

//=============================================================================
//
// lexer state handlers
//

// haleyjd 6/19/09 (happy birthday to me!): heredoc type enumeration.
// We support more than one kind for convenience. This allows strings to be
// stored that contain one of the delimiter types, but not all of them. This
// is an acknowledged limitation of this sort of heredoc, but is not important
// for Eternity's applications of the tool in general.
enum
{
    HEREDOC_SINGLE, // uses @' '@
    HEREDOC_DOUBLE, // uses @" "@
    HEREDOC_NUMDELIMS
};

// lexerstate structure; because lots of arguments annoy me :)
struct lexerstate_t
{
    cfg_t *cfg;         // current cfg_t
    int    state;       // current state of the lexer
    int    stringtype;  // type of string being parsed
    int    heredoctype; // type of heredoc delimiter being used
    char   c;           // current character
};

// lexer function protocol
using lexfunc_t = int (*)(lexerstate_t *);

// state enumeration for lexer FSA
enum
{
    STATE_NONE,
    STATE_SLCOMMENT,
    STATE_MLCOMMENT,
    STATE_STRING,
    STATE_ESCAPE,
    STATE_HEXESCAPE,
    STATE_LINECONTINUANCE,
    STATE_STRINGCOALESCE,
    STATE_UNQUOTEDSTRING,
    STATE_HEREDOC
};

//
// lexer_state_slcomment
//
// discard input up to next \n
//
static int lexer_state_slcomment(lexerstate_t *ls)
{
    if(ls->c == '\n')
    {
        ls->cfg->line++;
        ls->state = STATE_NONE;
    }

    return -1; // continue parsing
}

//
// lexer_state_mlcomment
//
// look for '*' to signal end of comment
//
static int lexer_state_mlcomment(lexerstate_t *ls)
{
    if(ls->c == '\n')
        ls->cfg->line++; // keep count of lines

    if(ls->c == '*')
    {
        if(*bufferpos == '/') // look ahead to next char
        {
            ++bufferpos; // move past '/'
            ls->state = STATE_NONE;
        }
    }

    return -1; // continue parsing
}

//
// lexer_state_string
//
static int lexer_state_string(lexerstate_t *ls)
{
    switch(ls->c)
    {
    case '\n': // free linebreak -- not allowed
        lexer_error(ls->cfg, "unterminated string constant");
        return 0;
    case '"':
        if(ls->stringtype == 1) // double-quoted string, end it
        {
            // check for coalescence
            ls->state = STATE_STRINGCOALESCE;
        }
        else
            qstr += ls->c;
        break;
    case '\'':
        if(ls->stringtype == 2) // single-quoted string, end it
        {
            // check for coalescence
            ls->state = STATE_STRINGCOALESCE;
        }
        else
            qstr += ls->c;
        break;
    case '\\':
        // a forward slash begins an escape sequence
        ls->state = STATE_ESCAPE;
        break;
    default: qstr += ls->c; break;
    }

    return -1;
}

//
// lexer_state_escape
//
// Handle escape sequences in quoted strings
//
static int lexer_state_escape(lexerstate_t *ls)
{
    switch(ls->c)
    {
    case '\n':           // line continuance for quoted strings
        ls->cfg->line++; // increment line count
        ls->state = STATE_LINECONTINUANCE;
        return -1;
    case 'a': // bell
        qstr += '\a';
        break;
    case 'b': // rub
        qstr += '\b';
        break;
    case 'n': // line break
        qstr += '\n';
        break;
    case 't': // tab
        qstr += '\t';
        break;
    case 'x': // hex escape sequence
        hexchar.clear();
        ls->state = STATE_HEXESCAPE;
        return -1;
    case '0': // NB: *not* null char! Brick color.
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': // haleyjd 03/14/06: color codes
        qstr += (char)((ls->c - '0') + TEXT_COLOR_MIN);
        break;
    case 'C': // absCentered
        qstr += (char)TEXT_CONTROL_ABSCENTER;
        break;
    case 'E': // error
        qstr += (char)TEXT_COLOR_ERROR;
        break;
    case 'H': // hi
        qstr += (char)TEXT_COLOR_HI;
        break;
    case 'K': // 'K' is an old alias for "brick" color.
        qstr += (char)TEXT_COLOR_MIN;
        break;
    case 'N': // normal
        qstr += (char)TEXT_COLOR_NORMAL;
        break;
    case 'S': // shadowed
        qstr += (char)TEXT_CONTROL_SHADOW;
        break;
    case 'T': // translucency
        qstr += (char)TEXT_CONTROL_TRANS;
        break;
    default: // Anything else is treated literally
        qstr += ls->c;
        break;
    }

    // If we reach here, return to STATE_STRING
    ls->state = STATE_STRING;
    return -1;
}

//
// lexer_state_hexescape
//
// Handle \x character constants
//
static int lexer_state_hexescape(lexerstate_t *ls)
{
    if((ls->c >= '0' && ls->c <= '9') || (ls->c >= 'A' && ls->c <= 'F') || (ls->c >= 'a' && ls->c <= 'f'))
    {
        hexchar += ls->c;

        if(hexchar.length() == 2) // Only two chars max.
        {
            qstr      += (char)(hexchar.toLong(nullptr, 16));
            ls->state  = STATE_STRING; // Back to string parsing.
        }
    }
    else // unknown character
    {
        lexer_error(ls->cfg, "illegal character in hex escape sequence");
        return 0;
    }

    return -1; // keep scanning
}

//
// lexer_state_linecontinuance
//
// Handle hacky C-style line continuance inside quoted string tokens
//
static int lexer_state_linecontinuance(lexerstate_t *ls)
{
    // stay in this state until a non-whitespace char is found
    if(ls->c != ' ' && ls->c != '\t')
    {
        // put back the last character and return to STATE_STRING
        --bufferpos;
        ls->state = STATE_STRING;
    }

    return -1;
}

//
// lexer_state_stringcoalesce
//
// Look for the start of another string literal after the current one,
// separated by nothing except whitespace. When this happens, coalesce
// the consecutive string tokens into one token.
//
static int lexer_state_stringcoalesce(lexerstate_t *ls)
{
    switch(ls->c)
    {
    case '\n': // increment line count, and fall through.
        ls->cfg->line++;
        [[fallthrough]];
    case ' ': // whitespace, continue scanning
    case '\t': return -1;
    case '"':                          // quotations: coalesce with previous token
    case '\'':                         //
        ls->state      = STATE_STRING; // go back to string state.
        ls->stringtype = (ls->c == '\'' ? 2 : 1);
        return -1;
    default: // something else; put it back and return token
        --bufferpos;
        mytext = qstr.constPtr();
        return CFGT_STR;
    }
}

//
// lexer_state_unquotedstring
//
static int lexer_state_unquotedstring(lexerstate_t *ls)
{
    char c = ls->c;

    if((!unquoted_spaces && (c == ' ' || c == '\t')) || (currentDialect >= CFG_DIALECT_ALFHEIM && c == ':') ||
       c == '"' || c == '\'' || c == '\n' || c == '=' || c == '{' || c == '}' || c == '(' || c == ')' || c == '+' ||
       c == ',' || c == '#' || c == '/' || c == ';')
    {
        // any special character ends an unquoted string
        --bufferpos; // put it back
        mytext = qstr.constPtr();

        return CFGT_STR; // return a string token
    }
    else // normal characters
    {
        qstr += c;

        return -1; // continue parsing
    }
}

//
// lexer_state_heredoc
//
static int lexer_state_heredoc(lexerstate_t *ls)
{
    char c = '\0';

    // 6/19/09: match against specific opening heredoc delimiter;
    // more strict, and offers more flexibility as a result.
    switch(ls->heredoctype)
    {
    case HEREDOC_SINGLE: c = '\''; break;
    case HEREDOC_DOUBLE: c = '"'; break;
    }

    // check for end of heredoc
    if(ls->c == c && *bufferpos == '@')
    {
        ++bufferpos; // move forward past @
        mytext = qstr.constPtr();

        return CFGT_STR; // return a string token
    }
    else // normal characters -- everything is literal
    {
        if(ls->c == '\n')
            ls->cfg->line++; // still need to track line numbers

        qstr += ls->c;

        return -1; // continue parsing
    }
}

//
// lexer_state_none
//
static int lexer_state_none(lexerstate_t *ls)
{
    int  ret = -1;
    char la;

    switch(ls->c)
    {
    case ';': // throw away optional semicolons
    case ' ': // throw away whitespace
    case '\f':
    case '\t': //
        break;
    case '\n': // count and throw away line breaks
        ls->cfg->line++;
        break;
    case '#': //
        ls->state = STATE_SLCOMMENT;
        break;
    case '/':            //
        la = *bufferpos; // look ahead to next character
        switch(la)
        {
        case '/':
        case '*':        //
            ++bufferpos; // move past / or *
            ls->state = (la == '/') ? STATE_SLCOMMENT : STATE_MLCOMMENT;
            break;
        default: //
            lexer_error(ls->cfg, "unexpected character after /");
            ret = 0;
        }
        break;
    case '{': //
        mytext = "{";
        ret    = '{';
        break;
    case '}': //
        mytext = "}";
        ret    = '}';
        break;
    case '(': //
        mytext = "(";
        ret    = '(';
        break;
    case ')': //
        mytext = ")";
        ret    = ')';
        break;
    case '=': //
        mytext = "=";
        ret    = '=';
        break;
    case '+':                 //
        if(*bufferpos != '=') // look ahead to next character
        {
            // if not '=', start an unquoted string
            qstr.clear();
            qstr      += ls->c;
            ls->state  = STATE_UNQUOTEDSTRING;
        }
        else
        {
            ++bufferpos; // move past =
            mytext = "+=";
            ret    = '+';
        }
        break;
    case ',': //
        mytext = ",";
        ret    = ',';
        break;
    case '"': // open double-quoted string
        qstr.clear();
        ls->state      = STATE_STRING;
        ls->stringtype = 1;
        break;
    case '\'': // open single-quoted string
        qstr.clear();
        ls->state      = STATE_STRING;
        ls->stringtype = 2;
        break;
    case '@':                                       // possibly open heredoc string
        if(*bufferpos == '"' || *bufferpos == '\'') // look ahead to next character
        {
            // 6/19/09: keep track of heredoc type by opening delimiter
            switch(*bufferpos)
            {
            case '\'': ls->heredoctype = HEREDOC_SINGLE; break;
            case '"':  ls->heredoctype = HEREDOC_DOUBLE; break;
            }
            ++bufferpos; // move past secondary delimiter character
            qstr.clear();
            ls->state = STATE_HEREDOC;
            break;
        }
        // fall through, @ is not special unless followed by " or '
        [[fallthrough]];
    default: // anything else is part of an unquoted string
        if(ls->c == ':' && currentDialect >= CFG_DIALECT_ALFHEIM)
        {
            mytext = ":";
            ret    = ':';
        }
        else
        {
            qstr.clear();
            qstr      += ls->c;
            ls->state  = STATE_UNQUOTEDSTRING;
        }
        break;
    }

    return ret;
}

// state handler routine table
static lexfunc_t lexerfuncs[] = {
    lexer_state_none,           lexer_state_slcomment, lexer_state_mlcomment,       lexer_state_string,
    lexer_state_escape,         lexer_state_hexescape, lexer_state_linecontinuance, lexer_state_stringcoalesce,
    lexer_state_unquotedstring, lexer_state_heredoc,
};

//
// mylex
//
// Retrieves tokens from the current DWFILE for use by libConfuse
//
int mylex(cfg_t *cfg)
{
    lexerstate_t ls;
    int          ret;

    ls.state      = STATE_NONE;
    ls.stringtype = 0;
    ls.cfg        = cfg;

include:
    while((ls.c = *bufferpos++))
    {
        if(ls.c != '\r') // keep reading on \r's
        {
            if((ret = lexerfuncs[ls.state](&ls)) != -1)
                return ret;
        }
    }

    // handle special cases at EOF:
    switch(ls.state)
    {
    case STATE_STRING:
    case STATE_ESCAPE:
    case STATE_HEXESCAPE:
    case STATE_LINECONTINUANCE:
    case STATE_HEREDOC:
        // EOF in quoted or heredoc string - not allowed
        lexer_error(cfg, "EOF in string constant");
        return 0;

    case STATE_UNQUOTEDSTRING:
    case STATE_STRINGCOALESCE:
        // EOF after unquoted string or while looking ahead for string
        // literal coalescence -- return the string, next call will
        // return EOF.
        --bufferpos;
        mytext = qstr.constPtr();
        return CFGT_STR;

    default:
        // EOF handling -- check the include stack
        if(--include_stack_ptr < 0)
        {
            lexer_free_buffer();
            return EOF;
        }
        else
        {
            // done with an include file
            efree(cfg->filename);
            lexer_free_buffer();
            lexbuffer      = include_stack[include_stack_ptr].buffer;
            bufferpos      = include_stack[include_stack_ptr].pos;
            cfg->filename  = include_stack[include_stack_ptr].filename;
            cfg->line      = include_stack[include_stack_ptr].line;
            cfg->lumpnum   = include_stack[include_stack_ptr].lumpnum;
            currentDialect = include_stack[include_stack_ptr].dialect;

            ls.state = STATE_NONE; // make sure it's not in an odd state
            goto include;          // haleyjd: goto -- kill me now!
        }
    }

    return EOF; // probably not reachable, but whatever
}

char *cfg_lexer_open(const char *filename, int lumpnum, size_t *len)
{
    DWFILE dwfile;

    // haleyjd 02/09/05: revised include handling for data vs file
    if(lumpnum >= 0)
        dwfile.openLump(lumpnum);
    else
        dwfile.openFile(filename, "rb");

    if(!dwfile.isOpen())
        return nullptr;

    return lexer_buffer_file(&dwfile, len);
}

char *cfg_lexer_mustopen(cfg_t *cfg, const char *filename, int lumpnum, size_t *len)
{
    char *ret = nullptr;

    if(!(ret = cfg_lexer_open(filename, lumpnum, len)))
    {
        cfg_error(cfg, "Error including file %s:\n%s\n", filename,
                  errno ? strerror(errno) : "unknown error"); // haleyjd
    }

    return ret;
}

int cfg_lexer_include(cfg_t *cfg, char *buffer, const char *filename, int lumpnum)
{
    if(include_stack_ptr >= MAX_INCLUDE_DEPTH)
    {
        cfg_error(cfg, "Error: includes nested too deeply.\n");
        return 1;
    }

    // haleyjd
    include_stack[include_stack_ptr].filename = cfg->filename;
    include_stack[include_stack_ptr].line     = cfg->line;
    include_stack[include_stack_ptr].lumpnum  = cfg->lumpnum;
    include_stack[include_stack_ptr].buffer   = lexbuffer;
    include_stack[include_stack_ptr].pos      = bufferpos;
    include_stack[include_stack_ptr].dialect  = currentDialect;
    include_stack_ptr++;

    cfg->filename = cfg_tilde_expand(filename);
    cfg->line     = 1;
    cfg->lumpnum  = lumpnum;

    bufferpos = lexbuffer = buffer;

    return 0;
}

//
// cfg_lexer_source_type
//
// haleyjd 02/09/05: function to tell whether current source is
// data or a physical file. If physical file, it will return -1.
// If a lump, it returns the lump number.
//
int cfg_lexer_source_type(cfg_t *cfg)
{
    return cfg->lumpnum;
}

//
// cfg_lexer_set_dialect
//
// Change the dialect being used by the lexer.
//
void cfg_lexer_set_dialect(cfg_dialect_t dialect)
{
    currentDialect = dialect;
}

// EOF

