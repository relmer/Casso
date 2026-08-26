#include "Pch.h"

#include "Ssi263.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ssi263
//
//  Constructing the chip leaves it in its power-on state, which the datasheet
//  defines as Power Down: CTL set, no audio, no request.
//
////////////////////////////////////////////////////////////////////////////////

Ssi263::Ssi263 (double clockHz)
{
    m_clockHz = (clockHz > 0.0) ? clockHz : kDefaultClockHz;

    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleRate
//
//  The host rendering rate. Deliberately does NOT participate in phoneme
//  timing -- that is driven from emulated cycles in Tick, so an utterance
//  occupies the same emulated span whatever the audio device is doing.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::SetSampleRate (uint32_t sampleRate)
{
    m_sampleRate = sampleRate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetClock
//
//  The external XCK time base. Every published timing formula is relative to
//  it, so changing it rescales durations and filter frequencies together.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::SetClock (double clockHz)
{
    if (clockHz > 0.0)
    {
        m_clockHz = clockHz;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectRegister
//
//  Maps the three address lines onto the five registers. RS2 high selects the
//  filter register regardless of RS1/RS0, so addresses 4..7 all alias to it.
//
////////////////////////////////////////////////////////////////////////////////

Byte Ssi263::SelectRegister (Byte address)
{
    Byte   sel = static_cast<Byte> (address & kAddressMask);

    return (sel >= kRegFilterFreq) ? kRegFilterFreq : sel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRegister
//
//  Loads one attribute register. Two writes have side effects beyond storing
//  the value:
//
//    * A CTL one-to-zero transition leaves Power Down and latches the
//      operating mode from the duration bits as they stand at that instant.
//      Later changes to those bits select a duration, not a mode.
//
//    * Writing the duration/phoneme register while the chip is running starts
//      that phoneme and withdraws the outstanding request. This is what the
//      software's pacing loop does on every A/R.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::WriteRegister (Byte reg, Byte value)
{
    Byte   sel      = SelectRegister (reg);
    bool   wasDown  = IsPoweredDown();



    m_reg[sel] = value;

    if (sel == kRegCtlArtAmp)
    {
        if (wasDown && !IsPoweredDown())
        {
            LatchMode();
        }
        else if (!wasDown && IsPoweredDown())
        {
            // Power Down silences the output and disables A/R without
            // disturbing the register contents.
            m_sounding      = false;
            m_phonemeCycles = 0.0;
            m_request       = false;
        }
    }
    else if (sel == kRegDurationPhoneme && !IsPoweredDown())
    {
        BeginPhoneme();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
//  A read enables D7 only and returns the INVERTED A/R level there; every
//  other bit is undriven. The register address is ignored entirely, so any
//  address in the chip's range yields the same status.
//
////////////////////////////////////////////////////////////////////////////////

Byte Ssi263::ReadRegister (Byte reg) const
{
    UNREFERENCED_PARAMETER (reg);

    return m_request ? kStatusRequest : static_cast<Byte> (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Power-on state. CTL is set high on reset and on power up, which IS Power
//  Down -- so quiescence is what the hardware does, not a safety measure added
//  here. Nothing sounds and nothing requests until software drives CTL low.
//
//  That property is what lets the sound+speech card default safely: an
//  unprogrammed voice chip cannot introduce an interrupt a sound-only title
//  never armed.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::Reset()
{
    int   i = 0;



    for (i = 0; i < kRegCount; i++)
    {
        m_reg[i] = 0;
    }

    m_reg[kRegCtlArtAmp] = kCtl;

    m_mode          = kModeArDisabled;
    m_request       = false;
    m_phonemeCycles = 0.0;
    m_sounding      = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advances the current phoneme by `cycles` of emulated machine time. When the
//  phoneme's duration is exhausted the chip stops sounding and -- in the modes
//  where A/R is enabled -- raises the request that asks software for the next
//  one.
//
//  A chip that is powered down, or not sounding, has nothing to advance. That
//  early exit is also what makes an idle chip free.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::Tick (uint32_t cycles)
{
    if (!m_sounding || IsPoweredDown())
    {
        return;
    }

    m_phonemeCycles -= static_cast<double> (cycles);

    if (m_phonemeCycles <= 0.0)
    {
        m_phonemeCycles = 0.0;
        m_sounding      = false;

        if (m_mode != kModeArDisabled)
        {
            m_request = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSilent
//
//  True when the chip contributes nothing to the mix, so the audio path can
//  skip synthesis outright. Powered down, not sounding, or at zero amplitude
//  all qualify.
//
////////////////////////////////////////////////////////////////////////////////

bool Ssi263::IsSilent() const
{
    return IsPoweredDown() || !m_sounding || (Amplitude() == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InflectionValue
//
//  Reassembles the 12-bit inflection value, which the register map scatters
//  non-contiguously: I11 sits alone at register 2 bit 3, I10-I3 fill register
//  1, and I2-I0 are register 2 bits 2-0.
//
////////////////////////////////////////////////////////////////////////////////

uint16_t Ssi263::InflectionValue() const
{
    uint16_t   high = static_cast<uint16_t> ((m_reg[kRegRateInflection] & kInflect11) != 0 ? 0x800 : 0);
    uint16_t   mid  = static_cast<uint16_t> (static_cast<uint16_t> (m_reg[kRegInflection]) << 3);
    uint16_t   low  = static_cast<uint16_t> (m_reg[kRegRateInflection] & kInflectLowMask);

    return static_cast<uint16_t> (high | mid | low);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FrameDurationSec
//
//  Datasheet: Frame Duration = 4096 * (16 - R) / XCK.
//
////////////////////////////////////////////////////////////////////////////////

double Ssi263::FrameDurationSec() const
{
    return (4096.0 * (16.0 - static_cast<double> (RateSel()))) / m_clockHz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PhonemeDurationSec
//
//  Datasheet: Phoneme Duration = Frame Duration * (4 - D).
//
//  In the frame-timing mode the duration bits select the mode rather than
//  scaling the frame, so the frame duration stands unmodified.
//
////////////////////////////////////////////////////////////////////////////////

double Ssi263::PhonemeDurationSec() const
{
    double   frame = FrameDurationSec();

    if (m_mode == kModeFrameImmediate)
    {
        return frame;
    }

    return frame * (4.0 - static_cast<double> (DurationSel()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FilterFrequencyHz
//
//  Datasheet: Filter Frequency = XCK / (2 * (256 - F)).
//
////////////////////////////////////////////////////////////////////////////////

double Ssi263::FilterFrequencyHz() const
{
    double   divisor = 2.0 * (256.0 - static_cast<double> (m_reg[kRegFilterFreq]));

    return (divisor > 0.0) ? (m_clockHz / divisor) : 0.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InflectionFrequencyHz
//
//  Datasheet: Inflection Frequency = XCK / (8 * (4096 - I)).
//
////////////////////////////////////////////////////////////////////////////////

double Ssi263::InflectionFrequencyHz() const
{
    double   divisor = 8.0 * (4096.0 - static_cast<double> (InflectionValue()));

    return (divisor > 0.0) ? (m_clockHz / divisor) : 0.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LatchMode
//
//  Captures the operating mode on the CTL one-to-zero transition, from the
//  duration bits as they stand at that instant. Leaving Power Down does not by
//  itself sound anything -- software still has to write a phoneme.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::LatchMode()
{
    m_mode          = DurationSel();
    m_request       = false;
    m_sounding      = false;
    m_phonemeCycles = 0.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginPhoneme
//
//  Starts the phoneme just written and withdraws any outstanding request --
//  the write IS the answer to the previous one.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::BeginPhoneme()
{
    double   seconds = PhonemeDurationSec();

    m_phonemeCycles = seconds * m_clockHz;
    m_sounding      = (m_phonemeCycles > 0.0);
    m_request       = false;
}
