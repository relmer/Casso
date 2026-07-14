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
        uint64_t    GetCycleCount () const override               { return 0; }

        void        SetInterruptLine (CpuInterruptKind kind, bool asserted) override
        {
            if (kind == CpuInterruptKind::kMaskable)
            {
                m_irqAsserted = asserted;
            }
        }

        bool        IrqAsserted () const { return m_irqAsserted; }

    private:
        bool    m_irqAsserted = false;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  MockingboardCardTests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MockingboardCardTests)
    {
    public:
        TEST_METHOD (ClaimsSlotIoPage)
        {
            MockingboardCard    card (4);



            Assert::AreEqual<Word> (0xC400, card.GetStart ());
            Assert::AreEqual<Word> (0xC4FF, card.GetEnd ());
        }


        TEST_METHOD (Address7SelectsSecondVia)
        {
            MockingboardCard    card (4);



            // Writing ORA on each VIA must land on independent chips.
            card.Write (kVia1Base + Via6522::kRegDdra, 0xFF);
            card.Write (kVia2Base + Via6522::kRegDdra, 0xFF);
            card.Write (kVia1Base + Via6522::kRegOra, 0x11);
            card.Write (kVia2Base + Via6522::kRegOra, 0x22);

            Assert::AreEqual<Byte> (0x11, card.GetVia (0).GetOra ());
            Assert::AreEqual<Byte> (0x22, card.GetVia (1).GetOra ());
        }


        TEST_METHOD (RegisterFileMirrorsEverySixteenBytes)
        {
            MockingboardCard    card (4);



            card.Write (kVia1Base + Via6522::kRegDdra, 0xFF);

            // $C401 and $C411 both address ORA on VIA #1.
            card.Write (0xC411, 0x77);
            Assert::AreEqual<Byte> (0x77, card.GetVia (0).GetOra ());
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
            InterruptController   ic (&cpu);
            MockingboardCard      card (4);
            HRESULT               hr = S_OK;



            hr = card.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            // VIA #1: enable T1 IRQ, continuous mode, latch = 99.
            card.Write (kVia1Base + Via6522::kRegIer,
                        static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer1));
            card.Write (kVia1Base + Via6522::kRegAcr, Via6522::kAcrT1Continuous);
            card.Write (kVia1Base + Via6522::kRegT1CL, 0x63);
            card.Write (kVia1Base + Via6522::kRegT1CH, 0x00);

            card.Tick (100);
            Assert::IsTrue (cpu.IrqAsserted (),
                            L"Continuous Timer 1 must drive the shared IRQ line");

            // Reading T1C-L on VIA #1 clears the flag and de-asserts.
            card.Read (kVia1Base + Via6522::kRegT1CL);
            Assert::IsFalse (cpu.IrqAsserted ());
        }


        TEST_METHOD (SecondViaTimerAlsoDrivesIrq)
        {
            MbTestCpu             cpu;
            InterruptController   ic (&cpu);
            MockingboardCard      card (4);
            HRESULT               hr = S_OK;



            hr = card.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            card.Write (kVia2Base + Via6522::kRegIer,
                        static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer1));
            card.Write (kVia2Base + Via6522::kRegAcr, Via6522::kAcrT1Continuous);
            card.Write (kVia2Base + Via6522::kRegT1CL, 0x14);
            card.Write (kVia2Base + Via6522::kRegT1CH, 0x00);

            card.Tick (21);
            Assert::IsTrue (cpu.IrqAsserted (),
                            L"VIA #2's timer shares the same interrupt controller");
        }


        TEST_METHOD (ProgrammedToneProducesAudio)
        {
            MockingboardCard    card (4);
            float               buffer[2000] = {};
            uint32_t            i    = 0;
            float               peak = 0.0f;



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
            Assert::IsNotNull (device.get ());
            Assert::AreEqual<Word> (0xC400, device->GetStart ());
            Assert::AreEqual<Word> (0xC4FF, device->GetEnd ());
        }


    private:
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
