// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   
//   SDL-specific GL 2D-in-3D video code
//
//-----------------------------------------------------------------------------

#ifdef EE_FEATURE_OPENGL

// SDL headers
#include "SDL.h"
#include "SDL_opengl.h"

// DOOM headers
#include "../z_zone.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../v_misc.h"
#include "../v_video.h"
#include "../version.h"
#include "../w_wad.h"

// Local driver header
#include "i_sdlgl2d.h"

// GL module headers
#include "../gl/gl_primitives.h"
#include "../gl/gl_projection.h"
#include "../gl/gl_texture.h"
#include "../gl/gl_vars.h"

//=============================================================================
//
// WM-related stuff (see i_input.c)
//

void UpdateGrab(void);
bool MouseShouldBeGrabbed(void);
void UpdateFocus(void);

//=============================================================================
//
// Static Data
//

// Surface returned from SDL_SetVideoMode; not really useful for anything.
static SDL_Surface *surface;

static bool havecolors;

// Temporary screen surface; this is what the game will draw itself into.
static SDL_Surface *screen; 
static SDL_Surface *colors;

// 32-bit converted palette for translation of the screen to 32-bit pixel data.
static Uint32    RGB8to32[256][256];
static byte      cachedpal[768];
static SDL_Color cachedpalcols[256];

// GL texture sizes sufficient to hold the screen buffer as a texture
static unsigned int framebuffer_umax;
static unsigned int framebuffer_vmax;
static unsigned int texturesize;

// maximum texture coordinates to put on right- and bottom-side vertices
static GLfloat texcoord_smax;
static GLfloat texcoord_tmax;

// GL texture names
static GLuint textureid;

// Framebuffer texture data
static Uint32 *framebuffer;

// Bump amount used to avoid cache misses on power-of-two-sized screens
static int bump;

// Options
static bool   use_arb_pbo; // If true, use ARB pixel buffer object extension
static GLuint pboIDs[2];   // IDs of pixel buffer objects

// PBO extension function pointers
static PFNGLGENBUFFERSARBPROC    pglGenBuffersARB    = NULL;
static PFNGLDELETEBUFFERSARBPROC pglDeleteBuffersARB = NULL;
static PFNGLBINDBUFFERARBPROC    pglBindBufferARB    = NULL;
static PFNGLBUFFERDATAARBPROC    pglBufferDataARB    = NULL;
static PFNGLMAPBUFFERARBPROC     pglMapBufferARB     = NULL;
static PFNGLUNMAPBUFFERARBPROC   pglUnmapBufferARB   = NULL;

//=============================================================================
//
// Graphics Code
//

static void (SDLGL2DVideoDriver::*draw)(void *, unsigned int);

//
// SDLGL2DVideoDriver::DrawPixels
//
// Protected method.
//
void SDLGL2DVideoDriver::DrawPixels(void *buffer, unsigned int destwidth)
{
   Uint32 *fb = (Uint32 *)buffer;

   for(int y = 0; y < screen->h; y++)
   {
      byte   *src  = (byte *)screen->pixels + y * screen->pitch;
      Uint32 *dest = fb + y * destwidth;

      for(int x = 0; x < screen->w - bump; x++)
      {
         *dest = RGB8to32[0][*src];
         ++src;
         ++dest;
      }
   }
}

//
// SDLGL2DVideoDriver::DrawColorPixels
//
// Protected method.
//
void SDLGL2DVideoDriver::DrawColorPixels(void *buffer, unsigned int destwidth)
{
   Uint32 *fb = (Uint32 *)buffer;

   for(int y = 0; y < screen->h; y++)
   {
      byte   *src  = (byte *)screen->pixels + y * screen->pitch;
      byte   *col  = (byte *)colors->pixels + y * colors->pitch;
      Uint32 *dest = fb + y * destwidth;

      for(int x = 0; x < screen->w - bump; x++)
      {
         *dest = RGB8to32[*col][*src];
         ++src;
         ++col;
         ++dest;
      }
   }
}

