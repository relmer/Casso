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
        bool                                  begun          = false;
        bool                                  verifying      = false;
        std::vector<SectorMark>               reportMarks;
        int                                   reportVerified = 0;
        int                                   reportEmpty    = 0;
        bool                                  bootstrapped   = false;
        FullSendKey                           beginKey       = FullSendKey::Space;
        std::vector<std::string>              entryLines;
        std::vector<std::vector<std::string>> pauseLines;
        std::deque<Byte>                      typed;                      // the codes of a line typed after P
        std::vector<std::vector<std::string>> typedLines;
        bool                                  wholeTracksBad = false;     // badDisk's tracks are not read whole in a check
        std::vector<int>                      tracksRead;                 // the tracks of the whole-disk reads
        std::set<int>                         blankTracks;                // tracks with no sector that reads, as on some protected disks
        std::vector<int>                      captures;                   // the drive of each raw capture
        Image                                 rawWoz         = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
        Image                                 rawReads;                   // a raw capture's reads, empty when they aren't saved

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
            outHasDisk = !disk.empty() && !blankTracks.contains (track);
            outWhole   = outHasDisk && !(disk == badDisk && wholeTracksBad) && unseatedReads[(size_t) drive - 1] == 0;

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

        bool     IsVerifying() override { return verifying; }
        void     ToggleVerify() override { verifying = !verifying; }

        HRESULT  ReadDisk (int drive, Image & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable, FullSendReadReport & outReport) override
        {
            outReport.marks          = reportMarks;
            outReport.tracksVerified = reportVerified;
            outReport.emptyTracks    = reportEmpty;

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

        bool     IsBootstrapped() override { return bootstrapped; }

        HRESULT  CaptureRaw (int drive, Image & outWoz, Image & outReads) override
        {
            captures.push_back (drive);
            outWoz   = rawWoz;
            outReads = rawReads;
            return S_OK;
        }

        HRESULT  WaitForKey (const std::vector<std::string> & lines, FullSendKey & outKey) override
        {
            //  The first wait is the one that begins the session.
            if (!begun)
            {
                begun      = true;
                entryLines = lines;
                outKey     = beginKey;
                return S_OK;
            }

            pauseLines.push_back (lines);
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

        HRESULT  WaitForAnyKey (const std::vector<std::string> & lines, Byte & outCode) override
        {
            typedLines.push_back (lines);

            if (typed.empty())
            {
                return HRESULT_FROM_WIN32 (ERROR_CANCELLED);
            }

            outCode = typed.front();
            typed.pop_front();
            return S_OK;
        }

        void  Report (const std::string & text) override { reports.push_back (text); }
    };

    struct Rig
    {
        ScriptedLink    link;
        FakeDiskFileIo  io;
        std::string     extension;
        std::string     prefix;
        FullSendStatus  status { [] () { return 0.0; }, [] () { return std::string(); } };

        std::unique_ptr<FullSendSession>  Run (HRESULT result = HRESULT_FROM_WIN32 (ERROR_CANCELLED))
        {
            auto  session = std::make_unique<FullSendSession> (link, io, [] () { return std::string ("2026-10-30T12:00:00Z"); }, kFolder, extension, prefix, &status);

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

    //  A ProDOS disk with one file, so it isn't blank.
    static Image  ProDos (const char * volume)
    {
        Image        buffer (NibblizationLayer::kImageByteSize, 0);
        Image        next;
        FilePayload  payload;

        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (600, 1);

        AssertSucceeded (ProDosSkeleton::Write (buffer, volume));
        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("DATA"), payload, next));

        return next;
    }

    //  A formatted disk with no files.
    static Image  BlankProDos (const char * volume)
    {
        Image  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, volume));

        return buffer;
    }

    static Image  BlankDos()
    {
        Image  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));

        return buffer;
    }

    //  A DOS 3.3 disk whose one file is a BASIC program with the given bytes.
    static Image  DosProgram (std::initializer_list<Byte> program)
    {
        Image        next;
        FilePayload  payload;

        payload.type  = Dos33Volume::kTypeApplesoft;
        payload.bytes = program;

        AssertSucceeded (Dos33Volume (BlankDos()).Write (FilePath::Parse ("HELLO"), payload, next));

        return next;
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
        Assert::AreEqual (std::string ("likely-copy-protected"), row.status);
    }

    //  A row's boot result and its capture's marks are written and read, a
    //  comma in the marks included; a manifest from before those two columns
    //  loads with both empty.
    TEST_METHOD (Manifest_HasTheBootResultAndTheCapturesMarks_AndReadsAManifestWithout)
    {
        FullSendManifest   written;
        FullSendManifest   read;
        FullSendManifest   prior;
        FullSendRow        row   = Row (7, "0007.woz");
        std::string        older = "sequence,file,drive,fingerprint,hash,time,unreadable,status,duplicateOf,verification,recovered,unreliable\r\n"
                                   "3,0003.dsk,1," + std::string (64, 'a') + "," + std::string (64, 'b') + ",2026-10-30T12:00:00Z,0,sent,,none,0,0\r\n";



        row.boot  = "did-not-boot";
        row.marks = "weak 3, 11; replaced 1.3";

        written.Add (row);
        AssertSucceeded (read.Parse (written.Format()));

        Assert::AreEqual (std::string ("did-not-boot"), read.GetRows()[0].boot);
        Assert::AreEqual (std::string ("weak 3, 11; replaced 1.3"), read.GetRows()[0].marks);

        AssertSucceeded (prior.Parse (std::vector<Byte> (older.begin(), older.end())));

        Assert::AreEqual ((size_t) 1, prior.GetRows().size());
        Assert::IsTrue (prior.GetRows()[0].boot.empty() && prior.GetRows()[0].marks.empty());
        Assert::AreEqual (std::string ("none"), prior.GetRows()[0].verification);
    }

    TEST_METHOD (Manifest_ParsesTheEarlierProtectedLabel)
    {
        FullSendManifest   manifest;
        FullSendManifest   parsed;
        auto               row = Row (1, "x.dsk");
        std::vector<Byte>  text;

        row.status = FullSendManifest::kStatusLegacyProtected;
        manifest.Add (row);
        text = manifest.Format();

        AssertSucceeded (parsed.Parse (text));
        Assert::AreEqual (std::string (FullSendManifest::kStatusLegacyProtected), parsed.GetRows()[0].status);
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
            std::string (text).replace (text.find ("sent") == std::string::npos ? text.find ("likely-copy-protected") : text.find ("sent"), 18, "whatever"),
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

    TEST_METHOD (Fingerprint_OfAnUnrecognizedDisk_AddsTrackThree)
    {
        Image        image = Other (0xE5);
        std::string  base;
        std::string  next;

        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, base));

        image[3 * kTrackBytes]++;
        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, next));

        Assert::AreNotEqual (base, next, L"two CP/M data disks differ only in their directory");
    }

    TEST_METHOD (Fingerprint_OfADosDisk_LeavesTrackThreeOut)
    {
        Image        image = Dos ("HELLO");
        std::string  base;
        std::string  next;

        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, base));

        image[3 * kTrackBytes]++;
        AssertSucceeded (FullSendDisk::ComputeFingerprint (image, next));

        Assert::AreEqual (base, next);
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

        Assert::AreEqual (std::string ("0003 GAMES"), FullSendDisk::MakeImageTitle (3, FullSendDisk::GetCatalogTitle (ProDos ("GAMES")), ""));
        Assert::AreEqual (std::string ("0012 HELLO"), FullSendDisk::MakeImageTitle (12, FullSendDisk::GetCatalogTitle (Dos ("HELLO")), ""));
        Assert::AreEqual (std::string ("0004 APPLE"), FullSendDisk::MakeImageTitle (4, FullSendDisk::GetCatalogTitle (pascalDisk), ""));
        Assert::AreEqual (std::string ("0005"),       FullSendDisk::MakeImageTitle (5, FullSendDisk::GetCatalogTitle (Other (9)), ""));
    }

    TEST_METHOD (Title_CleansCharactersAHostNameCannotHave)
    {
        Assert::AreEqual (std::string ("0001 A_B"), FullSendDisk::MakeImageTitle (1, "A/B", ""));
    }

    //  The first program on a protected disk can have a name of control
    //  characters, as on The Print Shop, which is no title at all.
    TEST_METHOD (Title_DropsControlCharacters_AndSkipsANameOfThemAlone)
    {
        constexpr size_t  kCatalogSector = (size_t) (17 * 16 + 15) * 256;
        constexpr size_t  kFirstName     = kCatalogSector + 0x0B + 3;
        constexpr size_t  kNameLength    = 30;
        constexpr Byte    kBackspace     = 0x88;
        constexpr Byte    kSpace         = 0xA0;
        constexpr Byte    kShopName[]    = { kBackspace, kBackspace, 0xD3, 0xC8, 0xCF, 0xD0 };   // two backspaces, then SHOP with the high bit set

        Image        disk = Dos ("HELLO");
        Image        next;
        FilePayload  payload;



        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (100, 3);

        AssertSucceeded (Dos33Volume (disk).Write (FilePath::Parse ("GAME"), payload, next));
        std::fill_n (next.begin() + (ptrdiff_t) kFirstName, kNameLength, kBackspace);

        Assert::AreEqual (std::string ("GAME"), FullSendDisk::GetCatalogTitle (next));

        //  Control characters ahead of a name are dropped from the title.
        std::fill_n (next.begin() + (ptrdiff_t) kFirstName, kNameLength, kSpace);
        std::copy_n (std::begin (kShopName), std::size (kShopName), next.begin() + (ptrdiff_t) kFirstName);

        Assert::AreEqual (std::string ("SHOP"), FullSendDisk::GetCatalogTitle (next));
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
        Assert::AreEqual (0, (int) sending->quip, L"the instruction, the first time");
        Assert::AreNotEqual (0, (int) rig.link.screens.back().quip, L"the pause has a prompt line");
    }

    TEST_METHOD (Session_PromptLines_ShowEveryLineBeforeAnyRepeats)
    {
        Rig                                   rig;
        std::vector<int>                      shown;
        const size_t                          count = FullSendScreen::kQuipCount;

        rig.link.keys.assign (count * 4, FullSendKey::Space);

        rig.Run();

        for (const FullSendScreen & screen : rig.link.screens)
        {
            if (screen.quip != 0)
            {
                shown.push_back (screen.quip);
            }
        }

        Assert::IsTrue (shown.size() > count * 3);

        for (size_t start = 0; start + count <= shown.size(); start += count)
        {
            std::set<int>  set (shown.begin() + (ptrdiff_t) start, shown.begin() + (ptrdiff_t) (start + count));

            Assert::AreEqual (count, set.size());
            Assert::AreEqual (1, *set.begin());
            Assert::AreEqual ((int) count, *set.rbegin());
        }

        for (size_t i = 1; i < shown.size(); i++)
        {
            Assert::AreNotEqual (shown[i - 1], shown[i]);
        }
    }

    TEST_METHOD (Screen_PromptLines_FitAfterOnDeck)
    {
        for (int quip = 1; quip <= FullSendScreen::kQuipCount; quip++)
        {
            Assert::IsTrue (strlen (FullSendScreen::GetQuipText ((Byte) quip)) <= 27, TextEncoding::NarrowToWide (FullSendScreen::GetQuipText ((Byte) quip)).c_str());
        }
    }

    TEST_METHOD (Screen_Rows_HaveTheCountsALineAndEachDrive)
    {
        FullSendScreen  screen;

        screen.drives = { FullSendDriveState::Sending, FullSendDriveState::OnDeck };
        screen.names  = { "13 GAMES DISK", "" };
        screen.sent   = 12;

        std::vector<std::string>  rows = screen.GetRows();

        Assert::AreEqual (std::string ("FULL SEND    VERIFY: OFF  PREFIX: NONE"), rows[0]);
        Assert::AreEqual (std::string ("SENT 12   DUP 0   BAD 0"), rows[1]);
        Assert::AreEqual (std::string (40, '-'), rows[2]);
        Assert::AreEqual (std::string ("D1: SENDING 13 GAMES DISK"), rows[3]);
        Assert::AreEqual (std::string ("D2: ON DECK--INSERT A DISK TO SEND NEXT"), rows[5]);

        screen.quip = 6;
        Assert::AreEqual (std::string ("D2: ON DECK--FEED ME, SEYMOUR!"), screen.GetRows()[5]);
    }

    TEST_METHOD (Session_ScreenName_HasNoLeadingZeros_AndTheMapMarksABadTrack)
    {
        Rig  rig;

        rig.link.drives[0]  = Dos ("HELLO");
        rig.link.badDisk    = rig.link.drives[0];
        rig.link.badSectors = { { 29, 3 } };
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::AreEqual (std::string ("1 HELLO"), session->GetScreen().names[0]);
        Assert::AreEqual ('X', session->GetScreen().maps[0][29]);
        Assert::AreEqual ('.', session->GetScreen().maps[0][28]);
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0001 HELLO.dsk"), L"the file keeps its zeros");
    }

    TEST_METHOD (Session_OnDeck_HasTheInstructionFirst_ThenAPromptLine)
    {
        Rig  rig;

        rig.link.drives[0]         = Other (1);
        rig.link.drives[1]         = Other (2);
        rig.link.duringDiskRead[2] = [] (ScriptedLink & link) { link.drives[0] = Other (3); };

        rig.Run();

        std::vector<int>  decks;

        for (const FullSendScreen & screen : rig.link.screens)
        {
            if (screen.drives[1] == FullSendDriveState::OnDeck)
            {
                decks.push_back (screen.quip);
            }
        }

        Assert::AreEqual (0, decks.front(), L"drive 2's first time on deck");
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

    TEST_METHOD (Session_VerifyKey_TogglesVerification_WithTheFlagOnTheScreen)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Verify };

        rig.Run();

        Assert::IsTrue (rig.link.verifying);
        Assert::IsTrue (rig.link.screens.back().verify);
        Assert::AreEqual ((int) (unsigned char) ('V' | 0x80), (int) (unsigned char) rig.link.screens.back().GetRows()[0][13], L"inverse on the Apple");
    }

    TEST_METHOD (Session_Marks_AreOnTheMap_AndInTheManifestRow)
    {
        Rig  rig;

        rig.link.drives[0]     = Other (1);
        rig.link.present[1]    = false;
        rig.link.reportMarks   = { SectorMark { 9, 1, 'R' }, SectorMark { 9, 2, 'U' }, SectorMark { 12, 0, 'R' } };
        rig.link.reportVerified = 20;

        auto  session = rig.Run();

        const FullSendRow  & row = session->GetManifest().GetRows()[0];

        Assert::AreEqual ('U', session->GetScreen().maps[0][9],  L"the worst of the track");
        Assert::AreEqual ('R', session->GetScreen().maps[0][12]);
        Assert::AreEqual (2, row.recovered);
        Assert::AreEqual (1, row.unreliable);
        Assert::AreEqual (std::string (FullSendManifest::kVerifyPartial), row.verification);
        Assert::AreEqual (1, session->GetScreen().recovered);
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\0001.dsk.unreadable.txt"));
    }

    TEST_METHOD (Assess_FollowsTheFileSystem_ThenTheTracks)
    {
        Assert::AreEqual (std::string (FullSendManifest::kStatusProtected), std::string (FullSendDisk::AssessDamage (Other (1), 4, 64)));
        Assert::AreEqual (std::string (FullSendManifest::kStatusUnclear),   std::string (FullSendDisk::AssessDamage (Other (1), 0, 2)));
        Assert::AreEqual (std::string (FullSendManifest::kStatusUnclear),   std::string (FullSendDisk::AssessDamage (Dos ("HELLO"), 4, 64)));
        Assert::AreEqual (std::string (FullSendManifest::kStatusDamaged),   std::string (FullSendDisk::AssessDamage (Dos ("HELLO"), 0, 2)));
    }

    //  A formatted disk with no files is blank, whatever its file system. A
    //  DOS 3.3 disk whose one file is a BASIC program with no lines is blank
    //  once that program's track is read, and one with a line isn't. A disk
    //  with a file isn't, and neither is one of no file system.
    TEST_METHOD (Blank_IsAFormattedDiskWithNoFiles_OrOnlyAProgramWithNoLines)
    {
        std::string       system;
        std::vector<int>  all (NibblizationLayer::kTrackCount);
        Image             empty   = DosProgram ({ 0x00, 0x00 });
        Image             written = DosProgram ({ 0x0A, 0x08, 0x0A, 0x00, 0xBA, 0x00, 0x00, 0x00 });

        std::iota (all.begin(), all.end(), 0);

        Assert::IsTrue (FullSendDisk::IsBlank (BlankDos(), {}, system));
        Assert::AreEqual (std::string ("DOS 3.3"), system);
        Assert::IsTrue (FullSendDisk::IsBlank (BlankProDos ("EMPTY"), {}, system));
        Assert::AreEqual (std::string ("ProDOS"), system);

        Assert::IsFalse (FullSendDisk::IsBlank (Dos ("HELLO"), all, system), L"A disk with a file");
        Assert::IsFalse (FullSendDisk::IsBlank (ProDos ("FULL"), all, system), L"A ProDOS disk with a file");
        Assert::IsFalse (FullSendDisk::IsBlank (Other (1), all, system), L"No file system");

        Assert::IsTrue (FullSendDisk::FindLoneProgramTrack (empty) >= 0, L"The program's track is found");
        Assert::IsFalse (FullSendDisk::IsBlank (empty, {}, system), L"Not before the program's track is read");
        Assert::IsTrue (FullSendDisk::IsBlank (empty, all, system), L"A program with no lines");
        Assert::IsFalse (FullSendDisk::IsBlank (written, all, system), L"A program with a line");
    }

    //  A blank disk isn't sent: its drive's row and the status have it, with
    //  its file system, no image is written, and the totals count it. The
    //  drive's number on the Apple sends it.
    TEST_METHOD (Session_ABlankDisk_IsReportedAndNotSent_UntilItsDriveIsPressed)
    {
        Rig  rig;

        rig.link.drives[0]  = BlankDos();
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files.empty() || !rig.io.files.contains ("C:\\Box\\0001.dsk"), L"No image");
        Assert::IsTrue (session->GetManifest().GetRows().empty(), L"No row");
        Assert::IsTrue (session->GetScreen().drives[0] == FullSendDriveState::Blank);
        Assert::IsTrue (session->GetScreen().GetRows()[3].starts_with ("D1: BLANK, DOS 3.3"), TextEncoding::NarrowToWide (session->GetScreen().GetRows()[3]).c_str());

        FullSendSnapshot  snapshot = rig.status.GetSnapshot();

        Assert::AreEqual (1, snapshot.totals.blanks);
        Assert::AreEqual (0, snapshot.totals.sent);
    }

    //  A raw capture's reads, when the link returns them, are written beside
    //  the image with .reads added to its file.
    TEST_METHOD (Session_ARawCapturesReads_AreWrittenBesideItsImage)
    {
        Rig  rig;

        rig.link.drives[0]   = Other (1);
        rig.link.badDisk     = rig.link.drives[0];
        rig.link.badSectors  = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty = 3;
        rig.link.present[1]  = false;
        rig.link.rawReads    = { 'C', 'R', 'A', 'W', 3, 0 };

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files["C:\\Box\\0001.woz"] == rig.link.rawWoz, L"The image is the capture");
        Assert::IsTrue (rig.io.files["C:\\Box\\0001.woz.reads"] == rig.link.rawReads, L"The reads are beside it");
        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size(), L"The reads have no row");
    }

    //  A likely copy-protected disk is captured raw: its image is the WOZ, its
    //  row is a raw capture, and the session goes on. A damaged disk with a
    //  file system isn't captured.
    TEST_METHOD (Session_ALikelyProtectedDisk_IsSavedAsARawCapture_AndTheSessionGoesOn)
    {
        Rig  rig;

        rig.link.drives[0]   = Other (1);
        rig.link.badDisk     = rig.link.drives[0];
        rig.link.badSectors  = { { 5, 0 }, { 6, 0 } };
        rig.link.reportEmpty = 3;
        rig.link.present[1]  = false;

        auto  session = rig.Run();

        Assert::IsTrue (rig.link.captures == std::vector<int> { 1 }, L"Drive 1 is captured");
        Assert::IsTrue (rig.io.files["C:\\Box\\0001.woz"] == rig.link.rawWoz, L"The image is the capture");
        Assert::IsFalse (rig.io.files.contains ("C:\\Box\\0001.dsk"), L"No sector image");
        Assert::AreEqual (std::string (FullSendManifest::kStatusRawCapture), session->GetManifest().GetRows()[0].status);
        Assert::AreEqual (std::string ("0001.woz"), session->GetManifest().GetRows()[0].file);
        Assert::AreEqual (1, session->GetScreen().prot);
        Assert::AreEqual (0, session->GetScreen().bad);
        Assert::IsTrue (session->GetScreen().drives[0] == FullSendDriveState::Protected);
        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & s) { return s.tone == FullSendTone::Protected; }));
        Assert::IsTrue (std::any_of (rig.link.screens.begin(), rig.link.screens.end(), [] (const FullSendScreen & s) { return s.drives[0] == FullSendDriveState::Capturing; }),
                        L"The Apple's screen has the capture while it runs");
        Assert::IsTrue (session->GetScreen().maps[0].empty(), L"No map of bad sectors");
        Assert::IsFalse (rig.io.files.contains ("C:\\Box\\0001.woz.unreadable.txt"), L"No list of unreadable sectors");
        Assert::IsFalse (rig.io.files.contains ("C:\\Box\\0001.woz.reads"), L"No reads file when the link has no reads");

        FullSendSnapshot  snapshot = rig.status.GetSnapshot();

        Assert::AreEqual (1, snapshot.totals.rawCaptures);
        Assert::AreEqual (std::string ("0001.woz"), snapshot.disks[0].row.file);
        Assert::IsFalse (snapshot.drives[0].positions.empty(), L"The drive's panel has the capture's positions");
        Assert::IsTrue (snapshot.drives[0].tracks[5] == FullSendTrackMark::Unreadable);

        Rig  damaged;

        damaged.link.drives[0]  = Dos ("HELLO");
        damaged.link.badDisk    = damaged.link.drives[0];
        damaged.link.badSectors = { { 20, 3 } };
        damaged.link.present[1] = false;

        auto  other = damaged.Run();

        Assert::IsTrue (damaged.link.captures.empty(), L"A damaged disk isn't captured");
        Assert::AreEqual (std::string (FullSendManifest::kStatusDamaged), other->GetManifest().GetRows()[0].status);
    }

    //  The Full Send window's model after a disk: its row, title, file
    //  system and tracks, the totals, the session's end and the lines.
    TEST_METHOD (Session_ADiskSent_IsInTheStatus_WithItsTitleTracksAndRow)
    {
        Rig  rig;

        rig.link.drives[0]  = Dos ("HELLO");
        rig.link.present[1] = false;

        auto              session  = rig.Run();
        FullSendSnapshot  snapshot = rig.status.GetSnapshot();

        Assert::IsTrue (snapshot.activity == FullSendActivity::Ended);
        Assert::IsFalse (snapshot.reason.empty());
        Assert::AreEqual ((size_t) 1, snapshot.disks.size());
        Assert::AreEqual (session->GetManifest().GetRows()[0].file, snapshot.disks[0].row.file);
        Assert::AreEqual (std::string ("DOS 3.3"), snapshot.disks[0].fileSystem);
        Assert::AreEqual (1, snapshot.totals.sent);
        Assert::AreEqual ((uint64_t) NibblizationLayer::kImageByteSize, snapshot.totals.bytes);
        Assert::IsTrue (snapshot.drives[0].state == FullSendDiskState::Sent);
        Assert::AreEqual (std::string ("0001 HELLO"), snapshot.drives[0].title);
        Assert::IsTrue (std::all_of (snapshot.drives[0].tracks.begin(), snapshot.drives[0].tracks.end(), [] (FullSendTrackMark mark) { return mark == FullSendTrackMark::Read; }));
        Assert::IsTrue (std::any_of (snapshot.lines.begin(), snapshot.lines.end(), [] (const FullSendLine & line) { return line.text.find ("0001 HELLO.dsk") != std::string::npos; }));
    }

    TEST_METHOD (Session_DoesNotReadADisk_UntilSpaceBegins)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.beginKey  = FullSendKey::Quit;

        rig.Run (IFullSendLink::kQuit);

        Assert::IsTrue (rig.link.trackDrives.empty());
        Assert::IsTrue (rig.link.diskReads.empty());
    }

    TEST_METHOD (Session_Begin_AsksToRemoveTheCassoLinkDisk_UnlessBootstrapped)
    {
        Rig  booted;
        Rig  bootstrapped;

        bootstrapped.link.bootstrapped = true;

        booted.Run();
        bootstrapped.Run();

        Assert::AreEqual (std::string ("REMOVE THE CASSOLINK DISK"), booted.link.entryLines[4]);
        Assert::AreEqual (std::string ("INSERT DISKS TO BE SENT INTO THE DRIVES"), bootstrapped.link.entryLines[4]);
    }

    //  Space with the disk just sent still in the drive changes nothing.
    TEST_METHOD (Session_Space_OnAnAlreadySentDisk_DoesNotReadIt_OrCountADuplicate)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Space };

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1 }));
        Assert::AreEqual (0, session->GetScreen().duplicates);
        Assert::AreEqual (std::string ("D1 MAY ALREADY HAVE BEEN SENT."), rig.link.pauseLines.back()[0]);
        Assert::AreEqual (std::string ("OR PRESS 1 TO SEND D1 ANYWAY."), rig.link.pauseLines.back()[4]);
    }

    //  Two Apples sending to one folder at once: the second session runs from
    //  start to end while the first is between reading its disk and writing it.
    TEST_METHOD (Session_TwoAtOnceInOneFolder_KeepEveryRow_AndNumberEachDisk)
    {
        Rig                                 first;
        Rig                                 second;
        std::unique_ptr<FullSendSession>    other;
        FullSendManifest                    manifest;



        first.link.drives[0]    = Dos ("HELLO");
        first.link.present[1]   = false;
        second.link.drives[0]   = Dos ("WORLD", 2);
        second.link.present[1]  = false;

        first.link.afterDiskRead[1] = [&] (ScriptedLink &)
        {
            other = std::make_unique<FullSendSession> (second.link, first.io, [] () { return std::string ("2026-10-30T12:00:00Z"); }, kFolder, "", "");
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), other->Run());
        };

        first.Run();

        AssertSucceeded (manifest.Parse (first.io.files["C:\\Box\\manifest.fullsend"]));
        Assert::AreEqual ((size_t) 2, manifest.GetRows().size());
        Assert::AreEqual (std::string ("0001 WORLD.dsk"), manifest.GetRows()[0].file);
        Assert::AreEqual (std::string ("0002 HELLO.dsk"), manifest.GetRows()[1].file);
    }

    TEST_METHOD (Session_ADiskAnotherSessionSent_IsARepeat)
    {
        Rig  first;
        Rig  second;



        first.link.drives[0]    = Dos ("HELLO");
        first.link.present[1]   = false;
        second.link.drives[0]   = Image();
        second.link.present[1]  = false;
        second.link.keys        = { FullSendKey::Space };

        //  The second session started first, and its drive gets the disk the
        //  first sent.
        second.link.beforeKey[1] = [&] (ScriptedLink & link)
        {
            FullSendSession  session (first.link, second.io, [] () { return std::string ("2026-10-30T12:00:00Z"); }, kFolder, "", "");

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), session.Run());
            link.drives[0] = Dos ("HELLO");
        };

        second.Run();

        Assert::IsTrue   (second.link.diskReads.empty(), L"The disk the first session sent isn't read again");
        Assert::AreEqual (std::string ("D1 MAY ALREADY HAVE BEEN SENT."), second.link.pauseLines.back()[0]);
    }

    //  A protected disk whose catalog track has no sector that reads: the read
    //  after the disk's is of track 0, so the disk isn't taken for a swap.
    TEST_METHOD (Session_ADiskWithABlankCatalogTrack_IsSent)
    {
        Rig  rig;

        rig.link.drives[0]   = Other (4);
        rig.link.present[1]  = false;
        rig.link.blankTracks = { FullSendDisk::kDosCatalogTrack };

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1 }));
        Assert::AreEqual ((size_t) 1, session->GetManifest().GetRows().size());
        Assert::IsTrue   (std::none_of (rig.link.reports.begin(), rig.link.reports.end(), [] (const std::string & line) { return line.find ("changed while it was read") != std::string::npos; }));
    }

    TEST_METHOD (Session_DriveKey_SendsTheListedDisk_EvenAsAnExactCopy)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Space, FullSendKey::Drive1 };

        auto  session = rig.Run();

        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1, 1 }));
        Assert::IsTrue   (rig.Files() == std::vector<std::string> ({ "0001.dsk", "0002.dsk", "manifest.fullsend" }));
        Assert::AreEqual (0, session->GetScreen().duplicates);
        Assert::AreEqual (std::string ("D1: SENT 2"), session->GetScreen().GetRows()[3]);
    }

    TEST_METHOD (Session_DriveKey_WithNoDiskListed_DoesNothing)
    {
        Rig  rig;

        rig.link.drives[0]  = Other (1);
        rig.link.present[1] = false;
        rig.link.keys       = { FullSendKey::Drive1 };

        rig.Run();

        Assert::IsTrue (rig.link.diskReads == std::vector<int> ({ 1 }));
    }

    TEST_METHOD (Session_Space_WithARepeatInBothDrives_ListsBoth)
    {
        Rig  rig;

        rig.link.drives[0] = Other (1);
        rig.link.drives[1] = Other (2);
        rig.link.keys      = { FullSendKey::Space, FullSendKey::Drive2 };

        rig.Run();

        //  The drive just sent isn't checked again before the first pause.
        Assert::AreEqual (std::string ("D1 MAY ALREADY HAVE BEEN SENT."), rig.link.pauseLines[0][0]);
        Assert::AreEqual (std::string ("D1: DUPE OF 0001.dsk?"), rig.link.screens[rig.link.screens.size() - 1].GetRows()[3]);
        Assert::AreEqual (std::string ("D1 AND D2 MAY ALREADY HAVE BEEN SENT."), rig.link.pauseLines[1][0]);
        Assert::AreEqual (std::string ("OR PRESS 1 OR 2 TO SEND D1 OR D2 ANYWAY."), rig.link.pauseLines[1][4]);
        Assert::IsTrue   (rig.link.diskReads == std::vector<int> ({ 1, 2, 2 }));
    }

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

        rig.link.drives[0]  = Dos ("HELLO");
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
        rig.link.drives[0]  = Dos ("HELLO");
        rig.link.drives[1]  = Other (2);

        auto  session = rig.Run();

        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\Box 3-0001 HELLO.dsk"));
        Assert::IsTrue (rig.io.files.contains ("C:\\Box\\Box 3-0002.dsk"));
        Assert::AreEqual (std::string ("Box 3-0001 HELLO.dsk"), session->GetManifest().GetRows()[0].file);
    }

    TEST_METHOD (Session_PrefixTypedOnTheApple_BeginsEachImagesName)
    {
        Rig  rig;

        rig.link.beginKey   = FullSendKey::Prefix;
        rig.link.keys       = { FullSendKey::Space };
        rig.link.typed      = { (Byte) 0xC2, (Byte) 0xCF, (Byte) 0xD8, (Byte) 0xDA, CassoLinkCodec::kKeyLeft, (Byte) 0xAD, CassoLinkCodec::kKeyReturn };
        rig.link.drives[0]  = Dos ("HELLO");
        rig.link.present[1] = false;

        auto  session = rig.Run();

        Assert::AreEqual (std::string ("BOX-"), session->GetPrefix());
        Assert::IsTrue   (rig.io.files.contains ("C:\\Box\\BOX-0001 HELLO.dsk"));
        Assert::AreEqual (std::string ("PREFIX: _"), rig.link.typedLines[0][1]);
        Assert::AreEqual (std::string ("PREFIX: BOX-_"), rig.link.typedLines.back()[1]);
        Assert::AreEqual (std::string ("P: FILE NAME PREFIX: BOX-"), rig.link.pauseLines[0][2]);
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
        Assert::AreEqual (std::string ("PREFIX: OLDX_"), rig.link.typedLines.back()[1]);
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
        Assert::AreEqual (std::string ("D1: SENT 2, DUPE OF 0001.dsk"), session->GetScreen().GetRows()[3]);

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
