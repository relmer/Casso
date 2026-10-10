#include "Pch.h"

#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSoftSwitchBank
//
////////////////////////////////////////////////////////////////////////////////

AppleSoftSwitchBank::AppleSoftSwitchBank()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Read or write to $C050-$C057 toggles video mode flags.
//  Even addresses clear the flag, odd addresses set it.
//
//  $C050/$C051: Graphics/Text
//  $C052/$C053: Full/Mixed
//  $C054/$C055: Page1/Page2
//  $C056/$C057: LoRes/HiRes
//  $C058-$C05D: AN0-AN2 off/on
//
//  None of these reads drives the data bus, so a read returns the floating
//  bus: the byte the video scanner fetched on that cycle, before the switch
//  the read flips takes effect (Sather, Understanding the Apple IIe, chapter
//  5). With no floating-bus source wired, a read returns 0.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleSoftSwitchBank::Read (Word address)
{
    Byte  value = ReadFloatingBus();



    switch (address)
    {
        case 0xC050:
            m_graphicsMode = true;
            break;
        case 0xC051:
            m_graphicsMode = false;
            break;
        case 0xC052:
            m_mixedMode = false;
            break;
        case 0xC053:
            m_mixedMode = true;
            break;
        case 0xC054:
            m_page2 = false;
            break;
        case 0xC055:
            m_page2 = true;
            break;
        case 0xC056:
            m_hiresMode = false;
            break;
        case 0xC057:
            m_hiresMode = true;
            break;
        case 0xC058:
        case 0xC059:
        case 0xC05A:
        case 0xC05B:
        case 0xC05C:
        case 0xC05D:
            AccessAnnunciator (address);
            break;
        default:
            break;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessAnnunciator
//
//  $C058 + 2n turns annunciator n off and $C059 + 2n turns it on, on a read
//  or a write alike. The outputs are latches: nothing but another access to
//  the same pair, or power-on, changes them.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::AccessAnnunciator (Word address)
{
    int   offset = static_cast<int> (address - kwFirstAnnunciatorAddress);
    int   index  = offset / 2;
    Byte  mask   = static_cast<Byte> (1 << index);



    if (offset & 1)
    {
        m_annunciators.fetch_or (mask, memory_order_acq_rel);
    }
    else
    {
        m_annunciators.fetch_and (static_cast<Byte> (~mask), memory_order_acq_rel);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAnnunciatorOn
//
////////////////////////////////////////////////////////////////////////////////

bool AppleSoftSwitchBank::IsAnnunciatorOn (int index) const
{
    Byte  mask = static_cast<Byte> (1 << index);



    return (m_annunciators.load (memory_order_acquire) & mask) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    // Write has same effect as read for soft switches
    Read (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::Reset()
{
    m_graphicsMode = false;
    m_mixedMode    = false;
    m_page2        = false;
    m_hiresMode    = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4 / FR-034: //e MMU asserts /RESET clearing the display soft
//  switches. PowerCycle inherits the same body via the default forwarder
//  on MemoryDevice (audit §10).
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::SoftReset()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  The annunciators come up off at power-on. A Ctrl-Reset leaves them as
//  they were, so Reset and SoftReset do not touch them; a program that
//  wants them in a known state after a reset writes them itself.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::PowerCycle (Prng & prng)
{
    UNREFERENCED_PARAMETER (prng);

    m_annunciators.store (0, memory_order_release);

    SoftReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleSoftSwitchBank::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return make_unique<AppleSoftSwitchBank> ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSoftSwitchBank::GetDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

void AppleSoftSwitchBank::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    DiagnosticsGroup  display { "Display", {} };



    display.rows.push_back (MakeTextRow ("Mode",  GetModeName()));
    display.rows.push_back (MakeFlagRow ("TEXT",  !m_graphicsMode));
    display.rows.push_back (MakeFlagRow ("MIXED", m_mixedMode));
    display.rows.push_back (MakeFlagRow ("PAGE2", m_page2));
    display.rows.push_back (MakeFlagRow ("HIRES", m_hiresMode));

    snapshot.groups.push_back (std::move (display));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSoftSwitchBank::GetModeName
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleSoftSwitchBank::GetModeName() const
{
    std::string  mode;



    if (!m_graphicsMode)
    {
        return "text";
    }

    mode  = m_hiresMode ? "hi-res" : "lo-res";
    mode += m_mixedMode ? ", mixed" : "";

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleSoftSwitchBank::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBool (m_graphicsMode);
    writer.WriteBool (m_mixedMode);
    writer.WriteBool (m_page2);
    writer.WriteBool (m_hiresMode);
    writer.WriteByte (m_annunciators.load (memory_order_acquire));

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  Fails with ERROR_INVALID_DATA when the annunciator byte holds a bit past
//  AN2.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleSoftSwitchBank::LoadState (StateReader & reader)
{
    constexpr Byte  kValidAnnunciators = static_cast<Byte> ((1 << kAnnunciatorCount) - 1);
    HRESULT         hr                 = S_OK;
    uint16_t        version            = 0;
    Byte            annunciators       = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBool (m_graphicsMode);
    reader.ReadBool (m_mixedMode);
    reader.ReadBool (m_page2);
    reader.ReadBool (m_hiresMode);
    reader.ReadByte (annunciators);

    hr = reader.EndSection();
    CHR (hr);

    CBREx ((annunciators & ~kValidAnnunciators) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_annunciators.store (annunciators, memory_order_release);

Error:
    return hr;
}
