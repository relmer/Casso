#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoLink/FullSend/FullSendBootResults.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendLink.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "CassoLink/FullSend/FullSendProtection.h"
#include "CassoLink/FullSend/FullSendScreen.h"
#include "CassoLink/FullSend/FullSendSession.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "CassoLink/Session/CassoLinkFullSend.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FullSendRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FullSendTestDisks;
using namespace FullSendRig;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestTests
//
//  FullSendSession's manifest and repeats over a scripted Apple: a legacy
//  manifest, the folder and format prefs, prefixes, the end of a session,
//  and images renamed or deleted after they were sent.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendManifestTests)
{
public:

    TEST_METHOD (Session_RenamesALegacyManifest_AndRecognizesItsDisks)
    {
        Rig          rig;
        Image        disk = Other (1);
        std::string  fingerprint;
        std::string  hash;

        AssertSucceeded (FullSendDisk::ComputeFingerprint (disk, fingerprint));
        AssertSucceeded (FullSendDisk::ComputeHash (disk, hash));

        FullSendManifest  manifest;
        FullSendRow       row = Row (7, "0007.dsk");

        row.fingerprint = fingerprint;
        row.hash        = hash;
        row.status      = FullSendManifest::kStatusSent;
        manifest.Add (row);

        rig.io.files["C:\\Box\\full-send.csv"] = manifest.Format();
        rig.io.files["C:\\Box\\0007.dsk"]      = disk;
        rig.link.drives[0] = disk;

        rig.Run();

        Assert::IsTrue (rig.link.diskReads.empty());
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\manifest.fullsend"));
        Assert::IsFalse (rig.io.files.contains ("C:\\Box\\full-send.csv"));
    }

    TEST_METHOD (Manifest_HasAHeaderOfComments_AndParsesPastThem)
    {
        FullSendManifest   manifest;
        FullSendManifest   read;
        std::vector<Byte>  bytes;

        manifest.Add (Row (1, "0001.dsk"));
        bytes = manifest.Format();

        Assert::AreEqual ('#', (char) bytes[0]);
        AssertSucceeded (read.Parse (bytes));
        Assert::AreEqual ((size_t) 1, read.GetRows().size());
    }

    TEST_METHOD (Session_Space_PrefersANewDisk_OverARepeatInTheOtherDrive)
    {
        Rig  rig;

        rig.link.drives[0]    = Other (1);
        rig.link.drives[1]    = Other (2);
        rig.link.beforeKey[1] = [] (ScriptedLink & link) { link.drives[1] = Other (3); };
        rig.link.keys         = { FullSendKey::Space };

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2, 2 }));
    }

    TEST_METHOD (Session_WithOneDrive_UsesItAlone_AndPausesAfterEachDisk)
    {
        Rig  rig;

        rig.link.drives[0]    = Other (1);
        rig.link.present[1]   = false;
        rig.link.beforeKey[1] = [] (ScriptedLink & link) { link.drives[0] = Other (2); };
        rig.link.keys         = { FullSendKey::Space };

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 1 }));
        Assert::AreEqual (2, rig.link.waits);
        Assert::IsFalse (std::find (rig.link.trackDrives.begin(), rig.link.trackDrives.end(), 2) != rig.link.trackDrives.end(), L"drive 2 was never read");
    }

    TEST_METHOD (Session_WithEmptyDrives_PausesAndReportsThemEmpty)
    {
        Rig  rig;

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads.empty());
        Assert::AreEqual (1, rig.link.waits);
        Assert::IsTrue (session->GetScreen().drives[0] == FullSendDriveState::Empty);
        Assert::IsTrue (session->GetScreen().drives[1] == FullSendDriveState::Empty);
    }

    TEST_METHOD (Session_ADiskNotSeated_IsNotSentUntilTwoReadsMatch)
    {
        Rig  rig;

        rig.link.drives[0]        = Other (1);
        rig.link.present[1]       = false;
        rig.link.unseatedReads[0] = 4;

        rig.Run();

        Assert::IsTrue (rig.link.diskReads.empty(), L"three reads, all different");
        Assert::AreEqual (1, rig.link.waits);
    }

    //  What a disk costs beyond its whole-disk read: its check, and the catalog
    //  track read again after it, with no further check before the pause.
    TEST_METHOD (Session_ADosDisk_ReadsTheCheckedTracksOnce_AndNoMoreBeforeThePause)
    {
        Rig  rig;

        rig.link.drives[0]  = Dos ("GAME");
        rig.link.present[1] = false;

        rig.Run();

        Assert::AreEqual ((size_t) 1, rig.link.diskReads.size());
        Assert::AreEqual ((size_t) 3, rig.link.trackDrives.size(), L"tracks 0 and 17, and 17 again after the read");
        Assert::AreEqual ((size_t) 33, rig.link.tracksRead.size(), L"the checked tracks are not read again");
    }

    TEST_METHOD (Session_AProDosDisk_ReadsTrackZeroOnce_AndNoMoreBeforeThePause)
    {
        Rig  rig;

        rig.link.drives[0]  = ProDos ("BOX");
        rig.link.present[1] = false;

        rig.Run();

        Assert::AreEqual ((size_t) 1, rig.link.diskReads.size());
        Assert::AreEqual ((size_t) 2, rig.link.trackDrives.size(), L"track 0, and track 0 again after the read");
    }

    TEST_METHOD (Session_ATrackNotReadWhole_TakesTwoMatchingReads)
    {
        Rig  rig;

        rig.link.drives[0]      = Other (1);
        rig.link.badDisk        = rig.link.drives[0];
        rig.link.wholeTracksBad = true;
        rig.link.present[1]     = false;

        rig.Run();

        Assert::AreEqual ((size_t) 1, rig.link.diskReads.size());
        Assert::AreEqual ((size_t) 7, rig.link.trackDrives.size(), L"tracks 0, 17 and 3 twice, and 17 again after the read");
    }

    TEST_METHOD (Session_SwappedDuringItsOwnRead_TheNewDiskIsSentWithNoKey)
    {
        Rig  rig;

        rig.link.drives[0]        = Other (1);
        rig.link.present[1]       = false;
        rig.link.afterDiskRead[1] = [] (ScriptedLink & link) { link.drives[0] = Other (2); };

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 1 }));
        Assert::AreEqual (1, rig.link.waits);
        Assert::AreEqual (1, session->GetScreen().sent);
    }

    TEST_METHOD (Session_Prefix_BeginsEachImagesName_AndTheManifestsRow)
    {
        Rig  rig;

        rig.prefix          = "Box 3-";
        rig.link.drives[0]  = Dos ("GAME");
        rig.link.drives[1]  = Other (2);

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\Box 3-0001 GAME.dsk"));
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\Box 3-0002.dsk"));
        Assert::AreEqual (std::string ("Box 3-0001 GAME.dsk"), session->GetManifest().GetRows()[0].file);
    }

    TEST_METHOD (Session_PrefixTypedOnTheApple_BeginsEachImagesName)
    {
        Rig  rig;

        rig.link.beginKey   = FullSendKey::Prefix;
        rig.link.keys       = { FullSendKey::Space };
        rig.link.typed      = { (Byte) 0xC2, (Byte) 0xCF, (Byte) 0xD8, (Byte) 0xDA, CassoLinkCodec::kKeyLeft, (Byte) 0xAD, CassoLinkCodec::kKeyReturn };
        rig.link.drives[0]  = Dos ("GAME");
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::AreEqual (std::string ("BOX-"), session->GetPrefix());
        Assert::IsTrue   (rig.io.files.contains ("C:\\Box\\BOX-0001 GAME.dsk"));
        Assert::AreEqual (std::string ("Prefix: _"), rig.link.typedLines[0][1]);
        Assert::AreEqual (std::string ("Prefix: BOX-_"), rig.link.typedLines.back()[1]);
        Assert::AreEqual (std::string ("P: File name prefix: BOX-"), rig.link.pauseLines[0][2]);
    }

    TEST_METHOD (Session_PrefixLine_EscPutsThePrefixBack)
    {
        Rig  rig;

        rig.prefix          = "OLD-";
        rig.link.beginKey   = FullSendKey::Prefix;
        rig.link.keys       = { FullSendKey::Space };
        rig.link.typed      = { CassoLinkCodec::kKeyDelete, (Byte) 0xD8, CassoLinkCodec::kKeyEscape };
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::AreEqual (std::string ("OLD-"), session->GetPrefix());
        Assert::AreEqual (std::string ("Prefix: OLDX_"), rig.link.typedLines.back()[1]);
    }

    TEST_METHOD (Title_Prefix_IsCleaned_AndEmptyAddsNothing)    {
        Assert::AreEqual (std::string ("A_B0001 GAMES"), FullSendDisk::MakeImageTitle (1, "GAMES", "A/B"));
        Assert::AreEqual (std::string ("0001 GAMES"),    FullSendDisk::MakeImageTitle (1, "GAMES", ""));
    }

    TEST_METHOD (Session_EndsOnQAtThePause)
    {
        Rig  rig;

        rig.link.keys = { FullSendKey::Quit };

        auto  session = rig.Run (IFullSendLink::kQuit);

        Assert::IsTrue (session->GetEndReason() == FullSendSession::EndReason::Quit);
    }

    TEST_METHOD (Session_QOrCancelDuringARead_WritesNoFile)
    {
        for (HRESULT end : { IFullSendLink::kQuit, HRESULT_FROM_WIN32 (ERROR_CANCELLED) })
        {
            Rig  rig;

            rig.link.drives[0]    = Other (1);
            rig.link.failDiskRead = end;

            auto  session = rig.Run (end);

            Assert::IsTrue (rig.io.files.empty(), L"no image, no manifest");
            Assert::IsTrue (session->GetEndReason() == (end == IFullSendLink::kQuit ? FullSendSession::EndReason::Quit : FullSendSession::EndReason::Canceled));
        }
    }

    TEST_METHOD (Session_ADroppedLink_LeavesWrittenImagesAndTheManifest)
    {
        Rig  rig;

        rig.link.drives[0]        = Other (1);
        rig.link.drives[1]        = Other (2);
        rig.link.duringDiskRead[1] = [] (ScriptedLink & link) { link.failDiskRead = HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE); };

        auto  session = rig.Run (HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

        Assert::IsTrue (session->GetEndReason() == FullSendSession::EndReason::LinkFailed);
        Assert::IsTrue (rig.Files() == std::vector<std::string> ({ "0001.dsk", "manifest.fullsend" }));
    }




    //  Images, duplicates and bad sectors.

    //  A row whose fingerprint is from another way of taking it, and whose
    //  image has the disk's hash, as after a change to the fingerprint's tracks.
    TEST_METHOD (Session_ADuplicateImage_IsWritten_WithARowAndTone)
    {
        Rig               rig;
        Image             disk = Other (1);
        FullSendManifest  manifest;
        FullSendRow       earlier = Row (1, "0001.dsk");

        AssertSucceeded (FullSendDisk::ComputeHash (disk, earlier.hash));

        earlier.status = FullSendManifest::kStatusSent;
        manifest.Add (earlier);

        rig.io.files["C:\\Box\\manifest.fullsend"] = manifest.Format();
        rig.io.files["C:\\Box\\0001.dsk"]          = disk;
        rig.link.drives[0]  = disk;
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::IsTrue   (rig.Files() == std::vector<std::string> ({ "0001.dsk", "0002.dsk", "manifest.fullsend" }));
        Assert::AreEqual (std::string ("D1: Sent 0002, dupe of 0001.dsk"), session->GetScreen().GetRows()[3]);

        const auto  & rows = session->GetManifest().GetRows();

        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::AreEqual (2, rows[1].sequence, L"a duplicate has a sequence number");
        Assert::AreEqual (std::string ("duplicate"), rows[1].status);
        Assert::AreEqual (std::string ("0001.dsk"), rows[1].duplicateOf);
        Assert::AreEqual (std::string ("0002.dsk"), rows[1].file);
        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & s) { return s.tone == FullSendTone::Duplicate; }));
    }

    TEST_METHOD (Session_ADiskWithBadSectors_IsFlagged_AndTheSessionContinues)
    {
        Rig    rig;
        Image  bad = Other (1);

        rig.link.drives[0]  = bad;
        rig.link.drives[1]  = Other (2);
        rig.link.badDisk    = bad;
        rig.link.badSectors = { { 3, 4 }, { 3, 5 } };

        auto  session = rig.Run();

        const auto  & rows = session->GetManifest().GetRows();

        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::AreEqual (2, rows[0].unreadable);
        Assert::AreEqual (std::string (FullSendManifest::kStatusUnclear), rows[0].status);
        Assert::AreEqual (std::string ("sent"), rows[1].status);
        Assert::IsTrue (rig.io.files.contains (CassoLinkDiskTransfer::GetUnreadableListPath ("C:\\Box\\0001.dsk")));
        Assert::AreEqual (1, session->GetScreen().bad);
        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & s) { return s.tone == FullSendTone::BadSectors; }));
        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & s) { return s.tone == FullSendTone::Done; }));
    }

    TEST_METHOD (Session_ADiskChangedDuringItsRead_LeavesNoFile_AndContinues)
    {
        Rig    rig;
        Image  next = Other (2);

        rig.link.drives[0]        = Other (1);
        rig.link.present[1]       = false;
        rig.link.afterDiskRead[1] = [&] (ScriptedLink & link) { link.drives[0] = next; };
        rig.link.beforeKey[1]     = [] (ScriptedLink &) {};

        auto  session = rig.Run();

        //  The first read's image is gone, and the disk now in the drive is
        //  new, so it is read and written.
        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 1 }));
        Assert::IsTrue (rig.Files() == std::vector<std::string> ({ "0001.dsk", "manifest.fullsend" }));
        Assert::IsTrue (rig.io.files["C:\\Box\\0001.dsk"] == next);
        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size());
        Assert::AreEqual ((size_t) 2, rig.link.reports.size(), L"the changed disk is reported");
    }

    //  A check's two reads of a track are not repeated in the disk's read, unless
    //  the track had an unreadable sector, whose listing comes from the disk's read.
    TEST_METHOD (Session_ReadsATrackWholeInACheck_NoMoreInTheDisksRead)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        rig.Run();

        Assert::AreEqual ((size_t) 32, rig.link.tracksRead.size(), L"tracks 0, 17 and 3 were read in the check");
        Assert::IsTrue (std::find (rig.link.tracksRead.begin(), rig.link.tracksRead.end(), 0) == rig.link.tracksRead.end());
        Assert::IsTrue (std::find (rig.link.tracksRead.begin(), rig.link.tracksRead.end(), 17) == rig.link.tracksRead.end());

        Rig  damaged;

        damaged.link.drives[0]    = Other (1);
        damaged.link.badDisk      = Other (1);
        damaged.link.wholeTracksBad = true;
        damaged.link.present[1]   = false;

        damaged.Run();

        Assert::AreEqual ((size_t) 35, damaged.link.tracksRead.size());
    }




    TEST_METHOD (Prefs_RoundTripTheFolderAndFormat_AndDropAnUnknownFormat)
    {
        CassoExplorerPrefs  written;
        CassoExplorerPrefs  read;
        CassoExplorerPrefs  bad;

        written.fullSendFolder    = L"D:\\Box";
        written.fullSendExtension = ".woz";
        AssertSucceeded (read.FromJson (written.ToJson()));

        Assert::AreEqual (std::wstring (L"D:\\Box"), read.fullSendFolder);
        Assert::AreEqual (std::string (".woz"), read.fullSendExtension);

        written.fullSendExtension = ".exe";
        AssertSucceeded (bad.FromJson (written.ToJson()));
        Assert::IsTrue (bad.fullSendExtension.empty());
    }




    TEST_METHOD (Session_UsesTheChosenFormat)
    {
        Rig  rig;

        rig.link.drives[0]  = ProDos ("GAMES");
        rig.link.present[1] = false;
        rig.extension       = ".dsk";

        rig.Run();

        Assert::IsTrue (rig.Files() == std::vector<std::string> ({ "0001 GAMES.dsk", "manifest.fullsend" }));
    }

    TEST_METHOD (Session_WritesTheManifestAfterEachDisk_NotAtTheEnd)
    {
        Rig     rig;
        size_t  atSecond = 0;

        rig.link.drives[0]         = Other (1);
        rig.link.drives[1]         = Other (2);
        rig.link.duringDiskRead[2] = [&] (ScriptedLink &) { atSecond = rig.io.files["C:\\Box\\manifest.fullsend"].size(); };

        rig.Run();

        Assert::IsTrue (atSecond > 0, L"the first disk's row was on the PC before the second disk's read");
    }




    //  A folder with a manifest.

    TEST_METHOD (Session_OnAFolderWithAManifest_TreatsItsDisksAsSent_AndContinuesItsNumbers)
    {
        Rig          rig;
        std::string  fingerprint;
        std::string  hash;
        Image        old = Other (1);

        AssertSucceeded (FullSendDisk::ComputeFingerprint (old, fingerprint));
        AssertSucceeded (FullSendDisk::ComputeHash (old, hash));

        FullSendManifest  manifest;
        FullSendRow       row = Row (41, "0041.dsk");

        row.fingerprint = fingerprint;
        row.hash        = hash;
        row.status      = FullSendManifest::kStatusSent;
        manifest.Add (row);

        rig.io.files["C:\\Box\\manifest.fullsend"] = manifest.Format();
        rig.io.files["C:\\Box\\0041.dsk"]          = old;
        rig.link.drives[0] = old;
        rig.link.drives[1] = Other (2);

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 2 }), L"the disk already sent is not read with no key");
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0042.dsk"));
        Assert::AreEqual ((size_t) 2, session->GetManifest().GetRows().size());
    }

    TEST_METHOD (Session_OnADamagedManifest_EndsBeforeAnyRead_AndLeavesIt)
    {
        Rig    rig;
        Image  damaged = { 'n', 'o', 'p', 'e', '\r', '\n' };

        rig.io.files["C:\\Box\\manifest.fullsend"] = damaged;
        rig.link.drives[0] = Other (1);

        auto  session = rig.Run (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        Assert::IsTrue (session->GetEndReason() == FullSendSession::EndReason::ManifestDamaged);
        Assert::IsTrue (rig.link.trackDrives.empty());
        Assert::IsTrue (rig.io.files["C:\\Box\\manifest.fullsend"] == damaged);
        Assert::IsFalse (session->GetMessage().empty());
    }

    TEST_METHOD (Session_AFileThatCannotBeWritten_EndsTheSessionWithTheReason)
    {
        Rig  rig;

        rig.link.drives[0]        = Other (1);
        rig.link.duringDiskRead[1] = [&] (ScriptedLink &) { rig.io.failNextRename = true; };

        auto  session = rig.Run (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

        Assert::IsTrue (session->GetEndReason() == FullSendSession::EndReason::FileFailed);
    }

    //  A manifest with a sent row for `disk` under each of `files`, in
    //  sequence from 1, written to the folder.
    static void  WriteManifestOf (Rig & rig, const Image & disk, const std::vector<std::string> & files)
    {
        FullSendManifest  manifest;
        int               sequence = 1;

        for (const std::string & file : files)
        {
            FullSendRow  row = Row (sequence++, file);

            AssertSucceeded (FullSendDisk::ComputeFingerprint (disk, row.fingerprint));
            AssertSucceeded (FullSendDisk::ComputeHash (disk, row.hash));

            row.status = FullSendManifest::kStatusSent;
            manifest.Add (row);
        }

        rig.io.files["C:\\Box\\manifest.fullsend"] = manifest.Format();
    }

    TEST_METHOD (Session_ARenamedImage_IsStillARepeat_AndItsRowHasItsNewName)
    {
        Rig  rig;

        WriteManifestOf (rig, Other (1), { "0001.dsk" });
        rig.io.files["C:\\Box\\Games.dsk"] = Other (1);
        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads.empty(), L"a repeat isn't read");
        Assert::AreEqual (std::string ("Games.dsk"), session->GetManifest().GetRows()[0].file);
    }

    TEST_METHOD (Session_ADeletedImage_MakesItsDiskNew)
    {
        Rig  rig;

        WriteManifestOf (rig, Other (1), { "0001.dsk" });
        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1 }));
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0002.dsk"));
    }

    TEST_METHOD (Session_TwoRenamedImagesOfADisk_EachGoToOneRow)
    {
        Rig  rig;

        WriteManifestOf (rig, Other (1), { "0001.dsk", "0002.dsk" });
        rig.io.files["C:\\Box\\A.dsk"] = Other (1);
        rig.io.files["C:\\Box\\B.dsk"] = Other (1);
        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads.empty());
        Assert::AreEqual (std::string ("A.dsk"), session->GetManifest().GetRows()[0].file);
        Assert::AreEqual (std::string ("B.dsk"), session->GetManifest().GetRows()[1].file);
    }

    TEST_METHOD (Session_TwoDeletedImagesOfADisk_MakeItNew)
    {
        Rig  rig;

        WriteManifestOf (rig, Other (1), { "0001.dsk", "0002.dsk" });
        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1 }));
    }

    TEST_METHOD (Session_OneRenamedAndOneDeleted_RenamesOneRow_AndKeepsTheOther)
    {
        Rig  rig;

        WriteManifestOf (rig, Other (1), { "0001.dsk", "0002.dsk" });
        rig.io.files["C:\\Box\\Kept.dsk"] = Other (1);
        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads.empty());
        Assert::AreEqual (std::string ("Kept.dsk"), session->GetManifest().GetRows()[0].file);
        Assert::AreEqual (std::string ("0002.dsk"), session->GetManifest().GetRows()[1].file);
    }

    TEST_METHOD (Session_OneRenamedAndOneInPlace_RenamesOnlyTheMissingRow)
    {
        Rig  rig;

        WriteManifestOf (rig, Other (1), { "0001.dsk", "0002.dsk" });
        rig.io.files["C:\\Box\\0001.dsk"] = Other (1);
        rig.io.files["C:\\Box\\New.dsk"]  = Other (1);
        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads.empty());
        Assert::AreEqual (std::string ("0001.dsk"), session->GetManifest().GetRows()[0].file);
        Assert::AreEqual (std::string ("New.dsk"),  session->GetManifest().GetRows()[1].file);
    }
};
