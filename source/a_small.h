// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//----------------------------------------------------------------------------
//
// Copyright(C) 2002 James Haley
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
//
// Small Scripting Engine Interface
//
// Functions needed to provide for game engine usage of the Small
// virtual machine (see amx.c)
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef A_SMALL_H__
#define A_SMALL_H__

#if 0

// callback flags

enum scb_flags
{
   SCBF_PAUSABLE = 0x00000001, // callback will wait if game pauses
   SCBF_MPAUSE   = 0x00000002, // callback will wait if menus up in non-netgame
};

// callbacks

typedef struct sc_callback_s
{
   char vm;     // vm to which this callback belongs
   int scriptNum;  // number of script to call (internal AMX #)

   enum
   {
      wt_none,          // not waiting
      wt_delay,         // wait for a set amount of time
      wt_tagwait,       // wait for sector to stop moving
      wt_numtypes,
   };
   int wait_type;

   int wait_data;       // data used for waiting, varies by type

   int flags;           // 05/24/04: flags for special behaviors

   struct sc_callback_s *next; // for linked list
   struct sc_callback_s *prev; // for linked list

} sc_callback_t;

sc_callback_t *SM_GetCallbackList(void);
void SM_LinkCallback(sc_callback_t *);
void SM_ExecuteCallbacks(void);
void SM_RemoveCallbacks(int vm);
void SM_RemoveCallback(sc_callback_t *callback);
int  SM_AddCallback(char *scrname, sc_vm_e vm,
                   int waittype, int waitdata, int waitflags);
#endif

class Mobj;

// haleyjd 07/06/04: FINE put it here!
Mobj *P_FindMobjFromTID(int tid, Mobj *rover, Mobj *trigger);

#endif

// EOF
