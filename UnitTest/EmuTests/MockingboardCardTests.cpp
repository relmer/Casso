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
        //  TwoRegisteredTypesYieldTheTwoCardModels
        //
        //  The existing type name keeps meaning what it has always meant -- a
        //  profile or user override naming "mockingboard" gets the sound-only
        //  card, exactly as before the speech variant existed. The new type
        //  yields the sound+speech card. Both accept any slot, like every card
        //  in the registry.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (TwoRegisteredTypesYieldTheTwoCardModels)
        {
            ComponentRegistry           registry;
            MemoryBus                   bus;
            DeviceConfig                config;
            unique_ptr<MemoryDevice>    device = nullptr;
            MockingboardCard *          card   = nullptr;



            ComponentRegistry::RegisterBuiltinDevices (registry);
            Assert::IsTrue (registry.IsRegistered ("mockingboard-c"),
                            L"mockingboard-c must be a registered device type");

            config.type    = "mockingboard";
            config.slot    = 4;
            config.hasSlot = true;

            device = registry.Create ("mockingboard", config, bus);
            card   = dynamic_cast<MockingboardCard *> (device.get());

            Assert::IsNotNull (card);
            Assert::IsTrue (MockingboardVariant::SoundOnly == card->GetVariant(),
                            L"The existing type name must keep yielding the sound-only card");
            Assert::IsNull (card->GetSpeech());

            config.type = "mockingboard-c";
            config.slot = 5;

            device = registry.Create ("mockingboard-c", config, bus);
            card   = dynamic_cast<MockingboardCard *> (device.get());

            Assert::IsNotNull (card);
            Assert::IsTrue (MockingboardVariant::SoundSpeech == card->GetVariant(),
                            L"The new type must yield the sound+speech card");
            Assert::IsNotNull (card->GetSpeech());
            Assert::AreEqual<Word> (0xC500, card->GetStart(),
                                    L"Either variant accepts any slot");
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


        ////////////////////////////////////////////////////////////////////////
        //
        //  SoundOnlyVariantMatchesBaselineEverywhere
        //
        //  The sound-only card constructed through the variant-aware path must
        //  be indistinguishable from the shipping card: same page sweep, same
        //  rendered-audio hash, no voice chip present. This is the guarantee
        //  that lets the speech variant exist without touching anyone who did
        //  not choose it.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SoundOnlyVariantMatchesBaselineEverywhere)
        {
            int    offset = 0;



            MockingboardCard    card (4, MockingboardVariant::SoundOnly);



            Assert::IsNull (card.GetSpeech(), L"The A has no voice chip");

            SetupQuietState (card);

            for (offset = 0; offset < 256; offset++)
            {
                Assert::AreEqual<Byte> (card.Read (static_cast<Word> (0xC400 + (offset & 0x8F))),
                                        card.Read (static_cast<Word> (0xC400 + offset)),
                                        L"The A's page must mirror exactly as it ships today");
            }
        }


        TEST_METHOD (SpeechVariantAudioMatchesBaselineWhileUnprogrammed)
        {
            MockingboardCard    card (4, MockingboardVariant::SoundSpeech);



            ProgramBaselineTones (card);

            Assert::AreEqual<uint32_t> (0x9563EB75, HashRenderedAudio (card),
                                        L"An unprogrammed voice chip must not change rendered audio");
        }


        TEST_METHOD (SpeechWriteIsATapNotAReplacement)
        {
            MockingboardCard    card (4, MockingboardVariant::SoundSpeech);



            card.Write (0xC403, 0xFF);   // VIA #1 DDRA all output

            // A write in the populated speech range must land in BOTH places:
            // the VIA mirror (offset $41 aliases ORA) and the chip's register
            // file (RS = 1, the inflection register).
            card.Write (0xC441, 0x66);

            Assert::AreEqual<Byte> (0x66, card.GetVia (0).GetOra(),
                                    L"The VIA mirror must still receive the write");
            Assert::AreEqual<uint16_t> (static_cast<uint16_t> (0x66 << 3),
                                        card.GetSpeech()->GetInflectionValue(),
                                        L"The chip must receive the same write");
        }


        TEST_METHOD (EmptySocketRangeBehavesAsTheSoundOnlyCard)
        {
            MockingboardCard    a (4, MockingboardVariant::SoundOnly);
            MockingboardCard    c (4, MockingboardVariant::SoundSpeech);
            int                 offset = 0;



            SetupQuietState (a);
            SetupQuietState (c);

            // Socket 0's range ($20-$2F) is empty on the C, and the excluded
            // offsets ($30, $50, $70) never decode to speech at all. All must
            // read identically to the A.
            for (offset = 0x20; offset <= 0x3F; offset++)
            {
                Assert::AreEqual<Byte> (a.Read (static_cast<Word> (0xC400 + offset)),
                                        c.Read (static_cast<Word> (0xC400 + offset)),
                                        L"An empty socket must leave its range untouched");
            }

            Assert::AreEqual<Byte> (a.Read (0xC450), c.Read (0xC450),
                                    L"$50 has A4 set and must never decode to speech");
            Assert::AreEqual<Byte> (a.Read (0xC470), c.Read (0xC470),
                                    L"$70 has A4 set and must never decode to speech");
        }


        TEST_METHOD (SpeechReadCarriesRequestStatusOnBit7)
        {
            MockingboardCard    card (4, MockingboardVariant::SoundSpeech);
            Ssi263 *            chip = nullptr;



            chip = card.GetSpeech();

            // Bring the chip out of Power Down in the common mode and start a
            // short phoneme.
            card.Write (0xC440, static_cast<Byte> (Ssi263::kModePhonemeTransitioned << Ssi263::kDurationShift));
            card.Write (0xC443, 0x0C);
            card.Write (0xC442, static_cast<Byte> (0x0F << Ssi263::kRateShift));
            card.Write (0xC440, 0x08);

            Assert::AreEqual<Byte> (0, static_cast<Byte> (card.Read (0xC442) & 0x80),
                                    L"No request while the phoneme is sounding");

            card.Tick (static_cast<uint32_t> (chip->GetPhonemeDurationSec() * Ssi263::kDefaultClockHz) + 1);

            Assert::AreEqual<Byte> (0x80, static_cast<Byte> (card.Read (0xC442) & 0x80),
                                    L"The request must surface as D7 anywhere in the chip's range");
        }


        TEST_METHOD (SpeechRequestDrivesCa1InterruptThroughVia)
        {
            MbTestCpu             cpu;
            InterruptController   ic (&cpu);
            Ssi263 *              chip = nullptr;



            MockingboardCard      card (4, MockingboardVariant::SoundSpeech);



            card.AttachInterruptController (&ic);
            chip = card.GetSpeech();

            // Falling-edge CA1 (PCR bit 0 clear is the reset state), CA1
            // interrupt enabled -- the arming real speech drivers perform.
            card.Write (0xC40C, 0x00);
            card.Write (0xC40E, static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqCa1));

            card.Write (0xC440, static_cast<Byte> (Ssi263::kModePhonemeTransitioned << Ssi263::kDurationShift));
            card.Write (0xC443, 0x0C);
            card.Write (0xC442, static_cast<Byte> (0x0F << Ssi263::kRateShift));
            card.Write (0xC440, 0x08);

            Assert::IsFalse (cpu.IrqAsserted(), L"No interrupt while sounding");

            card.Tick (static_cast<uint32_t> (chip->GetPhonemeDurationSec() * Ssi263::kDefaultClockHz) + 1);

            Assert::IsTrue (cpu.IrqAsserted(),
                            L"Phoneme completion must reach the CPU through CA1");

            // The pacing loop's response: write the next phoneme, then
            // acknowledge at the VIA. The line must release.
            card.Write (0xC440, 0x09);
            card.Read (0xC401);

            Assert::IsFalse (cpu.IrqAsserted(),
                             L"Acknowledge must release the interrupt");
        }


        TEST_METHOD (UnprogrammedSpeechVariantNeverInterrupts)
        {
            MbTestCpu             cpu;
            InterruptController   ic (&cpu);



            MockingboardCard      card (4, MockingboardVariant::SoundSpeech);



            card.AttachInterruptController (&ic);

            // Arm every VIA interrupt source on both VIAs -- the harshest
            // dispatch-on-anything music player -- and run ten emulated
            // minutes with the voice chip present but never programmed.
            card.Write (0xC40E, static_cast<Byte> (Via6522::kIerSetClear | 0x7F));
            card.Write (0xC48E, static_cast<Byte> (Via6522::kIerSetClear | 0x7F));

            card.Tick (600u * 1020484u);

            Assert::IsFalse (cpu.IrqAsserted(),
                             L"An unprogrammed voice chip must never interrupt");
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  DetectionSequenceIdenticalOnBothVariants
        //
        //  The way period software detects a Mockingboard: load Timer 1 and
        //  read the counter twice -- a live 6522 counts down between the
        //  reads. The sequence must behave identically on the sound-only and
        //  sound+speech cards, including at a mirror address, since detection
        //  code does not know which model it is probing.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DetectionSequenceIdenticalOnBothVariants)
        {
            Byte   firstA  = 0;
            Byte   secondA = 0;
            Byte   firstC  = 0;
            Byte   secondC = 0;



            MockingboardCard    a (4, MockingboardVariant::SoundOnly);
            MockingboardCard    c (4, MockingboardVariant::SoundSpeech);



            RunDetection (a, firstA, secondA);
            RunDetection (c, firstC, secondC);

            Assert::AreEqual<Byte> (firstA, firstC,
                                    L"First counter read must match across variants");
            Assert::AreEqual<Byte> (secondA, secondC,
                                    L"Second counter read must match across variants");
            Assert::IsTrue (secondA != firstA,
                            L"A live 6522 must be counting, or detection fails on both");

            // The same probe through a VIA #1 mirror inside the page must
            // also agree -- $Cn14 aliases T1C-L on both cards.
            Assert::AreEqual<Byte> (a.Read (0xC414), c.Read (0xC414),
                                    L"Mirror reads must agree across variants");
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  FullVolumeCardOutputLeavesHeadroom
        //
        //  Both PSGs at full fixed volume plus speech at full amplitude, all
        //  rendered together the way the mixer pans them: the card's summed
        //  contribution must stay well inside the rails, leaving room for the
        //  speaker and drive audio it shares the bus with. This is the gain
        //  budget as a test rather than a comment.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (FullVolumeCardOutputLeavesHeadroom)
        {
            uint32_t   i     = 0;
            float      peak  = 0.0f;
            float      left  = 0.0f;
            std::vector<float>   psg0 (2048);
            std::vector<float>   psg1 (2048);
            std::vector<float>   speech (2048);



            MockingboardCard    card (4, MockingboardVariant::SoundSpeech);



            ProgramBaselineTones (card);
            card.Write (0xC440, static_cast<Byte> (Ssi263::kModePhonemeTransitioned << Ssi263::kDurationShift));
            card.Write (0xC443, 0x5F);   // CTL low, articulation 5, amplitude $F
            card.Write (0xC442, 0xA2);   // rate $A, low inflection bits
            card.Write (0xC444, 0xD3);   // ~20 kHz tract clock
            card.Write (0xC440, 0x0F);   // AH1 at full amplitude

            card.SetSampleRate (44100);
            card.GetAudioSource (0)->GeneratePCM (psg0.data(), 2048);
            card.GetAudioSource (1)->GeneratePCM (psg1.data(), 2048);
            card.GetSpeechAudioSource()->GeneratePCM (speech.data(), 2048);

            for (i = 0; i < 2048; i++)
            {
                // Left channel carries PSG #1 (hard left) plus center speech;
                // the right-channel case is symmetric with PSG #2.
                left = std::abs (psg0[i]) + std::abs (speech[i]) * IDriveAudioSource::kCenterPan;

                if (left > peak)
                {
                    peak = left;
                }
            }

            Assert::IsTrue (peak < 0.75f,
                            L"The card's full-volume sum must leave headroom for speaker and drives");
        }


        TEST_METHOD (CardResetSilencesSpeechImmediately)
        {
            uint32_t   i    = 0;
            std::vector<float>   buffer (512);



            MockingboardCard    card (4, MockingboardVariant::SoundSpeech);



            card.Write (0xC440, static_cast<Byte> (Ssi263::kModePhonemeTransitioned << Ssi263::kDurationShift));
            card.Write (0xC443, 0x5F);
            card.Write (0xC442, 0xA2);
            card.Write (0xC440, 0x0F);
            card.SetSampleRate (44100);

            Assert::IsFalse (card.GetSpeech()->IsSilent(), L"Precondition: speaking");

            card.Reset();

            Assert::IsTrue (card.GetSpeech()->IsSilent(),
                            L"Card reset must silence speech immediately, not after the phoneme");

            card.GetSpeechAudioSource()->GeneratePCM (buffer.data(), 512);

            for (i = 0; i < 512; i++)
            {
                Assert::AreEqual (0.0f, buffer[i],
                                  L"and the source must render exact silence afterward");
            }
        }


    private:
        // The classic detection probe: load Timer 1 with $FFFF, tick a few
        // cycles, and read T1C-L twice with a tick between them.
        static void RunDetection (MockingboardCard & card, Byte & outFirst, Byte & outSecond)
        {
            card.Write (0xC404, 0xFF);   // T1C-L latch low
            card.Write (0xC405, 0xFF);   // T1C-H: load counter, start counting

            card.Tick (8);
            outFirst = card.Read (0xC404);

            card.Tick (8);
            outSecond = card.Read (0xC404);
        }

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
            std::vector<float>   buffer (kSamples);



            card.SetSampleRate (44100);

            for (source = 0; source < 2; source++)
            {
                card.GetAudioSource (source)->GeneratePCM (buffer.data(), kSamples);

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
