#include "Pch.h"

#include "HResultAssert.h"

#include "Devices/Network/PacketCodec.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Seams/Win32HostNetwork.h"

#include "NetworkTests/W5100Driver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetLoopbackTests
//
//  The Uthernet II over real Winsock on 127.0.0.1: the guest side driven
//  through the W5100's registers, the peer a socket the test opens. The unit
//  suite covers the same paths against a fake; this is the check that the
//  Win32 implementation behind the seam does what the fake does.
//
////////////////////////////////////////////////////////////////////////////////

namespace UthernetLoopbackTests
{
    static constexpr size_t  kLength     = 1024;
    static constexpr DWORD   kDeadlineMs = 5000;


    static std::vector<Byte> MakePattern (size_t count, Byte seed)
    {
        std::vector<Byte>  bytes (count);
        size_t             i     = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) (seed + i * 3);
        }

        return bytes;
    }


    // Pumps the card until `done` is true or the deadline passes.
    static void PumpUntil (UthernetCard & card, const std::function<bool()> & done, const wchar_t * what)
    {
        ULONGLONG  start = GetTickCount64();

        while (!done())
        {
            Assert::IsTrue (GetTickCount64() - start < kDeadlineMs, what);

            card.Tick (UthernetCard::kPumpCycles);
            Sleep (1);
        }
    }





    TEST_CLASS (UthernetLoopbackTests)
    {
    public:

        TEST_METHOD (TcpRoundTripThroughWinsock)
        {
            Win32HostNetwork               host;
            NetworkCardSettings            settings;
            UthernetCard                   card (3);
            W5100Driver                    bus (card.GetChip());
            std::unique_ptr<IHostSocket>   listener;
            std::unique_ptr<IHostSocket>   peer;
            HostEndpoint                   local;
            HostEndpoint                   from;
            std::vector<Byte>              outbound = MakePattern (kLength, 0x11);
            std::vector<Byte>              reply    = MakePattern (kLength, 0x99);
            std::vector<Byte>              received;
            std::vector<Byte>              buffer (kLength);
            size_t                         count    = 0;
            size_t                         sent     = 0;
            bool                           eof      = false;
            bool                           blocked  = false;

            card.AttachNetwork (&host, settings);
            bus.Initialize();

            AssertSucceeded (host.CreateSocket (HostSocketKind::Tcp, listener));
            AssertSucceeded (listener->Bind ({ VirtualSubnet::kLoopbackIp, 0 }, false));
            AssertSucceeded (listener->Listen());
            AssertSucceeded (listener->GetLocal (local));

            bus.Poke     (W5100Driver::SocketReg (0, R::kSnMr), R::kProtocolTcp);
            bus.Command  (0, R::kCmdOpen);
            bus.PokeWord (W5100Driver::SocketReg (0, R::kSnDipr),     (Word) (VirtualSubnet::kGatewayIp >> 16));
            bus.PokeWord (W5100Driver::SocketReg (0, R::kSnDipr + 2), (Word) (VirtualSubnet::kGatewayIp & 0xFFFF));
            bus.PokeWord (W5100Driver::SocketReg (0, R::kSnDport),    local.port);
            bus.Command  (0, R::kCmdConnect);

            PumpUntil (card, [&] { AssertSucceeded (listener->Accept (peer, from, blocked)); return peer != nullptr; }, L"the PC accepts");
            PumpUntil (card, [&] { return bus.Status (0) == R::kSockEstablished; }, L"the guest is established");

            bus.Send (0, outbound);

            PumpUntil (card, [&]
            {
                AssertSucceeded (peer->Receive (buffer, count, eof));
                received.insert (received.end(), buffer.begin(), buffer.begin() + count);
                return received.size() == kLength;
            }, L"the PC receives 1 KB");
            Assert::IsTrue (received == outbound);

            AssertSucceeded (peer->Send (reply, sent));
            Assert::AreEqual (kLength, sent);

            received.clear();
            PumpUntil (card, [&]
            {
                std::vector<Byte>  chunk = bus.ReceiveAll (0);
                received.insert (received.end(), chunk.begin(), chunk.end());
                return received.size() == kLength;
            }, L"the guest receives 1 KB");
            Assert::IsTrue (received == reply);

            bus.Command (0, R::kCmdDisconnect);
            PumpUntil (card, [&] { AssertSucceeded (peer->Receive (buffer, count, eof)); return eof; }, L"the PC sees the close");
        }


        TEST_METHOD (UdpRoundTripThroughWinsock)
        {
            Win32HostNetwork               host;
            NetworkCardSettings            settings;
            UthernetCard                   card (3);
            W5100Driver                    bus (card.GetChip());
            std::unique_ptr<IHostSocket>   peer;
            HostEndpoint                   local;
            HostEndpoint                   from;
            std::vector<Byte>              outbound = MakePattern (kLength, 0x22);
            std::vector<Byte>              reply    = MakePattern (kLength, 0x44);
            std::vector<Byte>              buffer (kLength * 2);
            std::vector<Byte>              block;
            size_t                         count    = 0;

            card.AttachNetwork (&host, settings);
            bus.Initialize();

            AssertSucceeded (host.CreateSocket (HostSocketKind::Udp, peer));
            AssertSucceeded (peer->Bind ({ VirtualSubnet::kLoopbackIp, 0 }, false));
            AssertSucceeded (peer->GetLocal (local));

            bus.Poke     (W5100Driver::SocketReg (1, R::kSnMr), R::kProtocolUdp);
            bus.Command  (1, R::kCmdOpen);
            bus.PokeWord (W5100Driver::SocketReg (1, R::kSnDipr),     (Word) (VirtualSubnet::kGatewayIp >> 16));
            bus.PokeWord (W5100Driver::SocketReg (1, R::kSnDipr + 2), (Word) (VirtualSubnet::kGatewayIp & 0xFFFF));
            bus.PokeWord (W5100Driver::SocketReg (1, R::kSnDport),    local.port);
            bus.Send     (1, outbound);

            PumpUntil (card, [&] { AssertSucceeded (peer->ReceiveFrom (buffer, count, from)); return count != 0; }, L"the PC receives the datagram");
            Assert::AreEqual (kLength, count);
            Assert::IsTrue (std::equal (outbound.begin(), outbound.end(), buffer.begin()));

            AssertSucceeded (peer->SendTo (from, reply));

            PumpUntil (card, [&] { return bus.PeekWord (W5100Driver::SocketReg (1, R::kSnRxRsr)) != 0; }, L"the guest receives the reply");
            block = bus.ReceiveAll (1);

            Assert::AreEqual (R::kUdpHeaderLength + kLength, block.size());
            Assert::AreEqual (VirtualSubnet::kGatewayIp, PacketCodec::ReadU32 (block, 0), L"this PC appears as the gateway");
            Assert::AreEqual (local.port,                PacketCodec::ReadU16 (block, 4));
            Assert::IsTrue (std::equal (reply.begin(), reply.end(), block.begin() + R::kUdpHeaderLength));
        }
    };
}
