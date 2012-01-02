// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3: 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//   Clientside packet buffer.
//
//-----------------------------------------------------------------------------

#ifndef CL_BUF_H__
#define CL_BUF_H__

class NetPacket
{
public:
   char *data;
   uint32_t size;

   NetPacket() : data(NULL), size(0) {}

   NetPacket(char *input_data, uint32_t input_size)
   {
      size = input_size;
      data = emalloc(char *, size);
      memcpy(data, input_data, size);
   }

   explicit NetPacket(const NetPacket& original)
   {
      size = original.size;
      data = emalloc(char *, size);
      memcpy(data, original.data, size);
   }

   explicit NetPacket(NetPacket& original)
   {
      size = original.size;
      data = emalloc(char *, size);
      memcpy(data, original.data, size);
   }

   NetPacket& operator= (const NetPacket& other)
   {
      if(this != &other)
      {
         size = other.size;
         data = erealloc(char *, data, size);
         memcpy(data, other.data, size);
      }
      return *this;
   }

   ~NetPacket() { efree(data); }

   int32_t getType() const { return *(uint32_t *)data; }

   const char* getName() const
   {
      if(getType() < nm_max_messages)
         return network_message_names[getType()];
      else
         return "Unknown";
   }

   uint32_t getWorldIndex() const { return *(((uint32_t *)data) + 1); }

   uint32_t getSize() const { return size; }

   const char* getData() const { return (const char *)data; }

   bool shouldProcessNow();

   void process();

};

class NetPacketBuffer
{
private:
   std::list<NetPacket *> packet_buffer;
   bool       m_buffering_independently;
   bool       m_needs_flushing;
   bool       m_needs_filling;
   bool       m_synchronized;
   bool       m_enabled;
   dthread_t *m_net_service_thread;
   uint32_t   m_size;
   uint32_t   m_capacity;

public:
   NetPacketBuffer(void);
   void     startBufferingIndependently();
   void     stopBufferingIndependently();
   bool     bufferingIndependently() const { return m_buffering_independently; }
   bool     needsFlushing(void) const { return m_needs_flushing; }
   void     setNeedsFlushing(bool b) { m_needs_flushing = b; }
   bool     needsFilling(void) const { return m_needs_filling; }
   void     setNeedsFilling(bool b) { m_needs_filling = b; }
   bool     synchronized(void) const { return m_synchronized; }
   void     setSynchronized(bool b);
   uint32_t size(void) const { return m_size; }
   void     enable(void);
   void     disable(void);
   bool     enabled(void) const { return m_enabled; }
   bool     adaptive(void) { return m_capacity == 0; }
   bool     overflowed(void);
   uint32_t capacity(void);
   void     setCapacity(uint32_t new_capacity);
   void     add(char *data, uint32_t data_size);
   void     processPacketsForIndex(uint32_t index);
   void     processAllPackets();

   friend int CL_serviceNetwork(void *);

};

extern NetPacketBuffer cl_packet_buffer;

#endif

