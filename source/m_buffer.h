//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
//------------------------------------------------------------------------------
//
// Purpose: Buffered file output.
// Authors: James Haley
//

#ifndef M_BUFFER_H__
#define M_BUFFER_H__

// Required for: byte
#include "doomtype.h"
#include "m_collection.h"

//
// An exception class for buffered IO errors
//
class BufferedIOException
{
protected:
    const char *message;

public:
    explicit BufferedIOException(const char *pMsg) : message(pMsg) {}
    const char *GetMessage() { return message; }
};

//
// BufferedFileBase
//
// Base class for buffers. Eventually I hpe to add an extra layer of indirection
// so that this can function on more than just raw physical files (for example a
// compressed data source).
//
class BufferedFileBase
{
protected:
    FILE  *f;        // destination or source file
    byte  *buffer;   // buffer
    size_t len;      // total buffer length
    size_t idx;      // current index
    int    endian;   // endianness indicator
    bool   throwing; // throws exceptions on IO errors
    bool   ownFile;  // buffer owns the file

    void initBuffer(size_t pLen, int pEndian);

public:
    BufferedFileBase() : f(nullptr), buffer(nullptr), len(0), idx(0), endian(0), throwing(false), ownFile(false) {}

    virtual ~BufferedFileBase()
    {
        if(ownFile && f)
            fclose(f);

        if(buffer)
        {
            efree(buffer);
            buffer = nullptr;
        }
    }

    long         tell();
    virtual void close();

    void swapLong(int32_t &x);
    void swapShort(int16_t &x);
    void swapULong(uint32_t &x);
    void swapUShort(uint16_t &x);

    void setThrowing(bool val) { throwing = val; }
    bool getThrowing() const { return throwing; }

    // endianness values
    enum
    {
        NENDIAN, // doesn't swap shorts or ints
        LENDIAN, // swaps shorts/ints to little endian
        BENDIAN  // swaps shorts/ints to big endian
    };
};

//
// IOutBuffer
//
class IOutBuffer
{
public:
    virtual bool write(const void *data, size_t size) = 0;
    virtual bool writeSint64(int64_t num)             = 0;
    virtual bool writeUint64(uint64_t num)            = 0;
    virtual bool writeSint32(int32_t num)             = 0;
    virtual bool writeUint32(uint32_t num)            = 0;
    virtual bool writeSint16(int16_t num)             = 0;
    virtual bool writeUint16(uint16_t num)            = 0;
    virtual bool writeSint8(int8_t num)               = 0;
    virtual bool writeUint8(uint8_t num)              = 0;
};

//
// OutBuffer
//
// Buffered binary file output.
//
class OutBuffer : public BufferedFileBase, public IOutBuffer
{
public:
    bool createFile(const char *filename, size_t pLen, int pEndian);
    bool flush();
    void close() override;

    bool write(const void *data, size_t size) override;
    bool writeSint64(int64_t num) override;
    bool writeUint64(uint64_t num) override;
    bool writeSint32(int32_t num) override;
    bool writeUint32(uint32_t num) override;
    bool writeSint16(int16_t num) override;
    bool writeUint16(uint16_t num) override;
    bool writeSint8(int8_t num) override;
    bool writeUint8(uint8_t num) override;
};

class OutMemoryBuffer : public IOutBuffer
{
public:
    bool write(const void *data, size_t size) override;
    bool writeSint64(int64_t num) override;
    bool writeUint64(uint64_t num) override;
    bool writeSint32(int32_t num) override;
    bool writeUint32(uint32_t num) override;
    bool writeSint16(int16_t num) override;
    bool writeUint16(uint16_t num) override;
    bool writeSint8(int8_t num) override;
    bool writeUint8(uint8_t num) override;

    PODCollection<byte> takeData() { return std::move(_data); }

private:
    PODCollection<byte> _data;
};

class IInBuffer
{
public:
    virtual int    seek(long offset, int origin) = 0;
    virtual long   itell()                       = 0;
    virtual size_t read(void *dest, size_t size) = 0;
    virtual int    skip(size_t skipAmt)          = 0;
    virtual bool   readSint64(int64_t &num)      = 0;
    virtual bool   readUint64(uint64_t &num)     = 0;
    virtual bool   readSint32(int32_t &num)      = 0;
    virtual bool   readUint32(uint32_t &num)     = 0;
    virtual bool   readSint16(int16_t &num)      = 0;
    virtual bool   readUint16(uint16_t &num)     = 0;
    virtual bool   readSint8(int8_t &num)        = 0;
    virtual bool   readUint8(uint8_t &num)       = 0;
};

//
// InBuffer
//
// Buffered binary file input.
//
class InBuffer : public BufferedFileBase, public IInBuffer
{
public:
    InBuffer() : BufferedFileBase() {}

    bool openFile(const char *filename, int pEndian);
    bool openExisting(FILE *f, int pEndian);

    int    seek(long offset, int origin) override;
    long   itell() override { return BufferedFileBase::tell(); }
    size_t read(void *dest, size_t size) override;
    int    skip(size_t skipAmt) override;
    bool   readSint64(int64_t &num) override;
    bool   readUint64(uint64_t &num) override;
    bool   readSint32(int32_t &num) override;
    bool   readUint32(uint32_t &num) override;
    bool   readSint16(int16_t &num) override;
    bool   readUint16(uint16_t &num) override;
    bool   readSint8(int8_t &num) override;
    bool   readUint8(uint8_t &num) override;
};

class InMemoryBuffer : public IInBuffer
{
public:
    InMemoryBuffer() : _data{} {}
    explicit InMemoryBuffer(const PODCollection<byte> *data) : _data(data) {}
    void setData(const PODCollection<byte> *data) { _data = data; }

    int    seek(long offset, int origin) override;
    long   itell() override { return (long)_pos; }
    size_t read(void *dest, size_t size) override;
    int    skip(size_t skipAmt) override;
    bool   readSint64(int64_t &num) override;
    bool   readUint64(uint64_t &num) override;
    bool   readSint32(int32_t &num) override;
    bool   readUint32(uint32_t &num) override;
    bool   readSint16(int16_t &num) override;
    bool   readUint16(uint16_t &num) override;
    bool   readSint8(int8_t &num) override;
    bool   readUint8(uint8_t &num) override;

private:
    size_t                     _pos = 0;
    const PODCollection<byte> *_data;
};

#endif

// EOF

