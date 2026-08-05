#include "Pch.h"

#include "RomDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RomDevice
//
////////////////////////////////////////////////////////////////////////////////

RomDevice::RomDevice (Word start, Word end, vector<Byte> && data)
    : m_start (start),
      m_end   (end),
      m_data  (move (data))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte RomDevice::Read (Word address)
{
    size_t offset = address - m_start;



    // Past the image (a short ROM file mapped to a wider range) reads as the
    // floating-bus 0xFF an empty socket would give.
    return (offset < m_data.size()) ? m_data[offset] : (Byte) 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write (ignored — ROM is read-only)
//
////////////////////////////////////////////////////////////////////////////////

void RomDevice::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (address);
    UNREFERENCED_PARAMETER (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset (no-op — ROM contents are immutable)
//
////////////////////////////////////////////////////////////////////////////////

void RomDevice::Reset()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateFromFile
//
//  Loads a ROM image and wraps it as a bus device covering an exact address
//  range.
//
//  The file size must match the declared range EXACTLY -- not merely fit. A
//  short ROM would leave a floating-bus hole in the middle of the address map
//  rather than at a recognizable end, and a program reading it gets plausible
//  garbage instead of an obvious failure. An oversized file is equally
//  suspect: it is a different image than the config believes.
//
//  Failure returns null with a message in outError rather than throwing or
//  asserting, because the caller is loading a user-supplied machine config and
//  needs to report which file was wrong.
//
//  The message names the file, its actual size, the range, and the required
//  size, since a ROM mismatch is nearly always a wrong-file problem and those
//  four facts are what identify it.
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> RomDevice::CreateFromFile (
    Word start, Word end, const string & filePath, string & outError)
{
    size_t                    expectedSize = 0;
    unique_ptr<MemoryDevice>  device;
    bool                      ok           = false;
    streampos                 fileSize     = 0;



    ifstream                    file (filePath, ios::binary | ios::ate);
    expectedSize = static_cast<size_t> (end - start + 1);
    ok = file.good();

    // Null out, message in outError. The size must match EXACTLY: a ROM that
    // does not fill its declared range would leave a floating-bus hole in the
    // middle of the address map rather than at a recognizable end.
    if (!ok)
    {
        outError = format ("Cannot open ROM file: {}", filePath);
    }
    else
    {
        fileSize = file.tellg();
        file.seekg (0, ios::beg);

        ok = (static_cast<size_t> (fileSize) == expectedSize);

        if (!ok)
        {
            outError = format ("ROM file '{}' is {} bytes but address range ${:04X}-${:04X} requires {} bytes",
                               filePath,
                               static_cast<size_t> (fileSize),
                               start, end,
                               expectedSize);
        }
    }

    if (ok)
    {
        vector<Byte> data (static_cast<size_t> (fileSize));
        file.read (reinterpret_cast<char *> (data.data()), fileSize);

        device = make_unique<RomDevice> (start, end, move (data));
    }

    return device;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateFromData
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> RomDevice::CreateFromData (
    Word start, Word end, const Byte * data, size_t size)
{
    vector<Byte> romData (data, data + size);
    return make_unique<RomDevice> (start, end, move (romData));
}
