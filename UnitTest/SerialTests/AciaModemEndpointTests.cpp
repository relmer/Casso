#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/Serial/AciaModemEndpoint.h"
#include "Devices/Serial/SerialLinkReports.h"
#include "Devices/Serial/TelnetCodec.h"
#include "../NetworkTests/FakeHostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace AciaModemEndpointTestNs
{
    static constexpr Word      kBase       = 0xC0A8;
    static constexpr Word      kAddrData   = 0xC0A8;
    static constexpr Word      kAddrCmd    = 0xC0AA;
    static constexpr Word      kAddrCtrl   = 0xC0AB;
    static constexpr Byte      kCmdPolled  = Acia6551::kCommandDtr | Acia6551::kCommandRxIrqDisable | Acia6551::kCommandTicTxIrqOff;
    static constexpr Byte      kCtrl9600   = 0x1E;
    static constexpr uint32_t  kBbs        = 0x0A000007;
    static constexpr uint16_t  kListenPort = 6400;
    static constexpr uint64_t  kSecond     = Acia6551::kDefaultCpuClockHz;
    static constexpr char      kTitle[]    = "Super Serial Card, slot 2";




    ////////////////////////////////////////////////////////////////////////////
    //
    //  ModemRig
    //
    //  A 6551 with the modem attachment over the fake network, driven as a
    //  terminal program drives it: text typed through the transmitter, and
    //  what comes back read as it lands.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ModemRig
    {
    public:
        explicit ModemRig (AciaModemEndpoint::Settings settings = {})
            : endpoint (acia, &network, reports, kTitle, settings)
        {
            network.SetHostAddress ("bbs.example.org", kBbs);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            acia.SetEndpoint (&endpoint);
        }

        ~ModemRig()
        {
            acia.SetEndpoint (nullptr);
        }

        void  Run (uint64_t cycles)
        {
            uint64_t  i = 0;



            for (i = 0; i < cycles; i += kStep)
            {
                acia.Tick (kStep);
                Collect();
            }
        }

        void  Type (const std::string & text)
        {
            for (char each : text)
            {
                while ((acia.GetStatus() & Acia6551::kStatusTxEmpty) == 0)
                {
                    Run (kStep);
                }

                acia.Write (kAddrData, (Byte) each);
            }

            Run (kCharacter * 4);
        }

        std::string  TakeOutput()
        {
            return std::exchange (output, {});
        }

        bool  IsDcdOn() const
        {
            return (acia.GetStatus() & Acia6551::kStatusDcd) == 0;
        }

        std::shared_ptr<FakeSocketState>  DialBbs()
        {
            std::shared_ptr<FakeSocketState>  server;



            Type ("ATDT bbs.example.org:23\r");
            Run (Acia6551::kIdlePollCycles * 2);

            server = network.FindConnected ({ kBbs, 23 });
            Assert::IsNotNull (server.get(), L"the modem connects to the BBS");

            server->connect = FakeSocketState::ConnectOutcome::Accepted;
            Run (Acia6551::kIdlePollCycles * 2 + kCharacter * 16);

            return server;
        }

        static constexpr uint32_t  kStep      = 8;
        static constexpr uint64_t  kCharacter = 1100;

        Acia6551           acia { kBase };
        FakeHostNetwork    network;
        SerialLinkReports  reports;
        AciaModemEndpoint  endpoint;
        std::string        output;

    private:
        void  Collect()
        {
            if (acia.GetStatus() & Acia6551::kStatusRxFull)
            {
                output += (char) acia.Read (kAddrData);
            }
        }
    };




    static AciaModemEndpoint::Settings MakeListening()
    {
        AciaModemEndpoint::Settings  settings;



        settings.listenPort = kListenPort;

        return settings;
    }




    ////////////////////////////////////////////////////////////////////////////
    //
    //  AciaModemEndpointTests
    //
    //  The modem attachment end to end: the spec's BBS call, an incoming
    //  call, and the line dropped by a resume.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AciaModemEndpointTests)
    {
    public:
        TEST_METHOD (AtFromTheGuestReturnsOk)
        {
            ModemRig  rig;



            rig.Type ("AT\r");
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 8);

            Assert::AreEqual (std::string ("AT\r\r\nOK\r\n"), rig.TakeOutput());
            Assert::IsFalse (rig.IsDcdOn());
            Assert::IsTrue  (rig.acia.IsCtsTrue());
            Assert::IsTrue  ((rig.acia.GetStatus() & Acia6551::kStatusDsr) == 0, L"DSR on: the modem is there");
        }


        TEST_METHOD (DialingABbsConnectsAndTurnsDcdOn)
        {
            ModemRig  rig;



            rig.DialBbs();

            Assert::IsTrue (rig.TakeOutput().ends_with ("\r\nCONNECT 9600\r\n"));
            Assert::IsTrue (rig.IsDcdOn());
        }


        TEST_METHOD (TextWithPlusesGoesToTheBbs)
        {
            ModemRig                          rig;
            std::shared_ptr<FakeSocketState>  server = rig.DialBbs();



            server->sent.clear();
            rig.Type ("hi+++there");
            rig.Run (kSecond * 2);

            Assert::AreEqual (std::string ("hi+++there"), std::string (server->sent.begin(), server->sent.end()));
            Assert::IsTrue (rig.IsDcdOn(), L"in data mode throughout");
        }


        TEST_METHOD (TheBbsScreensArriveInDataMode)
        {
            ModemRig                          rig;
            std::shared_ptr<FakeSocketState>  server = rig.DialBbs();



            rig.TakeOutput();
            server->inbound = { 'W', 'e', 'l', 'c', 'o', 'm', 'e' };
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 10);

            Assert::AreEqual (std::string ("Welcome"), rig.TakeOutput());
        }


        TEST_METHOD (EscapingThenHangingUp)
        {
            ModemRig                          rig;
            std::shared_ptr<FakeSocketState>  server = rig.DialBbs();



            rig.Run (kSecond + kSecond / 10);
            rig.TakeOutput();
            rig.Type ("+++");
            rig.Run (kSecond + kSecond / 5);

            Assert::AreEqual (std::string ("\r\nOK\r\n"), rig.TakeOutput());
            Assert::IsTrue (rig.IsDcdOn(), L"command mode with the connection open");

            rig.Type ("ATH\r");
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 8);

            Assert::IsTrue (rig.TakeOutput().ends_with ("\r\nOK\r\n"));
            Assert::IsTrue (server->destroyed, L"the connection is closed");
            Assert::IsFalse (rig.IsDcdOn());
        }


        TEST_METHOD (TheBbsHangingUpReturnsNoCarrier)
        {
            ModemRig                          rig;
            std::shared_ptr<FakeSocketState>  server = rig.DialBbs();



            rig.TakeOutput();
            server->inboundEof = true;
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 16);

            Assert::AreEqual (std::string ("\r\nNO CARRIER\r\n"), rig.TakeOutput());
            Assert::IsFalse (rig.IsDcdOn());
        }


        TEST_METHOD (AnOfflineOrUnknownHostReturnsNoCarrier)
        {
            ModemRig  refused;
            ModemRig  unknown;



            refused.Type ("ATDT bbs.example.org\r");
            refused.Run (Acia6551::kIdlePollCycles * 2);
            refused.network.FindConnected ({ kBbs, 23 })->connect = FakeSocketState::ConnectOutcome::Refused;
            refused.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 16);

            unknown.Type ("ATDT no.such.host\r");
            unknown.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 16);

            Assert::IsTrue (refused.TakeOutput().ends_with ("\r\nNO CARRIER\r\n"));
            Assert::IsTrue (unknown.TakeOutput().ends_with ("\r\nNO CARRIER\r\n"));
        }


        TEST_METHOD (AnIncomingCallRingsAndIsAnswered)
        {
            ModemRig                          rig    (MakeListening());
            std::shared_ptr<FakeSocketState>  caller;



            caller = rig.network.OfferConnection (rig.network.FindBound (HostSocketKind::Tcp, kListenPort), { 0x7F000001, 50000 });
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 10);
            Assert::AreEqual (std::string ("\r\nRING\r\n"), rig.TakeOutput());

            rig.Type ("ATA\r");
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 16);
            Assert::IsTrue (rig.TakeOutput().ends_with ("\r\nCONNECT 9600\r\n"));
            Assert::IsTrue (rig.IsDcdOn());

            caller->inbound = { 'h', 'i' };
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 4);
            Assert::AreEqual (std::string ("hi"), rig.TakeOutput());
        }


        TEST_METHOD (S0AnswersAnIncomingCall)
        {
            ModemRig  rig (MakeListening());



            rig.Type ("ATS0=1\r");
            rig.network.OfferConnection (rig.network.FindBound (HostSocketKind::Tcp, kListenPort), { 0x7F000001, 50000 });
            rig.Run (Acia6551::kIdlePollCycles * 2 + ModemRig::kCharacter * 20);

            Assert::IsTrue (rig.TakeOutput().ends_with ("\r\nRING\r\n\r\nCONNECT 9600\r\n"));
            Assert::IsTrue (rig.IsDcdOn());
        }


        TEST_METHOD (AnIncomingCallWhileDialingIsClosed)
        {
            ModemRig                          rig (MakeListening());
            std::shared_ptr<FakeSocketState>  caller;



            rig.Type ("ATDT bbs.example.org\r");
            rig.Run (Acia6551::kIdlePollCycles * 2);

            caller = rig.network.OfferConnection (rig.network.FindBound (HostSocketKind::Tcp, kListenPort), { 0x7F000001, 50000 });
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue (caller->destroyed);
        }


        TEST_METHOD (DroppingTheConnectionReturnsNoCarrier)
        {
            ModemRig                          rig;
            std::shared_ptr<FakeSocketState>  server = rig.DialBbs();



            rig.TakeOutput();
            rig.endpoint.DropConnection();
            rig.Run (ModemRig::kCharacter * 16);

            Assert::IsTrue  (server->destroyed);
            Assert::IsFalse (rig.IsDcdOn(), L"DCD off at once");
            Assert::AreEqual (std::string ("\r\nNO CARRIER\r\n"), rig.TakeOutput());
        }


        TEST_METHOD (DtrTurningOffHangsUp)
        {
            ModemRig                          rig;
            std::shared_ptr<FakeSocketState>  server = rig.DialBbs();



            rig.acia.Write (kAddrCmd, kCmdPolled & ~Acia6551::kCommandDtr);
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue (server->destroyed);
        }


        TEST_METHOD (TelnetIsOnByDefault)
        {
            ModemRig                          rig;
            std::shared_ptr<FakeSocketState>  server = rig.DialBbs();



            server->sent.clear();
            rig.Type ("\xFF");
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue (std::vector<Byte> { 0xFF, 0xFF } == server->sent);
        }


        TEST_METHOD (AListenPortInUseIsReported)
        {
            Acia6551           acia (kBase);
            FakeHostNetwork    network;
            SerialLinkReports  reports;
            AciaModemEndpoint  endpoint (acia, &network, reports, kTitle, MakeListening());



            network.OccupyPort (HostSocketKind::Tcp, kListenPort);
            acia.SetEndpoint (&endpoint);

            Assert::AreEqual (std::string ("Port 6400 is in use by another program."), reports.GetReports()[0].text);

            acia.SetEndpoint (nullptr);
        }
    };
}
