#pragma once

#include "Pch.h"

class MemoryBus;
class RamDevice;
class RomDevice;
class LanguageCard;
class AppleSoftSwitchBank;
class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  IBankingObserver
//
//  Subscribes to display-related soft-switch changes (PAGE2 / HIRES / TEXT /
//  MIXED) so that 80STORE-routed regions can be re-resolved without per-access
//  cost. The MMU registers itself as the observer.
//
////////////////////////////////////////////////////////////////////////////////

class IBankingObserver
{
public:
    virtual              ~IBankingObserver () = default;

    virtual void          OnSoftSwitchChanged () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IMmu
//
//  Apple //e MMU contract. Owns RAMRD/RAMWRT/ALTZP/80STORE/INTCXROM/
//  SLOTC3ROM/INTC8ROM state and rebinds the MemoryBus page-table on every
//  banking-changed event. Single source of truth for those flags.
//
//  Status reads ($C013-$C018) source bit 7 from the corresponding getter
//  on the implementation; bits 0-6 come from the floating-bus stand-in
//  in the soft-switch bank.
//
//  $CFFF-read-driven INTC8ROM auto-disable is modeled by ResetIntC8Rom
//  invoked from the ROM mapper read path.
//
////////////////////////////////////////////////////////////////////////////////

class IMmu : public IBankingObserver
{
public:
    virtual              ~IMmu () = default;

    virtual HRESULT       Initialize (
        MemoryBus            *   bus,
        RamDevice            *   mainRam,
        RamDevice            *   auxRam,
        RomDevice            *   internalRom,
        LanguageCard         *   lc,
        AppleSoftSwitchBank  *   ssBank) = 0;

    virtual void          SetRamRd       (bool v) = 0;
    virtual void          SetRamWrt      (bool v) = 0;
    virtual void          SetAltZp       (bool v) = 0;
    virtual void          Set80Store     (bool v) = 0;
    virtual void          SetIntCxRom    (bool v) = 0;
    virtual void          SetSlotC3Rom   (bool v) = 0;
    virtual void          ResetIntC8Rom  () = 0;

    virtual bool          GetRamRd       () const = 0;
    virtual bool          GetRamWrt      () const = 0;
    virtual bool          GetAltZp       () const = 0;
    virtual bool          Get80Store     () const = 0;
    virtual bool          GetIntCxRom    () const = 0;
    virtual bool          GetSlotC3Rom   () const = 0;

    virtual void          OnSoftReset    () = 0;
    virtual void          OnPowerCycle   (Prng & prng) = 0;
};
