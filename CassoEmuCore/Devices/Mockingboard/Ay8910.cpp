#include "Pch.h"

#include "Ay8910.h"




////////////////////////////////////////////////////////////////////////////////
//
//  File-local constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte    s_kToneCoarseMask  = 0x0F;   // coarse period is 4 bits
static constexpr Byte    s_kNoisePeriodMask = 0x1F;   // noise period is 5 bits
static constexpr int     s_kByteShift       = 8;
static constexpr int     s_kLfsrFeedbackTap = 3;      // AY noise taps bits 0 and 3
static constexpr int     s_kLfsrHighBit     = 16;     // 17-bit shift register




////////////////////////////////////////////////////////////////////////////////
//
//  Ay8910
//
////////////////////////////////////////////////////////////////////////////////

Ay8910::Ay8910 (double clockHz)
{
    m_clockHz = clockHz;

    Reset ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleRate
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::SetSampleRate (uint32_t sampleRate)
{
    m_sampleRate = sampleRate;

    if (m_sampleRate > 0)
    {
        m_ticksPerSample = (m_clockHz / kBaseTickDivisor) / static_cast<double> (m_sampleRate);
    }
    else
    {
        m_ticksPerSample = 0.0;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetClock
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::SetClock (double clockHz)
{
    m_clockHz = clockHz;

    SetSampleRate (m_sampleRate);
}




////////////////////////////////////////////////////////////////////////////////
//
//  WriteData
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::WriteData (Byte value)
{
    if (m_latched < kRegCount)
    {
        WriteRegister (m_latched, value);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadData
//
////////////////////////////////////////////////////////////////////////////////

Byte Ay8910::ReadData () const
{
    Byte   result = 0xFF;

    if (m_latched < kRegCount)
    {
        result = m_regs[m_latched];
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  WriteRegister
//
//  A write to the envelope-shape register always restarts the envelope,
//  even when the value is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::WriteRegister (Byte reg, Byte value)
{
    if (reg >= kRegCount)
    {
        return;
    }

    m_regs[reg] = value;

    if (reg == kRegEnvShape)
    {
        RestartEnvelope (value);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
////////////////////////////////////////////////////////////////////////////////

Byte Ay8910::ReadRegister (Byte reg) const
{
    Byte   result = 0xFF;

    if (reg < kRegCount)
    {
        result = m_regs[reg];
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::Reset ()
{
    int   c = 0;

    for (c = 0; c < kRegCount; c++)
    {
        m_regs[c] = 0;
    }

    m_latched   = 0;
    m_tickAccum = 0.0;

    for (c = 0; c < 3; c++)
    {
        m_toneCounter[c] = TonePeriod (c);
        m_toneState[c]   = 0;
    }

    m_noiseCounter = 2 * NoisePeriod ();
    m_lfsr         = 1;

    m_envCounter = 2 * EnvPeriod ();
    m_envLevel   = 0;
    m_envDirUp   = false;
    m_envHolding = false;
    m_envCont    = false;
    m_envAttack  = false;
    m_envAlt     = false;
    m_envHold    = false;

    SetSampleRate (m_sampleRate);
}




////////////////////////////////////////////////////////////////////////////////
//
//  GenerateSample
//
//  Advances the engine by one output sample worth of base ticks (a
//  fractional accumulator carries the remainder between samples) and
//  returns the resulting mono output.
//
////////////////////////////////////////////////////////////////////////////////

float Ay8910::GenerateSample ()
{
    int   ticks = 0;
    int   i     = 0;

    if (m_sampleRate == 0)
    {
        return 0.0f;
    }

    m_tickAccum += m_ticksPerSample;
    ticks        = static_cast<int> (m_tickAccum);
    m_tickAccum -= ticks;

    for (i = 0; i < ticks; i++)
    {
        AdvanceBaseTick ();
    }

    return CurrentOutput ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  VolumeForLevel
//
////////////////////////////////////////////////////////////////////////////////

float Ay8910::VolumeForLevel (int level)
{
    if (level < 0)
    {
        level = 0;
    }
    else if (level > kMaxEnvLevel)
    {
        level = kMaxEnvLevel;
    }

    return kVolTable[level];
}




////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceBaseTick
//
//  One clock/8 step of every generator. Tone channels toggle every
//  TonePeriod ticks; noise steps its LFSR every 2*NoisePeriod ticks; the
//  envelope advances one level every 2*EnvPeriod ticks.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::AdvanceBaseTick ()
{
    int   c = 0;

    for (c = 0; c < 3; c++)
    {
        if (--m_toneCounter[c] <= 0)
        {
            m_toneCounter[c] = TonePeriod (c);
            m_toneState[c]  ^= 1;
        }
    }

    if (--m_noiseCounter <= 0)
    {
        m_noiseCounter = 2 * NoisePeriod ();
        StepLfsr ();
    }

    if (--m_envCounter <= 0)
    {
        m_envCounter = 2 * EnvPeriod ();
        EnvelopeStep ();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  StepLfsr
//
//  17-bit LFSR with feedback from bits 0 and 3; the low bit is the noise
//  output.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::StepLfsr ()
{
    uint32_t   feedback = (m_lfsr ^ (m_lfsr >> s_kLfsrFeedbackTap)) & 1u;

    m_lfsr = (m_lfsr >> 1) | (feedback << s_kLfsrHighBit);
}




////////////////////////////////////////////////////////////////////////////////
//
//  EnvelopeStep
//
//  Advances the 4-bit envelope level one step and applies the HOLD /
//  ALTERNATE / ATTACK / CONTINUE rules at the end of each ramp.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::EnvelopeStep ()
{
    if (m_envHolding)
    {
        return;
    }

    // Still inside a ramp: move one level toward the far end.
    if (m_envDirUp && m_envLevel < kMaxEnvLevel)
    {
        m_envLevel++;
        return;
    }

    if (!m_envDirUp && m_envLevel > 0)
    {
        m_envLevel--;
        return;
    }

    // Ramp complete (level sits at the end it reached).
    if (!m_envCont)
    {
        m_envLevel   = 0;
        m_envHolding = true;
        return;
    }

    if (m_envHold)
    {
        m_envHolding = true;

        if (m_envAlt)
        {
            m_envLevel = m_envDirUp ? 0 : kMaxEnvLevel;
        }

        return;
    }

    if (m_envAlt)
    {
        m_envDirUp = !m_envDirUp;
    }
    else
    {
        m_envLevel = m_envDirUp ? 0 : kMaxEnvLevel;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  RestartEnvelope
//
//  A write to R13 reloads the shape flags and restarts the ramp from the
//  attack end.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::RestartEnvelope (Byte shape)
{
    m_envCont    = (shape & kEnvContinue)  != 0;
    m_envAttack  = (shape & kEnvAttack)    != 0;
    m_envAlt     = (shape & kEnvAlternate) != 0;
    m_envHold    = (shape & kEnvHold)      != 0;

    m_envHolding = false;
    m_envDirUp   = m_envAttack;
    m_envLevel   = m_envDirUp ? 0 : kMaxEnvLevel;
    m_envCounter = 2 * EnvPeriod ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  CurrentOutput
//
//  Mixes the three channels. For each channel the tone and noise streams
//  are gated by the mixer-enable bits (R7: a set bit disables that source),
//  then scaled by the channel's fixed or envelope-driven amplitude.
//
////////////////////////////////////////////////////////////////////////////////

float Ay8910::CurrentOutput () const
{
    Byte    mixer = m_regs[kRegMixer];
    float   out   = 0.0f;
    int     c     = 0;

    for (c = 0; c < 3; c++)
    {
        int    toneDisabled  = (mixer >> c) & 1;
        int    noiseDisabled = (mixer >> (c + 3)) & 1;
        int    toneTerm      = toneDisabled  | m_toneState[c];
        int    noiseTerm     = noiseDisabled | static_cast<int> (m_lfsr & 1u);
        int    active        = toneTerm & noiseTerm;
        Byte   amp           = m_regs[kRegAmpA + c];
        int    level         = (amp & kAmpUseEnvelope) ? m_envLevel : (amp & kAmpLevelMask);

        if (active != 0)
        {
            out += VolumeForLevel (level);
        }
    }

    return out;
}




////////////////////////////////////////////////////////////////////////////////
//
//  TonePeriod
//
////////////////////////////////////////////////////////////////////////////////

int Ay8910::TonePeriod (int channel) const
{
    Byte   fine   = m_regs[kRegToneAFine + channel * 2];
    Byte   coarse = m_regs[kRegToneACoarse + channel * 2];
    int    period = ((coarse & s_kToneCoarseMask) << s_kByteShift) | fine;

    return (period == 0) ? 1 : period;
}




////////////////////////////////////////////////////////////////////////////////
//
//  NoisePeriod
//
////////////////////////////////////////////////////////////////////////////////

int Ay8910::NoisePeriod () const
{
    int   period = m_regs[kRegNoisePeriod] & s_kNoisePeriodMask;

    return (period == 0) ? 1 : period;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EnvPeriod
//
////////////////////////////////////////////////////////////////////////////////

int Ay8910::EnvPeriod () const
{
    int   period = (m_regs[kRegEnvCoarse] << s_kByteShift) | m_regs[kRegEnvFine];

    return (period == 0) ? 1 : period;
}
