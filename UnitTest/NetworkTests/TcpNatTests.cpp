#include "Pch.h"

#include "Devices/Network/TcpNat.h"
#include "Devices/Network/VirtualSubnet.h"

#include "FakeHostNetwork.h"
#include "RecordingPacketSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using P = PacketCodec;





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatTests
//
//  The test builds the guest's segments itself; the fake host network is
//  the PC side.
//
////////////////////////////////////////////////////////////////////////////////

namespace TcpNatTests
{
    static constexpr uint16_t  kGuestPort   = 1025;
    static constexpr uint16_t  kPeerPort    = 80;
    static constexpr uint32_t  kLanPeer     = 0xC0A8010A;
    static constexpr uint32_t  kGuestIsn    = 1000;
    static constexpr uint16_t  kGuestMss    = 536;
    static constexpr uint16_t  kGuestWindow = 2048;


    static TcpSegment FromGuest (
        Byte                       flags,
        uint32_t                   sequence,
        uint32_t                   acknowledgment,
        const std::vector<Byte>  & payload = {},
        uint16_t                   window  = kGuestWindow,
        uint16_t                   mss     = 0)
    {
        TcpSegment  segment;

        segment.sourceIp        = VirtualSubnet::kGuestIp;
        segment.destinationIp   = kLanPeer;
        segment.sourcePort      = kGuestPort;
        segment.destinationPort = kPeerPort;
        segment.sequence        = sequence;
        segment.acknowledgment  = acknowledgment;
        segment.flags           = flags;
        segment.window          = window;
        segment.mss             = mss;
        segment.payload         = payload;

        return segment;
    }


