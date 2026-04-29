#include "Pch.h"

#include "RomDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RomDevice
//
////////////////////////////////////////////////////////////////////////////////

RomDevice::RomDevice (Word start, Word end, std::vector<Byte> && data)
    : m_start (start),
      m_end   (end),
      m_data  (std::move (data))
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

    if (offset < m_data.size ())
    {
        return m_data[offset];
    }

    return 0xFF;
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

void RomDevice::Reset ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateFromFile
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> RomDevice::CreateFromFile (
    Word start, Word end, const std::string & filePath, std::string & outError)
{
    std::ifstream file (filePath, std::ios::binary | std::ios::ate);

    if (!file.good ())
    {
        outError = std::format ("Cannot open ROM file: {}", filePath);
        return nullptr;
    }

    auto fileSize = file.tellg ();
    file.seekg (0, std::ios::beg);

    std::vector<Byte> data (static_cast<size_t> (fileSize));
    file.read (reinterpret_cast<char *> (data.data ()), fileSize);

    return std::make_unique<RomDevice> (start, end, std::move (data));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateFromData
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> RomDevice::CreateFromData (
    Word start, Word end, const Byte * data, size_t size)
{
    std::vector<Byte> romData (data, data + size);
    return std::make_unique<RomDevice> (start, end, std::move (romData));
}
