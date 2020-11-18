#include "gtest/gtest.h"
#include "../source/d_dehtbl.h"
#include "../source/p_mobj.h"

TEST(DDehTbl, HashTableKey)
{
   ASSERT_EQ(D_HashTableKey("Eternity"), D_HashTableKey("EtErNiTy"));
   ASSERT_NE(D_HashTableKeyCase("Eternity"), D_HashTableKeyCase("EtErNiTy"));
}

static dehflags_t s_list[] =
{
   { "FLAG1", 1 },
   { "FLAG2", 2 },
   { "FLAG3", 4 },
   { "FLAG3", 8, 2 },   // mode 2
   {}
};

TEST(DDehTbl, ParseFlag)
{
   dehflagset_t set =
   {
      s_list,
      DEHFLAGS_MODE_ALL,
   };

   ASSERT_EQ(deh_ParseFlag(&set, "FLAG1"), &s_list[0]);
   ASSERT_EQ(deh_ParseFlag(&set, "flag3"), &s_list[2]);
   ASSERT_EQ(deh_ParseFlag(&set, "flag2"), &s_list[1]);
   ASSERT_EQ(deh_ParseFlag(&set, "FLAG4"), nullptr);

   set.mode = 2;
   ASSERT_EQ(deh_ParseFlag(&set, "FLAG1"), nullptr);
   ASSERT_EQ(deh_ParseFlag(&set, "FLAG3"), &s_list[3]);
}

TEST(DDehTbl, ParseFlagCombined)
{
   ASSERT_EQ(deh_ParseFlagCombined("JUNK"), nullptr); // not exist
   ASSERT_EQ(deh_ParseFlagCombined("NoClip")->value, MF_NOCLIP);
   ASSERT_EQ(deh_ParseFlagCombined("SLIDE")->value, MF3_SLIDE);   // pick the right one
}

TEST(DDehTbl, ParseFlags)
{
   dehflagset_t set =
   {
      s_list
   };


   char flagstring[] = "flag1, flag2";
   char *flagptr = flagstring;
   deh_ParseFlags(&set, &flagptr);
   ASSERT_EQ(set.results[0], s_list[0].value | s_list[1].value);

   char flagstring2[] = "flag1+flag3";
   flagptr = flagstring2;
   deh_ParseFlags(&set, &flagptr);
   ASSERT_EQ(set.results[0], s_list[0].value | s_list[2].value);

   char flagstring3[] = "junk+flag2, flag1|junk";
   flagptr = flagstring3;
   deh_ParseFlags(&set, &flagptr);
   ASSERT_EQ(set.results[0], s_list[0].value | s_list[1].value);

   char flagstring4[] = "flag2 flag3";
   flagptr = flagstring4;
   deh_ParseFlags(&set, &flagptr);
   ASSERT_EQ(set.results[0], s_list[1].value | s_list[2].value);

   char flagstring5[] = "flag1|Flag2|flag3";
   flagptr = flagstring5;
   deh_ParseFlags(&set, &flagptr);
   ASSERT_EQ(set.results[0], s_list[0].value | s_list[1].value | s_list[2].value);
}
