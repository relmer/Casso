#include "Pch.h"

#include "AppleDriveRig.h"
#include "../EhmTestHelper.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "CassoExplorer/Model/CassoLinkShimBuilder.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "CassoLink/LinkProgram.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"

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

    using Rig = AppleDriveRig;



    //  A drive visited again within moments lists what was read rather than
    //  reading the disk again; a refresh reads it.
    TEST_METHOD (ADriveVisitedAgainSoon_ListsWhatWasRead_UntilRefreshed)
    {
        Rig   rig;
        int   reads = 0;
        auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (20);



        rig.Open (1);
        rig.Open (2);
        rig.Settle();
        reads = rig.apple.catalogReads;

        //  A read would run on the session's thread, so it is waited for.
        rig.Open (1);
        rig.Settle();
        AssertSucceeded (rig.browser->Refresh());
        Assert::AreEqual (reads, (int) rig.apple.catalogReads, L"no catalog read for a drive read moments ago");

        rig.browser->GetLoader().ForgetLinkVisit();
        AssertSucceeded (rig.browser->Refresh());

        while (rig.apple.catalogReads == reads && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::yield();
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
        rig.browser->GetNavigator().OpenInNewTab (Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, 2), std::string()));
        rig.Reload();
        rig.browser->GetLoader().ForgetLinkVisit();
        rig.Settle();
        reads = rig.apple.catalogReads;

        Assert::IsTrue (rig.browser->GetNavigator().SwitchTab (0));
        rig.Settle();
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
        rig.browser->GetSelection().GetSelectedEntries (entries);

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
        Assert::IsTrue (Rig::HasEntry (Dos33Volume (rig.Drive (1)), "GREETING"));

        rig.Select (L"GREETING");
        AssertSucceeded (rig.actions->DeleteSelected().hr);
        Assert::IsTrue (rig.Finish().empty());
        Assert::IsFalse (Rig::HasEntry (Dos33Volume (rig.Drive (1)), "GREETING"));
    }




    //  A drive's file has a path, so Properties opens for it, whatever root
    //  was listed before the drive.
    TEST_METHOD (ADrivesFile_HasARowPath)
    {
        Rig           rig;
        std::wstring  path;

        rig.browser->GetNavigator().NavigateToLocation (Location::MakeRoot (TreeModel::kCassoLinkRootId));
        rig.Open (1);

        for (int row = 0; row < (int) rig.browser->GetRows().size(); row++)
        {
            if (rig.browser->GetRows()[(size_t) row].name == L"HELLO")
            {
                Assert::IsTrue (BrowserRowPaths::TryGetRowPath (rig.browser->GetRowListing(), row, path));
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
        Assert::IsTrue (Rig::FindEntry (Dos33Volume (rig.Drive (1)), "HELLO", hello) && hello.isLocked);

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

        Assert::IsTrue (Rig::HasEntry (ProDosVolume (rig.Drive (2)), "STUFF"));
        Assert::IsTrue (Rig::HasEntry (ProDosVolume (rig.Drive (2)), "GAMES"));

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

        rig.io.files["C:\\Disks\\source.dsk"] = Rig::BuildDos33();
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




    TEST_METHOD (DescribeJobs_GivesTheFirstsProgressAndTheQueue)
    {
        auto  first  = std::make_shared<LinkOperation>();
        auto  second = std::make_shared<LinkOperation>();

        first->kind     = LinkOperationKind::ReadDisk;
        first->drive    = 2;
        first->progress = 40;

        Assert::AreEqual (std::wstring(), CassoLinkConnectionText::DescribeJobs ({}));
        Assert::AreEqual (std::wstring (L"Reading the disk in drive 2: 40%"), CassoLinkConnectionText::DescribeJobs ({ first }));
        Assert::AreEqual (std::wstring (L"Reading the disk in drive 2: 40%, 1 more queued"), CassoLinkConnectionText::DescribeJobs ({ first, second }));
    }

    TEST_METHOD (DescribeJobs_ForAFullSend_IsWaitingCheckingOrSending)
    {
        auto  job = std::make_shared<LinkOperation>();

        job->kind     = LinkOperationKind::FullSend;
        job->drive    = 2;
        job->progress = 20;

        Assert::AreEqual (std::wstring (L"Full Send: sending the disk in drive 2: 20%"), CassoLinkConnectionText::DescribeJobs ({ job }));

        job->fullSendChecking = true;
        Assert::AreEqual (std::wstring (L"Full Send: checking the disk in drive 2"), CassoLinkConnectionText::DescribeJobs ({ job }));

        job->fullSendWaiting = true;
        Assert::AreEqual (std::wstring (L"Full Send: waiting for a disk on the Apple"), CassoLinkConnectionText::DescribeJobs ({ job }));
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
        dial            = CassoLinkShimBuilder::MakeDialCommand (L"relmer-pc", modem);

        Assert::AreEqual (std::string ("ATDT relmer-pc.local:6400"), dial);
        Assert::AreEqual (std::string(), CassoLinkShimBuilder::MakeDialCommand (L"relmer-pc", CassoLinkSettings()), L"a serial link dials nothing");

        AssertSucceeded (LinkProgram::BuildShim (115200, shim, dial));
        Assert::AreEqual (dial, std::string (reinterpret_cast<const char *> (shim.data() + LinkProgram::kDialOffset)), L"the command, ending in a zero byte");
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



    //  GetChosenKind returns the kind SetChosenKind set on a running link.
    TEST_METHOD (ChosenKind_OnARunningLink_ReadsBack)
    {
        Rig  rig;



        rig.Open (1);
        Assert::IsTrue (rig.links->GetChosenKind ("Test", 1) == VolumeKind::Unknown, L"no choice yet");

        rig.links->SetChosenKind ("Test", 1, VolumeKind::Cpm);
        Assert::IsTrue (rig.links->GetChosenKind ("Test", 1) == VolumeKind::Cpm);
        Assert::IsTrue (rig.links->GetChosenKind ("Test", 2) == VolumeKind::Unknown, L"the choice was for the other drive");
    }
};