    static std::vector<Byte> MakePattern (size_t count)
    {
        std::vector<Byte>  bytes (count);
        size_t             i     = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) (i * 13 + 5);
        }

        return bytes;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Connected
    //
    //  An established relay: guest SYN, host connect accepted, SYN-ACK to the
    //  guest, guest ACK.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct Connected
    {
        RecordingPacketSink               sink;
        FakeHostNetwork                   host;
        TcpNat                            nat { sink };
        std::shared_ptr<FakeSocketState>  peer;
        uint32_t                          ourNext = 0;
        uint64_t                          now     = 0;

        explicit Connected (uint16_t window = kGuestWindow)
        {
            nat.SetHostNetwork (&host);
            nat.OnGuestSegment (FromGuest (P::kTcpSyn, kGuestIsn, 0, {}, window, kGuestMss), now);

            peer          = host.FindConnected ({ kLanPeer, kPeerPort });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;
            nat.Pump (++now);

            ourNext = sink.tcp.back().sequence + 1;
            nat.OnGuestSegment (FromGuest (P::kTcpAck, kGuestIsn + 1, ourNext, {}, window), ++now);
            sink.tcp.clear();
        }
    };





    TEST_CLASS (TcpNatTests)
    {
    public:

        TEST_METHOD (SynConnectsAndSynAckFollowsTheHostConnect)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            TcpNat               nat (sink);

            nat.SetHostNetwork (&host);
            nat.OnGuestSegment (FromGuest (P::kTcpSyn, kGuestIsn, 0, {}, kGuestWindow, kGuestMss), 0);

            auto  peer = host.FindConnected ({ kLanPeer, kPeerPort });

            Assert::IsNotNull (peer.get());
            Assert::AreEqual ((size_t) 0, sink.tcp.size(), L"no SYN-ACK before the host connects");

            peer->connect = FakeSocketState::ConnectOutcome::Accepted;
            nat.Pump (1);

            Assert::AreEqual ((size_t) 1, sink.tcp.size());
            Assert::AreEqual ((Byte) (P::kTcpSyn | P::kTcpAck), sink.tcp[0].flags);
            Assert::AreEqual (kGuestIsn + 1,                     sink.tcp[0].acknowledgment);
            Assert::AreEqual (kLanPeer,                          sink.tcp[0].sourceIp);
            Assert::AreEqual (kGuestPort,                        sink.tcp[0].destinationPort);
            Assert::AreNotEqual ((uint16_t) 0,                   sink.tcp[0].mss);
        }


        TEST_METHOD (RefusedConnectSendsReset)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            TcpNat               nat (sink);

            nat.SetHostNetwork (&host);
            nat.OnGuestSegment (FromGuest (P::kTcpSyn, kGuestIsn, 0), 0);
            host.FindConnected ({ kLanPeer, kPeerPort })->connect = FakeSocketState::ConnectOutcome::Refused;
            nat.Pump (1);

            Assert::AreEqual ((Byte) (P::kTcpRst | P::kTcpAck), sink.tcp.back().flags);
            Assert::AreEqual ((size_t) 0, nat.GetConnectionCount());
        }


        TEST_METHOD (GatewayConnectsToLoopback)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            TcpNat               nat (sink);
            TcpSegment           syn = FromGuest (P::kTcpSyn, kGuestIsn, 0);

            syn.destinationIp = VirtualSubnet::kGatewayIp;
            nat.SetHostNetwork (&host);
            nat.OnGuestSegment (syn, 0);

            Assert::IsNotNull (host.FindConnected ({ VirtualSubnet::kLoopbackIp, kPeerPort }).get());
        }


        TEST_METHOD (SegmentForNoConnectionGetsReset)
        {
            RecordingPacketSink  sink;
            FakeHostNetwork      host;
            TcpNat               nat (sink);

            nat.SetHostNetwork (&host);
            nat.OnGuestSegment (FromGuest (P::kTcpAck, 5000, 777, { 1, 2 }), 0);

            Assert::AreEqual ((Byte) P::kTcpRst, sink.tcp.back().flags);
            Assert::AreEqual ((uint32_t) 777,    sink.tcp.back().sequence, L"RST takes its sequence from the ACK");
        }


        TEST_METHOD (InOrderGuestDataReachesTheHostAndIsAcked)
        {
            Connected          c;
            std::vector<Byte>  data = MakePattern (100);

            c.nat.OnGuestSegment (FromGuest (P::kTcpAck | P::kTcpPsh, kGuestIsn + 1, c.ourNext, data), ++c.now);
            c.nat.Pump (++c.now);

            Assert::IsTrue (c.peer->sent == data);
            Assert::AreEqual (kGuestIsn + 1 + 100, c.sink.tcp.front().acknowledgment);
        }


        TEST_METHOD (OutOfOrderDataGetsADuplicateAck)
        {
            Connected  c;

            c.nat.OnGuestSegment (FromGuest (P::kTcpAck, kGuestIsn + 51, c.ourNext, MakePattern (10)), ++c.now);
            c.nat.Pump (++c.now);

            Assert::IsTrue (c.peer->sent.empty());
            Assert::AreEqual (kGuestIsn + 1, c.sink.tcp.back().acknowledgment);
        }


        TEST_METHOD (HostDataIsSegmentedByMssWithinTheWindow)
        {
            constexpr uint16_t  kSmallWindow = 1000;

            Connected          c (kSmallWindow);
            std::vector<Byte>  data = MakePattern (2000);
            std::vector<Byte>  got;

            c.peer->inbound.assign (data.begin(), data.end());
            c.nat.Pump (++c.now);

            Assert::AreEqual ((size_t) 2, c.sink.tcp.size());
            Assert::AreEqual ((size_t) kGuestMss,                  c.sink.tcp[0].payload.size());
            Assert::AreEqual ((size_t) (kSmallWindow - kGuestMss), c.sink.tcp[1].payload.size());

            for (const TcpSegment & s : c.sink.tcp)
            {
                got.insert (got.end(), s.payload.begin(), s.payload.end());
            }

            c.sink.tcp.clear();
            c.nat.OnGuestSegment (FromGuest (P::kTcpAck, kGuestIsn + 1, c.ourNext + kSmallWindow, {}, kSmallWindow), ++c.now);
            c.nat.Pump (++c.now);

            for (const TcpSegment & s : c.sink.tcp)
            {
                got.insert (got.end(), s.payload.begin(), s.payload.end());
            }

            Assert::IsTrue (got == data, L"every byte, in order, after the window reopens");
        }


        TEST_METHOD (UnacknowledgedDataIsResent)
        {
            Connected          c;
            std::vector<Byte>  data  = MakePattern (100);
            uint32_t           first = 0;

            c.peer->inbound.assign (data.begin(), data.end());
            c.nat.Pump (++c.now);
            first = c.sink.tcp.back().sequence;
            c.sink.tcp.clear();

            c.nat.Pump (c.now + TcpNatConnection::kRetransmitCycles);

            Assert::AreEqual ((size_t) 1, c.sink.tcp.size());
            Assert::AreEqual (first,      c.sink.tcp[0].sequence);
            Assert::IsTrue (c.sink.tcp[0].payload == data);
        }


        TEST_METHOD (GuestFinShutsDownTheHostSide)
        {
            Connected  c;

            c.nat.OnGuestSegment (FromGuest (P::kTcpFin | P::kTcpAck, kGuestIsn + 1, c.ourNext), ++c.now);
            c.nat.Pump (++c.now);

            Assert::IsTrue (c.peer->shutdownSent);
            Assert::AreEqual (kGuestIsn + 2, c.sink.tcp.front().acknowledgment, L"the FIN is acknowledged");
        }


        TEST_METHOD (HostCloseSendsFinAndBothFinsCloseTheRelay)
        {
            Connected  c;
            uint32_t   finSequence = 0;

            c.peer->inboundEof = true;
            c.nat.Pump (++c.now);

            Assert::AreEqual ((Byte) (P::kTcpFin | P::kTcpAck), c.sink.tcp.back().flags);
            finSequence = c.sink.tcp.back().sequence;

            c.nat.OnGuestSegment (FromGuest (P::kTcpFin | P::kTcpAck, kGuestIsn + 1, finSequence + 1), ++c.now);
            c.nat.Pump (++c.now);

            Assert::AreEqual ((size_t) 0, c.nat.GetConnectionCount());
            Assert::IsTrue (c.peer->destroyed);
        }


        TEST_METHOD (GuestResetClosesTheHostSocket)
        {
            Connected  c;

            c.nat.OnGuestSegment (FromGuest (P::kTcpRst, kGuestIsn + 1, 0), ++c.now);
            c.nat.Pump (++c.now);

            Assert::AreEqual ((size_t) 0, c.nat.GetConnectionCount());
            Assert::IsTrue (c.peer->destroyed);
        }


        TEST_METHOD (InboundConnectionSendsSynToTheGuest)
        {
            constexpr uint16_t  kForwardedPort = 23;
            constexpr uint16_t  kRemotePort    = 51000;

            RecordingPacketSink            sink;
            FakeHostNetwork                host;
            TcpNat                         nat (sink);
            std::unique_ptr<IHostSocket>   accepted;
            TcpSegment                     synAck;

            AssertSucceeded (host.CreateSocket (HostSocketKind::Tcp, accepted));
            nat.AcceptInbound (std::move (accepted), { VirtualSubnet::kLoopbackIp, kRemotePort }, kForwardedPort, 0);

            Assert::AreEqual ((Byte) P::kTcpSyn,         sink.tcp.back().flags);
            Assert::AreEqual (kForwardedPort,            sink.tcp.back().destinationPort);
            Assert::AreEqual (VirtualSubnet::kGatewayIp, sink.tcp.back().sourceIp, L"a peer on this PC appears as the gateway");

            synAck                 = FromGuest (P::kTcpSyn | P::kTcpAck, kGuestIsn, sink.tcp.back().sequence + 1);
            synAck.sourcePort      = kForwardedPort;
            synAck.destinationIp   = VirtualSubnet::kGatewayIp;
            synAck.destinationPort = kRemotePort;
            nat.OnGuestSegment (synAck, 1);

            Assert::AreEqual ((Byte) P::kTcpAck, sink.tcp.back().flags);
            Assert::AreEqual (kGuestIsn + 1,     sink.tcp.back().acknowledgment);
        }
    };
}
