#include "Pch.h"

#include "Devices/Network/UdpNat.h"
#include "Devices/Network/VirtualSubnet.h"

#include "FakeHostNetwork.h"
#include "RecordingPacketSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNatTests
//
////////////////////////////////////////////////////////////////////////////////

namespace UdpNatTests
{
    static constexpr uint16_t  kAdtPro    = 6502;
    static constexpr uint16_t  kPeerPort  = 7000;
    static constexpr uint32_t  kLanPeer   = 0xC0A8010A;
    static constexpr uint32_t  kOtherPeer = 0xC0A80114;
    static constexpr uint32_t  kPcDns     = 0x08080808;

    static const std::vector<Byte>  s_kPayload = { 'A', 'D', 'T' };





    TEST_CLASS (UdpNatTests)
    {
    public:

        TEST_METHOD (FlowBindsTheGuestsPortAndSends)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            UdpNat               nat (sink);

            nat.SetHostNetwork (&host);
            nat.SendFromGuest (kAdtPro, kLanPeer, kAdtPro, s_kPayload, 0);

            auto  socket = host.FindBound (HostSocketKind::Udp, kAdtPro);

            Assert::IsNotNull (socket.get(), L"same port number as the guest's");
            Assert::AreEqual ((size_t) 1, socket->sentDatagrams.size());
            Assert::IsTrue (socket->sentDatagrams[0].peer == HostEndpoint { kLanPeer, kAdtPro });
            Assert::IsTrue (socket->sentDatagrams[0].bytes == s_kPayload);
        }


        TEST_METHOD (TakenPortUsesAnEphemeralOne)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            UdpNat               nat (sink);

            nat.SetHostNetwork (&host);
            host.OccupyPort (HostSocketKind::Udp, kAdtPro);
            nat.SendFromGuest (kAdtPro, kLanPeer, kAdtPro, s_kPayload, 0);

            Assert::IsNotNull (host.FindBound (HostSocketKind::Udp, FakeHostNetwork::kFirstEphemeralPort).get());
        }


        TEST_METHOD (GatewayGoesToLoopbackAndRepliesComeFromTheGateway)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            UdpNat               nat (sink);

            nat.SetHostNetwork (&host);
            nat.SendFromGuest (kAdtPro, VirtualSubnet::kGatewayIp, kAdtPro, s_kPayload, 0);

            auto  socket = host.FindBound (HostSocketKind::Udp, kAdtPro);

            Assert::IsTrue (socket->sentDatagrams[0].peer == HostEndpoint { VirtualSubnet::kLoopbackIp, kAdtPro });

            socket->inboundDatagrams.push_back ({ { VirtualSubnet::kLoopbackIp, kAdtPro }, s_kPayload });
            nat.Pump (1);

            Assert::AreEqual ((size_t) 1, sink.udp.size());
            Assert::AreEqual (VirtualSubnet::kGatewayIp, sink.udp[0].sourceIp);
            Assert::AreEqual (kAdtPro,                   sink.udp[0].sourcePort);
            Assert::AreEqual (kAdtPro,                   sink.udp[0].guestPort);
        }


        TEST_METHOD (DnsIsRelayedToThePcsServer)
        {
            constexpr uint16_t  kResolverPort = 1053;

            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            UdpNat               nat (sink);

            nat.SetHostNetwork (&host);
            host.SetDnsServer (kPcDns);
            nat.SendFromGuest (kResolverPort, VirtualSubnet::kDnsIp, VirtualSubnet::kDnsPort, s_kPayload, 0);

            auto  socket = host.FindBound (HostSocketKind::Udp, kResolverPort);

            Assert::IsTrue (socket->sentDatagrams[0].peer == HostEndpoint { kPcDns, VirtualSubnet::kDnsPort });

            socket->inboundDatagrams.push_back ({ { kPcDns, VirtualSubnet::kDnsPort }, s_kPayload });
            nat.Pump (1);

            Assert::AreEqual (VirtualSubnet::kDnsIp,   sink.udp[0].sourceIp, L"the answer comes from the virtual DNS address");
            Assert::AreEqual (VirtualSubnet::kDnsPort, sink.udp[0].sourcePort);
        }


        TEST_METHOD (NoDnsServerSendsNothing)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            UdpNat               nat (sink);

            nat.SetHostNetwork (&host);
            nat.SendFromGuest (1053, VirtualSubnet::kDnsIp, VirtualSubnet::kDnsPort, s_kPayload, 0);

            Assert::AreEqual ((size_t) 0, host.GetSockets().size());
        }


        TEST_METHOD (InboundNeedsAPeerSentToOrLanAccess)
        {
            for (bool lanAccess : { false, true })
            {
                RecordingPacketSink  sink;
                FakeHostNetwork      host;
                UdpNat               nat (sink);

                nat.SetHostNetwork (&host);
                nat.SetLanAccess (lanAccess);
                nat.SendFromGuest (kAdtPro, kLanPeer, kPeerPort, s_kPayload, 0);

                auto  socket = host.FindBound (HostSocketKind::Udp, kAdtPro);

                socket->inboundDatagrams.push_back ({ { kLanPeer,   kPeerPort }, s_kPayload });
                socket->inboundDatagrams.push_back ({ { kOtherPeer, kPeerPort }, s_kPayload });
                nat.Pump (1);

                Assert::AreEqual ((size_t) (lanAccess ? 2 : 1), sink.udp.size());
                Assert::AreEqual (kLanPeer, sink.udp[0].sourceIp);
            }
        }


        TEST_METHOD (IdleFlowsExpire)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            UdpNat               nat (sink);

            nat.SetHostNetwork (&host);
            nat.SendFromGuest (kAdtPro, kLanPeer, kAdtPro, s_kPayload, 100);

            nat.Pump (100 + UdpNat::kIdleCycles);
            Assert::AreEqual ((size_t) 1, nat.GetFlowCount(), L"not yet idle long enough");

            nat.Pump (101 + UdpNat::kIdleCycles);
            Assert::AreEqual ((size_t) 0, nat.GetFlowCount());
            Assert::IsTrue (host.GetSockets()[0]->destroyed);
        }


        TEST_METHOD (CloseFlowsKeepsForwards)
        {
            RecordingPacketSink            sink;
            FakeHostNetwork                host;
            UdpNat                         nat (sink);
            std::unique_ptr<IHostSocket>   forward;

            nat.SetHostNetwork (&host);
            AssertSucceeded (host.CreateSocket (HostSocketKind::Udp, forward));
            nat.AddForward (kPeerPort, std::move (forward));
            nat.SendFromGuest (kAdtPro, kLanPeer, kAdtPro, s_kPayload, 0);

            nat.CloseFlows();
            Assert::AreEqual ((size_t) 1, nat.GetFlowCount());

            nat.Pump (UdpNat::kIdleCycles * 2);
            Assert::AreEqual ((size_t) 1, nat.GetFlowCount(), L"forwards never expire");
        }
    };
}
