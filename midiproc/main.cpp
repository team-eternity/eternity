//
// Copyright(C) 2017 James Haley, Max Waine, et al.
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
//
// Win32/SDL_mixer MIDI RPC Server
//
// Uses RPC to communicate with Eternity. This allows this separate process to
// have its own independent volume control even under Windows Vista and up's 
// broken, stupid, completely useless mixer model that can't assign separate
// volumes to different devices for the same process.
//
// Seriously, how did they screw up something so fundamental?
//
//-----------------------------------------------------------------------------

#include <windows.h>
#include <Psapi.h>

#include <stdlib.h>

#include <thread>
#include <vector>

#include "SDL.h"
#include "SDL_mixer.h"
#include "midiproc.h"

// Currently playing music track
static Mix_Music *music = NULL;
static SDL_RWops *rw    = NULL;

static void UnregisterSong();

//=============================================================================
//
// Sentinel That Checks Eternity Is Actually Running
//

class AutoHandle
{
public:
   HANDLE handle;
   AutoHandle(HANDLE h) : handle(h) {}
   ~AutoHandle()
   {
      if(handle != nullptr)
      {
         CloseHandle(handle);
      }
   }
};

static bool Sentinel_EnumerateProcesses(std::vector<DWORD> &ndwPIDs, size_t &numValidPIDs)
{
#pragma comment(lib, "psapi.lib")

   while(1)
   {
      DWORD cb = static_cast<DWORD>(ndwPIDs.size() * sizeof(DWORD));
      DWORD cbNeeded = 0;

      if(!EnumProcesses(&ndwPIDs[0], cb, &cbNeeded))
         return false;
      if(cb == cbNeeded)
      {
         // try again with a larger array
         ndwPIDs.resize(ndwPIDs.size() * 2);
      }
      else
      {
         // successful
         numValidPIDs = cbNeeded / sizeof(DWORD);
         return true;
      }
   }
}

static bool Sentinel_FindEternityPID(const std::vector<DWORD> &ndwPIDs,
                                     HANDLE &pHandle, size_t numValidPIDs)
{
#pragma comment(lib, "psapi.lib")

   for(size_t i = 0; i < numValidPIDs; i++)
   {
      AutoHandle chProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ndwPIDs[i]));
      if(chProcess.handle != nullptr)
      {
         char szProcessImage[MAX_PATH];
         ZeroMemory(szProcessImage, sizeof(szProcessImage));
         if(GetProcessImageFileNameA(chProcess.handle, szProcessImage, sizeof(szProcessImage)))
         {
            constexpr char   szProcessName[]   = "Eternity.exe"; // Case-insensitive
            constexpr size_t processNameLength = sizeof(szProcessName) - 1; // -1 to remove ending '\0'

            const size_t imageLength = strlen(szProcessImage);
            if(imageLength < processNameLength)
               continue;

            // Lop off the start of szProcessImage
            if(!strnicmp(szProcessImage + imageLength - processNameLength, szProcessName, processNameLength))
            {
               pHandle = chProcess.handle;
               chProcess.handle = nullptr; // Abuse AutoHandle's destructor behaviour
               return true;
            }
         }
      }
   }

   return false;
}

void Sentinel_Main()
{
   constexpr size_t initMaxNumPIDs = 1024;
   std::vector<DWORD> ndwPIDs(initMaxNumPIDs, 0);
   HANDLE pEternityHandle;
   size_t numValidPIDs;

   if(!Sentinel_EnumerateProcesses(ndwPIDs, numValidPIDs))
      exit(-1);
   if(!Sentinel_FindEternityPID(ndwPIDs, pEternityHandle, numValidPIDs))
   {
      MessageBox(
         NULL, TEXT("eternity.exe not currently running"), TEXT("midiproc: Error"), MB_ICONERROR
      );
      exit(-1);
   }

   DWORD dwExitCode;
   do
   {
      if(GetExitCodeProcess(pEternityHandle, &dwExitCode) == 0)
         exit(-1);
      Sleep(100);
   } while(dwExitCode == STILL_ACTIVE);

   exit(-1);
}

