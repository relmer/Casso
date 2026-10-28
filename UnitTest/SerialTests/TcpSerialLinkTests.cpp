#include "Pch.h"

#include "Devices/Serial/TcpSerialLink.h"
#include "Devices/Serial/TelnetCodec.h"
#include "../NetworkTests/FakeHostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace TcpSerialLinkTestNs
{
    static constexpr uint32_t  kBbsAddress = 0x0A000005;
    static constexpr uint16_t  kBbsPort    = 23;
    static constexpr uint16_t  kListenPort = 1977;
    static constexpr size_t    kPlenty     = 4096;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  TcpSerialLinkTests
    //
    //  The TCP side of the serial attachments, polled as the 6551 polls it:
    //  connecting out through a host name lookup, listening for one
    //  connection at a time, data both ways with and without Telnet, and
    //  each way a connection ends.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TcpSerialLinkTests)
    {
    public:
        static TcpSerialLink::PollResult Poll (TcpSerialLink & link, std::vector<Byte> * outData = nullptr, size_t limit = kPlenty)
        {
            TcpSerialLink::PollResult  result;
            std::vector<Byte>          data;



            link.Poll (limit, data, result);

            if (outData != nullptr)
            {
                *outData = data;
            }

            return result;
        }


        static std::shared_ptr<FakeSocketState> ConnectToBbs (FakeHostNetwork & network, TcpSerialLink & link)
        {
            std::shared_ptr<FakeSocketState>  socket;
            HRESULT                           hr     = S_OK;



            network.SetHostAddress ("bbs.example.org", kBbsAddress);
            hr = link.Connect ("bbs.example.org", kBbsPort);
            Assert::IsTrue (SUCCEEDED (hr));
            Poll (link);

            socket = network.FindConnected ({ kBbsAddress, kBbsPort });
            Assert::IsNotNull (socket.get(), L"the lookup done, the connect starts");

            socket->connect = FakeSocketState::ConnectOutcome::Accepted;
            Assert::IsTrue (Poll (link).isConnected);

            return socket;
        }


        TEST_METHOD (AConnectLooksUpTheHostThenConnects)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            TcpSerialLink::PollResult         result;
            std::shared_ptr<FakeSocketState>  socket;
            HRESULT                           hr     = S_OK;



            network.HoldLookups (true);
            network.SetHostAddress ("bbs.example.org", kBbsAddress);

            hr = link.Connect ("bbs.example.org", kBbsPort);
            Assert::IsTrue (SUCCEEDED (hr));
            Poll (link);
            Assert::AreEqual<size_t> (0, network.GetSockets().size(), L"no socket while the lookup runs");
            Assert::IsTrue (link.GetState() == TcpSerialLink::State::LookingUp);

            network.CompleteLookups();
            result = Poll (link);
            socket = network.FindConnected ({ kBbsAddress, kBbsPort });
            Assert::IsNotNull (socket.get());
            Assert::IsFalse (result.isConnected, L"connecting");

            socket->connect = FakeSocketState::ConnectOutcome::Accepted;
            result = Poll (link);
            Assert::IsTrue (result.isConnected);
            Assert::IsTrue (link.IsConnected());
            Assert::AreEqual (std::string ("bbs.example.org:23"), link.GetPeer());
        }


        TEST_METHOD (ARefusedConnectFails)
        {
            FakeHostNetwork            network;
            TcpSerialLink              link (&network);
            TcpSerialLink::PollResult  result;



            network.SetHostAddress ("bbs.example.org", kBbsAddress);
            link.Connect ("bbs.example.org", kBbsPort);
            Poll (link);

            network.FindConnected ({ kBbsAddress, kBbsPort })->connect = FakeSocketState::ConnectOutcome::Refused;
            result = Poll (link);

            Assert::IsTrue  (result.isFailed);
            Assert::IsFalse (result.isLookupFailure);
            Assert::IsTrue  (link.GetState() == TcpSerialLink::State::Idle);
            Assert::AreEqual<size_t> (0, network.CountLive(), L"the failed socket is closed");
        }


        TEST_METHOD (AnUnknownHostFailsAsALookup)
        {
            FakeHostNetwork            network;
            TcpSerialLink              link (&network);
            TcpSerialLink::PollResult  result;



            link.Connect ("no.such.host", kBbsPort);
            result = Poll (link);

            Assert::IsTrue (result.isFailed);
            Assert::IsTrue (result.isLookupFailure);
        }


        TEST_METHOD (SentBytesGoOutAndAPartialSendIsFinishedLater)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  socket = ConnectToBbs (network, link);
            const Byte                        data[] = { 'h', 'e', 'l', 'l', 'o' };



            socket->sendLimit = 2;
            link.Send (data);
            Poll (link);
            Assert::AreEqual<size_t> (2, socket->sent.size());

            socket->sendLimit = SIZE_MAX;
            Poll (link);
            Assert::IsTrue (std::vector<Byte> (std::begin (data), std::end (data)) == socket->sent);
        }


        TEST_METHOD (ReceivedBytesAreLimitedPerPoll)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  socket = ConnectToBbs (network, link);
            std::vector<Byte>                 data;



            socket->inbound = { 1, 2, 3, 4, 5 };

            Poll (link, &data, 3);
            Assert::IsTrue (std::vector<Byte> { 1, 2, 3 } == data);

            Poll (link, &data, 0);
            Assert::IsTrue (data.empty(), L"no room: nothing is read");

            Poll (link, &data, 10);
            Assert::IsTrue (std::vector<Byte> { 4, 5 } == data);
        }


        TEST_METHOD (BytesPassUnalteredWithoutTelnet)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  socket = ConnectToBbs (network, link);
            std::vector<Byte>                 data;
            const Byte                        sent[] = { 0xFF, '\r', 0 };



            socket->inbound = { 0xFF, 0xFF, '\r', 0 };
            link.Send (sent);
            Poll (link, &data);

            Assert::IsTrue (std::vector<Byte> { 0xFF, 0xFF, '\r', 0 } == data);
            Assert::IsTrue (std::vector<Byte> { 0xFF, '\r', 0 } == socket->sent);
        }


        TEST_METHOD (TelnetDoublesFFAndAnswersNegotiation)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  socket;
            std::vector<Byte>                 data;
            const Byte                        sent[] = { 0xFF };



            link.SetTelnet (true);
            socket = ConnectToBbs (network, link);

            socket->inbound = { 'A', TelnetCodec::kIac, TelnetCodec::kIac, TelnetCodec::kIac, TelnetCodec::kWill, TelnetCodec::kOptionEcho };
            link.Send (sent);
            Poll (link, &data);
            Poll (link);

            Assert::IsTrue (std::vector<Byte> { 'A', 0xFF } == data);
            Assert::IsTrue (std::vector<Byte> { 0xFF, 0xFF, TelnetCodec::kIac, TelnetCodec::kDo, TelnetCodec::kOptionEcho } == socket->sent);
        }


        TEST_METHOD (ARemoteCloseEndsTheConnection)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  socket = ConnectToBbs (network, link);
            TcpSerialLink::PollResult         result;
            std::vector<Byte>                 data;



            socket->inbound    = { 'b', 'y', 'e' };
            socket->inboundEof = true;

            result = Poll (link, &data);
            Assert::IsTrue (std::vector<Byte> { 'b', 'y', 'e' } == data, L"the last bytes arrive before the close");
            Assert::IsFalse (result.isClosed);

            result = Poll (link);
            Assert::IsTrue  (result.isClosed);
            Assert::IsFalse (link.IsConnected());
            Assert::IsTrue  (socket->destroyed);
        }


        TEST_METHOD (AResetConnectionCloses)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  socket = ConnectToBbs (network, link);



            socket->resetByPeer = true;

            Assert::IsTrue  (Poll (link).isClosed);
            Assert::IsFalse (link.IsConnected());
        }


        TEST_METHOD (ListeningAcceptsOneConnectionAtATime)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  listener;
            std::shared_ptr<FakeSocketState>  first;
            std::shared_ptr<FakeSocketState>  second;
            TcpSerialLink::PollResult         result;
            HRESULT                           hr       = S_OK;



            hr = link.Listen (kListenPort);
            Assert::IsTrue (SUCCEEDED (hr));
            listener = network.FindBound (HostSocketKind::Tcp, kListenPort);
            Assert::IsNotNull (listener.get());
            Assert::IsTrue (listener->listening);

            first  = network.OfferConnection (listener, { 0x7F000001, 50000 });
            result = Poll (link);
            Assert::IsTrue (result.isAccepted);
            Assert::IsTrue (link.IsConnected());
            Assert::AreEqual (std::string ("127.0.0.1:50000"), link.GetPeer());

            second = network.OfferConnection (listener, { 0x7F000001, 50001 });
            result = Poll (link);
            Assert::IsFalse (result.isAccepted);
            Assert::IsTrue  (second->destroyed, L"a second client is closed at once");
            Assert::IsFalse (first->destroyed);

            link.Close();
            Assert::IsTrue  (first->destroyed);
            Assert::IsFalse (listener->destroyed, L"the port is listening after the connection closes");
        }


        TEST_METHOD (AListenPortInUseFails)
        {
            FakeHostNetwork  network;
            TcpSerialLink    link (&network);
            HRESULT          hr = S_OK;



            network.OccupyPort (HostSocketKind::Tcp, kListenPort);
            hr = link.Listen (kListenPort);

            Assert::AreEqual (HRESULT_FROM_WIN32 (WSAEADDRINUSE), hr);
        }


        TEST_METHOD (AConnectionArrivingWhileConnectingOutIsClosed)
        {
            FakeHostNetwork                   network;
            TcpSerialLink                     link (&network);
            std::shared_ptr<FakeSocketState>  incoming;



            link.Listen (kListenPort);
            network.SetHostAddress ("bbs.example.org", kBbsAddress);
            link.Connect ("bbs.example.org", kBbsPort);
            Poll (link);

            incoming = network.OfferConnection (network.FindBound (HostSocketKind::Tcp, kListenPort), { 0x7F000001, 50000 });

            Assert::IsFalse (Poll (link).isAccepted);
            Assert::IsTrue  (incoming->destroyed);
        }


        TEST_METHOD (WithNoNetworkEverythingFails)
        {
            TcpSerialLink  link (nullptr);
            HRESULT        connected = S_OK;
            HRESULT        listened  = S_OK;



            connected = link.Connect ("bbs.example.org", kBbsPort);
            listened  = link.Listen (kListenPort);

            Assert::IsTrue (FAILED (connected));
            Assert::IsTrue (FAILED (listened));
        }
    };
}
