//
// The Eternity Engine
// Copyright (C) 2025 James Haley, Stephen McGranahan, et al.
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//  VBuffer has become a much more all-purpose solution for many video handling
//  needs. Simple, but powerful pixelbuffer object.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "m_compare.h"
#include "v_buffer.h"
#include "v_misc.h"
#include "v_patch.h"
#include "r_state.h"

//
// VB_AllocateData
//
static void VB_AllocateData(VBuffer *buffer)
{
   if(buffer->data && buffer->owndata)
      efree(buffer->data);

   buffer->data = ecalloc(byte *, buffer->width*buffer->height, buffer->pixelsize);
   buffer->owndata = true;
}

//
// VB_SetData
//
static void VB_SetData(VBuffer *buffer, byte *pixels)
{
   if(buffer->data && buffer->owndata)
      efree(buffer->data);

   buffer->data = pixels;
   buffer->owndata = false;
}

//
// V_InitVBuffer
//
// Initializes the given vbuffer and allocates pixeldata for it.
//
void V_InitVBuffer(VBuffer *vb, int width, int height, int bitdepth)
{
   int     psize;

   if(width < 0 || height < 0)
   {
      I_Error("V_InitVBuffer: Invalid dimensions %dx%d\n",
              width, height);
   }

   if(bitdepth != 8)
      I_Error("V_InitVBuffer: Invalid bitdepth %d\n", bitdepth);

   psize = bitdepth / 8;

   memset(vb, 0, sizeof(*vb));
   vb->width = width;
   vb->height = height;
   vb->pixelsize = psize;
   vb->pitch = height * psize;
   vb->scaled = false;
   vb->needfree = false;
   vb->freelookups = false;

   VB_AllocateData(vb);

   V_SetupBufferFuncs(vb, DRAWTYPE_UNSCALED);
}

//
// V_CreateVBuffer
//
// Allocates a new VBuffer object and returns it
//
VBuffer *V_CreateVBuffer(int width, int height, int bitdepth)
{
   VBuffer *ret;

   if(width < 0 || height < 0)
   {
      I_Error("V_CreateVBuffer: Invalid dimensions %dx%d\n",
              width, height);
   }

   if(bitdepth != 8)
      I_Error("V_CreateVBuffer: Invalid bitdepth %d\n", bitdepth);

   ret = estructalloc(VBuffer, 1);

   V_InitVBuffer(ret, width, height, bitdepth);
   ret->needfree = true;

   return ret;
}

//
// V_InitVBufferFrom
//
// Gives a VBuffer object the given pixeldata. The VBuffer does not OWN the
// given data and so it will not be freed by V_FreeVBuffer
//
void V_InitVBufferFrom(VBuffer *vb, int width, int height, int pitch, 
                       int bitdepth, byte *data)
{
   int     psize;

   if(width < 0 || height < 0)
   {
      I_Error("V_CreateVBufferFrom: Invalid dimensions %dx%d\n", 
              width, height);
   }

   if(bitdepth != 8)
      I_Error("V_CreateVBufferFrom: Invalid bitdepth %d\n", bitdepth);

   psize = bitdepth / 8;
   memset(vb, 0, sizeof(VBuffer));

   vb->width = width;
   vb->height = height;
   vb->pixelsize = psize;
   vb->pitch = pitch;
   vb->scaled = false;
   vb->needfree = false;
   vb->freelookups = false;

   V_SetupBufferFuncs(vb, DRAWTYPE_UNSCALED);

   VB_SetData(vb, data);
}

//
// V_CreateVBufferFrom
//
// Allocates a new VBuffer object with the given pixeldata. The VBuffer created
// by this function does not OWN the given data and so it will not be freed by
// V_FreeVBuffer
//
VBuffer *V_CreateVBufferFrom(int width, int height, int pitch, 
                             int bitdepth, byte *data)
{
   VBuffer *ret;

   if(width <= 0 || height <= 0)
   {
      I_Error("V_CreateVBufferFrom: Invalid dimensions %dx%d\n", 
              width, height);
   }

   if(bitdepth != 8)
      I_Error("V_CreateVBufferFrom: Invalid bitdepth %d\n", bitdepth);

   ret = estructalloc(VBuffer, 1);

   V_InitVBufferFrom(ret, width, height, pitch, bitdepth, data);
   ret->needfree = true;

   return ret;
}

