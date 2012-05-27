// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 David Hill
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
// ACS0 loader.
//
//----------------------------------------------------------------------------

#include "acs_intr.h"

#include "i_system.h"
#include "m_swap.h"
#include "w_wad.h"

#include <stddef.h>


//----------------------------------------------------------------------------|
// Types                                                                      |
//

typedef enum acs0_op_e
{
   #define ACS0_OP(OPDATA,OP,ARGC) ACS0_OP_##OP,
   #include "acs_op.h"
   #undef ACS0_OP

   ACS0_OPMAX
} acs0_op_t;

typedef struct acs0_opdata_s
{
   acs_opdata_t const *opdata;
   acs0_op_t op;
   int args;
} acs0_opdata_t;


//----------------------------------------------------------------------------|
// Static Variables                                                           |
//

static acs0_opdata_t const ACS0opdata[ACS0_OPMAX] =
{
   #define ACS0_OP(OPDATA,OP,ARGC) {OPDATA, ACS0_OP_##OP, ARGC},
   #include "acs_op.h"
   #undef ACS0_OP
};


//----------------------------------------------------------------------------|
// Static Functions                                                           |
//

//
// ACS_readStringACS0
//
static char *ACS_readStringACS0(byte *dataBegin, byte *dataEnd)
{
   char *begin = (char *)dataBegin, *end = (char *)dataEnd, *itr = begin;
   char *str;

   // Find either a null-terminator or the end of the lump.
   while(itr != end && *itr) ++itr;

   str = (char *)Z_Malloc(itr - begin + 1, PU_LEVEL, NULL);
   memcpy(str, begin, itr - begin);
   str[itr - begin] = 0;

   return str;
}

//
// ACS_touchScriptACS0
//
static bool ACS_touchScriptACS0(bool *begin, bool *end)
{
   bool *itr;

   int32_t touchCount = 0;

   for (itr = begin; itr != end; ++itr)
      touchCount += *itr;

   if(!touchCount)
   {
      for (itr = begin; itr != end; ++itr)
         *itr = true;

      return false;
   }

   return true;
}

//
// ACS_traceScriptACS0
//
static void ACS_traceScriptACS0(acsvm_t *vm, uint32_t lumpLength, byte *data,
                                bool *codeTouched, uint32_t index,
                                uint32_t &jumpCount)
{
   uint32_t indexNext;
   int32_t op;
   acs0_opdata_t const *opdata;

   for(;;)
   {
      op = SwapLong(*(int32_t *)(data + index));

      // Invalid opcode terminates tracer.
      if(op >= ACS0_OPMAX || op < 0) return;

      opdata = &ACS0opdata[op];
      indexNext = index + (opdata->args * 4) + 4;

      // Leaving the bounds of the lump also terminates the tracer.
      if (indexNext > lumpLength) return;

      // If already touched this instruction, stop tracing.
      if(ACS_touchScriptACS0(codeTouched + index, codeTouched + indexNext))
         return;

      // Determine how many internal codes this counts for.
      if (opdata->opdata && opdata->opdata->args != -1)
         vm->numCode += opdata->opdata->args + 1;

      else switch (op)
      {
      case ACS0_OP_LINESPEC1_IMM:
      case ACS0_OP_LINESPEC2_IMM:
      case ACS0_OP_LINESPEC3_IMM:
      case ACS0_OP_LINESPEC4_IMM:
      case ACS0_OP_LINESPEC5_IMM:
         vm->numCode += opdata->args + 2;
         break;

      default:
#ifdef RANGECHECK
         // This line should never be reached.
         I_Error("Unknown translation for opcode. opcode %i", (int)op);
#endif
         break;
      }

      // Advance the index past the instruction.
      switch(op)
      {
      case ACS0_OP_SCRIPT_TERMINATE:
         return;

      case ACS0_OP_BRANCH_IMM:
         ++jumpCount;
         index = SwapLong(*(int32_t *)(data + index + 4));
         continue;

      case ACS0_OP_BRANCH_NOTZERO:
      case ACS0_OP_BRANCH_ZERO:
         ++jumpCount;
         ACS_traceScriptACS0(vm, lumpLength, data, codeTouched,
                             SwapLong(*(int32_t *)(data + index + 4)), jumpCount);
         break;

      case ACS0_OP_BRANCH_CASE:
         ++jumpCount;
         ACS_traceScriptACS0(vm, lumpLength, data, codeTouched,
                             SwapLong(*(int32_t *)(data + index + 8)), jumpCount);
         break;
      }

      index = indexNext;
   }
}

