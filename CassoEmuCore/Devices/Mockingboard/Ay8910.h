#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Ay8910
//
//  Clean-room General Instrument AY-3-8910 Programmable Sound Generator,
//  implemented from the datasheet. Three square-wave tone channels, a
//  17-bit-LFSR noise generator, and a 16-level envelope generator feed a
//  logarithmic DAC; the three channels are summed to one mono float32
//  stream. The chip is register-driven: the host latches a register
//  address then writes or reads data, exactly as the Mockingboard's 6522
//  drives the AY bus.
//
//  Synthesis is pull-based: GenerateSample() advances the chip by one
//  host-audio-sample's worth of clocks (via a fractional accumulator) and
//  returns the summed channel output. The internal engine is stepped in
//  "base ticks" of clock/8, chosen so a tone channel that toggles every
//  TP base ticks produces the datasheet output frequency clock/(16*TP);
//  noise steps its LFSR every 2*NP base ticks -> clock/(16*NP), and the
//  envelope advances one level every 2*EP base ticks -> a full 16-step
//  ramp repeats at clock/(256*EP).
//
//  The output is unipolar (0..sum-of-levels), matching the real DAC; the
//  DC component is removed downstream by the audio source's DC blocker.
//  Register-timed PCM tricks (rapid amplitude writes within one audio
//  frame) are out of scope and not resolved sub-frame.
//
////////////////////////////////////////////////////////////////////////////////

class Ay8910
{
public:
    static constexpr Byte    kRegCount = 16;

    static constexpr Byte    kRegToneAFine   = 0;
    static constexpr Byte    kRegToneACoarse = 1;
    static constexpr Byte    kRegToneBFine   = 2;
    static constexpr Byte    kRegToneBCoarse = 3;
    static constexpr Byte    kRegToneCFine   = 4;
    static constexpr Byte    kRegToneCCoarse = 5;
    static constexpr Byte    kRegNoisePeriod = 6;
    static constexpr Byte    kRegMixer       = 7;
    static constexpr Byte    kRegAmpA        = 8;
    static constexpr Byte    kRegAmpB        = 9;
    static constexpr Byte    kRegAmpC        = 10;
    static constexpr Byte    kRegEnvFine     = 11;
    static constexpr Byte    kRegEnvCoarse   = 12;
    static constexpr Byte    kRegEnvShape    = 13;
    static constexpr Byte    kRegIoPortA     = 14;
    static constexpr Byte    kRegIoPortB     = 15;

    // Amplitude register: bits 0..3 select a fixed level, bit 4 selects
    // the envelope level instead.
    static constexpr Byte    kAmpLevelMask = 0x0F;
    static constexpr Byte    kAmpUseEnvelope = 0x10;

    // Envelope shape (R13) control bits.
    static constexpr Byte    kEnvHold      = 0x01;
    static constexpr Byte    kEnvAlternate = 0x02;
    static constexpr Byte    kEnvAttack    = 0x04;
    static constexpr Byte    kEnvContinue  = 0x08;

    // Mockingboard clocks the AY from the Apple II system clock (~1.023 MHz).
    static constexpr double  kDefaultClockHz = 1022727.0;

    // Internal prescaler: the synthesis engine steps at clock / this.
    static constexpr int     kBaseTickDivisor = 8;

    static constexpr int     kMaxEnvLevel = 15;

    explicit Ay8910 (double clockHz = kDefaultClockHz);

    void    SetSampleRate (uint32_t sampleRate);
    void    SetClock      (double clockHz);

    // Bus interface as driven by the 6522: latch an address, then write or
    // read the data register at that address.
    void    LatchAddress (Byte reg) { m_latched = reg; }
    void    WriteData    (Byte value);
    Byte    ReadData     () const;

    // Direct register access (used by the latch/data path and by tests).
    void    WriteRegister (Byte reg, Byte value);
    Byte    ReadRegister  (Byte reg) const;

    void    Reset ();

    // Advance the chip by one output sample and return the summed mono
    // output. Roughly [0, 3] before the caller's gain/DC-block stage.
    float   GenerateSample ();

    // Inspectors for tests.
    Byte     GetLatchedAddress () const { return m_latched; }
    bool     GetToneState  (int channel) const { return m_toneState[channel] != 0; }
    uint32_t GetNoiseLfsr  () const { return m_lfsr; }
    int      GetEnvLevel   () const { return m_envLevel; }
    bool     IsEnvHolding  () const { return m_envHolding; }

    static float VolumeForLevel (int level);

private:
    void    AdvanceBaseTick  ();
    void    StepLfsr         ();
    void    EnvelopeStep     ();
    void    RestartEnvelope  (Byte shape);
    float   CurrentOutput    () const;
    bool    IsSilent         () const;

    int     TonePeriod  (int channel) const;
    int     NoisePeriod () const;
    int     EnvPeriod   () const;

    // Logarithmic DAC: 16 levels, ~3 dB per step (sqrt(2) ratio),
    // level 0 == silence, level 15 == full scale. Derived from the
    // formula amp[L] = 2^((L-15)/2), a clean-room approximation of the
    // AY's non-linear volume curve.
    static constexpr float   kVolTable[16] =
    {
        0.00000000f, 0.00781250f, 0.01104854f, 0.01562500f,
        0.02209709f, 0.03125000f, 0.04419417f, 0.06250000f,
        0.08838835f, 0.12500000f, 0.17677670f, 0.25000000f,
        0.35355339f, 0.50000000f, 0.70710678f, 1.00000000f
    };

    Byte     m_regs[kRegCount] = {};
    Byte     m_latched = 0;

    double   m_clockHz        = kDefaultClockHz;
    uint32_t m_sampleRate     = 0;
    double   m_ticksPerSample = 0.0;
    double   m_tickAccum      = 0.0;

    // Tone generators.
    int      m_toneCounter[3] = { 1, 1, 1 };
    int      m_toneState[3]   = { 0, 0, 0 };

    // Noise generator (17-bit LFSR, seeded non-zero).
    int      m_noiseCounter = 1;
    uint32_t m_lfsr         = 1;

    // Envelope generator.
    int      m_envCounter = 1;
    int      m_envLevel   = 0;
    bool     m_envDirUp   = false;
    bool     m_envHolding = false;
    bool     m_envCont    = false;
    bool     m_envAttack  = false;
    bool     m_envAlt     = false;
    bool     m_envHold    = false;
};
