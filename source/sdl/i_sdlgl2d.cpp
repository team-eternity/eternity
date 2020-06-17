//
// The Eternity Engine
// Copyright (C) 2017 James Haley, Max Waine, et al.
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
// Purpose: SDL-specific GL 2D-in-3D video code
// Authors: James Haley, Max Waine
//

#ifdef EE_FEATURE_OPENGL

// SDL headers
#include "SDL.h"
#include "SDL_opengl.h"

// HAL header
#include "../hal/i_platform.h"

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

void UpdateGrab(SDL_Window *window);
bool MouseShouldBeGrabbed();
void UpdateFocus(SDL_Window *window);

//=============================================================================
//
// Static Data
//

// Temporary screen surface; this is what the game will draw itself into.
static SDL_Surface *screen; 

static SDL_GLContext glcontext;

// 32-bit converted palette for translation of the screen to 32-bit pixel data.
static Uint32 RGB8to32[256];
static byte   cachedpal[768];

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
static PFNGLGENBUFFERSARBPROC    pglGenBuffersARB    = nullptr;
static PFNGLDELETEBUFFERSARBPROC pglDeleteBuffersARB = nullptr;
static PFNGLBINDBUFFERARBPROC    pglBindBufferARB    = nullptr;
static PFNGLBUFFERDATAARBPROC    pglBufferDataARB    = nullptr;
static PFNGLMAPBUFFERARBPROC     pglMapBufferARB     = nullptr;
static PFNGLUNMAPBUFFERARBPROC   pglUnmapBufferARB   = nullptr;

// Data for vertex binding
static GLfloat screenVertices[4*2];
static GLfloat screenTexCoords[4*2];

static const GLubyte screenVtxOrder[3*2] = { 0, 1, 3, 3, 1, 2 };

//=============================================================================
//
// Graphics Code
//

//
// GL2D_setupVertexArray
//
// Static routine to setup vertex and texture coordinate arrays for use with
// glDrawElements.
//
static void GL2D_setupVertexArray(GLfloat x, GLfloat y, GLfloat w, GLfloat h,
                                  GLfloat smax, GLfloat tmax)
{
   // enable vertex and texture coordinate arrays
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   // Framebuffer Layout:
   //
   // 0-------3  0 = (x,  y  ) [0,   0   ]   Tris:
   // |       |  1 = (x,  y+h) [0,   tmax]   1: 0->1->3
   // |       |  2 = (x+w,y+h) [smax,tmax]   2: 3->1->2
   // 1-------2  3 = (x+w,y  ) [smax,0   ]

   // populate vertex coordinates
   screenVertices[0] = screenVertices[2] = x;
   screenVertices[1] = screenVertices[7] = y;
   screenVertices[3] = screenVertices[5] = y + h;
   screenVertices[4] = screenVertices[6] = x + w;

   // populate texture coordinates
   screenTexCoords[0] = screenTexCoords[1] = screenTexCoords[2] = screenTexCoords[7] = 0.0f;
   screenTexCoords[3] = screenTexCoords[5] = tmax;
   screenTexCoords[4] = screenTexCoords[6] = smax;

   // bind arrays
   glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, screenTexCoords);
   glVertexPointer  (2, GL_FLOAT, sizeof(GLfloat) * 2, screenVertices );
}

