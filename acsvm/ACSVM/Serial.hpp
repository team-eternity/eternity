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

#ifndef ACSVM__Serial_H__
#define ACSVM__Serial_H__

#include "ID.hpp"

#include <climits>


//----------------------------------------------------------------------------|
// Types                                                                      |
//

namespace ACSVM
{
   //
   // Signature
   //
   enum class Signature : std::uint32_t
   {
      Array       = MakeID("ARAY"),
      Environment = MakeID("ENVI"),
      GlobalScope = MakeID("GBLs"),
      HubScope    = MakeID("HUBs"),
      MapScope    = MakeID("MAPs"),
      ModuleScope = MakeID("MODs"),
      Serial      = MakeID("SERI"),
      Thread      = MakeID("THRD"),
   };

   //
   // Serial
   //
   // Read/write functions must either read/write the given number of bytes, or
   // throw an exception.
   //
   class Serial
   {
   public:
      Serial() : version{CurrentVersion}, signs{false} {}

      void loadHead();
      void loadTail();

      virtual void read(char *data, std::size_t size) = 0;
      virtual char readByte();
      void readSign(Signature sign);

      void saveHead();
      void saveTail();

      virtual void write(char const *data, std::size_t size) = 0;
      virtual void writeByte(char data);
      void writeSign(Signature sign);

      unsigned int version;
      bool         signs;


      static constexpr unsigned int CurrentVersion = 1;
   };
}


//----------------------------------------------------------------------------|
// Extern Functions                                                           |
//

namespace ACSVM
{
   constexpr Signature operator ~ (Signature sign)
      {return static_cast<Signature>(~static_cast<std::uint32_t>(sign));}

   //
   // ReadVLN
   //
   template<typename T>
   T ReadVLN(Serial &in)
   {
      T out{0};

      unsigned char c;
      while(((c = in.readByte()) & 0x80))
         out = (out << 7) + (c & 0x7F);
      out = (out << 7) + c;

      return out;
   }

   //
   // WriteVLN
   //
   template<typename T>
   void WriteVLN(Serial &out, T in)
   {
      constexpr std::size_t len = (sizeof(T) * CHAR_BIT + 6) / 7;
      char buf[len], *ptr = buf + len;

      *--ptr = static_cast<char>(in & 0x7F);
      while((in >>= 7))
         *--ptr = static_cast<char>(in & 0x7F) | 0x80;

      out.write(ptr, (buf + len) - ptr);
   }
}

#endif//ACSVM__Serial_H__

