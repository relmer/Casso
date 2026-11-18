#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Floppy35/Apple35CardMemory.h"




//  The card CPU's clock: the slot's 7.16 MHz clock times 2/7, in
//  nanoseconds a cycle times 1,000.
static constexpr uint64_t  s_kPicoNsPerCycle = 488'889;
static constexpr uint64_t  s_kPicoPerNs      = 1000;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35CardMemory
//
////////////////////////////////////////////////////////////////////////////////

Apple35CardMemory::Apple35CardMemory() :
    m_ram (kRamSize, 0)
{
    for (int i = 0; i < kDrives; i++)
    {
        m_drives[(size_t) i] = std::make_unique<Sony35Drive> (i);
        m_iwm.AttachDrive (i, m_drives[(size_t) i].get());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple35CardMemory::Read (Word address)
{
    if (address >= kIoPage && address <= kIoPageEnd)
    {
        return AccessIo (address, false, 0);
    }

    if (address >= kRomStart)
    {
        return m_rom.empty() ? (Byte) 0xFF : m_rom[address - kRomStart];
    }

    return m_ram[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void Apple35CardMemory::Write (Word address, Byte value)
{
    if (address >= kIoPage && address <= kIoPageEnd)
    {
        AccessIo (address, true, value);
    }
    else if (address < kRomStart)
    {
        m_ram[address] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Apple35CardMemory::Reset()
{
    m_iwm.Reset();
    m_led = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFirmware
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple35CardMemory::SetFirmware (std::vector<Byte> firmware)
{
    HRESULT  hr     = S_OK;
    size_t   length = firmware.size();



    m_rom.clear();
    CBREx (length == kRomSize, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    m_rom = std::move (firmware);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessIo
//
//  The SWIM in $0A00-$0A0F, the head select at $0A40-$0A41, the LED at
//  $0A80-$0A81. Every other address in the page reads $FF.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple35CardMemory::AccessIo (Word address, bool isWrite, Byte value)
{
    Byte  result = 0xFF;



    if (address <= kSwimEnd)
    {
        result = m_iwm.Access (address & 0x0F, isWrite, value, GetNowNs());
    }
    else if (address == kHeadLow || address == kHeadHigh)
    {
        for (auto & drive : m_drives)
        {
            drive->SetSel (address == kHeadHigh);
        }
    }
    else if (address == kLedOff || address == kLedOn)
    {
        m_led = address == kLedOn;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNowNs
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Apple35CardMemory::GetNowNs() const
{
    uint64_t  cycles = (m_cycles != nullptr) ? *m_cycles : 0;



    return cycles * s_kPicoNsPerCycle / s_kPicoPerNs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35CardMemory::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple35CardMemory::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.WriteUInt32 (static_cast<uint32_t> (m_ram.size()));
    writer.WriteBytes  (m_ram.data(), m_ram.size());

    m_iwm.SaveState (writer, GetNowNs());

    for (const std::unique_ptr<Sony35Drive> & drive : m_drives)
    {
        writer.WriteBool (drive != nullptr);

        if (drive != nullptr)
        {
            hr = drive->SaveState (writer);
            CHR (hr);
        }
    }

    writer.WriteBool (m_led);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35CardMemory::LoadState
//
//  A state saved with another set of drives on the card doesn't load.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple35CardMemory::LoadState (StateReader & reader)
{
    HRESULT   hr         = S_OK;
    bool      isSameSize = false;
    uint32_t  ramBytes   = 0;
    bool      hasDrive   = false;



    reader.ReadUInt32 (ramBytes);

    hr = reader.GetResult();
    CHR (hr);

    isSameSize = ramBytes == m_ram.size();
    CBREx (isSameSize, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    reader.ReadBytes (m_ram.data(), m_ram.size());

    hr = m_iwm.LoadState (reader, GetNowNs());
    CHR (hr);

    for (std::unique_ptr<Sony35Drive> & drive : m_drives)
    {
        reader.ReadBool (hasDrive);
        CBREx (hasDrive == (drive != nullptr), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        if (drive != nullptr)
        {
            hr = drive->LoadState (reader);
            CHR (hr);
        }
    }

    reader.ReadBool (m_led);

    hr = reader.GetResult();

Error:
    return hr;
}
