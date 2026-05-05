#include "Pch.h"

#include "LanguageCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCard
//
////////////////////////////////////////////////////////////////////////////////

LanguageCard::LanguageCard (MemoryBus & bus)
    : m_bus      (bus),
      m_ramBank1 (0x1000, 0),
      m_ramBank2 (0x1000, 0),
      m_ramMain  (0x2000, 0)
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
    Byte addrBits = switchAddr & 0x03;

    // Bank selection: bit 3 determines bank
    m_bank2Select = (switchAddr & 0x08) == 0;

    // Read source: determined by bits 0 and 1 together
    //   00 ($C080/$C088): Read RAM
    //   01 ($C081/$C089): Read ROM
    //   10 ($C082/$C08A): Read ROM
    //   11 ($C083/$C08B): Read RAM
    m_readRam = (addrBits == 0x00 || addrBits == 0x03);

    // Write enable: requires two consecutive reads of an odd-addressed switch
    bool writeEnableSwitch = (addrBits == 0x01 || addrBits == 0x03);

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
    fill (m_ramBank1.begin (), m_ramBank1.end (), Byte (0));
    fill (m_ramBank2.begin (), m_ramBank2.end (), Byte (0));
    fill (m_ramMain.begin (), m_ramMain.end (), Byte (0));

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

unique_ptr<MemoryDevice> LanguageCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);

    return make_unique<LanguageCard> (bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRom
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCard::ReadRom (Word address) const
{
    if (m_romData.empty () || address < 0xD000)
    {
        return 0xFF;
    }

    size_t offset = address - 0xD000;

    if (offset < m_romData.size ())
    {
        return m_romData[offset];
    }

    return 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank
//
////////////////////////////////////////////////////////////////////////////////

LanguageCardBank::LanguageCardBank (LanguageCard & lc)
    : m_lc (lc)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank::Read
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCardBank::Read (Word address)
{
    if (m_lc.IsReadRam ())
    {
        return m_lc.ReadRam (address);
    }

    return m_lc.ReadRom (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank::Write
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCardBank::Write (Word address, Byte value)
{
    m_lc.WriteRam (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank::Reset
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCardBank::Reset ()
{
    // State is managed by LanguageCard
}
