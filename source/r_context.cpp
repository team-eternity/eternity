//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Max Waine, et al.
// Copyright (C) 2020 Ethan Watson
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
//------------------------------------------------------------------------------
//
// Purpose: Renderer context
//  Some code is derived from Rum & Raisin Doom, by Ethan Watson, used under
//  terms of the GPLv3.
//  BasicSemaphore and RenderThreadSemaphore are based on
//  code from https://vorbrodt.blog/2019/02/05/fast-semaphore/.
//  I can't find a licence for it.
//
// Authors: Max Waine
//

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "c_io.h"
#include "c_runcmd.h"
#include "doomstat.h"
#include "m_misc.h"
#include "hal/i_platform.h"
#include "hal/i_timer.h"
#include "i_video.h"
#include "m_compare.h"
#include "r_context.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_state.h"
#include "v_misc.h"

#if (EE_CURRENT_COMPILER == EE_COMPILER_MSVC) && !defined(_DEBUG)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern int __cdecl I_W32ExceptionHandler(PEXCEPTION_POINTERS ep);
#endif

//
// Basic semaphore used by RenderThreadSemaphore
//
class BasicSemaphore
{
public:
   BasicSemaphore(ptrdiff_t count) noexcept
      : m_count(count)
   {
   }

   void release() noexcept
   {
      {
         std::unique_lock<std::mutex> lock(m_mutex);
         m_count++;
      }
      m_cv.notify_one();
   }

   void acquire() noexcept
   {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_cv.wait(lock, [&]() -> bool {
         return m_count != 0;
      });
      m_count--;
   }

private:
   ptrdiff_t               m_count;
   std::mutex              m_mutex;
   std::condition_variable m_cv;
};

//
// Fast semaphore for render threads
//
class RenderThreadSemaphore
{
public:
   RenderThreadSemaphore(ptrdiff_t count) noexcept
      : m_count(count), m_semaphore(0)
   {
   }

   void release()
   {
      std::atomic_thread_fence(std::memory_order_release);
      ptrdiff_t count = m_count.fetch_add(1, std::memory_order_relaxed);
      if(count < 0)
         m_semaphore.release();
   }

   void acquire()
   {
      ptrdiff_t count = m_count.fetch_sub(1, std::memory_order_relaxed);
      if(count < 1)
         m_semaphore.acquire();
      std::atomic_thread_fence(std::memory_order_acquire);
   }

private:
   std::atomic<ptrdiff_t> m_count;
   BasicSemaphore         m_semaphore;
};

struct renderdata_t
{
   rendercontext_t       context;
   std::thread           thread;
   RenderThreadSemaphore shouldrun;
   std::atomic_bool      running;
   bool                  parallel;

   std::condition_variable checkframe;
   std::mutex              checkmutex;
   bool                    shouldquit;
   bool                    framewaiting;
   bool                    framefinished;

   const char           *errormessage;
   std::atomic_bool      fatalerror;
};

#if (EE_CURRENT_COMPILER == EE_COMPILER_MSVC) && !defined(_DEBUG)
#define R_runData R_runDataInner
#endif

static void R_runData(renderdata_t *data);

#if (EE_CURRENT_COMPILER == EE_COMPILER_MSVC) && !defined(_DEBUG)
#undef R_runData
#endif

static renderdata_t *renderdatas      = nullptr;
static int           prev_numcontexts = 0;

//
// Grabs a given render context
//
rendercontext_t &R_GetContext(int index)
{
   return renderdatas[index].context;
}

//
// Frees up the context's heap, which frees /all/ data tied to the context
//
void R_freeContext(rendercontext_t &context)
{
   if(context.heap)
   {
      delete context.heap;
      context.heap = nullptr;
   }
}

//
// Frees up actual renderdatas, which need waiting on before we can safely free
//
void R_freeData(renderdata_t &data)
{
   if(data.parallel)
   {
      std::lock_guard lock(data.checkmutex);
      data.shouldquit = true;
      data.checkframe.notify_one();
   }

   while(data.running.load())
      i_haltimer.Sleep(1);

   R_freeContext(data.context);

   // Free actual thread
   if(data.thread.joinable())
      data.thread.join();
}

//
// Frees up all contexts (and renderdatas by way of R_freeData)
//
void R_FreeContexts()
{
   R_freeContext(r_globalcontext);

   if(renderdatas)
   {
      for(int currentcontext = 0; currentcontext < prev_numcontexts; currentcontext++)
         R_freeData(renderdatas[currentcontext]);
      efree(renderdatas);
      renderdatas = nullptr;
   }
}

//
// Handler installed to I_Error while running contexts
//
static void R_handleContextError(char *errmsg)
{
   // The error message needs clearing because otherwise if the main thread
   // I_Errors while doing R_runData then I_Error won't output our desired message
   qstring temp{ errmsg };
   *errmsg = 0;
   throw temp;
}

