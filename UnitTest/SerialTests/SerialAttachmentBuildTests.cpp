#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/IAciaEndpoint.h"
#include "Devices/Serial/AciaComPortEndpoint.h"
#include "Devices/Serial/SerialLinkReports.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "../EmuTests/TestMachine.h"
#include "FakeHostSerialPorts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SerialAttachmentBuildTestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialAttachmentBuildTests
    //
    //  What MachineBuilder plugs into each 6551: the Super Serial Card's
    //  connector from its slot's first port, and each //c port from the
    //  machine's ports list by name.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialAttachmentBuildTests)
    {
    public:
        static void SetApple2cPort (MachineConfig & config, const char * name, const char * device)
        {
            for (PortConfig & port : config.ports)
            {
                if (port.name == name)
                {
                    port.device = device;
                }
            }
        }


        static void AttachToCard (MachineConfig & config, const char * device)
        {
            for (SlotConfig & slot : config.slots)
            {
                if (slot.device == kpszSuperSerialCardDevice)
                {
                    slot.enabled = true;
                    slot.ports   = { PortConfig { std::string(), device } };
                }
            }
        }


        //  As a //c ships, nothing is plugged into either port.
        TEST_METHOD (TheApple2cPortsDefaultToNothing)
        {
            TestMachine  machine ("Apple2c");



            Assert::AreEqual<size_t> (2, machine.GetRefs().serialChips.size());

            for (Acia6551 * acia : machine.GetRefs().serialChips)
            {
                Assert::IsNull (acia->GetEndpoint(), L"an empty device is nothing");
            }
        }


        TEST_METHOD (AnApple2cPortTakesItsConfiguredAttachment)
        {
            TestMachine  machine ("Apple2c", [] (MachineConfig & config)
            {
                SetApple2cPort (config, kpszSerialPort2Name, "serial-unknown-device");
            });



            Assert::IsNull    (machine.GetRefs().serialChips[0]->GetEndpoint(), L"port 1 as shipped");
            Assert::IsNull    (machine.GetRefs().serialChips[1]->GetEndpoint(), L"port 2 has the attachment it was given");
            Assert::AreEqual<size_t> (1, machine.GetSerialLinkReports().GetReports().size(), L"an unknown attachment is reported");
            Assert::AreEqual (std::string ("Serial port 2"), machine.GetSerialLinkReports().GetReports()[0].portTitle);
        }


        TEST_METHOD (AnApple2cPortOpensItsComPort)
        {
            FakeHostSerialPorts                   ports;
            std::shared_ptr<FakeSerialPortState>  com3    = ports.AddPort ("COM3");
            TestMachine                           machine ("Apple2c", TestMachine::Slots::AsShipped, [&ports] (MachineConfig & config, MachineBuildServices & services)
            {
                for (PortConfig & port : config.ports)
                {
                    if (port.name == kpszSerialPort1Name)
                    {
                        port.device         = kpszSerialComPort;
                        port.serial.comPort = "COM3";
                    }
                }

                services.hostSerialPorts = &ports;
            });



            Assert::IsNotNull (dynamic_cast<AciaComPortEndpoint *> (machine.GetRefs().serialChips[0]->GetEndpoint()));
            Assert::IsTrue (com3->isOpen);
            Assert::AreEqual<size_t> (0, machine.GetSerialLinkReports().GetReports().size());
        }


        //  FR-023: a rebuild closes the old machine's attachments before the
        //  new machine opens its attachments, so the same COM port opens again.
        TEST_METHOD (ClosingTheAttachmentsFreesTheirPortsForTheNextBuild)
        {
            FakeHostSerialPorts                   ports;
            std::shared_ptr<FakeSerialPortState>  com3      = ports.AddPort ("COM3");
            auto                                  customize = [&ports] (MachineConfig & config, MachineBuildServices & services)
            {
                for (PortConfig & port : config.ports)
                {
                    if (port.name == kpszSerialPort1Name)
                    {
                        port.device         = kpszSerialComPort;
                        port.serial.comPort = "COM3";
                    }
                }

                services.hostSerialPorts = &ports;
            };
            TestMachine                           old       ("Apple2c", TestMachine::Slots::AsShipped, customize);



            Assert::IsTrue (com3->isOpen);

            {
                TestMachine  overlapping ("Apple2c", TestMachine::Slots::AsShipped, customize);

                Assert::AreEqual (std::string ("COM3 is in use by another program."), overlapping.GetSerialLinkReports().GetReports()[0].text,
                                  L"while the old machine has the port open, a new one cannot open it");
            }

            old.CloseSerialAttachments();
            Assert::IsFalse (com3->isOpen);
            Assert::IsNull  (old.GetRefs().serialChips[0]->GetEndpoint(), L"the chip is detached first");

            {
                TestMachine  rebuilt ("Apple2c", TestMachine::Slots::AsShipped, customize);

                Assert::IsTrue (com3->isOpen);
                Assert::AreEqual<size_t> (0, rebuilt.GetSerialLinkReports().GetReports().size());
            }
        }


        TEST_METHOD (WithoutTheSeamsTheHostAttachmentsAreReported)
        {
            TestMachine  machine ("Apple2c", [] (MachineConfig & config)
            {
                SetApple2cPort (config, kpszSerialPort1Name, kpszSerialComPort);
                SetApple2cPort (config, kpszSerialPort2Name, kpszSerialTcp);
            });



            Assert::AreEqual<size_t> (2, machine.GetSerialLinkReports().GetReports().size(), L"the scratch machine's case: nothing opened, both reported");
            Assert::IsTrue ((machine.GetRefs().serialChips[0]->GetStatus() & Acia6551::kStatusDcd) != 0, L"DCD off");
        }


        TEST_METHOD (TheCardsConnectorTakesTheSlotsFirstPort)
        {
            TestMachine  machine ("Apple2e", [] (MachineConfig & config)
            {
                AttachToCard (config, kpszSerialLoopback);
            });
            SuperSerialCard  * card = machine.GetRefs().superSerialCard;



            Assert::IsNotNull (card);
            Assert::IsNotNull (dynamic_cast<AciaLoopbackEndpoint *> (card->GetAcia().GetEndpoint()));
        }


        TEST_METHOD (TheCardsEmptyConnectorHasNothingOnIt)
        {
            TestMachine  machine ("Apple2e", [] (MachineConfig & config)
            {
                AttachToCard (config, "");
            });



            Assert::IsNull (machine.GetRefs().superSerialCard->GetAcia().GetEndpoint());
            Assert::AreEqual<size_t> (0, machine.GetSerialLinkReports().GetReports().size());
        }
    };
}
