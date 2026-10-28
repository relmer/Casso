#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "../EmuTests/TestMachine.h"
#include "../NetworkTests/FakeHostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SerialGuestTestNs
{
    static constexpr Word      kProgram    = 0x0300;
    static constexpr uint16_t  kListenPort = 1977;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialGuestTests
    //
    //  Guest code on a whole machine talking through a serial attachment:
    //  the 6502 programs the Super Serial Card's 6551 and the bytes reach the
    //  PC side of the fake network, and back.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialGuestTests)
    {
    public:
        static void AttachToCard (MachineConfig & config, const PortConfig & port)
        {
            for (SlotConfig & slot : config.slots)
            {
                if (slot.device == kpszSuperSerialCardDevice)
                {
                    slot.enabled = true;
                    slot.ports   = { port };
                }
            }
        }


        static void LoadProgram (TestMachine & machine, const Byte * bytes, size_t count)
        {
            size_t  i = 0;



            for (i = 0; i < count; ++i)
            {
                machine.GetCpu()->WriteByte (static_cast<Word> (kProgram + i), bytes[i]);
            }

            machine.GetCpu()->SetPC (kProgram);
        }


        //  9600 baud 8N1, then echo every byte received.
        static constexpr Byte  kEchoProgram[] =
        {
            0xA9, 0x1E, 0x8D, 0xAB, 0xC0,       // LDA #$1E / STA $C0AB
            0xA9, 0x0B, 0x8D, 0xAA, 0xC0,       // LDA #$0B / STA $C0AA
            0xAD, 0xA9, 0xC0,                   // $030A: LDA $C0A9
            0x29, 0x08,                         // AND #$08   RDRF
            0xF0, 0xF9,                         // BEQ $030A
            0xAD, 0xA8, 0xC0,                   // LDA $C0A8
            0x8D, 0xA8, 0xC0,                   // STA $C0A8
            0x4C, 0x0A, 0x03,                   // JMP $030A
        };


        static PortConfig MakeTcpListenPort()
        {
            PortConfig  port;



            port.device         = kpszSerialTcp;
            port.serial.listen  = true;
            port.serial.tcpPort = kListenPort;

            return port;
        }


        //  Runs until the client has `count` bytes or the time is up.
        static void RunUntilSent (TestMachine & machine, const FakeSocketState & client, size_t count)
        {
            constexpr uint64_t  kSlice   = 1000;
            constexpr uint64_t  kTimeout = 200000;
            uint64_t            ran      = 0;



            for (ran = 0; ran < kTimeout && client.sent.size() < count; ran += kSlice)
            {
                machine.RunCycles (kSlice);
            }
        }


        TEST_METHOD (AGuestEchoesWhatATcpClientSends)
        {
            FakeHostNetwork                   network;
            TestMachine                       machine ("Apple2e", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachToCard (config, MakeTcpListenPort());
                services.hostNetwork = &network;
            });
            std::shared_ptr<FakeSocketState>  client;



            machine.PowerCycle();
            machine.GetRefs().superSerialCard->SetFirmware ({});
            LoadProgram (machine, kEchoProgram, size (kEchoProgram));
            machine.RunCycles (Acia6551::kIdlePollCycles * 2);

            client          = network.OfferConnection (network.FindBound (HostSocketKind::Tcp, kListenPort), { 0x7F000001, 50000 });
            client->inbound = { 'H', 'I', 0xFF };

            RunUntilSent (machine, *client, 3);

            Assert::IsTrue (std::vector<Byte> { 'H', 'I', 0xFF } == client->sent);
        }
    };
}