#if (EE_CURRENT_COMPILER == EE_COMPILER_MSVC) && !defined(_DEBUG)
//
// In release builds we need to catch exceptions in render threads too
//
static void R_runData(renderdata_t *data)
{
   __try
   {
      R_runDataInner(data);
   }
   __except(I_W32ExceptionHandler(GetExceptionInformation()))
   {
      data->fatalerror = true;
   }
}
#endif

//
// This function is always going on in the background
// so that threads don't need to constantly be spawned
//
static void R_contextThreadFunc(renderdata_t *data)
{
   data->running.exchange(true);

   while(!data->shouldquit && !data->errormessage)
   {
      std::unique_lock lock(data->checkmutex);

      data->checkframe.wait(lock, [&data] { return data->framewaiting || data->shouldquit; });
      if(data->framewaiting)
      {
         data->framewaiting = false;
         data->shouldrun.acquire();
         R_runData(data);
         data->framefinished = true;
      }

      lock.unlock();
      data->checkframe.notify_one();
      std::this_thread::yield();
   }

   data->running.exchange(false);
   data->checkframe.notify_one();
}

//
// Allocates a context's PU_LEVEL data
//
void R_AllocateContextLevelData(rendercontext_t &context)
{
   context.spritecontext.sectorvisited = zhcalloctag(*context.heap, bool *, numsectors, sizeof(bool), PU_LEVEL, nullptr);

   context.portalcontext.numportalstates = R_GetNumPortals();
   context.portalcontext.portalstates =
      zhstructalloctag(*context.heap, portalstate_t, context.portalcontext.numportalstates, PU_LEVEL);
   for(int i = 0; i < context.portalcontext.numportalstates; i++)
      context.portalcontext.portalstates[i].poverlay = R_NewPlaneHash(*context.heap, 131);
}

//
// Initialises all the render contexts
//
void R_InitContexts(const int width)
{
   r_hascontexts = true;

   prev_numcontexts = r_numcontexts;

   r_globalcontext = {};
   r_globalcontext.bufferindex  = -1;
   r_globalcontext.bounds.startcolumn  = 0;
   r_globalcontext.bounds.endcolumn    = width;
   r_globalcontext.bounds.fstartcolumn = 0.0f;
   r_globalcontext.bounds.fendcolumn   = float(width);
   r_globalcontext.bounds.numcolumns   = width;

   r_globalcontext.heap = new ZoneHeap();

   if(r_numcontexts == 1)
   {
      r_globalcontext.portalcontext.portalrender = { false, MAX_SCREENWIDTH, 0 };

      if(numsectors && gamestate == GS_LEVEL)
         R_AllocateContextLevelData(r_globalcontext);

      return;
   }

   renderdatas = estructalloc(renderdata_t, r_numcontexts);

   const float contextwidth = float(width) / float(r_numcontexts);

   for(int currentcontext = 0; currentcontext < r_numcontexts; currentcontext++)
   {
      rendercontext_t &context = renderdatas[currentcontext].context;

      context.bufferindex = currentcontext;

      context.bounds.fstartcolumn = roundf(float(currentcontext)     * contextwidth);
      context.bounds.fendcolumn   = roundf(float(currentcontext + 1) * contextwidth);
      context.bounds.startcolumn  = int(roundf(context.bounds.fstartcolumn));
      context.bounds.endcolumn    = int(roundf(context.bounds.fendcolumn));
      context.bounds.numcolumns   = context.bounds.endcolumn - context.bounds.startcolumn;

      context.heap = new ZoneHeap();

      context.portalcontext.portalrender = { false, MAX_SCREENWIDTH, 0 }; // THREAD_FIXME: Adjust?

      if(numsectors && gamestate == GS_LEVEL)
         R_AllocateContextLevelData(context);

      // Wait until this context's thread is done running before creating a new one
      while(renderdatas[currentcontext].running.load())
         i_haltimer.Sleep(1);
      if(currentcontext < r_numcontexts - 1)
      {
         renderdatas[currentcontext].parallel = true;
         new(&renderdatas[currentcontext].shouldrun) RenderThreadSemaphore(0);
         new(&renderdatas[currentcontext].checkframe) std::condition_variable();
         new(&renderdatas[currentcontext].checkmutex) std::mutex();
         renderdatas[currentcontext].thread = std::thread(&R_contextThreadFunc, &renderdatas[currentcontext]);
      }
   }
}

//
// Perform level setup for all contexts
//
void R_RefreshContexts()
{
   if(!r_hascontexts)
      return;

   if(r_numcontexts == 1)
   {
      R_AllocateContextLevelData(r_globalcontext);
      return;
   }

   for(int currentcontext = 0; currentcontext < r_numcontexts; currentcontext++)
      R_AllocateContextLevelData(renderdatas[currentcontext].context);
}

