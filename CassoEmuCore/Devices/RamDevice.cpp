#include "Pch.h"

#include "RamDevice.h"
#include "Core/DramPowerOnPattern.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RamDevice
//
////////////////////////////////////////////////////////////////////////////////

RamDevice::RamDevice (Word start, Word end)
    : m_start (start),
      m_end   (end),
      m_data  (static_cast<size_t> (end - start + 1), 0)
{
    m_pages.Attach (nullptr, m_data.data(), m_data.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte RamDevice::Read (Word address)
{
    return m_data[address - m_start];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void RamDevice::Write (Word address, Byte value)
{
    m_data[address - m_start] = value;

    m_pages.MarkWritten (static_cast<size_t> (address - m_start));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void RamDevice::Reset()
{
    fill (m_data.begin(), m_data.end(), Byte (0));

    m_pages.MarkAllWritten();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4 / FR-034: //e soft reset preserves DRAM contents. No-op so the
//  ROM reset handler sees the same memory image it left behind.
//
////////////////////////////////////////////////////////////////////////////////

void RamDevice::SoftReset()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Refill the buffer with the DRAM power-on pattern. Its few seeded bytes
//  come from the shared Prng, so it is deterministic whenever the caller
//  pinned the seed.
//
////////////////////////////////////////////////////////////////////////////////

void RamDevice::PowerCycle (Prng & prng)
{
    DramPowerOnPattern::Fill (m_data.data(), m_data.size(), prng);

    m_pages.MarkAllWritten();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachPages
//
//  Registers the buffer with the bus, whose page table maps it, so every
//  store into it marks the page written; see RamPages.
//
////////////////////////////////////////////////////////////////////////////////

void RamDevice::AttachPages (MemoryBus & bus)
{
    m_pages.Attach (&bus, m_data.data(), m_data.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> RamDevice::Create (const DeviceConfig & config, MemoryBus & bus)
{
    // RamDevice instances are created directly by EmulatorShell from RAM regions
    // in the machine config. The DeviceConfig path is not used.
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);
    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RamDevice::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteWord  (m_start);
    writer.WriteWord  (m_end);

    m_pages.Save (writer, m_data.data(), m_data.size());

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  Fails with ERROR_INVALID_DATA when the saved RAM covered other addresses,
//  before any byte is read into this one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RamDevice::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;
    Word      start   = 0;
    Word      end     = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadWord (start);
    reader.ReadWord (end);

    hr = reader.GetResult();
    CHR (hr);

    CBREx (start == m_start && end == m_end, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    reader.ReadBytes (m_data.data(), m_data.size());

    m_pages.MarkAllWritten();

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}