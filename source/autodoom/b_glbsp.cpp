// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//      C interface with GLBSP module
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"

#include "b_botmap.h"
#include "b_botmaptemp.h"
#include "b_glbsp.h"
#include "b_util.h"
#include "../c_io.h"
#include "../m_bbox.h"
#include "../m_buffer.h"
#include "../p_info.h"
#include "../p_maputl.h"
#include "../r_state.h"
#include "glbsp/glbsp.h"
#include "../v_misc.h"

static const DLListItem<TempBotMap::Vertex> *vertListHead;
static const DLListItem<MetaSector> *msecListHead;
static const DLListItem<MetaSector> *msecListHead2;
static const DLListItem<TempBotMap::Line> *lineListHead;
static int vertIndex;
static int msecIndex;
static PODCollection<TempBotMap::Vertex *> vertRefColl;
static PODCollection<MetaSector *> msecRefColl;

//
// B_GLBSP_setupReferences
//
// Sets up the references for TempBotMap data
//
static void B_GLBSP_setupReferences()
{
   vertListHead = tempBotMap->vertGet();
   msecListHead = tempBotMap->msecGet();
   msecListHead2 = tempBotMap->msecGet();
   lineListHead = tempBotMap->lineGet();
   vertIndex = 0;
   msecIndex = 0;
   vertRefColl.clear();
   msecRefColl.clear();
}

static int barLimit[NUMOFGUITYPES];
inline static void nullFunc() {}
inline static boolean_g gbDisplayOpen(displaytype_e type)
{
   return TRUE;
}
inline static void gbSetTitle(const char *str)
{
   C_Printf(FC_HI "%s\n", str);   // just output it
}
inline static void gbSetBarText(int barnum, const char *str)
{
   C_Printf("->%s\n", str);
}
inline static void gbSetBarLimit(int barnum, int limit)
{
   V_SetLoading(limit, "Bot nodes");
   barLimit[barnum] = limit;
}
inline static void gbSetBar(int barnum, int count)
{
   V_LoadingUpdateTicked(count);
   if (count == barLimit[barnum])
      C_Puts("Done.");
}

//
// B_GLBSP_Start
//
// Starts the GLBSP worker on current tempBotMap data
//
void B_GLBSP_Start()
{
   B_GLBSP_setupReferences();
   
   // Setup the functions
   nodebuildfuncs_t funcs;
   
   funcs.fatal_error = I_Error;  // quit AutoDoom if fatal error
   funcs.print_msg = C_Printf;   // log info to console
   funcs.ticker = nullFunc;
   funcs.display_open = gbDisplayOpen;
   funcs.display_setTitle = gbSetTitle;
   funcs.display_setBarText = gbSetBarText;
   funcs.display_setBarLimit = gbSetBarLimit;
   funcs.display_setBar = gbSetBar;
   funcs.display_close = nullFunc;
   
   // Initialize the comm array
   volatile nodebuildcomms_t comms = default_buildcomms;
   
   // Initialize the parameter array
   nodebuildinfo_t info = default_buildinfo;
   info.no_reject = TRUE;
   info.no_progress = TRUE;
   info.no_normal = TRUE;
   info.no_prune = TRUE;
   
   info.factor = 32;

   info.input_file = GlbspStrDup(LevelInfo.levelName);   // display level name
   
   // Begin
   if (GlbspBuildNodes(&info, &funcs, &comms) != GLBSP_E_OK)
   {
      // TODO: WE HAVE A PROBLEM HERE.
   }
   
   
   // Free memory
   GlbspFree(info.input_file);
   msecRefColl.clear();
   vertRefColl.clear();
}

