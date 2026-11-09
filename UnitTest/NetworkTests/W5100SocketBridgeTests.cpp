#include "Pch.h"

#include "CassoLink/LinkAnnouncement.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Net/ListenGate.h"

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


    //  The PC ports a listener on the guest's port can use.
    static FirewallPort MakeListenRange (uint16_t port, uint16_t now)
    {
        return { HostSocketKind::Tcp, now, (uint16_t) (port + W5100SocketBridge::kListenPorts - 1), port };
    }


    //  Every port a listener on kAdtPro can use, in use on the PC.
    static void OccupyListenRange (UthernetRig & rig)
    {
        for (int i = 0; i < W5100SocketBridge::kListenPorts; i++)
        {
            rig.network.OccupyPort (HostSocketKind::Tcp, (uint16_t) (kAdtPro + i));
        }
    }


    //  A CassoLink announcement of 10.0.65.15 and the port.
    static std::vector<Byte> MakeAnnouncement (uint16_t port)
    {
        std::vector<Byte>  bytes = { 'C', 'A', 'S', 'S', 'O', 'L', 'I', 'N', 'K', LinkAnnouncement::kVersion, 10, 0, 65, 15 };

        bytes.push_back ((Byte) (port >> 8));
        bytes.push_back ((Byte) port);
        bytes.insert (bytes.end(), { 0x02, 0x00, 0x00, 0x12, 0x34, 0x56 });

        return bytes;
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


        //  Changing LAN access binds a listening socket again on the new
        //  address, so the Apple goes on listening.
        TEST_METHOD (ChangingLanAccess_BindsAListenerAgain)
        {
            UthernetRig          rig;
            NetworkCardSettings  settings;

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);

            settings.lanAccess = true;
            rig.card.ApplySettings (settings);

            auto  listener = rig.network.FindBound (HostSocketKind::Tcp, kAdtPro);

            Assert::IsNotNull (listener.get());
            Assert::IsTrue   (listener->listening);
            Assert::AreEqual (VirtualSubnet::kAnyIp, listener->local.ipv4);
            Assert::AreEqual (R::kSockListen, rig.bus.Status (0));

            settings.lanAccess = false;
            rig.card.ApplySettings (settings);

            Assert::AreEqual (VirtualSubnet::kLoopbackIp, rig.network.FindBound (HostSocketKind::Tcp, kAdtPro)->local.ipv4);
        }


        //  Without LAN access a broadcast goes to 127.0.0.1, so another PC
        //  doesn't hear of an Apple it can't reach.
        TEST_METHOD (ABroadcastWithoutLanAccess_StaysOnThePc)
        {
            UthernetRig        rig;
            std::vector<Byte>  data = MakePattern (20, 3);

            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);
            rig.SetDestination (1, VirtualSubnet::kBroadcastIp, kAdtPro);
            rig.bus.Send (1, data);

            auto  socket = rig.network.FindBound (HostSocketKind::Udp, kAdtPro);

            Assert::AreEqual ((size_t) 1, socket->sentDatagrams.size());
            Assert::IsTrue (socket->sentDatagrams[0].peer == HostEndpoint { VirtualSubnet::kLoopbackIp, kAdtPro });
        }


        TEST_METHOD (ABroadcastWithLanAccess_GoesToTheLan)
        {
            NetworkCardSettings  settings;

            settings.lanAccess = true;

            UthernetRig        rig (settings);
            std::vector<Byte>  data = MakePattern (20, 3);

            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);
            rig.SetDestination (1, VirtualSubnet::kBroadcastIp, kAdtPro);
            rig.bus.Send (1, data);

            Assert::IsTrue (rig.network.FindBound (HostSocketKind::Udp, kAdtPro)->sentDatagrams[0].peer == HostEndpoint { VirtualSubnet::kBroadcastIp, kAdtPro });
        }


        TEST_METHOD (AListenerAndAUdpSocket_AreInboundPorts)
        {
            UthernetRig                rig;
            std::vector<FirewallPort>  ports;

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.OpenSocket (1, R::kProtocolUdp, kPeerPort);
            rig.Pump();

            ports = rig.card.GetInboundPorts();

            Assert::AreEqual ((size_t) 2, ports.size());
            Assert::IsTrue   (ports[0] == MakeListenRange (kAdtPro, kAdtPro), L"a listener's range is one Allow admits");
            Assert::IsTrue   (ports[1] == FirewallPort { HostSocketKind::Udp, kPeerPort });
        }


        //  A UDP socket bound to an ephemeral port has no fixed port to admit.
        TEST_METHOD (AUdpSocketOnAnEphemeralPort_IsNotAnInboundPort)
        {
            UthernetRig  rig;

            rig.network.OccupyPort (HostSocketKind::Udp, kPeerPort);
            rig.OpenSocket (1, R::kProtocolUdp, kPeerPort);
            rig.Pump();

            Assert::IsTrue (rig.card.GetInboundPorts().empty());
        }


        TEST_METHOD (ListenWithEveryPortInUseClosesWithTimeout)
        {
            UthernetRig  rig;

            OccupyListenRange (rig);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);

            Assert::AreEqual (R::kSockClosed,  rig.bus.Status (0));
            Assert::AreEqual (R::kSnIrTimeout, (Byte) (rig.bus.Interrupts (0) & R::kSnIrTimeout));
        }


        //  A second Casso on the PC listens on the next port, and the guest's
        //  socket is on 6502.
        TEST_METHOD (AListenOnAPortInUse_MovesToTheNextPort)
        {
            UthernetRig                                  rig;
            std::vector<W5100SocketBridge::MovedListen>  moved;

            rig.network.OccupyPort (HostSocketKind::Tcp, kAdtPro);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.Pump();

            auto  listener = rig.network.FindBound (HostSocketKind::Tcp, kAdtPro + 1);

            Assert::AreEqual  (R::kSockListen, rig.bus.Status (0));
            Assert::IsNotNull (listener.get());
            Assert::IsTrue    (listener->listening);
            Assert::IsTrue    (rig.card.GetBridge().GetFailedListens().empty(), L"a moved listen is not a failed one");
            Assert::IsTrue    (rig.card.GetInboundPorts() == std::vector<FirewallPort> { MakeListenRange (kAdtPro, kAdtPro + 1) }, L"the PC port is the inbound one");

            moved = rig.card.GetBridge().GetMovedListens();

            Assert::AreEqual ((size_t) 1, moved.size());
            Assert::AreEqual ((uint16_t) kAdtPro,       moved[0].guestPort);
            Assert::AreEqual ((uint16_t) (kAdtPro + 1), moved[0].hostPort);
            Assert::AreEqual (std::string ("The Uthernet II is listening on TCP port 6503, because port 6502 is in use on this PC."),
                              W5100SocketBridge::DescribeMovedListen (moved[0]));

            rig.network.OfferConnection (listener, { VirtualSubnet::kLoopbackIp, kPeerPort });
            rig.Pump();

            Assert::AreEqual (R::kSockEstablished, rig.bus.Status (0), L"a connection to the PC's port is established on the guest's socket");
        }


        //  Casso Explorer connects to the port an announcement contains, so a
        //  moved listener's announcement has the PC's port.
        TEST_METHOD (AnAnnouncement_HasTheMovedListenersPort)
        {
            UthernetRig       rig;
            LinkAnnouncement  sent;

            rig.network.OccupyPort (HostSocketKind::Tcp, kAdtPro);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.OpenSocket (1, R::kProtocolUdp, kPeerPort);
            rig.SetDestination (1, VirtualSubnet::kBroadcastIp, LinkProgram::kAnnouncePort);
            rig.bus.Send (1, MakeAnnouncement (kAdtPro));

            auto  socket = rig.network.FindBound (HostSocketKind::Udp, kPeerPort);

            Assert::AreEqual ((size_t) 1, socket->sentDatagrams.size());
            Assert::IsTrue   (LinkAnnouncement::Parse (socket->sentDatagrams[0].bytes, sent));
            Assert::AreEqual ((uint16_t) (kAdtPro + 1), sent.port);
        }


        TEST_METHOD (AnAnnouncementOfAListenerOnItsPort_IsSentAsItIs)
        {
            UthernetRig  rig;

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.OpenSocket (1, R::kProtocolUdp, kPeerPort);
            rig.SetDestination (1, VirtualSubnet::kBroadcastIp, LinkProgram::kAnnouncePort);
            rig.bus.Send (1, MakeAnnouncement (kAdtPro));

            Assert::IsTrue (rig.network.FindBound (HostSocketKind::Udp, kPeerPort)->sentDatagrams[0].bytes == MakeAnnouncement (kAdtPro));
            Assert::IsTrue (rig.card.GetBridge().GetMovedListens().empty());
        }


        //  The link boot disk listens again every few seconds, so a port in
        //  use is one entry until a listen on it succeeds or the card resets.
        TEST_METHOD (AListenOnAPortInUse_IsListedUntilAListenSucceeds)
        {
            UthernetRig                                    rig;
            std::vector<W5100SocketBridge::ListenFailure>  failures;

            OccupyListenRange (rig);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);

            failures = rig.card.GetBridge().GetFailedListens();

            Assert::AreEqual ((size_t) 1, failures.size(), L"two listens, one entry");
            Assert::AreEqual ((uint16_t) kAdtPro, failures[0].port);
            Assert::AreEqual ((uint16_t) (kAdtPro + W5100SocketBridge::kListenPorts - 1), failures[0].lastPort);
            Assert::AreEqual (HRESULT_FROM_WIN32 (WSAEADDRINUSE), failures[0].hr);

            rig.network.ReleasePort (HostSocketKind::Tcp, kAdtPro);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);

            Assert::IsTrue (rig.card.GetBridge().GetFailedListens().empty(), L"the port opened");
        }


        TEST_METHOD (AResetClearsTheFailedListens)
        {
            UthernetRig  rig;

            OccupyListenRange (rig);
            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.card.GetBridge().OnReset();

            Assert::IsTrue (rig.card.GetBridge().GetFailedListens().empty(), L"a guest after a reset has listened on nothing yet");
        }

        TEST_METHOD (AFailedListen_IsDescribedByWhatHasThePort)
        {
            W5100SocketBridge::ListenFailure  inUse { 6502, 0, HRESULT_FROM_WIN32 (WSAEADDRINUSE) };
            W5100SocketBridge::ListenFailure  range { 6502, 6509, HRESULT_FROM_WIN32 (WSAEADDRINUSE) };
            W5100SocketBridge::ListenFailure  other { 6502, 0, E_FAIL };

            Assert::AreEqual (std::string ("The Uthernet II cannot listen on TCP port 6502, because the Super Serial Card in slot 2 is already using that port. "
                                           "Change the card's serial connector on the Serial card tab in Settings."),
                              W5100SocketBridge::DescribeListenFailure (inUse, 2, true));
            Assert::AreEqual (std::string ("The Uthernet II cannot listen on TCP port 6502, because serial port 2 is already using that port. "
                                           "Change that port's connector in Settings."),
                              W5100SocketBridge::DescribeListenFailure (inUse, 2, false));
            Assert::AreEqual (std::string ("The Uthernet II cannot listen on TCP port 6502, because another program on this PC has it."),
                              W5100SocketBridge::DescribeListenFailure (inUse, 0, false));
            Assert::AreEqual (std::string ("The Uthernet II cannot listen on TCP port 6502 or any port after it up to 6509, because other programs on this PC have them."),
                              W5100SocketBridge::DescribeListenFailure (range, 0, false));
            Assert::AreEqual (std::string ("The Uthernet II cannot listen on TCP port 6502."),
                              W5100SocketBridge::DescribeListenFailure (other, 0, false));
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


        //  For a taken port, another port isn't bound explicitly, which would
        //  raise a Windows Firewall prompt; the first send binds a temporary one.
        TEST_METHOD (TakenPortSendsFromATemporaryPort)
        {
            UthernetRig  rig;

            rig.network.OccupyPort (HostSocketKind::Udp, kAdtPro);
            rig.OpenSocket (1, R::kProtocolUdp, kAdtPro);
            rig.SetDestination (1, VirtualSubnet::kBroadcastIp, kAdtPro);
            rig.bus.Send (1, MakePattern (8, 1));

            auto  socket = rig.network.FindBound (HostSocketKind::Udp, FakeHostNetwork::kFirstEphemeralPort);

            Assert::AreEqual  (R::kSockUdp, rig.bus.Status (1));
            Assert::IsNotNull (socket.get());
            Assert::AreEqual  ((size_t) 1, socket->sentDatagrams.size());
            Assert::IsTrue    (socket->allowBroadcast, L"a broadcast is permitted");
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





    ////////////////////////////////////////////////////////////////////////////
    //
    //  W5100SocketBridgeGateTests
    //
    //  A port the firewall does not admit is deferred rather than opened, so
    //  no Windows Firewall prompt appears for it. It opens once the gate
    //  admits it.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (W5100SocketBridgeGateTests)
    {
    public:

        static constexpr const wchar_t *  kProgram = L"C:\\Casso\\Casso.exe";

        static FirewallProgram MakeProgram()
        {
            FirewallProgram  program;

            program.path = kProgram;

            return program;
        }


        //  A private network with the firewall on and no rule for Casso, or
        //  with an allow rule for each port given.
        static FirewallSnapshot MakeSnapshot (std::initializer_list<FirewallPort> admitted)
        {
            FirewallSnapshot  snapshot;

            snapshot.active = FirewallProfile::Private;

            for (const FirewallPort & port : admitted)
            {
                FirewallRule  rule;

                rule.name        = L"Casso";
                rule.application = kProgram;
                rule.anyProtocol = false;
                rule.protocol    = port.protocol;
                rule.localPorts  = std::to_wstring (port.port);
                rule.profiles    = FirewallProfile::Private;

                snapshot.rules.push_back (rule);
            }

            return snapshot;
        }


        static NetworkCardSettings MakeLanSettings()
        {
            NetworkCardSettings  settings;

            settings.lanAccess = true;

            return settings;
        }




        TEST_METHOD (AListenTheGateRefuses_ListensWithNoPcSocket_UntilAdmitted)
        {
            UthernetRig  rig (MakeLanSettings());
            ListenGate   gate;

            gate.Set (MakeSnapshot ({}), MakeProgram());
            rig.card.GetBridge().SetListenGate (&gate);

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.Pump();

            Assert::AreEqual (R::kSockListen, rig.bus.Status (0), L"the guest's socket listens");
            Assert::IsNull   (rig.network.FindBound (HostSocketKind::Tcp, kAdtPro).get(), L"and no PC socket is opened");
            Assert::IsTrue   (rig.card.GetBridge().GetDeferredPorts() == std::vector<FirewallPort> { MakeListenRange (kAdtPro, kAdtPro) });
            Assert::IsTrue   (rig.card.GetBridge().GetFailedListens().empty(), L"a deferred port is not a failed listen");

            gate.Set (MakeSnapshot ({ { HostSocketKind::Tcp, kAdtPro } }), MakeProgram());
            rig.Pump();

            Assert::IsNotNull (rig.network.FindBound (HostSocketKind::Tcp, kAdtPro).get(), L"the PC socket opens once admitted");
            Assert::AreEqual  (R::kSockListen, rig.bus.Status (0));
            Assert::IsTrue    (rig.card.GetBridge().GetDeferredPorts().empty());
        }


        //  A port an older rule admits alone, in use, moves the listener to
        //  the next port, which is deferred until Allow admits the range.
        TEST_METHOD (AMovedListenTheGateRefuses_IsDeferredOnTheNextPort)
        {
            UthernetRig  rig (MakeLanSettings());
            ListenGate   gate;

            gate.Set (MakeSnapshot ({ { HostSocketKind::Tcp, kAdtPro } }), MakeProgram());
            rig.card.GetBridge().SetListenGate (&gate);
            rig.network.OccupyPort (HostSocketKind::Tcp, kAdtPro);

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.Pump();

            Assert::AreEqual (R::kSockListen, rig.bus.Status (0));
            Assert::IsTrue   (rig.card.GetBridge().GetDeferredPorts() == std::vector<FirewallPort> { MakeListenRange (kAdtPro, kAdtPro + 1) });

            gate.Set (MakeSnapshot ({ { HostSocketKind::Tcp, kAdtPro }, { HostSocketKind::Tcp, kAdtPro + 1 } }), MakeProgram());
            rig.Pump();

            Assert::IsNotNull (rig.network.FindBound (HostSocketKind::Tcp, kAdtPro + 1).get(), L"the next port opens once admitted");
            Assert::IsTrue    (rig.card.GetBridge().GetDeferredPorts().empty());
        }


        //  With LAN access off a listener binds 127.0.0.1, which the firewall
        //  does not filter, so nothing is deferred.
        TEST_METHOD (WithLanAccessOff_AListenIsNotHeld)
        {
            UthernetRig  rig;
            ListenGate   gate;

            gate.Set (MakeSnapshot ({}), MakeProgram());
            rig.card.GetBridge().SetListenGate (&gate);

            rig.OpenSocket (0, R::kProtocolTcp, kAdtPro);
            rig.bus.Command (0, R::kCmdListen);
            rig.Pump();

            Assert::IsNotNull (rig.network.FindBound (HostSocketKind::Tcp, kAdtPro).get());
            Assert::IsTrue    (rig.card.GetBridge().GetDeferredPorts().empty());
        }


        //  With LAN access off no datagram from the LAN is delivered to the socket,
        //  so a port the gate doesn't admit isn't deferred; the socket sends
        //  unbound instead of binding the port, which would raise a prompt.
        TEST_METHOD (WithLanAccessOff_AUdpSocketTheGateRefuses_SendsUnbound)
        {
            UthernetRig  rig;
            ListenGate   gate;

            gate.Set (MakeSnapshot ({}), MakeProgram());
            rig.card.GetBridge().SetListenGate (&gate);

            rig.OpenSocket (1, R::kProtocolUdp, kPeerPort);
            rig.SetDestination (1, VirtualSubnet::kBroadcastIp, kAdtPro);
            rig.bus.Send (1, MakePattern (8, 1));

            Assert::IsNull    (rig.network.FindBound (HostSocketKind::Udp, kPeerPort).get(), L"the guest's port is not bound");
            Assert::IsNotNull (rig.network.FindBound (HostSocketKind::Udp, FakeHostNetwork::kFirstEphemeralPort).get(), L"the send binds a temporary port");
            Assert::IsTrue    (rig.card.GetBridge().GetDeferredPorts().empty());
        }


        TEST_METHOD (AUdpSocketTheGateRefuses_OpensWithNoPcSocket_UntilAdmitted)
        {
            UthernetRig  rig (MakeLanSettings());
            ListenGate   gate;

            gate.Set (MakeSnapshot ({}), MakeProgram());
            rig.card.GetBridge().SetListenGate (&gate);

            rig.OpenSocket (1, R::kProtocolUdp, kPeerPort);
            rig.Pump();

            Assert::AreEqual (R::kSockUdp, rig.bus.Status (1), L"the guest's socket is open");
            Assert::IsNull   (rig.network.FindBound (HostSocketKind::Udp, kPeerPort).get(), L"and no PC socket is bound");
            Assert::IsTrue   (rig.card.GetInboundPorts() == std::vector<FirewallPort> { { HostSocketKind::Udp, kPeerPort } }, L"the deferred port is one Allow admits");

            gate.Set (MakeSnapshot ({ { HostSocketKind::Udp, kPeerPort } }), MakeProgram());
            rig.Pump();

            Assert::IsNotNull (rig.network.FindBound (HostSocketKind::Udp, kPeerPort).get(), L"the PC socket binds once admitted");
            Assert::IsTrue    (rig.card.GetBridge().GetDeferredPorts().empty());
        }
    };
}
