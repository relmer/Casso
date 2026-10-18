#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

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




    TEST_METHOD (AnAdtProClient_ListsTheReason_AndNoDrives)
    {
        Rig                         rig;
        std::vector<Byte>           wire;
        std::vector<LinkTreeEntry>  entries;
        auto                        until  = std::chrono::steady_clock::now() + std::chrono::seconds (10);
        bool                        served = false;

        rig.factory.answer = false;
        AssertSucceeded (rig.links->Start ("Test"));

        while (rig.factory.GetUnansweredEnd() == nullptr && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        //  A Directory request, as ADTPro's client sends for its Dir command.
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kDirectory, std::vector<Byte> { 0 }, wire));
        rig.factory.GetUnansweredEnd()->Write (wire.data(), wire.size());

        while (!served && std::chrono::steady_clock::now() < until)
        {
            entries = rig.links->BuildTreeEntries();
            served  = entries[0].status.find (L"ADTPro") != std::wstring::npos;
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        Assert::IsTrue  (served, L"the reason names ADTPro's client");
        Assert::IsFalse (entries[0].replying);
        Assert::IsTrue  (entries[0].drives.empty());

        while (rig.links->GetActivity().empty() && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }

        Assert::IsFalse  (rig.links->GetActivity().empty(), L"the request is in the activity log");
        Assert::AreEqual (std::wstring (L"Test"), rig.links->GetActivity().back().connection);
        Assert::IsTrue   (rig.links->GetActivity().back().text.starts_with (L"ADTPro: listed the folder"));
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
        Assert::IsTrue   (entries[2].status.find (L"port 2") != std::wstring::npos, L"port 1 is listed with the reason");

        Assert::IsTrue  (rig.links->IsDiscovered ("Casso on port 6600"));
        Assert::IsFalse (rig.links->IsDiscovered ("Test"));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), rig.links->Start ("Casso on port 6700"));
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
                          CassoLinkConnections::DescribeState (LinkState::Disconnected, false, link, L"pc"));

        link.transport = LinkTransport::Emulated;
        link.address   = "127.0.0.1:6502";
        Assert::AreEqual (std::wstring (L"Waiting for My //c on 127.0.0.1:6502.\nBoot a CassoLink disk in that Casso to connect."),
                          CassoLinkConnections::DescribeState (LinkState::WaitingForReady, false, link, L"pc"));

        link.transport = LinkTransport::WifiModem;
        link.address   = "0.0.0.0:6400";
        Assert::AreEqual (std::wstring (L"Waiting for My //c to dial relmer-pc.local:6400.\nBoot that Apple from a CassoLink disk saved from My //c."),
                          CassoLinkConnections::DescribeState (LinkState::Opening, false, link, L"relmer-pc"));

        Assert::AreEqual (std::wstring (L"Click My //c to connect."), CassoLinkConnections::DescribeState (LinkState::Closed, false, link, L"pc"));
        Assert::AreEqual (std::wstring (L"1,200"), CassoLinkConnections::FormatBaud (1200));
    }



    //  A line that closes or goes quiet during an operation is reported as
    //  that, not as the Apple failing the operation.
    TEST_METHOD (AFailure_FromALostLine_GivesTheReason)
    {
        LinkOperation  operation;

        operation.drive  = 2;
        operation.result = HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE);
        Assert::AreEqual (std::wstring (L"Drive 2: the connection to the Apple closed."), CassoLinkConnections::DescribeFailure (operation));

        operation.result = HRESULT_FROM_WIN32 (ERROR_TIMEOUT);
        Assert::AreEqual (std::wstring (L"Drive 2: the Apple stopped answering."), CassoLinkConnections::DescribeFailure (operation));
    }
};