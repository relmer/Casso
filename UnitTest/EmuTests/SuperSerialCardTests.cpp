#include "Pch.h"

#include "Core/ComponentRegistry.h"
#include "Core/InterruptController.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "ICpu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace SuperSerialCardTestNs
{
    // Slot 2: I/O window $C0A0-$C0AF, ROM page $C200-$C2FF.
    static constexpr int   kSlot         = 2;
    static constexpr Word  kIoStart      = 0xC0A0;
    static constexpr Word  kIoEnd        = 0xC0AF;
    static constexpr Word  kAddrDipSw1   = 0xC0A1;
    static constexpr Word  kAddrDipSw2   = 0xC0A2;
    static constexpr Word  kAddrCommand  = 0xC0AA;
    static constexpr Word  kAddrControl  = 0xC0AB;
    static constexpr Word  kPageStart    = 0xC200;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SscTestCpu
    //
    //  The state of the maskable IRQ line is stored for the test.
    //
    ////////////////////////////////////////////////////////////////////////////

    class SscTestCpu : public ICpu
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
    //  SuperSerialCardTests
    //
    //  The card on its own: its I/O window, its firmware mapping and latch,
    //  and SW2-6. The firmware here is synthetic -- every byte distinct from
    //  its neighbors -- since Apple's is not in the test tree.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SuperSerialCardTests)
    {
    public:
        TEST_METHOD (IoWindowIsTheSlotsSixteenBytes)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);



            Assert::AreEqual (kIoStart, card.GetStart());
            Assert::AreEqual (kIoEnd,   card.GetEnd());
        }


        TEST_METHOD (SwitchRegistersReadTheSwitches)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);
            SscSwitches  sw       = SscSwitches::MakeDefault();
            Byte         floating = bus.GetFloatingBusValue();



            Assert::AreEqual (sw.GetDipSwitch1Byte (floating),       card.Read (kAddrDipSw1));
            Assert::AreEqual (sw.GetDipSwitch2Byte (true, floating), card.Read (kAddrDipSw2));

            sw.SetOn (SscBank::Sw1, 1, true);
            card.SetSwitches (sw);
            Assert::AreEqual (sw.GetDipSwitch1Byte (floating), card.Read (kAddrDipSw1),
                              L"a switch change reads back without a reset");
        }


        TEST_METHOD (UndecodedOffsetsReadTheFloatingBus)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);



            Assert::AreEqual (bus.GetFloatingBusValue(), card.Read (kIoStart));
            Assert::AreEqual (bus.GetFloatingBusValue(), card.Read (static_cast<Word> (kIoStart + 5)));
        }


        TEST_METHOD (AciaRegistersPassThrough)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);



            card.Write (kAddrControl, 0x1E);
            card.Write (kAddrCommand, 0x0B);

            Assert::AreEqual<Byte> (0x1E, card.Read (kAddrControl));
            Assert::AreEqual<Byte> (0x0B, card.Read (kAddrCommand));
            Assert::AreEqual<Byte> (0x1E, card.GetAcia().GetControl());
        }


        TEST_METHOD (PageReadsTheFirmwaresLastPageAndSelects)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);
            vector<Byte>     firmware = MakeFirmware();



            Assert::AreEqual (S_OK, card.SetFirmware (firmware));

            Assert::AreEqual (firmware[0x705], card.GetSlotPage().Read (0xC205));
            Assert::AreEqual (firmware[0x7FF], card.GetSlotPage().Read (0xC2FF));
            Assert::IsTrue (card.IsExpansionRomSelected());
        }


        TEST_METHOD (ExpansionReadsTheFirmwaresFirstPart)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);
            vector<Byte>     firmware = MakeFirmware();



            Assert::AreEqual (S_OK, card.SetFirmware (firmware));

            Assert::AreEqual (firmware[0x000], card.ReadExpansionRom (0xC800));
            Assert::AreEqual (firmware[0x6FF], card.ReadExpansionRom (0xCEFF));
            Assert::AreEqual (bus.GetFloatingBusValue(), card.ReadExpansionRom (0xCF00),
                              L"$CFxx is not mapped");
        }


        TEST_METHOD (PascalIdBytesComeFromTheFirmware)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);
            vector<Byte>     firmware (SuperSerialCard::kFirmwareSize, 0);



            // Apple's identification bytes for a serial card, at $Cs05,
            // $Cs07, $Cs0B and $Cs0C: ROM $705, $707, $70B, $70C.
            firmware[0x705] = 0x38;
            firmware[0x707] = 0x18;
            firmware[0x70B] = 0x01;
            firmware[0x70C] = 0x31;
            Assert::AreEqual (S_OK, card.SetFirmware (firmware));

            Assert::AreEqual<Byte> (0x38, card.GetSlotPage().Read (0xC205));
            Assert::AreEqual<Byte> (0x18, card.GetSlotPage().Read (0xC207));
            Assert::AreEqual<Byte> (0x01, card.GetSlotPage().Read (0xC20B));
            Assert::AreEqual<Byte> (0x31, card.GetSlotPage().Read (0xC20C));
        }


        TEST_METHOD (WriteToThePageSelectsToo)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);



            card.GetSlotPage().Write (kPageStart, 0);
            Assert::IsTrue (card.IsExpansionRomSelected());
        }


        TEST_METHOD (ResetDeselects)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);



            (void) card.GetSlotPage().Read (kPageStart);
            card.SoftReset();
            Assert::IsFalse (card.IsExpansionRomSelected());

            (void) card.GetSlotPage().Read (kPageStart);
            card.Reset();
            Assert::IsFalse (card.IsExpansionRomSelected());
        }


        TEST_METHOD (NoFirmwareReadsTheFloatingBus)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);



            Assert::IsFalse (card.HasFirmware());
            Assert::AreEqual (bus.GetFloatingBusValue(), card.GetSlotPage().Read (0xC205));
            Assert::AreEqual (bus.GetFloatingBusValue(), card.ReadExpansionRom (0xC800));
            Assert::IsTrue (card.IsExpansionRomSelected(), L"the latch works without a ROM");
        }


        TEST_METHOD (WrongSizeFirmwareIsRefused)
        {
            MemoryBus        bus;
            SuperSerialCard  card (kSlot, bus);
            HRESULT          hr = S_OK;



            hr = card.SetFirmware (vector<Byte> (0x100, 0xEA));

            Assert::IsTrue (FAILED (hr));
            Assert::IsFalse (card.HasFirmware());
        }


        TEST_METHOD (Sw26GatesTheIrqLine)
        {
            const Byte              bytes[] = { 0x41 };
            MemoryBus               bus;
            SscTestCpu              cpu;
            InterruptController     ic (&cpu);
            SuperSerialCard         card (kSlot, bus);
            SscSwitches             sw = SscSwitches::MakeDefault();
            HRESULT                 hr = S_OK;



            hr = card.GetAcia().AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            card.Write (kAddrControl, 0x1F);
            card.Write (kAddrCommand, Acia6551::kCommandDtr);       // receiver IRQ enabled
            card.GetAcia().EnqueueReceivedBytes (bytes, size (bytes));
            card.GetAcia().Tick (1);
            card.GetAcia().Tick (static_cast<uint32_t> (ceil (card.GetAcia().GetTransmitCharacterCycles())));

            Assert::IsTrue ((card.GetAcia().GetStatus() & Acia6551::kStatusIrq) != 0, L"the 6551 latches its IRQ");
            Assert::IsFalse (cpu.IrqAsserted(), L"with SW2-6 OFF (the default) the IRQ is not connected to the bus");

            sw.SetOn (SscBank::Sw2, 6, true);
            card.SetSwitches (sw);
            Assert::IsTrue (cpu.IrqAsserted(), L"SW2-6 ON connects it");
        }


        TEST_METHOD (FactoryCreatesTheCardInItsSlot)
        {
            ComponentRegistry           registry;
            MemoryBus                   bus;
            DeviceConfig                config;
            unique_ptr<MemoryDevice>    device;



            ComponentRegistry::RegisterBuiltinDevices (registry);
            Assert::IsTrue (registry.IsRegistered (kpszSuperSerialCardDevice));

            config.type    = kpszSuperSerialCardDevice;
            config.slot    = kSlot;
            config.hasSlot = true;

            device = registry.Create (kpszSuperSerialCardDevice, config, bus);
            Assert::IsNotNull (dynamic_cast<SuperSerialCard *> (device.get()));
            Assert::AreEqual (kIoStart, device->GetStart());
        }

    private:
        static vector<Byte> MakeFirmware()
        {
            constexpr Byte  kPattern = 0xA5;
            vector<Byte>    firmware (SuperSerialCard::kFirmwareSize);



            for (size_t i = 0; i < firmware.size(); ++i)
            {
                firmware[i] = static_cast<Byte> ((i ^ (i >> 8)) ^ kPattern);
            }

            return firmware;
        }
    };
}