//
// SDLGL2DVideoDriver::FinishUpdate
//
void SDLGL2DVideoDriver::FinishUpdate()
{
   // haleyjd 10/08/05: from Chocolate DOOM:
   UpdateGrab();

   // Don't update the screen if the window isn't visible.
   // Not doing this breaks under Windows when we alt-tab away 
   // while fullscreen.   
   if(!(SDL_GetAppState() & SDL_APPACTIVE))
      return;

   if(colors && havecolors)
      draw = &SDLGL2DVideoDriver::DrawColorPixels;
   else
      draw = &SDLGL2DVideoDriver::DrawPixels;

   if(!use_arb_pbo)
   {
      // Convert the game's 8-bit output to the 32-bit texture buffer
      (this->*draw)(framebuffer, (unsigned int)video.width);

      // bind the framebuffer texture if necessary
      GL_BindTextureIfNeeded(textureid);

      // update the texture data
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                      (GLsizei)video.width, (GLsizei)video.height, 
                      GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid *)framebuffer);
   }
   else
   {
      static int pboindex  = 0;
      int        nextindex = 0;
      GLvoid    *ptr       = NULL;

      // use the two pixel buffers in a rotation
      pboindex  = (pboindex + 1) % 2;
      nextindex = (pboindex + 1) % 2;

      // bind the framebuffer texture if necessary
      GL_BindTextureIfNeeded(textureid);

      // bind the primary PBO
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[pboindex]);

      // copy primary PBO to texture, using offset
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)framebuffer_umax,
                   (GLsizei)framebuffer_vmax, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

      // bind the secondary PBO
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[nextindex]);

      // map the PBO into client memory in such a way as to avoid stalls
      pglBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, texturesize, 0, GL_STREAM_DRAW_ARB);

      if((ptr = pglMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB)))
      {
         // draw directly into video memory
         (this->*draw)(ptr, framebuffer_umax);

         // release pointer
         pglUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
      }

      // Unbind all PBOs
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
   }

   // push screen quad w/tex coords
   glBegin(GL_QUADS);
   GL_OrthoQuadTextured(0.0f, 0.0f, (GLfloat)video.width, (GLfloat)video.height,
                        texcoord_smax, texcoord_tmax);
   glEnd();

   // push the frame
   SDL_GL_SwapBuffers();
}

//
// SDLGL2DVideoDriver::ReadScreen
//
void SDLGL2DVideoDriver::ReadScreen(byte *scr)
{
   if(bump == 0 && screen->pitch == screen->w)
   {
      // full block blit
      memcpy(scr, (byte *)screen->pixels, video.width * video.height);
   }
   else
   {
      // must copy one row at a time
      for(int y = 0; y < screen->h; y++)
      {
         byte *src  = (byte *)screen->pixels + y * screen->pitch;
         byte *dest = scr + y * video.width;

         memcpy(dest, src, screen->w - bump);
      }
   }
}

//
// SDLGL2DVideoDriver::InitDiskFlash
//
void SDLGL2DVideoDriver::InitDiskFlash()
{
   // Not implemented.
}

//
// SDLGL2DVideoDriver::BeginRead
//
void SDLGL2DVideoDriver::BeginRead()
{
   // Not implemented.
}

//
// SDLGL2DVideoDriver::EndRead
//
void SDLGL2DVideoDriver::EndRead()
{
   // Not implemented.
}

//
// I_loadLights
//
// Calculate static light colors
//
static void I_loadLights()
{
   int lump;

   if((lump = wGlobalDir.checkNumForName("PALLIGHT")) > 0 &&
      wGlobalDir.lumpLength(lump) == 768)
   {
      byte *lights = (byte *)wGlobalDir.cacheLumpNum(lump, PU_CACHE);
      havecolors = true;

      // tweak white light
      for(int i = 0; i < 256; i++)
      {
         Uint8 &r = cachedpalcols[i].r;
         Uint8 &g = cachedpalcols[i].g;
         Uint8 &b = cachedpalcols[i].b;
         Uint32 t;
         r = (Uint8)((t = (Uint32)r + (r > 160 ? r - 160 : 0)) > 255 ? 255 : t);
         g = (Uint8)((t = (Uint32)g + (g > 160 ? g - 160 : 0)) > 255 ? 255 : t);
         b = (Uint8)((t = (Uint32)b + (b > 160 ? b - 160 : 0)) > 255 ? 255 : t);
         RGB8to32[0][i] = ((Uint32)0xff << 24) | (r << 16) | (g <<  8) | b;
      }

      // copy base palette to color palettes
      byte *light = lights+3;
      for(int i = 1; i < 256; i++)
      {
         memcpy(RGB8to32[i], RGB8to32[0], 256*sizeof(Uint32));

         // modulate with light
         byte lr = *light++;
         byte lg = *light++;
         byte lb = *light++;

         if(lr | lg | lb)
         {
            for(int j = 0; j < 256; j++)
            {
               Uint32 r = ((Uint32)cachedpalcols[j].r * lr) / 256;
               Uint32 g = ((Uint32)cachedpalcols[j].g * lg) / 256;
               Uint32 b = ((Uint32)cachedpalcols[j].b * lb) / 256;
               RGB8to32[i][j] = ((Uint32)0xff << 24) | (r << 16) | (g <<  8) | b;
            }
         }
      }
   }
   else
      havecolors = false;
}

