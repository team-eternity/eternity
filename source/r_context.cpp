//
// The Eternity Engine
// Copyright(C) 2020 James Haley, Max Waine, et al.
// Copyright(C) 2020 Ethan Watson
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
//----------------------------------------------------------------------------
//
// Purpose: Renderer context
// Some code is derived from Rum & Raisin Doom, Ethan Watson, used under
// terms of the GPLv3.
//
// Authors: Max Waine
//

#include <thread>

#include "c_io.h"
#include "c_runcmd.h"
#include "doomstat.h"
#include "m_misc.h"
#include "i_video.h"
#include "r_context.h"
#include "r_state.h"
#include "v_misc.h"

using threadhandle_t = void (*)(); // FIXME: This elsewhere when threading real
using atomicval_t = ptrdiff_t; // FIXME: This elsewhere when threading real
struct renderdata_t
{
   rendercontext_t context;
   threadhandle_t	 thread;
   atomicval_t     running;
   atomicval_t     shouldquit;
   atomicval_t     framewaiting;
   atomicval_t     framefinished;
};

static renderdata_t *renderdatas = nullptr;

//
// Grabs a given render context
//
rendercontext_t &R_GetContext(int index)
{
   return renderdatas[index].context;
}

//
// Frees up the dynamically allocated members of a context that aren't tagged PU_VALLOC
//
void R_freeContext(rendercontext_t &context)
{
   // THREAD_FIXME: Verify this catches everything
   if(context.spritecontext.drawsegs_xrange)
      efree(context.spritecontext.drawsegs_xrange);
   if(context.spritecontext.vissprites)
      efree(context.spritecontext.vissprites);
   if(context.spritecontext.vissprite_ptrs)
      efree(context.spritecontext.vissprite_ptrs);
   if(context.spritecontext.sectorvisited)
      efree(context.spritecontext.sectorvisited);
}

//
// Initialises all the render contexts
//
void R_InitContexts(const int width)
{
   static int prev_numcontexts = r_numcontexts;

   r_globalcontext = {};
   r_globalcontext.bufferindex  = -1;
   r_globalcontext.bounds.startcolumn  = 0;
   r_globalcontext.bounds.endcolumn    = width;
   r_globalcontext.bounds.fstartcolumn = 0.0f;
   r_globalcontext.bounds.fendcolumn   = static_cast<float>(width);

   if(renderdatas)
   {
      for(int currentcontext = 0; currentcontext < prev_numcontexts; currentcontext++)
         R_freeContext(renderdatas[currentcontext].context);
      efree(renderdatas);
      prev_numcontexts = r_numcontexts;
   }

   renderdatas = estructalloc(renderdata_t, r_numcontexts);

   const float contextwidth = static_cast<float>(width) / static_cast<float>(r_numcontexts);

   for(int currentcontext = 0; currentcontext < r_numcontexts; currentcontext++)
   {
      rendercontext_t &context = renderdatas[currentcontext].context;

      context.bufferindex = currentcontext;

      context.bounds.fstartcolumn = static_cast<float>(currentcontext)     * contextwidth;
      context.bounds.fendcolumn   = static_cast<float>(currentcontext + 1) * contextwidth;
      context.bounds.startcolumn  = static_cast<int>(roundf(context.bounds.fstartcolumn));
      context.bounds.endcolumn    = static_cast<int>(roundf(context.bounds.fendcolumn));
      context.bounds.numcolumns   = context.bounds.endcolumn - context.bounds.startcolumn;

      if(numsectors && gamestate == GS_LEVEL)
         context.spritecontext.sectorvisited = ecalloctag(bool *, numsectors, sizeof(bool), PU_LEVEL, nullptr);
   }
}

void R_RefreshContexts()
{
   for(int currentcontext = 0; currentcontext < r_numcontexts; currentcontext++)
   {
      rendercontext_t &context = renderdatas[currentcontext].context;

      context.spritecontext.sectorvisited = ecalloctag(bool *, numsectors, sizeof(bool), PU_LEVEL, nullptr);
   }
}


VARIABLE_INT(r_numcontexts, nullptr, 0, UL, nullptr);
CONSOLE_VARIABLE(r_numcontexts, r_numcontexts, cf_buffered)
{
   const int maxcontexts = std::thread::hardware_concurrency();

   if(r_numcontexts == 0)
      r_numcontexts = maxcontexts - 1; // allow scrolling left from 1 to maxcontexts
   else if(r_numcontexts == maxcontexts + 1)
      r_numcontexts = 1; // allow scrolling right from maxcontexts to 1
   else if(r_numcontexts > maxcontexts)
   {
      C_Printf(FC_ERROR "Warning: r_numcontexts's current maximum is %d, resetting to 1", maxcontexts);
      r_numcontexts = 1;
   }

   I_SetMode();
}

// EOF

