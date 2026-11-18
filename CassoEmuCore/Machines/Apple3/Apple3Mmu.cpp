#include "Pch.h"

#include "Apple3Mmu.h"
#include "Core/DramPowerOnPattern.h"
#include "Core/MemoryBus.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Prng.h"





static constexpr int     s_kPageShift             = 8;
static constexpr Word    s_kPageMask              = 0xFF00;
static constexpr Word    s_kOffsetMask            = 0x00FF;
static constexpr Byte    s_kFirstBankPage         = 0x20;
static constexpr Byte    s_kFirstHighSystem       = 0xA0;
static constexpr Word    s_kFirstBankAddress      = 0x2000;
static constexpr Word    s_kFirstHighAddress      = 0xA000;
static constexpr Word    s_kHighSystemOffset      = 0x8000;
static constexpr Byte    s_kStackPage             = 0x01;
static constexpr Byte    s_kStackPageXor          = 0x01;
static constexpr Byte    s_kFirstIoPage           = 0xC0;
static constexpr Byte    s_kLastIoPage            = 0xCF;
static constexpr Byte    s_kFirstRamOnlyPage      = 0xC5;
static constexpr Byte    s_kLastRamOnlyPage       = 0xC7;
static constexpr Byte    s_kFirstRomPage          = 0xF0;
static constexpr Byte    s_kTopPage               = 0xFF;
static constexpr Word    s_kRomBase               = 0xF000;
static constexpr Word    s_kFirstRamUnderRom      = 0xFFC0;
static constexpr Word    s_kLastRamUnderRom       = 0xFFCF;
static constexpr Byte    s_kFirstExtendedZp       = 0x18;
static constexpr Byte    s_kLastExtendedZp        = 0x1F;
static constexpr Byte    s_kExtensionPageXor      = 0x0C;
static constexpr Byte    s_kExtensionEnable       = 0x80;
static constexpr Byte    s_kExtensionBits         = 0x8F;
static constexpr Byte    s_kExtensionBankBits     = 0x0F;
static constexpr Byte    s_kSystemArrangement     = 0x8F;
static constexpr Byte    s_kBankBits              = 0x0F;
static constexpr Word    s_kFirstExtendedAddress  = 0x0100;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Mmu
//
////////////////////////////////////////////////////////////////////////////////

