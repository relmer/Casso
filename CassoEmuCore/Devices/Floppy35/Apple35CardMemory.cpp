#include "Pch.h"

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