//
// SDLGL2DVideoDriver::DrawPixels
//
// Protected method.
//
void SDLGL2DVideoDriver::DrawPixels(void *buffer, unsigned int destwidth)
{
   Uint32   *fb          = static_cast<Uint32 *>(buffer);
   const int d_end       = (screen->w - bump) & ~7;
   const int d_remainder = (screen->w - bump) & 7;

   for(int y = 0; y < screen->h; y++)
   {
      byte   *src  = static_cast<byte *>(screen->pixels) + y * screen->pitch;
      Uint32 *dest = fb + y * destwidth;

      for(int x = d_end; x; x -= 8)
      {
         *dest   = RGB8to32[*src];
         dest[1] = RGB8to32[src[1]];
         dest[2] = RGB8to32[src[2]];
         dest[3] = RGB8to32[src[3]];
         dest[4] = RGB8to32[src[4]];
         dest[5] = RGB8to32[src[5]];
         dest[6] = RGB8to32[src[6]];
         dest[7] = RGB8to32[src[7]];
         src  += 8;
         dest += 8;
      }
      for(int i = d_remainder; i; i--)
      {
         *dest = RGB8to32[*src];
         ++src;
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
   UpdateGrab(window);

   // Don't update the screen if the window isn't visible.
   // Not doing this breaks under Windows when we alt-tab away 
   // while fullscreen.   
   if(!(SDL_GetWindowFlags(window) & SDL_WINDOW_SHOWN))
      return;

   if(!use_arb_pbo)
   {
      // Convert the game's 8-bit output to the 32-bit texture buffer
      DrawPixels(framebuffer, static_cast<unsigned int>(video.width));

      // bind the framebuffer texture if necessary
      GL_BindTextureIfNeeded(textureid);

      // update the texture data
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                      static_cast<GLsizei>(video.width), static_cast<GLsizei>(video.height),
                      GL_BGRA, GL_UNSIGNED_BYTE, static_cast<GLvoid *>(framebuffer));
   }
   else
   {
      static int pboindex  = 0;
      int        nextindex = 0;
      GLvoid    *ptr       = nullptr;

      // use the two pixel buffers in a rotation
      pboindex  = (pboindex + 1) % 2;
      nextindex = (pboindex + 1) % 2;

      // bind the framebuffer texture if necessary
      GL_BindTextureIfNeeded(textureid);

      // bind the primary PBO
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[pboindex]);

      // copy primary PBO to texture, using offset
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(framebuffer_umax),
                   static_cast<GLsizei>(framebuffer_vmax), 0, GL_BGRA, GL_UNSIGNED_BYTE,
                   nullptr);

      // bind the secondary PBO
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[nextindex]);

      // map the PBO into client memory in such a way as to avoid stalls
      pglBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, texturesize, nullptr, GL_STREAM_DRAW_ARB);

      if((ptr = pglMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB)))
      {
         // draw directly into video memory
         DrawPixels(ptr, framebuffer_umax);

         // release pointer
         pglUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
      }

      // Unbind all PBOs
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
   }

   // draw vertex array
   glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_BYTE, screenVtxOrder);

   // push the frame
   SDL_GL_SwapWindow(window);
}

//
// SDLGL2DVideoDriver::ReadScreen
//
void SDLGL2DVideoDriver::ReadScreen(byte *scr)
{
   if(bump == 0 && screen->pitch == screen->w)
   {
      // full block blit
      memcpy(scr, static_cast<byte *>(screen->pixels), video.width * video.height);
   }
   else
   {
      // must copy one row at a time
      for(int y = 0; y < screen->h; y++)
      {
         byte *src  = static_cast<byte *>(screen->pixels) + y * screen->pitch;
         byte *dest = scr + y * video.width;

         memcpy(dest, src, screen->w - bump);
      }
   }
}

//
// SDLGL2DVideoDriver::SetPalette
//
void SDLGL2DVideoDriver::SetPalette(byte *pal)
{
   byte *temppal;

   // Cache palette if a new one is being set (otherwise the gamma setting is 
   // being changed)
   if(pal)
      memcpy(cachedpal, pal, 768);

   temppal = cachedpal;
 
   // Create 32-bit translation lookup
   for(int i = 0; i < 256; i++)
   {
      RGB8to32[i] =
         (static_cast<Uint32>(0xff) << 24) |
         (static_cast<Uint32>(gammatable[usegamma][*(temppal + 0)]) << 16) |
         (static_cast<Uint32>(gammatable[usegamma][*(temppal + 1)]) <<  8) |
         (static_cast<Uint32>(gammatable[usegamma][*(temppal + 2)]) <<  0);
      
      temppal += 3;
   }
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
   if(!(screen = SDL_CreateRGBSurfaceWithFormat(0, video.width + bump, video.height,
                                                0, SDL_PIXELFORMAT_INDEX8)))
      I_Error("SDLGL2DVideoDriver::SetPrimaryBuffer: failed to create screen temp buffer\n");

   // Point screens[0] to 8-bit temp buffer
   video.screens[0] = static_cast<byte *>(screen->pixels);
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
      screen = nullptr;
   }
   video.screens[0] = nullptr;
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
   UpdateGrab(window);

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
      framebuffer = nullptr;
   }

   // Destroy the "primary buffer" screen surface
   UnsetPrimaryBuffer();

   // Clear the remembered texture binding
   GL_ClearBoundTexture();

   // Destroy the GL context
   SDL_GL_DeleteContext(glcontext);
   glcontext = nullptr;

   // Destroy the window
   SDL_DestroyWindow(window);
   window = nullptr;
}

