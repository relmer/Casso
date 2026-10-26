#include "Pch.h"

#include "HResultAssert.h"

#include "Config/NetworkCardSettings.h"
#include "Core/TextEncoding.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Seams/Win32HostNetwork.h"

#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetSoftwareTests
//
//  Real Apple II network software on an enhanced //e with the Uthernet II in
//  slot 3, over the PC's own network stack: IP65's WGET65 (the W5100's
//  hardware TCP socket), IP65's Telnet65 (raw Ethernet mode, IP65's own TCP)
//  and Contiki's web server (raw Ethernet mode, uIP's TCP, through a
//  port forward). Every server and client on the PC side runs in this test
//  on 127.0.0.1, polled between slices of emulation, so all the traffic is on
//  this PC.
//
//  The disks are third-party releases, not committed. scripts/
//  FetchNetworkDisks.ps1, which `RunTests.ps1 -Scenario` runs first, puts
//  them under Disks/Network/; a case whose disk is missing fails, as the
//  suite's other guest-visible cases do.
//
//  The guest runs at 1.023 MHz of wall time while the network is in use, so
//  its timeouts see real latency; booting and typing run unthrottled.
//
////////////////////////////////////////////////////////////////////////////////

namespace UthernetSoftwareTests
{
    static constexpr uint64_t  kCyclesPerMs       = 1023;
    static constexpr uint64_t  kBootCycles        = 30'000'000;
    static constexpr uint64_t  kSettleCycles      = 3'000'000;
    static constexpr uint64_t  kProgramLoadCycles = 30'000'000;
    static constexpr uint64_t  kMenuLoadCycles    = 80'000'000;
    static constexpr uint64_t  kKeyCycles         = 3'000'000;
    static constexpr DWORD     kKeyGapMs          = 40;
    static constexpr DWORD     kWaitMs            = 30'000;
    static constexpr DWORD     kPollMs            = 100;
    static constexpr int       kMaxMoves          = 12;
    static constexpr Byte      kKeyDown           = 0x0A;
    static constexpr Byte      kKeyReturn         = 0x0D;
    static constexpr Byte      kKeyTab            = 0x09;
    static constexpr uint16_t  kWebPort           = 80;
    static constexpr size_t    kReceiveBuffer     = 4096;

    static constexpr const char *  kpszIp65Disk    = "Disks/Network/ip65.dsk";
    static constexpr const char *  kpszContikiDisk = "Disks/Network/contiki-apple2-3.dsk";


    // A free TCP port on 127.0.0.1, for a port forward's host side.
    static uint16_t FindFreePort (IHostNetwork & network)
    {
        std::unique_ptr<IHostSocket>  socket;
        HostEndpoint                  local;

        AssertSucceeded (network.CreateSocket (HostSocketKind::Tcp, socket));
        AssertSucceeded (socket->Bind ({ VirtualSubnet::kLoopbackIp, 0 }, false));
        AssertSucceeded (socket->GetLocal (local));

        return local.port;
    }


    static void SendAll (IHostSocket & socket, const std::string & text)
    {
        size_t  sent = 0;

        AssertSucceeded (socket.Send (std::span<const Byte> ((const Byte *) text.data(), text.size()), sent));
        Assert::AreEqual (text.size(), sent, L"a short reply fits the send buffer");
    }


