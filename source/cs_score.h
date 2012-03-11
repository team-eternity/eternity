// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Client/Server scoreboard drawing.
//
//----------------------------------------------------------------------------

#ifndef CS_SCORE_H__
#define CS_SCORE_H__

#include "i_font.h"

class BaseScoreboard : public ZoneObject
{
protected:
   bool testing;
   unsigned int m_x, m_y;

public:
   BaseScoreboard();
   ~BaseScoreboard();

   virtual void display();
   virtual void setClientNeedsRepainted(int clientnum);

   void         setX(unsigned int new_x);
   void         setY(unsigned int new_y);
   unsigned int getX() const;
   unsigned int getY() const;
   void         setTesting(bool new_testing);
};

class LowResCoopScoreboard : public BaseScoreboard
{
public:
   void display();
};

class LowResDeathmatchScoreboard : public BaseScoreboard
{
public:
   void display();
};

class LowResTeamDeathmatchScoreboard : public BaseScoreboard
{
public:
   void display();
};

class HighResScoreboard : public BaseScoreboard
{
};

class HighResCoopScoreboard : public HighResScoreboard
{
};

class HighResDeathmatchScoreboard : public HighResScoreboard
{
};

class HighResTeamDeathmatchScoreboard : public HighResScoreboard
{
};

extern BaseScoreboard *cs_scoreboard;

void CS_InitScoreboard();
void CS_DrawScoreboard(unsigned int extra_top_margin);
void CS_ShowScores();

#endif

