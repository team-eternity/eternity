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
// ACS instructions.
//
//----------------------------------------------------------------------------

// Internal instructions.
#ifdef ACS_OP
   ACS_OP(NOP, 0)

   // Special Commands
   ACS_OP(LINESPEC, 2)
   ACS_OP(LINESPEC_IMM, -1)

   // SET
   ACS_OP(SET_LOCALVAR, 1)
   ACS_OP(SET_MAPVAR,   1)
   ACS_OP(SET_WORLDVAR, 1)

   // GET
   ACS_OP(GET_IMM,      1)
   ACS_OP(GET_LOCALVAR, 1)
   ACS_OP(GET_MAPVAR,   1)
   ACS_OP(GET_WORLDVAR, 1)

   // Binary Ops
   ACS_OP(ADD_STACK,    0)
   ACS_OP(ADD_LOCALVAR, 1)
   ACS_OP(ADD_MAPVAR,   1)
   ACS_OP(ADD_WORLDVAR, 1)
   ACS_OP(AND_STACK,    0)
   ACS_OP(CMP_EQ,       0)
   ACS_OP(CMP_NE,       0)
   ACS_OP(CMP_LT,       0)
   ACS_OP(CMP_GT,       0)
   ACS_OP(CMP_LE,       0)
   ACS_OP(CMP_GE,       0)
   ACS_OP(DEC_LOCALVAR, 1)
   ACS_OP(DEC_MAPVAR,   1)
   ACS_OP(DEC_WORLDVAR, 1)
   ACS_OP(DIV_STACK,    0)
   ACS_OP(DIV_LOCALVAR, 1)
   ACS_OP(DIV_MAPVAR,   1)
   ACS_OP(DIV_WORLDVAR, 1)
   ACS_OP(IOR_STACK,    0)
   ACS_OP(INC_LOCALVAR, 1)
   ACS_OP(INC_MAPVAR,   1)
   ACS_OP(INC_WORLDVAR, 1)
   ACS_OP(LSH_STACK,    0)
   ACS_OP(MOD_STACK,    0)
   ACS_OP(MOD_LOCALVAR, 1)
   ACS_OP(MOD_MAPVAR,   1)
   ACS_OP(MOD_WORLDVAR, 1)
   ACS_OP(MUL_STACK,    0)
   ACS_OP(MUL_LOCALVAR, 1)
   ACS_OP(MUL_MAPVAR,   1)
   ACS_OP(MUL_WORLDVAR, 1)
   ACS_OP(RSH_STACK,    0)
   ACS_OP(SUB_STACK,    0)
   ACS_OP(SUB_LOCALVAR, 1)
   ACS_OP(SUB_MAPVAR,   1)
   ACS_OP(SUB_WORLDVAR, 1)
   ACS_OP(XOR_STACK,    0)

   // Unary Ops
   ACS_OP(NEGATE_STACK, 0)

   // Logical Ops
   ACS_OP(LOGAND_STACK, 0)
   ACS_OP(LOGIOR_STACK, 0)
   ACS_OP(LOGNOT_STACK, 0)

   // Branching
   ACS_OP(BRANCH_CASE,    2)
   ACS_OP(BRANCH_IMM,     1)
   ACS_OP(BRANCH_NOTZERO, 1)
   ACS_OP(BRANCH_ZERO,    1)

   // Stack Control
   ACS_OP(STACK_DROP, 0)

   // Script Control
   ACS_OP(SCRIPT_RESTART,   0)
   ACS_OP(SCRIPT_SUSPEND,   0)
   ACS_OP(SCRIPT_TERMINATE, 0)

   // Printing
   ACS_OP(STARTPRINT, 0)
   ACS_OP(ENDPRINT, 0)
   ACS_OP(ENDPRINTBOLD, 0)
   ACS_OP(PRINTSTRING, 0)
   ACS_OP(PRINTINT, 0)
   ACS_OP(PRINTCHAR, 0)

   // Miscellaneous
   ACS_OP(TAGSTRING, 0)
   ACS_OP(DELAY, 0)
   ACS_OP(DELAY_IMM, 1)
   ACS_OP(RANDOM, 0)
   ACS_OP(RANDOM_IMM, 2)
   ACS_OP(THINGCOUNT, 0)
   ACS_OP(THINGCOUNT_IMM, 2)
   ACS_OP(TAGWAIT, 0)
   ACS_OP(TAGWAIT_IMM, 1)
   ACS_OP(POLYWAIT, 0)
   ACS_OP(POLYWAIT_IMM, 1)
   ACS_OP(CHANGEFLOOR, 0)
   ACS_OP(CHANGEFLOOR_IMM, 2)
   ACS_OP(CHANGECEILING, 0)
   ACS_OP(CHANGECEILING_IMM, 2)
   ACS_OP(LINESIDE, 0)
   ACS_OP(SCRIPTWAIT, 0)
   ACS_OP(SCRIPTWAIT_IMM, 1)
   ACS_OP(CLEARLINESPECIAL, 0)
   ACS_OP(PLAYERCOUNT, 0)
   ACS_OP(GAMETYPE, 0)
   ACS_OP(GAMESKILL, 0)
   ACS_OP(TIMER, 0)
   ACS_OP(SECTORSOUND, 0)
   ACS_OP(AMBIENTSOUND, 0)
   ACS_OP(SOUNDSEQUENCE, 0)
   ACS_OP(SETLINETEXTURE, 0)
   ACS_OP(SETLINEBLOCKING, 0)
   ACS_OP(SETLINESPECIAL, 0)
   ACS_OP(THINGSOUND, 0)
