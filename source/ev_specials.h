// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized line action system
//
//-----------------------------------------------------------------------------

#ifndef EV_SPECIALS_H__
#define EV_SPECIALS_H__

struct ev_action_t;
struct line_t;
class  Mobj;

// Action flags
enum EVActionFlags
{
   EV_PREALLOWMONSTERS = 0x00000001, // Preamble should allow non-players
   EV_PREALLOWZEROTAG  = 0x00000002, // Preamble should allow zero tag
   EV_PREFIRSTSIDEONLY = 0x00000004, // Disallow activation from back side
   
   EV_POSTCLEARSPECIAL = 0x00000008, // Clear special after activation
   EV_POSTCLEARALWAYS  = 0x00000010, // Always clear special
};

// Data related to a special activation.
struct specialactivation_t
{
   Mobj   *actor; // actor, if any
   line_t *line;  // line, if any
   int    *args;  // arguments (may point to line->args
   int     tag;   // tag (may == line->tag or line->args[0])
   int     side;  // side of activation
   int     spac;  // special activation type
};

//
// EVPreFunc
//
// Before a special will be activated, it will be checked for validity of the
// activation. This allows common prologue functionality to occur before the
// special is activated. Most arguments are optional and must be checked for
// validity.
//
typedef bool (*EVPreFunc)(ev_action_t *, specialactivation_t *);

//
// EVPostFunc
//
// This function will be executed after the special and can react to its
// execution. The result code of the EVActionFunc is passed as the first
// parameter. Most other parameters are optional and must be checked for
// validity.
//
typedef bool (*EVPostFunc)(ev_action_t *, bool, specialactivation_t *);

//
// EVActionFunc
//
// All actions must adhere to this call signature. Most arguments are optional
// and must be checked for validity.
//
typedef bool (*EVActionFunc)(ev_action_t *, specialactivation_t *);

struct ev_action_t
{
   int          activation; // activation type of this special, if restricted
   EVPreFunc    pre;        // pre-action callback
   EVActionFunc action;     // action function
   EVPostFunc   post;       // post-action callback
   unsigned int flags;      // action flags
   int          minversion; // minimum demo version
};

// Attaches a line special action to a specific action number in the indicated
// game type.
struct gamelinespecial_t
{
   int gameType;        // game to which this entry applies
   int actionNumber;    // line action number
   ev_action_t *action; // the actual action to execute
};

#endif

// EOF

