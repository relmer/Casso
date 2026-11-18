#include "Pch.h"

#include "EhmTestHelper.h"
#include "CassoLink/LinkAnnouncement.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/Win32ByteStreamFactory.h"
#include "Seams/Win32LinkAnnouncements.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkWin32SeamTests
//
//  The two Win32 seams of a network link, on this PC's loopback with no
//  emulator: Win32ByteStreamFactory's TCP listen and connect, and
//  Win32LinkAnnouncements receiving an announcement. The unit tests use fakes in place of
//  both.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkWin32SeamTests)
{
public:

    static constexpr uint16_t  kTcpPort      = 26530;
    static constexpr uint16_t  kAnnouncePort = 26531;
    static constexpr DWORD     kReadMs       = 100;
    static constexpr auto      kDeadline     = std::chrono::seconds (5);

    //  Bytes from `stream` until `count` have arrived or the deadline passes.
    static std::vector<Byte>  ReadSome (IByteStream & stream, size_t count)
    {
        std::vector<Byte>  got;
        Byte               buffer[64] = {};
        size_t             read       = 0;
        auto               until      = std::chrono::steady_clock::now() + kDeadline;



        while (got.size() < count && std::chrono::steady_clock::now() < until)
        {
            AssertSucceeded (stream.Read (buffer, sizeof (buffer), kReadMs, read));
            got.insert (got.end(), buffer, buffer + read);
        }

        return got;
    }



    //  A listening stream and a connection to it on 127.0.0.1, with bytes
    //  written at each end read at the other.
    TEST_METHOD (ATcpListenAndConnect_MoveBytesBothWays)
    {
        static const std::vector<Byte>  kToListener  = { 'C', 'A', 'S', 'S', 'O' };
        static const std::vector<Byte>  kToConnector = { 'L', 'I', 'N', 'K' };

        Win32ByteStreamFactory        factory;
        std::unique_ptr<IByteStream>  listener;
        std::unique_ptr<IByteStream>  connector;



        AssertSucceeded (factory.ListenTcp  (kTcpPort, LinkTransport::Emulated, listener));
        AssertSucceeded (factory.ConnectTcp ("127.0.0.1", kTcpPort, connector));

        AssertSucceeded (connector->Write (kToListener.data(), kToListener.size()));
        Assert::IsTrue (ReadSome (*listener, kToListener.size()) == kToListener, L"the connector's bytes at the listener");

        AssertSucceeded (listener->Write (kToConnector.data(), kToConnector.size()));
        Assert::IsTrue (ReadSome (*connector, kToConnector.size()) == kToConnector, L"the listener's bytes at the connector");

        connector->Close();
        listener->Close();
    }



    //  An announcement sent from this PC is received, with its port and MAC
    //  address, and the loopback address as its sender's.
    TEST_METHOD (AnAnnouncementOnThisPc_IsReceived)
    {
        static constexpr uint16_t  kLinkPort = 6502;

        Win32LinkAnnouncements  announcements (kAnnouncePort);
        std::array<Byte, 6>     mac           = { 0x00, 0x08, 0xDC, 0x44, 0x55, 0x66 };
        std::vector<Byte>       datagram      = { 'C', 'A', 'S', 'S', 'O', 'L', 'I', 'N', 'K', LinkAnnouncement::kVersion,
                                                  192, 168, 1, 20, (Byte) (kLinkPort >> 8), (Byte) (kLinkPort & 0xFF) };
        SOCKET                  sender        = INVALID_SOCKET;
        sockaddr_in             to            = {};
        auto                    until         = std::chrono::steady_clock::now() + kDeadline;
        bool                    found         = false;



        datagram.insert (datagram.end(), mac.begin(), mac.end());
        Assert::AreEqual (LinkAnnouncement::kLength, datagram.size());

        AssertSucceeded (announcements.SetListening (true));

        sender = ::socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        Assert::AreNotEqual (INVALID_SOCKET, sender);

        to.sin_family      = AF_INET;
        to.sin_port        = htons (kAnnouncePort);
        to.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

        while (!found && std::chrono::steady_clock::now() < until)
        {
            Assert::AreEqual ((int) datagram.size(), ::sendto (sender, reinterpret_cast<const char *> (datagram.data()), (int) datagram.size(), 0, (const sockaddr *) &to, sizeof (to)));
            std::this_thread::sleep_for (std::chrono::milliseconds (kReadMs));

            for (const LinkAnnouncement & a : announcements.GetAnnounced())
            {
                found = found || (a.mac == mac && a.port == kLinkPort);
            }
        }

        ::closesocket (sender);
        AssertSucceeded (announcements.SetListening (false));

        Assert::IsTrue (found, L"the announcement, with its port and MAC address");
    }
};
