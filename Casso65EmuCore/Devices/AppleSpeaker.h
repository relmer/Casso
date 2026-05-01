#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSpeaker
//
//  Speaker toggle at $C030. Each read toggles the speaker state and records
//  the CPU cycle timestamp for audio waveform generation.
//
////////////////////////////////////////////////////////////////////////////////

class AppleSpeaker : public MemoryDevice
{
public:
    AppleSpeaker ();

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC030; }
    Word GetEnd   () const override { return 0xC03F; }
    void Reset    () override;

    // Audio interface
    float GetSpeakerState () const { return m_speakerState; }

    const std::vector<uint32_t> & GetToggleTimestamps () const { return m_toggleTimestamps; }

    void ClearTimestamps () { m_toggleTimestamps.clear (); }

    void SetCycleCounter (uint64_t * pCycles) { m_pTotalCycles = pCycles; }

    // Call at the start of each frame so timestamps are frame-relative
    void BeginFrame ()
    {
        m_frameInitialState = m_speakerState;

        if (m_pTotalCycles != nullptr)
        {
            m_frameCycleStart = *m_pTotalCycles;
        }
    }

    float GetFrameInitialState () const { return m_frameInitialState; }

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    float                       m_speakerState      = -0.25f;
    float                       m_frameInitialState = -0.25f;
    std::vector<uint32_t>       m_toggleTimestamps;
    uint64_t *                  m_pTotalCycles      = nullptr;
    uint64_t                    m_frameCycleStart   = 0;
};
