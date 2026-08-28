#include "Pch.h"

#include "Ssi263.h"

// Per-phoneme acoustic targets, indexed by phoneme code $00-$3F.
//
// Derived from the chip's own parameter ROM, read off the visual6502 die
// photographs of the SSI 263P. Each phoneme stores six 4-bit fields (F1, F2,
// F3 filter codes, vocal and fricative amplitudes, nasal coupling) plus
// voiced/fricative/closure flags; the raw extraction lives in the repo under
// specs. Filter codes become Hz through the SC-01A's measured capacitor
// network (the 2007 decap, as modeled by MAME): the two chips demonstrably
// share one code scale -- 22 of 46 name-matched phonemes carry identical
// formant codes -- so the predecessor's silicon-true DAC curves replace the
// earlier literature-fitted lines. Levels are max(VA, FA)/15; the two
// closure-hold phonemes keep level 0 because the closure ramp the hardware
// applies is not modeled here. Still replaceable wholesale via
// SetFormantTable when better-calibrated data arrives.
static constexpr Ssi263PhonemeSpec  s_kPhonemes[Ssi263::kPhonemeCount] =
{
    {    0,    0,    0, false, false, 0.00f },   // 00 PA   (pause)
    {  314, 2330, 2756, true,  false, 0.80f },   // 01 E    meet
    {  446, 2330, 2677, true,  false, 0.67f },   // 02 E1   bent
    {  256, 2257, 2756, true,  false, 0.73f },   // 03 Y    before
    {  314, 2174, 2511, true,  false, 0.40f },   // 04 YI   year
    {  365, 2330, 2756, true,  false, 0.80f },   // 05 AY   please
    {  256, 2408, 2832, true,  false, 0.60f },   // 06 IE   any
    {  446, 2007, 2598, true,  false, 0.53f },   // 07 I    six
    {  482, 2096, 2511, true,  false, 0.53f },   // 08 A    made
    {  482, 1922, 2425, true,  false, 0.53f },   // 09 AI   care
    {  577, 1823, 2511, true,  false, 0.53f },   // 0A EH   nest
    {  605, 1922, 2511, true,  false, 0.53f },   // 0B EH1  belt
    {  683, 1922, 2511, true,  false, 0.40f },   // 0C AE   dad
    {  731, 1730, 2511, true,  false, 0.40f },   // 0D AE1  after
    {  731, 1261, 2511, true,  false, 0.40f },   // 0E AH   got
    {  731, 1387, 2511, true,  false, 0.47f },   // 0F AH1  father
    {  683, 1106, 2425, true,  false, 0.40f },   // 10 AW   office
    {  516,  943, 2511, true,  false, 0.53f },   // 11 O    store
    {  446,  943, 2511, true,  false, 0.60f },   // 12 OU   boat
    {  546, 1106, 2425, true,  false, 0.53f },   // 13 OO   look
    {  365, 1620, 2425, true,  false, 0.67f },   // 14 IU   you
    {  406, 1387, 2425, true,  false, 0.60f },   // 15 IU1  could
    {  365,  943, 2244, true,  false, 0.67f },   // 16 U    tune
    {  256,  722, 2142, true,  false, 0.67f },   // 17 U1   cartoon
    {  546, 1387, 2511, true,  false, 0.67f },   // 18 UH   wonder
    {  605, 1261, 2511, true,  false, 0.53f },   // 19 UH1  love
    {  657, 1261, 2511, true,  false, 0.40f },   // 1A UH2  what
    {  657, 1514, 2511, true,  false, 0.47f },   // 1B UH3  nut
    {  482, 1387, 1696, true,  false, 0.53f },   // 1C ER   bird
    {  365,  943, 1424, true,  false, 0.53f },   // 1D R    roof
    {  314, 1261, 1822, true,  false, 0.53f },   // 1E R1   rug
    {  516, 1620, 2336, true,  false, 0.40f },   // 1F R2   mutter
    {  365, 1261, 2756, true,  false, 0.47f },   // 20 L    lift
    {  256, 1514, 2832, true,  false, 0.53f },   // 21 L1   play
    {  446,  943, 2756, true,  false, 0.60f },   // 22 LF   fall
    {  365,  722, 2336, true,  false, 0.53f },   // 23 W    water
    {  256, 1261, 2598, true,  false, 0.53f },   // 24 B    bag
    {  256, 1922, 2756, true,  false, 0.53f },   // 25 D    paid
    {  365, 2007, 2244, true,  false, 0.67f },   // 26 KV   tag
    {  406, 1106, 2244, false, true,  1.00f },   // 27 P    pen
    {  406, 1922, 2756, false, true,  1.00f },   // 28 T    tart
    {  365, 2007, 2244, false, true,  0.27f },   // 29 K    kit
    {  516, 1922, 2598, true,  false, 0.40f },   // 2A HV   (hold vocal)
    {  516, 1922, 2598, true,  false, 0.00f },   // 2B HVC  (hold vocal closure)
    {  516, 1922, 2598, false, true,  0.53f },   // 2C HF   heart
    {  516, 1922, 2598, false, true,  0.00f },   // 2D HFC  (hold fric closure)
    {  516, 1922, 2598, true,  false, 0.27f },   // 2E HN   (hold nasal)
    {  365, 1106, 2677, true,  true,  0.67f },   // 2F Z    zero
    {  176, 1730, 2598, false, true,  1.00f },   // 30 S    same
    {  314, 2096, 2756, true,  true,  0.67f },   // 31 J    measure
    {  314, 2096, 2756, false, true,  0.40f },   // 32 SCH  ship
    {  314, 1261, 2336, true,  true,  0.27f },   // 33 V    very
    {  314, 1261, 2336, false, true,  0.27f },   // 34 F    four
    {  365, 1730, 2756, true,  true,  0.13f },   // 35 THV  there
    {  446, 1823, 2425, false, true,  0.13f },   // 36 TH   with
    {  176, 1261, 2336, true,  false, 0.67f },   // 37 M    more
    {  176, 1823, 2677, true,  false, 0.53f },   // 38 N    nine
    {  314, 2174, 2756, true,  false, 0.27f },   // 39 NG   rang
    {  516, 1922, 2425, true,  false, 0.53f },   // 3A :A   maerchen
    {  314, 1823, 2336, true,  false, 0.40f },   // 3B :OH  loewe
    {  256, 1730, 2336, true,  false, 0.67f },   // 3C :U   fuenf
    {  176, 1922, 2425, true,  false, 0.67f },   // 3D :UH  menu
    {  482, 1730, 2425, true,  false, 0.47f },   // 3E E2   bitte
    {  256,  943, 2756, true,  false, 0.53f },   // 3F LB   lube
};

