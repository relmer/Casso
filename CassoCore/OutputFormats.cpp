#include "Pch.h"

#include "OutputFormats.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBinary
//
////////////////////////////////////////////////////////////////////////////////

void OutputFormats::WriteBinary (const std::vector<Byte> & data, std::ostream & stream, Byte fillByte)
{
    stream.write (reinterpret_cast<const char *> (data.data()), data.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSRecordByte — format a byte as 2 uppercase hex chars
//
////////////////////////////////////////////////////////////////////////////////

static void WriteHexByte (std::ostream & stream, Byte value)
{
    char buf[4];
    snprintf (buf, sizeof (buf), "%02X", value);
    stream << buf;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSRecord
//
////////////////////////////////////////////////////////////////////////////////

void OutputFormats::WriteSRecord (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream)
{
    // S0 header record
    {
        // S0 has 2-byte address (0000), data = "HDR" (optional), checksum
        const char * header = "HDR";
        int          headerLen = 3;
        int          byteCount = 3 + headerLen;  // 2 addr + data + checksum
        Byte         checksum  = (Byte) byteCount;

        stream << "S0";
        WriteHexByte (stream, (Byte) byteCount);
        WriteHexByte (stream, 0x00);
        checksum += 0x00;
        WriteHexByte (stream, 0x00);
        checksum += 0x00;

        for (int i = 0; i < headerLen; i++)
        {
            WriteHexByte (stream, (Byte) header[i]);
            checksum += (Byte) header[i];
        }

        checksum = ~checksum;
        WriteHexByte (stream, checksum);
        stream << "\n";
    }

    // S1 data records (16 bytes per line)
    Word   addr      = startAddr;
    size_t dataLen   = (endAddr > startAddr) ? (endAddr - startAddr) : 0;

    if (dataLen > data.size())
    {
        dataLen = data.size();
    }

    size_t offset = 0;

    while (offset < dataLen)
    {
        int chunkSize = 16;

        if (offset + chunkSize > dataLen)
        {
            chunkSize = (int) (dataLen - offset);
        }

        // Byte count = 2 (address) + data + 1 (checksum)
        int  byteCount = 3 + chunkSize;
        Byte checksum  = (Byte) byteCount;
        Byte addrHi    = (Byte) ((addr >> 8) & 0xFF);
        Byte addrLo    = (Byte) (addr & 0xFF);

        stream << "S1";
        WriteHexByte (stream, (Byte) byteCount);
        WriteHexByte (stream, addrHi);
        checksum += addrHi;
        WriteHexByte (stream, addrLo);
        checksum += addrLo;

        for (int i = 0; i < chunkSize; i++)
        {
            Byte b = data[offset + i];
            WriteHexByte (stream, b);
            checksum += b;
        }

        checksum = ~checksum;
        WriteHexByte (stream, checksum);
        stream << "\n";

        addr   += (Word) chunkSize;
        offset += chunkSize;
    }

    // S9 start address record
    {
        int  byteCount = 3;  // 2 address + 1 checksum
        Byte checksum  = (Byte) byteCount;
        Byte addrHi    = (Byte) ((entryPoint >> 8) & 0xFF);
        Byte addrLo    = (Byte) (entryPoint & 0xFF);

        stream << "S9";
        WriteHexByte (stream, (Byte) byteCount);
        WriteHexByte (stream, addrHi);
        checksum += addrHi;
        WriteHexByte (stream, addrLo);
        checksum += addrLo;
        checksum = ~checksum;
        WriteHexByte (stream, checksum);
        stream << "\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteIntelHex
//
////////////////////////////////////////////////////////////////////////////////

void OutputFormats::WriteIntelHex (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream)
{
    Word   addr    = startAddr;
    size_t dataLen = (endAddr > startAddr) ? (endAddr - startAddr) : 0;

    if (dataLen > data.size())
    {
        dataLen = data.size();
    }

    size_t offset = 0;

    while (offset < dataLen)
    {
        int chunkSize = 16;

        if (offset + chunkSize > dataLen)
        {
            chunkSize = (int) (dataLen - offset);
        }

        Byte addrHi  = (Byte) ((addr >> 8) & 0xFF);
        Byte addrLo  = (Byte) (addr & 0xFF);
        Byte recType = 0x00;  // Data record

        // Checksum = two's complement of (bytecount + addrHi + addrLo + recType + data bytes)
        Byte checksum = (Byte) chunkSize + addrHi + addrLo + recType;

        stream << ":";
        WriteHexByte (stream, (Byte) chunkSize);
        WriteHexByte (stream, addrHi);
        WriteHexByte (stream, addrLo);
        WriteHexByte (stream, recType);

        for (int i = 0; i < chunkSize; i++)
        {
            Byte b = data[offset + i];
            WriteHexByte (stream, b);
            checksum += b;
        }

        checksum = (Byte) (-(int8_t) checksum);
        WriteHexByte (stream, checksum);
        stream << "\n";

        addr   += (Word) chunkSize;
        offset += chunkSize;
    }

    // EOF record
    stream << ":00000001FF\n";
}
