// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
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
//   Prediction routines for clientside and sector prediction
//
//----------------------------------------------------------------------------

#ifndef __CL_PRED_H__
#define __CL_PRED_H__

#include "cs_position.h"

extern boolean cl_predicting;

void      CL_InitPrediction(void);
cs_cmd_t* CL_GetCurrentCommand(void);
void      CL_PredictPlayerPosition(unsigned int index);
void      CL_PredictSectorPositions(unsigned int index);
void      CL_Predict(unsigned int start_index, unsigned int end_index,
                     boolean repredicting);

#endif

