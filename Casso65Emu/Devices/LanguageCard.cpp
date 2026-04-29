#include "Pch.h"

#include "LanguageCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCard
//
////////////////////////////////////////////////////////////////////////////////

LanguageCard::LanguageCard (MemoryBus & bus)
    : m_bus         (bus),
      m_ramBank1    (0x1000, 0),
      m_ramBank2    (0x1000, 0),
      m_ramMain     (0x2000, 0),
      m_readRam     (false),
      m_writeRam    (false),
      m_preWrite    (false),
      m_bank2Select (true),
      m_lastSwitch  (0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Reading $C080-$C08F updates the bank switching state.
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCard::Read (Word address)
{
    Byte switchAddr = static_cast<Byte> (address & 0x0F);
    UpdateState (switchAddr);

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    Byte switchAddr = static_cast<Byte> (address & 0x0F);
    UpdateState (switchAddr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateState
//
//  Language Card soft switch decoding:
//  Bit 0: 0=read ROM, 1=read RAM
//  Bit 1: 0=bank 2, 1=bank 1  (inverted: bank2Select = !(bit1))
//  Bit 3 (combined with read): Write enable requires two consecutive reads
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::UpdateState (Byte switchAddr)
{
    // Bank selection: bit 3 determines bank
    m_bank2Select = (switchAddr & 0x08) == 0;

    // Read source: bit 0 controls whether we read RAM or ROM
    // Even addresses ($C080, $C082, $C084, ...) = read RAM
    // Odd addresses  ($C081, $C083, $C085, ...) = read ROM
    m_readRam = (switchAddr & 0x01) == 0;

    // Write enable: requires two consecutive reads of same odd switch
    bool writeEnableSwitch = (switchAddr & 0x01) != 0;

    if (writeEnableSwitch)
    {
        if (m_preWrite && m_lastSwitch == switchAddr)
        {
            m_writeRam = true;
        }
        else
        {
            m_preWrite = true;
        }
    }
    else
    {
        m_writeRam = false;
        m_preWrite = false;
    }

    m_lastSwitch = switchAddr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRam
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCard::ReadRam (Word address)
{
    if (address >= 0xD000 && address <= 0xDFFF)
    {
        if (m_bank2Select)
        {
            return m_ramBank2[address - 0xD000];
        }
        else
        {
            return m_ramBank1[address - 0xD000];
        }
    }

    if (address >= 0xE000 && address <= 0xFFFF)
    {
        return m_ramMain[address - 0xE000];
    }

    return 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRam
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::WriteRam (Word address, Byte value)
{
    if (!m_writeRam)
    {
        return;
    }

    if (address >= 0xD000 && address <= 0xDFFF)
    {
        if (m_bank2Select)
        {
            m_ramBank2[address - 0xD000] = value;
        }
        else
        {
            m_ramBank1[address - 0xD000] = value;
        }
    }
    else if (address >= 0xE000 && address <= 0xFFFF)
    {
        m_ramMain[address - 0xE000] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::Reset ()
{
    std::fill (m_ramBank1.begin (), m_ramBank1.end (), Byte (0));
    std::fill (m_ramBank2.begin (), m_ramBank2.end (), Byte (0));
    std::fill (m_ramMain.begin (), m_ramMain.end (), Byte (0));

    m_readRam     = false;
    m_writeRam    = false;
    m_preWrite    = false;
    m_bank2Select = true;
    m_lastSwitch  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> LanguageCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);

    return std::make_unique<LanguageCard> (bus);
}