// WARNING: SDL_GL_GetProcAddress is non-portable!
// Returns function pointers through a void * return type, which is in violation
// of the C and C++ standards. Probably works everywhere SDL works, though...

#define GETPROC(ptr, name, type) \
   ptr = (type)SDL_GL_GetProcAddress(name); \
   extension_ok = (extension_ok && ptr != nullptr)

//
// SDLGL2DVideoDriver::LoadPBOExtension
//
// Load the ARB pixel buffer object extension if so specified and supported.
//
void SDLGL2DVideoDriver::LoadPBOExtension()
{
   static bool firsttime = true;
   bool extension_ok = true;
   const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
   bool want_arb_pbo = (cfg_gl_use_extensions && cfg_gl_arb_pixelbuffer);
   bool have_arb_pbo = (strstr(extensions, "GL_ARB_pixel_buffer_object") != nullptr);

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
   bool    wantdesktopfs  = false;
   bool    wantvsync      = false;
   bool    wanthardware   = false; // Not used - this is always "hardware".
   bool    wantframe      = true;
   int     v_w            = 640;
   int     v_h            = 480;
   int     v_displaynum   = 0;
   int     window_flags   = SDL_WINDOW_OPENGL|SDL_WINDOW_ALLOW_HIGHDPI;
   GLvoid *tempbuffer     = nullptr;
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

   // Allow end-user GL texture filtering specification
   if(cfg_gl_filter_type >= 0 && cfg_gl_filter_type < CFG_GL_NUMFILTERS)
      texfiltertype = textureFilterParams[cfg_gl_filter_type];

   // haleyjd 04/11/03: "vsync" or page-flipping support
   if(use_vsync)
      wantvsync = true;

   // set defaults using geom string from configuration file
   I_ParseGeom(i_videomode, &v_w, &v_h, &wantfullscreen, &wantvsync,
               &wanthardware, &wantframe, &wantdesktopfs);

   // haleyjd 06/21/06: allow complete command line overrides but only
   // on initial video mode set (setting from menu doesn't support this)
   I_CheckVideoCmds(&v_w, &v_h, &wantfullscreen, &wantvsync, &wanthardware,
                    &wantframe, &wantdesktopfs);

   if(!wantframe)
      window_flags |= SDL_WINDOW_BORDERLESS;

   // Set GL attributes through SDL
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   colordepth >= 24 ? 8 : 5);
   SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, colordepth >= 24 ? 8 : 5);
   SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  colordepth >= 24 ? 8 : 5);
   SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, colordepth == 32 ? 8 : 0);

   if(displaynum < SDL_GetNumVideoDisplays())
      v_displaynum = displaynum;
   else
      displaynum = 0;

   if(!(window = SDL_CreateWindow(ee_wmCaption,
                                  SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum),
                                  SDL_WINDOWPOS_CENTERED_DISPLAY(v_displaynum),
                                  v_w, v_h, window_flags)))
   {
      I_FatalError(I_ERR_KILL, "Couldn't create OpenGL window %dx%d\n"
                               "SDL Error: %s\n", v_w, v_h, SDL_GetError());
   }

