// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 David Hill et al.
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
// ACSE/e loader.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "acs_intr.h"

#include "i_system.h"
#include "m_swap.h"
#include "w_wad.h"


void ACS_LoadScriptChunksACSE(ACSVM *vm, WadDirectory *dir, byte *tableData,
                              uint32_t tableLength, bool fakeACS0);

//----------------------------------------------------------------------------|
// Types                                                                      |
//

enum
{
   ACS_CHUNKID_AIMP = ACS_CHUNKID('A', 'I', 'M', 'P'),
   ACS_CHUNKID_AINI = ACS_CHUNKID('A', 'I', 'N', 'I'),
   ACS_CHUNKID_ARAY = ACS_CHUNKID('A', 'R', 'A', 'Y'),
   ACS_CHUNKID_ASTR = ACS_CHUNKID('A', 'S', 'T', 'R'),
   ACS_CHUNKID_ATAG = ACS_CHUNKID('A', 'T', 'A', 'G'),
   ACS_CHUNKID_FNAM = ACS_CHUNKID('F', 'N', 'A', 'M'),
   ACS_CHUNKID_FUNC = ACS_CHUNKID('F', 'U', 'N', 'C'),
   ACS_CHUNKID_JUMP = ACS_CHUNKID('J', 'U', 'M', 'P'),
   ACS_CHUNKID_LOAD = ACS_CHUNKID('L', 'O', 'A', 'D'),
   ACS_CHUNKID_MEXP = ACS_CHUNKID('M', 'E', 'X', 'P'),
   ACS_CHUNKID_MIMP = ACS_CHUNKID('M', 'I', 'M', 'P'),
   ACS_CHUNKID_MINI = ACS_CHUNKID('M', 'I', 'N', 'I'),
   ACS_CHUNKID_MSTR = ACS_CHUNKID('M', 'S', 'T', 'R'),
   ACS_CHUNKID_SFLG = ACS_CHUNKID('S', 'F', 'L', 'G'),
   ACS_CHUNKID_SNAM = ACS_CHUNKID('S', 'N', 'A', 'M'),
   ACS_CHUNKID_SPTR = ACS_CHUNKID('S', 'P', 'T', 'R'),
   ACS_CHUNKID_STRE = ACS_CHUNKID('S', 'T', 'R', 'E'),
   ACS_CHUNKID_STRL = ACS_CHUNKID('S', 'T', 'R', 'L'),
   ACS_CHUNKID_SVCT = ACS_CHUNKID('S', 'V', 'C', 'T'),
};

typedef void (*acs_chunker_t)(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                              uint32_t chunkLength);


//----------------------------------------------------------------------------|
// Static Functions                                                           |
//

//
// ACS_getScriptTypeACSE
//
static acs_stype_t ACS_getScriptTypeACSE(uint16_t type)
{
   switch(type)
   {
   default:
   case  0: return ACS_STYPE_CLOSED;
   case  1: return ACS_STYPE_OPEN;
   case  4: return ACS_STYPE_ENTER;
   }
}

//
// ACS_chunkScriptACSE
//
static void ACS_chunkScriptACSE(ACSVM *vm, byte *tableData, uint32_t tableLength,
                                acs_chunker_t chunker)
{
   uint32_t chunkID, chunkLength;

   for(;;)
   {
      // Need space for header.
      if(tableLength < 8) break;

      // Read header.
      chunkID     = SwapLong(*(int32_t *)(tableData + 0));
      chunkLength = SwapLong(*(int32_t *)(tableData + 4));

      // Consume header.
      tableData   += 8;
      tableLength -= 8;

      // Need space for payload.
      if(tableLength < chunkLength) break;

      // Read payload.
      chunker(vm, chunkID, tableData, chunkLength);

      // Consume payload.
      tableData   += chunkLength;
      tableLength -= chunkLength;
   }
}

