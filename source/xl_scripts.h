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
// Purpose: Hexen script lumps. Parsing and processing for Hexen's data scripts.
// Authors: James Haley
//

#ifndef XL_SCRIPTS_H__
#define XL_SCRIPTS_H__

#include "m_qstr.h"

struct lumpinfo_t;
class WadDirectory;

//
// XLTokenizer
//
// Tokenizer used by XLParser
//
class XLTokenizer
{
public:
    // Tokenizer states
    enum
    {
        STATE_SCAN,       // scanning for a string token
        STATE_INTOKEN,    // in a string token
        STATE_INBRACKETS, // in a bracketed token
        STATE_QUOTED,     // in a quoted string
        STATE_COMMENT,    // reading out a comment (eat rest of line)
        STATE_INBCOMMENT, // in a block comment
        STATE_DONE        // finished the current token
    };

    // Token types
    enum
    {
        TOKEN_NONE,       // Nothing identified yet
        TOKEN_KEYWORD,    // Starts with a $ (or without quotes if specified); or same as a string
        TOKEN_STRING,     // Generic string token; ex: 92 foobar
        TOKEN_EOF,        // End of input
        TOKEN_LINEBREAK,  // '\n' character, only a token when TF_LINEBREAKS is enabled
        TOKEN_BRACKETSTR, // bracketed string, when TF_BRACKETS is enabled
        TOKEN_ERROR       // An unknown token
    };

    // Tokenizer flags
    enum
    {
        TF_DEFAULT       = 0,          // default state (nothing special enabled)
        TF_LINEBREAKS    = 0x00000001, // line breaks are treated as tokens
        TF_BRACKETS      = 0x00000002, // supports [keyword] tokens
        TF_HASHCOMMENTS  = 0x00000004, // supports comments starting with # signs
        TF_SLASHCOMMENTS = 0x00000008, // supports double-slash comments
        TF_OPERATORS     = 0x00000010, // C-style identifiers, no space operators
        TF_ESCAPESTRINGS = 0x00000020, // Add support for escaping strings
        TF_STRINGSQUOTED = 0x00000040, // Strings must be quoted, otherwise they're keywords
        TF_BLOCKCOMMENTS = 0x00000080, // supports block comments with /* */
    };

protected:
    int          state;     // state of the scanner
    const char  *input;     // input string
    int          idx;       // current position in input string
    int          tokentype; // type of current token
    qstring      token;     // current token value
    unsigned int flags;     // parser flags

    void doStateScan();
    void doStateInToken();
    void doStateInBrackets();
    void doStateQuoted();
    void doStateComment();
    void doStateBlockComment();

    // State table declaration
    static void (XLTokenizer::*States[])();

public:
    // Constructor / Destructor
    explicit XLTokenizer(const char *str)
        : state(STATE_SCAN), input(str), idx(0), tokentype(TOKEN_NONE), token(32), flags(TF_DEFAULT)
    {}

    int getNextToken();

    // Accessors
    int      getTokenType() const { return tokentype; }
    qstring &getToken() { return token; }

    void setTokenFlags(unsigned int pFlags) { flags = pFlags; }
};

//
// XLParser
//
// Base class for Hexen lump parsers
//
class XLParser
{
protected:
    // Data
    const char   *lumpname; // Name of lump handled by this parser
    char         *lumpdata; // Cached lump data
    WadDirectory *waddir;   // Current directory

    // Override me!
    virtual void startLump() {}                               // called at beginning of a new lump
    virtual void initTokenizer(XLTokenizer &) {}              // called before tokenization starts
    virtual bool doToken(XLTokenizer &token) { return true; } // called for each token
    virtual void onEOF(bool early) {}                         // called when EOF is reached

    void parseLumpRecursive(WadDirectory &dir, lumpinfo_t *curlump);

public:
    // Constructors
    explicit XLParser(const char *pLumpname) : lumpname(pLumpname), lumpdata(nullptr), waddir(nullptr) {}

    // Destructor
    virtual ~XLParser()
    {
        // kill off any lump that might still be cached
        if(lumpdata)
        {
            efree(lumpdata);
            lumpdata = nullptr;
        }
    }

    void parseAll(WadDirectory &dir);
    void parseNew(WadDirectory &dir);
    void parseLump(WadDirectory &dir, lumpinfo_t *lump, bool global);

    // Accessors
    const char *getLumpName() const { return lumpname; }
    void        setLumpName(const char *s) { lumpname = s; }
};

void XL_ParseHexenScripts();

#endif

// EOF