//
// B_GLBSP_GetNextVertex
//
// Gets the next vertex from the list, returning two floating-point numbers
// Returns 0 if reached the end
//
int B_GLBSP_GetNextVertex(double *coordx, double *coordy)
{
   if (!vertListHead)
      return 0;
   *coordx = M_FixedToDouble(vertListHead->dllObject->x);
   *coordy = M_FixedToDouble(vertListHead->dllObject->y);
   
   // Add reference to collection
   vertRefColl.add(vertListHead->dllObject);
   
   // set the index for future use
   tempBotMap->setItemIndex(vertIndex++, vertListHead->dllObject);
   
   vertListHead = vertListHead->dllNext;
   return 1;
}

//
// B_GLBSP_GetNextSector
//
// Gets the next metasector from the list, returning the heights.
// Returns 0 if reached the end
//
int B_GLBSP_GetNextSector(int *floor_h, int *ceil_h)
{
   if(!msecListHead)
   {
      return 0;
   }
   
   // probably only for reference, they don't technically influence the node
   // system
   *floor_h = msecListHead->dllObject->getFloorHeight() >> FRACBITS;
   *ceil_h = msecListHead->dllObject->getCeilingHeight() >> FRACBITS;
   
   // add to collection of indices
   msecRefColl.add(msecListHead->dllObject);
   
   // set the index for future use
   tempBotMap->setItemIndex(msecIndex++, msecListHead->dllObject);
   
   msecListHead = msecListHead->dllNext;
   return 1;
}

//
// B_GLBSP_GetNextSidedef
//
// Returns a dummy sidedef with a sector reference. Must be one per each meta-
// sector
//
int B_GLBSP_GetNextSidedef(int *index)
{
   if(!msecListHead2)
      return 0;
   
   *index = msecListHead2->dllData;  // return the index of the line
   
   msecListHead2 = msecListHead2->dllNext;
   return 1;
}

//
// B_GLBSP_GetNextLinedef
//
// Returns the line from the given botmap line.
//
int B_GLBSP_GetNextLinedef(int *startIdx, int *endIdx, int *rightIdx,
                           int *leftIdx, int* tag)
{
   if(!lineListHead)
      return 0;
   
   *startIdx = lineListHead->dllObject->v1->listLink.dllData;
   *endIdx = lineListHead->dllObject->v2->listLink.dllData;
   
   const MetaSector *msec = lineListHead->dllObject->metasec[0];
   if (msec == botMap->nullMSec)
      *rightIdx = -1;
   else
      *rightIdx = msec->listLink.dllData;
   
   msec = lineListHead->dllObject->metasec[1];
   if(msec == botMap->nullMSec)
      *leftIdx = -1;
   else
      *leftIdx = msec->listLink.dllData;

   *tag = lineListHead->dllObject->assocLine ? (int)(lineListHead->dllObject->assocLine - ::lines) : -1;
   
   lineListHead = lineListHead->dllNext;
   return 1;
}

//
// B_GLBSP_CreateVertexArray
//
// Creates the new vertex array
//
void B_GLBSP_CreateVertexArray(int numverts)
{
   botMap->vertices = emalloc(BotMap::Vertex *,
                              (botMap->numverts = numverts) *
                              sizeof(BotMap::Vertex));
}

//
// B_GLBSP_PutVertex
//
// Obtains new vertex from GLBSP output and places it in botMap's list
//
void B_GLBSP_PutVertex(double coordx, double coordy, int index)
{
	botMap->vertices[index].x = M_DoubleToFixed(coordx);
	botMap->vertices[index].y = M_DoubleToFixed(coordy);
}

//
// B_GLBSP_CreateSegArray
//
// Creates a new seg array
//
void B_GLBSP_CreateSegArray(int numsegs)
{
   for (int i = 0; i < numsegs; ++i)
      botMap->segs.add();
}

