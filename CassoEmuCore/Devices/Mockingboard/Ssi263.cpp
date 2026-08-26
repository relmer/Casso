#include "Pch.h"

#include "Ssi263.h"

// Per-phoneme acoustic targets, indexed by phoneme code $00-$3F.
//
// DISCLOSURE: these are NOT the chip's own values. The SSI 263A's phoneme
// parameters live in an internal ROM that was never published and has never
// been extracted, so this table is derived from the public acoustic-phonetics
// literature (Peterson & Barney vowel formants and standard references for
// the consonants). It is a starting approximation, replaceable wholesale via
// SetFormantTable when measured or extracted data becomes available.
static constexpr Ssi263PhonemeSpec  s_kPhonemes[Ssi263::kPhonemeCount] =
{
    {    0,    0,    0, false, false, 0.00f },   // 00 PA   (pause)
    {  270, 2290, 3010, true,  false, 1.00f },   // 01 E    meet
    {  400, 2000, 2550, true,  false, 1.00f },   // 02 E1   bent
    {  300, 2200, 3010, true,  false, 0.90f },   // 03 Y    before
    {  300, 2200, 2900, true,  false, 0.90f },   // 04 YI   year
    {  270, 2290, 3010, true,  false, 1.00f },   // 05 AY   please
    {  350, 2100, 2700, true,  false, 1.00f },   // 06 IE   any
    {  390, 1990, 2550, true,  false, 1.00f },   // 07 I    six
    {  480, 2100, 2700, true,  false, 1.00f },   // 08 A    made
    {  530, 1850, 2500, true,  false, 1.00f },   // 09 AI   care
    {  530, 1840, 2480, true,  false, 1.00f },   // 0A EH   nest
    {  550, 1770, 2490, true,  false, 1.00f },   // 0B EH1  belt
    {  660, 1720, 2410, true,  false, 1.00f },   // 0C AE   dad
    {  690, 1660, 2400, true,  false, 1.00f },   // 0D AE1  after
    {  640, 1190, 2390, true,  false, 1.00f },   // 0E AH   got
    {  730, 1090, 2440, true,  false, 1.00f },   // 0F AH1  father
    {  570,  840, 2410, true,  false, 1.00f },   // 10 AW   office
    {  500,  860, 2400, true,  false, 1.00f },   // 11 O    store
    {  450,  900, 2400, true,  false, 1.00f },   // 12 OU   boat
    {  440, 1020, 2240, true,  false, 1.00f },   // 13 OO   look
    {  300, 1700, 2200, true,  false, 0.90f },   // 14 IU   you
    {  430, 1100, 2250, true,  false, 0.90f },   // 15 IU1  could
    {  300,  870, 2240, true,  false, 1.00f },   // 16 U    tune
    {  320,  900, 2200, true,  false, 1.00f },   // 17 U1   cartoon
    {  640, 1190, 2390, true,  false, 0.90f },   // 18 UH   wonder
    {  620, 1200, 2400, true,  false, 0.90f },   // 19 UH1  love
    {  600, 1200, 2400, true,  false, 0.80f },   // 1A UH2  what
    {  640, 1190, 2390, true,  false, 0.90f },   // 1B UH3  nut
    {  490, 1350, 1690, true,  false, 1.00f },   // 1C ER   bird
    {  310, 1060, 1380, true,  false, 0.80f },   // 1D R    roof
    {  420, 1300, 1600, true,  false, 0.80f },   // 1E R1   rug
    {  400, 1200, 1500, true,  false, 0.70f },   // 1F R2   mutter
    {  360, 1300, 2700, true,  false, 0.80f },   // 20 L    lift
    {  380, 1400, 2700, true,  false, 0.80f },   // 21 L1   play
    {  450, 1000, 2600, true,  false, 0.70f },   // 22 LF   fall
    {  300,  610, 2200, true,  false, 0.80f },   // 23 W    water
    {  200,  900, 2100, true,  false, 0.50f },   // 24 B    bag
    {  300, 1700, 2600, true,  false, 0.50f },   // 25 D    paid
    {  300, 2000, 2500, true,  false, 0.50f },   // 26 KV   tag
    {  300,  900, 2100, false, true,  0.40f },   // 27 P    pen
    {  400, 1700, 2600, false, true,  0.40f },   // 28 T    tart
    {  350, 1900, 2500, false, true,  0.40f },   // 29 K    kit
    {  250, 1000, 2100, true,  false, 0.15f },   // 2A HV   (hold vocal)
    {  250, 1000, 2100, true,  false, 0.00f },   // 2B HVC  (hold vocal closure)
    {  500, 1500, 2500, false, true,  0.50f },   // 2C HF   heart
    {  500, 1500, 2500, false, true,  0.00f },   // 2D HFC  (hold fricative closure)
    {  250, 1200, 2200, true,  false, 0.20f },   // 2E HN   (hold nasal)
    {  300, 4300, 5600, true,  true,  0.50f },   // 2F Z    zero
    {  300, 4500, 6000, false, true,  0.50f },   // 30 S    same
    {  300, 2200, 3000, true,  true,  0.60f },   // 31 J    measure
    {  300, 2200, 3000, false, true,  0.55f },   // 32 SCH  ship
    {  250, 1400, 2400, true,  true,  0.50f },   // 33 V    very
    {  250, 1400, 2400, false, true,  0.40f },   // 34 F    four
    {  250, 1600, 2600, true,  true,  0.50f },   // 35 THV  there
    {  250, 1600, 2600, false, true,  0.35f },   // 36 TH   with
    {  250, 1000, 2200, true,  false, 0.70f },   // 37 M    more
    {  250, 1700, 2600, true,  false, 0.70f },   // 38 N    nine
    {  250, 2000, 2700, true,  false, 0.70f },   // 39 NG   rang
    {  530, 1840, 2480, true,  false, 1.00f },   // 3A :A   maerchen
    {  400, 1500, 2300, true,  false, 1.00f },   // 3B :OH  loewe
    {  270, 1850, 2100, true,  false, 1.00f },   // 3C :U   fuenf
    {  300, 1600, 2100, true,  false, 1.00f },   // 3D :UH  menu
    {  500, 1500, 2500, true,  false, 0.90f },   // 3E E2   bitte
    {  360,  900, 2400, true,  false, 0.80f },   // 3F LB   lube
};

