#pragma once

#include "Pch.h"

#include "../EmuTests/FakeDiskFileIo.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDriveRig
//
//  A CassoExplorerActions over a browser, links to a FakeApple with a DOS 3.3
//  disk in drive 1 and a ProDOS disk in drive 2, and an in-memory file
//  system. Open, Finish and Drive drive it and read the Apple's disks back.
//

struct AppleDriveRig
{
    InMemoryFileSystem                     fs;
    FakeDiskFileIo                         io;
    FakeApple                              apple;
    FakeAppleFactory                       factory { apple };
    InMemoryCassoLinkFolder                folder;
    std::unique_ptr<CassoLinkConnections>  links;
    std::unique_ptr<CassoExplorerBrowser>  browser;
    std::unique_ptr<CassoExplorerActions>  actions;

    AppleDriveRig()
    {
        CassoLinkSettings  settings;

        apple.drives[0] = BuildDos33();
        apple.drives[1] = BuildProDos();

        settings.name      = "Test";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        links   = std::make_unique<CassoLinkConnections> (factory, folder, io);
        browser = std::make_unique<CassoExplorerBrowser> (fs, io);
        actions = std::make_unique<CassoExplorerActions> (*browser, fs);

        links->SetLinks ({ settings });
        browser->SetLinks (links.get());
    }

    ~AppleDriveRig()
    {
        actions.reset();
        browser.reset();
        links.reset();
    }

    //  Opens a drive and waits for its catalog.
    void  Open (int drive)
    {
        browser->GetNavigator().NavigateToLocation (Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, drive), std::string()));
        Reload();
    }

    //  Refreshes until the list has the drive's catalog as read now.
    void  Reload()
    {
        auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (20);

        do
        {
            AssertSucceeded (browser->Refresh());

            if (browser->GetListError().empty() && !browser->IsListingKept())
            {
                return;
            }

            std::this_thread::yield();
        }
        while (std::chrono::steady_clock::now() < until);

        Assert::Fail (L"the catalog arrives");
    }

    //  Waits until no operation is queued or running, so a read that an
    //  action started has finished and been counted.
    void  Settle()
    {
        auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (20);

        while (!links->GetPendingJobs().empty() && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::yield();
        }
    }

    //  Waits for the queued operations, runs their completions, and
    //  lists the drive again.
    std::vector<std::wstring>  Finish()
    {
        std::vector<std::wstring>  messages;
        auto                       until = std::chrono::steady_clock::now() + std::chrono::seconds (20);

        while (!links->GetPendingJobs().empty() && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::yield();
        }

        messages = links->RunCompletions();
        Reload();

        return messages;
    }

    void  Select (const std::wstring & name)
    {
        const std::vector<CatalogRow> &  rows = browser->GetRows();

        for (size_t i = 0; i < rows.size(); i++)
        {
            if (rows[i].name == name)
            {
                browser->GetSelection().SetSelectedRows ({ (int) i });
                return;
            }
        }

        Assert::Fail (L"the row is listed");
    }

    std::vector<Byte>  Drive (int drive)
    {
        std::lock_guard<std::mutex>  guard (apple.lock);

        return apple.drives[(size_t) drive - 1];
    }

    static FilePayload  Binary (Byte type, size_t length, Byte seed)
    {
        FilePayload  payload;

        payload.type           = type;
        payload.loadAddress    = 0x6000;
        payload.hasLoadAddress = true;
        payload.bytes.resize (length);

        for (size_t i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 3);
        }

        return payload;
    }

    static std::vector<Byte>  BuildDos33()
    {
        std::vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);
        std::vector<Byte>  next;

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));
        AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse ("HELLO"), Binary (Dos33Volume::kTypeBinary, 900, 1), next));

        return next;
    }

    static std::vector<Byte>  BuildProDos()
    {
        std::vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, "LINKED"));

        return buffer;
    }

    static bool  HasEntry (const IVolume & volume, const std::string & name)
    {
        VolumeListing  listing;

        AssertSucceeded (volume.Enumerate (listing));

        return std::any_of (listing.entries.begin(), listing.entries.end(), [&name] (const FileEntry & entry) { return entry.name == name; });
    }

    static bool  FindEntry (const IVolume & volume, const std::string & name, FileEntry & outEntry)
    {
        VolumeListing  listing;

        AssertSucceeded (volume.Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == name)
            {
                outEntry = entry;
                return true;
            }
        }

        return false;
    }
};
