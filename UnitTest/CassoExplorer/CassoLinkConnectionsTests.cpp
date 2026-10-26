#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoLink/CassoLinkSession.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Net/FirewallMonitor.h"
#include "Seams/ILinkAnnouncements.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionsTests
//
//  Casso Explorer's links over FakeApple: a request starts its link and
//  returns at once, the result arrives later, and the tree's entries follow
//  the link's state, including ADTPro's own client on the line.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkConnectionsTests)
{
public:

    struct Rig
    {
        FakeApple                              apple;
        FakeAppleFactory                       factory { apple };
        InMemoryCassoLinkFolder                folder;
        FakeDiskFileIo                         fileIo;
        std::unique_ptr<CassoLinkConnections>  links;

        Rig()
        {
            std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);
            std::vector<Byte>  next;
            FilePayload        payload;
            CassoLinkSettings  settings;

            AssertSucceeded (Dos33Skeleton::Write (buffer, 200));

            payload.type           = Dos33Volume::kTypeBinary;
            payload.loadAddress    = 0x2000;
            payload.hasLoadAddress = true;
            payload.bytes.assign (700, 0x5A);

            AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse ("HELLO"), payload, next));
            apple.drives[0] = next;
            apple.drives[1] = std::vector<Byte> (NibblizationLayer::kImageByteSize, 0);

            settings.name      = "Test";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";

            links = std::make_unique<CassoLinkConnections> (factory, folder, fileIo);
            links->SetLinks ({ settings });
        }

        ~Rig()
        {
            links.reset();
        }

        //  TryGetListing, repeated until the listing arrives.
        bool  WaitForListing (int drive, VolumeListing & outListing, HRESULT & outResult)
        {
            VolumeKind   kind    = VolumeKind::Unknown;
            std::string  message;
            auto         until   = std::chrono::steady_clock::now() + std::chrono::seconds (20);

            while (std::chrono::steady_clock::now() < until)
            {
                if (links->TryGetListing ("Test", drive, std::string(), outListing, kind, outResult, message))
                {
                    return true;
                }

                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }

            return false;
        }
    };




    //  A link any window has as its location is viewed, so another window
    //  going elsewhere does not stop it.
    TEST_METHOD (ALink_IsViewedWhileAnyWindowShowsIt)
    {
        Rig        rig;
        const int  first  = 1;
        const int  second = 2;

        rig.links->SetViewing (&first,  "Apple");
        rig.links->SetViewing (&second, "Apple");
        Assert::IsTrue  (rig.links->IsViewed ("Apple"));

        rig.links->SetViewing (&first, std::string());
        Assert::IsTrue  (rig.links->IsViewed ("Apple"), L"it is the second window's location");

        rig.links->SetViewing (&second, "Other");
        Assert::IsFalse (rig.links->IsViewed ("Apple"));
    }


    TEST_METHOD (AStoppedLink_HasItsStateInPlaceOfDrives)
    {
        Rig                         rig;
        std::vector<LinkTreeEntry>  entries = rig.links->BuildTreeEntries();

        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::IsFalse  (entries[0].replying);
        Assert::IsFalse  (entries[0].status.empty());
        Assert::IsFalse  (rig.links->IsStarted ("Test"));
        Assert::AreEqual (0, rig.factory.connections.load(), L"no port is open until the link is used");
    }




    TEST_METHOD (AListing_StartsTheLink_AndArrivesLater)
    {
        Rig            rig;
        VolumeListing  listing;
        VolumeKind     kind    = VolumeKind::Unknown;
        HRESULT        result  = S_OK;
        std::string    message;

        Assert::IsFalse (rig.links->TryGetListing ("Test", 1, std::string(), listing, kind, result, message), L"the first call returns at once");
        Assert::IsTrue  (rig.links->IsStarted ("Test"));

        Assert::IsTrue  (rig.WaitForListing (1, listing, result));
        AssertSucceeded (result);
        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::AreEqual (std::string ("HELLO"), listing.entries[0].name);
        Assert::IsTrue   (rig.links->TakeChanged(), L"a change was posted");
    }




    TEST_METHOD (TreeEntries_ListTheDrives_WithTheirVolumes)
    {
        Rig                         rig;
        VolumeListing               listing;
        HRESULT                     result = S_OK;
        std::vector<LinkTreeEntry>  entries;

        Assert::IsTrue (rig.WaitForListing (1, listing, result));
        Assert::IsTrue (rig.WaitForListing (2, listing, result));

        entries = rig.links->BuildTreeEntries();

        Assert::IsTrue   (entries[0].replying);
        Assert::AreEqual ((size_t) 2, entries[0].drives.size());
        Assert::AreEqual (std::wstring (L"DOS 3.3 volume 200"), entries[0].drives[0].volume);
        Assert::IsTrue   (entries[0].drives[1].volume.empty(), L"an unformatted disk has no volume");
    }




    //  A connected Apple's drives are read without being opened, and a drive
    //  is listed only once its catalog has arrived; until then the link has
    //  that it is reading them.
    TEST_METHOD (AConnectedApple_HasItsDrivesProbed_AndEachListedOnceRead)
    {
        Rig                         rig;
        std::vector<LinkTreeEntry>  entries;
        auto                        until = std::chrono::steady_clock::now() + std::chrono::seconds (20);

        AssertSucceeded (rig.links->Start ("Test"));

        do
        {
            entries = rig.links->BuildTreeEntries();
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }
        while (!entries[0].replying && std::chrono::steady_clock::now() < until);

        Assert::IsTrue  (entries[0].replying);
        Assert::IsTrue  (entries[0].probing, L"no drive is listed before its catalog is read");
        Assert::IsTrue  (entries[0].drives.empty());
        Assert::IsTrue  (entries[0].status.starts_with (L"Reading the drives on"));

        rig.links->ProbeDrives();

        do
        {
            entries = rig.links->BuildTreeEntries();
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }
        while (entries[0].drives.size() < 2 && std::chrono::steady_clock::now() < until);

        Assert::AreEqual ((size_t) 2, entries[0].drives.size(), L"both drives, read with no listing requested");
        Assert::IsFalse  (entries[0].probing);
        Assert::AreEqual (std::wstring (L"DOS 3.3 volume 200"), entries[0].drives[0].volume);
    }




    TEST_METHOD (Stop_ClosesTheLink_AndInvalidate_ReadsAgain)
    {
        Rig            rig;
        VolumeListing  listing;
        HRESULT        result = S_OK;

        Assert::IsTrue (rig.WaitForListing (1, listing, result));
        Assert::AreEqual (0, rig.apple.sectorsWritten.load());

        rig.links->Invalidate ("Test", 1);
        Assert::IsTrue (rig.WaitForListing (1, listing, result), L"read again after Invalidate");

        rig.links->Stop ("Test");
        Assert::IsFalse (rig.links->IsStarted ("Test"));
        Assert::IsFalse (rig.links->BuildTreeEntries()[0].replying);
    }




    //  ADTPro's client is served by an ADTPro-compatible server, which lists
    //  no drives, and not by any other connection.
    TEST_METHOD (AnAdtProServer_ServesAdtProsClient_AndNoOtherConnectionDoes)
    {
        Rig                         rig;
        CassoLinkSettings           ordinary = rig.links->GetLinks()[0];
        CassoLinkSettings           server;
        std::vector<Byte>           wire;
        std::vector<LinkTreeEntry>  entries;
        auto                        until    = std::chrono::steady_clock::now() + std::chrono::seconds (10);
        bool                        logged   = false;
        int                         opened   = 0;

        server.transport = LinkTransport::AdtProServer;
        server.address   = "127.0.0.1:6502";
        server.name      = CassoLinkSettings::MakeName (server);

        //  A Directory request, as ADTPro's client sends for its Dir command.
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kDirectory, std::vector<Byte> { 0 }, wire));

        rig.factory.answer = false;
        AssertSucceeded (rig.links->Start ("Test"));

        while (rig.factory.GetUnansweredEnd() == nullptr && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        rig.factory.GetUnansweredEnd()->Write (wire.data(), wire.size());
        std::this_thread::sleep_for (std::chrono::milliseconds (500));

        for (const CassoLinkConnections::Activity & activity : rig.links->GetActivity())
        {
            Assert::IsFalse (activity.text.starts_with (L"ADTPro:"), L"an ordinary connection ignores ADTPro's request");
        }

        rig.links->Stop ("Test");
        rig.links->SetLinks ({ ordinary, server });
        opened = rig.factory.connections.load();
        AssertSucceeded (rig.links->Start (server.name));

        while (rig.factory.connections.load() == opened && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        rig.factory.GetUnansweredEnd()->Write (wire.data(), wire.size());

        while (!logged && std::chrono::steady_clock::now() < until)
        {
            for (const CassoLinkConnections::Activity & activity : rig.links->GetActivity())
            {
                logged = logged || (activity.connection == TextEncoding::NarrowToWide (server.name) && activity.text.starts_with (L"ADTPro: listed the folder"));
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        Assert::IsTrue (logged, L"the server's request is in the activity log");

        entries = rig.links->BuildTreeEntries();
        Assert::IsTrue (entries[1].drives.empty(), L"a server lists no drives");
        Assert::IsTrue (entries[1].status.find (L"ADTPro") != std::wstring::npos);
    }



    TEST_METHOD (Connecting_IsInTheActivityLog)
    {
        Rig   rig;
        auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (10);

        AssertSucceeded (rig.links->Start ("Test"));

        while (rig.links->GetActivity().empty() && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        Assert::IsFalse (rig.links->GetActivity().empty());
        Assert::IsTrue  (rig.links->GetActivity().front().text.starts_with (L"Connected to"));
    }



    TEST_METHOD (RunningCassos_AreRoots_UnlessASavedLinkHasTheirAddress)
    {
        Rig                         rig;
        RunningCasso                second;
        RunningCasso                covered;
        RunningCasso                portOne;
        std::vector<LinkTreeEntry>  entries;

        second.pid  = 7;   second.machine  = "Apple //c"; second.serialPort  = 2; second.address  = "127.0.0.1:6600";
        covered.pid = 8;   covered.machine = "Apple //c"; covered.serialPort = 2; covered.address = "127.0.0.1:6502";
        portOne.pid = 9;   portOne.machine = "Apple //c"; portOne.serialPort = 1; portOne.address = "127.0.0.1:6700";

        rig.links->SetRunning ({ second, covered, portOne });
        entries = rig.links->BuildTreeEntries();

        Assert::AreEqual ((size_t) 3, entries.size(), L"the saved link, then two running Cassos; the one at the saved link's address is left out");
        Assert::AreEqual (std::wstring (L"Test"),                    entries[0].name);
        Assert::AreEqual (std::wstring (L"Casso on port 6600"),   entries[1].name);
        Assert::AreEqual (std::wstring (L"Casso on port 6700"),   entries[2].name);
        Assert::IsTrue   (entries[2].status.find (L"port 2") == std::wstring::npos, L"port 1 is usable, as the shim finds its port");

        Assert::IsTrue  (rig.links->IsDiscovered ("Casso on port 6600"));
        Assert::IsFalse (rig.links->IsDiscovered ("Test"));
    }




    //  An ADTPro server at a running Casso's port has that port, so the
    //  Casso is not listed as well: a second connection to the one serial
    //  line would wait for good on it.
    TEST_METHOD (ARunningCasso_AtAnAdtProServersAddress_IsLeftOut)
    {
        Rig                         rig;
        RunningCasso                running;
        CassoLinkSettings           test;
        CassoLinkSettings           server;
        std::vector<LinkTreeEntry>  entries;

        test.name      = "Test";
        test.transport = LinkTransport::Emulated;
        test.address   = "127.0.0.1:6502";

        server.transport = LinkTransport::AdtProServer;
        server.address   = "127.0.0.1:6600";
        server.folder    = L"C:\\ADTPro";
        server.name      = CassoLinkSettings::MakeName (server);

        rig.links->SetLinks ({ test, server });

        running.pid = 7; running.machine = "Apple //e"; running.serialPort = 2; running.address = "127.0.0.1:6600";
        rig.links->SetRunning ({ running });
        entries = rig.links->BuildTreeEntries();

        Assert::IsTrue (std::none_of (entries.begin(), entries.end(), [] (const LinkTreeEntry & entry) { return entry.name == L"Casso on port 6600"; }));
        Assert::AreEqual ((size_t) 2, entries.size(), L"the two saved connections only");
    }




    //  A card in any slot is usable, since the shim finds the slot.
    TEST_METHOD (ARunningCassosCard_IsARootForItsSlot)
    {
        Rig                         rig;
        RunningCasso                card;
        std::vector<LinkTreeEntry>  entries;
        bool                        found = false;

        card.pid = 7;   card.machine = "Apple //e";   card.serialPort = 5;   card.onCard = true;   card.address = "127.0.0.1:6505";

        rig.links->SetRunning ({ card });
        entries = rig.links->BuildTreeEntries();

        Assert::AreEqual ((size_t) 2, entries.size());
        Assert::IsTrue   (entries[1].status.find (L"port 2") == std::wstring::npos, L"no reason for a card");

        for (const CassoLinkSettings & settings : rig.links->GetLinks())
        {
            found = found || settings.name == "Casso on port 6505";
        }

        Assert::IsTrue (found, L"the card's Casso is a root");
    }




    //  The listener's state, as CassoLinkConnections sets it.
    class FakeAnnouncements : public ILinkAnnouncements
    {
    public:
        HRESULT                        SetListening (bool listening) override { opened += (listening && !this->listening) ? 1 : 0; this->listening = listening; return S_OK; }
        std::vector<LinkAnnouncement>  GetAnnounced() const override          { return {}; }
        uint32_t                       GetChangeCount() const override        { return 0; }

        bool  listening = false;
        int   opened    = 0;
    };


    static CassoLinkSettings MakeSearch()
    {
        CassoLinkSettings  search;

        search.transport = LinkTransport::Uthernet;
        search.name      = CassoLinkSettings::MakeName (search);

        return search;
    }


    static LinkAnnouncement MakeAnnouncement()
    {
        LinkAnnouncement  apple;

        apple.address = { 192, 168, 1, 77 };
        apple.port    = 6502;
        apple.mac     = { 0x00, 0x08, 0xDC, 0x01, 0x02, 0x03 };

        return apple;
    }


    //  Links that no announcement serves open no listener, so nothing is
    //  bound beyond the loopback address.
    TEST_METHOD (LinksWithNoSearch_OpenNoListener)
    {
        Rig                 rig;
        FakeAnnouncements   announcements;
        CassoLinkSettings   serial;
        CassoLinkSettings   uthernet;

        serial.transport   = LinkTransport::Serial;
        serial.name        = CassoLinkSettings::MakeName (serial);
        uthernet.transport = LinkTransport::Uthernet;
        uthernet.address   = "192.168.1.50:6502";
        uthernet.name      = CassoLinkSettings::MakeName (uthernet);

        rig.links->SetAnnouncements (&announcements);
        rig.links->SetLinks ({ serial, uthernet });

        Assert::AreEqual (0, announcements.opened, L"an Uthernet II with an address dials out");
    }


    //  An Uthernet II with no address opens the listener, and removing it
    //  closes the listener.
    TEST_METHOD (ASearch_OpensTheListenerWhileItIsSaved)
    {
        Rig                 rig;
        FakeAnnouncements   announcements;

        rig.links->SetAnnouncements (&announcements);
        rig.links->SetLinks ({ MakeSearch() });

        Assert::IsTrue   (announcements.listening);
        Assert::AreEqual (std::string ("Uthernet II (any address)"), MakeSearch().name);

        rig.links->SetLinks ({});
        Assert::IsFalse  (announcements.listening);
    }


    //  A firewall with no rules on a public network, which blocks every port.
    class FakeFirewall : public IHostFirewall
    {
    public:
        HRESULT  Read (FirewallSnapshot & outSnapshot) override { outSnapshot = snapshot; return S_OK; }
        HRESULT  ReadActiveProfiles (FirewallProfile & outActive) override { outActive = snapshot.active; return S_OK; }

        FirewallSnapshot  snapshot;
    };


    static std::vector<LinkTreeEntry> BuildWithFirewall (const std::vector<CassoLinkSettings> & links, FirewallProfile active)
    {
        Rig              rig;
        FakeFirewall     firewall;
        FirewallMonitor  monitor (firewall, nullptr, FirewallProgram());

        firewall.snapshot.active = active;
        monitor.Update();

        rig.links->SetFirewall (&monitor);
        rig.links->SetLinks (links);

        return rig.links->BuildTreeEntries();
    }


    //  The firewall's cause comes first in the entry, and on a public
    //  network the entry offers making the network private.
    TEST_METHOD (ABlockedSearch_HasTheCauseInItsEntry)
    {
        std::vector<LinkTreeEntry>  entries = BuildWithFirewall ({ MakeSearch() }, FirewallProfile::Public);

        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::IsTrue   (entries[0].firewall.find (L"UDP port 6510") != std::wstring::npos, entries[0].firewall.c_str());
        Assert::IsTrue   (entries[0].firewallPublic);
        Assert::IsTrue   (entries[0].status.starts_with (entries[0].firewall));
    }


    TEST_METHOD (ABlockedWifiModem_OnAPrivateNetwork_OffersNoCategoryChange)
    {
        CassoLinkSettings           modem;
        std::vector<LinkTreeEntry>  entries;

        modem.transport = LinkTransport::WifiModem;
        modem.name      = CassoLinkSettings::MakeName (modem);
        entries         = BuildWithFirewall ({ modem }, FirewallProfile::Private);

        Assert::IsTrue  (entries[0].firewall.find (L"TCP port 6400") != std::wstring::npos, entries[0].firewall.c_str());
        Assert::IsFalse (entries[0].firewallPublic);
    }


    //  A connection that dials out or is on a cable is never reported
    //  blocked, whatever the firewall has.
    TEST_METHOD (AConnectionReachedFromNowhere_IsNeverBlocked)
    {
        CassoLinkSettings  serial;
        CassoLinkSettings  uthernet;
        CassoLinkSettings  tcp;

        serial.transport   = LinkTransport::Serial;
        serial.name        = CassoLinkSettings::MakeName (serial);
        uthernet.transport = LinkTransport::Uthernet;
        uthernet.address   = "192.168.1.50:6502";
        uthernet.name      = CassoLinkSettings::MakeName (uthernet);
        tcp.transport      = LinkTransport::Tcp;
        tcp.address        = "192.168.1.5:6502";
        tcp.name           = CassoLinkSettings::MakeName (tcp);

        for (const LinkTreeEntry & entry : BuildWithFirewall ({ serial, uthernet, tcp }, FirewallProfile::Public))
        {
            Assert::IsTrue (entry.firewall.empty(), entry.name.c_str());
        }
    }


    //  A search connects to nothing, so starting it opens no stream.
    TEST_METHOD (ASearch_StartsNoSession)
    {
        Rig  rig;

        rig.links->SetLinks ({ MakeSearch() });

        AssertSucceeded (rig.links->Start (MakeSearch().name));
        Assert::IsFalse (rig.links->IsStarted (MakeSearch().name));
    }


    //  Announcements are listed only while a search is saved.
    TEST_METHOD (AnAnnouncedApple_IsListedOnlyWithASearch)
    {
        Rig  rig;

        rig.links->SetAnnounced ({ MakeAnnouncement() });
        Assert::AreEqual ((size_t) 1, rig.links->BuildTreeEntries().size());
    }


    //  An announcement's Apple is an Uthernet II root at the announcement's
    //  address, never saved.
    TEST_METHOD (AnAnnouncedApple_IsAnUthernetRoot)
    {
        Rig                         rig;
        std::vector<LinkTreeEntry>  entries;
        bool                        found = false;

        rig.links->SetLinks ({ MakeSearch() });
        rig.links->SetAnnounced ({ MakeAnnouncement() });
        entries = rig.links->BuildTreeEntries();

        Assert::AreEqual ((size_t) 2, entries.size());
        Assert::AreEqual (std::wstring (L"Uthernet II at 192.168.1.77:6502"), entries[1].name);
        Assert::IsTrue   (rig.links->IsDiscovered ("Uthernet II at 192.168.1.77:6502"));

        for (const CassoLinkSettings & settings : rig.links->GetLinks())
        {
            found = found || (settings.transport == LinkTransport::Uthernet && settings.address == "192.168.1.77:6502");
        }

        Assert::IsTrue (found);
    }




    TEST_METHOD (ARunningCassoThatExits_StopsItsLink)
    {
        Rig           rig;
        RunningCasso  running;

        running.pid        = 7;
        running.machine    = "Apple //c";
        running.serialPort = 2;
        running.address    = "127.0.0.1:6600";

        rig.links->SetRunning ({ running });
        AssertSucceeded (rig.links->Start ("Casso on port 6600"));
        Assert::IsTrue (rig.links->IsStarted ("Casso on port 6600"));

        rig.links->SetRunning ({});
        Assert::IsFalse (rig.links->IsStarted ("Casso on port 6600"));
        Assert::AreEqual ((size_t) 1, rig.links->GetLinks().size());
    }



    TEST_METHOD (AWifiModemLink_ListsTheAddressToDial)
    {
        CassoLinkSettings  modem;

        modem.transport = LinkTransport::WifiModem;
        modem.address   = "0.0.0.0:6400";

        Assert::AreEqual (std::wstring (L"Dial ATDT relmer-pc.local:6400 from the modem."), CassoLinkConnections::DescribeDial (L"relmer-pc", modem));
    }



    TEST_METHOD (TheWaitingText_NamesTheMachine_AndWhatToDo)
    {
        CassoLinkSettings  link;

        link.name = "My //c";
        link.port = "COM3";

        Assert::AreEqual (std::wstring (L"Searching for My //c on COM3 at 115,200 baud.\nBoot a CassoLink disk on the Apple, or choose Bootstrap the Apple from My //c's menu."),
                          CassoLinkConnections::DescribeState (LinkState::Disconnected, link, L"pc"));

        link.transport = LinkTransport::Emulated;
        link.address   = "127.0.0.1:6502";
        Assert::AreEqual (std::wstring (L"Waiting for My //c on 127.0.0.1:6502.\nBoot a CassoLink disk in that Casso to connect."),
                          CassoLinkConnections::DescribeState (LinkState::WaitingForReady, link, L"pc"));

        link.transport = LinkTransport::WifiModem;
        link.address   = "0.0.0.0:6400";
        Assert::AreEqual (std::wstring (L"Waiting for My //c to dial relmer-pc.local:6400.\nBoot that Apple from a CassoLink disk saved from My //c."),
                          CassoLinkConnections::DescribeState (LinkState::Opening, link, L"relmer-pc"));

        Assert::AreEqual (std::wstring (L"Click My //c to connect."), CassoLinkConnections::DescribeState (LinkState::Closed, link, L"pc"));
        Assert::AreEqual (std::wstring (L"1,200"), CassoLinkConnections::FormatBaud (1200));
    }



    //  Only an Apple on a serial card can be started from the PC, so a
    //  network card's link and an ADTPro server offer no bootstrap.
    TEST_METHOD (OnlyALinkOverASerialCard_CanBootstrap)
    {
        CassoLinkSettings  link;

        for (LinkTransport transport : { LinkTransport::Serial, LinkTransport::Tcp, LinkTransport::Emulated, LinkTransport::WifiModem })
        {
            link.transport = transport;
            Assert::IsTrue (link.CanBootstrap());
        }

        for (LinkTransport transport : { LinkTransport::Uthernet, LinkTransport::AdtProServer })
        {
            link.transport = transport;
            Assert::IsFalse (link.CanBootstrap());
        }
    }



    //  A name made from the settings already ends in the address, so the
    //  waiting text does not say it twice.
    TEST_METHOD (TheWaitingText_SaysTheAddressOnce)
    {
        CassoLinkSettings  link;

        link.transport = LinkTransport::Uthernet;
        link.address   = "127.0.0.1:6502";
        link.name      = CassoLinkSettings::MakeName (link);

        Assert::AreEqual (std::wstring (L"Connecting to Uthernet II at 127.0.0.1:6502.\nBoot that Apple from a CassoLink disk saved from Uthernet II at 127.0.0.1:6502."),
                          CassoLinkConnections::DescribeState (LinkState::Opening, link, L"pc"));

        link.name = "Lab //e";
        Assert::AreEqual (std::wstring (L"Connecting to Lab //e at 127.0.0.1:6502.\nBoot that Apple from a CassoLink disk saved from Lab //e."),
                          CassoLinkConnections::DescribeState (LinkState::Opening, link, L"pc"), L"a name of its own still gets the address");

        link.transport = LinkTransport::Emulated;
        link.name      = "Casso on port 6502";
        Assert::AreEqual (std::wstring (L"Waiting for Casso on port 6502.\nBoot a CassoLink disk in that Casso to connect."),
                          CassoLinkConnections::DescribeState (LinkState::WaitingForReady, link, L"pc"));

        Assert::IsFalse (CassoLinkConnections::NamesItsPort (L"Port 65020", L"127.0.0.1:6502"), L"the digits must stand alone");
    }



    //  A line that closes or goes quiet during an operation is reported as
    //  that, not as the Apple failing the operation.
    //  A link connected again reads its drives again, so a disk changed while
    //  it was gone has its own volume on the drive's row.
    TEST_METHOD (ALinkConnectedAgain_ReadsItsDrivesAgain)
    {
        Rig                         rig;
        std::vector<LinkTreeEntry>  entries;
        std::vector<Byte>           other (NibblizationLayer::kImageByteSize, 0);
        auto                        until = std::chrono::steady_clock::now() + std::chrono::seconds (30);

        auto  volumeOf = [&rig, &entries] () -> std::wstring
        {
            rig.links->ProbeDrives();
            entries = rig.links->BuildTreeEntries();
            return entries[0].drives.empty() ? std::wstring() : entries[0].drives[0].volume;
        };

        AssertSucceeded (rig.links->Start ("Test"));

        while (volumeOf() != L"DOS 3.3 volume 200" && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        Assert::AreEqual (std::wstring (L"DOS 3.3 volume 200"), volumeOf());

        rig.apple.Detach();

        while (entries[0].replying && std::chrono::steady_clock::now() < until)
        {
            volumeOf();
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        Assert::IsFalse (entries[0].replying, L"the line is lost");

        AssertSucceeded (Dos33Skeleton::Write (other, 201));
        rig.apple.drives[0] = other;

        while (volumeOf() != L"DOS 3.3 volume 201" && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        Assert::AreEqual (std::wstring (L"DOS 3.3 volume 201"), volumeOf(), L"the drive read again once connected again");
    }




    TEST_METHOD (AFailure_FromALostLine_GivesTheReason)
    {
        LinkOperation  operation;

        operation.drive  = 2;
        operation.result = HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE);
        Assert::AreEqual (std::wstring (L"Drive 2: the connection to the Apple closed."), CassoLinkConnections::DescribeFailure (operation));

        operation.result = HRESULT_FROM_WIN32 (ERROR_TIMEOUT);
        Assert::AreEqual (std::wstring (L"Drive 2: the Apple stopped answering."), CassoLinkConnections::DescribeFailure (operation));
    }




    TEST_METHOD (ALostLine_IsDescribedInWords)
    {
        Assert::AreEqual (std::string ("the connection closed"), CassoLinkSession::DescribeLineFailure (HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE)));
        Assert::AreEqual (std::string ("no reply"),              CassoLinkSession::DescribeLineFailure (HRESULT_FROM_WIN32 (ERROR_TIMEOUT)));
        Assert::IsTrue   (CassoLinkSession::DescribeLineFailure (E_ACCESSDENIED).starts_with ("the line failed: access is denied"));
    }




    TEST_METHOD (AFailedCopy_SaysWhy)
    {
        Assert::IsTrue (CassoExplorerDragOut::DescribeCopyFailure (CLIPBRD_E_CANT_OPEN).starts_with (L"the clipboard could not be opened."));
        Assert::IsTrue (CassoExplorerDragOut::DescribeCopyFailure (HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE)).starts_with (L"the Apple stopped answering"));
        Assert::AreEqual (std::wstring (L"access is denied."), CassoExplorerDragOut::DescribeCopyFailure (E_ACCESSDENIED));
    }
};