// Synthesis constants: excitation gains, the spectral tilt on the glottal
// source, per-stage resonator bandwidths, and the output level that keeps a
// full-amplitude vowel inside the sample range after three resonators.
static constexpr float   s_kfVoicedGain   = 5.00f;
static constexpr float   s_kfNoiseGain    = 0.80f;

// Two-pole smoothing on the glottal impulse train. A bare impulse opened
// every pitch period with a step -- an audible click at the fundamental
// rate -- while a wide shaped pulse rolled off the upper formants and
// muffled the voice. Two cascaded one-pole sections start each pulse from
// zero (click-free) yet keep a -12 dB/oct tail that still excites F2/F3.
static constexpr float   s_kfSourcePole   = 0.15f;
static constexpr float   s_kfOutputGain   = 0.25f;
static constexpr double  s_kBandwidthHz[3] = { 60.0, 90.0, 120.0 };

// Amplitude envelope rates: a fast attack and a slightly longer release,
// expressed as time constants the per-sample coefficient is derived from.
static constexpr double  s_kAttackTauSec  = 0.002;
static constexpr double  s_kReleaseTauSec = 0.004;





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
    Byte   outgoing = Phoneme();



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
            m_envLevel      = 0.0f;
        }
    }
    else if (sel == kRegDurationPhoneme && !IsPoweredDown())
    {
        BeginPhoneme (outgoing);
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

    for (i = 0; i < 3; i++)
    {
        m_fCur[i]  = 0.0;
        m_resY1[i] = 0.0f;
        m_resY2[i] = 0.0f;
    }

    m_envLevel     = 0.0f;
    m_glottalPhase = 0.0;
    m_excLp1       = 0.0f;
    m_excLp2       = 0.0f;
    m_noiseLp      = 0.0f;
    m_lfsr         = 0xACE1u;
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
    // A finished phoneme is not silent until its release tail has decayed:
    // the envelope ramps the boundary instead of gating it, so the chip
    // stays audible for the few milliseconds the ramp needs. Idle and
    // unprogrammed chips have a zero envelope and stay free.
    return IsPoweredDown() || (Amplitude() == 0) ||
           (!m_sounding && m_envLevel < 0.001f);
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
//  GenerateSample
//
//  One mono sample of formant synthesis: an excitation source shaped by
//  three resonators whose centers glide toward the active phoneme's targets.
//  The filter-frequency register scales the whole tract relative to its
//  nominal clock, which is the datasheet's "voice type" adjustment; the
//  amplitude register scales the output.
//
//  Silent states pay nothing here -- the IsSilent early-out is the idle
//  fast-path that keeps an unprogrammed chip free.
//
////////////////////////////////////////////////////////////////////////////////

float Ssi263::GenerateSample()
{
    float    sample = 0.0f;
    float    target = 0.0f;
    double   tau    = 0.0;
    double   coef   = 0.0;
    double   scale  = 1.0;
    int      stage  = 0;



    if (IsSilent() || m_sampleRate == 0)
    {
        return 0.0f;
    }

    GlideFormants();

    scale = FilterFrequencyHz() / kNominalFilterHz;
    scale = std::clamp (scale, 0.5, 2.0);

    sample = Excitation();

    for (stage = 0; stage < 3; stage++)
    {
        sample = Resonate (stage, sample, m_fCur[stage] * scale);
    }

    // The amplitude envelope: eases toward the active level while sounding
    // and toward zero once the phoneme has finished, so every boundary is a
    // short ramp over the resonators' natural tail rather than a gate --
    // the linear amplitude transitioning the datasheet describes. Truncating
    // it instead put an audible click at every phoneme edge.
    target = m_sounding
                 ? ActiveSpec().level * (static_cast<float> (Amplitude()) / 15.0f)
                 : 0.0f;
    tau    = (target > m_envLevel) ? s_kAttackTauSec : s_kReleaseTauSec;
    coef   = 1.0 - std::exp (-1.0 / (tau * static_cast<double> (m_sampleRate)));

    m_envLevel += static_cast<float> (coef) * (target - m_envLevel);

    sample *= m_envLevel * s_kfOutputGain;

    return std::clamp (sample, -1.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFormantTable
//
//  Swaps the per-phoneme acoustic table. The synthesis reads targets through
//  this seam only, so better data -- measured from hardware or extracted from
//  the chip's ROM -- replaces the built-in approximation with no other change.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::SetFormantTable (const Ssi263PhonemeSpec * table)
{
    m_formants = table;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActiveSpec
//
////////////////////////////////////////////////////////////////////////////////

const Ssi263PhonemeSpec & Ssi263::ActiveSpec() const
{
    const Ssi263PhonemeSpec *   table = (m_formants != nullptr) ? m_formants : s_kPhonemes;



    return table[Phoneme()];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlideFormants
//
//  Eases the current formant centers toward the active phoneme's targets at
//  the articulation rate -- the linear transition to a new set of
//  characteristics the datasheet describes, with T2-T0 setting how fast.
//  A tract starting from silence snaps rather than sweeping up from zero.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::GlideFormants()
{
    const Ssi263PhonemeSpec &   spec = ActiveSpec();



    double   target[3] = { 0.0, 0.0, 0.0 };
    double   tauSec    = 0.0;
    double   coef      = 0.0;
    int      i         = 0;



    // A pause or closure has no formant targets of its own: the tract HOLDS
    // its position through it rather than sinking toward zero -- otherwise
    // the phoneme after every pause would sweep up from the basement and
    // every utterance would open on a spurious glide.
    if (spec.f1 == 0)
    {
        return;
    }

    target[0] = spec.f1;
    target[1] = spec.f2;
    target[2] = spec.f3;

    if (m_fCur[0] <= 0.0)
    {
        for (i = 0; i < 3; i++)
        {
            m_fCur[i] = target[i];
        }

        return;
    }

    // Articulation 0 is the slowest transition, 7 the fastest.
    tauSec = (8.0 - static_cast<double> (Articulation())) * 0.010;
    coef   = 1.0 - std::exp (-1.0 / (tauSec * static_cast<double> (m_sampleRate)));

    for (i = 0; i < 3; i++)
    {
        m_fCur[i] += coef * (target[i] - m_fCur[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Excitation
//
//  The source the resonators shape: a tilted glottal pulse train at the
//  inflection frequency for voiced phonemes, deterministic LFSR noise for
//  fricatives, both for voiced fricatives.
//
////////////////////////////////////////////////////////////////////////////////

float Ssi263::Excitation()
{
    const Ssi263PhonemeSpec &   spec = ActiveSpec();



    float    src     = 0.0f;
    float    impulse = 0.0f;
    double   pitch   = 0.0;
    Byte     bit     = 0;



    if (spec.voiced)
    {
        pitch = std::clamp (InflectionFrequencyHz(), 30.0, 400.0);

        m_glottalPhase += pitch / static_cast<double> (m_sampleRate);

        if (m_glottalPhase >= 1.0)
        {
            m_glottalPhase -= 1.0;
            impulse         = 1.0f;
        }

        // Two cascaded one-pole sections: the pulse rises from zero rather
        // than opening with a step, and its tail keeps enough energy at the
        // upper formants to excite them.
        m_excLp1 += s_kfSourcePole * (impulse - m_excLp1);
        m_excLp2 += s_kfSourcePole * (m_excLp1 - m_excLp2);

        src = m_excLp2 * s_kfVoicedGain;
    }

    if (spec.fricative)
    {
        bit    = static_cast<Byte> (m_lfsr & 1u);
        m_lfsr = m_lfsr >> 1;

        if (bit != 0)
        {
            m_lfsr ^= 0xB400u;
        }

        // One-pole smoothing: raw LFSR output swings rail to rail between
        // adjacent samples, which is a stream of clicks, not a hiss.
        m_noiseLp += 0.30f * (((bit != 0) ? 1.0f : -1.0f) - m_noiseLp);

        src += m_noiseLp * s_kfNoiseGain;
    }

    return src;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Resonate
//
//  One two-pole resonator stage, normalized for unit gain at DC so a
//  cascade passes energy below each center instead of attenuating it --
//  the classic formant-synthesis arrangement, and what makes three stages
//  in series workable. The stages are the "cascaded programmable low pass
//  filter sections" the datasheet describes, programmed here by the glided
//  formant centers.
//
////////////////////////////////////////////////////////////////////////////////

float Ssi263::Resonate (int stage, float input, double centerHz)
{
    double   fs = static_cast<double> (m_sampleRate);
    double   fc = std::clamp (centerHz, 50.0, fs * 0.45);
    double   r  = std::exp (-std::numbers::pi * s_kBandwidthHz[stage] / fs);
    double   b  = 2.0 * r * std::cos (2.0 * std::numbers::pi * fc / fs);
    double   c  = -(r * r);
    float    y  = 0.0f;



    y = static_cast<float> ((1.0 - b - c) * input +
                            b * m_resY1[stage] +
                            c * m_resY2[stage]);

    m_resY2[stage] = m_resY1[stage];
    m_resY1[stage] = y;

    return y;
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
//  Articulation glides the tract only between adjacent SPOKEN phonemes.
//  When the outgoing phoneme was silent -- a pause or a closure -- the
//  articulators re-set during it, so the new phoneme snaps to its own
//  targets rather than glide from wherever speech last left the tract:
//  gliding across silence opened every utterance with a spurious w-like
//  onglide from the previous word's final vowel.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263::BeginPhoneme (Byte outgoing)
{
    double                      seconds = PhonemeDurationSec();
    const Ssi263PhonemeSpec *   table   = (m_formants != nullptr) ? m_formants : s_kPhonemes;
    int                         i       = 0;



    if (table[outgoing & kPhonemeMask].level <= 0.0f)
    {
        for (i = 0; i < 3; i++)
        {
            m_fCur[i] = 0.0;   // GlideFormants snaps to the new targets
        }
    }

    m_phonemeCycles = seconds * m_clockHz;
    m_sounding      = (m_phonemeCycles > 0.0);
    m_request       = false;
}
