#include "Pch.h"

#include "Devices/Mockingboard/Ay8910.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace Ay8910TestNs
{
    static constexpr double     kClockHz    = 1022727.0;
    static constexpr uint32_t   kSampleRate = 44100;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  Ay8910Tests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Ay8910Tests)
    {
    public:
        TEST_METHOD (ResetClearsRegisters)
        {
            Ay8910    ay (kClockHz);



            for (Byte r = 0; r < Ay8910::kRegCount; r++)
            {
                Assert::AreEqual<Byte> (0, ay.ReadRegister (r));
            }

            Assert::AreEqual (0, ay.GetEnvLevel ());
        }


        TEST_METHOD (LatchAndWriteDataRoutesToRegister)
        {
            Ay8910    ay (kClockHz);



            ay.LatchAddress (Ay8910::kRegAmpA);
            ay.WriteData (0x0C);

            Assert::AreEqual<Byte> (Ay8910::kRegAmpA, ay.GetLatchedAddress ());
            Assert::AreEqual<Byte> (0x0C, ay.ReadRegister (Ay8910::kRegAmpA));

            ay.LatchAddress (Ay8910::kRegAmpA);
            Assert::AreEqual<Byte> (0x0C, ay.ReadData ());
        }


        TEST_METHOD (OutOfRangeLatchIsIgnored)
        {
            Ay8910    ay (kClockHz);



            // Only 16 registers exist; a latched address of 16+ is inert.
            ay.LatchAddress (0x20);
            ay.WriteData (0x55);

            Assert::AreEqual<Byte> (0xFF, ay.ReadData ());
        }


        TEST_METHOD (ToneFrequencyMatchesDatasheetFormula)
        {
            Ay8910    ay (kClockHz);
            int       period      = 254;
            int       toggles     = 0;
            bool      prevState   = false;
            uint32_t  i           = 0;
            double    expectedHz  = kClockHz / (16.0 * period);
            double    measuredHz  = 0.0;



            ay.SetSampleRate (kSampleRate);

            // Channel A tone period = 254 (fine=0xFE, coarse=0x00).
            ay.WriteRegister (Ay8910::kRegToneAFine, static_cast<Byte> (period & 0xFF));
            ay.WriteRegister (Ay8910::kRegToneACoarse, static_cast<Byte> ((period >> 8) & 0x0F));

            prevState = ay.GetToneState (0);

            // Generate exactly one second and count the square-wave toggles.
            for (i = 0; i < kSampleRate; i++)
            {
                ay.GenerateSample ();

                if (ay.GetToneState (0) != prevState)
                {
                    toggles++;
                    prevState = ay.GetToneState (0);
                }
            }

            // Two toggles per full cycle.
            measuredHz = toggles / 2.0;

            Assert::IsTrue (std::abs (measuredHz - expectedHz) < expectedHz * 0.02,
                            L"Tone frequency must match clock/(16*period) within 2%");
        }


        TEST_METHOD (SilentChipProducesNoOutput)
        {
            Ay8910    ay (kClockHz);
            uint32_t  i   = 0;
            float     sum = 0.0f;



            ay.SetSampleRate (kSampleRate);

            // Mixer default 0 => all tone/noise enabled, but every channel
            // amplitude is 0, so the DAC output is silence.
            for (i = 0; i < 1000; i++)
            {
                sum += ay.GenerateSample ();
            }

            Assert::AreEqual (0.0f, sum, L"Zero amplitude must yield pure silence");
        }


        TEST_METHOD (AmplitudeScalesOutput)
        {
            Ay8910    ay (kClockHz);
            uint32_t  i    = 0;
            float     peak = 0.0f;



            ay.SetSampleRate (kSampleRate);

            // Channel A: tone enabled, noise disabled, full amplitude.
            ay.WriteRegister (Ay8910::kRegToneAFine, 0xFF);
            ay.WriteRegister (Ay8910::kRegToneACoarse, 0x01);
            ay.WriteRegister (Ay8910::kRegMixer, 0x3E);   // tone A on, everything else off
            ay.WriteRegister (Ay8910::kRegAmpA, 0x0F);

            for (i = 0; i < 5000; i++)
            {
                float  s = ay.GenerateSample ();

                if (s > peak)
                {
                    peak = s;
                }
            }

            Assert::IsTrue (peak > 0.9f,
                            L"Full-amplitude tone should peak near full scale");
        }


        TEST_METHOD (NoiseOutputVaries)
        {
            Ay8910    ay (kClockHz);
            uint32_t  i        = 0;
            float     minv     = 1.0e9f;
            float     maxv     = -1.0e9f;



            ay.SetSampleRate (kSampleRate);

            // Channel A: noise enabled, tone disabled, full amplitude.
            ay.WriteRegister (Ay8910::kRegNoisePeriod, 0x01);
            ay.WriteRegister (Ay8910::kRegMixer, 0x37);   // noise A on, tone A off
            ay.WriteRegister (Ay8910::kRegAmpA, 0x0F);

            for (i = 0; i < 5000; i++)
            {
                float  s = ay.GenerateSample ();

                minv = (s < minv) ? s : minv;
                maxv = (s > maxv) ? s : maxv;
            }

            Assert::IsTrue (maxv > minv,
                            L"Noise must produce a varying output");
            Assert::AreNotEqual<uint32_t> (1u, ay.GetNoiseLfsr (),
                                           L"LFSR must have advanced from its seed");
        }


        TEST_METHOD (EnvelopeSawtoothUpRampsAndRepeats)
        {
            Ay8910    ay (kClockHz);
            uint32_t  i        = 0;
            int       maxLevel = 0;
            bool      dropped  = false;
            int       lastPeak = 0;



            ay.SetSampleRate (kSampleRate);

            ay.WriteRegister (Ay8910::kRegEnvFine, 0x04);
            ay.WriteRegister (Ay8910::kRegEnvCoarse, 0x00);
            ay.WriteRegister (Ay8910::kRegAmpA, Ay8910::kAmpUseEnvelope);
            ay.WriteRegister (Ay8910::kRegEnvShape, 0x0C);   // /|/|  sawtooth up, repeating

            for (i = 0; i < 4000; i++)
            {
                ay.GenerateSample ();

                int  level = ay.GetEnvLevel ();

                maxLevel = (level > maxLevel) ? level : maxLevel;

                // Detect the reset back toward 0 after reaching the top.
                if (lastPeak == Ay8910::kMaxEnvLevel && level < 4)
                {
                    dropped = true;
                }

                lastPeak = level;
            }

            Assert::AreEqual (Ay8910::kMaxEnvLevel, maxLevel,
                              L"Sawtooth-up envelope must reach full level");
            Assert::IsTrue (dropped, L"Repeating sawtooth must restart from the bottom");
            Assert::IsFalse (ay.IsEnvHolding ());
        }


        TEST_METHOD (EnvelopeOneShotDecayHoldsAtZero)
        {
            Ay8910    ay (kClockHz);
            uint32_t  i = 0;



            ay.SetSampleRate (kSampleRate);

            ay.WriteRegister (Ay8910::kRegEnvFine, 0x04);
            ay.WriteRegister (Ay8910::kRegEnvCoarse, 0x00);
            ay.WriteRegister (Ay8910::kRegAmpA, Ay8910::kAmpUseEnvelope);
            ay.WriteRegister (Ay8910::kRegEnvShape, 0x00);   // \___ decay then hold at 0

            for (i = 0; i < 4000; i++)
            {
                ay.GenerateSample ();
            }

            Assert::AreEqual (0, ay.GetEnvLevel (),
                              L"One-shot decay must settle at 0");
            Assert::IsTrue (ay.IsEnvHolding (),
                            L"One-shot envelope must hold when complete");
        }


        TEST_METHOD (EnvelopeAttackHoldReachesAndHoldsTop)
        {
            Ay8910    ay (kClockHz);
            uint32_t  i = 0;



            ay.SetSampleRate (kSampleRate);

            ay.WriteRegister (Ay8910::kRegEnvFine, 0x04);
            ay.WriteRegister (Ay8910::kRegEnvCoarse, 0x00);
            ay.WriteRegister (Ay8910::kRegAmpA, Ay8910::kAmpUseEnvelope);
            ay.WriteRegister (Ay8910::kRegEnvShape, 0x0D);   // /--- attack then hold at 15

            for (i = 0; i < 4000; i++)
            {
                ay.GenerateSample ();
            }

            Assert::AreEqual (Ay8910::kMaxEnvLevel, ay.GetEnvLevel (),
                              L"Attack-hold envelope must hold at full level");
            Assert::IsTrue (ay.IsEnvHolding ());
        }


        TEST_METHOD (OutputIsDeterministic)
        {
            Ay8910    a (kClockHz);
            Ay8910    b (kClockHz);
            uint32_t  i = 0;



            a.SetSampleRate (kSampleRate);
            b.SetSampleRate (kSampleRate);

            for (Byte r = 0; r < Ay8910::kRegCount; r++)
            {
                a.WriteRegister (r, static_cast<Byte> (0x11 * (r + 1)));
                b.WriteRegister (r, static_cast<Byte> (0x11 * (r + 1)));
            }

            // Two identically-programmed chips must render bit-identical
            // streams -- the "reference render" reproducibility guarantee.
            for (i = 0; i < 8000; i++)
            {
                Assert::AreEqual (a.GenerateSample (), b.GenerateSample ());
            }
        }


        TEST_METHOD (VolumeForLevelIsMonotonic)
        {
            for (int level = 1; level <= Ay8910::kMaxEnvLevel; level++)
            {
                Assert::IsTrue (Ay8910::VolumeForLevel (level) > Ay8910::VolumeForLevel (level - 1),
                                L"DAC table must increase monotonically");
            }

            Assert::AreEqual (0.0f, Ay8910::VolumeForLevel (0));
            Assert::AreEqual (1.0f, Ay8910::VolumeForLevel (Ay8910::kMaxEnvLevel));
        }
    };
}
