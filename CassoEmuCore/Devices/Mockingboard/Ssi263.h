#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ssi263
//
//  Clean-room Silicon Systems SSI 263A phoneme speech synthesizer, written
//  from the datasheet register map. The same part was marketed by Votrax as
//  the SC-02, and it is the voice chip on the Mockingboard C. Generic and
//  reusable: the class knows nothing about the Mockingboard -- it exposes
//  five attribute registers, a status read, and a mono sample stream.
//
//  Register file, selected by the three address lines RS2-RS0:
//
//    0  DUR/PHON   D7:6 = DR1,DR0 duration    D5:0 = P5-P0 phoneme
//    1  INFLECT    D7:0 = I10-I3
//    2  RATE/INFL  D7:4 = R3-R0 rate          D3 = I11   D2:0 = I2-I0
//    3  CTL/ART/A  D7 = CTL   D6:4 = T2-T0    D3:0 = A3-A0 amplitude
//    4  FILTER     D7:0 = F7-F0
//
//  RS2 high selects the filter register regardless of the low two lines, so
//  addresses 4..7 all alias to it -- five registers in eight slots.
//
//  The 12-bit inflection value is scattered non-contiguously across two
//  registers and is reassembled by InflectionValue().
//
//  Timing follows the datasheet formulas, all relative to the external clock:
//
//    Frame Duration       = 4096 * (16 - R) / XCK
//    Phoneme Duration     = Frame Duration * (4 - D)
//    Inflection Frequency = XCK / (8 * (4096 - I))
//    Filter Frequency     = XCK / (2 * (256 - F))
//
//  A/R (Acknowledge/Request Not) goes from high to low once a phoneme has
//  been generated, and the owning card wires it to a VIA control line as an
//  interrupt. Reading the chip returns the INVERTED A/R state in D7 and
//  ignores the register address entirely.
//
//  Quiescence is the chip's own documented behavior rather than a policy
//  imposed here: CTL is set on power-up and reset, which is Power Down --
//  excitation and analog off, registers retained, A/R disabled. Software
//  brings CTL low to select an operating mode from the DR bits, and the
//  DR1=DR0=0 mode disables A/R outright.
//
////////////////////////////////////////////////////////////////////////////////

class Ssi263
{
public:
    static constexpr Byte    kRegCount    = 5;
    static constexpr Byte    kAddressMask = 0x07;

    static constexpr Byte    kRegDurationPhoneme = 0;
    static constexpr Byte    kRegInflection      = 1;
    static constexpr Byte    kRegRateInflection  = 2;
    static constexpr Byte    kRegCtlArtAmp       = 3;
    static constexpr Byte    kRegFilterFreq      = 4;

    static constexpr Byte    kPhonemeMask   = 0x3F;
    static constexpr Byte    kPhonemeCount  = 64;
    static constexpr Byte    kDurationShift = 6;

    static constexpr Byte    kCtl           = 0x80;
    static constexpr Byte    kArticMask     = 0x70;
    static constexpr Byte    kArticShift    = 4;
    static constexpr Byte    kAmplitudeMask = 0x0F;

    static constexpr Byte    kRateMask       = 0xF0;
    static constexpr Byte    kRateShift      = 4;
    static constexpr Byte    kInflect11      = 0x08;
    static constexpr Byte    kInflectLowMask = 0x07;

    // D7 on a read carries the inverted A/R level.
    static constexpr Byte    kStatusRequest = 0x80;

    // Duration-bit mode encodings, latched on a CTL one-to-zero transition.
    static constexpr Byte    kModePhonemeTransitioned = 3;
    static constexpr Byte    kModePhonemeImmediate    = 2;
    static constexpr Byte    kModeFrameImmediate      = 1;
    static constexpr Byte    kModeArDisabled          = 0;

    // The datasheet's nominal time base: a colorburst crystal divided by two.
    static constexpr double  kDefaultClockHz = 1789772.5;

    explicit Ssi263 (double clockHz = kDefaultClockHz);

    void    SetSampleRate (uint32_t sampleRate);
    void    SetClock      (double clockHz);

    void    WriteRegister (Byte reg, Byte value);
    Byte    ReadRegister  (Byte reg) const;

    void    Reset ();

    // Advance the chip by `cycles` of emulated machine time. Phoneme duration
    // and the A/R request are driven from here, never from the audio path, so
    // an utterance occupies the same emulated span at any host sample rate.
    void    Tick (uint32_t cycles);

    bool    IsRequesting () const { return m_request; }
    bool    IsPoweredDown() const { return (m_reg[kRegCtlArtAmp] & kCtl) != 0; }
    bool    IsSilent     () const;

    Byte    Phoneme     () const { return static_cast<Byte> (m_reg[kRegDurationPhoneme] & kPhonemeMask); }
    Byte    DurationSel () const { return static_cast<Byte> (m_reg[kRegDurationPhoneme] >> kDurationShift); }
    Byte    RateSel     () const { return static_cast<Byte> ((m_reg[kRegRateInflection] & kRateMask) >> kRateShift); }
    Byte    Amplitude   () const { return static_cast<Byte> (m_reg[kRegCtlArtAmp] & kAmplitudeMask); }
    Byte    Articulation() const { return static_cast<Byte> ((m_reg[kRegCtlArtAmp] & kArticMask) >> kArticShift); }
    Byte    ActiveMode  () const { return m_mode; }

    uint16_t InflectionValue  () const;
    double   FrameDurationSec () const;
    double   PhonemeDurationSec () const;
    double   FilterFrequencyHz () const;
    double   InflectionFrequencyHz () const;

    static Byte  SelectRegister (Byte address);

private:
    void    LatchMode     ();
    void    BeginPhoneme  ();

    double     m_clockHz    = kDefaultClockHz;
    uint32_t   m_sampleRate = 0;

    Byte       m_reg[kRegCount] = {};

    // Mode latched on the CTL one-to-zero transition, from the DR bits as they
    // stood at that moment -- NOT re-read as DR changes afterwards.
    Byte       m_mode = kModeArDisabled;

    // A/R is active-low on the pin; this tracks the logical "wants data" state.
    bool       m_request = false;

    // Remaining emulated cycles in the current phoneme, and whether one is
    // sounding at all.
    double     m_phonemeCycles = 0.0;
    bool       m_sounding      = false;
};
