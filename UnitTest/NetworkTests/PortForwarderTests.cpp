#include "Pch.h"

#include "Devices/Network/PortForwarder.h"
#include "Devices/Network/TcpNat.h"
#include "Devices/Network/UdpNat.h"
#include "Devices/Network/VirtualSubnet.h"

#include "FakeHostNetwork.h"
#include "RecordingPacketSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarderTests
//
////////////////////////////////////////////////////////////////////////////////

namespace PortForwarderTests
{
    static constexpr uint16_t  kHostPort   = 8080;
    static constexpr uint16_t  kGuestPort  = 80;
    static constexpr uint16_t  kRemotePort = 51000;
    static constexpr uint32_t  kLanPeer    = 0xC0A8010A;

    static const std::vector<PortForward>  s_kTcpForward = { { PortForwardProtocol::Tcp, kHostPort, kGuestPort } };
    static const std::vector<PortForward>  s_kUdpForward = { { PortForwardProtocol::Udp, kHostPort, kGuestPort } };





    TEST_CLASS (PortForwarderTests)
    {
    public:

        TEST_METHOD (TcpForwardListensOnLoopbackWithoutLanAccess)
        {
            for (bool lanAccess : { false, true })
            {
                RecordingPacketSink  sink;
                FakeHostNetwork      host;
                TcpNat               tcp (sink);
                UdpNat               udp (sink);
                PortForwarder        forwarder (tcp, udp);

                forwarder.Start (&host, s_kTcpForward, lanAccess);

                auto  listener = host.FindBound (HostSocketKind::Tcp, kHostPort);

                Assert::IsNotNull (listener.get());
                Assert::IsTrue (listener->listening);
                Assert::AreEqual (lanAccess ? VirtualSubnet::kAnyIp : VirtualSubnet::kLoopbackIp, listener->local.ipv4);
                Assert::IsTrue (forwarder.GetErrors().empty());
            }
        }


        TEST_METHOD (APortInUseIsReportedAndTheOthersOpen)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            TcpNat               tcp (sink);
            UdpNat               udp (sink);
            PortForwarder        forwarder (tcp, udp);
            std::vector<PortForward>  forwards = { { PortForwardProtocol::Tcp, kHostPort, kGuestPort },
                                                   { PortForwardProtocol::Tcp, kHostPort + 1, 23 } };

            host.OccupyPort (HostSocketKind::Tcp, kHostPort);
            forwarder.Start (&host, forwards, false);

            Assert::AreEqual ((size_t) 1, forwarder.GetErrors().size());
            Assert::IsTrue (forwarder.GetErrors()[0].find ("8080") != std::string::npos);
            Assert::IsNotNull (host.FindBound (HostSocketKind::Tcp, kHostPort + 1).get());
        }


        TEST_METHOD (AcceptedConnectionSendsSynToTheGuestPort)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            TcpNat               tcp (sink);
            UdpNat               udp (sink);
            PortForwarder        forwarder (tcp, udp);

            forwarder.Start (&host, s_kTcpForward, false);
            host.OfferConnection (host.FindBound (HostSocketKind::Tcp, kHostPort), { VirtualSubnet::kLoopbackIp, kRemotePort });
            forwarder.Pump (0, true);

            Assert::AreEqual ((size_t) 1, sink.tcp.size());
            Assert::AreEqual (kGuestPort,                sink.tcp[0].destinationPort);
            Assert::AreEqual (VirtualSubnet::kGatewayIp, sink.tcp[0].sourceIp);
            Assert::AreEqual ((size_t) 1, tcp.GetConnectionCount());
        }


        TEST_METHOD (ConnectionWithNoGuestReadyIsClosed)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            TcpNat               tcp (sink);
            UdpNat               udp (sink);
            PortForwarder        forwarder (tcp, udp);

            forwarder.Start (&host, s_kTcpForward, false);
            auto  remote = host.OfferConnection (host.FindBound (HostSocketKind::Tcp, kHostPort), { VirtualSubnet::kLoopbackIp, kRemotePort });
            forwarder.Pump (0, false);

            Assert::IsTrue (remote->destroyed);
            Assert::AreEqual ((size_t) 0, sink.tcp.size());
        }


        TEST_METHOD (UdpForwardDeliversToTheGuestPortWithinTheLanRule)
        {
            const std::vector<Byte>  payload = { 7, 7, 7 };

            for (bool lanAccess : { false, true })
            {
                RecordingPacketSink  sink;
                FakeHostNetwork      host;
                TcpNat               tcp (sink);
                UdpNat               udp (sink);
                PortForwarder        forwarder (tcp, udp);

                udp.SetLanAccess (lanAccess);
                forwarder.Start (&host, s_kUdpForward, lanAccess);

                auto  socket = host.FindBound (HostSocketKind::Udp, kHostPort);

                socket->inboundDatagrams.push_back ({ { VirtualSubnet::kLoopbackIp, kRemotePort }, payload });
                socket->inboundDatagrams.push_back ({ { kLanPeer, kRemotePort }, payload });
                udp.Pump (0);

                Assert::AreEqual ((size_t) (lanAccess ? 2 : 1), sink.udp.size());
                Assert::AreEqual (kGuestPort, sink.udp[0].guestPort);
            }
        }
    };
}