//
// ACS_chunkerString
//
// Reads a string table. Returns number of strings read.
//
static void ACS_chunkerString(uint32_t *&outStrings, unsigned int &outNumStrings,
                              byte *chunkData, uint32_t chunkLength, bool junk)
{
   uint32_t numStrings, stringBase, stringIndex;
   uint32_t *rover = (uint32_t *)chunkData;

   if(junk)
   {
      // Need space for string count.
      if(chunkLength < 12) return;

      // Read number of strings. (Before and after is unused data.)
      ++rover;
      numStrings = SwapLong(*rover++);
      ++rover;

      // This is where string data starts.
      stringBase = 12 + numStrings*4;
   }
   else
   {
      // Need space for string count.
      if(chunkLength < 4) return;

      // Read number of strings.
      numStrings = SwapULong(*rover++);

      // This is where string data starts.
      stringBase = 4 + numStrings*4;
   }

   // Need space for string pointers.
   if(chunkLength < stringBase) return;

   // Read strings.
   outNumStrings += numStrings;
   outStrings = (uint32_t *)Z_Realloc(outStrings, outNumStrings * sizeof(uint32_t),
                                      PU_LEVEL, NULL);

   for(uint32_t *end = outStrings + outNumStrings,
       *itr = end - numStrings; itr != end; ++itr)
   {
      stringIndex = SwapLong(*rover++);

      if (stringIndex < chunkLength && stringIndex >= stringBase)
         *itr = ACS_LoadStringACS0(chunkData + stringIndex, chunkData + chunkLength);
      else
         *itr = ACS_LoadStringACS0(chunkData + chunkLength, chunkData + chunkLength);
   }
}

//
// ACS_chunkerAIMP
//
// Imports map arrays.
//
static void ACS_chunkerAIMP(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_AIMP) return;

   ACSArray *var;
   uint32_t index, count;
   const char *name;

   if(chunkLength < 4) return;

   count = SwapULong(*(uint32_t *)chunkData);
   chunkData   += 4;
   chunkLength -= 4;

   // Read imports until the end of the chunk is reached or count exhausted.
   while(count--)
   {
      // Need an index, size, and a null-terminated string.
      if(chunkLength < 9) break;

      index = SwapULong(*(uint32_t *)chunkData);
      name = (const char *)chunkData + 8;
      chunkData   += 8;
      chunkLength -= 8;

      // Find the end of the string.
      while(*chunkData)
      {
         --chunkData;
         --chunkLength;

         // If at the end of the chunk, return.
         if(!chunkLength) return;
      }

      if(index >= ACS_NUM_MAPARRS) continue;

      // Search through all of the imported VMs for that named array.
      for(ACSVM **itrVM = vm->importVMs,
          **endVM = itrVM + vm->numImports; itrVM != endVM; ++itrVM)
      {
         if(*itrVM && (var = (*itrVM)->findMapArr(name)))
         {
            vm->mapatab[index] = var;
            break;
         }
      }
   }
}

//
// ACS_chunkerAINI
//
// Initializes map arrays.
//
static void ACS_chunkerAINI(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_AINI) return;

   int32_t *ini, *end;
   uint32_t var;
   ACSArray *arr;

   if(chunkLength < 4) return;

   ini = (int32_t *)chunkData;
   end = ini + chunkLength/4;

   var = SwapLong(*ini++);
   if(var >= ACS_NUM_MAPARRS) return;

   arr = &vm->maparrs[var];
   var = 0;

   while(ini != end)
      arr->at(var++) = SwapLong(*ini++);
}

//
// ACS_chunkerARAY
//
// Defines map arrays.
//
static void ACS_chunkerARAY(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_ARAY) return;

   for(uint32_t var, *itr = (uint32_t *)chunkData,
       *end = itr + chunkLength/8*2; itr != end;)
   {
      var = SwapULong(*itr++);
      if(var < ACS_NUM_MAPARRS)
      {
         vm->mapalen[var] = SwapULong(*itr++);
         vm->mapahas[var] = true;
      }
   }
}

//
// ACS_chunkerASTR
//
// Tags map arrays as strings.
//
static void ACS_chunkerASTR(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_ASTR) return;

   for(uint32_t var, *itr = (uint32_t *)chunkData,
       *end = itr + chunkLength/4; itr != end;)
   {
      var = SwapULong(*itr++);
      if(var < ACS_NUM_MAPARRS)
      {
         ACSArray *arr = &vm->maparrs[var];
         for(uint32_t i = vm->mapalen[var]; i--;)
            arr->at(i) = vm->getStringIndex(arr->at(i));
      }
   }
}

