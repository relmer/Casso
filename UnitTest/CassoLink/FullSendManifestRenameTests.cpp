#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "CassoLink/FullSend/FullSendSession.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestRenameTests
//
//  An image renamed in Casso Explorer is renamed in its folder's manifest.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendManifestRenameTests)
{
public:

    static constexpr const char *  kFolder = "C:\\Sends";

    //  A sent image and a later duplicate of it.
    static FullSendManifest MakeManifest()
    {
        FullSendManifest  manifest;
        FullSendRow       sent;
        FullSendRow       duplicate;

        sent.sequence         = 1;
        sent.file             = "Disk 001.dsk";
        sent.fingerprint      = std::string (64, 'a');
        sent.hash             = std::string (64, 'b');
        sent.time             = "2026-11-08T10:00:00";
        sent.status           = FullSendManifest::kStatusSent;
        duplicate.sequence    = 2;
        duplicate.fingerprint = sent.fingerprint;
        duplicate.hash        = sent.hash;
        duplicate.time        = "2026-11-08T10:05:00";
        duplicate.status      = FullSendManifest::kStatusDuplicate;
        duplicate.duplicateOf = sent.file;

        manifest.Add (sent);
        manifest.Add (duplicate);

        return manifest;
    }


    static std::string ManifestPath()
    {
        return std::string (kFolder) + "\\" + FullSendManifest::kFileName;
    }




    TEST_METHOD (ARenamedImage_IsRenamedInEveryRowThatHasIt)
    {
        FullSendManifest  manifest = MakeManifest();

        Assert::IsTrue   (manifest.RenameImage ("disk 001.DSK", "Archon.dsk"), L"the comparison ignores case");
        Assert::AreEqual (std::string ("Archon.dsk"), manifest.GetRows()[0].file);
        Assert::AreEqual (std::string ("Archon.dsk"), manifest.GetRows()[1].duplicateOf);
        Assert::IsTrue   (manifest.GetRows()[1].file.empty(), L"a duplicate has no image to rename");
        Assert::IsFalse  (manifest.RenameImage ("Other.dsk", "Else.dsk"));
    }


    TEST_METHOD (NoteRename_WritesTheManifestWithTheNewName)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  bytes;
        FullSendManifest   read;

        AssertSucceeded (io.WriteAllBytes (ManifestPath(), MakeManifest().Format()));
        AssertSucceeded (FullSendManifestFile::NoteRename (io, kFolder, "Disk 001.dsk", "Archon.dsk"));
        AssertSucceeded (io.ReadAllBytes (ManifestPath(), bytes));
        AssertSucceeded (read.Parse (bytes));

        Assert::AreEqual (std::string ("Archon.dsk"), read.GetRows()[0].file);
        Assert::AreEqual (std::string ("Archon.dsk"), read.GetRows()[1].duplicateOf);
    }


    //  A boot result is written to the image's row, and the row's status is
    //  returned; for a file with no row, the status is empty.
    TEST_METHOD (NoteBoot_WritesTheResult_AndReturnsTheRowsStatus)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  bytes;
        FullSendManifest   read;
        std::string        status;

        AssertSucceeded (io.WriteAllBytes (ManifestPath(), MakeManifest().Format()));
        AssertSucceeded (FullSendManifestFile::NoteBoot (io, kFolder, "disk 001.DSK", FullSendManifest::kBootBooted, status));
        AssertSucceeded (io.ReadAllBytes (ManifestPath(), bytes));
        AssertSucceeded (read.Parse (bytes));

        Assert::AreEqual (std::string (FullSendManifest::kStatusSent), status);
        Assert::AreEqual (std::string (FullSendManifest::kBootBooted), read.GetRows()[0].boot);

        AssertSucceeded (FullSendManifestFile::NoteBoot (io, kFolder, "Other.dsk", FullSendManifest::kBootBooted, status));
        Assert::IsTrue (status.empty());
    }


    //  A raw capture is renamed in the folder and the manifest, with its
    //  reads; a sector image, a file with no row and a name that exists
    //  aren't.
    TEST_METHOD (RenameCapture_RenamesARawCaptureItsReadsAndItsRow)
    {
        FakeDiskFileIo     io;
        FullSendManifest   manifest = MakeManifest();
        FullSendManifest   read;
        FullSendRow        capture;
        std::vector<Byte>  bytes;
        std::string        folder   = std::string (kFolder) + "\\";
        bool               renamed  = true;

        capture.sequence    = 3;
        capture.file        = "0003.woz";
        capture.fingerprint = std::string (64, 'c');
        capture.hash        = std::string (64, 'd');
        capture.time        = "2026-11-08T10:10:00";
        capture.status      = FullSendManifest::kStatusRawCapture;
        manifest.Add (capture);

        AssertSucceeded (io.WriteAllBytes (ManifestPath(), manifest.Format()));
        AssertSucceeded (io.WriteAllBytes (folder + "0003.woz", { 1, 2, 3 }));
        AssertSucceeded (io.WriteAllBytes (folder + "0003.woz.reads", { 4 }));
        AssertSucceeded (io.WriteAllBytes (folder + "Disk 001.dsk", { 5 }));

        AssertSucceeded (FullSendManifestFile::RenameCapture (io, kFolder, "Disk 001.dsk", "Other.dsk", renamed));
        Assert::IsFalse (renamed, L"A sector image has its name from its catalog");

        AssertSucceeded (FullSendManifestFile::RenameCapture (io, kFolder, "0003.woz", "Disk 001.dsk", renamed));
        Assert::IsFalse (renamed, L"The new name exists");

        AssertSucceeded (FullSendManifestFile::RenameCapture (io, kFolder, "0003.woz", "0003 The Witness.woz", renamed));
        Assert::IsTrue (renamed);
        Assert::IsTrue (io.Exists (folder + "0003 The Witness.woz") && !io.Exists (folder + "0003.woz"));
        Assert::IsTrue (io.Exists (folder + "0003 The Witness.woz.reads") && !io.Exists (folder + "0003.woz.reads"));

        AssertSucceeded (io.ReadAllBytes (ManifestPath(), bytes));
        AssertSucceeded (read.Parse (bytes));
        Assert::AreEqual (std::string ("0003 The Witness.woz"), read.GetRows()[2].file);
    }


    TEST_METHOD (NoteRename_OfAFileTheManifestLacks_LeavesItAsItIs)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  before = MakeManifest().Format();
        std::vector<Byte>  after;

        AssertSucceeded (io.WriteAllBytes (ManifestPath(), before));
        AssertSucceeded (FullSendManifestFile::NoteRename (io, kFolder, "Notes.txt", "Notes2.txt"));
        AssertSucceeded (io.ReadAllBytes (ManifestPath(), after));

        Assert::IsTrue (after == before);
    }


    TEST_METHOD (NoteRename_InAFolderWithNoManifest_WritesNothing)
    {
        FakeDiskFileIo  io;

        AssertSucceeded (FullSendManifestFile::NoteRename (io, kFolder, "Disk 001.dsk", "Archon.dsk"));
        Assert::IsFalse (io.Exists (ManifestPath()));
    }


    //  A row of an older manifest, a duplicate with no file, is
    //  removed with the image it repeats when no copy of that image remains.
    TEST_METHOD (ADeletedImage_RemovesItsRowAndItsDuplicates_AndRestoringPutsThemBack)
    {
        FullSendManifest  manifest = MakeManifest();
        FullSendRow       later;
        FullSendRemoval   removal;

        later.sequence    = 3;
        later.file        = "Disk 003.dsk";
        later.fingerprint = std::string (64, 'c');
        later.hash        = std::string (64, 'd');
        later.status      = FullSendManifest::kStatusSent;
        manifest.Add (later);

        removal = manifest.RemoveImage ("DISK 001.dsk");

        Assert::AreEqual ((size_t) 2, removal.removed.size(), L"the image's row and its duplicate's");
        Assert::IsTrue   (removal.changed.empty());
        Assert::AreEqual ((size_t) 1, manifest.GetRows().size());
        Assert::AreEqual (std::string ("Disk 003.dsk"), manifest.GetRows()[0].file);

        manifest.RestoreRows (removal);
        manifest.RestoreRows (removal);

        Assert::AreEqual ((size_t) 3, manifest.GetRows().size(), L"a row the manifest has already is left out");
        Assert::AreEqual (1, manifest.GetRows()[0].sequence, L"in order of sequence");
        Assert::AreEqual (2, manifest.GetRows()[1].sequence);
        Assert::AreEqual (3, manifest.GetRows()[2].sequence);
    }


    //  The earliest copy of a deleted image takes its place, and the other
    //  rows that repeat it then repeat that copy. Restoring puts every row
    //  back as it was.
    TEST_METHOD (ADeletedImage_ThatHasCopies_HasItsEarliestCopyTakeItsPlace)
    {
        FullSendManifest   manifest;
        FullSendRow        row;
        FullSendRemoval    removal;
        std::vector<Byte>  before;

        row.fingerprint = std::string (64, 'a');
        row.hash        = std::string (64, 'b');

        row.sequence    = 6;
        row.file        = "0006.dsk";
        row.status      = FullSendManifest::kStatusSent;
        manifest.Add (row);

        row.sequence    = 14;
        row.file        = "FS0014.dsk";
        row.duplicateOf = "0006.dsk";
        manifest.Add (row);

        row.sequence    = 15;
        row.file        = "0015.dsk";
        row.status      = FullSendManifest::kStatusDuplicate;
        manifest.Add (row);

        row.sequence    = 16;
        row.file.clear();
        manifest.Add (row);

        before  = manifest.Format();
        removal = manifest.RemoveImage ("0006.dsk");

        Assert::AreEqual ((size_t) 1, removal.removed.size(), L"the deleted image's row alone");
        Assert::AreEqual ((size_t) 3, removal.changed.size());
        Assert::AreEqual ((size_t) 3, manifest.GetRows().size());
        Assert::AreEqual (std::string ("FS0014.dsk"), manifest.GetRows()[0].file);
        Assert::IsTrue   (manifest.GetRows()[0].duplicateOf.empty(), L"the earliest copy repeats nothing now");
        Assert::AreEqual (std::string ("FS0014.dsk"), manifest.GetRows()[1].duplicateOf, L"and the later copy repeats it");
        Assert::AreEqual (std::string ("FS0014.dsk"), manifest.GetRows()[2].duplicateOf, L"as does a duplicate with no image");

        manifest.RestoreRows (removal);

        Assert::IsTrue (manifest.Format() == before);
    }


    //  The images are the same, so a duplicate that takes the deleted image's
    //  place takes its status as well.
    TEST_METHOD (ADuplicateThatTakesADeletedImagesPlace_TakesItsStatus)
    {
        FullSendManifest  manifest;
        FullSendRow       row;

        row.fingerprint = std::string (64, 'a');
        row.hash        = std::string (64, 'b');

        row.sequence    = 1;
        row.file        = "Disk 001.dsk";
        row.status      = FullSendManifest::kStatusDamaged;
        manifest.Add (row);

        row.sequence    = 2;
        row.file        = "Disk 002.dsk";
        row.status      = FullSendManifest::kStatusDuplicate;
        row.duplicateOf = "Disk 001.dsk";
        manifest.Add (row);

        manifest.RemoveImage ("Disk 001.dsk");

        Assert::AreEqual (std::string (FullSendManifest::kStatusDamaged), manifest.GetRows()[0].status);
        Assert::IsTrue   (manifest.GetRows()[0].duplicateOf.empty());
    }


    TEST_METHOD (NoteDeleteThenNoteRestore_RoundTripsTheManifest)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  before = MakeManifest().Format();
        std::vector<Byte>  bytes;
        FullSendRemoval    removal;
        FullSendManifest   read;

        AssertSucceeded (io.WriteAllBytes (ManifestPath(), before));
        AssertSucceeded (FullSendManifestFile::NoteDelete (io, kFolder, "Disk 001.dsk", removal));
        AssertSucceeded (io.ReadAllBytes (ManifestPath(), bytes));
        AssertSucceeded (read.Parse (bytes));

        Assert::AreEqual ((size_t) 2, removal.removed.size());
        Assert::IsTrue   (read.GetRows().empty(), L"the image and its duplicate are removed");

        AssertSucceeded (FullSendManifestFile::NoteRestore (io, kFolder, removal));
        AssertSucceeded (io.ReadAllBytes (ManifestPath(), bytes));

        Assert::IsTrue (bytes == before, L"restoring puts the rows back as they were");
    }

    //  A manifest that can't be read is left for the user to repair.
    TEST_METHOD (NoteRename_OfADamagedManifest_FailsAndWritesNothing)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  damaged = { 'n', 'o', 't', ',', 'a' };
        std::vector<Byte>  after;
        HRESULT            noted   = S_OK;

        AssertSucceeded (io.WriteAllBytes (ManifestPath(), damaged));
        noted = FullSendManifestFile::NoteRename (io, kFolder, "Disk 001.dsk", "Archon.dsk");
        Assert::IsTrue  (FAILED (noted));
        AssertSucceeded (io.ReadAllBytes (ManifestPath(), after));
        Assert::IsTrue  (after == damaged);
    }
};
