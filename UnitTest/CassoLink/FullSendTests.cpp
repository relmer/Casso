#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendSession.h"
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
//  FullSendTests
//
//  FullSendManifest, FullSendDisk and FullSendSession, the session over a
//  scripted Apple: each drive's disk changes at scripted events, and each key
//  in the script ends a pause.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendTests)
{
public:

    using Image = std::vector<Byte>;

    static constexpr size_t  kTrackBytes = 16 * 256;
    static constexpr const char * kFolder = "C:\\Box";

    struct ScriptedLink : IFullSendLink
    {
        std::array<Image, 2>                  drives;
        std::array<bool, 2>                   present        = { true, true };
        std::deque<FullSendKey>               keys;
        std::vector<int>                      diskReads;     // the drive of each whole-disk read, in order
        std::vector<int>                      trackDrives;   // the drive of each track read
        std::vector<FullSendScreen>           screens;
        std::vector<std::string>              reports;
        std::vector<std::pair<int, int>>      badSectors;    // reported on every disk read of badDisk
        Image                                 badDisk;
        int                                   waits          = 0;
        bool                                  wholeTracksBad = false;     // badDisk's tracks are not read whole in a check
        std::vector<int>                      tracksRead;                 // the tracks of the whole-disk reads

        //  Called at the end of the Nth whole-disk read, counted from 1, and
        //  at the Nth wait for a key.
        std::map<int, std::function<void (ScriptedLink &)>>  afterDiskRead;
        std::map<int, std::function<void (ScriptedLink &)>>  duringDiskRead;
        std::map<int, std::function<void (ScriptedLink &)>>  beforeKey;

        //  Reads of a drive's track return alternating fingerprints this many
        //  times, as a disk not seated.
        std::array<int, 2>                    unseatedReads  = { 0, 0 };
        HRESULT                               failDiskRead   = S_OK;

        bool  HasDrive (int drive) override { return present[(size_t) drive - 1]; }

        HRESULT  ReadTrack (int drive, int track, Image & ioImage, bool & outHasDisk, bool & outWhole) override
        {
            const Image  & disk = drives[(size_t) drive - 1];

            trackDrives.push_back (drive);
            outHasDisk = !disk.empty();
            outWhole   = outHasDisk && !(disk == badDisk && wholeTracksBad);

            if (outHasDisk)
            {
                std::copy_n (disk.begin() + (ptrdiff_t) (track * kTrackBytes), kTrackBytes, ioImage.begin() + (ptrdiff_t) (track * kTrackBytes));

                if (unseatedReads[(size_t) drive - 1] > 0)
                {
                    ioImage[(size_t) track * kTrackBytes + 200] = (Byte) unseatedReads[(size_t) drive - 1]--;
                }
            }

            return S_OK;
        }

        HRESULT  ReadDisk (int drive, Image & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable) override
        {
            int  read = (int) diskReads.size() + 1;

            diskReads.push_back (drive);

            if (failDiskRead != S_OK)
            {
                return failDiskRead;
            }

            if (duringDiskRead.contains (read))
            {
                duringDiskRead[read] (*this);
            }

            for (int track = 0; track < 35; track++)
            {
                if (std::find (haveTracks.begin(), haveTracks.end(), track) == haveTracks.end())
                {
                    tracksRead.push_back (track);
                    std::copy_n (drives[(size_t) drive - 1].begin() + (ptrdiff_t) (track * kTrackBytes), kTrackBytes, ioImage.begin() + (ptrdiff_t) (track * kTrackBytes));
                }
            }

            if (ioImage == badDisk)
            {
                outUnreadable = badSectors;
            }

            if (afterDiskRead.contains (read))
            {
                afterDiskRead[read] (*this);
            }

            return S_OK;
        }

        HRESULT  ShowScreen (const FullSendScreen & screen) override
        {
            screens.push_back (screen);
            return S_OK;
        }

        HRESULT  WaitForKey (FullSendKey & outKey) override
        {
            waits++;

            if (beforeKey.contains (waits))
            {
                beforeKey[waits] (*this);
            }

            if (keys.empty())
            {
                return HRESULT_FROM_WIN32 (ERROR_CANCELLED);
            }

            outKey = keys.front();
            keys.pop_front();
            return S_OK;
        }

        void  Report (const std::string & text) override { reports.push_back (text); }
    };

    struct Rig
    {
        ScriptedLink    link;
        FakeDiskFileIo  io;
        std::string     extension;

        std::unique_ptr<FullSendSession>  Run (HRESULT result = HRESULT_FROM_WIN32 (ERROR_CANCELLED))
        {
            auto  session = std::make_unique<FullSendSession> (link, io, [] () { return std::string ("2026-10-30T12:00:00Z"); }, kFolder, extension);

            Assert::AreEqual (result, session->Run());
            Assert::IsTrue (session->GetState() == FullSendSession::State::Ended);

            return session;
        }

        std::vector<std::string>  Files() const
        {
            std::vector<std::string>  files;

            for (const auto & [path, bytes] : io.files)
            {
                files.push_back (std::filesystem::path (path).filename().string());
            }

            std::sort (files.begin(), files.end());

            return files;
        }
    };

    static Image  Dos (const char * program, Byte seed = 1)
    {
        Image        buffer (NibblizationLayer::kImageByteSize, 0);
        Image        next;
        FilePayload  payload;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (600, seed);

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));
        AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse (program), payload, next));

        return next;
    }

    static Image  ProDos (const char * volume)
    {
        Image  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, volume));

        return buffer;
    }

    //  An unrecognized disk, with bytes of its seed in every sector.
    static Image  Other (Byte seed)
    {
        return Image (NibblizationLayer::kImageByteSize, seed);
    }




    //  The manifest.

    static FullSendRow  Row (int sequence, const std::string & file, char fingerprint = 'a', char hash = 'b')
    {
        FullSendRow  row;

        row.sequence    = sequence;
        row.file        = file;
        row.drive       = 2;
        row.fingerprint = std::string (64, fingerprint);
        row.hash        = std::string (64, hash);
        row.time        = "2026-10-30T12:00:00Z";
        row.unreadable  = 3;
        row.status      = FullSendManifest::kStatusProtected;

        return row;
    }

    TEST_METHOD (Manifest_RoundTripsEveryColumn_WithACommaAndQuoteInAFileName)
    {
        FullSendManifest  written;
        FullSendManifest  read;

        written.Add (Row (7, "0007 A, \"B\".dsk"));
        AssertSucceeded (read.Parse (written.Format()));

        Assert::AreEqual ((size_t) 1, read.GetRows().size());

        const FullSendRow  & row = read.GetRows()[0];

        Assert::AreEqual (7, row.sequence);
        Assert::AreEqual (std::string ("0007 A, \"B\".dsk"), row.file);
        Assert::AreEqual (2, row.drive);
        Assert::AreEqual (std::string (64, 'a'), row.fingerprint);
        Assert::AreEqual (std::string (64, 'b'), row.hash);
        Assert::AreEqual (std::string ("2026-10-30T12:00:00Z"), row.time);
        Assert::AreEqual (3, row.unreadable);
        Assert::AreEqual (std::string ("possibly-protected"), row.status);
    }

    TEST_METHOD (Manifest_FailsToParseDamage_WithNoRows)
    {
        FullSendManifest   good;
        const std::string  text = [&] () { good.Add (Row (1, "x.dsk")); auto b = good.Format(); return std::string (b.begin(), b.end()); }();

        std::vector<std::string>  damaged =
        {
            "",
            "not a manifest\r\n",
            text + "1,2,3\r\n",
            std::string (text).replace (text.find ("sent") == std::string::npos ? text.find ("possibly-protected") : text.find ("sent"), 18, "whatever"),
            text + "x,y.dsk,1," + std::string (64, 'a') + "," + std::string (64, 'b') + ",t,0,sent,\r\n",
            text + "2,\"unclosed,1," + std::string (64, 'a') + "," + std::string (64, 'b') + ",t,0,sent,\r\n",
            text + "2,y.dsk,3," + std::string (64, 'a') + "," + std::string (64, 'b') + ",t,0,sent,\r\n",
            text + "2,y.dsk,1,abc," + std::string (64, 'b') + ",t,0,sent,\r\n",
        };

        Assert::IsFalse (damaged.empty());

        for (const std::string & bytes : damaged)
        {
            FullSendManifest  manifest;
            HRESULT           hr = manifest.Parse (std::vector<Byte> (bytes.begin(), bytes.end()));

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, TextEncoding::NarrowToWide (bytes).c_str());
            Assert::IsTrue (manifest.GetRows().empty());
        }
    }

    TEST_METHOD (Manifest_NextSequenceFollowsTheHighest_AndADuplicateHasNoImage)
    {
        FullSendManifest  manifest;
        FullSendRow       duplicate = Row (9, "", 'c', 'd');

        Assert::AreEqual (1, manifest.GetNextSequence());

        manifest.Add (duplicate);
        manifest.Add (Row (4, "0004.dsk", 'e', 'd'));

        Assert::AreEqual (10, manifest.GetNextSequence());
        Assert::IsTrue  (manifest.HasFingerprint (std::string (64, 'c')));
        Assert::IsFalse (manifest.HasFingerprint (std::string (64, 'f')));
        Assert::AreEqual (std::string ("0004.dsk"), manifest.FindImageByHash (std::string (64, 'd'))->file);
    }




    //  Fingerprints and hashes.

    TEST_METHOD (Sha256_MatchesAKnownDigest)
    {
        const Byte   abc[] = { 'a', 'b', 'c' };
        std::string  hex;

        AssertSucceeded (FullSendDisk::ComputeHash (abc, hex));

        Assert::AreEqual (std::string ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), hex);
    }

    TEST_METHOD (Fingerprint_CoversTrackZeroAndTheCatalogTrack_AndNoOther)
    {
        Image        image = Other (5);
        std::string  base;
        std::string  next;

        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, base));

        image[5 * kTrackBytes]++;
        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, next));
        Assert::AreEqual (base, next, L"track 5 is not part of it");

        image[17 * kTrackBytes]++;
        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, next));
        Assert::AreNotEqual (base, next, L"track 17 is part of it");

        image = Other (5);
        image[0]++;
        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, next));
        Assert::AreNotEqual (base, next, L"track 0 is part of it");
    }

    TEST_METHOD (Fingerprint_OfProDos_IsTrackZeroAlone)
    {
        Image        image = ProDos ("BOX");
        std::string  base;
        std::string  next;

        Assert::IsTrue (FullSendDisk::IsCatalogOnTrackZero (image));
        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, base));

        image[17 * kTrackBytes]++;
        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, next));

        Assert::AreEqual (base, next);
        Assert::IsFalse (FullSendDisk::IsCatalogOnTrackZero (Dos ("HELLO")));
    }




    //  File titles.

    TEST_METHOD (Title_ComesFromTheCatalog_OrIsTheNumberAlone)
    {
        Image   pascalDisk = Other (0);
        size_t  at         = ProDosSkeleton::GetBlockByteOffset (2, 0);

        pascalDisk[at + 2] = 6;
        pascalDisk[at + 6] = 5;
        std::copy_n ("APPLE", 5, pascalDisk.begin() + (ptrdiff_t) at + 7);

        Assert::AreEqual (std::string ("0003 GAMES"), FullSendDisk::MakeImageTitle (3, FullSendDisk::GetCatalogTitle (ProDos ("GAMES"))));
        Assert::AreEqual (std::string ("0012 HELLO"), FullSendDisk::MakeImageTitle (12, FullSendDisk::GetCatalogTitle (Dos ("HELLO"))));
        Assert::AreEqual (std::string ("0004 APPLE"), FullSendDisk::MakeImageTitle (4, FullSendDisk::GetCatalogTitle (pascalDisk)));
        Assert::AreEqual (std::string ("0005"),       FullSendDisk::MakeImageTitle (5, FullSendDisk::GetCatalogTitle (Other (9))));
    }

    TEST_METHOD (Title_CleansCharactersAHostNameCannotHave)
    {
        Assert::AreEqual (std::string ("0001 A_B"), FullSendDisk::MakeImageTitle (1, "A/B"));
    }

    TEST_METHOD (Session_NeverOverwritesAFile)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.present[1] = false;
        rig.io.files["C:\\Box\\0001.dsk"] = { 1, 2, 3 };

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files["C:\\Box\\0001.dsk"] == Image ({ 1, 2, 3 }));
        Assert::IsTrue (rig.io.files["C:\\Box\\0001 (2).dsk"] == Other (1));
        Assert::AreEqual (std::string ("0001 (2).dsk"), session->GetManifest().GetRows()[0].file);
    }




    //  Drive order, pauses and ends.

    TEST_METHOD (Session_ReadsDriveOneThenTwo_WritingEachWithNoKey)
    {
        Rig  rig;

        rig.link.drives[0] = Dos ("HELLO");
        rig.link.drives[1] = ProDos ("GAMES");

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2 }));
        Assert::IsTrue (rig.io.files["C:\\Box\\0001 HELLO.dsk"] == rig.link.drives[0]);
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0002 GAMES.po"));
        Assert::AreEqual (1, rig.link.waits, L"the pause came only once neither drive had a new disk");
        Assert::AreEqual (2, session->GetScreen().sent);
    }

    TEST_METHOD (Session_AfterADisk_ChecksTheOtherDriveFirst)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.drives[1] = Other (2);

        //  Drive 2 was read last, so drive 1 is the other drive at the pause.
        rig.link.beforeKey[1] = [] (ScriptedLink & link) { link.drives[0] = Other (3); link.drives[1] = Other (4); };
        rig.link.keys         = { FullSendKey::Space };

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2, 1, 2 }));
    }

    TEST_METHOD (Session_ASwapDuringTheOtherDrivesTransfer_IsSentWithNoKey)
    {
        Rig  rig;

        rig.link.drives[0]         = Other (1);
        rig.link.drives[1]         = Other (2);
        rig.link.duringDiskRead[2] = [] (ScriptedLink & link) { link.drives[0] = Other (3); };
        rig.link.duringDiskRead[3] = [] (ScriptedLink & link) { link.drives[1] = Other (4); };

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2, 1, 2 }));
        Assert::AreEqual (1, rig.link.waits);
        Assert::AreEqual (4, session->GetScreen().sent);
    }

    TEST_METHOD (Session_WithNoNewDisk_PausesWithNoDriveRead_UntilSpace)
    {
        Rig  rig;
        int  tracksAtPause = -1;

        rig.link.drives[0]    = Other (1);
        rig.link.beforeKey[1] = [&] (ScriptedLink & link) { tracksAtPause = (int) link.trackDrives.size(); };

        rig.Run();

        Assert::AreEqual (1, rig.link.waits);
        Assert::AreEqual (tracksAtPause, (int) rig.link.trackDrives.size(), L"no track was read after the pause began");
    }

    TEST_METHOD (Session_Sending_PutsTheOtherDriveOnDeck)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);

        rig.Run();

        auto  sending = std::find_if (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & screen) { return screen.drives[0] == FullSendDriveState::Sending; });

        Assert::IsTrue (sending != rig.link.screens.end());
        Assert::IsTrue (sending->drives[1] == FullSendDriveState::OnDeck);
        Assert::AreNotEqual (0, (int) sending->quip);
        Assert::AreNotEqual (0, (int) rig.link.screens.back().quip, L"the pause has a prompt line");
    }

    TEST_METHOD (Session_OneDrive_Sending_HasNoPromptLine)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        rig.Run();

        for (const FullSendScreen & screen : rig.link.screens)
        {
            Assert::IsTrue (screen.drives[0] != FullSendDriveState::Sending || screen.quip == 0);
        }
    }

    TEST_METHOD (Session_OneDrive_LeavesTheMissingDriveOff)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;

        rig.Run();

        for (const FullSendScreen & screen : rig.link.screens)
        {
            Assert::IsTrue (screen.drives[1] == FullSendDriveState::Off);
        }
    }

    TEST_METHOD (Session_Space_SendsARepeatedDisk_WhenNoDriveHasANewOne)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.keys      = { FullSendKey::Space };

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 1 }));
        Assert::AreEqual (1, session->GetScreen().duplicates, L"the same bytes are a duplicate");
    }

    TEST_METHOD (Session_Space_WithARepeatInBothDrives_SendsTheDriveNotReadLast)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.drives[1] = Other (2);
        rig.link.keys      = { FullSendKey::Space };

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1, 2, 1 }));
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

    TEST_METHOD (Session_ADiskRead_TakesTwoMatchingFingerprintReads)
    {
        Rig  rig;

        rig.link.drives[0]  = ProDos ("BOX");
        rig.link.present[1] = false;

        rig.Run();

        Assert::AreEqual ((size_t) 1, rig.link.diskReads.size());
        Assert::IsTrue (rig.link.trackDrives.size() >= 2, L"track 0 twice before the read");
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
        Assert::IsTrue (rig.Files() == std::vector<std::string> ({ "0001.dsk", "full-send.csv" }));
    }




    //  Images, duplicates and bad sectors.

    TEST_METHOD (Session_ADuplicateImage_IsDiscarded_WithARowAndTone)
    {
        Rig    rig;
        Image  first  = Other (1);
        Image  second = first;

        //  A copy has the first disk's fingerprint, so Space sends it as a repeat.
        rig.link.drives[0] = first;
        rig.link.drives[1] = second;
        rig.link.keys      = { FullSendKey::Space };

        auto  session = rig.Run();

        Assert::IsTrue (rig.Files() == std::vector<std::string> ({ "0001.dsk", "full-send.csv" }));

        const auto  & rows = session->GetManifest().GetRows();

        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::AreEqual (2, rows[1].sequence, L"a duplicate has a sequence number");
        Assert::AreEqual (std::string ("duplicate"), rows[1].status);
        Assert::AreEqual (std::string ("0001.dsk"), rows[1].duplicateOf);
        Assert::IsTrue (rows[1].file.empty());
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
        Assert::AreEqual (std::string ("possibly-protected"), rows[0].status);
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
        Assert::IsTrue (rig.Files() == std::vector<std::string> ({ "0001.dsk", "full-send.csv" }));
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

        Assert::AreEqual ((size_t) 33, rig.link.tracksRead.size());
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

        Assert::IsTrue (rig.Files() == std::vector<std::string> ({ "0001 GAMES.dsk", "full-send.csv" }));
    }

    TEST_METHOD (Session_WritesTheManifestAfterEachDisk_NotAtTheEnd)
    {
        Rig     rig;
        size_t  atSecond = 0;

        rig.link.drives[0]         = Other (1);
        rig.link.drives[1]         = Other (2);
        rig.link.duringDiskRead[2] = [&] (ScriptedLink &) { atSecond = rig.io.files["C:\\Box\\full-send.csv"].size(); };

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

        rig.io.files["C:\\Box\\full-send.csv"] = manifest.Format();
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

        rig.io.files["C:\\Box\\full-send.csv"] = damaged;
        rig.link.drives[0] = Other (1);

        auto  session = rig.Run (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        Assert::IsTrue (session->GetEndReason() == FullSendSession::EndReason::ManifestDamaged);
        Assert::IsTrue (rig.link.trackDrives.empty());
        Assert::IsTrue (rig.io.files["C:\\Box\\full-send.csv"] == damaged);
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
};
