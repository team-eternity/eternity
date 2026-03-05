//-----------------------------------------------------------------------------
//
// Copyright (C) 2017-2025 David Hill
//
// See COPYING for license information.
//
//-----------------------------------------------------------------------------
//
// Serialization.
//
//-----------------------------------------------------------------------------

#include "Serial.hpp"

#include "BinaryIO.hpp"
#include "Error.hpp"

#include <cstring>


//----------------------------------------------------------------------------|
// Extern Functions                                                           |
//

namespace ACSVM
{
   //
   // Serial::loadHead
   //
   void Serial::loadHead()
   {
      char buf[6] = {};
      read(buf, 6);

      if(std::memcmp(buf, "ACSVM\0", 6))
         throw SerialError{"invalid file signature"};

      version = ReadVLN<unsigned int>(*this);
      if(version > CurrentVersion)
         throw SerialError("unknown version");

      auto flags = ReadVLN<std::uint_fast32_t>(*this);
      signs = flags & 0x0001;
   }

   //
   // Serial::loadTail
   //
   void Serial::loadTail()
   {
      readSign(~Signature::Serial);
   }

   //
   // Serial::readByte
   //
   char Serial::readByte()
   {
      char buf[1];
      read(buf, 1);
      return buf[0];
   }

   //
   // Serial::readSign
   //
   void Serial::readSign(Signature sign)
   {
      if(!signs) return;

      unsigned char buf[4];
      read(reinterpret_cast<char *>(buf), 4);
      auto got = static_cast<Signature>(ReadLE4(buf));

      if(sign != got)
         throw SerialSignError{sign, got};
   }

   //
   // Serial::saveHead
   //
   void Serial::saveHead()
   {
      write("ACSVM\0", 6);
      WriteVLN(*this, CurrentVersion);

      std::uint_fast32_t flags = 0;
      if(signs) flags |= 0x0001;
      WriteVLN(*this, flags);
   }

   //
   // Serial::saveTail
   //
   void Serial::saveTail()
   {
      writeSign(~Signature::Serial);
   }

   //
   // Serial::writeByte
   //
   void Serial::writeByte(char data)
   {
      write(&data, 1);
   }

   //
   // Serial::writeSign
   //
   void Serial::writeSign(Signature sign)
   {
      if(!signs) return;

      unsigned char buf[4];
      WriteLE4(buf, static_cast<std::uint32_t>(sign));
      write(reinterpret_cast<char *>(buf), 4);
   }
}

// EOF