#endif//ACS_OP

// ACS0 instructions.
#ifdef ACS0_OP
   /*   0 */ ACS0_OP(&ACSopdata[ACS_OP_NOP], NOP, 0)
   /*   1 */ ACS0_OP(&ACSopdata[ACS_OP_SCRIPT_TERMINATE], SCRIPT_TERMINATE, 0)
   /*   2 */ ACS0_OP(&ACSopdata[ACS_OP_SCRIPT_SUSPEND], SCRIPT_SUSPEND, 0)
   /*   3 */ ACS0_OP(&ACSopdata[ACS_OP_GET_IMM], GET_IMM, 1)
   /*   4 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC    ], LINESPEC1,     1)
   /*   5 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC    ], LINESPEC2,     1)
   /*   6 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC    ], LINESPEC3,     1)
   /*   7 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC    ], LINESPEC4,     1)
   /*   8 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC    ], LINESPEC5,     1)
   /*   9 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC_IMM], LINESPEC1_IMM, 2)
   /*  10 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC_IMM], LINESPEC2_IMM, 3)
   /*  11 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC_IMM], LINESPEC3_IMM, 4)
   /*  12 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC_IMM], LINESPEC4_IMM, 5)
   /*  13 */ ACS0_OP(&ACSopdata[ACS_OP_LINESPEC_IMM], LINESPEC5_IMM, 6)
   /*  14 */ ACS0_OP(&ACSopdata[ACS_OP_ADD_STACK], ADD_STACK, 0)
   /*  15 */ ACS0_OP(&ACSopdata[ACS_OP_SUB_STACK], SUB_STACK, 0)
   /*  16 */ ACS0_OP(&ACSopdata[ACS_OP_MUL_STACK], MUL_STACK, 0)
   /*  17 */ ACS0_OP(&ACSopdata[ACS_OP_DIV_STACK], DIV_STACK, 0)
   /*  18 */ ACS0_OP(&ACSopdata[ACS_OP_MOD_STACK], MOD_STACK, 0)
   /*  19 */ ACS0_OP(&ACSopdata[ACS_OP_CMP_EQ], CMP_EQ, 0)
   /*  20 */ ACS0_OP(&ACSopdata[ACS_OP_CMP_NE], CMP_NE, 0)
   /*  21 */ ACS0_OP(&ACSopdata[ACS_OP_CMP_LT], CMP_LT, 0)
   /*  22 */ ACS0_OP(&ACSopdata[ACS_OP_CMP_GT], CMP_GT, 0)
   /*  23 */ ACS0_OP(&ACSopdata[ACS_OP_CMP_LE], CMP_LE, 0)
   /*  24 */ ACS0_OP(&ACSopdata[ACS_OP_CMP_GE], CMP_GE, 0)
   /*  25 */ ACS0_OP(&ACSopdata[ACS_OP_SET_LOCALVAR], SET_LOCALVAR, 1)
   /*  26 */ ACS0_OP(&ACSopdata[ACS_OP_SET_MAPVAR  ], SET_MAPVAR,   1)
   /*  27 */ ACS0_OP(&ACSopdata[ACS_OP_SET_WORLDVAR], SET_WORLDVAR, 1)
   /*  28 */ ACS0_OP(&ACSopdata[ACS_OP_GET_LOCALVAR], GET_LOCALVAR, 1)
   /*  29 */ ACS0_OP(&ACSopdata[ACS_OP_GET_MAPVAR  ], GET_MAPVAR,   1)
   /*  30 */ ACS0_OP(&ACSopdata[ACS_OP_GET_WORLDVAR], GET_WORLDVAR, 1)
   /*  31 */ ACS0_OP(&ACSopdata[ACS_OP_ADD_LOCALVAR], ADD_LOCALVAR, 1)
   /*  32 */ ACS0_OP(&ACSopdata[ACS_OP_ADD_MAPVAR  ], ADD_MAPVAR,   1)
   /*  33 */ ACS0_OP(&ACSopdata[ACS_OP_ADD_WORLDVAR], ADD_WORLDVAR, 1)
   /*  34 */ ACS0_OP(&ACSopdata[ACS_OP_SUB_LOCALVAR], SUB_LOCALVAR, 1)
   /*  35 */ ACS0_OP(&ACSopdata[ACS_OP_SUB_MAPVAR  ], SUB_MAPVAR,   1)
   /*  36 */ ACS0_OP(&ACSopdata[ACS_OP_SUB_WORLDVAR], SUB_WORLDVAR, 1)
   /*  37 */ ACS0_OP(&ACSopdata[ACS_OP_MUL_LOCALVAR], MUL_LOCALVAR, 1)
   /*  38 */ ACS0_OP(&ACSopdata[ACS_OP_MUL_MAPVAR  ], MUL_MAPVAR,   1)
   /*  39 */ ACS0_OP(&ACSopdata[ACS_OP_MUL_WORLDVAR], MUL_WORLDVAR, 1)
   /*  40 */ ACS0_OP(&ACSopdata[ACS_OP_DIV_LOCALVAR], DIV_LOCALVAR, 1)
   /*  41 */ ACS0_OP(&ACSopdata[ACS_OP_DIV_MAPVAR  ], DIV_MAPVAR,   1)
   /*  42 */ ACS0_OP(&ACSopdata[ACS_OP_DIV_WORLDVAR], DIV_WORLDVAR, 1)
   /*  43 */ ACS0_OP(&ACSopdata[ACS_OP_MOD_LOCALVAR], MOD_LOCALVAR, 1)
   /*  44 */ ACS0_OP(&ACSopdata[ACS_OP_MOD_MAPVAR  ], MOD_MAPVAR,   1)
   /*  45 */ ACS0_OP(&ACSopdata[ACS_OP_MOD_WORLDVAR], MOD_WORLDVAR, 1)
   /*  46 */ ACS0_OP(&ACSopdata[ACS_OP_INC_LOCALVAR], INC_LOCALVAR, 1)
   /*  47 */ ACS0_OP(&ACSopdata[ACS_OP_INC_MAPVAR  ], INC_MAPVAR,   1)
   /*  48 */ ACS0_OP(&ACSopdata[ACS_OP_INC_WORLDVAR], INC_WORLDVAR, 1)
   /*  49 */ ACS0_OP(&ACSopdata[ACS_OP_DEC_LOCALVAR], DEC_LOCALVAR, 1)
   /*  50 */ ACS0_OP(&ACSopdata[ACS_OP_DEC_MAPVAR  ], DEC_MAPVAR,   1)
   /*  51 */ ACS0_OP(&ACSopdata[ACS_OP_DEC_WORLDVAR], DEC_WORLDVAR, 1)
   /*  52 */ ACS0_OP(&ACSopdata[ACS_OP_BRANCH_IMM], BRANCH_IMM, 1)
   /*  53 */ ACS0_OP(&ACSopdata[ACS_OP_BRANCH_NOTZERO], BRANCH_NOTZERO, 1)
   /*  54 */ ACS0_OP(&ACSopdata[ACS_OP_STACK_DROP], STACK_DROP, 0)
   /*  55 */ ACS0_OP(&ACSopdata[ACS_OP_DELAY    ], DELAY,     0)
   /*  56 */ ACS0_OP(&ACSopdata[ACS_OP_DELAY_IMM], DELAY_IMM, 1)
   /*  57 */ ACS0_OP(&ACSopdata[ACS_OP_RANDOM    ], RANDOM,     0)
   /*  58 */ ACS0_OP(&ACSopdata[ACS_OP_RANDOM_IMM], RANDOM_IMM, 2)
   /*  59 */ ACS0_OP(&ACSopdata[ACS_OP_THINGCOUNT    ], THINGCOUNT,     0)
   /*  60 */ ACS0_OP(&ACSopdata[ACS_OP_THINGCOUNT_IMM], THINGCOUNT_IMM, 2)
   /*  61 */ ACS0_OP(&ACSopdata[ACS_OP_TAGWAIT    ], TAGWAIT,     0)
   /*  62 */ ACS0_OP(&ACSopdata[ACS_OP_TAGWAIT_IMM], TAGWAIT_IMM, 1)
   /*  63 */ ACS0_OP(&ACSopdata[ACS_OP_POLYWAIT    ], POLYWAIT,     0)
   /*  64 */ ACS0_OP(&ACSopdata[ACS_OP_POLYWAIT_IMM], POLYWAIT_IMM, 1)
   /*  65 */ ACS0_OP(&ACSopdata[ACS_OP_CHANGEFLOOR    ], CHANGEFLOOR,     0)
   /*  66 */ ACS0_OP(&ACSopdata[ACS_OP_CHANGEFLOOR_IMM], CHANGEFLOOR_IMM, 2)
   /*  67 */ ACS0_OP(&ACSopdata[ACS_OP_CHANGECEILING    ], CHANGECEILING,     0)
   /*  68 */ ACS0_OP(&ACSopdata[ACS_OP_CHANGECEILING_IMM], CHANGECEILING_IMM, 2)
   /*  69 */ ACS0_OP(&ACSopdata[ACS_OP_SCRIPT_RESTART], SCRIPT_RESTART, 0)
   /*  70 */ ACS0_OP(&ACSopdata[ACS_OP_LOGAND_STACK], LOGAND_STACK, 0)
   /*  71 */ ACS0_OP(&ACSopdata[ACS_OP_LOGIOR_STACK], LOGIOR_STACK, 0)
   /*  72 */ ACS0_OP(&ACSopdata[ACS_OP_AND_STACK], AND_STACK, 0)
   /*  73 */ ACS0_OP(&ACSopdata[ACS_OP_IOR_STACK], IOR_STACK, 0)
   /*  74 */ ACS0_OP(&ACSopdata[ACS_OP_XOR_STACK], XOR_STACK, 0)
   /*  75 */ ACS0_OP(&ACSopdata[ACS_OP_LOGNOT_STACK], LOGNOT_STACK, 0)
   /*  76 */ ACS0_OP(&ACSopdata[ACS_OP_LSH_STACK], LSH_STACK, 0)
   /*  77 */ ACS0_OP(&ACSopdata[ACS_OP_RSH_STACK], RSH_STACK, 0)
   /*  78 */ ACS0_OP(&ACSopdata[ACS_OP_NEGATE_STACK], NEGATE_STACK, 0)
   /*  79 */ ACS0_OP(&ACSopdata[ACS_OP_BRANCH_ZERO], BRANCH_ZERO, 1)
   /*  80 */ ACS0_OP(&ACSopdata[ACS_OP_LINESIDE], LINESIDE, 0)
   /*  81 */ ACS0_OP(&ACSopdata[ACS_OP_SCRIPTWAIT    ], SCRIPTWAIT,     0)
   /*  82 */ ACS0_OP(&ACSopdata[ACS_OP_SCRIPTWAIT_IMM], SCRIPTWAIT_IMM, 1)
   /*  83 */ ACS0_OP(&ACSopdata[ACS_OP_CLEARLINESPECIAL], CLEARLINESPECIAL, 0)
   /*  84 */ ACS0_OP(&ACSopdata[ACS_OP_BRANCH_CASE], BRANCH_CASE, 2)
   /*  85 */ ACS0_OP(&ACSopdata[ACS_OP_STARTPRINT], STARTPRINT, 0)
   /*  86 */ ACS0_OP(&ACSopdata[ACS_OP_ENDPRINT], ENDPRINT, 0)
   /*  87 */ ACS0_OP(&ACSopdata[ACS_OP_PRINTSTRING], PRINTSTRING, 0)
   /*  88 */ ACS0_OP(&ACSopdata[ACS_OP_PRINTINT], PRINTINT, 0)
   /*  89 */ ACS0_OP(&ACSopdata[ACS_OP_PRINTCHAR], PRINTCHAR, 0)
   /*  90 */ ACS0_OP(&ACSopdata[ACS_OP_PLAYERCOUNT], PLAYERCOUNT, 0)
   /*  91 */ ACS0_OP(&ACSopdata[ACS_OP_GAMETYPE], GAMETYPE, 0)
   /*  92 */ ACS0_OP(&ACSopdata[ACS_OP_GAMESKILL], GAMESKILL, 0)
   /*  93 */ ACS0_OP(&ACSopdata[ACS_OP_TIMER], TIMER, 0)
   /*  94 */ ACS0_OP(&ACSopdata[ACS_OP_SECTORSOUND], SECTORSOUND, 0)
   /*  95 */ ACS0_OP(&ACSopdata[ACS_OP_AMBIENTSOUND], AMBIENTSOUND, 0)
   /*  96 */ ACS0_OP(&ACSopdata[ACS_OP_SOUNDSEQUENCE], SOUNDSEQUENCE, 0)
   /*  97 */ ACS0_OP(&ACSopdata[ACS_OP_SETLINETEXTURE], SETLINETEXTURE, 0)
   /*  98 */ ACS0_OP(&ACSopdata[ACS_OP_SETLINEBLOCKING], SETLINEBLOCKING, 0)
   /*  99 */ ACS0_OP(&ACSopdata[ACS_OP_SETLINESPECIAL], SETLINESPECIAL, 0)
   /* 100 */ ACS0_OP(&ACSopdata[ACS_OP_THINGSOUND], THINGSOUND, 0)
   /* 101 */ ACS0_OP(&ACSopdata[ACS_OP_ENDPRINTBOLD], ENDPRINTBOLD, 0)
#endif//ACS0_OP

// EOF

