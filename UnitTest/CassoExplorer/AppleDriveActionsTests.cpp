#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "CassoLink/LinkProgram.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDriveActionsTests
//
//  Casso Explorer's verbs on a linked Apple's drive, through the link to
//  FakeApple: copy out, put in, delete, rename, new folder, a copy from an
//  image, and a drag out. Each write is checked on the Apple's own disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleDriveActionsTests)
{
public:

    struct Rig
    {
        InMemoryFileSystem                     fs;
        FakeDiskFileIo                         io;
        FakeApple                              apple;
        FakeAppleFactory                       factory { apple };
        InMemoryCassoLinkFolder                folder;
        std::unique_ptr<CassoLinkConnections>  links;
        std::unique_ptr<CassoExplorerBrowser>  browser;
        std::unique_ptr<CassoExplorerActions>  actions;

        Rig()
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

        ~Rig()
        {
            actions.reset();
            browser.reset();
            links.reset();
        }

        //  Opens a drive and waits for its catalog.
        void  Open (int drive)
        {
            browser->NavigateToLocation (Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, drive), std::string()));
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

                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }
            while (std::chrono::steady_clock::now() < until);

            Assert::Fail (L"the catalog arrives");
        }

        //  Waits for the queued operations, runs their completions, and
        //  lists the drive again.
        std::vector<std::wstring>  Finish()
        {
            std::vector<std::wstring>  messages;
            auto                       until = std::chrono::steady_clock::now() + std::chrono::seconds (20);

            while (!links->GetPendingJobs().empty() && std::chrono::steady_clock::now() < until)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (10));
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
                    browser->SetSelectedRows ({ (int) i });
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
    };

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
        std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  next;

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));
        AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse ("HELLO"), Binary (Dos33Volume::kTypeBinary, 900, 1), next));

        return next;
    }

    static std::vector<Byte>  BuildProDos()
    {
        std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

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



    //  A drive visited again within moments lists what was read rather than
    //  reading the disk again; a refresh reads it.
    TEST_METHOD (ADriveVisitedAgainSoon_ListsWhatWasRead_UntilRefreshed)
    {
        Rig   rig;
        int   reads = 0;
        auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (20);



        rig.Open (1);
        rig.Open (2);
        std::this_thread::sleep_for (std::chrono::milliseconds (500));
        reads = rig.apple.catalogReads;

        //  A read would start on the session's thread, so it is given time.
        rig.Open (1);
        std::this_thread::sleep_for (std::chrono::milliseconds (500));
        AssertSucceeded (rig.browser->Refresh());
        Assert::AreEqual (reads, (int) rig.apple.catalogReads, L"no catalog read for a drive read moments ago");

        rig.browser->ForgetLinkVisit();
        AssertSucceeded (rig.browser->Refresh());

        while (rig.apple.catalogReads == reads && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
            AssertSucceeded (rig.browser->Refresh());
        }

        Assert::IsTrue (rig.apple.catalogReads > reads, L"a refresh reads the catalog");
    }



    //  Switching back to a tab on a drive, however long after, doesn't read
    //  its disk again.
    TEST_METHOD (ReturningToADrivesTab_ListsWhatWasRead)
    {
        Rig   rig;
        int   reads = 0;



        rig.Open (1);
        rig.browser->OpenInNewTab (Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, 2), std::string()));
        rig.Reload();
        rig.browser->ForgetLinkVisit();
        std::this_thread::sleep_for (std::chrono::milliseconds (500));
        reads = rig.apple.catalogReads;

        Assert::IsTrue (rig.browser->SwitchTab (0));
        std::this_thread::sleep_for (std::chrono::milliseconds (500));
        AssertSucceeded (rig.browser->Refresh());

        Assert::AreEqual (reads, (int) rig.apple.catalogReads, L"no catalog read on returning to the tab");
    }




    TEST_METHOD (CopyOut_WritesTheFileToTheHostFolder)
    {
        Rig                     rig;
        std::vector<FileEntry>  entries;
        FilePayload             expected;
        std::string             written;
        bool                    converted = false;

        rig.Open (1);
        rig.Select (L"HELLO");
        rig.browser->GetSelectedEntries (entries);

        AssertSucceeded (rig.actions->GetSelected (L"C:\\Out", HostFileNaming::Style::Descriptive).hr);
        Assert::IsTrue (rig.Finish().empty());

        AssertSucceeded (Dos33Volume (rig.Drive (1)).Read (FilePath::Parse ("HELLO"), expected));
        AssertSucceeded (rig.fs.ReadAllText (L"C:\\Out\\" + DragPayload::GetHostName (entries[0], VolumeKind::Dos33, HostFileNaming::Style::Descriptive, converted), written));
        Assert::IsTrue (std::vector<Byte> (written.begin(), written.end()) == expected.bytes);
    }




    TEST_METHOD (Put_WritesTheFileOnTheDrive)
    {
        Rig          rig;
        FilePayload  stored;

        AssertSucceeded (rig.fs.WriteAllText (L"C:\\In\\NOTES.txt", "THIS IS A NOTE\r\n"));

        rig.Open (1);
        AssertSucceeded (rig.actions->PutFiles ({ L"C:\\In\\NOTES.txt" }).hr);
        Assert::IsTrue (rig.Finish().empty());

        AssertSucceeded (Dos33Volume (rig.Drive (1)).Read (FilePath::Parse ("NOTES"), stored));
        Assert::AreEqual<int> (Dos33Volume::kTypeText, stored.type);
        Assert::IsTrue (std::any_of (rig.browser->GetRows().begin(), rig.browser->GetRows().end(), [] (const CatalogRow & row) { return row.name == L"NOTES"; }),
                        L"the list has it once the write is done");
    }




    TEST_METHOD (DeleteAndRename_ChangeTheDrive)
    {
        Rig  rig;

        rig.Open (1);
        rig.Select (L"HELLO");
        AssertSucceeded (rig.actions->RenameSelected (L"GREETING").hr);
        Assert::IsTrue (rig.Finish().empty());
        Assert::IsTrue (HasEntry (Dos33Volume (rig.Drive (1)), "GREETING"));

        rig.Select (L"GREETING");
        AssertSucceeded (rig.actions->DeleteSelected().hr);
        Assert::IsTrue (rig.Finish().empty());
        Assert::IsFalse (HasEntry (Dos33Volume (rig.Drive (1)), "GREETING"));
    }




    //  A drive's file has a path, so Properties opens for it, whatever root
    //  was listed before the drive.
    TEST_METHOD (ADrivesFile_HasARowPath)
    {
        Rig           rig;
        std::wstring  path;

        rig.browser->NavigateToLocation (Location::MakeRoot (TreeModel::kCassoLinkRootId));
        rig.Open (1);

        for (int row = 0; row < (int) rig.browser->GetRows().size(); row++)
        {
            if (rig.browser->GetRows()[(size_t) row].name == L"HELLO")
            {
                Assert::IsTrue (rig.browser->TryGetRowPath (row, path));
                Assert::IsTrue (path.ends_with (L"HELLO"), path.c_str());
                return;
            }
        }

        Assert::Fail (L"HELLO is listed");
    }




    TEST_METHOD (LockAndConvert_ChangeTheDrive)
    {
        Rig          rig;
        FilePayload  listing;
        std::string  name;
        FileEntry    hello;

        rig.Open (1);
        rig.Select (L"HELLO");
        AssertSucceeded (rig.actions->SetLockedSelected (true).hr);
        Assert::IsTrue (rig.Finish().empty());
        Assert::IsTrue (FindEntry (Dos33Volume (rig.Drive (1)), "HELLO", hello) && hello.isLocked);

        //  A listing put on the drive is tokenized into a program, which
        //  converts to a text listing beside it.
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\In\\PROG.txt", "10 PRINT 1\r\n"));
        AssertSucceeded (rig.actions->PutFiles ({ L"C:\\In\\PROG.txt" }).hr);
        Assert::IsTrue (rig.Finish().empty());

        rig.Reload();
        rig.Select (L"PROG");
        AssertSucceeded (rig.actions->ConvertSelected (name).hr);
        Assert::IsTrue (rig.Finish().empty(), L"the read");
        Assert::IsTrue (rig.Finish().empty(), L"the write it queues");
        Assert::AreEqual (std::string ("PROG.LIST"), name);
        AssertSucceeded (Dos33Volume (rig.Drive (1)).Read (FilePath::Parse (name), listing));
        Assert::AreEqual<int> (Dos33Volume::kTypeText, listing.type);
    }




    TEST_METHOD (NewFolderAndAFolderPut_OnProDos)
    {
        Rig            rig;
        VolumeListing  inside;

        AssertSucceeded (rig.fs.WriteAllText (L"C:\\In\\Games\\README.txt", "PLAY\r\n"));

        rig.Open (2);
        AssertSucceeded (rig.actions->CreateFolder (L"STUFF").hr);
        AssertSucceeded (rig.actions->PutFiles ({ L"C:\\In\\Games" }).hr);
        Assert::IsTrue (rig.Finish().empty());

        Assert::IsTrue (HasEntry (ProDosVolume (rig.Drive (2)), "STUFF"));
        Assert::IsTrue (HasEntry (ProDosVolume (rig.Drive (2)), "GAMES"));

        {
            std::vector<Byte>  disk = rig.Drive (2);

            AssertSucceeded (ProDosVolume (disk).EnumerateDirectory (FilePath::Parse ("GAMES"), inside));
        }

        Assert::AreEqual ((size_t) 1, inside.entries.size());
        Assert::AreEqual (std::string ("README"), inside.entries[0].name);
    }




    TEST_METHOD (APutOnAWriteProtectedDisk_ReportsWhy)
    {
        Rig                        rig;
        std::vector<std::wstring>  messages;

        AssertSucceeded (rig.fs.WriteAllText (L"C:\\In\\NOTES.txt", "NOTE\r\n"));
        rig.apple.writeProtected[0] = true;

        rig.Open (1);
        AssertSucceeded (rig.actions->PutFiles ({ L"C:\\In\\NOTES.txt" }).hr);
        messages = rig.Finish();

        Assert::AreEqual ((size_t) 1, messages.size());
        Assert::IsTrue (messages[0].find (L"write-protected") != std::wstring::npos);
    }




    TEST_METHOD (CopyFromAnImage_WritesTheStoredFile)
    {
        Rig          rig;
        FilePayload  source;
        FilePayload  copied;

        rig.io.files["C:\\Disks\\source.dsk"] = BuildDos33();
        AssertSucceeded (Dos33Volume (rig.io.files["C:\\Disks\\source.dsk"]).Read (FilePath::Parse ("HELLO"), source));

        rig.Open (2);
        AssertSucceeded (rig.actions->CopyEntriesToAppleDrive ("C:\\Disks\\source.dsk", VolumeKind::Dos33, { "HELLO" }).hr);
        Assert::IsTrue (rig.Finish().empty());

        AssertSucceeded (ProDosVolume (rig.Drive (2)).Read (FilePath::Parse ("HELLO"), copied));
        Assert::IsTrue (copied.bytes == source.bytes);
        Assert::AreEqual<int> (ProDosVolume::kTypeBinary, copied.type, L"a DOS 3.3 binary is a ProDOS BIN");
    }




    TEST_METHOD (DragOut_ReadsEachFileOverTheLink)
    {
        Rig                                      rig;
        std::vector<DxuiDragDropSource::Format>  formats;
        std::vector<uint8_t>                     bytes;
        FilePayload                              expected;
        CLIPFORMAT                               contents = (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS);

        rig.Open (1);
        rig.Select (L"HELLO");

        formats = CassoExplorerDragOut::BuildFormats (*rig.browser, HostFileNaming::Style::Descriptive);

        for (const DxuiDragDropSource::Format & format : formats)
        {
            if (format.format == contents)
            {
                AssertSucceeded (format.render (0, bytes));
            }
        }

        AssertSucceeded (Dos33Volume (rig.Drive (1)).Read (FilePath::Parse ("HELLO"), expected));
        Assert::IsTrue (std::vector<Byte> (bytes.begin(), bytes.end()) == expected.bytes);
    }




    TEST_METHOD (StartFullSend_WritesTheImages_AndShowsTheResult)
    {
        Rig  rig;

        rig.Open (1);
        rig.apple.keys.push_back (CassoLinkCodec::kKeyEscape);

        AssertSucceeded (rig.actions->StartFullSend ("Test", L"C:\\Box", L".dsk").hr);

        std::vector<std::wstring>  messages = rig.Finish();

        Assert::AreEqual ((size_t) 1, messages.size());
        Assert::IsTrue (messages[0].find (L"Escape") != std::wstring::npos, messages[0].c_str());
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\full-send.csv"));
    }


    TEST_METHOD (ReadDiskToImage_WritesTheDrivesDisk)
    {
        Rig   rig;
        bool  offered = false;

        rig.Open (1);
        AssertSucceeded (rig.actions->ReadDiskToImage (L"C:\\Images\\out.dsk", false, [&offered] (const LinkOperation &, const std::string &, int) { offered = true; }).hr);
        Assert::IsTrue (rig.Finish().empty());

        Assert::IsFalse (offered);
        Assert::IsTrue  (rig.io.files["C:\\Images\\out.dsk"] == rig.Drive (1));
    }




    TEST_METHOD (ReadDiskToImage_WithUnreadableSectors_OffersToBlankThem)
    {
        Rig                        rig;
        std::vector<std::wstring>  messages;
        size_t                     offered = 0;
        std::string                link;
        int                        drive   = 0;

        rig.apple.badSectors = { { 4, 2 } };

        rig.Open (1);
        AssertSucceeded (rig.actions->ReadDiskToImage (L"C:\\Images\\out.dsk", false,
                                                       [&] (const LinkOperation & operation, const std::string & readLink, int readDrive)
                                                       {
                                                           offered = operation.unreadable.size();
                                                           link    = readLink;
                                                           drive   = readDrive;
                                                       }).hr);
        messages = rig.Finish();

        Assert::IsTrue   (messages.empty(), L"offered instead of reported");
        Assert::AreEqual ((size_t) 1, offered);
        Assert::AreEqual (1, drive, L"the offer is given the drive that was read");
        Assert::IsTrue   (rig.io.files.find ("C:\\Images\\out.dsk") == rig.io.files.end());

        //  Read again by the link and drive the offer was given, not by the location.
        AssertSucceeded (rig.actions->ReadDiskToImage (link, drive, L"C:\\Images\\out.dsk", true, {}).hr);
        Assert::IsTrue (rig.Finish().empty());
        Assert::IsTrue (rig.io.files.find ("C:\\Images\\out.dsk") != rig.io.files.end(), L"read again with the sector as zeros");
    }




    //  The connections every window shares run a finished job's completion
    //  later; one whose window, and so whose actions, have closed by then
    //  is not run.
    TEST_METHOD (ACompletion_IsDropped_OnceItsActionsAreGone)
    {
        Rig   rig;
        bool  offered = false;

        rig.apple.badSectors = { { 4, 2 } };

        rig.Open (1);
        AssertSucceeded (rig.actions->ReadDiskToImage (L"C:\\Images\\out.dsk", false,
                                                       [&offered] (const LinkOperation &, const std::string &, int) { offered = true; }).hr);
        rig.actions.reset();

        Assert::IsTrue  (rig.Finish().empty());
        Assert::IsFalse (offered, L"the completion of a closed window is not run");
    }




    TEST_METHOD (WriteImageToDisk_ReplacesTheDisk_AndListsItAgain)
    {
        Rig  rig;

        rig.io.files["C:\\Images\\in.dsk"] = BuildProDos();

        rig.Open (1);
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (DOS 3.3 volume 254)"), rig.actions->DescribeAppleDrive());

        AssertSucceeded (rig.actions->WriteImageToDisk (L"C:\\Images\\in.dsk").hr);
        Assert::IsTrue (rig.Finish().empty());

        Assert::IsTrue (rig.Drive (1) == rig.io.files["C:\\Images\\in.dsk"]);
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (/LINKED)"), rig.actions->DescribeAppleDrive());
    }




    TEST_METHOD (DescribeJobs_GivesTheFirstsProgressAndTheQueue)
    {
        auto  first  = std::make_shared<LinkOperation>();
        auto  second = std::make_shared<LinkOperation>();

        first->kind     = LinkOperationKind::ReadDisk;
        first->drive    = 2;
        first->progress = 40;

        Assert::AreEqual (std::wstring(), CassoLinkConnections::DescribeJobs ({}));
        Assert::AreEqual (std::wstring (L"Reading the disk in drive 2: 40%"), CassoLinkConnections::DescribeJobs ({ first }));
        Assert::AreEqual (std::wstring (L"Reading the disk in drive 2: 40%, 1 more queued"), CassoLinkConnections::DescribeJobs ({ first, second }));
    }



    TEST_METHOD (SaveLinkToDisk_WritesTheBootDiskToTheDrive)
    {
        Rig                rig;
        std::vector<Byte>  disk;
        LinkShimOptions    options;

        options.baud = CassoLinkSettings::kDefaultBaud;
        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, disk));

        rig.Open (2);
        AssertSucceeded (rig.actions->SaveLinkToDisk ("Test", 2, L"C:\\Temp").hr);

        //  No catalog to list afterward: the disk has no file system.
        {
            auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (20);

            while (!rig.links->GetPendingJobs().empty() && std::chrono::steady_clock::now() < until)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }
        }

        Assert::IsTrue (rig.links->RunCompletions().empty());

        Assert::IsTrue (rig.Drive (2) == disk, L"the drive is the universal boot disk");
    }




    TEST_METHOD (FormatAppleDrive_WritesANewEmptyDisk_InEitherFormat)
    {
        Rig                             rig;
        DiskOperations::NewDiskRequest  prodos;
        DiskOperations::NewDiskRequest  dos;

        prodos.formatName = "prodos";
        prodos.volumeName = "BLANK";
        dos.formatName    = "dos33";
        dos.volumeName    = "100";

        rig.Open (1);
        AssertSucceeded (rig.actions->FormatAppleDrive (prodos, L"C:\\Temp").hr);
        Assert::IsTrue   (rig.Finish().empty());
        Assert::IsTrue   (rig.browser->GetRows().empty(), L"no files on the new disk");
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (/BLANK)"), rig.actions->DescribeAppleDrive());

        rig.Open (2);
        AssertSucceeded (rig.actions->FormatAppleDrive (dos, L"C:\\Temp").hr);
        Assert::IsTrue   (rig.Finish().empty());
        Assert::IsTrue   (rig.browser->GetRows().empty());
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 2 (DOS 3.3 volume 100)"), rig.actions->DescribeAppleDrive());
    }




    TEST_METHOD (FormatAppleDrive_FormatsADiskWhoseCatalogCannotBeRead)
    {
        Rig                             rig;
        DiskOperations::NewDiskRequest  prodos;
        auto                            until     = std::chrono::steady_clock::now() + std::chrono::seconds (20);
        HRESULT                         refreshed = S_OK;

        prodos.formatName = "prodos";
        prodos.volumeName = "FRESH";

        {
            std::lock_guard<std::mutex>  guard (rig.apple.lock);

            std::fill (rig.apple.drives[0].begin(), rig.apple.drives[0].end(), (Byte) 0);
        }

        rig.browser->NavigateToLocation (Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, 1), std::string()));

        do
        {
            refreshed = rig.browser->Refresh();
            IGNORE_RETURN_VALUE (refreshed, S_OK);
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }
        while (rig.browser->GetListError() == CassoExplorerBrowser::s_kWorkingText && std::chrono::steady_clock::now() < until);

        Assert::IsFalse (rig.browser->IsAppleDriveLocation(), L"no catalog");
        Assert::IsTrue  (rig.browser->IsAppleDriveOpen(), L"but the drive is open, for its whole-disk commands");

        AssertSucceeded (rig.actions->FormatAppleDrive (prodos, L"C:\\Temp").hr);
        Assert::IsTrue   (rig.Finish().empty());
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (/FRESH)"), rig.actions->DescribeAppleDrive());
    }




    TEST_METHOD (FormatAppleDrive_RefusesAnUnformattedDisk)
    {
        Rig                             rig;
        DiskOperations::NewDiskRequest  none;
        std::vector<Byte>               before;
        HRESULT                         result = S_OK;

        none.formatName = "none";

        rig.Open (1);
        before = rig.Drive (1);

        result = rig.actions->FormatAppleDrive (none, L"C:\\Temp").hr;
        Assert::IsTrue (FAILED (result));
        Assert::IsTrue (rig.Drive (1) == before, L"the disk is untouched");
    }




    TEST_METHOD (SaveLinkDiskImage_WritesTheImageOnThePc)
    {
        Rig                rig;
        std::vector<Byte>  disk;
        CassoLinkSettings  serial;
        LinkShimOptions    options;

        serial.baud  = 9600;
        options.baud = 9600;

        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, disk));
        AssertSucceeded (rig.actions->SaveLinkDiskImage (L"C:\\Disks\\link.dsk", serial).hr);

        Assert::IsTrue (rig.io.files["C:\\Disks\\link.dsk"] == disk);
    }



    TEST_METHOD (AWifiModemLinksDisk_DialsThisPc)
    {
        Rig                rig;
        CassoLinkSettings  modem;
        std::vector<Byte>  disk;
        std::vector<Byte>  shim;
        std::string        dial;

        modem.transport = LinkTransport::WifiModem;
        modem.address   = "0.0.0.0:6400";
        dial            = CassoLinkConnections::MakeDialCommand (L"relmer-pc", modem);

        Assert::AreEqual (std::string ("ATDT relmer-pc.local:6400"), dial);
        Assert::AreEqual (std::string(), CassoLinkConnections::MakeDialCommand (L"relmer-pc", CassoLinkSettings()), L"a serial link dials nothing");

        AssertSucceeded (LinkProgram::BuildShim (115200, shim, dial));
        Assert::AreEqual (dial, std::string (reinterpret_cast<const char *> (shim.data() + LinkProgram::kDialOffset)), L"the command, ending in a zero byte");
    }



    TEST_METHOD (CreateImageFromAppleDrive_NamesItForTheVolume_AndMakesTheNameFree)
    {
        Rig  rig;

        rig.Open (1);
        rig.io.files["C:\\Out\\DOS 3.3 volume 254.dsk"] = std::vector<Byte> (10, 0);

        AssertSucceeded (rig.actions->CreateImageFromAppleDrive ("Test", 1, L"C:\\Out").hr);
        Assert::IsTrue (rig.Finish().empty());

        Assert::IsTrue (rig.io.files["C:\\Out\\DOS 3.3 volume 254 (2).dsk"] == rig.Drive (1), L"a name no file in the folder has");
    }




    TEST_METHOD (MakeDriveImageName_UsesTheVolume)
    {
        VolumeListing  listing;

        Assert::AreEqual (std::wstring (L"Slot 6 Drive 2.dsk"), ActionsAppleDrive::MakeDriveImageName (listing, VolumeKind::Unknown, 6, 2));

        listing.hasVolumeNumber = true;
        listing.volumeNumber    = 254;
        Assert::AreEqual (std::wstring (L"DOS 3.3 volume 254.dsk"), ActionsAppleDrive::MakeDriveImageName (listing, VolumeKind::Dos33, 6, 1));

        listing.hasVolumeName = true;
        listing.volumeName    = "GAMES";
        Assert::AreEqual (std::wstring (L"GAMES.po"), ActionsAppleDrive::MakeDriveImageName (listing, VolumeKind::ProDos, 6, 1));
    }




    TEST_METHOD (CheckLinkDisk_FindsTheLinkBootDisk)
    {
        Rig                rig;
        std::vector<Byte>  boot;
        int                calls    = 0;
        bool               dataDisk = true;
        bool               linkDisk = false;

        AssertSucceeded (LinkProgram::BuildBootDisk (CassoLinkSettings::kDefaultBaud, boot));

        {
            std::lock_guard<std::mutex>  guard (rig.apple.lock);

            rig.apple.drives[1] = boot;
        }

        rig.Open (1);
        AssertSucceeded (rig.actions->CheckLinkDisk ("Test", 1, [&] (bool found, HRESULT result) { AssertSucceeded (result); dataDisk = found; calls++; }));
        AssertSucceeded (rig.actions->CheckLinkDisk ("Test", 2, [&] (bool found, HRESULT result) { AssertSucceeded (result); linkDisk = found; calls++; }));
        Assert::IsTrue (rig.Finish().empty());

        Assert::AreEqual (2, calls);
        Assert::IsFalse (dataDisk, L"a DOS 3.3 disk");
        Assert::IsTrue  (linkDisk, L"the link boot disk");
    }
};