//
// ACS_chunkerATAG
//
// Tags map arrays.
//
static void ACS_chunkerATAG(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_ATAG) return;

   // Need version byte and array number.
   if(chunkLength < 5) return;

   // Check version.
   if(*chunkData++ != 0) return;

   // Check array number.
   uint32_t arrnum = SwapULong(*(uint32_t *)chunkData); chunkData += 4;
   if(arrnum >= ACS_NUM_MAPARRS) return;

   ACSArray &arr = vm->maparrs[arrnum];

   // Remaining data are tag types.
   chunkLength -= 5;
   for(uint32_t i = 0; chunkLength--; ++i)
   {
      switch(*chunkData++)
      {
      case 1: // string
         arr[i] = vm->getStringIndex(arr[i]);
         break;

      case 2: // function
         arr[i] = (uint32_t)arr[i] < vm->numFuncs ? vm->funcptrs[arr[i]]->number : 0;
         break;
      }
   }
}

//
// ACS_chunkerFNAM
//
// Function names.
//
static void ACS_chunkerFNAM(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_FNAM) return;

   ACS_chunkerString(vm->funcNames, vm->numFuncNames, chunkData, chunkLength, false);
}

//
// ACS_chunkerFUNC
//
// Function pointers.
//
static void ACS_chunkerFUNC(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_FUNC) return;

   uint32_t numFuncs = chunkLength / 8;

   // Read scripts.
   vm->numFuncs += numFuncs;
   vm->funcs = (ACSFunc *)Z_Realloc(vm->funcs, vm->numFuncs * sizeof(ACSFunc),
                                    PU_LEVEL, NULL);

   for(ACSFunc *end = vm->funcs + vm->numFuncs,
       *itr = end - numFuncs; itr != end; ++itr)
   {
      itr->numArgs   = *(uint8_t *)(chunkData + 0);
      itr->numVars   = *(uint8_t *)(chunkData + 1);
      itr->numRetn   = *(uint8_t *)(chunkData + 2);
      itr->codeIndex = SwapLong(*(uint32_t *)(chunkData + 4));
      itr->vm        = vm;

      if(itr->numVars < itr->numArgs)
         itr->numVars = itr->numArgs;

      chunkData += 8;
   }
}

//
// ACS_chunkerJUMP
//
// Dynamic jump targets.
//
static void ACS_chunkerJUMP(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_JUMP) return;

   // Chunk is an unadorned list of jump targets.
   uint32_t numJumps = vm->numJumps + chunkLength / 4, *chunkItr;
   ACSJump *jumpItr, *jumpEnd;

   // Start by resizing the current array (if any).
   vm->jumps = (ACSJump *)Z_Realloc(vm->jumps, numJumps * sizeof(ACSJump), PU_LEVEL, NULL);

   jumpItr = vm->jumps + vm->numJumps;
   jumpEnd = vm->jumps + numJumps;
   chunkItr = (uint32_t *)chunkData;

   // Just read in the values for now. Translating will happen later.
   while(jumpItr != jumpEnd)
      (*jumpItr++).codeIndex = SwapULong(*chunkItr++);

   // Put jump count back into VM.
   vm->numJumps = numJumps;
}

//
// ACS_chunkerLOAD
//
static void ACS_chunkerLOAD(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_LOAD) return;

   unsigned int numImports = 0;

   for(byte *itr = chunkData, *end = itr + chunkLength; itr != end; ++itr)
      if(!*itr) ++numImports;

   vm->numImports += numImports;
   vm->imports = (const char **)Z_Realloc(vm->imports,
      vm->numImports * sizeof(const char *), PU_LEVEL, NULL);

   // No need to copy out the data, since it's only for loading.
   for(const char **end = vm->imports + vm->numImports,
       **itr = end - numImports; itr != end; ++itr)
   {
      *itr = (const char *)chunkData;

      // Advance past the string.
      while(*chunkData++);
   }
}

//
// ACS_chunkerMEXP
//
// Names/exports map variables/arrays.
//
static void ACS_chunkerMEXP(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_MEXP) return;

   ACS_chunkerString(vm->exports, vm->numExports, chunkData, chunkLength, false);
}

