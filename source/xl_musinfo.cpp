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
// Purpose: MUSINFO parser.
// Authors: James Haley
//

#include "z_zone.h"

#include "p_info.h"
#include "w_wad.h"
#include "xl_scripts.h"

//=============================================================================
//
// Risen3D MUSINFO
//

class XLMusInfoParser : public XLParser
{
protected:
    // state enumeration
    enum
    {
        STATE_EXPECTMAP,
        STATE_EXPECTMAPNUM,
        STATE_EXPECTMAPNUM2,
        STATE_EXPECTMUSLUMP
    };

    int     state;
    qstring mapname;
    qstring lumpname;
    int     mapnum;

    void doStateExpectMap(XLTokenizer &token);
    void doStateExpectMapNum(XLTokenizer &token);
    void doStateExpectMapNum2(XLTokenizer &token);
    void doStateExpectMusLump(XLTokenizer &token);

    void pushMusInfoDef();

    // State table declaration
    static void (XLMusInfoParser::*States[])(XLTokenizer &);

    virtual bool doToken(XLTokenizer &token);
    virtual void startLump();

public:
    // Constructor
    XLMusInfoParser() : XLParser("MUSINFO"), state(STATE_EXPECTMAP), mapname(), mapnum(0) {}
};

//
// XLMusInfoParser::DoStateExpectMap
//
// Expecting a map name.
//
void XLMusInfoParser::doStateExpectMap(XLTokenizer &token)
{
    qstring &tokenText = token.getToken();

    switch(token.getTokenType())
    {
    case XLTokenizer::TOKEN_STRING: // a normal string
        mapname = tokenText;        // remember the map name
        state   = STATE_EXPECTMAPNUM;
        break;
    default: break;
    }
}

//
// XLMusInfoParser::doStateExpectMapNum
//
// Expecting a map number.
//
void XLMusInfoParser::doStateExpectMapNum(XLTokenizer &token)
{
    mapnum = token.getToken().toInt();
    state  = STATE_EXPECTMUSLUMP;
}

//
// XLMusInfoParser::pushMusInfoDef
//
// Called when enough information is available to push a MUSINFO definition.
//
void XLMusInfoParser::pushMusInfoDef()
{
    if(mapnum >= 0 && waddir->checkNumForName(mapname.constPtr()) >= 0 &&
       waddir->checkNumForName(lumpname.constPtr()) >= 0)
    {
        P_AddMusInfoMusic(mapname.constPtr(), mapnum, lumpname.constPtr());
    }
    mapnum = -1;
    lumpname.clear();
}

//
// XLMusInfoParser::doStateExpectMapNum2
//
// Expecting a map number, or the next map name.
//
void XLMusInfoParser::doStateExpectMapNum2(XLTokenizer &token)
{
    char    *end       = nullptr;
    qstring &tokenText = token.getToken();
    long     num       = tokenText.toLong(&end, 10);

    if(end && *end != '\0') // not a number?
    {
        // push out any current definition
        pushMusInfoDef();

        // return to STATE_EXPECTMAP immediately
        state = STATE_EXPECTMAP;
        doStateExpectMap(token);
    }
    else
    {
        mapnum = (int)num;
        state  = STATE_EXPECTMUSLUMP;
    }
}

//
// XLMusInfoParser::doStateExpectMusLump
//
// Expecting a music lump.
//
void XLMusInfoParser::doStateExpectMusLump(XLTokenizer &token)
{
    lumpname = token.getToken();
    pushMusInfoDef(); // push out the complete definition

    // expecting either another mapnum, or a new mapname token
    state = STATE_EXPECTMAPNUM2;
}

// State table for MUSINFO parser
void (XLMusInfoParser::*XLMusInfoParser::States[])(XLTokenizer &) = {
    &XLMusInfoParser::doStateExpectMap,     //
    &XLMusInfoParser::doStateExpectMapNum,  //
    &XLMusInfoParser::doStateExpectMapNum2, //
    &XLMusInfoParser::doStateExpectMusLump  //
};

//
// XLMusInfoParser::doToken
//
// Dispatch a token via this class's state table.
//
bool XLMusInfoParser::doToken(XLTokenizer &token)
{
    (this->*States[state])(token);
    return true;
}

//
// XLMusInfoParser::startLump
//
void XLMusInfoParser::startLump()
{
    // clear all data
    state = STATE_EXPECTMAP;
    mapname.clear();
    lumpname.clear();
    mapnum = -1;
}

//=============================================================================
//
// External Interface
//

void XL_ParseMusInfo()
{
    XLMusInfoParser musInfoParser;

    musInfoParser.parseAll(wGlobalDir);
}

// EOF

