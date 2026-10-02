#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Ay8910.h"
#include "Devices/Ssi263.h"
#include "Devices/Via6522.h"
#include "Machines/Apple2/Common/MockingboardCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MockingboardStateTests
//
//  Save and load of the 6522, the AY-3-8910, the SSI 263 and the Mockingboard
//  that holds them. Each chip's round trip fills every saved field of a source
//  and a target from seeds that differ in every bit, saves the source, loads
//  the target and compares every field, so a field missing from either side
//  fails. The card tests run the card on after a save and check that a card
//  loaded from that save runs to the same samples and state.
//
////////////////////////////////////////////////////////////////////////////////

namespace MockingboardState
{
    static constexpr HRESULT  kInvalidData = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    static constexpr Byte     kSeed        = 0x35;
    static constexpr Byte     kTargetSeed  = 0xCA;     // every bit the complement of kSeed
    static constexpr uint32_t kSampleRate  = 44100;
    static constexpr int      kSlot        = 4;
    static constexpr Word     kBase        = static_cast<Word> (MockingboardCard::kIoBase + kSlot * MockingboardCard::kSlotStride);


    static bool Bit (Byte seed, int bit)
    {
        return ((seed >> bit) & 1) != 0;
    }


    template <typename T>
    static std::vector<Byte> Save (const T & part)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = part.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    // Loads a blob into a part; on success the part must have consumed it all.
    static HRESULT LoadFrom (IMachineState & target, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);
        HRESULT      hr = S_OK;



        hr = target.LoadState (reader);

        if (SUCCEEDED (hr))
        {
            Assert::IsTrue (reader.IsAtEnd());
        }