//
// ACS_chunkerMIMP
//
// Imports map variables.
//
static void ACS_chunkerMIMP(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_MIMP) return;

   int32_t *var;
   uint32_t index;
   const char *name;

   // Read imports until the end of the chunk is reached.
   for(;;)
   {
      // Need an index and a null-terminated string.
      if(chunkLength < 5) break;

      index = SwapULong(*(uint32_t *)chunkData);
      name = (const char *)chunkData + 4;
      chunkData   += 4;
      chunkLength -= 4;

      // Find the end of the string.
      while(*chunkData)
      {
         --chunkData;
         --chunkLength;

         // If at the end of the chunk, return.
         if(!chunkLength) return;
      }

      if(index >= ACS_NUM_MAPVARS) continue;

      // Search through all of the imported VMs for that named variable.
      for(ACSVM **itrVM = vm->importVMs,
          **endVM = itrVM + vm->numImports; itrVM != endVM; ++itrVM)
      {
         if(*itrVM && (var = (*itrVM)->findMapVar(name)))
         {
            vm->mapvtab[index] = var;
            break;
         }
      }
   }
}

//
// ACS_chunkerMINI
//
// Initializes map variables.
//
static void ACS_chunkerMINI(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_MINI) return;

   int32_t *ini, *var, *end;
   uint32_t base, vars;

   if(chunkLength < 4) return;

   ini = (int32_t *)chunkData;

   base = SwapLong(*ini++);
   if(base >= ACS_NUM_MAPVARS) return;

   vars = chunkLength / 4 - 1;
   if(vars >= ACS_NUM_MAPVARS || base + vars >= ACS_NUM_MAPVARS)
      vars = ACS_NUM_MAPVARS - base;

   var = vm->mapvars + base;
   end = var + vars;

   while(var != end)
      *var++ = SwapLong(*ini++);
}

//
// ACS_chunkerMSTR
//
// Tags map variables as strings.
//
static void ACS_chunkerMSTR(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_MSTR) return;

   for(uint32_t var, *itr = (uint32_t *)chunkData,
       *end = itr + chunkLength/4; itr != end;)
   {
      var = SwapULong(*itr++);
      if(var < ACS_NUM_MAPVARS)
         vm->mapvars[var] = vm->getStringIndex(vm->mapvars[var]);
   }
}

//
// ACS_chunkerSFLG
//
// Reads script flags.
//
static void ACS_chunkerSFLG(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_SFLG) return;

   uint32_t numFlags = chunkLength / 4;

   int32_t  number;
   uint16_t flags;

   while(numFlags--)
   {
      number = SwapShort (*( int16_t *)(chunkData + 0));
      flags  = SwapUShort(*(uint16_t *)(chunkData + 2));

      chunkData += 4;

      // Set the flags for any relevant script.
      for(ACSScript *itr = vm->scripts, *end = itr + vm->numScripts; itr != end; ++itr)
      {
         if(itr->number == number)
            itr->flags = flags;
      }
   }
}

//
// ACS_chunkerSNAM
//
// Script names.
//
static void ACS_chunkerSNAM(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_SNAM) return;

   ACS_chunkerString(vm->scriptNames, vm->numScriptNames, chunkData, chunkLength, false);
}

//
// ACS_chunkerSPTR8
//
// Reads 8-byte script-pointers.
//
static void ACS_chunkerSPTR8(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                             uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_SPTR) return;

   uint32_t numScripts = chunkLength / 8;

   // Read scripts.
   vm->numScripts += numScripts;
   vm->scripts = (ACSScript *)Z_Realloc(vm->scripts,
      vm->numScripts * sizeof(ACSScript), PU_LEVEL, NULL);

   for(ACSScript *end = vm->scripts + vm->numScripts,
       *itr = end - numScripts; itr != end; ++itr)
   {
      memset(itr, 0, sizeof(ACSScript));

      itr->number    = SwapShort(*(int16_t *)(chunkData + 0));
      itr->type      = ACS_getScriptTypeACSE(*(uint8_t *)(chunkData + 2));
      itr->numArgs   = (*(uint8_t *)(chunkData + 3));
      itr->codeIndex = SwapLong(*(uint32_t *)(chunkData + 4));

      itr->numVars   = ACS_NUM_LOCALVARS;
      itr->vm        = vm;

      chunkData += 8;
   }
}

