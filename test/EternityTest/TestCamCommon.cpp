#include "gtest/gtest.h"
#include "../source/cam_common.h"
#include "../source/e_exdata.h"
#include "../source/r_defs.h"
#include "../source/r_portal.h"

TEST(LineOpening, Calculate)
{
   line_t line = {};
   line.sidenum[1] = -1;

   lineopening_t opening = {};
   opening.calculate(&line);

   ASSERT_EQ(opening.openrange, 0);

   side_t frontside = {}, backside = {};
   sector_t front = {}, back = {};
   frontside.sector = &front;
   backside.sector = &back;
   line.frontsector = &front;
   line.backsector = &back;
   line.sidenum[0] = line.sidenum[1] = 0;

   front.srf.floor.height = 10000;
   front.srf.ceiling.height = 20000;
   back.srf.floor.height = 5000;
   back.srf.ceiling.height = 25000;

   opening.calculate(&line);
   ASSERT_EQ(opening.open.floor, 10000);
   ASSERT_EQ(opening.open.ceiling, 20000);
   ASSERT_EQ(opening.openrange, 10000);

   front.srf.floor.height = 4500;
   opening.calculate(&line);
   ASSERT_EQ(opening.open.floor, 5000);
   ASSERT_EQ(opening.open.ceiling, 20000);
   ASSERT_EQ(opening.openrange, 15000);

   back.srf.ceiling.height = 15000;

   line.extflags |= EX_ML_LOWERPORTAL;
   opening.calculate(&line);
   ASSERT_EQ(opening.open.floor, 5000);   // edge portal flag but no portal behind
   ASSERT_EQ(opening.open.ceiling, 15000);
   ASSERT_EQ(opening.openrange, 10000);

   back.srf.floor.pflags |= PS_PASSABLE;
   opening.calculate(&line);
   ASSERT_EQ(opening.open.floor, 4500);   // edge portal now
   ASSERT_EQ(opening.open.ceiling, 15000);
   ASSERT_EQ(opening.openrange, 10500);

   line.extflags |= EX_ML_UPPERPORTAL;
   opening.calculate(&line);
   ASSERT_EQ(opening.open.floor, 4500);   // edge portal flag but no portal behind
   ASSERT_EQ(opening.open.ceiling, 15000);
   ASSERT_EQ(opening.openrange, 10500);

   back.srf.ceiling.pflags |= PS_PASSABLE;
   front.srf.ceiling.height = 30000;
   opening.calculate(&line);
   ASSERT_EQ(opening.open.floor, 4500);   // edge portal flag but no portal behind
   ASSERT_EQ(opening.open.ceiling, 30000);
   ASSERT_EQ(opening.openrange, 25500);
}