//
// V_InitSubVBuffer
//
// Inits a VBuffer object to share pixel data with another. 
//
void V_InitSubVBuffer(VBuffer *vb, VBuffer *parent, int x, int y, 
                      int width, int height)
{
   int     psize;

   if(!parent)
      return;

   if(width < 0 || height < 0)
   {
      I_Error("V_SubVBuffer: Invalid dimensions %dx%d\n", 
              width, height);
   }

   if(x < 0 || y < 0 || 
      x + width - 1 > parent->width || y + height - 1 > parent->height)
   {
      I_Error("V_SubVBuffer: Invalid dimensions: x=%d, y=%d, w=%d, h=%d\n", 
              x, y, width, height);
   }

   psize = parent->pixelsize;
   memset(vb, 0, sizeof(VBuffer));

   vb->pitch = parent->pitch;
   vb->width = width;
   vb->height = height;
   vb->pixelsize = psize;
   vb->subx = x;
   vb->suby = y;
   vb->scaled = false;
   vb->needfree = false;
   vb->freelookups = false;

   VB_SetData(vb, parent->data + x * parent->pitch + y * parent->pixelsize);

   V_SetupBufferFuncs(vb, DRAWTYPE_UNSCALED);
}

//
// V_SubVBuffer
//
// Allocates a new VBuffer object that shares pixel data with another. 
//
VBuffer *V_SubVBuffer(VBuffer *parent, int x, int y, int width, int height)
{
   VBuffer *ret;

   ret = estructalloc(VBuffer, 1);
   V_InitSubVBuffer(ret, parent, x, y, width, height);
   ret->needfree = true;

   return ret;
}

//
// V_FreeVBuffer
//
// Frees the given VBuffer object. If the VBuffer owns it's pixeldata, the data
// will be freed. If the VBuffer has child buffers (created with V_SubVBuffer)
// the child buffers will be given their own pixeldata.
//
void V_FreeVBuffer(VBuffer *buffer)
{
   V_UnsetScaling(buffer);

   if(buffer->owndata)
   {
      efree(buffer->data);
      buffer->data = nullptr;
      buffer->owndata = false;
   }

   if(buffer->needfree)
      efree(buffer);
   else
      memset(buffer, 0, sizeof(VBuffer));
}

//
// V_UnsetScaling
//
void V_UnsetScaling(VBuffer *buffer)
{
   if(!buffer || !buffer->scaled)
      return;

   buffer->scaled = false;
   buffer->unscaledw = buffer->unscaledh = 0;
   buffer->ixscale = buffer->iyscale = 0;

   if(buffer->freelookups)
   {
      efree(buffer->x1lookup);
      efree(buffer->x2lookup);
      efree(buffer->y1lookup);
      efree(buffer->y2lookup);
   }

   buffer->x1lookup = buffer->x2lookup 
      = buffer->y1lookup = buffer->y2lookup = nullptr;

   V_SetupBufferFuncs(buffer, DRAWTYPE_UNSCALED);
}