//
// ACS_chunkerSPTR12
//
// Reads 12-byte script-pointers.
//
static void ACS_chunkerSPTR12(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                              uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_SPTR) return;

   uint32_t numScripts = chunkLength / 12;

   // Read scripts.
   vm->numScripts += numScripts;
   vm->scripts = (ACSScript *)Z_Realloc(vm->scripts,
      vm->numScripts * sizeof(ACSScript), PU_LEVEL, NULL);

   for(ACSScript *end = vm->scripts + vm->numScripts,
       *itr = end - numScripts; itr != end; ++itr)
   {
      memset(itr, 0, sizeof(ACSScript));

      itr->number    = SwapShort(*(int16_t *)(chunkData + 0));
      itr->type      = ACS_getScriptTypeACSE(SwapShort(*(uint16_t *)(chunkData + 2)));
      itr->codeIndex = SwapLong(*(uint32_t *)(chunkData + 4));
      itr->numArgs   = SwapLong(*(uint32_t *)(chunkData + 8));

      itr->numVars   = ACS_NUM_LOCALVARS;
      itr->vm        = vm;

      chunkData += 12;
   }
}

//
// ACS_chunkerSTRL
//
// Read string literal tables.
//
static void ACS_chunkerSTRL(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_STRL) return;

   ACS_chunkerString(vm->strings, vm->numStrings, chunkData, chunkLength, true);
}

//
// ACS_chunkerSVCT
//
// Reads script variable counts.
//
static void ACS_chunkerSVCT(ACSVM *vm, uint32_t chunkID, byte *chunkData,
                            uint32_t chunkLength)
{
   if(chunkID != ACS_CHUNKID_SVCT) return;

   uint32_t numCounts = chunkLength / 4;

   int32_t  number;
   uint16_t numVars;

   while(numCounts--)
   {
      number  = SwapShort (*( int16_t *)(chunkData + 0));
      numVars = SwapUShort(*(uint16_t *)(chunkData + 2));

      chunkData += 4;

      // Set the variable count for any relevant script.
      for(ACSScript *itr = vm->scripts, *end = itr + vm->numScripts; itr != end; ++itr)
      {
         if(itr->number == number)
         {
            // Ensure there is enough space for the args, at least.
            if(numVars < itr->numArgs)
               itr->numVars = itr->numArgs;
            else
               itr->numVars = numVars;
         }
      }
   }
}

//
// ACS_loadScriptDataACSE
//
void ACS_loadScriptDataACSE(ACSVM *vm, WadDirectory *dir, int lump, byte *data,
                            uint32_t tableOffset, bool compressed)
{
   uint32_t lumpLength = dir->lumpLength(lump);
   byte *tableData;
   uint32_t tableIndex, tableLength;

   // Header + table index = 8 bytes.
   if (lumpLength < 8) return;

   // Find the chunk table.
   if (tableOffset > lumpLength - 4) return;
   tableIndex = SwapLong(*(int32_t *)(data + tableOffset));

   // If chunk table is after the end of the lump...
   if(tableIndex > lumpLength) return;

   // If there is a fake ACS0 lump at the end, don't include it in the table.
   if(tableOffset != 4)
   {
      // Take tableOffset as the lump length
      tableData   = data + tableIndex;
      tableLength = tableOffset - tableIndex;
   }
   else
   {
      tableData   = data + tableIndex;
      tableLength = lumpLength - tableIndex;
   }

   // Internal-data chunks.
   ACS_LoadScriptChunksACSE(vm, dir, tableData, tableLength, tableOffset != 4);

   // Read code.
   ACS_LoadScriptCodeACS0(vm, data, lumpLength, compressed);

   vm->loaded = true;
}


//----------------------------------------------------------------------------|
// Global Functions                                                           |
//

//
// ACS_LoadScriptACSE
//
void ACS_LoadScriptACSE(ACSVM *vm, WadDirectory *dir, int lump, byte *data,
                        uint32_t tableOffset)
{
   ACS_loadScriptDataACSE(vm, dir, lump, data, tableOffset, false);
}

