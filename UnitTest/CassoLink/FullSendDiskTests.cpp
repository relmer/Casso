#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "FullSendTestDisks.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FullSendTestDisks;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskTests
//
//  FullSendManifest and FullSendDisk: the manifest, fingerprints and hashes,
//  and image titles.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendDiskTests)
{
public:

    //  The manifest.

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
        Image        image = Dos ("GAME");
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
        Assert::IsFalse (FullSendDisk::IsCatalogOnTrackZero (Dos ("GAME")));
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
        Assert::AreEqual (std::string ("0012 START"), FullSendDisk::MakeImageTitle (12, FullSendDisk::GetCatalogTitle (Dos ("START")), ""));
        Assert::AreEqual (std::string ("0013"),       FullSendDisk::MakeImageTitle (13, FullSendDisk::GetCatalogTitle (Dos ("HELLO")), ""), L"HELLO is no title: the boot screen has it");
        Assert::AreEqual (std::string ("0004 APPLE"), FullSendDisk::MakeImageTitle (4, FullSendDisk::GetCatalogTitle (pascalDisk), ""));
        Assert::AreEqual (std::string ("0005"),       FullSendDisk::MakeImageTitle (5, FullSendDisk::GetCatalogTitle (Other (9)), ""));
    }

    //  With HELLO first, the next program is the title, less a version at
    //  its end; a disk with HELLO alone has no title.
    TEST_METHOD (ProgramTitle_IsTheFirstProgramAfterHello_LessItsVersion)
    {
        Image        disk = Dos ("HELLO");
        Image        next;
        FilePayload  payload;



        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (100, 3);

        Assert::AreEqual (std::string(), FullSendDisk::GetProgramTitle (disk), L"HELLO alone");

        AssertSucceeded (Dos33Volume (disk).Write (FilePath::Parse ("WIZFIX.2"), payload, next));
        Assert::AreEqual (std::string ("WIZFIX"), FullSendDisk::GetProgramTitle (next));
        Assert::AreEqual (std::string ("GAME"),   FullSendDisk::GetProgramTitle (Dos ("GAME")));
        Assert::AreEqual (std::string(),          FullSendDisk::GetProgramTitle (Other (9)), L"no DOS 3.3 catalog");
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
};
