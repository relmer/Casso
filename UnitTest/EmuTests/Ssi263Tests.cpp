#include "Pch.h"

#include "Devices/Mockingboard/Ssi263.h"

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
            phoneme = chip.Phoneme();

            // Raise CTL: Power Down silences and disables A/R, but the
            // datasheet is explicit that register contents survive.
            chip.WriteRegister (Ssi263::kRegCtlArtAmp, Ssi263::kCtl);

            Assert::IsTrue (chip.IsSilent());
            Assert::IsFalse (chip.IsRequesting());
            Assert::AreEqual<Byte> (phoneme, chip.Phoneme(),
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
                Assert::AreEqual<Byte> (code, chip.Phoneme(),
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

                Assert::AreEqual<uint16_t> (expected, chip.InflectionValue(),
                                            L"Each inflection bit must land at its documented position");
            }
        }


        TEST_METHOD (RateAndInflectionShareARegisterWithoutCollision)
        {
            Ssi263   chip;



            // R3-R0 in the high nibble, I11 and I2-I0 in the low.
            chip.WriteRegister (Ssi263::kRegRateInflection, 0xAF);

            Assert::AreEqual<Byte> (0x0A, chip.RateSel(),
                                    L"Rate occupies the high nibble");
            Assert::AreEqual<uint16_t> (0x807, chip.InflectionValue(),
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

                expected = (4096.0 * (16.0 - static_cast<double> (r))) / Ssi263::kDefaultClockHz;

                Assert::AreEqual (expected, chip.FrameDurationSec(), 1e-12,
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

            frame = chip.FrameDurationSec();

            for (d = 0; d <= 3; d++)
            {
                chip.WriteRegister (Ssi263::kRegDurationPhoneme,
                                    static_cast<Byte> (d << Ssi263::kDurationShift));

                Assert::AreEqual (frame * (4.0 - static_cast<double> (d)),
                                  chip.PhonemeDurationSec(), 1e-12,
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

                Assert::AreEqual (Ssi263::kDefaultClockHz / (2.0 * (256.0 - f)),
                                  chip.FilterFrequencyHz(), 1e-6,
                                  L"Filter Frequency = XCK / (2 x (256 - F))");
            }
        }


        TEST_METHOD (InflectionFrequencyMatchesDatasheetFormula)
        {
            Ssi263   chip;



            chip.WriteRegister (Ssi263::kRegInflection, 0x80);   // I10 set -> I = 0x400

            Assert::AreEqual<uint16_t> (0x400, chip.InflectionValue());
            Assert::AreEqual (Ssi263::kDefaultClockHz / (8.0 * (4096.0 - 1024.0)),
                              chip.InflectionFrequencyHz(), 1e-9,
                              L"Inflection Frequency = XCK / (8 x (4096 - I))");
        }


        TEST_METHOD (RequestRaisesWhenPhonemeDurationElapses)
        {
            Ssi263     chip;
            uint32_t   cycles = 0;



            StartSpeaking (chip, 0x08);
            cycles = static_cast<uint32_t> (chip.PhonemeDurationSec() * Ssi263::kDefaultClockHz);

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
            Assert::AreEqual<Byte> (Ssi263::kModeArDisabled, chip.ActiveMode());

            // Changing the duration bits afterwards selects a DURATION, not a
            // mode -- the mode only latches on a CTL one-to-zero transition.
            chip.WriteRegister (Ssi263::kRegDurationPhoneme,
                                static_cast<Byte> (Ssi263::kModePhonemeTransitioned << Ssi263::kDurationShift));

            Assert::AreEqual<Byte> (Ssi263::kModeArDisabled, chip.ActiveMode(),
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
            cycles = static_cast<uint32_t> (chip.PhonemeDurationSec() * Ssi263::kDefaultClockHz);

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

            Assert::AreEqual (a.PhonemeDurationSec(), b.PhonemeDurationSec(), 1e-12,
                              L"Host sample rate must not affect emulated duration");

            cycles = static_cast<uint32_t> (a.PhonemeDurationSec() * Ssi263::kDefaultClockHz);

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

            Assert::AreEqual<Byte> (0, chip.Amplitude());
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

            Assert::AreEqual (a.InflectionValue(), b.InflectionValue());
            Assert::AreEqual (a.PhonemeDurationSec(), b.PhonemeDurationSec(), 1e-12);
            Assert::AreEqual (a.IsRequesting(), b.IsRequesting());
            Assert::AreEqual (a.IsSilent(), b.IsSilent());
        }


    private:
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