Apple3Mmu::Apple3Mmu()
{
    m_absentRead.fill (0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Apple3Mmu
//
////////////////////////////////////////////////////////////////////////////////

Apple3Mmu::~Apple3Mmu() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Allocates the RAM, takes a copy of the ROM, registers the RAM with the bus
//  for history's written-page tracking and publishes the power-on map: every
//  register zero, so the alternate stack, no ROM and no I/O, until the VIAs
//  set the registers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Mmu::Initialize (MemoryBus * bus, size_t ramBytes, const std::vector<Byte> & rom)
{
    HRESULT  hr          = S_OK;
    bool     isKnownSize = ramBytes == kRam128K || ramBytes == kRam256K;
    size_t   romBytes    = rom.size();



    CBRAEx (bus != nullptr,          E_INVALIDARG);
    CBRAEx (isKnownSize,             E_INVALIDARG);
    CBRAEx (romBytes == kRomBytes,   E_INVALIDARG);

    m_bus           = bus;
    m_ram           = std::vector<Byte> (ramBytes, 0);
    m_rom           = rom;
    m_physicalBanks = ramBytes / kBankBytes;

    m_ramPages.Attach (m_bus, m_ram.data(), m_ram.size());
    m_ramPages.MarkAllWritten();

    Remap();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetEnvironment
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::SetEnvironment (Byte value)
{
    if (value != m_environment)
    {
        m_environment = value;
        Remap();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetZeroPage
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::SetZeroPage (Byte value)
{
    if (value != m_zeroPage)
    {
        m_zeroPage = value;
        Remap();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetBank
//
//  The bank register's low four bits. Bit 3 is not connected on Apple's
//  board; the bank wrapping produces the same bank either way.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::SetBank (Byte value)
{
    Byte  bank = (Byte) (value & s_kBankBits);



    if (bank != m_bank)
    {
        m_bank = bank;
        Remap();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetLockout
//
//  The Apple II lockout removes the VIAs from $FFD0-$FFEF, so a write there
//  can't change the three registers, and a read returns the RAM under them.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::SetLockout (bool isLockedOut)
{
    m_isLockedOut = isLockedOut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekPhysical
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Mmu::PeekPhysical (size_t bank, Word offset) const
{
    size_t  index = (bank * kBankBytes) + (offset % kBankBytes);



    return (index < m_ram.size()) ? m_ram[index] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekSystem
//
//  The system bank's byte at a system address in $0000-$1FFF or
//  $A000-$FFFF; the RAM, never the ROM or a device.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Mmu::PeekSystem (Word address) const
{
    Word  offset = (address < s_kFirstHighAddress) ? address : (Word) (address - s_kHighSystemOffset);



    return PeekPhysical (m_physicalBanks - 1, offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBankPresent
//
//  Banks 0 to 2 on 128K and 0 to 6 on 256K: the physical banks but the last,
//  which is the system bank. A bank past them has no RAM: it reads $FF, and
//  a write to it is discarded. Apple documents the result as undefined; the
//  Confidence Program's memory map reports 128K on a 128K machine only
//  when the absent banks do not answer as RAM.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Mmu::IsBankPresent (Byte bank) const
{
    return (size_t) bank + 1 < m_physicalBanks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSystemBankCell
//
////////////////////////////////////////////////////////////////////////////////

Byte * Apple3Mmu::GetSystemBankCell (Word address)
{
    size_t  base   = (m_physicalBanks - 1) * kBankBytes;
    Word    offset = (address < s_kFirstHighAddress) ? address : (Word) (address - s_kHighSystemOffset);



    return &m_ram[base + offset];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBankCell
//
//  A cell of a switched bank by bank number and offset, or null for an
//  absent bank.
//
////////////////////////////////////////////////////////////////////////////////

Byte * Apple3Mmu::GetBankCell (Byte bank, Word offset)
{
    return IsBankPresent (bank) ? &m_ram[(size_t) bank * kBankBytes + offset] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetExtendedCell
//
//  A cell of an extended access: the bank pair's RAM is consecutive, so an
//  address of $8000 or above is in bank n + 1, and past the last bank the
//  system bank. Null past the end of RAM.
//
////////////////////////////////////////////////////////////////////////////////

Byte * Apple3Mmu::GetExtendedCell (Byte bank, Word offset)
{
    size_t  index = (size_t) bank * kBankBytes + offset;



    return (index < m_ram.size()) ? &m_ram[index] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSystemPage
//
//  A page of the system address space with the current bank switched in.
//
////////////////////////////////////////////////////////////////////////////////

Byte * Apple3Mmu::GetSystemPage (Byte page)
{
    Word  address = (Word) (page << s_kPageShift);



    if (page < s_kFirstBankPage || page >= s_kFirstHighSystem)
    {
        return GetSystemBankCell (address);
    }

    return GetBankCell (m_bank, (Word) (address - s_kFirstBankAddress));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReadCells / GetWriteCells
//
//  A page of the system address space, or for an absent bank the page that
//  reads $FF and the page whose writes are discarded.
//
////////////////////////////////////////////////////////////////////////////////

Byte * Apple3Mmu::GetReadCells (Byte page)
{
    Byte  * cells = GetSystemPage (page);



    return (cells != nullptr) ? cells : m_absentRead.data();
}


Byte * Apple3Mmu::GetWriteCells (Byte page)
{
    Byte  * cells = GetSystemPage (page);



    return (cells != nullptr) ? cells : m_absentWrite.data();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetExtendedPage
//
//  The page of an extended access at an address, from the extension
//  byte's bank bits. $8F is the system bank with bank 0 switched in, the I/O
//  space and the VIAs replaced by the RAM under them.
//
////////////////////////////////////////////////////////////////////////////////

Byte * Apple3Mmu::GetExtendedPage (Byte extension, Word address)
{
    Word  page = (Word) (address & s_kPageMask);



    if (extension != s_kSystemArrangement)
    {
        return GetExtendedCell ((Byte) (extension & s_kExtensionBankBits), page);
    }

    if (page >= s_kFirstBankAddress && page < s_kFirstHighAddress)
    {
        return GetBankCell (0, (Word) (page - s_kFirstBankAddress));
    }

    return GetSystemBankCell (page);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsIoPage
//
//  Whether a page in $C000-$CFFF is in the I/O space: every one while I/O
//  is on, except $C500-$C7FF, which is always RAM.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Mmu::IsIoPage (int page) const
{
    bool  isIoRange  = page >= s_kFirstIoPage && page <= s_kLastIoPage;
    bool  isRamOnly  = page >= s_kFirstRamOnlyPage && page <= s_kLastRamOnlyPage;



    return isIoRange && !isRamOnly && (m_environment & kEnvIoEnable) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsViaAddress
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Mmu::IsViaAddress (Word address) const
{
    return !m_isLockedOut && address >= kViaFirst && address <= kViaLast;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFloatingBus
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Mmu::ReadFloatingBus() const
{
    return (m_bus != nullptr) ? m_bus->GetFloatingBusValue() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Remap
//
//  Rebuilds both maps from the registers and publishes every page. An
//  extended access cannot be in progress: the registers change only through
//  the VIAs, and an extended access to their addresses accesses RAM.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::Remap()
{
    if (m_ram.empty())
    {
        return;
    }

    BuildReadMap();
    BuildWriteMap();

    for (int page = 0; page < kPageCount; page++)
    {
        PublishPage (page);
    }

    m_bus->NotifyBankingChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildReadMap
//
//  Zero page from the zero page register; the stack from page $01 or, with
//  the alternate stack, the page the zero page register XOR $01 selects;
//  the I/O pages and page $FF left null so their reads come to Read; the ROM
//  at $F000-$FEFF when it is selected.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::BuildReadMap()
{
    bool  isPrimaryStack = (m_environment & kEnvPrimaryStack) != 0;
    bool  isRomSelected  = (m_environment & kEnvRomSelect) != 0;
    Byte  stackPage      = isPrimaryStack ? s_kStackPage : (Byte) (m_zeroPage ^ s_kStackPageXor);



    for (int page = 0; page < kPageCount; page++)
    {
        m_readMap[(size_t) page] = GetReadCells ((Byte) page);

        if (IsIoPage (page) || page == s_kTopPage)
        {
            m_readMap[(size_t) page] = nullptr;
        }
        else if (isRomSelected && page >= s_kFirstRomPage)
        {
            m_readMap[(size_t) page] = &m_rom[(size_t) ((page << s_kPageShift) - s_kRomBase)];
        }
    }

    m_readMap[0]            = GetReadCells (m_zeroPage);
    m_readMap[s_kStackPage] = GetReadCells (stackPage);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildWriteMap
//
//  The RAM each page writes, the ROM's pages included: a write under the ROM
//  goes to the RAM beneath it. With write protection on, $C000-$FFFF has no
//  RAM to write.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::BuildWriteMap()
{
    bool  isProtected    = (m_environment & kEnvWriteProtect) != 0;
    bool  isPrimaryStack = (m_environment & kEnvPrimaryStack) != 0;



    for (int page = 0; page < kPageCount; page++)
    {
        bool  isBlocked = isProtected && page >= s_kFirstIoPage;



        m_writeMap[(size_t) page] = isBlocked ? nullptr : GetWriteCells ((Byte) page);
    }

    m_writeMap[0]            = GetWriteCells (m_zeroPage);
    m_writeMap[s_kStackPage] = GetWriteCells (isPrimaryStack ? s_kStackPage : (Byte) (m_zeroPage ^ s_kStackPageXor));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishPage
//
//  Puts a page's cells in the bus's tables. The bus sends every write at
//  $C000 and above here whatever the table contains, so for those pages the
//  table serves only reads.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::PublishPage (int page)
{
    m_bus->SetReadPage  (page, m_readMap[(size_t) page]);
    m_bus->SetWritePage (page, m_writeMap[(size_t) page]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  A read the page table does not serve: the I/O space, and page $FF, where
//  the VIAs are at $FFD0-$FFEF. $FFC0-$FFCF is the RAM under the ROM even
//  while the ROM is selected, as MAME's Apple /// driver has it.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Mmu::Read (Word address)
{
    int   page          = address >> s_kPageShift;
    bool  isRomSelected = (m_environment & kEnvRomSelect) != 0;
    bool  isRamUnderRom = address >= s_kFirstRamUnderRom && address <= s_kLastRamUnderRom;



    if (IsIoPage (page))
    {
        return (m_io != nullptr) ? m_io->Read (address) : ReadFloatingBus();
    }

    if (page != s_kTopPage)
    {
        return m_readMap[(size_t) page][address & s_kOffsetMask];
    }

    if (IsViaAddress (address) && m_vias != nullptr)
    {
        return m_vias->Read (address);
    }

    if (isRomSelected && !isRamUnderRom)
    {
        return m_rom[(size_t) (address - s_kRomBase)];
    }

    return *GetSystemBankCell (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Every write at $C000 and above. During an extended access to one of these
//  pages the write goes to the extended page. Otherwise the I/O space and the
//  VIAs take theirs, and the rest go to RAM unless it is write-protected.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::Write (Word address, Byte value)
{
    int  page = address >> s_kPageShift;



    if (page == m_extendedPage)
    {
        m_extendedCells[address & s_kOffsetMask] = value;
        m_bus->MarkPointerWritten (&m_extendedCells[address & s_kOffsetMask]);
        return;
    }

    if (IsIoPage (page))
    {
        if (m_io != nullptr)
        {
            m_io->Write (address, value);
        }

        return;
    }

    if (IsViaAddress (address) && m_vias != nullptr)
    {
        m_vias->Write (address, value);
        return;
    }

    WriteRam (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRam
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::WriteRam (Word address, Byte value)
{
    Byte  * page = m_writeMap[(size_t) (address >> s_kPageShift)];



    if (page != nullptr)
    {
        page[address & s_kOffsetMask] = value;
        m_bus->MarkPointerWritten (&page[address & s_kOffsetMask]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
//  The RAM and ROM this device serves read without side effects; the I/O
//  space and the VIAs answer for themselves.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Mmu::TryPeekIo (Word address, Byte & value)
{
    int   page          = address >> s_kPageShift;
    bool  isRomSelected = (m_environment & kEnvRomSelect) != 0;
    bool  isRamUnderRom = address >= s_kFirstRamUnderRom && address <= s_kLastRamUnderRom;



    if (IsIoPage (page))
    {
        return (m_io != nullptr) ? m_io->TryPeekIo (address, value) : false;
    }

    if (IsViaAddress (address))
    {
        return (m_vias != nullptr) ? m_vias->TryPeekIo (address, value) : false;
    }

    if (page != s_kTopPage)
    {
        value = m_readMap[(size_t) page][address & s_kOffsetMask];
    }
    else if (isRomSelected && !isRamUnderRom)
    {
        value = m_rom[(size_t) (address - s_kRomBase)];
    }
    else
    {
        value = *GetSystemBankCell (address);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryBeginExtendedAccess
//
//  Enhanced indirect addressing. It is on only while the zero page register
//  is $18-$1F. The extension byte is in the system bank at the pointer's high
//  byte with the zero page XOR $0C as its page; with bit 7 set, its low bits
//  select the bank pair of the access. An access to $0000-$00FF goes to
//  the current zero page, as an ordinary one does. The page the access
//  touches is mapped from the extension byte's bank until the instruction is
//  done.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Mmu::TryBeginExtendedAccess (Byte pointerHigh, Word effectiveAddress)
{
    Word    extensionAddress = (Word) (((m_zeroPage ^ s_kExtensionPageXor) << s_kPageShift) | pointerHigh);
    Byte    extension        = 0;
    int     page             = effectiveAddress >> s_kPageShift;



    if (m_zeroPage < s_kFirstExtendedZp || m_zeroPage > s_kLastExtendedZp)
    {
        return false;
    }

    extension = (Byte) (*GetSystemBankCell (extensionAddress) & s_kExtensionBits);

    if ((extension & s_kExtensionEnable) == 0 || effectiveAddress < s_kFirstExtendedAddress)
    {
        return false;
    }

    m_extendedPage  = page;
    m_extendedCells = GetExtendedPage (extension, effectiveAddress);

    m_bus->SetReadPage  (page, (m_extendedCells != nullptr) ? m_extendedCells : m_absentRead.data());
    m_bus->SetWritePage (page, (m_extendedCells != nullptr) ? m_extendedCells : m_absentWrite.data());

    m_extendedCells = (m_extendedCells != nullptr) ? m_extendedCells : m_absentWrite.data();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndExtendedAccess
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::EndExtendedAccess()
{
    int  page = m_extendedPage;



    if (page >= 0)
    {
        m_extendedPage  = -1;
        m_extendedCells = nullptr;
        PublishPage (page);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Fills the RAM with the DRAM power-on pattern from the shared Prng.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Mmu::PowerCycle (Prng & prng)
{
    DramPowerOnPattern::Fill (m_ram.data(), m_ram.size(), prng);
    m_ramPages.MarkAllWritten();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Mmu::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte (m_environment);
    writer.WriteByte (m_zeroPage);
    writer.WriteByte (m_bank);
    writer.WriteBool (m_isLockedOut);

    m_ramPages.Save (writer, m_ram.data(), m_ram.size());

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Mmu::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte  (m_environment);
    reader.ReadByte  (m_zeroPage);
    reader.ReadByte  (m_bank);
    reader.ReadBool  (m_isLockedOut);
    reader.ReadBytes (m_ram.data(), m_ram.size());

    m_ramPages.MarkAllWritten();
    m_extendedPage  = -1;
    m_extendedCells = nullptr;

    hr = reader.EndSection();
    CHR (hr);

    Remap();

Error:
    return hr;
}