//
// B_GLBSP_PutSegment
//
// Puts map seg
//
void B_GLBSP_PutSegment(int v1idx, int v2idx, int back, int lnidx, int part,
                        int sgidx)
{
   BotMap::Seg &sg = botMap->segs[sgidx];
   sg.owner = nullptr;

   sg.v[0] = botMap->vertices + v1idx;
   sg.v[1] = botMap->vertices + v2idx;

   if(v1idx >= botMap->numverts || v2idx >= botMap->numverts)
      puts("!!!");
   sg.dx = sg.v[1]->x - sg.v[0]->x;
   sg.dy = sg.v[1]->y - sg.v[0]->y;
   sg.isback = back ? true : false;
   sg.ln = lnidx >= 0 ? botMap->lines + lnidx : NULL;

   if (part >= 0 && sgidx > part)
   {
      // Set partner, if it has been created already
      sg.partner = &botMap->segs[part];
      botMap->segs[part].partner = &sg;
   }
   
   // mid-point
   sg.mid.x = (sg.v[0]->x + sg.v[1]->x) >> 1;
   sg.mid.y = (sg.v[0]->y + sg.v[1]->y) >> 1;
   
   // put into blockmap
   botMap->getTouchedBlocks(sg.v[0]->x, sg.v[0]->y, sg.v[1]->x, sg.v[1]->y,
                            [&sg](int b)->void
                            {
                               botMap->segBlocks[b].add(&sg);
                               sg.blocklist.add(b);
                            });
   
   // Bounding box
   if(sg.v[0]->x < sg.v[1]->x)
   {
      sg.bbox[BOXLEFT]  = sg.v[0]->x;
      sg.bbox[BOXRIGHT] = sg.v[1]->x;
   }
   else
   {
      sg.bbox[BOXLEFT]  = sg.v[1]->x;
      sg.bbox[BOXRIGHT] = sg.v[0]->x;
   }
   
   if(sg.v[0]->y < sg.v[1]->y)
   {
      sg.bbox[BOXBOTTOM] = sg.v[0]->y;
      sg.bbox[BOXTOP]    = sg.v[1]->y;
   }
   else
   {
      sg.bbox[BOXBOTTOM] = sg.v[1]->y;
      sg.bbox[BOXTOP]    = sg.v[0]->y;
   }
}

//
// B_GLBSP_CreateLineArray
//
// create new line array
//
void B_GLBSP_CreateLineArray(int numlines)
{
   botMap->lines = emalloc(BotMap::Line *,
                           (botMap->numlines = numlines) *
                           sizeof(BotMap::Line));
}

//
// B_GLBSP_PutLine
//
// create new line array
//
void B_GLBSP_PutLine(int v1idx, int v2idx, int s1idx, int s2idx, int lnidx, int tag)
{
   BotMap::Line &ln = botMap->lines[lnidx];

   ln.v[0] = botMap->vertices + v1idx;
   ln.v[1] = botMap->vertices + v2idx;
   ln.msec[0] = s1idx >= 0 ? msecRefColl[s1idx] : botMap->nullMSec;
   ln.msec[1] = s2idx >= 0 ? msecRefColl[s2idx] : botMap->nullMSec;
   ln.specline = tag >= 0 ? ::lines + tag : nullptr;

   // put into blockmap
   botMap->getTouchedBlocks(ln.v[0]->x, ln.v[0]->y, ln.v[1]->x, ln.v[1]->y,
                            [&ln](int b)->void
                            {
                               botMap->lineBlocks[b].add(&ln);
                            });
}

