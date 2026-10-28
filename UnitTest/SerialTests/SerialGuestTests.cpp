#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "Devices/Acia6551.h"
#include "Devices/Serial/HayesModem.h"
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
    //  the 6502 programs the Super Serial Card's 6551 and the bytes arrive at the
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


        //  9600 baud 8N1; send the zero-terminated text at $0340, then store
        //  every byte received at $0800 on.
        static constexpr Byte  kDialProgram[] =
        {
            0xA9, 0x1E, 0x8D, 0xAB, 0xC0,       // LDA #$1E / STA $C0AB
            0xA9, 0x0B, 0x8D, 0xAA, 0xC0,       // LDA #$0B / STA $C0AA
            0xA2, 0x00,                         // LDX #$00
            0xBD, 0x40, 0x03,                   // $030C: LDA $0340,X
            0xF0, 0x0F,                         // BEQ $0320
            0x48,                               // PHA
            0xAD, 0xA9, 0xC0,                   // $0312: LDA $C0A9
            0x29, 0x10,                         // AND #$10   TDRE
            0xF0, 0xF9,                         // BEQ $0312
            0x68,                               // PLA
            0x8D, 0xA8, 0xC0,                   // STA $C0A8
            0xE8,                               // INX
            0xD0, 0xEC,                         // BNE $030C
            0xA0, 0x00,                         // $0320: LDY #$00
            0xAD, 0xA9, 0xC0,                   // $0322: LDA $C0A9
            0x29, 0x08,                         // AND #$08   RDRF
            0xF0, 0xF9,                         // BEQ $0322
            0xAD, 0xA8, 0xC0,                   // LDA $C0A8
            0x99, 0x00, 0x08,                   // STA $0800,Y
            0xC8,                               // INY
            0xD0, 0xF0,                         // BNE $0322
            0x4C, 0x32, 0x03,                   // $0332: JMP $0332
        };


        static std::string ReadReceived (TestMachine & machine)
        {
            constexpr Word  kBuffer = 0x0800;
            constexpr int   kLength = 256;
            std::string     text;
            int             i       = 0;



            for (i = 0; i < kLength; i++)
            {
                text += (char) machine.GetCpu()->ReadByte ((Word) (kBuffer + i));
            }

            return text;
        }


        //  SC-001's call at the guest's end: the guest dials through the
        //  modem, reads CONNECT, and receives the BBS's screen.
        TEST_METHOD (AGuestDialsABbsThroughTheModem)
        {
            constexpr Word                    kText   = 0x0340;
            constexpr uint32_t                kBbs    = 0x0A000009;
            constexpr uint64_t                kRun    = 400000;
            const std::string                 dial    = "ATDT bbs.example.org\r";
            FakeHostNetwork                   network;
            TestMachine                       machine ("Apple2e", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachToCard (config, PortConfig { std::string(), kpszSerialModem });
                services.hostNetwork = &network;
            });
            std::shared_ptr<FakeSocketState>  server;
            std::string                       received;
            size_t                            i       = 0;



            network.SetHostAddress ("bbs.example.org", kBbs);

            machine.PowerCycle();
            machine.GetRefs().superSerialCard->SetFirmware ({});

            for (i = 0; i <= dial.size(); i++)
            {
                machine.GetCpu()->WriteByte ((Word) (kText + i), (i < dial.size()) ? (Byte) dial[i] : 0);
            }

            LoadProgram (machine, kDialProgram, size (kDialProgram));
            machine.RunCycles (kRun);

            server = network.FindConnected ({ kBbs, HayesModem::kDefaultDialPort });
            Assert::IsNotNull (server.get(), L"the guest's ATDT dialed the fake network");

            server->connect = FakeSocketState::ConnectOutcome::Accepted;
            server->inbound = { 'W', 'e', 'l', 'c', 'o', 'm', 'e' };
            machine.RunCycles (kRun);

            //  The program reads only once the whole command is sent, so the
            //  echo overruns the 6551 as it would on the card.
            received = ReadReceived (machine);
            Assert::IsTrue (received.find ("\r\nCONNECT\r\nWelcome") != std::string::npos, TextEncoding::NarrowToWide (received.substr (0, 64)).c_str());
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