    // Appends what has arrived; true once the peer has closed.
    static bool ReceiveSome (IHostSocket & socket, std::string & inOutText)
    {
        std::vector<Byte>  buffer (kReceiveBuffer);
        size_t             count = 0;
        bool               eof   = false;

        AssertSucceeded (socket.Receive (buffer, count, eof));
        inOutText.append ((const char *) buffer.data(), count);

        return eof;
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Guest
    //
    //  The //e, the card in slot 3 on the real host network, and a callback
    //  that polls the test's own sockets between slices of emulation.
    //
    ////////////////////////////////////////////////////////////////////////////

    class Guest
    {
    public:

        explicit Guest (std::vector<PortForward> forwards = {})
            : m_machine ("Apple2eEnhanced", TestMachine::Slots::AsShipped,
                         [this, forwards] (MachineConfig & config, MachineBuildServices & services)
                         {
                             for (SlotConfig & slot : config.slots)
                             {
                                 if (slot.device == "uthernet2")
                                 {
                                     slot.enabled              = true;
                                     slot.network.portForwards = forwards;
                                 }
                             }

                             services.hostNetwork = &m_host;
                         })
        {
            Assert::IsNotNull (m_machine.GetRefs().uthernet, L"the card must be installed");
        }

        Win32HostNetwork  & GetHost() { return m_host; }

        void  SetPoll (std::function<void()> poll) { m_poll = std::move (poll); }

        void  Boot (const std::vector<Byte> & disk)
        {
            GuestSession::Mount (m_machine, disk);
            m_machine.RunCycles (kBootCycles);
        }

        // Runs at 1.023 MHz of wall time, polling the test's sockets.
        void  Run (DWORD ms)
        {
            ULONGLONG  start = GetTickCount64();
            uint64_t   done  = 0;
            uint64_t   due   = 0;

            while (GetTickCount64() - start < ms)
            {
                due = (GetTickCount64() - start) * kCyclesPerMs;

                if (done < due)
                {
                    done += m_machine.RunCycles (due - done);
                }
                else
                {
                    Sleep (1);
                }

                if (m_poll)
                {
                    m_poll();
                }
            }
        }

        void  Press (Byte key)
        {
            AssertSucceeded (KeystrokeInjector::InjectKey (m_machine, key, kKeyCycles), L"the guest reads the key");
            Run (kKeyGapMs);
        }

        // Types the text and Return.
        void  Type (const std::string & text)
        {
            for (char ch : text)
            {
                Press ((Byte) ch);
            }

            Press (kKeyReturn);
        }

        bool  ShowsText (const std::string & text)
        {
            bool  found = false;

            for (const std::string & row : TextScreenScraper::Scrape (m_machine))
            {
                found = found || row.find (text) != std::string::npos;
            }

            return found;
        }

        void  WaitFor (const std::string & text, DWORD ms = kWaitMs)
        {
            ULONGLONG  start = GetTickCount64();

            while (!ShowsText (text) && GetTickCount64() - start < ms)
            {
                Run (kPollMs);
            }

            Assert::IsTrue (ShowsText (text), std::format (L"not on the screen: \"{}\"\n{}", TextEncoding::NarrowToWide (text), Screen()).c_str());
        }

        // Presses `key` until a row contains `marker`, as an inverse-video
        // highlight appears in the scraped text.
        void  MoveTo (Byte key, const std::string & marker)
        {
            int  moves = 0;

            for (moves = 0; moves < kMaxMoves && !ShowsText (marker); moves++)
            {
                Press (key);
                m_machine.RunCycles (kSettleCycles);
            }

            Assert::IsTrue (ShowsText (marker), std::format (L"no highlight on \"{}\"\n{}", TextEncoding::NarrowToWide (marker), Screen()).c_str());
        }

        void  Settle (uint64_t cycles) { m_machine.RunCycles (cycles); }

        std::wstring  Screen()
        {
            std::wstring  text;

            for (const std::string & row : TextScreenScraper::Scrape (m_machine))
            {
                text += TextEncoding::NarrowToWide (row) + L"\n";
            }

            return text;
        }

    private:

        Win32HostNetwork        m_host;
        TestMachine             m_machine;
        std::function<void()>   m_poll;
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Listener
    //
    //  A TCP listener on 127.0.0.1 that takes one connection at a time.
    //
    ////////////////////////////////////////////////////////////////////////////

    class Listener
    {
    public:

        explicit Listener (IHostNetwork & network)
        {
            AssertSucceeded (network.CreateSocket (HostSocketKind::Tcp, m_listener));
            AssertSucceeded (m_listener->Bind ({ VirtualSubnet::kLoopbackIp, 0 }, false));
            AssertSucceeded (m_listener->Listen());
            AssertSucceeded (m_listener->GetLocal (m_local));
        }

        uint16_t  GetPort() const { return m_local.port; }

        // The open connection, accepting one when there is none.
        IHostSocket * GetConnection (bool & outIsNew)
        {
            HostEndpoint  peer;
            bool          blocked = false;

            outIsNew = false;

            if (m_connection == nullptr)
            {
                AssertSucceeded (m_listener->Accept (m_connection, peer, blocked));
                outIsNew = m_connection != nullptr;
            }

            return m_connection.get();
        }

        void  Close()
        {
            HRESULT  hr = S_OK;



            if (m_connection != nullptr)
            {
                hr = m_connection->ShutdownSend();
                IGNORE_RETURN_VALUE (hr, S_OK);
                m_connection.reset();
            }
        }

    private:

        std::unique_ptr<IHostSocket>  m_listener;
        std::unique_ptr<IHostSocket>  m_connection;
        HostEndpoint                  m_local;
    };





    TEST_CLASS (UthernetSoftwareTests)
    {
    public:

        //  The W5100's hardware TCP socket: WGET65 fetches a file from an HTTP
        //  server on this PC through the gateway address.
        TEST_METHOD (Wget65DownloadsAFileThroughAHardwareSocket)
        {
            std::vector<Byte>  disk     = GuestSession::RequireRepoImage (kpszIp65Disk);
            std::string        body     = "HELLO FROM THE PC\r\n";
            Guest              guest;
            Listener           server (guest.GetHost());
            std::string        request;
            bool               answered = false;



            for (int i = 0; i < 40; i++)
            {
                body += "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF\r\n";
            }

            guest.SetPoll ([&]
            {
                bool           isNew = false;
                IHostSocket  * peer  = server.GetConnection (isNew);

                if (peer != nullptr && !answered)
                {
                    ReceiveSome (*peer, request);

                    if (request.find ("\r\n\r\n") != std::string::npos)
                    {
                        SendAll (*peer, std::format ("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: {}\r\n\r\n{}", body.size(), body));
                        server.Close();
                        answered = true;
                    }
                }
            });

            guest.Boot (disk);
            guest.Type ("-WGET65");
            guest.WaitFor ("URL?");
            guest.Type (std::format ("http://10.0.65.1:{}/hello.txt", server.GetPort()));
            guest.WaitFor ("File?");
            guest.Type ("HELLO");
            guest.WaitFor ("Disconnecting - Ok");

            Assert::IsTrue (request.starts_with ("GET /hello.txt HTTP/1.0\r\n"), TextEncoding::NarrowToWide (request).c_str());
            Assert::IsTrue (guest.ShowsText (std::format ("Writing {} bytes - Ok", body.size())), guest.Screen().c_str());
        }


        //  Raw Ethernet mode with IP65's own TCP: a Telnet65 session
        //  with a line-echo server on this PC, which then closes it.
        TEST_METHOD (Telnet65RoundTripWithALocalServer)
        {
            std::vector<Byte>  disk = GuestSession::RequireRepoImage (kpszIp65Disk);
            Guest              guest;
            Listener           server (guest.GetHost());
            std::string        pending;
            std::string        heard;



            guest.SetPoll ([&]
            {
                bool           isNew = false;
                IHostSocket  * peer  = server.GetConnection (isNew);
                size_t         end   = 0;
                std::string    line;

                if (peer == nullptr)
                {
                    return;
                }

                if (isNew)
                {
                    SendAll (*peer, "\r\nWELCOME TO THE CASSO TEST BBS\r\n> ");
                }

                ReceiveSome (*peer, pending);
                end = pending.find_first_of ("\r\n");

                if (end == std::string::npos)
                {
                    return;
                }

                line = pending.substr (0, end);
                pending.erase (0, pending.find_first_not_of ("\r\n", end) == std::string::npos ? pending.size() : pending.find_first_not_of ("\r\n", end));
                std::erase_if (line, [] (char ch) { return (Byte) ch < 0x20 || (Byte) ch > 0x7E; });

                if (line == "BYE")
                {
                    SendAll (*peer, "\r\nGOODBYE\r\n");
                    server.Close();
                }
                else if (!line.empty())
                {
                    heard = line;
                    SendAll (*peer, "\r\nYOU SAID: " + line + "\r\n> ");
                }
            });

            guest.Boot (disk);
            guest.Type ("-TELNET65");
            guest.WaitFor ("Hostname");
            guest.Type ("10.0.65.1");
            guest.WaitFor ("Port Num");
            guest.Type (std::to_string (server.GetPort()));
            guest.WaitFor ("WELCOME TO THE CASSO TEST BBS");
            guest.Type ("HELLO FROM CASSO");
            guest.WaitFor ("YOU SAID: HELLO FROM CASSO");
            guest.Type ("BYE");
            guest.WaitFor ("Disconnected");

            Assert::AreEqual (std::string ("HELLO FROM CASSO"), heard);
        }


        //  Raw Ethernet mode with uIP's TCP: Contiki takes its address from the
        //  virtual network's DHCP server, then a request from this PC, through
        //  a port forward, gets its web server's page.
        TEST_METHOD (ContikiServesItsPageThroughAPortForward)
        {
            std::vector<Byte>  disk     = GuestSession::RequireRepoImage (kpszContikiDisk);
            Win32HostNetwork   probe;
            uint16_t           hostPort = FindFreePort (probe);
            Guest                          guest ({ { PortForwardProtocol::Tcp, hostPort, kWebPort } });
            std::unique_ptr<IHostSocket>   client;
            HostSocketReadiness            readiness;
            std::string                    response;
            bool                           requested = false;
            bool                           closed    = false;



            guest.Boot (disk);

            //  ETHCONFI: the card, then its slot, saved to the disk in
            //  memory.
            guest.MoveTo  (kKeyDown, "`ETHCONFI");
            guest.Press   (kKeyReturn);
            guest.Settle  (kProgramLoadCycles);
            guest.Press   ('2');
            guest.Press   ('3');
            guest.WaitFor ("Saving Config - Done");
            guest.Settle  (kMenuLoadCycles);

            //  IPCONFIG: DHCP, then Save & close.
            guest.MoveTo  (kKeyDown, "`IPCONFIG");
            guest.Press   (kKeyReturn);
            guest.Settle  (kProgramLoadCycles * 2);
            guest.Press   (kKeyReturn);
            guest.WaitFor ("IP address _10.0.65.15");
            guest.MoveTo  (kKeyTab, "[Save`f`close]");
            guest.Press   (kKeyReturn);
            guest.Settle  (kMenuLoadCycles);

            guest.MoveTo  (kKeyDown, "`WEBSERV");
            guest.Press  (kKeyReturn);
            guest.WaitFor ("Contiki up and running");

            AssertSucceeded (guest.GetHost().CreateSocket (HostSocketKind::Tcp, client));
            AssertSucceeded (client->Connect ({ VirtualSubnet::kLoopbackIp, hostPort }));

            guest.SetPoll ([&]
            {
                AssertSucceeded (client->Poll (readiness));
                Assert::IsFalse (readiness.failed, L"the forward accepts the connection");

                if (!requested && readiness.writable)
                {
                    SendAll (*client, "GET / HTTP/1.0\r\n\r\n");
                    requested = true;
                }

                if (requested && !closed)
                {
                    closed = ReceiveSome (*client, response);
                }
            });

            guest.WaitFor ("10.0.65.1: /index.htm");

            for (int i = 0; i < 50 && !closed; i++)
            {
                guest.Run (kPollMs);
            }

            Assert::IsTrue (closed, L"the server closes the connection");
            Assert::IsTrue (response.starts_with ("HTTP/1.0 200 OK"), TextEncoding::NarrowToWide (response.substr (0, 200)).c_str());
            Assert::IsTrue (response.find ("It Worked!") != std::string::npos);
        }
    };
}
