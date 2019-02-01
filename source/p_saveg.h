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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Savegame I/O, archiving, persistence.
//
//-----------------------------------------------------------------------------

#ifndef __P_SAVEG__
#define __P_SAVEG__

// Persistent storage/archiving.
// These are the load / save game routines.

class  Thinker;
class  Mobj;
class  OutBuffer;
class  InBuffer;
struct inventoryslot_t;
struct spectransfer_t;
struct mapthing_t;
struct sector_t;
struct line_t;
struct v2fixed_t;
struct zrefs_t;

class SaveArchive
{
protected:
   OutBuffer *savefile;        // valid when saving
   InBuffer  *loadfile;        // valid when loading

public:
   explicit SaveArchive(OutBuffer *pSaveFile);
   explicit SaveArchive(InBuffer  *pLoadFile);

   // Accessors
   bool isSaving()  const   { return (savefile != nullptr); }
   bool isLoading() const   { return (loadfile != nullptr); }
   OutBuffer *getSaveFile() { return savefile; }
   InBuffer  *getLoadFile() { return loadfile; }

   // Methods
   void archiveCString(char *str,  size_t maxLen);
   void archiveLString(char *&str, size_t &len);
   
   // writeLString is valid during saving only. This is to accomodate const
   // char *'s which must be saved, and are read into temporary buffers 
   // during loading.
   void writeLString(const char *str, size_t len = 0);

   // archive a size_t
   void archiveSize(size_t &value);

   // Operators
   // Similar to ZDoom's FArchive class, these are symmetric - they are used
   // both for reading and writing.
   // Basic types:
   SaveArchive &operator << (int32_t  &x);
   SaveArchive &operator << (uint32_t &x);
   SaveArchive &operator << (int16_t  &x);
   SaveArchive &operator << (uint16_t &x);
   SaveArchive &operator << (int8_t   &x);
   SaveArchive &operator << (uint8_t  &x); 
   SaveArchive &operator << (bool     &x);
   SaveArchive &operator << (float    &x);
   SaveArchive &operator << (double   &x);
   // Pointers:
   SaveArchive &operator << (sector_t *&s);
   SaveArchive &operator << (line_t   *&ln);
   // Structures:
   SaveArchive &operator << (spectransfer_t  &st);
   SaveArchive &operator << (mapthing_t      &mt);
   SaveArchive &operator << (inventoryslot_t &slot);
   SaveArchive &operator << (v2fixed_t &vec);
   SaveArchive &operator << (zrefs_t &zref);
};

// Global template functions for SaveArchive

//
// P_ArchiveArray
//
// Archives an array. The base element of the array must have an
// operator << overload for SaveArchive.
//
template <typename T>
void P_ArchiveArray(SaveArchive &arc, T *ptrArray, int numElements)
{
   for(int i = 0; i < numElements; i++)
      arc << ptrArray[i];
}

// haleyjd: These utilities are now needed for external serialization support
// in Thinker-derived classes.
unsigned int P_NumForThinker(Thinker *th);
Thinker *P_ThinkerForNum(unsigned int n);
void P_SetNewTarget(Mobj **mop, Mobj *targ);

void P_SaveCurrentLevel(char *filename, char *description);
void P_LoadGame(const char *filename);

#endif

//----------------------------------------------------------------------------
//
// $Log: p_saveg.h,v $
// Revision 1.5  1998/05/03  23:10:40  killough
// beautification
//
// Revision 1.4  1998/02/23  04:50:09  killough
// Add automap marks and properties to saved state
//
// Revision 1.3  1998/02/17  06:26:04  killough
// Remove unnecessary plat archive/unarchive funcs
//
// Revision 1.2  1998/01/26  19:27:26  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:06  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
