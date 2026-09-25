#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSoftSwitchBank
//
//  Video mode toggles at $C050-$C057 and annunciators AN0-AN2 at
//  $C058-$C05D. AN3 ($C05E/$C05F) is the //e's double hi-res switch and
//  belongs to Apple2eSoftSwitchBank.
//
////////////////////////////////////////////////////////////////////////////////

class AppleSoftSwitchBank : public MemoryDevice
{
public:
    AppleSoftSwitchBank ();

    Byte Read       (Word address) override;
    void Write      (Word address, Byte value) override;
    Word GetStart   () const override { return 0xC050; }
    Word GetEnd     () const override { return 0xC05F; }
    void Reset      () override;
    void SoftReset  () override;
    void PowerCycle (Prng & prng) override;

    // State accessors for video mode selection
    bool IsGraphicsMode () const { return m_graphicsMode; }
    bool IsMixedMode    () const { return m_mixedMode; }
    bool IsPage2        () const { return m_page2; }
    bool IsHiresMode    () const { return m_hiresMode; }

    // Annunciator output AN0-AN2 (index 0-2), as a program last set it.
    bool IsAnnunciatorOn (int index) const;

    static constexpr int  kAnnunciatorCount = 3;

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

protected:
    static constexpr Word  kwFirstAnnunciatorAddress = 0xC058;

    void AccessAnnunciator (Word address);

    bool          m_graphicsMode  = false;
    bool          m_mixedMode     = false;
    bool          m_page2         = false;
    bool          m_hiresMode     = false;
    atomic<Byte>  m_annunciators  { 0 };
};