//
// B_GLBSP_CreateSubsectorArray
//
// create subsec array
//
void B_GLBSP_CreateSubsectorArray(int numssecs)
{
   for (int i = 0; i < numssecs; ++i)
   {
      botMap->ssectors.add();
   }
}
void B_GLBSP_PutSubsector(int first, int num, int ssidx)
{
   BotMap::Subsec &ss = botMap->ssectors[ssidx];
   ss.segs = &botMap->segs[first];
   ss.nsegs = num;
   ss.msector = nullptr;   // shouldn't stay
   double A = 0, x0, y0, x1, y1, tmp, Cx = 0, Cy = 0;
   ss.neighs.reserve(num); // THIS MUST BE CALLED TO AVOID REALLOCATIONS
   PODCollection<BNeigh*> neighRefs;
   neighRefs.reserve(num);

   for (int i = 0; i < num; ++i)
   {
      BotMap::Seg &sg = botMap->segs[i + first];
      // set the owner reference from this seg to this subsector
      sg.owner = &ss;

      // Set the neighbours
      if (sg.partner && sg.partner->owner)
      {
          if (ssidx == (int)(sg.partner->owner - &botMap->ssectors[0]))
          {
             B_Log("WARNING: same-subsector neigh at seg %g %g %g %g",
                   M_FixedToDouble(sg.v[0]->x), M_FixedToDouble(sg.v[0]->y),
                   M_FixedToDouble(sg.v[1]->x), M_FixedToDouble(sg.v[1]->y));
             goto dorest;
          }
          BNeigh n;
          n.dist = 0; // to be updated later
          n.otherss = sg.partner->owner;
          n.myss = sg.owner;
          n.v.x = sg.v[0]->x;
          n.v.y = sg.v[0]->y;
          n.d.x = sg.dx;
          n.d.y = sg.dy;
          n.line = sg.ln;
          ss.neighs.add(n);

          n.otherss = &ss;
          n.myss = sg.partner->owner;
          n.v.x = sg.partner->v[0]->x;
          n.v.y = sg.partner->v[0]->y;
          n.d.x = sg.partner->dx;
          n.d.y = sg.partner->dy;
          n.line = sg.partner->ln;
          sg.partner->owner->neighs.add(n);

          neighRefs.add(&sg.partner->owner->neighs.back());
      }
   dorest:
      // set the metasector if not set already
      if (!ss.msector && sg.ln)
         ss.msector = sg.ln->msec[sg.isback];
      x0 = M_FixedToDouble(sg.v[0]->x);
      y1 = M_FixedToDouble(sg.v[1]->y);
      x1 = M_FixedToDouble(sg.v[1]->x);
      y0 = M_FixedToDouble(sg.v[0]->y);
      A += (tmp = x0 * y1 - x1 * y0);
      Cx += tmp * (x0 + x1);
      Cy += tmp * (y0 + y1);
   }
   A /= 2;
   Cx /= A * 6;
   Cy /= A * 6;
   
   ss.mid.x = M_DoubleToFixed(Cx);
   ss.mid.y = M_DoubleToFixed(Cy);

   // Update these because you can
   fixed_t dist;
   int i = 0;
   for(BNeigh& n : ss.neighs)
   {
       dist = (ss.mid - n.otherss->mid).sqrtabs();
      n.dist = dist;
      neighRefs[i++]->dist = dist;
   }
}

void B_GLBSP_CreateNodeArray(int numnodes)
{
   botMap->nodes = emalloc(BotMap::Node *,
                           (botMap->numnodes = numnodes) *
                           sizeof(BotMap::Node));
}
void B_GLBSP_PutNode(double x, double y, double dx, double dy, int rnode,
                     int lnode, int riss, int liss, int ndidx)
{
   botMap->nodes[ndidx].x = M_DoubleToFixed(x);
   botMap->nodes[ndidx].y = M_DoubleToFixed(y);
   botMap->nodes[ndidx].dx = M_DoubleToFixed(dx);
   botMap->nodes[ndidx].dy = M_DoubleToFixed(dy);

   //int adx = D_abs(dx), ady = D_abs(dy);
   //if(ady > adx ? ady < 10 : adx < 10)
   //{
   //   botMap->nodes[ndidx].dx <<= 4;
   //   botMap->nodes[ndidx].dy <<= 4;
   //}
   botMap->nodes[ndidx].child[0] = rnode | (riss ? NF_SUBSECTOR : 0);
   botMap->nodes[ndidx].child[1] = lnode | (liss ? NF_SUBSECTOR : 0);
   //if(!dx && !dy)
   //   printf("WARNING: micro-node at %d %d\n", x, y);
}

// EOF

