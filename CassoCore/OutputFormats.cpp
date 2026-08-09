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
//  Emits the assembled image as Motorola S-records: an S0 header, S1 data
//  records, and an S9 termination record carrying the entry point.
//
//  All three record types are written, not just the data. Programmers and
//  loaders reject a stream that ends without a termination record, and the
//  entry point has nowhere else to live in this format.
//
//  Sixteen data bytes per record is the near-universal convention. The format
//  permits more, but tools and terminals of the era expect a line of this
//  width, and matching it is the whole reason to emit S-records at all.
//
//  The checksum is accumulated as each field is written rather than computed
//  afterwards over a buffer, so it cannot drift from what was actually
//  emitted. It covers the byte count, the address, and the data, then is
//  one's-complemented -- the format's own definition.
//
//  The byte count includes the two address bytes and the checksum itself, not
//  merely the payload; getting that wrong produces records that look right and
//  load short.
//
//  A data length beyond the supplied buffer is clamped, so a caller passing an
//  end address past the end of its data truncates cleanly instead of reading
//  off the end.
//
////////////////////////////////////////////////////////////////////////////////

void OutputFormats::WriteSRecord (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream)
{
    size_t offset = 0;



    // S0 header record
    {
        int   headerLen = 0;
        int   byteCount = 0;
        Byte  checksum  = 0;

        // S0 has 2-byte address (0000), data = "HDR" (optional), checksum
        const char * header = "HDR";
        headerLen = 3;
        byteCount = 3 + headerLen; // 2 addr + data + checksum
        checksum = (Byte) byteCount;

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


    while (offset < dataLen)
    {
        int   chunkSize = 16;
        int   byteCount = 0;
        Byte  checksum  = 0;
        Byte  addrHi    = 0;
        Byte  addrLo    = 0;

        if (offset + chunkSize > dataLen)
        {
            chunkSize = (int) (dataLen - offset);
        }

        // Byte count = 2 (address) + data + 1 (checksum)
        byteCount = 3 + chunkSize;
        checksum = (Byte) byteCount;
        addrHi = (Byte) ((addr >> 8) & 0xFF);
        addrLo = (Byte) (addr & 0xFF);

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
//  Emits the assembled image as Intel HEX: data records, then an
//  end-of-file record.
//
//  Structurally the twin of WriteSRecord, with two differences that are easy
//  to conflate when editing both. Intel HEX byte-counts only the DATA -- not
//  the address, record type, or checksum, as S-records do -- and its checksum
//  is the TWO'S complement of the sum, where S-records use the one's
//  complement. Copying either rule across produces a file every loader
//  rejects.
//
//  Sixteen bytes per record again matches what tools of the era expect.
//
//  The checksum is accumulated as fields are written, so it cannot drift from
//  what was emitted.
//
//  A data length beyond the supplied buffer is clamped, so an end address past
//  the end of the data truncates cleanly.
//
////////////////////////////////////////////////////////////////////////////////

void OutputFormats::WriteIntelHex (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream)
{
    Word    addr    = startAddr;
    size_t  dataLen = (endAddr > startAddr) ? (endAddr - startAddr) : 0;
    size_t  offset  = 0;



    if (dataLen > data.size())
    {
        dataLen = data.size();
    }


    while (offset < dataLen)
    {
        int   chunkSize = 16;
        Byte  addrHi    = 0;
        Byte  addrLo    = 0;
        Byte  recType   = 0;
        Byte  checksum  = 0;

        if (offset + chunkSize > dataLen)
        {
            chunkSize = (int) (dataLen - offset);
        }

        addrHi = (Byte) ((addr >> 8) & 0xFF);
        addrLo = (Byte) (addr & 0xFF);
        recType = 0x00; // Data record

        // Checksum = two's complement of (bytecount + addrHi + addrLo + recType + data bytes)
        checksum = (Byte) chunkSize + addrHi + addrLo + recType;

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