        return hr;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeVia
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeVia : public Via6522
    {
    public:
        void Fill (Byte seed)
        {
            m_ora       = static_cast<Byte> (seed + 1);
            m_orb       = static_cast<Byte> (seed + 2);
            m_ddra      = static_cast<Byte> (seed + 3);
            m_ddrb      = static_cast<Byte> (seed + 4);
            m_portAIn   = static_cast<Byte> (seed + 5);
            m_portBIn   = static_cast<Byte> (seed + 6);
            m_ca1       = Bit (seed, 0);
            m_cb1       = Bit (seed, 1);
            m_sr        = static_cast<Byte> (seed + 7);
            m_acr       = static_cast<Byte> (seed + 8);
            m_pcr       = static_cast<Byte> (seed + 9);
            m_ifr       = static_cast<Byte> ((seed + 10) & 0x7F);
            m_ier       = static_cast<Byte> ((seed + 11) & 0x7F);
            m_t1LatchLo = static_cast<Byte> (seed + 12);
            m_t1LatchHi = static_cast<Byte> (seed + 13);
            m_t1Counter = 0x1000 + seed;
            m_t1Armed   = Bit (seed, 2);
            m_t2LatchLo = static_cast<Byte> (seed + 14);
            m_t2Start   = 0x2000 + seed;
            m_t2Counter = -1 - seed;
            m_t2Armed   = Bit (seed, 3);
        }

        void AssertSameState (const StateProbeVia & other) const
        {
            Assert::AreEqual (m_ora,       other.m_ora,       L"ORA");
            Assert::AreEqual (m_orb,       other.m_orb,       L"ORB");
            Assert::AreEqual (m_ddra,      other.m_ddra,      L"DDRA");
            Assert::AreEqual (m_ddrb,      other.m_ddrb,      L"DDRB");
            Assert::AreEqual (m_portAIn,   other.m_portAIn,   L"port A input");
            Assert::AreEqual (m_portBIn,   other.m_portBIn,   L"port B input");
            Assert::AreEqual (m_ca1,       other.m_ca1,       L"CA1");
            Assert::AreEqual (m_cb1,       other.m_cb1,       L"CB1");
            Assert::AreEqual (m_sr,        other.m_sr,        L"SR");
            Assert::AreEqual (m_acr,       other.m_acr,       L"ACR");
            Assert::AreEqual (m_pcr,       other.m_pcr,       L"PCR");
            Assert::AreEqual (m_ifr,       other.m_ifr,       L"IFR");
            Assert::AreEqual (m_ier,       other.m_ier,       L"IER");
            Assert::AreEqual (m_t1LatchLo, other.m_t1LatchLo, L"T1 latch low");
            Assert::AreEqual (m_t1LatchHi, other.m_t1LatchHi, L"T1 latch high");
            Assert::AreEqual (m_t1Counter, other.m_t1Counter, L"T1 counter");
            Assert::AreEqual (m_t1Armed,   other.m_t1Armed,   L"T1 armed");
            Assert::AreEqual (m_t2LatchLo, other.m_t2LatchLo, L"T2 latch low");
            Assert::AreEqual (m_t2Start,   other.m_t2Start,   L"T2 start");
            Assert::AreEqual (m_t2Counter, other.m_t2Counter, L"T2 counter");
            Assert::AreEqual (m_t2Armed,   other.m_t2Armed,   L"T2 armed");
        }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeAy
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeAy : public Ay8910
    {
    public:
        void Fill (Byte seed)
        {
            int   i = 0;



            for (i = 0; i < kRegCount; i++)
            {
                m_regs[i] = static_cast<Byte> (seed + i);
            }

            m_latched   = static_cast<Byte> (seed + 20);
            m_tickAccum = 0.25 + seed / 1024.0;

            for (i = 0; i < 3; i++)
            {
                m_toneCounter[i] = 100 + seed + i;
                m_toneState[i]   = Bit (seed, i) ? 1 : 0;
            }

            m_noiseCounter = 200 + seed;
            m_lfsr         = 0x10000u + seed;
            m_envCounter   = 300 + seed;
            m_envLevel     = seed & kMaxEnvLevel;
            m_envDirUp     = Bit (seed, 0);
            m_envHolding   = Bit (seed, 1);
            m_envCont      = Bit (seed, 2);
            m_envAttack    = Bit (seed, 3);
            m_envAlt       = Bit (seed, 4);
            m_envHold      = Bit (seed, 5);
        }

        void AssertSameState (const StateProbeAy & other) const
        {
            int   i = 0;



            for (i = 0; i < kRegCount; i++)
            {
                Assert::AreEqual (m_regs[i], other.m_regs[i], L"register");
            }

            Assert::AreEqual (m_latched,   other.m_latched,   L"latched address");
            Assert::AreEqual (m_tickAccum, other.m_tickAccum, L"tick accumulator");

            for (i = 0; i < 3; i++)
            {
                Assert::AreEqual (m_toneCounter[i], other.m_toneCounter[i], L"tone counter");
                Assert::AreEqual (m_toneState[i],   other.m_toneState[i],   L"tone state");
            }

            Assert::AreEqual (m_noiseCounter, other.m_noiseCounter, L"noise counter");
            Assert::AreEqual (m_lfsr,         other.m_lfsr,         L"LFSR");
            Assert::AreEqual (m_envCounter,   other.m_envCounter,   L"envelope counter");
            Assert::AreEqual (m_envLevel,     other.m_envLevel,     L"envelope level");
            Assert::AreEqual (m_envDirUp,     other.m_envDirUp,     L"envelope direction");
            Assert::AreEqual (m_envHolding,   other.m_envHolding,   L"envelope holding");
            Assert::AreEqual (m_envCont,      other.m_envCont,      L"envelope continue");
            Assert::AreEqual (m_envAttack,    other.m_envAttack,    L"envelope attack");
            Assert::AreEqual (m_envAlt,       other.m_envAlt,       L"envelope alternate");
            Assert::AreEqual (m_envHold,      other.m_envHold,      L"envelope hold");
        }

        void SetEnvLevel (int level) { m_envLevel = level; }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeSsi
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeSsi : public Ssi263
    {
    public:
        void Fill (Byte seed)
        {
            float  f = seed / 256.0f;
            int    i = 0;



            for (i = 0; i < kRegCount; i++)
            {
                m_reg[i] = static_cast<Byte> (seed + i);
            }

            m_mode          = static_cast<Byte> (seed & 0x03);
            m_request       = Bit (seed, 0);
            m_sounding      = Bit (seed, 1);
            m_phonemeCycles = 1000.5 + seed;

            for (i = 0; i < 3; i++)
            {
                m_fCur[i]  = 500.25 + seed + i;
                m_resY1[i] = f + i * 0.01f;
                m_resY2[i] = f + i * 0.02f + 0.5f;
            }

            m_envLevel     = f + 0.03f;
            m_glottalPhase = seed / 512.0 + 0.125;
            m_excLp1       = f + 0.04f;
            m_excLp2       = f + 0.05f;
            m_lfsr         = 0xBEEF0000u + seed;
            m_noiseLp      = f + 0.06f;
            m_vaCur        = f + 0.07f;
            m_faCur        = f + 0.08f;
            m_fricLp       = f + 0.09f;
            m_fricLp2      = f + 0.10f;
            m_fricLp3      = f + 0.11f;
            m_fricY1       = f + 0.12f;
            m_fricY2       = f + 0.13f;
            m_radPrev      = f + 0.14f;
            m_outLp        = f + 0.15f;
            m_outLp2       = f + 0.16f;
        }

        void AssertSameState (const StateProbeSsi & other) const
        {
            int   i = 0;



            for (i = 0; i < kRegCount; i++)
            {
                Assert::AreEqual (m_reg[i], other.m_reg[i], L"register");
            }

            Assert::AreEqual (m_mode,          other.m_mode,          L"mode");
            Assert::AreEqual (m_request,       other.m_request,       L"request");
            Assert::AreEqual (m_sounding,      other.m_sounding,      L"sounding");
            Assert::AreEqual (m_phonemeCycles, other.m_phonemeCycles, L"phoneme cycles");

            for (i = 0; i < 3; i++)
            {
                Assert::AreEqual (m_fCur[i],  other.m_fCur[i],  L"formant center");
                Assert::AreEqual (m_resY1[i], other.m_resY1[i], L"resonator y1");
                Assert::AreEqual (m_resY2[i], other.m_resY2[i], L"resonator y2");
            }

            Assert::AreEqual (m_envLevel,     other.m_envLevel,     L"envelope level");
            Assert::AreEqual (m_glottalPhase, other.m_glottalPhase, L"glottal phase");
            Assert::AreEqual (m_excLp1,       other.m_excLp1,       L"excitation low-pass 1");
            Assert::AreEqual (m_excLp2,       other.m_excLp2,       L"excitation low-pass 2");
            Assert::AreEqual (m_lfsr,         other.m_lfsr,         L"LFSR");
            Assert::AreEqual (m_noiseLp,      other.m_noiseLp,      L"noise low-pass");
            Assert::AreEqual (m_vaCur,        other.m_vaCur,        L"voiced amplitude");
            Assert::AreEqual (m_faCur,        other.m_faCur,        L"fricative amplitude");
            Assert::AreEqual (m_fricLp,       other.m_fricLp,       L"fricative low-pass");
            Assert::AreEqual (m_fricLp2,      other.m_fricLp2,      L"fricative low-pass 2");
            Assert::AreEqual (m_fricLp3,      other.m_fricLp3,      L"fricative low-pass 3");
            Assert::AreEqual (m_fricY1,       other.m_fricY1,       L"fricative y1");
            Assert::AreEqual (m_fricY2,       other.m_fricY2,       L"fricative y2");
            Assert::AreEqual (m_radPrev,      other.m_radPrev,      L"radiation history");
            Assert::AreEqual (m_outLp,        other.m_outLp,        L"output low-pass");
            Assert::AreEqual (m_outLp2,       other.m_outLp2,       L"output low-pass 2");
        }

        void SetMode (Byte mode) { m_mode = mode; }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeCard
    //
    //  A Mockingboard with its PSG control-line memory opened up, and a fixed
    //  workload: program both PSGs, start VIA #1's Timer 1 free-running, start
    //  a phoneme, then tick and render.
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeCard : public MockingboardCard
    {
    public:
        explicit StateProbeCard (MockingboardVariant variant) : MockingboardCard (kSlot, variant)
        {
            SetSampleRate (kSampleRate);
        }

        Byte GetLastControl (int index) const      { return m_lastControl[index]; }
        void SetLastControl (int index, Byte value) { m_lastControl[index] = value; }

        void SetPsgRegister (int index, Byte reg, Byte value)
        {
            Word   via = static_cast<Word> (kBase + (index != 0 ? kVia2Select : 0));



            Write (static_cast<Word> (via + Via6522::kRegDdra), 0xFF);
            Write (static_cast<Word> (via + Via6522::kRegDdrb), 0xFF);
            Write (static_cast<Word> (via + Via6522::kRegOra),  reg);
            Write (static_cast<Word> (via + Via6522::kRegOrb),  kAyResetLow | kAyBdir | kAyBc1);
            Write (static_cast<Word> (via + Via6522::kRegOrb),  kAyResetLow);
            Write (static_cast<Word> (via + Via6522::kRegOra),  value);
            Write (static_cast<Word> (via + Via6522::kRegOrb),  kAyResetLow | kAyBdir);
            Write (static_cast<Word> (via + Via6522::kRegOrb),  kAyResetLow);
        }

        void Program()
        {
            int   i = 0;



            for (i = 0; i < kViaCount; i++)
            {
                SetPsgRegister (i, Ay8910::kRegToneAFine,   static_cast<Byte> (0x40 + i * 0x10));
                SetPsgRegister (i, Ay8910::kRegToneBFine,   0x77);
                SetPsgRegister (i, Ay8910::kRegNoisePeriod, 0x0B);
                SetPsgRegister (i, Ay8910::kRegMixer,       0x30);
                SetPsgRegister (i, Ay8910::kRegAmpA,        0x0F);
                SetPsgRegister (i, Ay8910::kRegAmpB,        Ay8910::kAmpUseEnvelope);
                SetPsgRegister (i, Ay8910::kRegEnvFine,     0x20);
                SetPsgRegister (i, Ay8910::kRegEnvShape,    Ay8910::kEnvContinue | Ay8910::kEnvAlternate);
            }

            Write (static_cast<Word> (kBase + Via6522::kRegIer),  Via6522::kIerSetClear | Via6522::kIrqTimer1);
            Write (static_cast<Word> (kBase + Via6522::kRegAcr),  Via6522::kAcrT1Continuous);
            Write (static_cast<Word> (kBase + Via6522::kRegT1CL), 0x34);
            Write (static_cast<Word> (kBase + Via6522::kRegT1CH), 0x02);

            if (GetSpeech() != nullptr)
            {
                Write (static_cast<Word> (kBase + kSpeechChip1 + Ssi263::kRegDurationPhoneme), 0xC0 | 0x12);
                Write (static_cast<Word> (kBase + kSpeechChip1 + Ssi263::kRegRateInflection),  0xA0);
                Write (static_cast<Word> (kBase + kSpeechChip1 + Ssi263::kRegCtlArtAmp),       0x7C);
            }
        }

        // Ticks and renders `steps` audio-sample-sized slices and returns every
        // rendered sample and the VIA #1 IFR after each slice.
        std::vector<float> Run (int steps)
        {
            constexpr uint32_t  kCyclesPerStep = 23;
            std::vector<float>  out;
            int                 i              = 0;



            for (i = 0; i < steps; i++)
            {
                Tick (kCyclesPerStep);

                out.push_back (GetPsg (0).GenerateSample());
                out.push_back (GetPsg (1).GenerateSample());
                out.push_back (GetSpeech() != nullptr ? GetSpeech()->GenerateSample() : 0.0f);
                out.push_back (static_cast<float> (GetVia (0).GetIfr()));
            }

            return out;
        }
    };


    TEST_CLASS (MockingboardStateTests)
    {
    public:
        TEST_METHOD (ViaRoundTripsEveryField)
        {
            StateProbeVia      source;
            StateProbeVia      target;
            HRESULT            hr = S_OK;



            source.Fill (kSeed);
            target.Fill (kTargetSeed);

            hr = LoadFrom (target, Save (source));
            Assert::AreEqual (S_OK, hr);

            source.AssertSameState (target);
        }


        TEST_METHOD (ViaFlagBit7Fails)
        {
            StateProbeVia      via;
            std::vector<Byte>  bytes;
            HRESULT            hr = S_OK;



            via.Fill (kSeed);
            bytes = Save (via);

            // IFR follows six port bytes, the two control lines, SR, ACR and PCR.
            bytes[StateWriter::kSectionHeaderSize + 11] |= Via6522::kIrqAny;

            hr = LoadFrom (via, bytes);
            Assert::AreEqual (kInvalidData, hr, L"IFR");

            // IER is the byte after IFR.
            via.Fill (kSeed);
            bytes = Save (via);
            bytes[StateWriter::kSectionHeaderSize + 12] |= Via6522::kIrqAny;

            hr = LoadFrom (via, bytes);
            Assert::AreEqual (kInvalidData, hr, L"IER");
        }


        TEST_METHOD (AyRoundTripsEveryField)
        {
            StateProbeAy       source;
            StateProbeAy       target;
            HRESULT            hr = S_OK;



            source.Fill (kSeed);
            target.Fill (kTargetSeed);

            hr = LoadFrom (target, Save (source));
            Assert::AreEqual (S_OK, hr);

            source.AssertSameState (target);
        }


        TEST_METHOD (AyEnvelopeLevelOutOfRangeFails)
        {
            StateProbeAy       ay;
            HRESULT            hr = S_OK;



            ay.SetEnvLevel (Ay8910::kMaxEnvLevel + 1);

            hr = LoadFrom (ay, Save (ay));
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (SsiRoundTripsEveryField)
        {
            StateProbeSsi      source;
            StateProbeSsi      target;
            HRESULT            hr = S_OK;



            source.Fill (kSeed);
            target.Fill (kTargetSeed);

            hr = LoadFrom (target, Save (source));
            Assert::AreEqual (S_OK, hr);

            source.AssertSameState (target);
        }


        TEST_METHOD (SsiModeOutOfRangeFails)
        {
            StateProbeSsi      ssi;
            HRESULT            hr = S_OK;



            ssi.SetMode (static_cast<Byte> (Ssi263::kModePhonemeTransitioned + 1));

            hr = LoadFrom (ssi, Save (ssi));
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (CardRoundTripsEveryPart)
        {
            StateProbeCard     source (MockingboardVariant::SoundSpeech);
            StateProbeCard     target (MockingboardVariant::SoundSpeech);
            std::vector<Byte>  saved;
            HRESULT            hr = S_OK;



            source.Program();
            source.Run (500);

            // The workload leaves both control lines idle, as a fresh card has
            // them, so set values a fresh card does not hold.
            source.SetLastControl (0, MockingboardCard::kAyBdir);
            source.SetLastControl (1, MockingboardCard::kAyBc1);
            saved = Save (source);

            Assert::IsFalse (saved == Save (target));

            hr = LoadFrom (target, saved);
            Assert::AreEqual (S_OK, hr);

            Assert::IsTrue   (saved == Save (target));
            Assert::AreEqual (source.GetLastControl (0), target.GetLastControl (0), L"VIA #1 PSG control");
            Assert::AreEqual (source.GetLastControl (1), target.GetLastControl (1), L"VIA #2 PSG control");
        }


        TEST_METHOD (CardRunsTheSameAfterLoad)
        {
            constexpr int       kSteps = 4000;
            StateProbeCard      source (MockingboardVariant::SoundSpeech);
            StateProbeCard      target (MockingboardVariant::SoundSpeech);
            std::vector<Byte>   saved;
            std::vector<float>  expected;
            std::vector<float>  replayed;
            std::vector<float>  reloaded;
            HRESULT             hr     = S_OK;



            source.Program();
            source.Run (300);
            saved    = Save (source);
            expected = source.Run (kSteps);

            hr = LoadFrom (target, saved);
            Assert::AreEqual (S_OK, hr);

            replayed = target.Run (kSteps);
            Assert::IsTrue (expected == replayed, L"a fresh card loaded from the save");
            Assert::IsTrue (Save (source) == Save (target));

            hr = LoadFrom (source, saved);
            Assert::AreEqual (S_OK, hr);

            reloaded = source.Run (kSteps);
            Assert::IsTrue (expected == reloaded, L"the same card loaded back");
        }


        TEST_METHOD (CardVariantMismatchFails)
        {
            StateProbeCard     speech (MockingboardVariant::SoundSpeech);
            StateProbeCard     sound  (MockingboardVariant::SoundOnly);
            HRESULT            hr     = S_OK;



            hr = LoadFrom (sound, Save (speech));
            Assert::AreEqual (kInvalidData, hr);

            hr = LoadFrom (speech, Save (sound));
            Assert::AreEqual (kInvalidData, hr);
        }
    };
}
