#pragma once

#include "Pch.h"
#include "IRomBankSwitch.h"

class LanguageCard;
class Apple2eMmu;




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cRomBank
//
//  Owns the Apple //c's 32K firmware ROM as two 16K banks, each a full
//  $C000-$FFFF image. A $C028 access (routed here through the soft-switch
//  bank's IRomBankSwitch hook) toggles which bank is visible across
//  $C100-$FFFF; reset restores bank 0 (the main monitor/Applesoft bank).
//
//  Rather than teach the LanguageCard and CxxxRomRouter to hold two banks,
//  this coordinator re-slices the active bank into them on each switch using
//  their existing single-image setters:
//
//    - $C100-$CFFF -> Apple2eMmu::AttachInternalCxxxRom (the CxxxRomRouter's
//      internal ROM)
//    - $D000-$FFFF -> LanguageCard::SetRomData (a ROM-only swap; LC RAM and
//      banking flags are untouched)
//
//  Bank switches are infrequent, so the per-switch copy is not a hot path.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cRomBank : public IRomBankSwitch
{
public:
    Apple2cRomBank (LanguageCard & lc, Apple2eMmu & mmu);

    // bank0 / bank1 are each a full 16K ($C000-$FFFF) image. Applies bank 0.
    void SetBankImages (vector<Byte> bank0, vector<Byte> bank1);

    void ToggleRomBank() override;
    void ResetRomBank  () override;

    int  CurrentBank   () const { return m_current; }

private:
    void ApplyBank     (int bank);

    // Bank image geometry: each 16K bank is a full $C000-$FFFF image;
    // ApplyBank re-slices $C100-$CFFF and $D000-$FFFF out of it.
    static constexpr Word    kBankImageStart = 0xC000;
    static constexpr size_t  kBankImageSize  = 0x4000;      // 16 KiB
    static constexpr Word    kCxxxStart      = 0xC100;
    static constexpr size_t  kCxxxSize       = 0x0F00;      // $C100-$CFFF
    static constexpr Word    kLcRomStart     = 0xD000;
    static constexpr size_t  kLcRomSize      = 0x3000;      // $D000-$FFFF
    static constexpr size_t  kCxxxOffset     = kCxxxStart  - kBankImageStart;   // $0100
    static constexpr size_t  kLcRomOffset    = kLcRomStart - kBankImageStart;   // $1000

    LanguageCard &  m_lc;
    Apple2eMmu   &  m_mmu;
    vector<Byte>    m_bank[2];
    int             m_current = 0;
};