//=============================================================================
//
// RPC Memory Management
//

void __RPC_FAR * __RPC_USER midl_user_allocate(size_t size)
{
   return malloc(size);
}

void __RPC_USER midl_user_free(void __RPC_FAR *p)
{
   free(p);
}

//=============================================================================
//
// SDL_mixer Interface
//

//
// InitSDL
//
// Start up SDL and SDL_mixer.
//
static bool InitSDL()
{
   if(SDL_Init(SDL_INIT_AUDIO) == -1)
      return false;

   if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
      return false;

   return true;
}

//
// RegisterSong
//
static void RegisterSong(void *data, size_t size)
{
   if(music)
      UnregisterSong();

   rw    = SDL_RWFromMem(data, size);
   music = Mix_LoadMUS_RW(rw, true);
}

//
// StartSong
//
static void StartSong(bool loop)
{
   if(music)
      Mix_PlayMusic(music, loop ? -1 : 0);
}

//
// SetVolume
//
static void SetVolume(int volume)
{
   Mix_VolumeMusic((volume * 128) / 15);
}

static int paused_midi_volume;

//
// PauseSong
//
static void PauseSong()
{
   paused_midi_volume = Mix_VolumeMusic(-1);
   Mix_VolumeMusic(0);
}

//
// ResumeSong
//
static void ResumeSong()
{
   Mix_VolumeMusic(paused_midi_volume);
}

//
// StopSong
//
static void StopSong()
{
   if(music)
      Mix_HaltMusic();
}

//
// UnregisterSong
//
static void UnregisterSong()
{
   if(!music)
      return;

   StopSong();
   Mix_FreeMusic(music);
   rw    = NULL;
   music = NULL;
}

//
// ShutdownSDL
//
static void ShutdownSDL()
{
   UnregisterSong();
   Mix_CloseAudio();
   SDL_Quit();
}

//=============================================================================
//
// Song Buffer
//
// The MIDI program will be transmitted by the client across RPC in fixed-size
// chunks until all data has been transmitted.
//

typedef unsigned char midibyte;

class SongBuffer
{
protected:
   midibyte *buffer;    // accumulated input
   size_t    size;      // size of input
   size_t    allocated; // amount of memory allocated (>= size)

   static const int defaultSize = 128*1024; // 128 KB

public:
   // Constructor
   // Start out with an empty 128 KB buffer.
   SongBuffer()
   {
      buffer = static_cast<midibyte *>(calloc(1, defaultSize));
      size = 0;
      allocated = defaultSize;
   }

   // Destructor.
   // Release the buffer.
   ~SongBuffer()
   {
      if(buffer)
      {
         free(buffer);
         buffer = NULL;
         size = allocated = 0;
      }
   }

   //
   // addChunk
   //
   // Add a chunk of MIDI data to the buffer.
   //
   void addChunk(midibyte *data, size_t newsize)
   {
      if(size + newsize > allocated)
      {
         allocated += newsize * 2;
         buffer = static_cast<midibyte *>(realloc(buffer, allocated));
      }

      memcpy(buffer + size, data, newsize);
      size += newsize;
   }

   // Accessors

   midibyte *getBuffer() const { return buffer; }
   size_t    getSize()   const { return size;   }
};

static SongBuffer *song;

//=============================================================================
//
// RPC Server Interface
//

//
// MidiRPC_PrepareNewSong
//
// Prepare the engine to receive new song data from the RPC client.
//
void MidiRPC_PrepareNewSong()
{
   // Stop anything currently playing and free it.
   UnregisterSong();

   // free any previous song buffer
   delete song;

   // prep new song buffer
   song = new SongBuffer();
}

//
// MidiRPC_AddChunk
//
// Add a chunk of data to the song.
//
void MidiRPC_AddChunk(unsigned int count, byte *pBuf)
{
   song->addChunk(pBuf, static_cast<size_t>(count));
}