//
// SDLGL2DVideoDriver::SetPalette
//
void SDLGL2DVideoDriver::SetPalette(byte *pal)
{
   byte *temppal;
   SDL_Color *col;

   // Cache palette if a new one is being set (otherwise the gamma setting is 
   // being changed)
   if(pal)
      memcpy(cachedpal, pal, 768);

   temppal = cachedpal;
   col     = cachedpalcols;
 
   // Create 32-bit translation lookup
   for(int i = 0; i < 256; i++)
   {
      RGB8to32[0][i] =
         ((Uint32)0xff << 24) |
         ((Uint32)(col->r = gammatable[usegamma][*(temppal + 0)]) << 16) |
         ((Uint32)(col->g = gammatable[usegamma][*(temppal + 1)]) <<  8) |
         ((Uint32)(col->b = gammatable[usegamma][*(temppal + 2)]) <<  0);
      
      temppal += 3;
      ++col;
   }
   I_loadLights();
}

//
// SDLGL2DVideoDriver::SetPrimaryBuffer
//
void SDLGL2DVideoDriver::SetPrimaryBuffer()
{
   // Bump up size of power-of-two framebuffers
   if(video.width == 512 || video.width == 1024 || video.width == 2048)
      bump = 4;
   else
      bump = 0;

   // Create screen surface for the high-level code to render the game into
   screen = SDL_CreateRGBSurface(SDL_SWSURFACE, video.width + bump, video.height,
                                 8, 0, 0, 0, 0);

   // Create colors surface
   colors = SDL_CreateRGBSurface(SDL_SWSURFACE, video.width + bump, video.height,
                                 8, 0, 0, 0, 0);

   if(!screen)
      I_Error("SDLGL2DVideoDriver::SetPrimaryBuffer: failed to create screen temp buffer\n");

   // Point screens[0] to 8-bit temp buffer
   video.screens[0] = (byte *)(screen->pixels);
   video.pitch      = screen->pitch;
}

//
// SDLGL2DVideoDriver::UnsetPrimaryBuffer
//
void SDLGL2DVideoDriver::UnsetPrimaryBuffer()
{
   if(screen)
   {
      SDL_FreeSurface(screen);
      screen = NULL;
   }
   video.screens[0] = NULL;
}

