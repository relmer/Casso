#include "Pch.h"

#include "Devices/Ay8910.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Core/StateFloat.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DiagnosticsSnapshot.h"





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
static constexpr int     s_kChannelCount    = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  Ay8910
//
////////////////////////////////////////////////////////////////////////////////

Ay8910::Ay8910 (double clockHz)
{
    m_clockHz = clockHz;

    Reset();
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
//  A data write reaches the register selected by the last latched address.
//  Addresses outside 0..15 select no register, so the write is inert -- the
//  documented AY behavior, not a bug.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::WriteData (Byte value)
{
    if (m_latched < kRegCount)
    {
        WriteRegister (m_latched, value);

        m_writeCount++;
        m_hasLastWrite   = true;
        m_lastWriteReg   = m_latched;
        m_lastWriteValue = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadData
//
//  Reads the register selected by the last latched address; an out-of-range
//  address reads as the floating data bus (0xFF).
//
////////////////////////////////////////////////////////////////////////////////

Byte Ay8910::ReadData() const
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
//  Direct register write. `reg` must name one of the 16 registers -- the bus
//  path (WriteData) filters out-of-range addresses, so reaching here with
//  reg >= 16 is a caller bug and asserts. A write to the envelope-shape
//  register always restarts the envelope, even when the value is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::WriteRegister (Byte reg, Byte value)
{
    HRESULT  hr = S_OK;



    CBRAEx (reg < kRegCount, E_INVALIDARG);

    m_regs[reg] = value;

    if (reg == kRegEnvShape)
    {
        RestartEnvelope (value);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
//  Direct register read. `reg` must name one of the 16 registers; reaching
//  here with reg >= 16 is a caller bug and asserts.
//
////////////////////////////////////////////////////////////////////////////////

Byte Ay8910::ReadRegister (Byte reg) const
{
    HRESULT  hr     = S_OK;
    Byte     result = 0xFF;



    CBRAEx (reg < kRegCount, E_INVALIDARG);

    result = m_regs[reg];

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Returns the PSG to its power-on state: registers cleared and every
//  generator re-armed.
//
//  The LFSR seeds to 1, not 0. A linear-feedback shift register at zero is a
//  fixed point -- it stays zero forever -- so the noise channel would be
//  permanently silent. Any non-zero seed works; 1 is the conventional one.
//
//  Tone and noise counters are reloaded from their PERIODS rather than zeroed,
//  so each generator starts a full period rather than firing immediately on
//  the first tick and producing a click.
//
//  The noise and envelope counters load twice their period because both are
//  clocked at half the tone rate in the real part -- the factor is the divider,
//  not a fudge.
//
//  SetSampleRate is re-applied last so the tick accumulator's scale matches
//  the freshly cleared state.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::Reset()
{
    int   c = 0;



    for (c = 0; c < kRegCount; c++)
    {
        m_regs[c] = 0;
    }

    m_latched        = 0;
    m_tickAccum      = 0.0;
    m_writeCount     = 0;
    m_hasLastWrite   = false;
    m_lastWriteReg   = 0;
    m_lastWriteValue = 0;

    for (c = 0; c < s_kChannelCount; c++)
    {
        m_toneCounter[c] = GetTonePeriod (c);
        m_toneState[c]   = 0;
    }

    m_noiseCounter = 2 * GetNoisePeriod();
    m_lfsr         = 1;

    m_envCounter = 2 * GetEnvPeriod();
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
//  returns the resulting mono output. Requires a configured sample rate --
//  generating audio before SetSampleRate is a code-ordering bug and asserts.
//
////////////////////////////////////////////////////////////////////////////////

float Ay8910::GenerateSample()
{
    HRESULT  hr     = S_OK;
    float    result = 0.0f;
    int      ticks  = 0;
    int      i      = 0;
    bool     silent = false;



    CBRAEx (m_sampleRate != 0, E_UNEXPECTED);

    // Silence fast-path: with every channel amplitude register zero (fixed
    // volume 0, envelope mode off) the mixed output is 0 regardless of the
    // tone / noise / envelope phases, so skip advancing the generators and
    // return silence (result is already 0). The frozen phases are unobservable
    // while the whole chip is muted and resume on the next amplitude write; a
    // single active channel disables the fast-path so real playback is exact.
    silent = IsSilent();
    BAIL_OUT_IF (silent, S_OK);

    m_tickAccum += m_ticksPerSample;
    ticks        = static_cast<int> (m_tickAccum);
    m_tickAccum -= ticks;

    for (i = 0; i < ticks; i++)
    {
        AdvanceBaseTick();
    }

    result = GetCurrentOutput();

Error:
    if (m_scopeSamplesLeft > 0)
    {
        CaptureScope();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetVolumeForLevel
//
////////////////////////////////////////////////////////////////////////////////

float Ay8910::GetVolumeForLevel (int level)
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
//  GetTonePeriod ticks; noise steps its LFSR every 2*GetNoisePeriod ticks; the
//  envelope advances one level every 2*GetEnvPeriod ticks.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::AdvanceBaseTick()
{
    int   c = 0;



    for (c = 0; c < s_kChannelCount; c++)
    {
        if (--m_toneCounter[c] <= 0)
        {
            m_toneCounter[c] = GetTonePeriod (c);
            m_toneState[c]  ^= 1;
        }
    }

    if (--m_noiseCounter <= 0)
    {
        m_noiseCounter = 2 * GetNoisePeriod();
        StepLfsr();
    }

    if (--m_envCounter <= 0)
    {
        m_envCounter = 2 * GetEnvPeriod();
        StepEnvelope();
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

void Ay8910::StepLfsr()
{
    uint32_t   feedback = (m_lfsr ^ (m_lfsr >> s_kLfsrFeedbackTap)) & 1u;



    m_lfsr = (m_lfsr >> 1) | (feedback << s_kLfsrHighBit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepEnvelope
//
//  Advances the 4-bit envelope level one step and applies the HOLD /
//  ALTERNATE / ATTACK / CONTINUE rules at the end of each ramp.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::StepEnvelope()
{
    if (m_envHolding)
    {
        // At the last level until R13 is written; only the picture's time moves on.
        m_envHeldSteps = std::min (m_envHeldSteps + 1, kEnvHeldStepCap);
    }
    else if (m_envDirUp && m_envLevel < kMaxEnvLevel)
    {
        // Still ramping up toward the top.
        m_envLevel++;
    }
    else if (!m_envDirUp && m_envLevel > 0)
    {
        // Still ramping down toward zero.
        m_envLevel--;
    }
    else if (!m_envCont)
    {
        // Ramp complete, one-shot: park at 0 and hold.
        m_envLevel   = 0;
        m_envHolding = true;
        m_envRamps++;
    }
    else if (m_envHold)
    {
        // Ramp complete, hold at the end reached (ALTERNATE flips it once).
        m_envHolding = true;
        m_envRamps++;

        if (m_envAlt)
        {
            m_envLevel = m_envDirUp ? 0 : kMaxEnvLevel;
        }
    }
    else if (m_envAlt)
    {
        // ALTERNATE: reverse direction for the next ramp.
        m_envDirUp = !m_envDirUp;
        m_envRamps++;
    }
    else
    {
        // Repeat: jump back to the start of the ramp.
        m_envLevel = m_envDirUp ? 0 : kMaxEnvLevel;
        m_envRamps++;
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

    m_envHolding   = false;
    m_envRamps     = 0;
    m_envHeldSteps = 0;
    m_envDirUp     = m_envAttack;
    m_envLevel   = m_envDirUp ? 0 : kMaxEnvLevel;
    m_envCounter = 2 * GetEnvPeriod();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCurrentOutput
//
//  Mixes the three channels. For each channel the tone and noise streams
//  are gated by the mixer-enable bits (R7: a set bit disables that source),
//  then scaled by the channel's fixed or envelope-driven amplitude.
//
////////////////////////////////////////////////////////////////////////////////

float Ay8910::GetCurrentOutput() const
{
    float   out = 0.0f;
    int     c   = 0;



    for (c = 0; c < s_kChannelCount; c++)
    {
        if (!IsChannelMuted (c))
        {
            out += GetChannelOutput (c);
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetChannelOutput
//
////////////////////////////////////////////////////////////////////////////////

float Ay8910::GetChannelOutput (int channel) const
{
    Byte   mixer         = m_regs[kRegMixer];
    int    toneDisabled  = (mixer >> channel) & 1;
    int    noiseDisabled = (mixer >> (channel + s_kChannelCount)) & 1;
    int    toneTerm      = toneDisabled  | m_toneState[channel];
    int    noiseTerm     = noiseDisabled | static_cast<int> (m_lfsr & 1u);
    int    active        = toneTerm & noiseTerm;
    Byte   amp           = m_regs[kRegAmpA + channel];
    int    level         = (amp & kAmpUseEnvelope) ? m_envLevel : (amp & kAmpLevelMask);



    return (active != 0) ? GetVolumeForLevel (level) : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureScope
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::CaptureScope()
{
    int   c = 0;



    for (c = 0; c < s_kChannelCount; c++)
    {
        m_scope[(size_t) c * kScopeSamples + m_scopeAt] = GetChannelOutput (c);
    }

    m_scopeAt = (m_scopeAt + 1) % kScopeSamples;
    m_scopeSamplesLeft--;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetChannelMuted
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::SetChannelMuted (int channel, bool isMuted)
{
    Byte  bit = (Byte) (1 << channel);



    m_muted = (Byte) (isMuted ? (m_muted | bit) : (m_muted & ~bit));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSilent
//
//  True when every channel amplitude register is zero -- fixed volume 0 with
//  envelope mode off -- so the mixed output is guaranteed silence regardless
//  of the tone / noise / envelope phases.
//
////////////////////////////////////////////////////////////////////////////////

bool Ay8910::IsSilent() const
{
    int   c      = 0;
    bool  silent = true;



    // One audible channel is enough to disqualify, so the scan stops there.
    for (c = 0; silent && c < s_kChannelCount; c++)
    {
        silent = (m_regs[kRegAmpA + c] == 0);
    }

    return silent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTonePeriod
//
////////////////////////////////////////////////////////////////////////////////

int Ay8910::GetTonePeriod (int channel) const
{
    Byte   fine   = m_regs[kRegToneAFine + channel * 2];
    Byte   coarse = m_regs[kRegToneACoarse + channel * 2];
    int    period = ((coarse & s_kToneCoarseMask) << s_kByteShift) | fine;



    return (period == 0) ? 1 : period;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNoisePeriod
//
////////////////////////////////////////////////////////////////////////////////

int Ay8910::GetNoisePeriod() const
{
    int   period = m_regs[kRegNoisePeriod] & s_kNoisePeriodMask;



    return (period == 0) ? 1 : period;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEnvPeriod
//
////////////////////////////////////////////////////////////////////////////////

int Ay8910::GetEnvPeriod() const
{
    int   period = (m_regs[kRegEnvCoarse] << s_kByteShift) | m_regs[kRegEnvFine];



    return (period == 0) ? 1 : period;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ay8910::AppendDiagnostics
//
//  The periods as the registers set them, the mixer, each channel's
//  amplitude, and the envelope.
//
//  The mixer row's value is the register, and its decode is lit for a tone or
//  noise source that is on. The register turns a source off with a set bit,
//  so those six bits are inverted for the decode; the I/O port bits are set
//  for an output and are shown as they are.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::AppendDiagnostics (const std::string & title, DiagnosticsSnapshot & snapshot) const
{
    using P = IDiagnosticsProvider;

    static constexpr std::array<const char *, 8>  kMixerBits      = { "IOB", "IOA", "NC", "NB", "NA", "TC", "TB", "TA" };
    static constexpr std::array<const char *, 8>  kShapeBits      = { "", "", "", "", "CONT", "ATT", "ALT", "HOLD" };
    static constexpr std::array<const char *, 8>  kAmpBits        = { "", "", "", "ENV", "", "", "", "" };
    static constexpr const char *                 kNames[]        = { "A", "B", "C" };
    constexpr int                                 kChannels       = 3;
    constexpr Byte                                kSourceDisables = 0x3F;   // tone and noise, A to C
    DiagnosticsGroup                              group           { title, {} };
    DiagnosticsRow                                mixer           = P::MakeHexRow ("Mixer (lit = enabled)", m_regs[kRegMixer], P::kByteDigits);
    Byte                                          enables         = (Byte) (m_regs[kRegMixer] ^ kSourceDisables);



    mixer.bits = P::MakeByteRow (mixer.label, enables, kMixerBits).bits;

    for (int channel = 0; channel < kChannels; channel++)
    {
        group.rows.push_back (P::MakeHexRow (std::format ("Tone {} period", kNames[channel]), (uint32_t) GetTonePeriod (channel), P::kWordDigits));
    }

    group.rows.push_back (P::MakeHexRow  ("Noise period", (uint32_t) GetNoisePeriod(), P::kByteDigits));
    group.rows.push_back (std::move (mixer));

    for (int channel = 0; channel < kChannels; channel++)
    {
        group.rows.push_back (P::MakeByteRow (std::format ("Amplitude {}", kNames[channel]), m_regs[kRegAmpA + channel], kAmpBits));
    }

    group.rows.push_back (P::MakeHexRow  ("Envelope period", (uint32_t) GetEnvPeriod(), P::kWordDigits));
    group.rows.push_back (P::MakeByteRow ("Envelope shape",  m_regs[kRegEnvShape], kShapeBits));
    group.rows.push_back (P::MakeTextRow ("Envelope level",  std::format ("{}", m_envLevel)));
    group.rows.push_back (P::MakeHexRow  ("Latched register", m_latched, P::kByteDigits));

    snapshot.groups.push_back (std::move (group));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ay8910::AppendSoundBoard
//
//  The frequencies are the datasheet's: clock / (16 * period) for a tone and for
//  noise, and 256 * period / clock for one ramp of the envelope.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::AppendSoundBoard (DiagnosticsSoundPsg & out) const
{
    constexpr double  kToneDivisor     = 16.0;
    constexpr double  kEnvelopeDivisor = 256.0;
    constexpr double  kMsPerSecond     = 1000.0;
    int               c                = 0;



    for (c = 0; c < DiagnosticsSoundPsg::kChannelCount; c++)
    {
        DiagnosticsSoundChannel  & channel = out.channels[c];
        Byte                       amp     = m_regs[kRegAmpA + c];
        size_t                     first   = (size_t) c * kScopeSamples;

        channel.period       = GetTonePeriod (c);
        channel.frequencyHz  = m_clockHz / (kToneDivisor * channel.period);
        channel.isToneOn     = ((m_regs[kRegMixer] >> c) & 1) == 0;
        channel.isNoiseOn    = ((m_regs[kRegMixer] >> (c + s_kChannelCount)) & 1) == 0;
        channel.usesEnvelope = (amp & kAmpUseEnvelope) != 0;
        channel.level        = channel.usesEnvelope ? m_envLevel : (amp & kAmpLevelMask);
        channel.isMuted      = IsChannelMuted (c);

        channel.scope.clear();

        for (size_t i = 0; i < kScopeSamples && !m_scope.empty(); i++)
        {
            channel.scope.push_back (m_scope[first + (m_scopeAt + i) % kScopeSamples]);
        }
    }

    out.envelopeRegister = m_regs[kRegEnvShape];
    out.envelopePeriod   = GetEnvPeriod();
    out.envelopeLevel    = m_envLevel;
    out.envelopeRamps      = m_envRamps;
    out.envelopeHeldSteps  = m_envHeldSteps;
    out.isEnvelopeHeld     = m_envHolding;
    out.envelopeRampMs   = kEnvelopeDivisor * out.envelopePeriod / m_clockHz * kMsPerSecond;
    out.noisePeriod      = GetNoisePeriod();
    out.noiseHz          = m_clockHz / (kToneDivisor * out.noisePeriod);
    out.mixer            = m_regs[kRegMixer];
    out.writeCount       = m_writeCount;
    out.hasLastWrite     = m_hasLastWrite;
    out.lastWriteReg     = m_lastWriteReg;
    out.lastWriteValue   = m_lastWriteValue;

    std::copy (std::begin (m_regs), std::end (m_regs), out.registers.begin());

    if (m_scope.empty())
    {
        m_scope.assign ((size_t) DiagnosticsSoundPsg::kChannelCount * kScopeSamples, 0.0f);
    }

    m_scopeSamplesLeft = (int64_t) m_sampleRate * kScopeCaptureSeconds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ay8910::AppendChannelLevels
//
//  A channel's amplitude register, or the envelope's level where the register
//  hands the channel to the envelope, whatever the mixer is set to. A channel
//  with tone and noise both turned off does not go quiet: its output holds
//  at that level (GetCurrentOutput), which is how samples are played through
//  the amplitude register.
//
////////////////////////////////////////////////////////////////////////////////

void Ay8910::AppendChannelLevels (const std::string & title, DiagnosticsMeters & meters) const
{
    static constexpr const char * kNames[]  = { "A", "B", "C" };
    constexpr int                 kChannels = 3;



    for (int channel = 0; channel < kChannels; channel++)
    {
        Byte   amp   = m_regs[kRegAmpA + channel];
        int    level = (amp & kAmpUseEnvelope) ? m_envLevel : (amp & kAmpLevelMask);

        meters.levels.push_back ({ std::format ("{} {}", title, kNames[channel]),
                                   (float) level / (float) kMaxEnvLevel });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
//  THE TONE, NOISE AND ENVELOPE GENERATORS ARE NOT SAVED. They are advanced
//  by the host's audio thread, one output sample at a time, so how far they
//  have gone at a given cycle depends on when the audio device pulled
//  samples, and a replay of history never matches it. Saved, they made every
//  replay of a Mockingboard program differ from its keyframes, and history
//  was cut there. The guest reads back only the registers, so the registers,
//  the latch and the record of the bus's writes are the chip's machine
//  state; the generators go on from where the host has them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ay8910::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBytes (m_regs, kRegCount);
    writer.WriteByte  (m_latched);

    writer.WriteUInt32 (m_writeCount);
    writer.WriteBool   (m_hasLastWrite);
    writer.WriteByte   (m_lastWriteReg);
    writer.WriteByte   (m_lastWriteValue);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  A state before version 3 has the tone, noise and envelope generators
//  after the latch, which are read and dropped (see SaveState); version
//  1 has no record of the bus's writes after them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ay8910::LoadState (StateReader & reader)
{
    constexpr uint16_t  kFirstWithWrites        = 2;
    constexpr uint16_t  kFirstWithoutGenerators = 3;
    HRESULT             hr                      = S_OK;
    uint16_t            version                 = 0;
    double              tickAccum               = 0.0;
    uint32_t            dropped                 = 0;
    bool                droppedFlag             = false;
    int                 i                       = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBytes (m_regs, kRegCount);
    reader.ReadByte  (m_latched);

    if (version < kFirstWithoutGenerators)
    {
        StateFloat::Read (reader, tickAccum);

        //  Three tone channels' counter and state, then the noise counter,
        //  the LFSR, the envelope's counter and level, and its six flags.
        for (i = 0; i < 3 * 2 + 4; i++)
        {
            reader.ReadUInt32 (dropped);
        }

        for (i = 0; i < 6; i++)
        {
            reader.ReadBool (droppedFlag);
        }
    }

    m_writeCount     = 0;
    m_hasLastWrite   = false;
    m_lastWriteReg   = 0;
    m_lastWriteValue = 0;

    if (version >= kFirstWithWrites)
    {
        reader.ReadUInt32 (m_writeCount);
        reader.ReadBool   (m_hasLastWrite);
        reader.ReadByte   (m_lastWriteReg);
        reader.ReadByte   (m_lastWriteValue);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}