//
// ACS_translateScriptACS0
//
static void ACS_translateScriptACS0(acsvm_t *vm, uint32_t lumpLength, byte *data,
                                    bool *codeTouched, int32_t *codeIndexMap,
                                    uint32_t jumpCount)
{
   int32_t *codePtr = vm->code, *rover;
   uint32_t codeIndex = 1; // Start at 1 so 0 is invalid instruction.
   int32_t op, temp;
   acs0_opdata_t const *opdata;
   int32_t **jumps, **jumpItr;

   // This is used to store all of the places that need to have a jump target
   // translated. The count was determined by the tracer.
   jumps = (int32_t **)Z_Malloc(jumpCount * sizeof(int32_t *), PU_STATIC, NULL);
   jumpItr = jumps;

   // Set the first instruction to a TERMINATE.
   // TODO: Perhaps this should be set to some kind of fatal error indicator?
   *codePtr++ = ACS_OP_SCRIPT_TERMINATE;

   while(codeIndex < lumpLength)
   {
      // Search for code.
      if (!codeTouched[codeIndex])
      {
         ++codeIndex;
         continue;
      }

      rover = (int32_t *)(data + codeIndex);

      op = SwapLong(*rover);

#ifdef RANGECHECK
      // This was already checked in the tracer.
      if(op >= ACS0_OPMAX || op < 0)
         I_Error("ACS0 opcode out of range in translate. opcode %i at %i",
                 (int)op, (int)codeIndex);
#endif

      opdata = &ACS0opdata[op];

      codeIndexMap[codeIndex] = codePtr - vm->code;
      codeIndex += (opdata->args * 4) + 4;

      switch (op)
      {
      case ACS0_OP_LINESPEC1:
      case ACS0_OP_LINESPEC2:
      case ACS0_OP_LINESPEC3:
      case ACS0_OP_LINESPEC4:
      case ACS0_OP_LINESPEC5:
         temp = op - ACS0_OP_LINESPEC1 + 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*++rover);
         *codePtr++ = temp;
         break;

      case ACS0_OP_LINESPEC1_IMM:
      case ACS0_OP_LINESPEC2_IMM:
      case ACS0_OP_LINESPEC3_IMM:
      case ACS0_OP_LINESPEC4_IMM:
      case ACS0_OP_LINESPEC5_IMM:
         temp = opdata->args - 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*++rover);
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = SwapLong(*++rover);
         break;

      case ACS0_OP_BRANCH_IMM:
      case ACS0_OP_BRANCH_NOTZERO:
      case ACS0_OP_BRANCH_ZERO:
         *jumpItr++ = codePtr + 1;
         goto case_direct;

      case ACS0_OP_BRANCH_CASE:
         *jumpItr++ = codePtr + 2;
         goto case_direct;

      default: case_direct:
         // Direct translation.
#ifdef RANGECHECK
         // This should never happen.
         if (!opdata->opdata)
            I_Error("Unknown translation for opcode. opcode %i", (int)op);
#endif

         *codePtr++ = opdata->opdata->op;
         for(int i = opdata->args; i--;)
            *codePtr++ = SwapLong(*++rover);
         break;
      }
   }

#ifdef RANGECHECK
   // If this isn't true, something very wrong has happened internally.
   if (codePtr != vm->code + vm->numCode)
      I_Error("Incorrect code count. %i/%i/%i", (int)(codePtr - vm->code),
              (int)vm->numCode, (int)(lumpLength / 4));

   // Same here, this is just a sanity check.
   if (jumpItr != jumps + jumpCount)
      I_Error("Incorrect jump count. %i/%i", (int)(jumpItr - jumps), (int)jumpCount);
#endif

   // Translate jumps. Has to be done after code in order to jump forward.
   while(jumpItr != jumps)
   {
      codePtr = *--jumpItr;

      if ((uint32_t)*codePtr < lumpLength)
         *codePtr = codeIndexMap[*codePtr];
      else
         *codePtr = 0;
   }
}


