#include "Pch.h"

#include "Devices/Network/VirtualNetwork.h"
#include "Devices/Network/VirtualSubnet.h"

#include "DhcpTestPackets.h"
#include "FakeHostNetwork.h"
#include "UthernetRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using P = PacketCodec;
using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetworkTests
//
//  Whole Ethernet frames in, whole frames out.
//
////////////////////////////////////////////////////////////////////////////////

namespace VirtualNetworkTests
{
    static constexpr MacAddress::Octets  kGuestMac = { 0x00, 0x08, 0xDC, 0x11, 0x11, 0x11 };
    static constexpr uint32_t            kLanPeer  = 0xC0A8010A;


    static std::vector<Byte> MakeArpRequest (uint32_t senderIp, uint32_t targetIp)
    {
        ArpPacket  request;

        request.operation = P::kArpRequest;
        request.senderMac = kGuestMac;
        request.senderIp  = senderIp;
        request.targetIp  = targetIp;

        return P::BuildEthernet (P::kBroadcastMac, kGuestMac, P::kEtherTypeArp, P::BuildArp (request));
    }


    static std::vector<Byte> MakeIpFrame (uint32_t destination, Byte protocol, const std::vector<Byte> & payload)
    {
        return P::BuildEthernet (VirtualSubnet::kGatewayMac,
                                 kGuestMac,
                                 P::kEtherTypeIpv4,
                                 P::BuildIpv4 (VirtualSubnet::kGuestIp, destination, protocol, 1, payload));
    }


    static bool TryTakeIpv4 (VirtualNetwork & network, Ipv4Packet & out, std::vector<Byte> & storage)
    {
        EthernetFrame  frame;

        if (!network.HasFrame())
        {
            return false;
        }

        storage = network.PeekFrame();
        network.PopFrame();

        return P::TryParseEthernet (storage, frame)
            && frame.etherType == P::kEtherTypeIpv4
            && P::TryParseIpv4 (frame.payload, out);
    }





