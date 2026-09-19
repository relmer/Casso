#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Debugger/IDiagnosticsProvider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSoftSwitchBank
//
//  Video mode toggles at $C050-$C057 and annunciators $C058-$C05F.
//
////////////////////////////////////////////////////////////////////////////////

class AppleSoftSwitchBank : public MemoryDevice, public IDiagnosticsProvider
{
public:
    AppleSoftSwitchBank ();

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC050; }
    Word GetEnd   () const override { return 0xC05F; }
    void Reset    () override;
    void SoftReset () override;

    // State accessors for video mode selection
    bool IsGraphicsMode () const { return m_graphicsMode; }
    bool IsMixedMode    () const { return m_mixedMode; }
    bool IsPage2        () const { return m_page2; }
    bool IsHiresMode    () const { return m_hiresMode; }

    // The video panel: the display switches and the mode they select.
    std::string  GetDiagnosticsId    () const override { return "video"; }
    std::string  GetDiagnosticsTitle () const override { return "Video"; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

protected:
    virtual std::string  GetModeName () const;

    bool    m_graphicsMode = false;
    bool    m_mixedMode    = false;
    bool    m_page2        = false;
    bool    m_hiresMode    = false;
};
