#include "Pch.h"

#include "Machines/Apple2/Common/VideoTiming.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Same effect as SoftReset — the timing model has no DRAM-shaped state.
//
////////////////////////////////////////////////////////////////////////////////

void VideoTiming::PowerCycle (Prng & prng)
{
    UNREFERENCED_PARAMETER (prng);

    SoftReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VideoTiming::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);
    writer.WriteUInt32  (m_cycleCounter);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VideoTiming::LoadState (StateReader & reader)
{
    HRESULT   hr           = S_OK;
    uint16_t  version      = 0;
    uint32_t  cycleCounter = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadUInt32 (cycleCounter);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (cycleCounter < kCyclesPerFrame, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_cycleCounter = cycleCounter;

Error:
    return hr;
}