    TEST_CLASS (VirtualNetworkTests)
    {
    public:

        TEST_METHOD (ArpResolvesEveryAddressButTheGuestsToTheGateway)
        {
            VirtualNetwork  network;

            for (uint32_t target : { VirtualSubnet::kGatewayIp, kLanPeer, VirtualSubnet::kDnsIp })
            {
                EthernetFrame  frame;
                ArpPacket      reply;

                network.OnGuestFrame (MakeArpRequest (VirtualSubnet::kGuestIp, target), 0);

                Assert::IsTrue (network.HasFrame());
                Assert::IsTrue (P::TryParseEthernet (network.PeekFrame(), frame));
                Assert::IsTrue (frame.destination == kGuestMac);
                Assert::IsTrue (P::TryParseArp (frame.payload, reply));
                Assert::AreEqual (P::kArpReply, reply.operation);
                Assert::AreEqual (target,       reply.senderIp);
                Assert::IsTrue (reply.senderMac == VirtualSubnet::kGatewayMac);

                network.PopFrame();
            }
        }


        TEST_METHOD (ArpProbesAndTheGuestsOwnAddressGetNoReply)
        {
            VirtualNetwork  network;

            network.OnGuestFrame (MakeArpRequest (0, VirtualSubnet::kGuestIp), 0);
            network.OnGuestFrame (MakeArpRequest (VirtualSubnet::kGuestIp, VirtualSubnet::kGuestIp), 0);
            network.OnGuestFrame (MakeArpRequest (VirtualSubnet::kGatewayIp, VirtualSubnet::kGuestIp), 0);

            Assert::IsFalse (network.HasFrame());
        }


        TEST_METHOD (PingToTheGatewayIsAnswered)
        {
            VirtualNetwork     network;
            std::vector<Byte>  echo = { P::kIcmpEchoRequest, 0, 0, 0, 0x00, 0x01, 0x00, 0x07, 'h', 'i' };
            std::vector<Byte>  storage;
            Ipv4Packet         reply;

            P::WriteU16 (echo, 2, P::Checksum (echo));

            network.OnGuestFrame (MakeIpFrame (kLanPeer, P::kProtocolIcmp, echo), 0);
            Assert::IsFalse (network.HasFrame(), L"only the gateway answers");

            network.OnGuestFrame (MakeIpFrame (VirtualSubnet::kGatewayIp, P::kProtocolIcmp, echo), 0);
            Assert::IsTrue (TryTakeIpv4 (network, reply, storage));
            Assert::AreEqual (VirtualSubnet::kGatewayIp, reply.source);
            Assert::AreEqual (P::kIcmpEchoReply,         reply.payload[0]);
        }


        TEST_METHOD (DhcpOverMacRawGetsABroadcastOffer)
        {
            constexpr uint32_t  kXid = 0x00C0FFEE;

            VirtualNetwork     network;
            std::vector<Byte>  request = DhcpTestPackets::MakeRequest (DhcpTestPackets::kDiscover, kGuestMac, kXid);
            std::vector<Byte>  udp     = P::BuildUdp (0, VirtualSubnet::kBroadcastIp, 68, 67, request);
            std::vector<Byte>  frame   = P::BuildEthernet (P::kBroadcastMac, kGuestMac, P::kEtherTypeIpv4, P::BuildIpv4 (0, VirtualSubnet::kBroadcastIp, P::kProtocolUdp, 1, udp));
            std::vector<Byte>  storage;
            Ipv4Packet         ip;
            UdpDatagram        reply;

            network.OnGuestFrame (frame, 0);

            Assert::IsTrue (TryTakeIpv4 (network, ip, storage));
            Assert::AreEqual (VirtualSubnet::kBroadcastIp, ip.destination);
            Assert::IsTrue (P::TryParseUdp (ip, reply));
            Assert::AreEqual ((uint16_t) 68, reply.destinationPort);
            Assert::AreEqual (DhcpTestPackets::kOffer, DhcpTestPackets::GetMessageType (reply.payload));
            Assert::AreEqual (kXid,                    DhcpTestPackets::GetXid (reply.payload));
        }


        TEST_METHOD (UdpAndTcpFramesReachTheHost)
        {
            FakeHostNetwork          host;
            VirtualNetwork           network;
            const std::vector<Byte>  payload = { 1, 2, 3 };
            TcpSegment               syn;

            network.SetHostNetwork (&host);

            network.OnGuestFrame (MakeIpFrame (kLanPeer, P::kProtocolUdp, P::BuildUdp (VirtualSubnet::kGuestIp, kLanPeer, 6502, 6502, payload)), 0);
            Assert::IsNotNull (host.FindBound (HostSocketKind::Udp, 6502).get());

            syn.sourceIp        = VirtualSubnet::kGuestIp;
            syn.destinationIp   = kLanPeer;
            syn.sourcePort      = 1025;
            syn.destinationPort = 80;
            syn.sequence        = 1;
            syn.flags           = P::kTcpSyn;
            syn.window          = 1024;
            network.OnGuestFrame (MakeIpFrame (kLanPeer, P::kProtocolTcp, P::BuildTcp (syn)), 0);

            Assert::IsNotNull (host.FindConnected ({ kLanPeer, 80 }).get());
        }


        TEST_METHOD (DamagedFramesAreDropped)
        {
            FakeHostNetwork          host;
            VirtualNetwork           network;
            const std::vector<Byte>  payload = { 1, 2, 3 };
            std::vector<Byte>        frame   = MakeIpFrame (kLanPeer, P::kProtocolUdp, P::BuildUdp (VirtualSubnet::kGuestIp, kLanPeer, 6502, 6502, payload));

            network.SetHostNetwork (&host);
            frame[P::kEthernetHeader + 3] ^= 0x40;    // IPv4 total length

            network.OnGuestFrame (frame, 0);

            Assert::AreEqual ((size_t) 0, host.GetSockets().size());
        }


        TEST_METHOD (QueueIsBounded)
        {
            VirtualNetwork  network;
            size_t          count   = 0;
            size_t          i       = 0;

            for (i = 0; i < VirtualNetwork::kMaxQueuedFrames + 50; i++)
            {
                network.OnGuestFrame (MakeArpRequest (VirtualSubnet::kGuestIp, VirtualSubnet::kGatewayIp), 0);
            }

            while (network.HasFrame())
            {
                network.PopFrame();
                count++;
            }

            Assert::AreEqual (VirtualNetwork::kMaxQueuedFrames, count);
        }


        TEST_METHOD (MacRawSocketExchangesFramesWithLengthHeaders)
        {
            UthernetRig        rig;
            std::vector<Byte>  request = MakeArpRequest (VirtualSubnet::kGuestIp, VirtualSubnet::kGatewayIp);
            std::vector<Byte>  received;

            rig.bus.PokeBytes (R::kShar, kGuestMac);
            rig.OpenSocket (0, R::kProtocolMacRaw | R::kSnMrMacFilter, 0);
            Assert::AreEqual (R::kSockMacRaw, rig.bus.Status (0));

            rig.bus.Send (0, request);
            received = rig.bus.ReceiveAll (0);

            Assert::AreEqual ((size_t) (2 + P::kMinimumFrame), received.size());
            Assert::AreEqual ((uint16_t) (2 + P::kMinimumFrame), P::ReadU16 (received, 0), L"the length counts its own 2 bytes");
            Assert::IsTrue (std::equal (kGuestMac.begin(), kGuestMac.end(), received.begin() + 2), L"addressed to the guest");
        }


        TEST_METHOD (MacFilterDropsFramesForOtherMacs)
        {
            constexpr MacAddress::Octets  kOtherMac = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };

            UthernetRig        rig;

            rig.bus.PokeBytes (R::kShar, kOtherMac);
            rig.OpenSocket (0, R::kProtocolMacRaw | R::kSnMrMacFilter, 0);

            rig.bus.Send (0, MakeArpRequest (VirtualSubnet::kGuestIp, VirtualSubnet::kGatewayIp));

            Assert::AreEqual ((Word) 0, rig.bus.PeekWord (W5100Driver::SocketReg (0, R::kSnRxRsr)), L"the reply is for kGuestMac, not SHAR");
        }
    };
}
