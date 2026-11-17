#pragma once

#include "Pch.h"

#include "CassoLink/Audio/ICassetteAudio.h"
#include "Devices/Tape/ITapeDeckPort.h"
#include "Machines/Apple2/Common/CassettePort.h"
#include "../EmuTests/TestMachine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatedCassetteAudio
//
//  The PC's audio wired to an emulated ][+ or //e's cassette port, in place
//  of its recorder. The PC's output is the cassette input, read as a
//  comparator with a little hysteresis reads it; the output's toggles are
//  the PC's input, a square wave. A Capture runs the machine for its wait,
//  so the PC and the machine share the machine's clock.
//
//  The faults of a real path are added on request: either direction
//  inverted, and the PC's input weakened, filtered, hissing and offset.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatedCassetteAudio : public ICassetteAudio, public ITapeDeckPort
{
public:
    static constexpr uint64_t  kStepCycles = 17'030;

    struct Faults
    {
        bool   invertToApple   = false;
        bool   invertFromApple = false;
        float  level           = 0.7f;   // the PC's input, from the Apple's output
        float  hiss            = 0.0f;
        double lowPassHz       = 0.0;
        float  offset          = 0.0f;
    };

    EmulatedCassetteAudio (TestMachine & machine, uint32_t sampleRate = 44100, Faults faults = Faults()) :
        m_machine    (machine),
        m_rate       (sampleRate),
        m_clock      ((double) machine.GetConfig().clockSpeed),
        m_faults     (faults),
        m_random     (2026)
    {
        m_machine.GetRefs().cassettePort->SetDeck (this);
        m_origin = Now();
    }

    ~EmulatedCassetteAudio() override
    {
        m_machine.GetRefs().cassettePort->SetDeck (nullptr);
    }

    uint32_t  GetSampleRate () const override { return m_rate; }

    HRESULT  Play (std::span<const float> samples) override
    {
        uint64_t  now = GetSample (Now());



        if (m_inputEnd < now)
        {
            m_input.clear();
            m_inputBase = now;
            m_inputEnd  = now;
        }

        m_input.insert (m_input.end(), samples.begin(), samples.end());
        m_inputEnd += samples.size();

        return S_OK;
    }

    HRESULT  Capture (DWORD waitMs, std::vector<float> & outSamples) override
    {
        uint64_t  cycles = (uint64_t) ((double) waitMs * m_clock / 1000.0);
        uint64_t  run    = 0;



        while (run < cycles)
        {
            uint64_t  step = std::min (kStepCycles, cycles - run);

            m_machine.RunCycles (step);
            run += step;
        }

        RenderOutput (GetSample (Now()), outSamples);
        return S_OK;
    }

    double  GetPlayingMs () const override
    {
        uint64_t  now = GetSample (Now());



        return (m_inputEnd > now) ? (double) (m_inputEnd - now) * 1000.0 / m_rate : 0.0;
    }

    bool  ReadInputLevel (uint64_t busCycle) override
    {
        static constexpr float  kHysteresis = 0.02f;

        uint64_t  sample = GetSample (busCycle);
        float     value  = 0.0f;



        while (!m_input.empty() && m_inputBase + 1 < sample)
        {
            m_input.pop_front();
            m_inputBase++;
        }

        if (sample >= m_inputBase && sample < m_inputBase + m_input.size())
        {
            value = m_input[(size_t) (sample - m_inputBase)];
        }

        if (m_faults.invertToApple)
        {
            value = -value;
        }

        if (value > kHysteresis)
        {
            m_inputLevel = true;
        }
        else if (value < -kHysteresis)
        {
            m_inputLevel = false;
        }

        return m_inputLevel;
    }

    void  OnOutputToggle (uint64_t busCycle) override
    {
        m_toggles.push_back (busCycle);
    }

private:
    uint64_t  Now () const
    {
        return *m_machine.GetCpu()->GetBusCyclePtr();
    }

    uint64_t  GetSample (uint64_t busCycle) const
    {
        return (uint64_t) ((double) (busCycle - m_origin) * m_rate / m_clock);
    }

    //  Each sample is the square wave's mean over its interval, then the
    //  faults.
    void  RenderOutput (uint64_t endSample, std::vector<float> & outSamples)
    {
        std::uniform_real_distribution<float>  hiss (-m_faults.hiss, m_faults.hiss);
        double                                 smoothing = (m_faults.lowPassHz > 0.0) ? 1.0 - exp (-2.0 * std::numbers::pi * m_faults.lowPassHz / m_rate) : 1.0;
        double                                 cyclesPer = m_clock / m_rate;



        outSamples.clear();

        for (; m_rendered < endSample; m_rendered++)
        {
            double  start = (double) m_origin + (double) m_rendered * cyclesPer;
            double  end   = start + cyclesPer;
            double  high  = 0.0;
            double  at    = start;
            double  value = 0.0;

            while (!m_toggles.empty() && (double) m_toggles.front() < end)
            {
                double  edge = std::max ((double) m_toggles.front(), start);

                high += m_outputLevel ? (edge - at) : 0.0;
                at    = edge;
                m_outputLevel = !m_outputLevel;
                m_toggles.pop_front();
            }

            high  += m_outputLevel ? (end - at) : 0.0;
            value  = (2.0 * high / cyclesPer - 1.0) * m_faults.level;
            value  = m_faults.invertFromApple ? -value : value;
            m_smoothed += smoothing * (value - m_smoothed);

            outSamples.push_back ((float) m_smoothed + m_faults.offset + hiss (m_random));
        }
    }

    TestMachine           & m_machine;
    uint32_t                m_rate;
    double                  m_clock;
    Faults                  m_faults;
    std::mt19937            m_random;
    uint64_t                m_origin      = 0;
    std::deque<float>       m_input;
    uint64_t                m_inputBase   = 0;
    uint64_t                m_inputEnd    = 0;
    bool                    m_inputLevel  = false;
    std::deque<uint64_t>    m_toggles;
    uint64_t                m_rendered    = 0;
    bool                    m_outputLevel = false;
    double                  m_smoothed    = 0.0;
};