//
// MidiRPC_PlaySong
//
// Start playing the song.
//
void MidiRPC_PlaySong(boolean looping)
{
   RegisterSong(song->getBuffer(), song->getSize());
   StartSong(!!looping);
}

//
// MidiRPC_StopSong
//
// Stop the song.
//
void MidiRPC_StopSong()
{
   StopSong();
}

//
// MidiRPC_ChangeVolume
//
// Set playback volume level.
//
void MidiRPC_ChangeVolume(int volume)
{
   SetVolume(volume);
}

//
// MidiRPC_PauseSong
//
// Pause the song.
//
void MidiRPC_PauseSong()
{
   PauseSong();
}

//
// MidiRPC_ResumeSong
//
// Resume after pausing.
//
void MidiRPC_ResumeSong()
{
   ResumeSong();
}

//
// MidiRPC_StopServer
//
// Stops the RPC server so the program can shutdown.
//
void MidiRPC_StopServer()
{
   // Local shutdown tasks
   ShutdownSDL();
   delete song;
   song = NULL;

   // Stop RPC server
   RpcMgmtStopServerListening(NULL);
}

//
// RPC Server Init
//
static bool MidiRPC_InitServer()
{
   RPC_STATUS status;

   // Initialize RPC protocol
   status = 
      RpcServerUseProtseqEp
      (
         (RPC_CSTR)("ncalrpc"),
         RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
         (RPC_CSTR)("2d4dc2f9-ce90-4080-8a00-1cb819086970"),
         NULL
      );

   if(status)
      return false;

   // Register server
   status = RpcServerRegisterIf(MidiRPC_v1_0_s_ifspec, NULL, NULL);

   if(status)
      return false;

   // Start listening
   status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, FALSE);

   return !status;
}

//
// Checks if Windows version is 10 or higher, for audio kludge.
// I wish we could use the Win 8.1 API and Versionhelpers.h
//
inline bool I_IsWindowsVistaOrHigher()
{
#pragma comment(lib, "version.lib")

   static const CHAR kernel32[] = "\\kernel32.dll";
   CHAR *path;
   void *ver, *block;
   UINT  dirLength;
   DWORD versionSize;
   UINT  blockSize;
   VS_FIXEDFILEINFO *vInfo;
   WORD  majorVer;

   path = static_cast<CHAR *>(malloc(sizeof(*path) * MAX_PATH));

   dirLength = GetSystemDirectory(path, MAX_PATH);
   if(dirLength >= MAX_PATH || dirLength == 0 ||
      dirLength > MAX_PATH - sizeof(kernel32) / sizeof(*kernel32))
      abort();
   memcpy(path + dirLength, kernel32, sizeof(kernel32));

   versionSize = GetFileVersionInfoSize(path, nullptr);
   if(versionSize == 0)
      abort();
   ver = malloc(versionSize);
   if(!GetFileVersionInfo(path, 0, versionSize, ver))
      abort();
   if(!VerQueryValue(ver, "\\", &block, &blockSize) || blockSize < sizeof(VS_FIXEDFILEINFO))
      abort();
   vInfo = static_cast<VS_FIXEDFILEINFO *>(block);
   majorVer = HIWORD(vInfo->dwProductVersionMS);
   //minorVer = LOWORD(vInfo->dwProductVersionMS);
   //buildNum = HIWORD(vInfo->dwProductVersionLS);
   free(path);
   free(ver);
   return majorVer >= 6; // Vista is NT 6.0
}

//=============================================================================
//
// Main Program
//

//
// WinMain
//
// Application entry point.
//
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
   // Initialize SDL
   if(!InitSDL())
      return -1;

   if(I_IsWindowsVistaOrHigher())
      SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
   else
      SDL_setenv("SDL_AUDIODRIVER", "winmm", true);

   std::thread watcher(Sentinel_Main);

   // Initialize RPC Server
   if(!MidiRPC_InitServer())
      return -1;

   return 0;
}

// EOF