//----------------------------------------------------------------------------|
// Global Functions                                                           |
//

//
// ACS_LoadScriptACS0
//
void ACS_LoadScriptACS0(acsvm_t *vm, int lump, byte *data)
{
   int32_t *codeIndexMap;
   bool *codeTouched;
   uint32_t jumpCount = 0;
   uint32_t lumpLength = wGlobalDir.lumpLength(lump);
   uint32_t lumpAvail; // Used in bounds checking.
   int32_t *rover;
   uint32_t tableIndex;

   // Header + table index + script count + string count = 16 bytes.
   if (lumpLength < 16) return;
   lumpAvail = lumpLength - 16;

   rover = (int32_t *)data + 1;

   // Find script table.
   tableIndex = SwapLong(*rover);

   // At the index there must be at least the script count and string count.
   // Subtracting from lumpLength instead of adding to tableIndex in case the
   // latter would cause an overflow.
   if (tableIndex > lumpLength - 8) return;

   rover = (int32_t *)(data + tableIndex);

   // Read script count.
   vm->numScripts = SwapLong(*rover++);

   // Verify that there is enough space for the given number of scripts.
   if (vm->numScripts * 12 > lumpAvail) return;
   lumpAvail -= vm->numScripts * 12;

   // Also verify that the string count will be inside the lump.
   if (tableIndex + 8 > lumpLength - (vm->numScripts * 12)) return;
   tableIndex += 8 + (vm->numScripts * 12);

   // Read scripts.
   vm->scripts = estructalloctag(acscript_t, vm->numScripts, PU_LEVEL);

   for(acscript_t *end = vm->scripts + vm->numScripts,
       *itr = vm->scripts; itr != end; ++itr)
   {
      itr->number    = SwapLong(*rover++);
      itr->codeIndex = SwapLong(*rover++);
      itr->numArgs   = SwapLong(*rover++);

      if(itr->number >= 1000)
      {
         itr->type = 1;
         itr->number -= 1000;
      }
   }

   // Read string count.
   vm->numStrings = SwapLong(*rover++);

   // Again, verify that there is enough space for the table first.
   if (vm->numStrings * 4 > lumpAvail) return;
   lumpAvail -= vm->numStrings * 4;

   // This time, just verify the end of the table is in bounds.
   if (tableIndex > lumpLength - (vm->numStrings * 4)) return;

   // Read strings.
   vm->strings = (const char **)Z_Malloc(vm->numStrings * sizeof(char *), PU_LEVEL, NULL);

   for(const char **end = vm->strings + vm->numStrings,
       **itr = vm->strings; itr != end; ++itr)
   {
      tableIndex = SwapLong(*rover++);

      if (tableIndex < lumpLength)
         *itr = ACS_readStringACS0(data + tableIndex, data + lumpLength);
      else
         *itr = "";
   }

   // Read code.

   vm->numCode = 1; // Start at 1 so that 0 is an invalid index.

   // Find where there is code by tracing potential execution paths.
   codeTouched = ecalloc(bool *, lumpLength, sizeof(bool));
   for(acscript_t *end = vm->scripts + vm->numScripts,
       *itr = vm->scripts; itr != end; ++itr)
   {
      ACS_traceScriptACS0(vm, lumpLength, data, codeTouched, itr->codeIndex, jumpCount);
   }

   // Translate all the instructions found.
   vm->code = (int32_t *)Z_Malloc(vm->numCode * sizeof(int32_t), PU_LEVEL, NULL);
   codeIndexMap = ecalloc(int32_t *, lumpLength, sizeof(int32_t));
   ACS_translateScriptACS0(vm, lumpLength, data, codeTouched, codeIndexMap, jumpCount);

   efree(codeTouched);

   // Process script indexes.
   for(acscript_t *end = vm->scripts + vm->numScripts,
       *itr = vm->scripts; itr != end; ++itr)
   {
      if(itr->codeIndex < lumpLength)
         itr->codePtr = vm->code + codeIndexMap[itr->codeIndex];
      else
         itr->codePtr = vm->code;
   }

   efree(codeIndexMap);

   vm->loaded = true;
}

// EOF

