#include "Pch.h"

#include "Devices/Network/VirtualSubnet.h"

#include "DhcpTestPackets.h"
#include "UthernetRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridgeTests
//
//  Hardware TCP and UDP sockets against a fake host network, with the test
//  as the remote peer.
//
////////////////////////////////////////////////////////////////////////////////

namespace W5100SocketBridgeTests
{
    static constexpr uint16_t  kHttp     = 80;
    static constexpr uint16_t  kAdtPro   = 6502;
    static constexpr uint16_t  kPeerPort = 50000;
    static constexpr Word      kTwoKb    = 0x0800;

    static std::vector<Byte> MakePattern (size_t count, Byte seed)
    {
        std::vector<Byte>  bytes (count);
        size_t             i     = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) (seed + i * 7);
        }

        return bytes;
    }


    static Word ReadWord (UthernetRig & rig, int socket, Word offset)
    {
        return rig.bus.PeekWord (W5100Driver::SocketReg (socket, offset));
    }





    TEST_CLASS (W5100SocketBridgeTcpTests)
    {
    public:

        TEST_METHOD (ConnectEstablishesAndRaisesCon)
        {
            UthernetRig  rig;

            auto  peer = rig.ConnectEstablished (1, UthernetRig::kLanPeer, kHttp);

            Assert::IsNotNull (peer.get(), L"a host connect to the destination");
            Assert::AreEqual (R::kSockEstablished, rig.bus.Status (1));
            Assert::AreEqual (R::kSnIrConnect,     (Byte) (rig.bus.Interrupts (1) & R::kSnIrConnect));
        }


        TEST_METHOD (ConnectIsSynSentUntilTheHostCompletes)
        {
            UthernetRig  rig;

            rig.OpenSocket (1, R::kProtocolTcp, 0xC000);
            rig.SetDestination (1, UthernetRig::kLanPeer, kHttp);
            rig.bus.Command (1, R::kCmdConnect);
            rig.Pump();

            Assert::AreEqual (R::kSockSynSent, rig.bus.Status (1));
        }


        TEST_METHOD (RefusedConnectClosesWithTimeout)
        {
            UthernetRig  rig;

            rig.OpenSocket (1, R::kProtocolTcp, 0xC000);
            rig.SetDestination (1, UthernetRig::kLanPeer, kHttp);
            rig.bus.Command (1, R::kCmdConnect);

            rig.network.FindConnected ({ UthernetRig::kLanPeer, kHttp })->connect = FakeSocketState::ConnectOutcome::Refused;
            rig.Pump();

            Assert::AreEqual (R::kSockClosed,  rig.bus.Status (1));
            Assert::AreEqual (R::kSnIrTimeout, (Byte) (rig.bus.Interrupts (1) & R::kSnIrTimeout));
        }


        TEST_METHOD (NoHostNetworkMeansTimeout)
        {
            UthernetRig  rig ({}, false);

            rig.OpenSocket (1, R::kProtocolTcp, 0xC000);
            rig.SetDestination (1, UthernetRig::kLanPeer, kHttp);
            rig.bus.Command (1, R::kCmdConnect);

            Assert::AreEqual (R::kSockClosed,  rig.bus.Status (1));
            Assert::AreEqual (R::kSnIrTimeout, (Byte) (rig.bus.Interrupts (1) & R::kSnIrTimeout));
        }


        TEST_METHOD (GatewayDestinationConnectsToLoopback)
        {
            UthernetRig  rig;

            auto  peer = rig.ConnectEstablished (0, VirtualSubnet::kGatewayIp, kAdtPro);
            Assert::IsNull (peer.get(), L"nothing connects to the gateway address itself");

            Assert::IsNotNull (rig.network.FindConnected ({ VirtualSubnet::kLoopbackIp, kAdtPro }).get());
        }


        TEST_METHOD (SendAcrossTheBufferEndReachesThePeer)
        {
            UthernetRig        rig;
            std::vector<Byte>  first  = MakePattern (0x600, 1);
            std::vector<Byte>  second = MakePattern (0x400, 9);
            std::vector<Byte>  both   = first;

            auto  peer = rig.ConnectEstablished (2, UthernetRig::kLanPeer, kHttp);

            rig.bus.Send (2, first);
            rig.bus.ClearInterrupts (2, 0xFF);
            rig.bus.Send (2, second);
            both.insert (both.end(), second.begin(), second.end());

            Assert::IsTrue  (peer->sent == both, L"bytes arrive in order across the 2 KB wrap");
            Assert::AreEqual (kTwoKb,           ReadWord (rig, 2, R::kSnTxFsr));
            Assert::AreEqual (R::kSnIrSendOk,   (Byte) (rig.bus.Interrupts (2) & R::kSnIrSendOk));
        }


        TEST_METHOD (PartialHostSendHoldsFreeSpaceUntilTaken)
        {
            UthernetRig        rig;
            std::vector<Byte>  data = MakePattern (0x300, 3);

            auto  peer = rig.ConnectEstablished (1, UthernetRig::kLanPeer, kHttp);

            rig.bus.ClearInterrupts (1, 0xFF);
            peer->sendLimit = 0x100;
            rig.bus.Send (1, data);

            Assert::AreEqual ((Word) (kTwoKb - 0x200), ReadWord (rig, 1, R::kSnTxFsr));
            Assert::AreEqual ((Byte) 0, (Byte) (rig.bus.Interrupts (1) & R::kSnIrSendOk), L"no SEND_OK yet");

            peer->sendLimit = SIZE_MAX;
            rig.Pump();

            Assert::AreEqual (kTwoKb,         ReadWord (rig, 1, R::kSnTxFsr));
            Assert::AreEqual (R::kSnIrSendOk, (Byte) (rig.bus.Interrupts (1) & R::kSnIrSendOk));
            Assert::IsTrue (peer->sent == data);
        }


        //  Bytes waiting for a full host buffer, then the peer resets: the
        //  failed send closes the socket, in a pump and in a DISCON.
        TEST_METHOD (AResetWhileBytesWait_ClosesTheSocket)
        {
            for (bool disconnect : { false, true })
            {
                UthernetRig  rig;

                auto  peer = rig.ConnectEstablished (1, UthernetRig::kLanPeer, kHttp);

                peer->sendLimit = 0;
                rig.bus.Send (1, MakePattern (0x40, 9));
                peer->resetByPeer = true;

                if (disconnect)
                {
                    rig.bus.Command (1, R::kCmdDisconnect);
                }
                else
                {
                    rig.Pump();
                }

                Assert::AreEqual (R::kSockClosed, rig.bus.Status (1));
                Assert::IsTrue (peer->destroyed);
            }
        }


        TEST_METHOD (ReceiveIsLimitedToRxFreeSpace)
        {
            UthernetRig        rig;
            std::vector<Byte>  data = MakePattern (0x900, 5);
            std::vector<Byte>  got;
            std::vector<Byte>  rest;

            auto  peer = rig.ConnectEstablished (3, UthernetRig::kLanPeer, kHttp);

            peer->inbound.assign (data.begin(), data.end());
            rig.Pump();

            Assert::AreEqual (kTwoKb,          ReadWord (rig, 3, R::kSnRxRsr));
            Assert::AreEqual (R::kSnIrReceive, (Byte) (rig.bus.Interrupts (3) & R::kSnIrReceive));

            got = rig.bus.ReceiveAll (3);
            rig.Pump();
            rest = rig.bus.ReceiveAll (3);
            got.insert (got.end(), rest.begin(), rest.end());

            Assert::IsTrue (got == data, L"all bytes, in order, once RECV frees space");
        }


        TEST_METHOD (PeerCloseIsCloseWaitThenDisconCloses)
        {
            UthernetRig  rig;

            auto  peer = rig.ConnectEstablished (1, UthernetRig::kLanPeer, kHttp);

            peer->inboundEof = true;
            rig.Pump();

            Assert::AreEqual (R::kSockCloseWait,     rig.bus.Status (1));
            Assert::AreEqual (R::kSnIrDisconnect,    (Byte) (rig.bus.Interrupts (1) & R::kSnIrDisconnect));

            rig.bus.Command (1, R::kCmdDisconnect);

            Assert::AreEqual (R::kSockClosed, rig.bus.Status (1));
            Assert::IsTrue (peer->shutdownSent);
            Assert::IsTrue (peer->destroyed);
        }


        TEST_METHOD (ListenAcceptsFromThisPcOnly)
        {
            UthernetRig  rig;

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);

            auto  listener = rig.network.FindBound (HostSocketKind::Tcp, kAdtPro);

            Assert::AreEqual (R::kSockListen,           rig.bus.Status (0));
            Assert::IsNotNull (listener.get());
            Assert::IsTrue (listener->listening);
            Assert::AreEqual (VirtualSubnet::kLoopbackIp, listener->local.ipv4, L"loopback without LAN access");

            rig.network.OfferConnection (listener, { VirtualSubnet::kLoopbackIp, kPeerPort });
            rig.Pump();

            Assert::AreEqual (R::kSockEstablished,        rig.bus.Status (0));
            Assert::AreEqual ((Word) kPeerPort,           ReadWord (rig, 0, R::kSnDport));
            Assert::AreEqual ((Word) (VirtualSubnet::kGatewayIp >> 16), ReadWord (rig, 0, R::kSnDipr), L"the PC appears as the gateway");
        }


        TEST_METHOD (ListenWithLanAccessBindsEveryInterface)
        {
            NetworkCardSettings  settings;

            settings.lanAccess = true;

            UthernetRig  rig (settings);

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);

            Assert::AreEqual (VirtualSubnet::kAnyIp, rig.network.FindBound (HostSocketKind::Tcp, kAdtPro)->local.ipv4);
        }


        TEST_METHOD (ListenOnAPortInUseClosesWithTimeout)
        {
            UthernetRig  rig;

            rig.network.OccupyPort (HostSocketKind::Tcp, kAdtPro);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);

            Assert::AreEqual (R::kSockClosed,  rig.bus.Status (0));
            Assert::AreEqual (R::kSnIrTimeout, (Byte) (rig.bus.Interrupts (0) & R::kSnIrTimeout));
        }


        TEST_METHOD (SoftwareResetClosesHostSockets)
        {
            UthernetRig  rig;

            auto  peer = rig.ConnectEstablished (1, UthernetRig::kLanPeer, kHttp);

            rig.bus.Initialize();

            Assert::IsTrue (peer->destroyed);
            Assert::AreEqual (R::kSockClosed, rig.bus.Status (1));
        }
    };





    TEST_CLASS (W5100SocketBridgeUdpTests)
    {
    public:

        TEST_METHOD (OpenBindsTheSourcePort)
        {
            UthernetRig  rig;

            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);

            auto  bound = rig.network.FindBound (HostSocketKind::Udp, kAdtPro);

            Assert::AreEqual (R::kSockUdp, rig.bus.Status (1));
            Assert::IsNotNull (bound.get());
            Assert::IsTrue (bound->allowBroadcast);
        }


        TEST_METHOD (TakenPortFallsBackToEphemeral)
        {
            UthernetRig  rig;

            rig.network.OccupyPort (HostSocketKind::Udp, kAdtPro);
            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);

            Assert::AreEqual (R::kSockUdp, rig.bus.Status (1));
            Assert::IsNotNull (rig.network.FindBound (HostSocketKind::Udp, FakeHostNetwork::kFirstEphemeralPort).get());
        }


        TEST_METHOD (SendIsOneDatagramToTheDestination)
        {
            UthernetRig        rig;
            std::vector<Byte>  data = MakePattern (100, 2);

            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);
            rig.SetDestination (1, UthernetRig::kLanPeer, kAdtPro);
            rig.bus.Send (1, data);

            auto  socket = rig.network.FindBound (HostSocketKind::Udp, kAdtPro);

            Assert::AreEqual ((size_t) 1, socket->sentDatagrams.size());
            Assert::IsTrue (socket->sentDatagrams[0].peer == HostEndpoint { UthernetRig::kLanPeer, kAdtPro });
            Assert::IsTrue (socket->sentDatagrams[0].bytes == data);
            Assert::AreEqual (R::kSnIrSendOk, (Byte) (rig.bus.Interrupts (1) & R::kSnIrSendOk));
        }


        TEST_METHOD (ReceivedDatagramHasTheEightByteHeader)
        {
            UthernetRig                    rig;
            std::vector<Byte>              data  = MakePattern (3, 4);
            UthernetRig::ReceivedDatagram  got;

            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);
            rig.SetDestination (1, UthernetRig::kLanPeer, kAdtPro);
            rig.bus.Send (1, data);

            rig.network.FindBound (HostSocketKind::Udp, kAdtPro)->inboundDatagrams.push_back ({ { UthernetRig::kLanPeer, kAdtPro }, data });
            rig.Pump();

            Assert::AreEqual ((Word) 11, ReadWord (rig, 1, R::kSnRxRsr), L"8-byte header + 3-byte payload");
            Assert::IsTrue (rig.TryReceiveDatagram (1, got));
            Assert::AreEqual (UthernetRig::kLanPeer, got.ipv4);
            Assert::AreEqual (kAdtPro,               got.port);
            Assert::IsTrue (got.payload == data, L"the length field counts the payload only");
        }


        TEST_METHOD (UnsolicitedLanDatagramsNeedLanAccess)
        {
            std::vector<Byte>     data = MakePattern (16, 6);
            NetworkCardSettings   open;

            open.lanAccess = true;

            for (bool lanAccess : { false, true })
            {
                UthernetRig  rig (lanAccess ? open : NetworkCardSettings {});

                rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);

                auto  socket = rig.network.FindBound (HostSocketKind::Udp, kAdtPro);

                socket->inboundDatagrams.push_back ({ { UthernetRig::kOtherLanPeer, kPeerPort }, data });
                socket->inboundDatagrams.push_back ({ { VirtualSubnet::kLoopbackIp, kPeerPort }, data });
                rig.Pump();

                Assert::AreEqual ((Word) ((lanAccess ? 2 : 1) * (8 + data.size())),
                                  ReadWord (rig, 1, R::kSnRxRsr),
                                  lanAccess ? L"both with LAN access" : L"only this PC's without");
            }
        }


        TEST_METHOD (DatagramsWaitForSpaceAndOversizedAreDiscarded)
        {
            UthernetRig                    rig;
            std::vector<Byte>              tooBig = MakePattern (kTwoKb - 7, 1);
            std::vector<Byte>              half   = MakePattern (0x500, 2);
            UthernetRig::ReceivedDatagram  got;

            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);

            auto  socket = rig.network.FindBound (HostSocketKind::Udp, kAdtPro);

            socket->inboundDatagrams.push_back ({ { VirtualSubnet::kLoopbackIp, kPeerPort }, tooBig });
            socket->inboundDatagrams.push_back ({ { VirtualSubnet::kLoopbackIp, kPeerPort }, half });
            socket->inboundDatagrams.push_back ({ { VirtualSubnet::kLoopbackIp, kPeerPort }, half });
            rig.Pump();

            Assert::AreEqual ((Word) (8 + half.size()), ReadWord (rig, 1, R::kSnRxRsr), L"one fits; the other waits");

            Assert::IsTrue (rig.TryReceiveDatagram (1, got));
            rig.Pump();

            Assert::AreEqual ((Word) (8 + half.size()), ReadWord (rig, 1, R::kSnRxRsr), L"the second arrives after RECV");
        }


        TEST_METHOD (DhcpGoesToTheVirtualServer)
        {
            constexpr uint16_t  kDhcpClient = 68;
            constexpr uint16_t  kDhcpServer = 67;
            constexpr uint32_t  kXid        = 0x12345678;

            UthernetRig                    rig ({}, false);
            MacAddress::Octets             mac  = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
            UthernetRig::ReceivedDatagram  got;

            rig.OpenSocket (1, R::kProtocolUdp, kDhcpClient);
            rig.SetDestination (1, VirtualSubnet::kBroadcastIp, kDhcpServer);
            rig.bus.Send (1, DhcpTestPackets::MakeRequest (DhcpTestPackets::kDiscover, mac, kXid));

            Assert::IsTrue (rig.TryReceiveDatagram (1, got));
            Assert::AreEqual (VirtualSubnet::kGatewayIp,  got.ipv4);
            Assert::AreEqual (kDhcpServer,                got.port);
            Assert::AreEqual (DhcpTestPackets::kOffer,    DhcpTestPackets::GetMessageType (got.payload));
            Assert::AreEqual (VirtualSubnet::kGuestIp,    DhcpTestPackets::GetYourAddress (got.payload));
            Assert::AreEqual (kXid,                       DhcpTestPackets::GetXid (got.payload));
        }
    };
}
