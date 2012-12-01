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
   EV_PREMONSTERSONLY  = 0x00000002, // Preamble should only allow monsters
   EV_PREALLOWZEROTAG  = 0x00000004, // Preamble should allow zero tag
   EV_PREFIRSTSIDEONLY = 0x00000008, // Disallow activation from back side
   
   EV_POSTCLEARSPECIAL = 0x00000010, // Clear special after activation
   EV_POSTCLEARALWAYS  = 0x00000020, // Always clear special
};

// Data related to an instance of a special activation.
struct ev_instance_t
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
typedef bool (*EVPreFunc)(ev_action_t *, ev_instance_t *);

//
// EVPostFunc
//
// This function will be executed after the special and can react to its
// execution. The result code of the EVActionFunc is passed as the first
// parameter. Most other parameters are optional and must be checked for
// validity.
//
typedef bool (*EVPostFunc)(ev_action_t *, bool, ev_instance_t *);

//
// EVActionFunc
//
// All actions must adhere to this call signature. Most members of the 
// specialactivation structure are optional and must be checked for validity.
//
typedef bool (*EVActionFunc)(ev_action_t *, ev_instance_t *);

//
// ev_actiontype_t
//
// This structure represents a distinct type of action, for example a DOOM
// cross-type action.
//
struct ev_actiontype_t
{
   int          activation; // activation type of this special, if restricted
   EVPreFunc    pre;        // pre-action callback
   EVPostFunc   post;       // post-action callback
   unsigned int flags;      // flags; the action may add additional flags.
};

struct ev_action_t
{
   ev_actiontype_t *type;   // actiontype structure
   EVActionFunc action;     // action function
   unsigned int flags;      // action flags
   int          minversion; // minimum demo version
};

// Binds a line special action to a specific action number in the indicated
// game type.
struct ev_binding_t
{
   int gameType;        // game to which this entry applies
   int actionNumber;    // line action number
   ev_action_t *action; // the actual action to execute

   ev_binding_t *next;  // next on hash chain
   ev_binding_t *first; // first on hash chain at this slot
};

#endif

// EOF

