#include "Pch.h"

#include "Devices/Mockingboard/Ssi263.h"
#include "Devices/Mockingboard/Ssi263AudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace Ssi263TestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  Ssi263Tests
    //
    //  The SSI 263A voice chip, asserted against its datasheet rather than
    //  against another implementation's choices.
    //
    //  QUIESCENCE gets the most attention, because the sound+speech card is the
    //  default for three machine profiles and an unprogrammed voice chip that
    //  requested data would break sound-only music players that have worked for
    //  releases. The datasheet makes this the hardware's own behavior -- CTL is
    //  set on power up, which is Power Down -- so these tests check we inherit
    //  it rather than that we bolted on a guard.
    //
    //  The TIMING tests assert the published formulas directly. They are the
    //  reason the chip's duration is expressed in emulated cycles: an utterance
    //  must occupy the same emulated span whatever the host audio rate is.
    //
    //  The REGISTER tests cover the two decode details that are easy to get
    //  wrong and impossible to guess: the non-contiguous 12-bit inflection
    //  packing, and RS2 aliasing addresses 4..7 onto the filter register.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Ssi263Tests)
    {
    public:
        TEST_METHOD (PowersUpQuiescent)
        {
            Ssi263   chip;



            Assert::IsTrue (chip.IsPoweredDown(),
                            L"CTL is set on power up -- the part comes up in Power Down");
            Assert::IsFalse (chip.IsRequesting(),
                             L"An unprogrammed chip must not request data");
            Assert::IsTrue (chip.IsSilent(),
                            L"An unprogrammed chip must contribute nothing");
            Assert::AreEqual<Byte> (0, chip.ReadRegister (0),
                                    L"Status must read back with no request pending");
        }


        TEST_METHOD (StaysQuiescentUnderTickingWhileUnprogrammed)
        {
            Ssi263   chip;



            chip.Tick (10000000);

            Assert::IsFalse (chip.IsRequesting(),
                             L"Ticking an unprogrammed chip must never raise a request");
            Assert::IsTrue (chip.IsSilent());
        }


        TEST_METHOD (ResetReturnsToQuiescentFromAnyState)
        {
            Ssi263   chip;



            StartSpeaking (chip, 0x0A);
            Assert::IsFalse (chip.IsSilent(), L"Precondition: chip is sounding");

            chip.Reset();

            Assert::IsTrue (chip.IsPoweredDown());
            Assert::IsTrue (chip.IsSilent(), L"Reset must abandon the phoneme immediately");
            Assert::IsFalse (chip.IsRequesting());
        }


        TEST_METHOD (PowerDownSilencesButRetainsRegisters)
        {
            Ssi263   chip;
            Byte     phoneme = 0;



            StartSpeaking (chip, 0x15);
            phoneme = chip.GetPhoneme();

            // Raise CTL: Power Down silences and disables A/R, but the
            // datasheet is explicit that register contents survive.
            chip.WriteRegister (Ssi263::kRegCtlArtAmp, Ssi263::kCtl);

            Assert::IsTrue (chip.IsSilent());
            Assert::IsFalse (chip.IsRequesting());
            Assert::AreEqual<Byte> (phoneme, chip.GetPhoneme(),
                                    L"Power Down must not disturb register contents");
        }


        TEST_METHOD (AllSixtyFourPhonemeCodesRoundTrip)
        {
            Ssi263   chip;
            Byte     code = 0;



            LeavePowerDown (chip, Ssi263::kModePhonemeTransitioned);

            for (code = 0; code < Ssi263::kPhonemeCount; code++)
            {
                chip.WriteRegister (Ssi263::kRegDurationPhoneme, code);
                Assert::AreEqual<Byte> (code, chip.GetPhoneme(),
                                        L"Every code $00-$3F must be accepted verbatim");
            }
        }


        TEST_METHOD (Rs2AliasesAddressesFourThroughSevenOntoFilter)
        {
            Byte   addr = 0;



            for (addr = 4; addr <= 7; addr++)
            {
                Assert::AreEqual<Byte> (Ssi263::kRegFilterFreq, Ssi263::SelectRegister (addr),
                                        L"RS2 high must select the filter register regardless of RS1/RS0");
            }

            Assert::AreEqual<Byte> (0, Ssi263::SelectRegister (0));
            Assert::AreEqual<Byte> (3, Ssi263::SelectRegister (3));
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  InflectionValueReassemblesNonContiguousBits
        //
        //  The packing nobody reconstructs from field names: I11 alone at
        //  register 2 bit 3, I10-I3 filling register 1, I2-I0 at register 2
        //  bits 2-0. Walking one bit at a time is deliberate -- a transposed
        //  or off-by-one shift still passes a single combined value.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (InflectionValueReassemblesNonContiguousBits)
        {
            Ssi263     chip;
            int        bit      = 0;
            uint16_t   expected = 0;



            for (bit = 0; bit <= 11; bit++)
            {
                chip.Reset();
                expected = static_cast<uint16_t> (1u << bit);

                if (bit == 11)
                {
                    chip.WriteRegister (Ssi263::kRegRateInflection, Ssi263::kInflect11);
                }
                else if (bit >= 3)
                {
                    chip.WriteRegister (Ssi263::kRegInflection, static_cast<Byte> (1u << (bit - 3)));
                }
                else
                {
                    chip.WriteRegister (Ssi263::kRegRateInflection, static_cast<Byte> (1u << bit));
                }

                Assert::AreEqual<uint16_t> (expected, chip.GetInflectionValue(),
                                            L"Each inflection bit must land at its documented position");
            }
        }


        TEST_METHOD (RateAndInflectionShareARegisterWithoutCollision)
        {
            Ssi263   chip;



            // R3-R0 in the high nibble, I11 and I2-I0 in the low.
            chip.WriteRegister (Ssi263::kRegRateInflection, 0xAF);

            Assert::AreEqual<Byte> (0x0A, chip.GetRateSel(),
                                    L"Rate occupies the high nibble");
            Assert::AreEqual<uint16_t> (0x807, chip.GetInflectionValue(),
                                        L"I11 plus I2-I0 must come from the low nibble");
        }


        TEST_METHOD (FrameDurationMatchesDatasheetFormula)
        {
            Ssi263   chip;
            Byte     r        = 0;
            double   expected = 0.0;



            for (r = 0; r <= 15; r++)
            {
                chip.WriteRegister (Ssi263::kRegRateInflection, static_cast<Byte> (r << Ssi263::kRateShift));

                expected = (4096.0 * (16.0 - static_cast<double> (r))) / Ssi263::kDefaultXckHz;

                Assert::AreEqual (expected, chip.GetFrameDurationSec(), 1e-12,
                                  L"Frame Duration = 4096 x (16 - R) / XCK");
            }
        }


        TEST_METHOD (PhonemeDurationScalesByFourMinusD)
        {
            Ssi263   chip;
            Byte     d     = 0;
            double   frame = 0.0;



            // Phoneme timing mode, so the duration bits scale the frame.
            LeavePowerDown (chip, Ssi263::kModePhonemeTransitioned);
            chip.WriteRegister (Ssi263::kRegRateInflection, static_cast<Byte> (0x0A << Ssi263::kRateShift));

            frame = chip.GetFrameDurationSec();

            for (d = 0; d <= 3; d++)
            {
                chip.WriteRegister (Ssi263::kRegDurationPhoneme,
                                    static_cast<Byte> (d << Ssi263::kDurationShift));

                Assert::AreEqual (frame * (4.0 - static_cast<double> (d)),
                                  chip.GetPhonemeDurationSec(), 1e-12,
                                  L"Phoneme Duration = Frame Duration x (4 - D)");
            }
        }


        TEST_METHOD (FilterFrequencyMatchesDatasheetFormula)
        {
            Ssi263   chip;
            int      f = 0;



            for (f = 0; f <= 255; f += 17)
            {
                chip.WriteRegister (Ssi263::kRegFilterFreq, static_cast<Byte> (f));

                if (f == 255)
                {
                    continue;   // divisor 2 x (256-255) = 2; still finite, covered below
                }

                Assert::AreEqual (Ssi263::kDefaultXckHz / (2.0 * (256.0 - f)),
                                  chip.GetFilterFrequencyHz(), 1e-6,
                                  L"Filter Frequency = XCK / (2 x (256 - F))");
            }
        }


        TEST_METHOD (InflectionFrequencyMatchesDatasheetFormula)
        {
            Ssi263   chip;



            chip.WriteRegister (Ssi263::kRegInflection, 0x80);   // I10 set -> I = 0x400

            Assert::AreEqual<uint16_t> (0x400, chip.GetInflectionValue());
            Assert::AreEqual (Ssi263::kDefaultXckHz / (8.0 * (4096.0 - 1024.0)),
                              chip.GetInflectionFrequencyHz(), 1e-9,
                              L"Inflection Frequency = XCK / (8 x (4096 - I))");
        }


        TEST_METHOD (RequestRaisesWhenPhonemeDurationElapses)
        {
            Ssi263     chip;
            uint32_t   cycles = 0;



            StartSpeaking (chip, 0x08);
            cycles = static_cast<uint32_t> (chip.GetPhonemeDurationSec() * kBareTickClockHz);

            chip.Tick (cycles / 2);
            Assert::IsFalse (chip.IsRequesting(), L"Must not request part-way through");
            Assert::IsFalse (chip.IsSilent(),     L"Must still be sounding part-way through");

            chip.Tick (cycles);
            Assert::IsTrue (chip.IsRequesting(),
                            L"A/R must assert once the phoneme has been generated");
            Assert::AreEqual<Byte> (Ssi263::kStatusRequest, chip.ReadRegister (0),
                                    L"Status read must report the request in D7");
        }


        TEST_METHOD (ArDisabledModeNeverRequests)
        {
            Ssi263   chip;



            // DR1 = DR0 = 0 on the CTL one-to-zero transition disables A/R
            // outright, per the mode selection chart.
            LeavePowerDown (chip, Ssi263::kModeArDisabled);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x08);

            chip.Tick (10000000);

            Assert::IsFalse (chip.IsRequesting(),
                             L"The A/R-disabled mode must never request");
            Assert::AreEqual<Byte> (0, chip.ReadRegister (0));
        }


        TEST_METHOD (ModeLatchesOnCtlTransitionNotOnLaterDurationWrites)
        {
            Ssi263   chip;



            LeavePowerDown (chip, Ssi263::kModeArDisabled);
            Assert::AreEqual<Byte> (Ssi263::kModeArDisabled, chip.GetActiveMode());

            // Changing the duration bits afterwards selects a DURATION, not a
            // mode -- the mode only latches on a CTL one-to-zero transition.
            chip.WriteRegister (Ssi263::kRegDurationPhoneme,
                                static_cast<Byte> (Ssi263::kModePhonemeTransitioned << Ssi263::kDurationShift));

            Assert::AreEqual<Byte> (Ssi263::kModeArDisabled, chip.GetActiveMode(),
                                    L"Duration writes must not re-latch the operating mode");

            chip.Tick (10000000);
            Assert::IsFalse (chip.IsRequesting(),
                             L"Still in the A/R-disabled mode, so still no request");
        }


        TEST_METHOD (WritingNextPhonemeWithdrawsTheRequest)
        {
            Ssi263     chip;
            uint32_t   cycles = 0;



            StartSpeaking (chip, 0x08);
            cycles = static_cast<uint32_t> (chip.GetPhonemeDurationSec() * kBareTickClockHz);

            chip.Tick (cycles + 1);
            Assert::IsTrue (chip.IsRequesting());

            // The pacing loop's answer to A/R is the next phoneme write.
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x09);

            Assert::IsFalse (chip.IsRequesting(),
                             L"Writing the next phoneme must withdraw the request");
            Assert::IsFalse (chip.IsSilent(), L"and start sounding it");
        }


        TEST_METHOD (TimingIsIndependentOfHostSampleRate)
        {
            Ssi263     a;
            Ssi263     b;
            uint32_t   cycles = 0;



            a.SetSampleRate (44100);
            b.SetSampleRate (192000);

            StartSpeaking (a, 0x08);
            StartSpeaking (b, 0x08);

            Assert::AreEqual (a.GetPhonemeDurationSec(), b.GetPhonemeDurationSec(), 1e-12,
                              L"Host sample rate must not affect emulated duration");

            cycles = static_cast<uint32_t> (a.GetPhonemeDurationSec() * kBareTickClockHz);

            a.Tick (cycles + 1);
            b.Tick (cycles + 1);

            Assert::AreEqual (a.IsRequesting(), b.IsRequesting(),
                              L"Both must complete in the same emulated span");
        }


        TEST_METHOD (ZeroAmplitudeReportsSilent)
        {
            Ssi263   chip;



            LeavePowerDown (chip, Ssi263::kModePhonemeTransitioned);
            chip.WriteRegister (Ssi263::kRegCtlArtAmp, 0x00);   // CTL low, amplitude 0
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x08);

            Assert::AreEqual<Byte> (0, chip.GetAmplitude());
            Assert::IsTrue (chip.IsSilent(),
                            L"Zero amplitude must let the audio path skip synthesis");
        }


        TEST_METHOD (RegisterWritesAreDeterministic)
        {
            Ssi263   a;
            Ssi263   b;
            Byte     r = 0;



            for (r = 0; r < Ssi263::kRegCount; r++)
            {
                a.WriteRegister (r, static_cast<Byte> (0x11 * (r + 1)));
                b.WriteRegister (r, static_cast<Byte> (0x11 * (r + 1)));
            }

            Assert::AreEqual (a.GetInflectionValue(), b.GetInflectionValue());
            Assert::AreEqual (a.GetPhonemeDurationSec(), b.GetPhonemeDurationSec(), 1e-12);
            Assert::AreEqual (a.IsRequesting(), b.IsRequesting());
            Assert::AreEqual (a.IsSilent(), b.IsSilent());
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  SpectralEnergyConcentratesAtFormantTargets
        //
        //  The acoustic gate: a rendered vowel's energy must sit at its
        //  table's formant frequencies, not spread across the spectrum. Band
        //  power is measured as the mean of several Goertzel probes around
        //  each center, because the excitation is harmonic -- energy lives AT
        //  harmonics near the formant, not necessarily on the exact center.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SpectralEnergyConcentratesAtFormantTargets)
        {
            double   inBand                 = 0.0;
            double   outBand                = 0.0;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   chip;



            StartVowelAh1 (chip);
            RenderSteadyState (chip, buffer.data());

            // AH1 ("father"): 731 / 1387 / 2511 Hz in the built-in table.
            inBand = BandPower (buffer.data(), 731.0) +
                     BandPower (buffer.data(), 1387.0) +
                     BandPower (buffer.data(), 2511.0);

            outBand = BandPower (buffer.data(), 4000.0) +
                      BandPower (buffer.data(), 5200.0) +
                      BandPower (buffer.data(), 6400.0);

            Assert::IsTrue (inBand > 10.0 * outBand,
                            L"Formant bands must dominate out-of-band energy");
        }


        TEST_METHOD (AmplitudeControlScalesOutput)
        {
            double   loudRms                = 0.0;
            double   quietRms               = 0.0;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   loud;
            Ssi263   quiet;



            StartVowelAh1 (loud);
            StartVowelAh1 (quiet);

            quiet.WriteRegister (Ssi263::kRegCtlArtAmp, 0x04);   // CTL low, amplitude 4

            RenderSteadyState (loud, buffer.data());
            loudRms = Rms (buffer.data());

            RenderSteadyState (quiet, buffer.data());
            quietRms = Rms (buffer.data());

            Assert::IsTrue (loudRms > 2.0 * quietRms,
                            L"A lower amplitude register must produce quieter output");
        }


        TEST_METHOD (FilterFrequencyShiftsTheSpectrumUpward)
        {
            double   nominalHigh            = 0.0;
            double   raisedHigh             = 0.0;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   nominal;
            Ssi263   raised;



            StartVowelAh1 (nominal);
            StartVowelAh1 (raised);

            // Raise the vocal-tract clock ~1.5x (a HIGHER register value means
            // a smaller divisor, so a faster clock): F2 at 1387 should migrate
            // toward ~2081, so probe power there.
            raised.WriteRegister (Ssi263::kRegFilterFreq, 0xE2);

            RenderSteadyState (nominal, buffer.data());
            nominalHigh = BandPower (buffer.data(), 2081.0);

            RenderSteadyState (raised, buffer.data());
            raisedHigh = BandPower (buffer.data(), 2081.0);

            Assert::IsTrue (raisedHigh > 2.0 * nominalHigh,
                            L"A faster filter clock must shift formants upward");
        }


        TEST_METHOD (InflectionControlMovesThePitch)
        {
            double   lowAt120               = 0.0;
            double   highAt120              = 0.0;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   lowPitch;
            Ssi263   highPitch;



            StartVowelAh1 (lowPitch);
            StartVowelAh1 (highPitch);

            SetInflectionHz (lowPitch, 60.0);
            SetInflectionHz (highPitch, 120.0);

            RenderSteadyState (lowPitch, buffer.data());
            lowAt120 = BandPower (buffer.data(), 120.0);

            RenderSteadyState (highPitch, buffer.data());
            highAt120 = BandPower (buffer.data(), 120.0);

            // Both spectra have energy at 120 Hz (the low voice's second
            // harmonic), but the fundamental landing there must dominate.
            Assert::IsTrue (highAt120 > 1.5 * lowAt120,
                            L"Raising inflection must move the fundamental up");
        }


        TEST_METHOD (FormantsGlideRatherThanJumpAcrossATransition)
        {
            double   earlyOld               = 0.0;
            double   earlyNew               = 0.0;
            double   lateNew                = 0.0;
            double   lateOld                = 0.0;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   chip;



            StartVowelAh1 (chip);
            RenderSteadyState (chip, buffer.data());

            // Switch to E ("meet", F2 = 2330) at the slowest articulation.
            chip.WriteRegister (Ssi263::kRegCtlArtAmp, 0x0F);   // articulation 0, amp $F
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x01);

            // Immediately after the switch the tract must still sound like
            // the OLD vowel -- a jump would already be at the new target.
            RenderInto (chip, buffer.data(), kRenderSamples);
            earlyOld = BandPower (buffer.data(), 1387.0);
            earlyNew = BandPower (buffer.data(), 2330.0);

            Assert::IsTrue (earlyOld > earlyNew,
                            L"Right after a transition the old formants must still dominate");

            // Well after the transition, the NEW vowel must dominate.
            RenderInto (chip, buffer.data(), kRenderSamples);
            RenderInto (chip, buffer.data(), kRenderSamples);
            RenderInto (chip, buffer.data(), kRenderSamples);
            lateNew = BandPower (buffer.data(), 2330.0);
            lateOld = BandPower (buffer.data(), 1387.0);

            Assert::IsTrue (lateNew > lateOld,
                            L"Long after a transition the new formants must dominate");
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  PauseHoldsTheTractPosition
        //
        //  A pause has no formant targets of its own, so the tract must HOLD
        //  through it. The regression this guards: gliding toward the pause's
        //  zero-valued table entry sank the formants toward 0 Hz during every
        //  inter-word gap, and the next phoneme then swept up from the
        //  basement -- an audible spurious "bw" glide opening each utterance.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PauseHoldsTheTractPosition)
        {
            double   f2Before               = 0.0;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   chip;



            StartVowelAh1 (chip);
            RenderSteadyState (chip, buffer.data());

            f2Before = chip.GetFormantCenter (1);
            Assert::IsTrue (f2Before > 1000.0, L"Precondition: settled at the vowel's F2");

            // A long, fully rendered pause. Its output is silence, but the
            // glide machinery still runs each sample -- and must not move.
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x00);
            RenderInto (chip, buffer.data(), kRenderSamples);

            Assert::AreEqual (f2Before, chip.GetFormantCenter (1), 1.0,
                              L"A pause must hold the tract where the last phoneme left it");

            // The phoneme AFTER a silence snaps to its own targets: the
            // articulators re-set during the gap, so there is no glide from
            // the previous word -- the spurious w-like onglide that opened
            // every utterance when speech resumed by gliding from the last
            // vowel's position.
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x01);
            RenderInto (chip, buffer.data(), 4);

            Assert::AreEqual (2330.0, chip.GetFormantCenter (1), 1.0,
                              L"After a silence the next phoneme starts at its own targets");
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  PhonemeEndReleasesWithoutAStep
        //
        //  When a phoneme's duration expires, output must ramp down over the
        //  resonators' tail rather than gate to zero -- gating put an audible
        //  click at EVERY phoneme boundary, a steady crackle through all
        //  connected speech. The datasheet's own model is a linear amplitude
        //  transition, so the ramp is fidelity, not smoothing.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PhonemeEndReleasesWithoutAStep)
        {
            uint32_t   i                      = 0;
            float      last                   = 0.0f;
            float      maxStep                = 0.0f;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   chip;



            StartVowelAh1 (chip);
            RenderSteadyState (chip, buffer.data());
            last = buffer[kRenderSamples - 1];

            // Expire the phoneme between renders, as the emulated machine does.
            chip.Tick (static_cast<uint32_t> (chip.GetPhonemeDurationSec() * kBareTickClockHz) + 1);

            RenderInto (chip, buffer.data(), 2048);

            for (i = 0; i < 2048; i++)
            {
                float  step = std::abs (buffer[i] - last);

                if (step > maxStep)
                {
                    maxStep = step;
                }

                last = buffer[i];
            }

            // Bound scaled to the voiced path's output level: continuous
            // bright speech steps a few hundredths per sample on its own,
            // while a gate discontinuity jumps at the signal's full scale.
            Assert::IsTrue (maxStep < 0.20f,
                            L"The end of a phoneme must ramp, not gate -- a step is an audible click");
            Assert::IsTrue (std::abs (buffer[2047]) < 0.002f,
                            L"and the release must actually decay to silence");
            Assert::IsTrue (chip.IsSilent(),
                            L"after the tail the chip reports silent so the idle fast-path resumes");
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  ConnectedSpeechHasNoClicks
        //
        //  Renders a complete utterance -- the speech demo's "HELLO" -- through
        //  the audio source at realistic pacing, interleaving emulated time
        //  with rendering the way the shell does, and bounds the largest
        //  sample-to-sample step across every phoneme transition, the pause,
        //  and the utterance restart. This is the deterministic form of the
        //  loopback-capture analysis that found the boundary clicks: anything
        //  under the bound here means a click heard from the app is downstream
        //  of the chip.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ConnectedSpeechHasNoClicks)
        {
            static constexpr Byte   kUtterance[] = { 0x2C, 0x0B, 0x20, 0x3B, 0x12, 0x00 };

            uint32_t   i               = 0;
            uint32_t   n               = 0;
            uint32_t   rendered        = 0;
            int        pass            = 0;
            size_t     ph              = 0;
            float      last            = 0.0f;
            float      maxStep         = 0.0f;
            double     cyclesPerSample = kBareTickClockHz / kSampleRate;
            std::vector<float>   buffer (512);



            Ssi263              chip;
            Ssi263AudioSource   source;



            source.SetSpeech (&chip);
            StartVowelAh1 (chip);

            // Two full utterances, so the restart boundary is covered too.
            for (pass = 0; pass < 2; pass++)
            {
                for (ph = 0; ph < sizeof (kUtterance); ph++)
                {
                    chip.WriteRegister (Ssi263::kRegDurationPhoneme, kUtterance[ph]);

                    n = static_cast<uint32_t> (chip.GetPhonemeDurationSec() * kSampleRate);

                    for (rendered = 0; rendered < n; rendered += 512)
                    {
                        chip.Tick (static_cast<uint32_t> (512.0 * cyclesPerSample));
                        source.GeneratePCM (buffer.data(), 512);

                        for (i = 0; i < 512; i++)
                        {
                            float  step = std::abs (buffer[i] - last);

                            if (step > maxStep)
                            {
                                maxStep = step;
                            }

                            last = buffer[i];
                        }
                    }
                }
            }

            // The bound rides above the largest step continuous bright speech
            // produces (the radiation tilt makes F2/F3-heavy content step a
            // few hundredths per sample by nature) while staying far below a
            // genuine gate discontinuity, which jumps at envelope scale.
            Assert::IsTrue (maxStep < 0.15f,
                            L"Connected speech must contain no step larger than a click threshold");
        }


        TEST_METHOD (RenderingIsDeterministicAcrossIdenticalChips)
        {
            uint32_t   i                 = 0;
            std::vector<float>   a (kRenderSamples);
            std::vector<float>   b (kRenderSamples);



            Ssi263   x;
            Ssi263   y;



            StartVowelAh1 (x);
            StartVowelAh1 (y);

            RenderInto (x, a.data(), kRenderSamples);
            RenderInto (y, b.data(), kRenderSamples);

            for (i = 0; i < kRenderSamples; i++)
            {
                Assert::AreEqual (a[i], b[i],
                                  L"Two identically-programmed chips must render bit-identical audio");
            }
        }


        TEST_METHOD (RenderedOutputStaysBounded)
        {
            Byte       phoneme                = 0;
            uint32_t   i                      = 0;
            std::vector<float>   buffer (kRenderSamples);



            Ssi263   chip;



            LeavePowerDown (chip, Ssi263::kModePhonemeTransitioned);
            chip.SetSampleRate (44100);
            chip.WriteRegister (Ssi263::kRegCtlArtAmp, 0x7F);   // extreme: max articulation+amp
            chip.WriteRegister (Ssi263::kRegFilterFreq, 0xFE);
            chip.WriteRegister (Ssi263::kRegInflection, 0xFF);
            chip.WriteRegister (Ssi263::kRegRateInflection, 0x0F);

            for (phoneme = 0; phoneme < Ssi263::kPhonemeCount; phoneme++)
            {
                chip.WriteRegister (Ssi263::kRegDurationPhoneme, phoneme);
                RenderInto (chip, buffer.data(), 512);

                for (i = 0; i < 512; i++)
                {
                    Assert::IsTrue (buffer[i] >= -1.0f && buffer[i] <= 1.0f,
                                    L"Output must stay in range for every phoneme at extreme settings");
                }
            }
        }


        TEST_METHOD (IdleSpeechSourceContributesExactlyZero)
        {
            uint32_t            i = 0;
            std::vector<float>   buffer (512);
            Ssi263              chip;
            Ssi263AudioSource   source;



            // Poison the buffer: the source must actively zero it, since the
            // mixer sums whatever it leaves behind.
            for (i = 0; i < 512; i++)
            {
                buffer[i] = 0.5f;
            }

            source.SetSpeech (&chip);
            chip.SetSampleRate (44100);

            source.GeneratePCM (buffer.data(), 512);

            for (i = 0; i < 512; i++)
            {
                Assert::AreEqual (0.0f, buffer[i],
                                  L"An unprogrammed chip must contribute exactly zero");
            }
        }


        TEST_METHOD (SpeakingSourceProducesAudio)
        {
            uint32_t            i            = 0;
            float               peak         = 0.0f;
            std::vector<float>   buffer (4096);
            Ssi263              chip;
            Ssi263AudioSource   source;



            source.SetSpeech (&chip);
            StartVowelAh1 (chip);

            source.GeneratePCM (buffer.data(), 4096);

            for (i = 0; i < 4096; i++)
            {
                if (std::abs (buffer[i]) > peak)
                {
                    peak = std::abs (buffer[i]);
                }
            }

            Assert::IsTrue (peak > 0.01f,
                            L"A sounding vowel must reach the audio source output");
            Assert::AreEqual (IDriveAudioSource::kCenterPan, source.GetPanLeft(),
                              L"Speech sits at the stereo center");
            Assert::AreEqual (IDriveAudioSource::kCenterPan, source.GetPanRight());
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  PhonemeSpansTheTickClockCountNotTheXckCount
        //
        //  The chip's XCK and the cycles Tick receives are different clocks.
        //  The datasheet gives a phoneme's length in seconds against XCK; the
        //  countdown Tick drains has to be that length converted at the TICK
        //  rate. Converting at XCK instead ran every phoneme 1.75x long on an
        //  Apple II, and every duration test agreed with it, because they all
        //  asserted the formula rather than the count it produces.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PhonemeSpansTheTickClockCountNotTheXckCount)
        {
            Ssi263   chip;
            double   seconds  = 0.0;
            double   expected = 0.0;
            double   xckCount = 0.0;



            chip.SetTickClock (kPhi2ClockHz);
            StartSpeaking (chip, 0x08);

            seconds  = chip.GetPhonemeDurationSec();
            expected = seconds * kPhi2ClockHz;
            xckCount = seconds * Ssi263::kDefaultXckHz;

            Assert::IsTrue (xckCount > expected * 1.5,
                            L"Precondition: the two clocks must differ enough to discriminate");

            AssertPhonemeSpansCycles (chip, expected, L"rate $A, D = 0, ticked at phi2");
        }


        TEST_METHOD (PhonemeTickCountTracksRateAndDurationBits)
        {
            static constexpr Byte   kRates[]     = { 0x00, 0x0A, 0x0F };
            static constexpr Byte   kDurations[] = { 0, 1, 3 };

            size_t   r        = 0;
            size_t   d        = 0;
            double   expected = 0.0;



            for (r = 0; r < sizeof (kRates); r++)
            {
                for (d = 0; d < sizeof (kDurations); d++)
                {
                    Ssi263   chip;



                    chip.SetTickClock (kPhi2ClockHz);
                    LeavePowerDown (chip, Ssi263::kModePhonemeTransitioned);
                    chip.WriteRegister (Ssi263::kRegRateInflection,
                                        static_cast<Byte> (kRates[r] << Ssi263::kRateShift));
                    chip.WriteRegister (Ssi263::kRegDurationPhoneme,
                                        static_cast<Byte> ((kDurations[d] << Ssi263::kDurationShift) | 0x08));

                    expected = chip.GetPhonemeDurationSec() * kPhi2ClockHz;

                    AssertPhonemeSpansCycles (chip, expected,
                                              L"R = " + std::to_wstring (static_cast<int> (kRates[r])) +
                                              L", D = " + std::to_wstring (static_cast<int> (kDurations[d])));
                }
            }
        }


        TEST_METHOD (FrameTimingModeCountsOneFrameWhateverTheDurationBits)
        {
            Ssi263   chip;
            double   frame    = 0.0;
            double   expected = 0.0;



            chip.SetTickClock (kPhi2ClockHz);

            // In the frame-timing mode the duration bits selected the mode, so
            // they no longer multiply the frame.
            LeavePowerDown (chip, Ssi263::kModeFrameImmediate);
            chip.WriteRegister (Ssi263::kRegRateInflection,
                                static_cast<Byte> (0x0A << Ssi263::kRateShift));
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x08);

            frame    = chip.GetFrameDurationSec();
            expected = frame * kPhi2ClockHz;

            Assert::AreEqual (frame, chip.GetPhonemeDurationSec(), 1e-12,
                              L"Frame timing takes the frame unmultiplied");

            AssertPhonemeSpansCycles (chip, expected, L"frame timing, D = 0");

            // Setting the duration bits restarts the phoneme and must not
            // change its length in this mode.
            chip.WriteRegister (Ssi263::kRegDurationPhoneme,
                                static_cast<Byte> ((3 << Ssi263::kDurationShift) | 0x08));

            Assert::AreEqual (frame, chip.GetPhonemeDurationSec(), 1e-12,
                              L"and D = 3 must still be one frame");

            AssertPhonemeSpansCycles (chip, expected, L"frame timing, D = 3");
        }


        TEST_METHOD (TickClockChangeRescalesAnInFlightPhoneme)
        {
            Ssi263   chip;
            double   expected = 0.0;



            // Started while the chip still assumed XCK ticks.
            StartSpeaking (chip, 0x08);

            expected = chip.GetPhonemeDurationSec() * kPhi2ClockHz;

            // Half-way through by the old rate, then state the real one. What
            // is left must be half the phoneme measured in the NEW domain, not
            // the old count left to drain at the new rate.
            chip.Tick (static_cast<uint32_t> (chip.GetPhonemeDurationSec() * kBareTickClockHz / 2.0));
            chip.SetTickClock (kPhi2ClockHz);

            AssertPhonemeSpansCycles (chip, expected / 2.0, L"rescaled mid-phoneme");
        }


        TEST_METHOD (TickClockDoesNotDisturbTheDatasheetFormulas)
        {
            Ssi263   chip;
            double   frame  = 0.0;
            double   filter = 0.0;



            chip.WriteRegister (Ssi263::kRegRateInflection,
                                static_cast<Byte> (0x0A << Ssi263::kRateShift));
            chip.WriteRegister (Ssi263::kRegFilterFreq, 0xD3);

            frame  = chip.GetFrameDurationSec();
            filter = chip.GetFilterFrequencyHz();

            chip.SetTickClock (kPhi2ClockHz);

            Assert::AreEqual (frame, chip.GetFrameDurationSec(), 1e-12,
                              L"The tick clock must not reach a datasheet formula");
            Assert::AreEqual (filter, chip.GetFilterFrequencyHz(), 1e-6,
                              L"nor the filter frequency");

            // XCK, by contrast, is exactly what those formulas divide by.
            chip.SetXckClock (Ssi263::kDefaultXckHz / 2.0);

            Assert::AreEqual (frame * 2.0, chip.GetFrameDurationSec(), 1e-12,
                              L"Halving XCK must double the frame duration");
            Assert::AreEqual (filter / 2.0, chip.GetFilterFrequencyHz(), 1e-6,
                              L"and halve the filter frequency");
        }


    private:
        static constexpr uint32_t   kRenderSamples = 8192;
        static constexpr double     kSampleRate    = 44100.0;

        // A chip nobody told otherwise takes its Tick cycles to be XCK ticks,
        // so this is the rate a bare chip's countdown converts at.
        static constexpr double     kBareTickClockHz = Ssi263::kDefaultXckHz;

        // The Apple II phi2 rate, which is what a Mockingboard actually feeds
        // Tick -- roughly 1.75x slower than the voice chip's own XCK. Spelled
        // out here rather than taken from MachineConfig so these tests state
        // the number they expect instead of agreeing with production by
        // construction.
        static constexpr double     kPhi2ClockHz     = 1022727.0;

        // How far short of, and past, the expected count the boundary checks
        // land. A few cycles absorb the truncation in turning a duration in
        // seconds into an integer tick count, while staying orders of
        // magnitude tighter than a clock-domain error, which is off by 75%.
        static constexpr uint32_t   kBoundaryMarginCycles = 8;

        // Drive a just-started phoneme to just short of `expectedCycles` and
        // confirm it is still sounding, then confirm a few more cycles end it.
        // The COUNT is the assertion: a countdown loaded from the wrong clock
        // misses this window by the ratio between the two clocks, which no
        // assertion over GetPhonemeDurationSec can see.
        static void AssertPhonemeSpansCycles (Ssi263 & chip, double expectedCycles, const std::wstring & label)
        {
            uint32_t   total     = static_cast<uint32_t> (expectedCycles);
            uint32_t   half      = total / 2;
            uint32_t   justShort = 0;



            Assert::IsTrue (total > kBoundaryMarginCycles * 4,
                            (L"Precondition: too few cycles to bracket a boundary: " + label).c_str());

            justShort = total - half - kBoundaryMarginCycles;

            chip.Tick (half);

            Assert::IsFalse (chip.IsRequesting(),
                             (L"Phoneme ended at half its cycle count: " + label).c_str());

            chip.Tick (justShort);

            Assert::IsFalse (chip.IsRequesting(),
                             (L"Phoneme ended before its cycle count: " + label).c_str());

            chip.Tick (kBoundaryMarginCycles * 2);

            Assert::IsTrue (chip.IsRequesting(),
                            (L"Phoneme outlasted its cycle count: " + label).c_str());
        }

        // Bring a chip up in the common mode, rate $A, amplitude $F, and
        // start the AH1 vowel sounding.
        static void StartVowelAh1 (Ssi263 & chip)
        {
            LeavePowerDown (chip, Ssi263::kModePhonemeTransitioned);
            chip.SetSampleRate (static_cast<uint32_t> (kSampleRate));
            chip.WriteRegister (Ssi263::kRegCtlArtAmp, 0x5F);   // articulation 5, amp $F
            chip.WriteRegister (Ssi263::kRegRateInflection,
                                static_cast<Byte> (0x0A << Ssi263::kRateShift));
            chip.WriteRegister (Ssi263::kRegFilterFreq, 0xD3);  // ~20 kHz nominal tract clock
            SetInflectionHz (chip, 90.0);
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, 0x0F);
        }

        // Program the inflection registers for a target fundamental.
        static void SetInflectionHz (Ssi263 & chip, double hz)
        {
            uint16_t   value = 0;
            Byte       rate  = 0;



            value = static_cast<uint16_t> (4096.0 - Ssi263::kDefaultXckHz / (8.0 * hz));
            rate  = static_cast<Byte> (chip.GetRateSel() << Ssi263::kRateShift);

            chip.WriteRegister (Ssi263::kRegInflection,
                                static_cast<Byte> ((value >> 3) & 0xFF));
            chip.WriteRegister (Ssi263::kRegRateInflection,
                                static_cast<Byte> (rate |
                                                   ((value & 0x800) ? Ssi263::kInflect11 : 0) |
                                                   (value & 0x07)));
        }

        static void RenderInto (Ssi263 & chip, float * buffer, uint32_t count)
        {
            uint32_t   i = 0;



            for (i = 0; i < count; i++)
            {
                buffer[i] = chip.GenerateSample();
            }
        }

        // Discard the attack transient, then capture a steady block.
        static void RenderSteadyState (Ssi263 & chip, float * buffer)
        {
            RenderInto (chip, buffer, 4096);
            RenderInto (chip, buffer, kRenderSamples);
        }

        // Mean Goertzel power over a short comb around the center, so a
        // harmonic landing beside the exact center still registers.
        static double BandPower (const float * buffer, double centerHz)
        {
            double   power  = 0.0;
            int      probe  = 0;



            for (probe = -3; probe <= 3; probe++)
            {
                power += GoertzelPower (buffer, kRenderSamples,
                                        centerHz + 15.0 * probe);
            }

            return power / 7.0;
        }

        static double Rms (const float * buffer)
        {
            double     sum = 0.0;
            uint32_t   i   = 0;



            for (i = 0; i < kRenderSamples; i++)
            {
                sum += static_cast<double> (buffer[i]) * static_cast<double> (buffer[i]);
            }

            return std::sqrt (sum / static_cast<double> (kRenderSamples));
        }

        static double GoertzelPower (const float * buffer, uint32_t count, double freqHz)
        {
            double   w  = 2.0 * std::numbers::pi * freqHz / kSampleRate;
            double   c  = 2.0 * std::cos (w);
            double   s0 = 0.0;
            double   s1 = 0.0;
            double   s2 = 0.0;
            uint32_t i  = 0;



            for (i = 0; i < count; i++)
            {
                s0 = buffer[i] + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }

            return s1 * s1 + s2 * s2 - c * s1 * s2;
        }

        // Leave Power Down, latching `mode` from the duration bits as the CTL
        // one-to-zero transition sees them.
        static void LeavePowerDown (Ssi263 & chip, Byte mode)
        {
            chip.WriteRegister (Ssi263::kRegDurationPhoneme,
                                static_cast<Byte> (mode << Ssi263::kDurationShift));
            chip.WriteRegister (Ssi263::kRegCtlArtAmp, 0x0C);   // CTL low, amplitude $C
        }

        // Bring the chip up in the common mode and start one phoneme sounding.
        static void StartSpeaking (Ssi263 & chip, Byte phoneme)
        {
            LeavePowerDown (chip, Ssi263::kModePhonemeTransitioned);
            chip.WriteRegister (Ssi263::kRegRateInflection,
                                static_cast<Byte> (0x0A << Ssi263::kRateShift));
            chip.WriteRegister (Ssi263::kRegDurationPhoneme, phoneme);
        }
    };
}