// Synthesis constants: excitation gains, the spectral tilt on the glottal
// source, per-stage resonator bandwidths, and the output level that keeps a
// full-amplitude vowel inside the sample range after three resonators.
// The voiced gain rides well above the noise gain because the radiation
// tilt at the output costs F1-dominant voiced energy 20+ dB while barely
// touching the fricatives' F2-region noise; these two together set the
// vowel-to-sibilant balance heard in connected speech.
static constexpr float   s_kfVoicedGain   = 60.00f;
static constexpr float   s_kfNoiseGain    = 0.085f;

// Output low-pass (one-pole, ~5.5 kHz at 44.1 kHz): rounds off the top
// octave the radiation tilt would otherwise push to Nyquist -- the digital
// sheen on fricatives -- standing in for the analog output stage between
// the chip and the card's mixer.
static constexpr float   s_kfOutputLpCoef = 0.54f;

// Two-pole smoothing on the glottal impulse train. A bare impulse opened
// every pitch period with a step -- an audible click at the fundamental
// rate -- while a wide shaped pulse rolled off the upper formants and
// muffled the voice. Two cascaded one-pole sections start each pulse from
// zero (click-free) yet keep a -12 dB/oct tail that still excites F2/F3.
// The pole sets where that tail starts: higher was audibly buzzy against
// the radiation tilt, lower muffles F2.
static constexpr float   s_kfSourcePole   = 0.10f;
static constexpr float   s_kfOutputGain   = 3.00f;
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
    m_radPrev      = 0.0f;
    m_outLp        = 0.0f;
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
    float    diffed = 0.0f;
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

    // Voiced excitation drives the whole tract from the glottis; frication
    // is front-cavity excitation and enters AFTER the first formant stage.
    // Injecting noise at the glottis instead sent it through the F1 low-pass,
    // which crushed sibilants into a quiet sub-1 kHz rumble.
    sample = Resonate (0, Excitation(), m_fCur[0] * scale);

    if (ActiveSpec().fricative)
    {
        sample += NoiseSample() * s_kfNoiseGain;
    }

    for (stage = 1; stage < 3; stage++)
    {
        sample = Resonate (stage, sample, m_fCur[stage] * scale);
    }

    // Radiation characteristic: the lips differentiate the volume flow,
    // a +6 dB/oct tilt. Without it the -12 dB/oct glottal source leaves
    // the cascade too dark -- F2/F3 sit far below F1 and every vowel
    // smears toward an "aw". The first difference is normalized by the
    // sample rate so its gain at a given frequency is host-rate-invariant.
    diffed    = (sample - m_radPrev) *
                (static_cast<float> (m_sampleRate) / 44100.0f);
    m_radPrev = sample;
    sample    = diffed;

    m_outLp  += s_kfOutputLpCoef * (sample - m_outLp);
    sample    = m_outLp;

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
//  The glottal source: a tilted pulse train at the inflection frequency for
//  voiced phonemes. Frication is generated separately by NoiseSample and
//  injected past the first formant stage.
//
////////////////////////////////////////////////////////////////////////////////

float Ssi263::Excitation()
{
    float    src     = 0.0f;
    float    impulse = 0.0f;
    double   pitch   = 0.0;



    if (ActiveSpec().voiced)
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

        // The radiation tilt at the output taxes low-F1 vowels roughly with
        // the square of F1; compensating one power of it here keeps close
        // vowels (OU, :OH) audible next to open ones (AH, AE) while leaving
        // the natural open-vowels-are-louder tendency in place. Glided F1 is
        // used so the correction moves smoothly through transitions.
        src *= static_cast<float> (731.0 / std::max (m_fCur[0], 170.0));
    }

    return src;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoiseSample
//
//  Deterministic LFSR noise for frication, one-pole smoothed: raw LFSR
//  output swings rail to rail between adjacent samples, which is a stream
//  of clicks, not a hiss. The coefficient sets the noise brightness.
//
////////////////////////////////////////////////////////////////////////////////

float Ssi263::NoiseSample()
{
    Byte   bit = static_cast<Byte> (m_lfsr & 1u);



    m_lfsr = m_lfsr >> 1;

    if (bit != 0)
    {
        m_lfsr ^= 0xB400u;
    }

    m_noiseLp += 0.65f * (((bit != 0) ? 1.0f : -1.0f) - m_noiseLp);

    return m_noiseLp;
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
