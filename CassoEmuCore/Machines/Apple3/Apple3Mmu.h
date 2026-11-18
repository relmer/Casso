#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Core/RamPages.h"
#include "IExtendedAddressing.h"

class MemoryBus;
class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Mmu
//
//  The Apple ///'s memory map: the system bank and the switched 32K banks,
//  the environment, bank and zero page registers' effect on the address
//  space, the 4K ROM, the I/O space, write protection, the moved zero page
//  and stack, and the enhanced indirect addressing.
//
//  RAM is stored as physical 32K banks with the system bank last, so 128K is
//  banks 0 to 2 and the system bank, and 256K is banks 0 to 6 and the
//  system bank. The system bank's first 8K appears at $0000-$1FFF and the
//  rest at $A000-$FFFF.
//
//  Every page the CPU can access directly is published in the bus's page
//  table. The I/O pages, while I/O is on, and page $FF, which has the VIAs
//  at $FFD0-$FFEF, are left null so their accesses come here. The bus sends
//  every write at $C000 and above here as well.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Mmu : public MemoryDevice, public IExtendedAddressing, public IMachineState
{
public:
    static constexpr Byte    kEnvRomSelect     = 0x01;
    static constexpr Byte    kEnvRomSelect2    = 0x02;
    static constexpr Byte    kEnvPrimaryStack  = 0x04;
    static constexpr Byte    kEnvWriteProtect  = 0x08;
    static constexpr Byte    kEnvResetEnable   = 0x10;
    static constexpr Byte    kEnvScreen        = 0x20;
    static constexpr Byte    kEnvIoEnable      = 0x40;
    static constexpr Byte    kEnvSlowClock     = 0x80;

    static constexpr size_t  kBankBytes        = 0x8000;
    static constexpr size_t  kRomBytes         = 0x1000;
    static constexpr size_t  kRam128K          = 0x20000;
    static constexpr size_t  kRam256K          = 0x40000;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'M', 'M');
    static constexpr uint16_t  kStateVersion = 1;

    Apple3Mmu  ();
    ~Apple3Mmu () override;

    HRESULT  Initialize  (MemoryBus * bus, size_t ramBytes, const std::vector<Byte> & rom);

    void     AttachIo    (MemoryDevice * io)   { m_io   = io;   }
    void     AttachVias  (MemoryDevice * vias) { m_vias = vias; }

    void     SetEnvironment (Byte value);
    void     SetZeroPage    (Byte value);
    void     SetBank        (Byte value);
    void     SetLockout     (bool isLockedOut);

    Byte     GetEnvironment () const { return m_environment; }
    Byte     GetZeroPage    () const { return m_zeroPage;    }
    Byte     GetBank        () const { return m_bank;        }
    bool     IsLockedOut    () const { return m_isLockedOut; }
    size_t   GetRamSize     () const { return m_ram.size();  }
    size_t   GetBankCount   () const { return m_physicalBanks - 1; }

    //  The byte at an offset of a physical 32K bank, and the system bank's
    //  byte at a system address, with no side effects; for tests and the
    //  debugger.
    Byte     PeekPhysical   (size_t bank, Word offset) const;
    Byte     PeekSystem     (Word address) const;

    // MemoryDevice: $C000-$FFFF.
    Byte     Read       (Word address) override;
    void     Write      (Word address, Byte value) override;
    Word     GetStart   () const override { return kFirstDeviceAddress; }
    Word     GetEnd     () const override { return kLastDeviceAddress;  }
    void     Reset      () override {}
    void     PowerCycle (Prng & prng) override;
    bool     TryPeekIo  (Word address, Byte & value) override;

    // IExtendedAddressing
    bool     TryBeginExtendedAccess (Byte pointerHigh, Word effectiveAddress) override;
    void     EndExtendedAccess      () override;

    // IMachineState: the RAM and the register values the map was built from.
    HRESULT  SaveState  (StateWriter & writer) const override;
    HRESULT  LoadState  (StateReader & reader) override;

private:
    static constexpr Word    kFirstDeviceAddress = 0xC000;
    static constexpr Word    kLastDeviceAddress  = 0xFFFF;
    static constexpr Word    kViaFirst           = 0xFFD0;
    static constexpr Word    kViaLast            = 0xFFEF;
    static constexpr int     kPageCount          = 0x100;
    static constexpr size_t  kPageBytes          = 0x100;

    Byte   * GetSystemBankCell   (Word address);
    Byte   * GetBankCell         (Byte bank, Word offset);
    Byte   * GetExtendedCell     (Byte bank, Word offset);
    Byte   * GetReadCells        (Byte page);
    Byte   * GetWriteCells       (Byte page);
    Byte   * GetSystemPage       (Byte page);
    Byte   * GetExtendedPage     (Byte extension, Word address);
    bool     IsBankPresent       (Byte bank) const;
    bool     IsIoPage            (int page) const;
    bool     IsViaAddress        (Word address) const;
    Byte     ReadFloatingBus     () const;

    void     Remap               ();
    void     BuildReadMap        ();
    void     BuildWriteMap       ();
    void     PublishPage         (int page);
    void     WriteRam            (Word address, Byte value);

    MemoryBus        * m_bus           = nullptr;
    MemoryDevice     * m_io            = nullptr;
    MemoryDevice     * m_vias          = nullptr;
    std::vector<Byte>  m_ram;
    std::vector<Byte>  m_rom;
    RamPages           m_ramPages;
    size_t             m_physicalBanks = 0;

    Byte               m_environment   = 0;
    Byte               m_zeroPage      = 0;
    Byte               m_bank          = 0;
    bool               m_isLockedOut   = false;

    std::array<Byte *, kPageCount>  m_readMap  = {};
    std::array<Byte *, kPageCount>  m_writeMap = {};

    int                m_extendedPage  = -1;
    Byte             * m_extendedCells = nullptr;

    //  The pages an absent bank reads and writes.
    std::array<Byte, kPageBytes>  m_absentRead  = {};
    std::array<Byte, kPageBytes>  m_absentWrite = {};
};
