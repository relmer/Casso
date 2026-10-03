#pragma once

#include "Pch.h"
#include "Core/IMachineState.h"
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

class AppleSpeaker : public MemoryDevice, public IMachineState
{
public:
    AppleSpeaker ();

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC030; }
    Word GetEnd   () const override { return 0xC03F; }
    void Reset    () override;
    void SoftReset () override;

    // Audio interface
    float GetSpeakerState () const { return m_speakerState; }

    const vector<uint32_t> & GetToggleTimestamps () const { return m_toggleTimestamps; }

    void ClearTimestamps () { m_toggleTimestamps.clear (); }

    void SetCycleCounter (uint64_t * pCycles) { m_pTotalCycles = pCycles; }

    // While muted the cone still moves, but no toggle is recorded for the
    // audio pipeline to play. Reverse execution mutes it during a replay.
    void SetMuted (bool isMuted) { m_isMuted = isMuted; }
    bool IsMuted  () const       { return m_isMuted; }

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

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // IMachineState: which way the cone is pushed. The toggle timestamps
    // and the frame's starting level are audio output, not machine state.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('S', 'P', 'K', 'R');
    static constexpr uint16_t  kStateVersion = 1;

private:
    float               m_speakerState      = -0.25f;
    float               m_frameInitialState = -0.25f;
    vector<uint32_t>    m_toggleTimestamps;
    uint64_t          * m_pTotalCycles      = nullptr;
    uint64_t            m_frameCycleStart   = 0;
    bool                m_isMuted           = false;
};