//
// Sets the bounds (start and end columns, and column count) of all the contexts
//
void R_UpdateContextBounds()
{
   if(r_numcontexts == 1)
   {
      r_globalcontext.bounds.startcolumn   = 0;
      r_globalcontext.bounds.endcolumn     = viewwindow.width;
      r_globalcontext.bounds.fstartcolumn = 0.0f;
      r_globalcontext.bounds.fendcolumn   = float(viewwindow.width);
      return;
   }

   const float contextwidth = float(viewwindow.width) / float(r_numcontexts);
   for(int currentcontext = 0; currentcontext < r_numcontexts; currentcontext++)
   {
      rendercontext_t &context = renderdatas[currentcontext].context;

      context.bounds.fstartcolumn = roundf(float(currentcontext)     * contextwidth);
      context.bounds.fendcolumn   = roundf(float(currentcontext + 1) * contextwidth);
      context.bounds.startcolumn  = int(roundf(context.bounds.fstartcolumn));
      context.bounds.endcolumn    = int(roundf(context.bounds.fendcolumn));
      context.bounds.numcolumns   = context.bounds.endcolumn - context.bounds.startcolumn;
   }
}

//
// Checks if any of the contexts encountered an error while running,
// either an ordinary one (in which case errormessage will be set) or
// a fatal error
//
static void R_checkForContextErrors()
{
   // Check for errors
   bool    hasError = false;
   qstring errorMessage{ "R_RunContexts: Error in context(s):\n" };
   for(int currentcontext = 0; currentcontext < r_numcontexts; currentcontext++)
   {
      if(renderdatas[currentcontext].fatalerror)
      {
         I_FatalError(
            0,
            "Exception caught in R_RunContexts: see CRASHLOG.TXT for info, and in the "
            "same directory please upload eternity.dmp along with the crash log\n"
         );
      }

      if(renderdatas[currentcontext].errormessage)
      {
         errorMessage << "\t" << currentcontext + 1 << ": " << renderdatas[currentcontext].errormessage;
         if(!errorMessage.endsWith('\n'))
            errorMessage << "\n";

         hasError = true;
      }
   }

   if(hasError)
      I_Error("%s", errorMessage.constPtr());
}

//
// Runs all the contexts by setting the waiting-for-frame atomics bool to true,
// then waits for the frame-finished-rendering atomic bools to be true
// (setting them to false after)
//
void R_RunContexts()
{
   int finishedcontexts = 0;

   I_SetErrorHandler(R_handleContextError);

   for(int currentcontext = 0; currentcontext < r_numcontexts; currentcontext++)
   {
      if(currentcontext < r_numcontexts - 1)
      {
         std::lock_guard lock(renderdatas[currentcontext].checkmutex);
         renderdatas[currentcontext].framewaiting = true;
         renderdatas[currentcontext].checkframe.notify_one();
         renderdatas[currentcontext].shouldrun.release();
      }
      else
      {
         // The last context can be rendered on the main thread
         R_runData(&renderdatas[currentcontext]);
         finishedcontexts++;
      }
   }

   while(finishedcontexts != r_numcontexts)
   {
      for(int currentcontext = 0; currentcontext < r_numcontexts - 1; currentcontext++)
      {
         std::unique_lock lock(renderdatas[currentcontext].checkmutex);
         renderdatas[currentcontext].checkframe.wait(lock, [currentcontext] { return renderdatas[currentcontext].framefinished; });
         renderdatas[currentcontext].framefinished = false;
         finishedcontexts++;
      }
   }

   I_SetErrorHandler(nullptr);

   R_checkForContextErrors();
}

#if (EE_CURRENT_COMPILER == EE_COMPILER_MSVC) && !defined(_DEBUG)
#define R_runData R_runDataInner
#endif

//
// Tries to render the view context for a data, catching any possible errors
//
static void R_runData(renderdata_t *data)
{
   try
   {
      R_RenderViewContext(data->context);
   }
   catch(qstring &errorMessage)
   {
      data->errormessage = errorMessage.duplicate();
   }
}

VARIABLE_INT(r_numcontexts, nullptr, 0, UL, nullptr);
CONSOLE_VARIABLE(r_numcontexts, r_numcontexts, cf_buffered)
{
   const int maxcontexts = emax(std::thread::hardware_concurrency(), 1u);

   if(r_numcontexts == 0)
      r_numcontexts = maxcontexts; // allow scrolling left from 1 to maxcontexts
   else if(r_numcontexts == maxcontexts + 1)
      r_numcontexts = 1; // allow scrolling right from maxcontexts to 1
   else if(r_numcontexts > maxcontexts)
   {
      C_Printf(FC_ERROR "Warning: r_numcontexts's current maximum is %d, resetting to 1", maxcontexts);
      r_numcontexts = 1;
   }

   P_CheckSpriteTouchingSectorLists();

   I_SetMode();
}

//
// True if conditions met to have thorough sprite collection when projecting them.
//
bool R_NeedThoroughSpriteCollection()
{
   return !nodrawers && r_sprprojstyle != R_SPRPROJSTYLE_FAST && 
      (r_numcontexts != 1 || r_sprprojstyle != R_SPRPROJSTYLE_DEFAULT);
}

// EOF

