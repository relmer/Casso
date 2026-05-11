#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Devices/IMmu.h"
#include "Devices/CxxxRomRouter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeMmu
//
//  Apple //e Memory Management Unit. Owns RAMRD/RAMWRT/ALTZP/80STORE/
//  INTCXROM/SLOTC3ROM/INTC8ROM flag state and the 64 KiB auxiliary RAM
//  buffer. Single source of truth — replaces (and deletes) the legacy
//  AuxRamCard. On every banking-changed event, rebinds the MemoryBus
//  page table so $0000-$BFFF reads/writes go to main vs. aux RAM at
//  zero per-access cost.
//
//  Per audit §1.1, $C002-$C00B is the correct write-switch surface
//  (the old AuxRamCard wired the wrong addresses).
//
////////////////////////////////////////////////////////////////////////////////

class AppleIIeMmu : public IMmu
{
public:
    AppleIIeMmu ();

    HRESULT  Initialize (
        MemoryBus            *   bus,
        RamDevice            *   mainRam,
        RamDevice            *   auxRam,
        RomDevice            *   internalRom,
        LanguageCard         *   lc,
        AppleSoftSwitchBank  *   ssBank) override;

    void  SetRamRd       (bool v) override;
    void  SetRamWrt      (bool v) override;
    void  SetAltZp       (bool v) override;
    void  Set80Store     (bool v) override;
    void  SetIntCxRom    (bool v) override;
    void  SetSlotC3Rom   (bool v) override;
    void  ResetIntC8Rom  () override;

    bool  GetRamRd       () const override { return m_ramRd; }
    bool  GetRamWrt      () const override { return m_ramWrt; }
    bool  GetAltZp       () const override { return m_altZp; }
    bool  Get80Store     () const override { return m_store80; }
    bool  GetIntCxRom    () const override { return m_intCxRom; }
    bool  GetSlotC3Rom   () const override { return m_slotC3Rom; }
    bool  GetIntC8Rom    () const          { return m_intC8Rom; }

    void  OnSoftSwitchChanged () override;
    void  OnSoftReset         () override;
    void  OnPowerCycle        (Prng & prng) override;

    void  SetIntC8Rom    (bool v) { m_intC8Rom = v; }

    Byte *       GetAuxBuffer ()       { return m_auxRam.data (); }
    const Byte * GetAuxBuffer () const { return m_auxRam.data (); }

    // Cxxx-ROM routing (audit C8 carryover). Internal $C100-$CFFF ROM
    // bytes and slot ROM bytes are attached after Initialize; the MMU
    // owns the CxxxRomRouter device that is registered on the bus.
    void               AttachInternalCxxxRom (vector<Byte> data);
    void               AttachSlotRom         (int slot, vector<Byte> data);
    CxxxRomRouter *    GetCxxxRouter         () { return &m_cxxxRouter; }

private:
    void   RebindPageTable     ();
    void   ResolveZeroPage     ();
    void   ResolveMain02_BF    ();
    void   ResolveText04_07    ();
    void   ResolveHires20_3F   ();
    Byte * SelectMainRead      (int page);
    Byte * SelectMainWrite     (int page);

    MemoryBus            *   m_bus         = nullptr;
    Byte                 *   m_mainRamPtr  = nullptr;
    AppleSoftSwitchBank  *   m_ssBank      = nullptr;
    vector<Byte>             m_auxRam;
    CxxxRomRouter            m_cxxxRouter;

    bool                     m_ramRd       = false;
    bool                     m_ramWrt      = false;
    bool                     m_altZp       = false;
    bool                     m_store80     = false;
    bool                     m_intCxRom    = false;
    bool                     m_slotC3Rom   = false;
    bool                     m_intC8Rom    = false;
};
