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
// EDF States Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_STATES_H__
#define E_STATES_H__

struct mobjinfo_t;
struct state_t;

int E_StateNumForDEHNum(int dehnum);        // dehnum lookup
int E_GetStateNumForDEHNum(int dehnum);     //    fatal error version
int E_SafeState(int dehnum);                //    fallback version
int E_SafeStateName(const char *name);      //    fallback by name
int E_StateNumForName(const char *name);    // mnemonic lookup
int E_GetStateNumForName(const char *name); //    fatal error version

int E_SafeStateNameOrLabel(const mobjinfo_t *mi, const char *name);

extern int NullStateNum;

// EDF-Only Definitions/Declarations
#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_FRAME      "frame"
#define EDF_SEC_FRMDELTA   "framedelta"
#define EDF_SEC_FRAMEBLOCK "frameblock"

extern cfg_opt_t edf_frame_opts[];
extern cfg_opt_t edf_fdelta_opts[];
extern cfg_opt_t edf_fblock_opts[];

void E_CollectStates(cfg_t *scfg);
void E_ProcessStates(cfg_t *cfg);
void E_ProcessStateDeltas(cfg_t *cfg);
bool E_AutoAllocStateDEHNum(int statenum);
void E_ProcessFrameBlocks(cfg_t *cfg);

#endif

// For DECORATE states:
void E_ReallocStates(int numnewstates);
void E_CreateArgList(state_t *state);

#endif

// EOF

