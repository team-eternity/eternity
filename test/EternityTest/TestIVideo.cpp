#include "gtest/gtest.h"
#include "../source/i_video.h"

TEST(IVideo, Geom)
{
   Geom geom;
   ASSERT_EQ(geom.toString(), "640x480w");

   geom.parse("800x600");
   ASSERT_EQ(geom.toString(), "800x600w");

   geom.parse("200x200");
   ASSERT_EQ(geom.toString(), "640x200w");

   geom.parse("320x100");
   ASSERT_EQ(geom.toString(), "320x480w");

   geom.parse("320x200");
   ASSERT_EQ(geom.toString(), "320x200w");

   geom.parse("+500x+500wddffaaavvvshn"); // allow positive signs and reduce doubles, choosing last
   ASSERT_EQ(geom.toString(), "500x500fvhn");

   // NOTE: it alters the previous geom state
   geom.parse("500x500wddffdvvvahsn");
   ASSERT_EQ(geom.toString(), "500x500dan");

   geom.parse("1024X768FD");
   ASSERT_EQ(geom.toString(), "1024x768dan");

   // Change just to windowed mode
   geom.parse("W");
   ASSERT_EQ(geom.toString(), "1024x768wan");

   // Remove sync option
   geom.vsync = Geom::TriState::neutral;
   ASSERT_EQ(geom.toString(), "1024x768wn");

   // Must not parse octal values
   Geom geom2("0800x0600"); // work with a clear one
   ASSERT_EQ(geom2.toString(), "800x600w");

   Geom geom3("x400");
   ASSERT_EQ(geom3.toString(), "640x400w");

   geom3.parse("500");
   ASSERT_EQ(geom3.toString(), "500x400w");

   geom3.parse("x650");
   ASSERT_EQ(geom3.toString(), "500x650w");

   geom3.parse("x"); // make sure this junk x does nothing
   ASSERT_EQ(geom3.toString(), "500x650w");

   geom3.parse("");  // or this
   ASSERT_EQ(geom3.toString(), "500x650w");

   geom3.parse("-");  // or this
   ASSERT_EQ(geom3.toString(), "500x650w");

   geom3.parse("0"); // a stray zero should be interpreted as an invalid width attempt
   ASSERT_EQ(geom3.toString(), "640x650w");

   geom3.parse("500xf");
   ASSERT_EQ(geom3.toString(), "500x650f");

   geom3.parse("-d");  // invalid character followed by flag
   ASSERT_EQ(geom3.toString(), "500x650d");

   geom3.parse("-400x-300");  // negative values should be interpreted likewise as invalid
   ASSERT_EQ(geom3.toString(), "640x480d");

   Geom geom4("60000x40000fd");  // ridiculously large value
   ASSERT_EQ(geom4.toString(), "640x480d");
}