//
// ACS_LoadScriptACSe
//
void ACS_LoadScriptACSe(ACSVM *vm, WadDirectory *dir, int lump, byte *data,
                        uint32_t tableOffset)
{
   ACS_loadScriptDataACSE(vm, dir, lump, data, tableOffset, true);
}

//
// ACS_LoadScriptChunksACSE
//
// Reads ACSE chunks.
//
void ACS_LoadScriptChunksACSE(ACSVM *vm, WadDirectory *dir, byte *tableData,
                              uint32_t tableLength, bool fakeACS0)
{
   ACSFunc *func;

   // Strings first so they get the right global indexes in VM-0.

   // STRE - String Literals Encrypted
   // TODO (is this deprecated?)

   // STRL - String Literals
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerSTRL);

   // The first part of the global string table must match VM-0 for compatibility.
   if(vm->id == 0 && ACSVM::GlobalNumStrings < vm->numStrings)
      vm->addStrings();

   // AINI - Map Array Init
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerAINI);

   // ARAY - Map Arrays
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerARAY);

   // ASTR - Map Array Strings
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerASTR);

   // FNAM - Function Names
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerFNAM);

   // FUNC - Functions
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerFUNC);

   // JUMP - Dynamic Jump Targets
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerJUMP);

   // MEXP - Map Variable/Array Export
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerMEXP);

   // MINI - Map Variable Init
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerMINI);

   // MSTR - Map Variable Strings
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerMSTR);

   // SNAM - Script Names
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerSNAM);

   // SPTR - Script Pointers
   if(fakeACS0)
      ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerSPTR8);
   else
      ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerSPTR12);

   // SFLG - Script Flags
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerSFLG);

   // SVCT - Script Variable Count
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerSVCT);

   // Process exports. (Do this before loading any other libraries so they can
   // import from here.)
   // Unfortunately, this includes the ZDoom baggage of map-arrays consuming
   // the indexes of map-variables.
   for(unsigned int i = vm->numExports; i--;)
   {
      ACSString *name = ACSVM::GlobalStrings[vm->exports[i]];

      if(!name->data.s || !name->data.s[0]) continue;

      if(i < ACS_NUM_MAPARRS && vm->mapahas[i])
         vm->mapanam[i] = name->data.s;
      else if(i < ACS_NUM_MAPVARS)
         vm->mapvnam[i] = name->data.s;
   }

   // LOAD - Library Loading
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerLOAD);

   // Process imports.
   vm->importVMs = (ACSVM **)Z_Malloc(vm->numImports * sizeof(ACSVM *), PU_LEVEL, NULL);
   for(unsigned int i = vm->numImports; i--;)
   {
      int importLump = dir->checkNumForName(vm->imports[i], lumpinfo_t::ns_acs);

      if(importLump != -1)
      {
         // Don't import self.
         if((vm->importVMs[i] = ACS_LoadScript(dir, importLump)) == vm)
            vm->importVMs[i] = NULL;
      }
      else
         vm->importVMs[i] = NULL;
   }

   // AIMP - Map Array Import
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerAIMP);

   // MIMP - Map Variable Import
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerMIMP);

   // Process functions.
   vm->funcptrs = (ACSFunc **)Z_Malloc(vm->numFuncs * sizeof(ACSFunc *), PU_LEVEL, NULL);
   for(unsigned int i = vm->numFuncs; i--;)
   {
      vm->funcptrs[i] = &vm->funcs[i];

      // If the function already has an address (or no name exists for it), we're done.
      if(vm->funcptrs[i]->codeIndex || i >= vm->numFuncNames)
         continue;

      ACSString *funcname = ACSVM::GlobalStrings[vm->funcNames[i]];

      // Search through all of the imported VMs for the function.
      for(ACSVM **itrVM = vm->importVMs,
          **endVM = itrVM + vm->numImports; itrVM != endVM; ++itrVM)
      {
         if(*itrVM && (func = (*itrVM)->findFunction(funcname->data.s)))
         {
            vm->funcptrs[i] = func;
            break;
         }
      }
   }

   // ATAG - Map Array Tagging
   ACS_chunkScriptACSE(vm, tableData, tableLength, ACS_chunkerATAG);
}

// EOF

