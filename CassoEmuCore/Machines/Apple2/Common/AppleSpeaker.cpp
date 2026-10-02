#include "Pch.h"

#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSpeaker
//
////////////////////////////////////////////////////////////////////////////////

AppleSpeaker::AppleSpeaker()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleSpeaker::Read (Word address)
{
    UNREFERENCED_PARAMETER (address);

    // Toggle speaker state (use +/-1.0 to match audio pipeline expectations)
    m_speakerState = (m_speakerState > 0.0f) ? -0.25f : 0.25f;

    // Record frame-relative timestamp
    if (m_pTotalCycles != nullptr)
    {
        uint32_t relCycle = static_cast<uint32_t> (*m_pTotalCycles - m_frameCycleStart);
        m_toggleTimestamps.push_back (relCycle);
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleSpeaker::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (address);
    UNREFERENCED_PARAMETER (value);

    // Write also toggles (some software uses STA $C030)
    Read (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleSpeaker::Reset()
{
    m_speakerState = -0.25f;
    m_toggleTimestamps.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4: speaker has no DRAM-shaped state; soft and power resets do
//  the same thing — back to the resting position with no pending toggles.
//
////////////////////////////////////////////////////////////////////////////////

void AppleSpeaker::SoftReset()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleSpeaker::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return make_unique<AppleSpeaker> ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleSpeaker::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);
    writer.WriteBool    (m_speakerState > 0.0f);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleSpeaker::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;
    bool      isHigh  = false;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBool (isHigh);

    hr = reader.EndSection();
    CHR (hr);

    m_speakerState = isHigh ? 0.25f : -0.25f;

Error:
    return hr;
}
