#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/Serial/AciaTcpEndpoint.h"
#include "Devices/Serial/SerialLinkReports.h"
#include "Devices/Serial/TelnetCodec.h"
#include "../NetworkTests/FakeHostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace AciaTcpEndpointTestNs
{
    static constexpr Word      kBase       = 0xC0A8;
    static constexpr Word      kAddrData   = 0xC0A8;
    static constexpr Word      kAddrStatus = 0xC0A9;
    static constexpr Word      kAddrCmd    = 0xC0AA;
    static constexpr Word      kAddrCtrl   = 0xC0AB;
    static constexpr Byte      kCmdPolled  = Acia6551::kCommandDtr | Acia6551::kCommandRxIrqDisable | Acia6551::kCommandTicTxIrqOff;
    static constexpr Byte      kCmdRxIrq   = Acia6551::kCommandDtr | Acia6551::kCommandTicTxIrqOff;
    static constexpr Byte      kCtrl9600   = 0x1E;
    static constexpr uint16_t  kPort       = 1977;
    static constexpr uint32_t  kHost       = 0x0A000002;
    static constexpr char      kTitle[]    = "Serial port 2";




    ////////////////////////////////////////////////////////////////////////////
    //
    //  TcpRig
    //
    //  A 6551 with a TCP attachment over the fake network, set up for 9600
    //  baud polled I/O.
    //
    ////////////////////////////////////////////////////////////////////////////

    class TcpRig
    {
    public:
        explicit TcpRig (AciaTcpEndpoint::Settings settings, Byte command = kCmdPolled)
            : endpoint (acia, &network, reports, kTitle, settings)
        {
            network.SetHostAddress ("peer.example.org", kHost);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  command);
            acia.SetEndpoint (&endpoint);
        }

        ~TcpRig()
        {
            acia.SetEndpoint (nullptr);
        }

        void  Run (uint64_t cycles)
        {
            uint64_t  i = 0;



            for (i = 0; i < cycles; i += kStep)
            {
                acia.Tick (kStep);
            }
        }

        std::shared_ptr<FakeSocketState>  Accept()
        {
            std::shared_ptr<FakeSocketState>  client = network.OfferConnection (network.FindBound (HostSocketKind::Tcp, kPort), { 0x7F000001, 50000 });



            Run (Acia6551::kIdlePollCycles);

            return client;
        }

        std::string  GetReport() const
        {
            std::vector<SerialLinkReports::Report>  listed = reports.GetReports();



            return listed.empty() ? std::string() : listed.back().text;
        }

        static constexpr uint32_t  kStep = 4;

        Acia6551           acia { kBase };
        FakeHostNetwork    network;
        SerialLinkReports  reports;
        AciaTcpEndpoint    endpoint;
    };




    static AciaTcpEndpoint::Settings MakeListening (bool isTelnet = false, int port = kPort)
    {
        AciaTcpEndpoint::Settings  settings;



        settings.port     = port;
        settings.isListen = true;
        settings.isTelnet = isTelnet;

        return settings;
    }




    static AciaTcpEndpoint::Settings MakeConnecting (const char * host = "peer.example.org")
    {
        AciaTcpEndpoint::Settings  settings;



        settings.host = host;
        settings.port = kPort;

        return settings;
    }




    ////////////////////////////////////////////////////////////////////////////
    //
    //  AciaTcpEndpointTests
    //
    //  A 6551 on TCP: DCD and DSR follow the connection, bytes pass both ways
    //  at the guest's pace, and each way the line cannot be made is reported.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AciaTcpEndpointTests)
    {
    public:
        TEST_METHOD (TheLinesAreOffUntilAClientConnects)
        {
            TcpRig  rig (MakeListening(), kCmdRxIrq);



            Assert::IsTrue ((rig.acia.GetStatus() & (Acia6551::kStatusDcd | Acia6551::kStatusDsr)) == (Acia6551::kStatusDcd | Acia6551::kStatusDsr),
                            L"no connection: DCD and DSR off");
            Assert::IsTrue (rig.acia.IsCtsTrue());

            (void) rig.acia.Read (kAddrStatus);
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusIrq) == 0);

            rig.Accept();

            Assert::IsTrue ((rig.acia.GetStatus() & (Acia6551::kStatusDcd | Acia6551::kStatusDsr)) == 0, L"connected: both on");
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusIrq) != 0, L"the line change raises the interrupt");
        }


        TEST_METHOD (BytesPassBothWaysFFIncluded)
        {
            TcpRig                            rig (MakeListening());
            std::shared_ptr<FakeSocketState>  client = rig.Accept();



            client->inbound = { 0xFF, 'A' };
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::AreEqual<Byte> ('A', rig.acia.Read (kAddrData), L"the last of the two landed");

            rig.acia.Write (kAddrData, 0xFF);
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue (std::vector<Byte> { 0xFF } == client->sent);
        }


        TEST_METHOD (TheTelnetOptionDoublesFFOnTheWire)
        {
            TcpRig                            rig    (MakeListening (true));
            std::shared_ptr<FakeSocketState>  client = rig.Accept();



            rig.acia.Write (kAddrData, 0xFF);
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue (std::vector<Byte> { 0xFF, 0xFF } == client->sent);
        }


        TEST_METHOD (AClientDisconnectingTurnsTheLinesOff)
        {
            TcpRig                            rig (MakeListening());
            std::shared_ptr<FakeSocketState>  client = rig.Accept();



            client->inboundEof = true;
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue ((rig.acia.GetStatus() & (Acia6551::kStatusDcd | Acia6551::kStatusDsr)) == (Acia6551::kStatusDcd | Acia6551::kStatusDsr));
        }


        TEST_METHOD (BytesArePacedAtTheGuestsBaudRate)
        {
            TcpRig                            rig        (MakeListening());
            std::shared_ptr<FakeSocketState>  client     = rig.Accept();
            uint64_t                          charCycles = (uint64_t) ceil (rig.acia.GetTransmitCharacterCycles());
            int                               landed     = 0;
            uint64_t                          elapsed    = 0;



            client->inbound = { 1, 2, 3, 4, 5, 6, 7, 8 };

            for (elapsed = 0; elapsed < charCycles * 10; elapsed += TcpRig::kStep)
            {
                rig.acia.Tick (TcpRig::kStep);

                if (rig.acia.GetStatus() & Acia6551::kStatusRxFull)
                {
                    (void) rig.acia.Read (kAddrData);
                    landed++;
                }
            }

            Assert::AreEqual (8, landed, L"all eight, one character time apart");

            client->inbound = { 9, 10, 11, 12 };
            landed          = 0;

            for (elapsed = 0; elapsed < charCycles * 2; elapsed += TcpRig::kStep)
            {
                rig.acia.Tick (TcpRig::kStep);

                if (rig.acia.GetStatus() & Acia6551::kStatusRxFull)
                {
                    (void) rig.acia.Read (kAddrData);
                    landed++;
                }
            }

            Assert::IsTrue (landed <= 2, L"no faster than the baud rate allows");
        }


        TEST_METHOD (AFloodWaitsInThePcsBuffer)
        {
            TcpRig                            rig (MakeListening());
            std::shared_ptr<FakeSocketState>  client = rig.Accept();



            client->inbound.assign (10000, 'x');
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::IsTrue (rig.acia.GetQueuedReceiveCount() <= AciaTcpEndpoint::kReceiveQueueLimit);
            Assert::IsTrue (client->inbound.size() > 9000, L"the rest stays on the PC's side");
        }


        TEST_METHOD (AConnectionIsMadeOutToTheHost)
        {
            TcpRig                            rig (MakeConnecting());
            std::shared_ptr<FakeSocketState>  server;



            rig.Run (Acia6551::kIdlePollCycles);
            server = rig.network.FindConnected ({ kHost, kPort });
            Assert::IsNotNull (server.get());

            server->connect = FakeSocketState::ConnectOutcome::Accepted;
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) == 0, L"DCD on");
        }


        TEST_METHOD (AnOfflineHostIsReportedAndDcdStaysOff)
        {
            TcpRig  rig (MakeConnecting());



            rig.Run (Acia6551::kIdlePollCycles);
            rig.network.FindConnected ({ kHost, kPort })->connect = FakeSocketState::ConnectOutcome::Refused;
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::AreEqual (std::string ("Cannot connect to peer.example.org:1977. The host is offline, or nothing is listening on that port."), rig.GetReport());
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) != 0);
        }


        TEST_METHOD (AFailedConnectIsTriedAgain)
        {
            TcpRig  rig (MakeConnecting());



            rig.Run (Acia6551::kIdlePollCycles);
            rig.network.FindConnected ({ kHost, kPort })->connect = FakeSocketState::ConnectOutcome::Refused;
            rig.Run (AciaTcpEndpoint::kRetrySeconds * Acia6551::kDefaultCpuClockHz + Acia6551::kIdlePollCycles * 2);

            rig.network.FindConnected ({ kHost, kPort })->connect = FakeSocketState::ConnectOutcome::Accepted;
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) == 0, L"connected on the second attempt");
            Assert::AreEqual (std::string(), rig.GetReport(), L"the report is cleared");
        }


        TEST_METHOD (AnUnknownHostIsReported)
        {
            TcpRig  rig (MakeConnecting ("no.such.host"));



            rig.Run (Acia6551::kIdlePollCycles);

            Assert::AreEqual (std::string ("Cannot find the host no.such.host."), rig.GetReport());
        }


        TEST_METHOD (AListenPortInUseIsReported)
        {
            Acia6551           acia (kBase);
            FakeHostNetwork    network;
            SerialLinkReports  reports;
            AciaTcpEndpoint    endpoint (acia, &network, reports, kTitle, MakeListening());



            network.OccupyPort (HostSocketKind::Tcp, kPort);
            acia.SetEndpoint (&endpoint);

            Assert::AreEqual<size_t> (1, reports.GetReports().size());
            Assert::AreEqual (std::string ("Port 1977 is in use by another program."), reports.GetReports()[0].text);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusDcd) != 0);

            acia.SetEndpoint (nullptr);
        }


        TEST_METHOD (APortOutOfRangeIsReported)
        {
            constexpr int  kTooHigh = 70000;
            TcpRig         rig (MakeListening (false, kTooHigh));



            Assert::AreEqual (std::string ("The TCP port must be from 1 to 65535."), rig.GetReport());
        }


        TEST_METHOD (DroppingTheConnectionClosesItAndTurnsDcdOff)
        {
            TcpRig                            rig (MakeListening());
            std::shared_ptr<FakeSocketState>  client = rig.Accept();



            rig.endpoint.DropConnection();

            Assert::IsTrue (client->destroyed);
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) != 0, L"DCD off at once");
            Assert::IsNotNull (rig.network.FindBound (HostSocketKind::Tcp, kPort).get(), L"the port is listening");
        }


        TEST_METHOD (WithNoNetworkTheAttachmentIsReported)
        {
            Acia6551           acia (kBase);
            SerialLinkReports  reports;
            AciaTcpEndpoint    endpoint (acia, nullptr, reports, kTitle, MakeListening());



            acia.SetEndpoint (&endpoint);

            Assert::AreEqual (std::string ("The PC's network is not available."), reports.GetReports()[0].text);

            acia.SetEndpoint (nullptr);
        }
    };
}
