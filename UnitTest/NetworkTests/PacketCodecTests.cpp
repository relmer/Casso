#include "Pch.h"

#include "Devices/Network/PacketCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using P = PacketCodec;





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodecTests
//
//  Header building and parsing for the virtual network, against RFC 1071's
//  checksum example and build-then-parse round trips.
//
////////////////////////////////////////////////////////////////////////////////

namespace PacketCodecTests
{
    static constexpr uint32_t  kGuest = 0x0A00410F;
    static constexpr uint32_t  kPeer  = 0xC0A8010A;

    static constexpr MacAddress::Octets  kGuestMac = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F };
    static constexpr MacAddress::Octets  kPeerMac  = { 0x02, 0xCA, 0x55, 0x0A, 0x00, 0x01 };





    TEST_CLASS (PacketCodecTests)
    {
    public:

        TEST_METHOD (ChecksumMatchesRfc1071Example)
        {
            // RFC 1071 section 3: the sum of these words is $DDF2.
            const std::vector<Byte>  bytes = { 0x00, 0x01, 0xF2, 0x03, 0xF4, 0xF5, 0xF6, 0xF7 };

            Assert::AreEqual ((uint16_t) 0x220D, P::Checksum (bytes));
        }


        TEST_METHOD (ChecksumPadsAnOddByte)
        {
            const std::vector<Byte>  odd  = { 0x12, 0x34, 0x56 };
            const std::vector<Byte>  even = { 0x12, 0x34, 0x56, 0x00 };

            Assert::AreEqual (P::Checksum (even), P::Checksum (odd));
        }


        TEST_METHOD (EthernetPadsToTheMinimumAndParses)
        {
            const std::vector<Byte>  payload = { 1, 2, 3 };
            std::vector<Byte>        frame   = P::BuildEthernet (kPeerMac, kGuestMac, P::kEtherTypeArp, payload);
            EthernetFrame            parsed;

            Assert::AreEqual (P::kMinimumFrame, frame.size());
            Assert::IsTrue (P::TryParseEthernet (frame, parsed));
            Assert::IsTrue (parsed.destination == kPeerMac);
            Assert::IsTrue (parsed.source == kGuestMac);
            Assert::AreEqual (P::kEtherTypeArp, parsed.etherType);
        }


        TEST_METHOD (ArpRoundTrips)
        {
            ArpPacket  request;
            ArpPacket  parsed;

            request.operation = P::kArpRequest;
            request.senderMac = kGuestMac;
            request.senderIp  = kGuest;
            request.targetIp  = kPeer;

            Assert::IsTrue (P::TryParseArp (P::BuildArp (request), parsed));
            Assert::AreEqual (P::kArpRequest, parsed.operation);
            Assert::IsTrue (parsed.senderMac == kGuestMac);
            Assert::AreEqual (kGuest, parsed.senderIp);
            Assert::AreEqual (kPeer,  parsed.targetIp);
        }


        TEST_METHOD (Ipv4RoundTripsWithAValidHeaderChecksum)
        {
            const std::vector<Byte>  payload = { 9, 8, 7, 6 };
            std::vector<Byte>        packet  = P::BuildIpv4 (kGuest, kPeer, P::kProtocolUdp, 0x1234, payload);
            Ipv4Packet               parsed;

            Assert::AreEqual ((uint16_t) 0, P::Checksum (std::span<const Byte> (packet).first (P::kIpv4Header)));
            Assert::IsTrue (P::TryParseIpv4 (packet, parsed));
            Assert::AreEqual (kGuest, parsed.source);
            Assert::AreEqual (kPeer,  parsed.destination);
            Assert::AreEqual (P::kProtocolUdp, parsed.protocol);
            Assert::IsTrue (std::equal (payload.begin(), payload.end(), parsed.payload.begin(), parsed.payload.end()));
        }


        TEST_METHOD (Ipv4RejectsDamageTruncationAndFragments)
        {
            constexpr size_t         kFragmentOffset = 6;
            const std::vector<Byte>  payload         = { 1, 2, 3, 4 };
            std::vector<Byte>        packet          = P::BuildIpv4 (kGuest, kPeer, P::kProtocolTcp, 1, payload);
            std::vector<Byte>        damaged         = packet;
            std::vector<Byte>        fragment        = packet;
            Ipv4Packet               parsed;

            damaged[P::kIpv4Header - 1] ^= 0xFF;
            Assert::IsFalse (P::TryParseIpv4 (damaged, parsed), L"bad header checksum");

            Assert::IsFalse (P::TryParseIpv4 (std::span<const Byte> (packet).first (P::kIpv4Header + 1), parsed), L"truncated");

            P::WriteU16 (fragment, kFragmentOffset, 0x2000);   // MF
            P::WriteU16 (fragment, 10, 0);
            P::WriteU16 (fragment, 10, P::Checksum (std::span<const Byte> (fragment).first (P::kIpv4Header)));
            Assert::IsFalse (P::TryParseIpv4 (fragment, parsed), L"fragment");
        }


        TEST_METHOD (UdpRoundTripsAndRejectsABadChecksum)
        {
            const std::vector<Byte>  payload = { 'h', 'e', 'l', 'l', 'o' };
            std::vector<Byte>        udp     = P::BuildUdp (kGuest, kPeer, 6502, 6502, payload);
            std::vector<Byte>        packet  = P::BuildIpv4 (kGuest, kPeer, P::kProtocolUdp, 7, udp);
            Ipv4Packet               ip;
            UdpDatagram              parsed;

            Assert::IsTrue (P::TryParseIpv4 (packet, ip));
            Assert::IsTrue (P::TryParseUdp (ip, parsed));
            Assert::AreEqual ((uint16_t) 6502, parsed.sourcePort);
            Assert::IsTrue (std::equal (payload.begin(), payload.end(), parsed.payload.begin(), parsed.payload.end()));

            udp.back() ^= 0x01;
            packet = P::BuildIpv4 (kGuest, kPeer, P::kProtocolUdp, 7, udp);
            Assert::IsTrue (P::TryParseIpv4 (packet, ip));
            Assert::IsFalse (P::TryParseUdp (ip, parsed));
        }


        TEST_METHOD (TcpRoundTripsWithMss)
        {
            TcpSegment         segment;
            TcpSegment         parsed;
            Ipv4Packet         ip;
            std::vector<Byte>  packet;

            segment.sourceIp        = kGuest;
            segment.destinationIp   = kPeer;
            segment.sourcePort      = 1025;
            segment.destinationPort = 80;
            segment.sequence        = 0x01020304;
            segment.acknowledgment  = 0x0A0B0C0D;
            segment.flags           = P::kTcpSyn | P::kTcpAck;
            segment.window          = 2048;
            segment.mss             = 1460;
            segment.payload         = { 1, 2, 3 };

            packet = P::BuildIpv4 (kGuest, kPeer, P::kProtocolTcp, 9, P::BuildTcp (segment));

            Assert::IsTrue (P::TryParseIpv4 (packet, ip));
            Assert::IsTrue (P::TryParseTcp (ip, parsed));
            Assert::AreEqual (segment.sequence,       parsed.sequence);
            Assert::AreEqual (segment.acknowledgment, parsed.acknowledgment);
            Assert::AreEqual (segment.flags,          parsed.flags);
            Assert::AreEqual (segment.window,         parsed.window);
            Assert::AreEqual (segment.mss,            parsed.mss);
            Assert::IsTrue (segment.payload == parsed.payload);

            packet.back() ^= 0x01;
            Assert::IsTrue (P::TryParseIpv4 (packet, ip));
            Assert::IsFalse (P::TryParseTcp (ip, parsed), L"bad TCP checksum");
        }


        TEST_METHOD (IcmpEchoReplyEchoesTheRequest)
        {
            std::vector<Byte>  request = { P::kIcmpEchoRequest, 0, 0, 0, 0x12, 0x34, 0x00, 0x01, 'p', 'i', 'n', 'g' };
            std::vector<Byte>  reply;

            P::WriteU16 (request, 2, P::Checksum (request));
            reply = P::BuildIcmpEchoReply (request);

            Assert::AreEqual (P::kIcmpEchoReply, reply[0]);
            Assert::AreEqual ((uint16_t) 0, P::Checksum (reply));
            Assert::IsTrue (std::equal (request.begin() + 4, request.end(), reply.begin() + 4));
        }
    };
}
