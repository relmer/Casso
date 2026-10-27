#include "Pch.h"

#include "Devices/Network/DhcpServer.h"
#include "Devices/Network/VirtualSubnet.h"

#include "DhcpTestPackets.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpServerTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DhcpServerTests
{
    static constexpr uint32_t  kXid = 0xCAFEF00D;

    static constexpr MacAddress::Octets  kClientMac = { 0x00, 0x08, 0xDC, 0x11, 0x11, 0x11 };

    static constexpr Byte  kOptionSubnetMask = 1;
    static constexpr Byte  kOptionRouter     = 3;
    static constexpr Byte  kOptionDns        = 6;
    static constexpr Byte  kOptionLeaseTime  = 51;
    static constexpr Byte  kOptionServerId   = 54;


    static DhcpServer MakeServer()
    {
        return DhcpServer ({ VirtualSubnet::kGuestIp, VirtualSubnet::kSubnetMask, VirtualSubnet::kGatewayIp, VirtualSubnet::kDnsIp });
    }





    TEST_CLASS (DhcpServerTests)
    {
    public:

        TEST_METHOD (DiscoverGetsAnOfferWithTheLease)
        {
            DhcpServer         server = MakeServer();
            std::vector<Byte>  reply  = server.Process (DhcpTestPackets::MakeRequest (DhcpTestPackets::kDiscover, kClientMac, kXid));

            Assert::IsTrue (reply.size() >= 300, L"BOOTP minimum");
            Assert::AreEqual ((Byte) 2,                     reply[0], L"BOOTREPLY");
            Assert::AreEqual (DhcpTestPackets::kOffer,      DhcpTestPackets::GetMessageType (reply));
            Assert::AreEqual (kXid,                         DhcpTestPackets::GetXid (reply));
            Assert::AreEqual (VirtualSubnet::kGuestIp,      DhcpTestPackets::GetYourAddress (reply));
            Assert::IsTrue (std::equal (kClientMac.begin(), kClientMac.end(), reply.begin() + 28), L"chaddr echoed");

            Assert::AreEqual (VirtualSubnet::kSubnetMask,   DhcpTestPackets::GetOption32 (reply, kOptionSubnetMask));
            Assert::AreEqual (VirtualSubnet::kGatewayIp,    DhcpTestPackets::GetOption32 (reply, kOptionRouter));
            Assert::AreEqual (VirtualSubnet::kDnsIp,        DhcpTestPackets::GetOption32 (reply, kOptionDns));
            Assert::AreEqual (DhcpServer::kLeaseSeconds,    DhcpTestPackets::GetOption32 (reply, kOptionLeaseTime));
            Assert::AreEqual (VirtualSubnet::kGatewayIp,    DhcpTestPackets::GetOption32 (reply, kOptionServerId));
        }


        TEST_METHOD (RequestGetsAnAck)
        {
            DhcpServer         server = MakeServer();
            std::vector<Byte>  reply  = server.Process (DhcpTestPackets::MakeRequest (DhcpTestPackets::kRequest, kClientMac, kXid));

            Assert::AreEqual (DhcpTestPackets::kAck,   DhcpTestPackets::GetMessageType (reply));
            Assert::AreEqual (VirtualSubnet::kGuestIp, DhcpTestPackets::GetYourAddress (reply));
        }


        TEST_METHOD (OtherMessagesAndMalformedPacketsGetNoReply)
        {
            constexpr Byte  kInform = 8;

            DhcpServer         server  = MakeServer();
            std::vector<Byte>  inform  = DhcpTestPackets::MakeRequest (kInform, kClientMac, kXid);
            std::vector<Byte>  reply   = DhcpTestPackets::MakeRequest (DhcpTestPackets::kDiscover, kClientMac, kXid);
            std::vector<Byte>  cookie  = DhcpTestPackets::MakeRequest (DhcpTestPackets::kDiscover, kClientMac, kXid);
            std::vector<Byte>  shortly = DhcpTestPackets::MakeRequest (DhcpTestPackets::kDiscover, kClientMac, kXid);

            reply[0]    = 2;      // a BOOTREPLY is not a request
            cookie[236] = 0;      // magic cookie damaged
            shortly.resize (100);

            Assert::IsTrue (server.Process (inform).empty(),  L"DHCPINFORM");
            Assert::IsTrue (server.Process (reply).empty(),   L"BOOTREPLY");
            Assert::IsTrue (server.Process (cookie).empty(),  L"no magic cookie");
            Assert::IsTrue (server.Process (shortly).empty(), L"truncated");
        }
    };
}