//
// SDLGL2DVideoDriver::ShutdownGraphics
//
void SDLGL2DVideoDriver::ShutdownGraphics()
{
   ShutdownGraphicsPartway();

   // quit SDL video
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

//
// SDLGL2DVideoDriver::ShutdownGraphicsPartway
//
void SDLGL2DVideoDriver::ShutdownGraphicsPartway()
{   
   // haleyjd 06/21/06: use UpdateGrab here, not release
   UpdateGrab();

   // Code to allow changing resolutions in OpenGL.
   // Must shutdown everything.
   
   // Delete textures and clear names 
   if(textureid)
   {
      glDeleteTextures(1, &textureid);
      textureid = 0;
   }

   // Destroy any PBOs
   if(pboIDs[0])
   {
      pglDeleteBuffersARB(2, pboIDs);
      memset(pboIDs, 0, sizeof(pboIDs));
   }

   // Destroy the allocated temporary framebuffer
   if(framebuffer)
   {
      efree(framebuffer);
      framebuffer = NULL;
   }

   // Destroy the "primary buffer" screen surface
   UnsetPrimaryBuffer();

   // Clear the remembered texture binding
   GL_ClearBoundTexture();
}

// WARNING: SDL_GL_GetProcAddress is non-portable!
// Returns function pointers through a void * return type, which is in violation
// of the C and C++ standards. Probably works everywhere SDL works, though...

#define GETPROC(ptr, name, type) \
   ptr = (type)SDL_GL_GetProcAddress(name); \
   extension_ok = (extension_ok && ptr != NULL)

//
// SDLGL2DVideoDriver::LoadPBOExtension
//
// Load the ARB pixel buffer object extension if so specified and supported.
//
void SDLGL2DVideoDriver::LoadPBOExtension()
{
   static bool firsttime = true;
   bool extension_ok = true;
   const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
   bool want_arb_pbo = (cfg_gl_use_extensions && cfg_gl_arb_pixelbuffer);
   bool have_arb_pbo = (strstr(extensions, "GL_ARB_pixel_buffer_object") != NULL);

   // * Extensions must be enabled in general
   // * GL ARB PBO extension must be specifically enabled
   // * GL ARB PBO extension must be supported locally
   if(want_arb_pbo && have_arb_pbo)
   {
      GETPROC(pglGenBuffersARB,    "glGenBuffersARB",    PFNGLGENBUFFERSARBPROC);
      GETPROC(pglDeleteBuffersARB, "glDeleteBuffersARB", PFNGLDELETEBUFFERSARBPROC);
      GETPROC(pglBindBufferARB,    "glBindBufferARB",    PFNGLBINDBUFFERARBPROC);
      GETPROC(pglBufferDataARB,    "glBufferDataARB",    PFNGLBUFFERDATAARBPROC);
      GETPROC(pglMapBufferARB,     "glMapBufferARB",     PFNGLMAPBUFFERARBPROC);
      GETPROC(pglUnmapBufferARB,   "glUnmapBufferARB",   PFNGLUNMAPBUFFERARBPROC);

      // Use the extension if all procedures were found
      use_arb_pbo = extension_ok;

      if(firsttime && use_arb_pbo)
         usermsg(" Loaded extension GL_ARB_pixel_buffer_object");
   }
   else
      use_arb_pbo = false;

   // If wanted, but not enabled, warn
   if(firsttime && want_arb_pbo && !use_arb_pbo)
      usermsg(" Could not enable extension GL_ARB_pixel_buffer_object");

   // Don't print messages in this routine more than once
   firsttime = false;
}

// Config-to-GL enumeration lookups

// Configurable internal texture formats
static GLint internalTextureFormats[CFG_GL_NUMTEXFORMATS] =
{
   GL_R3_G3_B2,
   GL_RGB4,
   GL_RGB5,
   GL_RGB8,
   GL_RGB10,
   GL_RGB12,
   GL_RGB16,
   GL_RGBA2,
   GL_RGBA4,
   GL_RGB5_A1,
   GL_RGBA8,
   GL_RGB10_A2,
   GL_RGBA12,
   GL_RGBA16
   
};

// Configurable texture filtering parameters
static GLint textureFilterParams[CFG_GL_NUMFILTERS] =
{
   GL_LINEAR,
   GL_NEAREST
};

//
// SDLGL2DVideoDriver::InitGraphicsMode
//
bool SDLGL2DVideoDriver::InitGraphicsMode()
{
   bool    wantfullscreen = false;
   bool    wantvsync      = false;
   bool    wanthardware   = false; // Not used - this is always "hardware".
   bool    wantframe      = true;
   int     v_w            = 640;
   int     v_h            = 480;
   int     flags          = SDL_OPENGL;
   GLvoid *tempbuffer     = NULL;
   GLint   texformat      = GL_RGBA8;
   GLint   texfiltertype  = GL_LINEAR;

   // Get video commands and geometry settings

   // Allow end-user GL colordepth setting
   switch(cfg_gl_colordepth)
   {
   case 16: // Valid supported values
   case 24:
   case 32:
      colordepth = cfg_gl_colordepth;
      break;
   default:
      colordepth = 32;
      break;
   }

   // Allow end-user GL internal texture format specification
   if(cfg_gl_texture_format >= 0 && cfg_gl_texture_format < CFG_GL_NUMTEXFORMATS)
      texformat = internalTextureFormats[cfg_gl_texture_format];

   // Allow end-user GL texture filtering specification
   if(cfg_gl_filter_type >= 0 && cfg_gl_filter_type < CFG_GL_NUMFILTERS)
      texfiltertype = textureFilterParams[cfg_gl_filter_type];

   // haleyjd 04/11/03: "vsync" or page-flipping support
   if(use_vsync)
      wantvsync = true;
   
   // set defaults using geom string from configuration file
   I_ParseGeom(i_videomode, &v_w, &v_h, &wantfullscreen, &wantvsync, 
               &wanthardware, &wantframe);
   
   // haleyjd 06/21/06: allow complete command line overrides but only
   // on initial video mode set (setting from menu doesn't support this)
   I_CheckVideoCmds(&v_w, &v_h, &wantfullscreen, &wantvsync, &wanthardware,
                    &wantframe);

   if(wantfullscreen)
      flags |= SDL_FULLSCREEN;
   
   if(!wantframe)
      flags |= SDL_NOFRAME;
   
   // Set GL attributes through SDL
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   colordepth >= 24 ? 8 : 5);
   SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, colordepth >= 24 ? 8 : 5);
   SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  colordepth >= 24 ? 8 : 5);
   SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, colordepth == 32 ? 8 : 0);
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, wantvsync ? 1 : 0); // OMG vsync!

   // Set GL video mode
   if(!(surface = SDL_SetVideoMode(v_w, v_h, colordepth, flags)))
   {
      I_FatalError(I_ERR_KILL, "Couldn't set OpenGL video mode %dx%dx%d\n", 
                   v_w, v_h, colordepth);
   }

   // wait for a bit so the screen can settle
   if(flags & SDL_FULLSCREEN)
      I_Sleep(500);

   // Try loading the ARB PBO extension
   LoadPBOExtension();

   // Enable two-dimensional texture mapping
   glEnable(GL_TEXTURE_2D);

   // Set viewport
   glViewport(0, 0, (GLsizei)v_w, (GLsizei)v_h);

   // Set ortho projection
   GL_SetOrthoMode(v_w, v_h);
   
   // Calculate framebuffer texture sizes
   framebuffer_umax = GL_MakeTextureDimension((unsigned int)v_w);
   framebuffer_vmax = GL_MakeTextureDimension((unsigned int)v_h);

   // calculate right- and bottom-side texture coordinates
   texcoord_smax = (GLfloat)v_w / framebuffer_umax;
   texcoord_tmax = (GLfloat)v_h / framebuffer_vmax;

   // Create texture
   glGenTextures(1, &textureid);

   // Configure framebuffer texture
   texturesize = framebuffer_umax * framebuffer_vmax * 4;
   tempbuffer = ecalloc(GLvoid *, framebuffer_umax * 4, framebuffer_vmax);
   GL_BindTextureAndRemember(textureid);
   
   // villsa 05/29/11: set filtering otherwise texture won't render
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texfiltertype); 
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texfiltertype);   
   
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

   // TODO: allow user selection of internal texture format
   glTexImage2D(GL_TEXTURE_2D, 0, texformat, (GLsizei)framebuffer_umax, 
                (GLsizei)framebuffer_vmax, 0, GL_BGRA, GL_UNSIGNED_BYTE, 
                tempbuffer);
   efree(tempbuffer);

   // Allocate framebuffer data, or PBOs
   if(!use_arb_pbo)
      framebuffer = ecalloc(Uint32 *, v_w * 4, v_h);
   else
   {
      pglGenBuffersARB(2, pboIDs);
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[0]);
      pglBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, texturesize, 0, GL_STREAM_DRAW_ARB);
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[1]);
      pglBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, texturesize, 0, GL_STREAM_DRAW_ARB);
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
   }

   SDL_WM_SetCaption(ee_wmCaption, ee_wmCaption);
   UpdateFocus();
   UpdateGrab();

   // Init Cardboard video metrics
   video.width     = v_w;
   video.height    = v_h;
   video.bitdepth  = 8;
   video.pixelsize = 1;

   UnsetPrimaryBuffer();
   SetPrimaryBuffer();

   // Set initial palette
   SetPalette((byte *)wGlobalDir.cacheLumpName("PLAYPAL", PU_CACHE));

   return false;
}

