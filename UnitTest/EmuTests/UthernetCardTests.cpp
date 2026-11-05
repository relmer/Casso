#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Core/ComponentRegistry.h"
#include "Core/InterruptController.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/TextEncoding.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Devices/Network/W5100Registers.h"
#include "Machines/Apple2/Common/UthernetCard.h"

#include "../NetworkTests/FakeHostNetwork.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using R = W5100Registers;





namespace UthernetCardTests
{
    static constexpr Word      kSlot3Mode = 0xC0B4;
    static constexpr Word      kSlot3High = 0xC0B5;
    static constexpr Word      kSlot3Low  = 0xC0B6;
    static constexpr Word      kSlot3Data = 0xC0B7;
    static constexpr Word      kSlot5Mode = 0xC0D4;
    static constexpr int       kSlot3     = 3;
    static constexpr int       kSlot5     = 5;
    static constexpr uint64_t  kPrngSeed  = 0x43A55011;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EnableCardSlot
    //
    //  The shipped slot-3 entry, switched on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static void EnableCardSlot (MachineConfig & config)
    {
        for (SlotConfig & slot : config.slots)
        {
            if (slot.device == "uthernet2")
            {
                slot.enabled = true;
            }
        }
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EnableCard
    //
    //  EnableCardSlot, as a build step.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static void EnableCard (MachineConfig & config, MachineBuildServices & services)
    {
        (void) services;

        EnableCardSlot (config);
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IsProbeFound
    //
    //  The card manual's probe: $80 to the mode register reads back $00, then
    //  $03 reads back $03.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static bool IsProbeFound (MemoryBus & bus, Word modeRegister)
    {
        Byte  afterReset = 0;
        Byte  afterMode  = 0;

        bus.WriteByte (modeRegister, R::kMrReset);
        afterReset = bus.ReadByte (modeRegister);

        bus.WriteByte (modeRegister, R::kMrIndirect | R::kMrAutoIncrement);
        afterMode = bus.ReadByte (modeRegister);

        return afterReset == 0x00 && afterMode == 0x03;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UthernetCardTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UthernetCardTests)
    {
    public:

        TEST_METHOD (DecodesOnlyTheFourRegisters)
        {
            UthernetCard  slot3 (kSlot3);
            UthernetCard  slot5 (kSlot5);

            Assert::AreEqual (kSlot3Mode,             slot3.GetStart());
            Assert::AreEqual (kSlot3Data,             slot3.GetEnd());
            Assert::AreEqual (kSlot5Mode,             slot5.GetStart());
            Assert::AreEqual ((Word) (kSlot5Mode + 3), slot5.GetEnd());
        }


        TEST_METHOD (RegistryCreatesTheCard)
        {
            ComponentRegistry  registry;
            DeviceConfig       config;
            MemoryBus          bus;

            ComponentRegistry::RegisterBuiltinDevices (registry);
            Assert::IsTrue (registry.IsRegistered ("uthernet2"));

            config.type    = "uthernet2";
            config.slot    = kSlot5;
            config.hasSlot = true;

            auto  device = registry.Create ("uthernet2", config, bus);

            Assert::IsNotNull (device.get());
            Assert::AreEqual (kSlot5Mode, device->GetStart());
        }


        TEST_METHOD (ManualProbeAndRtrProbe)
        {
            UthernetCard  card (kSlot3);

            card.Write (kSlot3Mode, R::kMrReset);
            Assert::AreEqual ((Byte) 0x00, card.Read (kSlot3Mode));

            card.Write (kSlot3Mode, R::kMrIndirect | R::kMrAutoIncrement);
            Assert::AreEqual ((Byte) 0x03, card.Read (kSlot3Mode));

            card.Write (kSlot3High, 0x00);
            card.Write (kSlot3Low,  (Byte) R::kRtr);
            Assert::AreEqual ((Byte) 0x07, card.Read (kSlot3Data));
            Assert::AreEqual ((Byte) 0xD0, card.Read (kSlot3Data));
        }


        TEST_METHOD (InterruptLineFollowsIrAndImr)
        {
            InterruptController  ic;
            UthernetCard         card (kSlot3);

            AssertSucceeded (card.AttachInterruptController (&ic));

            card.Write (kSlot3Mode, R::kMrIndirect | R::kMrAutoIncrement);
            card.Write (kSlot3High, 0x00);
            card.Write (kSlot3Low,  (Byte) R::kImr);
            card.Write (kSlot3Data, 0x01);

            card.GetChip().RaiseSocketInterrupt (0, R::kSnIrReceive);
            card.Tick (UthernetCard::kPumpCycles);
            Assert::IsTrue (ic.IsAnyAsserted(), L"S0_INT unmasked");

            // Clear S0_IR's RECV bit by writing 1.
            card.Write (kSlot3High, 0x04);
            card.Write (kSlot3Low,  (Byte) R::kSnIr);
            card.Write (kSlot3Data, R::kSnIrReceive);
            Assert::IsFalse (ic.IsAnyAsserted(), L"the write itself releases the line");
        }


        TEST_METHOD (CtrlResetLeavesTheChipAndReassertsTheLine)
        {
            InterruptController  ic;
            UthernetCard         card (kSlot3);

            AssertSucceeded (card.AttachInterruptController (&ic));

            card.Write (kSlot3Mode, R::kMrIndirect | R::kMrAutoIncrement);
            card.Write (kSlot3High, 0x00);
            card.Write (kSlot3Low,  (Byte) R::kImr);
            card.Write (kSlot3Data, 0x01);
            card.GetChip().RaiseSocketInterrupt (0, R::kSnIrReceive);
            card.Tick (UthernetCard::kPumpCycles);

            card.SoftReset();
            ic.SoftReset();
            Assert::IsFalse (ic.IsAnyAsserted(), L"the machine's reset clears the controller");

            card.Tick (UthernetCard::kPumpCycles);
            Assert::IsTrue (ic.IsAnyAsserted(), L"the chip has S0_INT pending");
            Assert::AreEqual ((Byte) 0x03, card.Read (kSlot3Mode), L"MR survives Ctrl-Reset");
        }


        TEST_METHOD (PowerCycleResetsThePointer)
        {
            Prng          prng (kPrngSeed);
            UthernetCard  card (kSlot3);

            card.Write (kSlot3Mode, R::kMrIndirect);
            card.Write (kSlot3High, 0x12);
            card.Write (kSlot3Low,  0x34);

            card.PowerCycle (prng);

            Assert::AreEqual ((Word) 0, card.GetChip().GetAddressPointer());
            Assert::AreEqual ((Byte) 0, card.Read (kSlot3Mode));
        }


        //  A saved machine has the W5100's registers, buffers and address
        //  pointer, so a load, or a replay from a keyframe, puts them back.
        TEST_METHOD (TheMachineStateHasTheChipsRegistersAndPointer)
        {
            TestMachine     machine ("Apple2e", [] (MachineConfig & config) { EnableCard (config, *(MachineBuildServices *) nullptr); });
            UthernetCard  * card    = machine.GetRefs().uthernet;
            MemoryBus     & bus     = machine.GetMemoryBus();
            StateWriter     writer;
            HRESULT         hr      = S_OK;
            Byte            ip[]    = { 192, 168, 1, 10 };



            Assert::IsNotNull (card);

            bus.WriteByte (kSlot3Mode, R::kMrIndirect | R::kMrAutoIncrement);
            bus.WriteByte (kSlot3High, 0x00);
            bus.WriteByte (kSlot3Low,  (Byte) R::kSipr);

            for (Byte each : ip)
            {
                bus.WriteByte (kSlot3Data, each);
            }

            hr = machine.SaveState (writer);
            Assert::AreEqual (S_OK, hr, L"SaveState");

            bus.WriteByte (kSlot3High, 0x00);
            bus.WriteByte (kSlot3Low,  (Byte) R::kSipr);
            bus.WriteByte (kSlot3Data, 10);
            bus.WriteByte (kSlot3Mode, R::kMrIndirect);

            {
                StateReader  reader (writer.GetBytes());

                hr = machine.LoadState (reader);
                Assert::AreEqual (S_OK, hr, L"LoadState");
            }

            Assert::AreEqual ((Byte) 192, card->GetChip().PeekCommon (R::kSipr), L"the IP address register");
            Assert::AreEqual ((Byte) (R::kMrIndirect | R::kMrAutoIncrement), card->GetChip().PeekCommon (R::kMr), L"the mode register");
            Assert::AreEqual ((Word) (R::kSipr + 4), card->GetChip().GetAddressPointer(), L"the address pointer");
        }


        TEST_METHOD (IdleCardCreatesNoHostSockets)
        {
            FakeHostNetwork      network;
            NetworkCardSettings  settings;
            UthernetCard         card (kSlot3);
            int                  i        = 0;

            card.AttachNetwork (&network, settings);

            for (i = 0; i < 100; i++)
            {
                card.Tick (UthernetCard::kPumpCycles);
            }

            Assert::IsFalse (card.GetBridge().IsActive());
            Assert::AreEqual ((size_t) 0, network.GetSockets().size());
        }


        TEST_METHOD (ApplySettingsReopensTheForwardsOnARunningCard)
        {
            FakeHostNetwork                    network;
            NetworkCardSettings                settings;
            UthernetCard                       card (kSlot3);
            std::shared_ptr<FakeSocketState>   before;
            std::shared_ptr<FakeSocketState>   after;

            settings.portForwards = { { PortForwardProtocol::Tcp, 8080, 80 } };
            card.AttachNetwork (&network, settings);

            before = network.FindBound (HostSocketKind::Tcp, 8080);
            Assert::IsNotNull (before.get());
            Assert::AreEqual (VirtualSubnet::kLoopbackIp, before->local.ipv4);

            settings.lanAccess = true;
            card.ApplySettings (settings);

            after = network.FindBound (HostSocketKind::Tcp, 8080);
            Assert::IsNotNull (after.get());
            Assert::IsTrue (before->destroyed, L"the loopback listener is closed");
            Assert::AreEqual (VirtualSubnet::kAnyIp, after->local.ipv4);
            Assert::IsTrue (after->listening);
        }

        TEST_METHOD (ShippedSlottedMachinesCarryTheCardDisabled)
        {
            for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
            {
                TestMachine  machine (id);
                bool         found = false;

                for (const SlotConfig & slot : machine.GetConfig().slots)
                {
                    if (slot.device == "uthernet2")
                    {
                        found = true;
                        Assert::AreEqual (kSlot3, slot.slot);
                        Assert::IsFalse (slot.enabled);
                    }
                }

                Assert::IsTrue (found, TextEncoding::NarrowToWide (id).c_str());
                Assert::IsNull (machine.GetRefs().uthernet, L"a disabled card is not installed");
            }
        }


        TEST_METHOD (TheIicHasNoCard)
        {
            TestMachine  machine ("Apple2c");

            for (const SlotConfig & slot : machine.GetConfig().slots)
            {
                Assert::AreNotEqual (std::string ("uthernet2"), slot.device);
            }
        }


        TEST_METHOD (EnabledCardAnswersTheProbeInItsSlotOnly)
        {
            TestMachine  machine ("Apple2e", TestMachine::Slots::AsShipped, EnableCard);

            Assert::IsNotNull (machine.GetRefs().uthernet);
            Assert::IsTrue  (IsProbeFound (machine.GetMemoryBus(), kSlot3Mode), L"slot 3");
            Assert::IsFalse (IsProbeFound (machine.GetMemoryBus(), kSlot5Mode), L"slot 5 is empty");
        }


        TEST_METHOD (LowerFourLocationsBehaveAsAnEmptySlot)
        {
            constexpr Word  kSlot3Base = 0xC0B0;

            TestMachine  without ("Apple2e");
            TestMachine  with    ("Apple2e", TestMachine::Slots::AsShipped, EnableCard);
            Word         address  = 0;

            for (address = kSlot3Base; address < kSlot3Mode; address++)
            {
                Assert::AreEqual (without.GetMemoryBus().ReadByte (address),
                                  with.GetMemoryBus().ReadByte (address),
                                  std::format (L"${:04X}", address).c_str());
            }
        }
    };
}