#if EE_CURRENT_PLATFORM == EE_PLATFORM_MACOSX
   // this and the below #else block are done here as monitor video mode isn't
   // set when SDL_WINDOW_FULLSCREEN (sans desktop) is ORed in during window creation
   if(wantfullscreen)
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
   if(wantfullscreen && wantdesktopfs)
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
   else if(wantfullscreen) // && !wantdesktopfs
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
#endif

   if(!(glcontext = SDL_GL_CreateContext(window)))
   {
      I_FatalError(I_ERR_KILL, "Couldn't create OpenGL context\n"
                               "SDL Error: %s\n", SDL_GetError());
   }

   // Set swap interval through SDL (must be done after context is created)
   SDL_GL_SetSwapInterval(wantvsync ? 1 : 0); // OMG vsync!

   Uint32 format;
   if(colordepth == 32)
      format = SDL_PIXELFORMAT_RGBA32;
   else if(colordepth == 24)
      format = SDL_PIXELFORMAT_RGB24;
   else // 16
      format = SDL_PIXELFORMAT_RGB555;

   if(!(screen = SDL_CreateRGBSurfaceWithFormat(0, v_w, v_h, 0, format)))
   {
      I_FatalError(I_ERR_KILL, "Couldn't set RGB surface with colordepth %d, format %s\n",
                   colordepth, SDL_GetPixelFormatName(format));
   }

   const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));

   // Try loading the ARB PBO extension
   LoadPBOExtension();

   // Enable two-dimensional texture mapping
   glEnable(GL_TEXTURE_2D);

   // Set viewport
   // This is necessary for high-DPI displays (tested so far on macOS).
   int drawableW = 0;
   int drawableH = 0;
   SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
   if(!drawableW || !drawableH)
   {
      // If the function somehow fails, reset to v_w and v_h
      drawableW = v_w;
      drawableH = v_h;
   }
   glViewport(0, 0, static_cast<GLsizei>(drawableW), static_cast<GLsizei>(drawableH));

   // Set ortho projection
   GL_SetOrthoMode(v_w, v_h);

   // Calculate framebuffer texture sizes
   if(strstr(extensions, "GL_ARB_texture_non_power_of_two") != nullptr)
   {
       framebuffer_umax = v_w;
       framebuffer_vmax = v_h;
   }
   else
   {
       framebuffer_umax = GL_MakeTextureDimension(static_cast<unsigned int>(v_w));
       framebuffer_vmax = GL_MakeTextureDimension(static_cast<unsigned int>(v_h));
   }

   // calculate right- and bottom-side texture coordinates
   texcoord_smax = static_cast<GLfloat>(v_w) / framebuffer_umax;
   texcoord_tmax = static_cast<GLfloat>(v_h) / framebuffer_vmax;

   GL2D_setupVertexArray(0.0f, 0.0f, static_cast<float>(v_w), static_cast<float>(v_h),
                         texcoord_smax, texcoord_tmax);

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

   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(framebuffer_umax),
                static_cast<GLsizei>(framebuffer_vmax), 0, GL_BGRA, GL_UNSIGNED_BYTE, 
                tempbuffer);
   efree(tempbuffer);

   // Allocate framebuffer data, or PBOs
   if(!use_arb_pbo)
      framebuffer = ecalloc(Uint32 *, v_w * 4, v_h);
   else
   {
      pglGenBuffersARB(2, pboIDs);
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[0]);
      pglBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, texturesize, nullptr, GL_STREAM_DRAW_ARB);
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIDs[1]);
      pglBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, texturesize, nullptr, GL_STREAM_DRAW_ARB);
      pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
   }

   UpdateFocus(window);
   UpdateGrab(window);

   // Init Cardboard video metrics
   video.width     = v_w;
   video.height    = v_h;
   video.bitdepth  = 8;
   video.pixelsize = 1;

   UnsetPrimaryBuffer();
   SetPrimaryBuffer();

   // Set initial palette
   SetPalette(static_cast<byte *>(wGlobalDir.cacheLumpName("PLAYPAL", PU_CACHE)));

   return false;
}

// The one and only global instance of the SDL GL 2D-in-3D video driver.
SDLGL2DVideoDriver i_sdlgl2dvideodriver;

#endif

// EOF