//
// Colors Support
//

//
// SDLGL2DVideoDriver::hasColors
//
bool SDLGL2DVideoDriver::hasColors() const
{
   return (colors != NULL && havecolors);
}

//
// SDLGL2DVideoDriver::drawColorColumn
//
void SDLGL2DVideoDriver::drawColorColumn(int x, int y1, int y2, byte color)
{
   int count;
   byte *dest;
   
   count = y2 - y1 + 1;
   if(count <= 0)
      return;

#ifdef RANGECHECK
   if(x < 0 || x >= colors->w || 
      y1 < 0 || y1 >= colors->h || y2 < 0 || y2 >= colors->h)
      I_Error("SDLGL2DVideoDriver::drawColorColumn: column out of range\n");
#endif

   dest = (byte *)colors->pixels + y1 * colors->pitch + x;
   do
   {
      *dest = color;
      dest += colors->pitch;
   }
   while(--count);
}

//
// SDLGL2DVideoDriver::drawColorSpan
//
void SDLGL2DVideoDriver::drawColorSpan(int y, int x1, int x2, byte color)
{
   int count;
   byte *dest;

   count = x2 - x1 + 1;
   if(count <= 0)
      return;

#ifdef RANGECHECK
   if(y < 0 || y >= colors->h ||
      x1 < 0 || x1 >= colors->w || x2 < 0 || x2 >= colors->w)
      I_Error("SDLGL2DVideoDriver::drawColorSpan: span out of range\n");
#endif

   dest = (byte *)colors->pixels + y * colors->pitch + x1;
   memset(dest, color, count);
}

// The one and only global instance of the SDL GL 2D-in-3D video driver.
SDLGL2DVideoDriver i_sdlgl2dvideodriver;

#endif

// EOF

