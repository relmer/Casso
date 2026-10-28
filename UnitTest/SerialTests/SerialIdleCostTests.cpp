#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/Serial/AciaModemEndpoint.h"
#include "Devices/Serial/AciaTcpEndpoint.h"
#include "Devices/Serial/SerialLinkReports.h"
#include "../NetworkTests/FakeHostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SerialIdleCostTestNs
{
    static constexpr Word      kBase        = 0xC0A8;
    static constexpr uint16_t  kListenPort  = 1977;
    static constexpr uint32_t  kStep        = 4;
    static constexpr uint64_t  kOneSecond   = Acia6551::kDefaultCpuClockHz;
    static constexpr char      kTitle[]     = "Serial port 2";

    //  An idle link is polled once per idle interval: about 125 times a
    //  second, with a margin for rounding.
    static constexpr size_t    kMaxIdlePolls = kOneSecond / Acia6551::kIdlePollCycles + 5;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialIdleCostTests
    //
    //  SC-006: an idle TCP or modem attachment costs one non-blocking call or
    //  two per idle interval, not one per instruction.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialIdleCostTests)
    {
    public:
        static AciaTcpEndpoint::Settings MakeTcpListen()
        {
            AciaTcpEndpoint::Settings  settings;



            settings.port     = kListenPort;
            settings.isListen = true;

            return settings;
        }


        static AciaModemEndpoint::Settings MakeModemListen()
        {
            AciaModemEndpoint::Settings  settings;



            settings.listenPort = kListenPort;

            return settings;
        }


        static void RunOneSecond (Acia6551 & acia)
        {
            uint64_t  ran = 0;



            for (ran = 0; ran < kOneSecond; ran += kStep)
            {
                acia.Tick (kStep);
            }
        }


        TEST_METHOD (AnIdleListeningTcpPortMakesAboutOneCallPerInterval)
        {
            Acia6551           acia     (kBase);
            FakeHostNetwork    network;
            SerialLinkReports  reports;
            AciaTcpEndpoint    endpoint (acia, &network, reports, kTitle, MakeTcpListen());
            size_t             calls    = 0;



            acia.SetEndpoint (&endpoint);
            calls = network.GetCallCount();
            RunOneSecond (acia);
            calls = network.GetCallCount() - calls;

            Assert::IsTrue (calls > 0, L"the port is polled");
            Assert::IsTrue (calls <= kMaxIdlePolls, std::format (L"{} calls in a second", calls).c_str());

            acia.SetEndpoint (nullptr);
        }


        TEST_METHOD (AnIdleTcpConnectionMakesAtMostTwoCallsPerInterval)
        {
            Acia6551           acia     (kBase);
            FakeHostNetwork    network;
            SerialLinkReports  reports;
            AciaTcpEndpoint    endpoint (acia, &network, reports, kTitle, MakeTcpListen());
            size_t             calls    = 0;



            acia.SetEndpoint (&endpoint);
            network.OfferConnection (network.FindBound (HostSocketKind::Tcp, kListenPort), { 0x7F000001, 50000 });
            RunOneSecond (acia);

            calls = network.GetCallCount();
            RunOneSecond (acia);
            calls = network.GetCallCount() - calls;

            Assert::IsTrue (calls <= kMaxIdlePolls * 2, std::format (L"{} calls in a second", calls).c_str());

            acia.SetEndpoint (nullptr);
        }


        TEST_METHOD (AnIdleModemMakesAboutOneCallPerInterval)
        {
            Acia6551           acia     (kBase);
            FakeHostNetwork    network;
            SerialLinkReports  reports;
            AciaModemEndpoint  endpoint (acia, &network, reports, kTitle, MakeModemListen());
            size_t             calls    = 0;



            acia.SetEndpoint (&endpoint);
            calls = network.GetCallCount();
            RunOneSecond (acia);
            calls = network.GetCallCount() - calls;

            Assert::IsTrue (calls <= kMaxIdlePolls, std::format (L"{} calls in a second", calls).c_str());

            acia.SetEndpoint (nullptr);
        }
    };
}