//
// V_SetScaling
//
void V_SetScaling(VBuffer *buffer, int unscaledw, int unscaledh)
{
   int     i;
   fixed_t frac, lastfrac;

   if(!buffer)
      return;

   if(buffer->scaled)
   {
      if(buffer->unscaledw == unscaledw && buffer->unscaledh == unscaledh)
         return;

      V_UnsetScaling(buffer);
   }
   else
      buffer->scaled = true;

   buffer->unscaledw = unscaledw;
   buffer->unscaledh = unscaledh;

   if(unscaledw == SCREENWIDTH && unscaledh == SCREENHEIGHT 
      && buffer->width == video.width && buffer->height == video.height)
   {
      buffer->x1lookup = video.x1lookup;
      buffer->x2lookup = video.x2lookup;
      buffer->y1lookup = video.y1lookup;
      buffer->y2lookup = video.y2lookup;
      buffer->freelookups = false;
   }
   else
   {
      int size = sizeof(int) * (emax(unscaledw, unscaledh) + 1);

      buffer->x1lookup = ecalloc(int *, 1, size);
      buffer->x2lookup = ecalloc(int *, 1, size);
      buffer->y1lookup = ecalloc(int *, 1, size);
      buffer->y2lookup = ecalloc(int *, 1, size);
      buffer->freelookups = true;
   }

   buffer->ixscale = ((unscaledw << FRACBITS) / buffer->width) + 1;
   buffer->iyscale = ((unscaledh << FRACBITS) / buffer->height) + 1;


   buffer->x1lookup[0] = 0;
   lastfrac = frac = 0;
   for(i = 0; i < buffer->width; i++)
   {
      if(frac >> FRACBITS > lastfrac >> FRACBITS)
      {
         buffer->x1lookup[frac >> FRACBITS] = i;
         buffer->x2lookup[lastfrac >> FRACBITS] = i-1;
         lastfrac = frac;
      }

      frac += buffer->ixscale;
   }
   buffer->x2lookup[unscaledw - 1] = buffer->width - 1;
   buffer->x1lookup[unscaledw] = buffer->x2lookup[unscaledw] = buffer->width;

   buffer->y1lookup[0] = 0;
   lastfrac = frac = 0;
   for(i = 0; i < buffer->height; i++)
   {
      if(frac >> FRACBITS > lastfrac >> FRACBITS)
      {
         buffer->y1lookup[frac >> FRACBITS] = i;
         buffer->y2lookup[lastfrac >> FRACBITS] = i-1;
         lastfrac = frac;
      }

      frac += buffer->iyscale;
   }
   buffer->y2lookup[unscaledh - 1] = buffer->height - 1;
   buffer->y1lookup[unscaledh] = buffer->y2lookup[unscaledh] = buffer->height;

   V_SetupBufferFuncs(buffer, DRAWTYPE_GENSCALED);
}


//
// V_BlitVBuffer
//
// SoM: blit from one vbuffer to another
//
void V_BlitVBuffer(VBuffer *dest, int dx, int dy, VBuffer *src, 
                   unsigned int sx, unsigned int sy, unsigned int width, 
                   unsigned int height)
{
   byte *dbuf, *sbuf;
   int i = height;
   int slice = width, dpitch = dest->pitch, spitch = src->pitch;

   if(dx < 0)
   {
      slice += dx;
      sx += D_abs(dx);
      dx = 0;
   }

   if(slice < 0 || (int)sx >= src->width)
      return;

   if(dy < 0)
   {
      i += dy;
      sy += D_abs(dy);
      dy = 0;
   }

   if((int)sy >= src->height || i < 0)
      return;

   if(dx + slice > dest->width)
      slice = dest->width - dx;
   if(dy + i > dest->height)
      i = dest->height - dy;

   if((int)sx + slice > src->width)
      slice = src->width - (int)sx;

   if((int)sy + i > src->height)
      i = src->height - (int)sy;

   if(slice < 0 || i < 0)
      return;

   dbuf = dest->data + (dpitch * dx) + dy;
   sbuf = src->data + (spitch * sx) + sy;

   while(slice--)
   {
      memcpy(dbuf, sbuf, i);
      dbuf += dpitch;
      sbuf += spitch;
   }
}

//
// VBuffer::getRealAspectRatio
//
// Returns the ratio of width / height.
//
fixed_t VBuffer::getRealAspectRatio() const
{
   return width * FRACUNIT / height;
}

//
// VBuffer::getVirtualAspectRatio
//
// As above, but lies if the screen is in a "legacy" 16:10 mode
// (320x200 or 640x400).
//
fixed_t VBuffer::getVirtualAspectRatio() const
{
   if((width == 320 && height == 200) ||
      (width == 640 && height == 400))
      return 4 * FRACUNIT / 3;
   else
      return getRealAspectRatio();
}

//
// Maps an unscaled x value from one buffer to this one.
//
int VBuffer::mapXFromOther(const int x, const VBuffer &other) const
{
   if(&other == this)
      return x;

   float screenX = float(other.subx - this->subx);
   if(other.scaled)
      screenX += float(other.x1lookup[x]);
   else
      screenX += float(x);

   if(!this->scaled)
      return int(screenX);
   else
      return int(screenX * this->unscaledw / this->width);
}

//
// Maps an unscaled y value from one buffer to this one.
//
int VBuffer::mapYFromOther(const int y, const VBuffer &other) const
{
   if(&other == this)
      return y;

   float screenY = float(other.suby - this->suby);
   if(other.scaled)
      screenY += float(other.y1lookup[y]);
   else
      screenY += float(y);

   if(!this->scaled)
      return int(screenY);
   else
      return int(screenY * this->unscaledh / this->height);
}

// EOF

