#include "Pch.h"

#include "Devices/Mockingboard/MockingboardCard.h"
#include "Core/InterruptController.h"
#include "Core/ComponentRegistry.h"
#include "Core/MemoryBus.h"
#include "Core/MachineConfig.h"
#include "ICpu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace MockingboardCardTestNs
{
    // Slot 4 register bases.
    static constexpr Word    kVia1Base = 0xC400;
    static constexpr Word    kVia2Base = 0xC480;

    // AY control-line values written to ORB (RESET released, PB2 high).
    static constexpr Byte    kOrbInactive = 0x04;
    static constexpr Byte    kOrbLatch    = 0x07;
    static constexpr Byte    kOrbWrite    = 0x06;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  MbTestCpu
    //
    ////////////////////////////////////////////////////////////////////////////

    class MbTestCpu : public ICpu
    {
    public:
        HRESULT     Reset         () override                     { return S_OK; }
        HRESULT     Step          (uint32_t & outCycles) override { outCycles = 0; return S_OK; }
        uint64_t    GetCycleCount() const override               { return 0; }

        void        SetInterruptLine (CpuInterruptKind kind, bool asserted) override
        {
            if (kind == CpuInterruptKind::kMaskable)
            {
                m_irqAsserted = asserted;
            }
        }

        bool        IrqAsserted() const { return m_irqAsserted; }

    private:
        bool    m_irqAsserted = false;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  MockingboardCardTests
    //
    //  The card as a whole: two VIA/PSG pairs behind one slot, and the
    //  interrupt path from timer to CPU.
    //
    //  The card is TWO independent channels, and that pairing is what these
    //  test -- each VIA drives its own PSG, so a card wiring both VIAs to one
    //  PSG produces mono output from stereo software and looks like a mixing
    //  bug.
    //
    //  The register handshake gets specific coverage: a PSG register is
    //  addressed and latched through VIA port operations rather than written
    //  directly, so the sequence matters and a reordering writes the right
    //  value to the wrong register.
    //
    //  The interrupt path is exercised end to end -- VIA timer to the shared
    //  controller to the CPU -- since that is how the card paces music, and
    //  each link works in isolation while the chain can still be broken.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MockingboardCardTests)
    {
    public:
        TEST_METHOD (ClaimsSlotIoPage)
        {
            MockingboardCard    card (4);



            Assert::AreEqual<Word> (0xC400, card.GetStart());
            Assert::AreEqual<Word> (0xC4FF, card.GetEnd());
        }


        TEST_METHOD (Address7SelectsSecondVia)
        {
            MockingboardCard    card (4);



            // Writing ORA on each VIA must land on independent chips.
            card.Write (kVia1Base + Via6522::kRegDdra, 0xFF);
            card.Write (kVia2Base + Via6522::kRegDdra, 0xFF);
            card.Write (kVia1Base + Via6522::kRegOra, 0x11);
            card.Write (kVia2Base + Via6522::kRegOra, 0x22);

            Assert::AreEqual<Byte> (0x11, card.GetVia (0).GetOra());
            Assert::AreEqual<Byte> (0x22, card.GetVia (1).GetOra());
        }


        TEST_METHOD (RegisterFileMirrorsEverySixteenBytes)
        {
            MockingboardCard    card (4);



            card.Write (kVia1Base + Via6522::kRegDdra, 0xFF);

            // $C401 and $C411 both address ORA on VIA #1.
            card.Write (0xC411, 0x77);
            Assert::AreEqual<Byte> (0x77, card.GetVia (0).GetOra());
        }


        TEST_METHOD (BusProtocolWritesPsgRegister)
        {
            MockingboardCard    card (4);



            InitAy (card, kVia1Base);
            WriteAy (card, kVia1Base, Ay8910::kRegAmpA, 0x0C);
            WriteAy (card, kVia1Base, Ay8910::kRegToneAFine, 0xAB);

            Assert::AreEqual<Byte> (0x0C, card.GetPsg (0).ReadRegister (Ay8910::kRegAmpA));
            Assert::AreEqual<Byte> (0xAB, card.GetPsg (0).ReadRegister (Ay8910::kRegToneAFine));
        }


        TEST_METHOD (SecondViaDrivesSecondPsg)
        {
            MockingboardCard    card (4);



            InitAy (card, kVia1Base);
            InitAy (card, kVia2Base);

            WriteAy (card, kVia2Base, Ay8910::kRegAmpB, 0x09);

            Assert::AreEqual<Byte> (0x00, card.GetPsg (0).ReadRegister (Ay8910::kRegAmpB),
                                    L"PSG #1 must be untouched by a PSG #2 write");
            Assert::AreEqual<Byte> (0x09, card.GetPsg (1).ReadRegister (Ay8910::kRegAmpB));
        }


        TEST_METHOD (ResetLineClearsPsg)
        {
            MockingboardCard    card (4);



            InitAy (card, kVia1Base);
            WriteAy (card, kVia1Base, Ay8910::kRegAmpA, 0x0F);
            Assert::AreEqual<Byte> (0x0F, card.GetPsg (0).ReadRegister (Ay8910::kRegAmpA));

            // Drive PB2 low (RESET active) via ORB.
            card.Write (kVia1Base + Via6522::kRegOrb, 0x00);

            Assert::AreEqual<Byte> (0x00, card.GetPsg (0).ReadRegister (Ay8910::kRegAmpA),
                                    L"Active-low RESET must clear the PSG registers");
        }


        TEST_METHOD (Timer1ContinuousDrivesSharedIrq)
        {
            MbTestCpu             cpu;
            HRESULT               hr = S_OK;
            InterruptController   ic (&cpu);
            MockingboardCard      card (4);



            hr = card.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            // VIA #1: enable T1 IRQ, continuous mode, latch = 99.
            card.Write (kVia1Base + Via6522::kRegIer,
                        static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer1));
            card.Write (kVia1Base + Via6522::kRegAcr, Via6522::kAcrT1Continuous);
            card.Write (kVia1Base + Via6522::kRegT1CL, 0x63);
            card.Write (kVia1Base + Via6522::kRegT1CH, 0x00);

            card.Tick (100);
            Assert::IsTrue (cpu.IrqAsserted(),
                            L"Continuous Timer 1 must drive the shared IRQ line");

            // Reading T1C-L on VIA #1 clears the flag and de-asserts.
            card.Read (kVia1Base + Via6522::kRegT1CL);
            Assert::IsFalse (cpu.IrqAsserted());
        }


        TEST_METHOD (SecondViaTimerAlsoDrivesIrq)
        {
            MbTestCpu             cpu;
            HRESULT               hr = S_OK;
            InterruptController   ic (&cpu);
            MockingboardCard      card (4);



            hr = card.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            card.Write (kVia2Base + Via6522::kRegIer,
                        static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer1));
            card.Write (kVia2Base + Via6522::kRegAcr, Via6522::kAcrT1Continuous);
            card.Write (kVia2Base + Via6522::kRegT1CL, 0x14);
            card.Write (kVia2Base + Via6522::kRegT1CH, 0x00);

            card.Tick (21);
            Assert::IsTrue (cpu.IrqAsserted(),
                            L"VIA #2's timer shares the same interrupt controller");
        }


        TEST_METHOD (ProgrammedToneProducesAudio)
        {
            uint32_t  i            = 0;
            float     peak         = 0.0f;
            float     buffer[2000] = {};



            MockingboardCard    card (4);



            InitAy (card, kVia1Base);

            // Tone A period ~0x01FF, tone A enabled in the mixer, full amp.
            WriteAy (card, kVia1Base, Ay8910::kRegToneAFine, 0xFF);
            WriteAy (card, kVia1Base, Ay8910::kRegToneACoarse, 0x01);
            WriteAy (card, kVia1Base, Ay8910::kRegMixer, 0x3E);
            WriteAy (card, kVia1Base, Ay8910::kRegAmpA, 0x0F);

            card.SetSampleRate (44100);
            card.GetAudioSource (0)->GeneratePCM (buffer, 2000);

            for (i = 0; i < 2000; i++)
            {
                float  mag = std::abs (buffer[i]);

                if (mag > peak)
                {
                    peak = mag;
                }
            }

            Assert::IsTrue (peak > 0.01f,
                            L"A programmed tone must reach the audio source output");
        }


        TEST_METHOD (FactoryPlacesCardInSlotIoSpace)
        {
            ComponentRegistry           registry;
            MemoryBus                   bus;
            DeviceConfig                config;
            unique_ptr<MemoryDevice>    device = nullptr;



            ComponentRegistry::RegisterBuiltinDevices (registry);
            Assert::IsTrue (registry.IsRegistered ("mockingboard"),
                            L"mockingboard must be a registered device type");

            config.type    = "mockingboard";
            config.slot    = 4;
            config.hasSlot = true;

            device = registry.Create ("mockingboard", config, bus);
            Assert::IsNotNull (device.get());
            Assert::AreEqual<Word> (0xC400, device->GetStart());
            Assert::AreEqual<Word> (0xC4FF, device->GetEnd());
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  PageSweepMatchesMirrorModel
        //
        //  Pins the card's whole-page decode: every offset in $Cn00-$CnFF
        //  behaves as its canonical VIA register (bit 7 selects the VIA, the
        //  low four bits the register, A4-A6 undecoded) -- which is what the
        //  real board does, since its VIAs see neither A4, A5, nor A6.
        //
        //  This is the baseline a speech-equipped variant must be measured
        //  against: that variant adds a listener on part of the page, and this
        //  sweep is the proof the sound-only card never changes underneath it.
        //  The card is put in a quiet state first so no read in the sweep has
        //  a side effect that would skew a later comparison.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PageSweepMatchesMirrorModel)
        {
            int    offset    = 0;
            Word   canonical = 0;



            MockingboardCard    card (4);



            SetupQuietState (card);

            for (offset = 0; offset < 256; offset++)
            {
                canonical = static_cast<Word> (0xC400 + (offset & 0x8F));

                Assert::AreEqual<Byte> (card.Read (canonical),
                                        card.Read (static_cast<Word> (0xC400 + offset)),
                                        L"Every offset must alias its canonical VIA register");
            }

            // Spot-pin a few canonical values so the sweep cannot pass by
            // both sides drifting together.
            Assert::AreEqual<Byte> (0x5A, card.Read (0xC401), L"VIA #1 ORA");
            Assert::AreEqual<Byte> (0xA5, card.Read (0xC481), L"VIA #2 ORA");
            Assert::AreEqual<Byte> (0xFF, card.Read (0xC403), L"VIA #1 DDRA");
            Assert::AreEqual<Byte> (kOrbInactive, card.Read (0xC400), L"VIA #1 ORB");
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  RenderedAudioMatchesBaseline
        //
        //  Pins the card's rendered audio bit-for-bit: a fixed two-PSG program
        //  rendered to a fixed sample count must hash to the value captured
        //  from the shipping card. Sample values are quantized to 16 bits
        //  before hashing so the pin is on audible content, not on float noise.
        //
        //  A change to the PSG core, the audio source, or the card's port
        //  plumbing that alters what the user hears fails here -- and a
        //  speech-equipped variant with its chip left unprogrammed must
        //  produce this identical hash.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RenderedAudioMatchesBaseline)
        {
            static constexpr uint32_t   kExpectedHash = 0x9563EB75;   // captured from the shipping card

            uint32_t   hash    = 0;
            wchar_t    msg[64] = {};



            MockingboardCard    card (4);



            ProgramBaselineTones (card);

            hash = HashRenderedAudio (card);

            swprintf_s (msg, L"Rendered-audio hash was 0x%08X", hash);
            Assert::AreEqual<uint32_t> (kExpectedHash, hash, msg);
        }


    private:
        // A deterministic, side-effect-free state for read sweeps: DDRs set,
        // distinctive port values, T1 latches loaded but neither timer armed,
        // no interrupt source enabled or pending.
        static void SetupQuietState (MockingboardCard & card)
        {
            card.Write (0xC402, 0xFF);   // VIA #1 DDRB
            card.Write (0xC403, 0xFF);   // VIA #1 DDRA
            card.Write (0xC401, 0x5A);   // VIA #1 ORA
            card.Write (0xC400, kOrbInactive);
            card.Write (0xC406, 0x34);   // VIA #1 T1 latch low  ($6 does not arm)
            card.Write (0xC407, 0x12);   // VIA #1 T1 latch high ($7 does not arm)

            card.Write (0xC482, 0xFF);   // VIA #2 DDRB
            card.Write (0xC483, 0xFF);   // VIA #2 DDRA
            card.Write (0xC481, 0xA5);   // VIA #2 ORA
            card.Write (0xC480, kOrbInactive);
        }

        // Distinct tones on both PSGs so the baseline covers both channels
        // and the port plumbing that reaches them.
        static void ProgramBaselineTones (MockingboardCard & card)
        {
            InitAy (card, kVia1Base);
            InitAy (card, kVia2Base);

            WriteAy (card, kVia1Base, Ay8910::kRegToneAFine, 0xFF);
            WriteAy (card, kVia1Base, Ay8910::kRegToneACoarse, 0x01);
            WriteAy (card, kVia1Base, Ay8910::kRegMixer, 0x3E);
            WriteAy (card, kVia1Base, Ay8910::kRegAmpA, 0x0F);

            WriteAy (card, kVia2Base, Ay8910::kRegToneBFine, 0x80);
            WriteAy (card, kVia2Base, Ay8910::kRegToneBCoarse, 0x02);
            WriteAy (card, kVia2Base, Ay8910::kRegMixer, 0x3D);
            WriteAy (card, kVia2Base, Ay8910::kRegAmpB, 0x0C);
        }

        // FNV-1a over the 16-bit quantized samples of both sources.
        static uint32_t HashRenderedAudio (MockingboardCard & card)
        {
            static constexpr uint32_t   kSamples = 4096;

            uint32_t   hash             = 2166136261u;
            uint32_t   i                = 0;
            int        source           = 0;
            int16_t    q                = 0;
            float      buffer[kSamples] = {};



            card.SetSampleRate (44100);

            for (source = 0; source < 2; source++)
            {
                card.GetAudioSource (source)->GeneratePCM (buffer, kSamples);

                for (i = 0; i < kSamples; i++)
                {
                    q = static_cast<int16_t> (buffer[i] * 32767.0f);

                    hash ^= static_cast<uint16_t> (q);
                    hash *= 16777619u;
                }
            }

            return hash;
        }

        // Ports A and B to outputs, RESET released (PB2 high, lines idle).
        static void InitAy (MockingboardCard & card, Word base)
        {
            card.Write (base + Via6522::kRegDdrb, 0xFF);
            card.Write (base + Via6522::kRegDdra, 0xFF);
            card.Write (base + Via6522::kRegOrb, kOrbInactive);
        }

        // Standard Mockingboard latch-address-then-write-data sequence.
        static void WriteAy (MockingboardCard & card, Word base, Byte reg, Byte value)
        {
            card.Write (base + Via6522::kRegOra, reg);
            card.Write (base + Via6522::kRegOrb, kOrbLatch);
            card.Write (base + Via6522::kRegOrb, kOrbInactive);

            card.Write (base + Via6522::kRegOra, value);
            card.Write (base + Via6522::kRegOrb, kOrbWrite);
            card.Write (base + Via6522::kRegOrb, kOrbInactive);
        }
    };
}
