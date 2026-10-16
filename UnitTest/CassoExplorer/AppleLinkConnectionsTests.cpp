#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../AppleLink/FakeApple.h"
#include "../AppleLink/InMemoryAppleLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "AppleLink/AppleLinkCodec.h"
#include "CassoExplorer/Model/AppleLinkConnections.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnectionsTests
//
//  Casso Explorer's links over FakeApple: a request starts its link and
//  returns at once, the result arrives later, and the tree's entries follow
//  the link's state, including ADTPro's own client on the line.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLinkConnectionsTests)
{
public:

    struct Rig
    {
        FakeApple                              apple;
        FakeAppleFactory                       factory { apple };
        InMemoryAppleLinkFolder                folder;
        FakeDiskFileIo                         fileIo;
        std::unique_ptr<AppleLinkConnections>  links;

        Rig()
        {
            std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);
            std::vector<Byte>  next;
            FilePayload        payload;
            AppleLinkSettings  settings;

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

            links = std::make_unique<AppleLinkConnections> (factory, folder, fileIo);
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
        AssertSucceeded (AppleLinkCodec::Encode (AppleLinkCodec::kDirectory, std::vector<Byte> { 0 }, wire));
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
    }
};
