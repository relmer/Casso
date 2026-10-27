#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Common/IExpansionRomCard.h"
#include "Machines/Apple2/Common/SlotRomPage.h"
#include "Machines/Apple2/Common/SscSwitches.h"

class MemoryBus;

struct DeviceConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  SuperSerialCard
//
//  Apple's Super Serial Card in slot s, from Apple's SSC Installation and
//  Operating Manual (1981). As a memory device it decodes the slot's 16-byte
//  I/O window at $C080 + s*16:
//
//      +1      DIPSW1: SW1-1 to SW1-6           (SscSwitches)
//      +2      DIPSW2: SW2-1 to SW2-5 and CTS   (SscSwitches)
//      +8..+B  the 6551                          (Acia6551)
//
//  Every other offset reads the floating bus.
//
//  The 2 KB firmware maps its last page ($700-$7FF) at $Cs00-$CsFF and its
//  first $700 bytes at $C800-$CEFF while the expansion-ROM latch is set. An
//  access to $Cs00-$CsFF sets the latch, and $CF00-$CFFF and RESET reset it
//  (IExpansionRomCard; SlotRomPage and ExpansionRomBus do the routing).
//
//  With no firmware loaded, the ROM space reads the floating bus, as an empty
//  ROM socket does, and the 6551, switches and interrupt work as with it.
//
////////////////////////////////////////////////////////////////////////////////

class SuperSerialCard : public MemoryDevice, public IExpansionRomCard, public IMachineState, public IDiagnosticsProvider
{
public:
    static constexpr size_t  kFirmwareSize       = 0x800;
    static constexpr Word    kFirmwarePageOffset = 0x700;
    static constexpr Word    kExpansionRomStart  = 0xC800;
    static constexpr Word    kSlotIoBase         = 0xC080;
    static constexpr Word    kSlotIoStride       = 0x10;
    static constexpr Word    kSlotIoSize         = 0x10;
    static constexpr Word    kDipSwitch1Offset   = 0x1;
    static constexpr Word    kDipSwitch2Offset   = 0x2;
    static constexpr Word    kAciaOffset         = 0x8;

    SuperSerialCard (int slot, MemoryBus & bus);

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // MemoryDevice: the I/O window.
    Byte    Read       (Word address) override;
    void    Write      (Word address, Byte value) override;
    Word    GetStart   () const override { return m_ioStart; }
    Word    GetEnd     () const override { return m_ioEnd; }
    void    Reset      () override;
    void    SoftReset  () override;
    void    PowerCycle (Prng & prng) override;

    // IExpansionRomCard
    Byte    OnSlotPageAccess       (Word address, bool isWrite, Byte value) override;
    bool    IsExpansionRomSelected () const override { return m_expansionSelected; }
    Byte    ReadExpansionRom       (Word address) override;
    void    DeselectExpansionRom   () override { m_expansionSelected = false; }
    int     GetSlot                () const override { return m_slot; }

    // Firmware of exactly kFirmwareSize bytes, or empty for none. Any other
    // size fails, and the card then has no firmware.
    HRESULT SetFirmware (vector<Byte> firmware);
    bool    HasFirmware () const { return !m_firmware.empty(); }

    // Takes effect at once: the switch registers read the new positions and
    // SW2-6 connects or disconnects the IRQ.
    void                 SetSwitches (const SscSwitches & switches);
    const SscSwitches &  GetSwitches () const { return m_switches; }

    Acia6551     &  GetAcia     () { return m_acia; }
    SlotRomPage  &  GetSlotPage () { return m_slotPage; }

    // IMachineState: the expansion-ROM latch, then the 6551's section.
    // The firmware and the switches come from the slot's config, and the
    // attachment is wiring.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('S', 'S', 'C', 'D');
    static constexpr uint16_t  kStateVersion = 1;

    // IDiagnosticsProvider: the slot, the switches, the expansion-ROM latch
    // and the firmware, then the 6551.
    std::string  GetDiagnosticsId    () const override { return "serial-card"; }
    std::string  GetDiagnosticsTitle () const override { return "Super Serial Card"; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;

    // Why the serial connector has nothing attached when its config specifies an
    // attachment (a file that could not be opened); empty when it does.
    void            SetAttachmentProblem (const string & text) { m_attachmentProblem = text; }
    const string &  GetAttachmentProblem () const { return m_attachmentProblem; }

private:
    int             m_slot              = 0;
    Word            m_ioStart           = 0;
    Word            m_ioEnd             = 0;
    MemoryBus    &  m_bus;
    Acia6551        m_acia;
    SlotRomPage     m_slotPage;
    SscSwitches     m_switches          = SscSwitches::MakeDefault();
    vector<Byte>    m_firmware;
    bool            m_expansionSelected = false;
    string          m_attachmentProblem;
};
