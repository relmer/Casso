#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/Dos33MapReader.h"
#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FileMapTestImages.h"
#include "RepairFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Images = FileMapTestImages;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepairTests
//
//  The file-system verdict and Repair on planted bookkeeping damage: OK and
//  nothing to repair on each undamaged volume, Bad or Suspect on each kind
//  of damage, and each repaired copy byte for byte the image before the
//  damage. A chain off the disk or a cross-link isn't repaired, and a
//  catalog DOS 3.3 doesn't write is non-standard rather than damaged.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskRepairTests)
{
public:

    static void Analyze (const vector<Byte> & image, bool isProDosOrder, DiskAnalysis & out)
    {
        Images::Analyze (image, isProDosOrder, out);
    }



    static vector<Byte> ToDosOrder (const vector<Byte> & image, bool isProDosOrder)
    {
        return isProDosOrder ? Images::ToDosOrder (image) : image;
    }



    static int CountFileSystemFindings (const DiskAnalysis & analysis, FindingWeight weight)
    {
        return static_cast<int> (std::count_if (analysis.findings.begin(), analysis.findings.end(), [&] (const Finding & f)
        {
            return f.category == FindingCategory::FileSystem && f.weight == weight;
        }));
    }



    static bool HasKind (const DiskAnalysis & analysis, FindingKind kind)
    {
        return std::any_of (analysis.findings.begin(), analysis.findings.end(), [&] (const Finding & f) { return f.kind == kind; });
    }



    static FileSystemVerdict VerdictOf (const vector<Byte> & image, bool isProDosOrder)
    {
        DiskAnalysis  analysis;



        Analyze (image, isProDosOrder, analysis);

        return analysis.verdict;
    }



    TEST_METHOD (EachUndamagedVolumeIsOkWithNothingToRepair)
    {
        for (bool isProDosOrder : { false, true })
        {
            for (const vector<Byte> & image : { RepairFixtures::MakeDos33(), RepairFixtures::MakeProDos(), RepairFixtures::MakePascal() })
            {
                DiskAnalysis  analysis;
                RepairPlan    plan;
                bool          isProDos = image == RepairFixtures::MakeProDos() || image == RepairFixtures::MakePascal();
                vector<Byte>  ordered  = (isProDos == isProDosOrder) ? image : (isProDos ? Images::ToDosOrder (image) : Images::ToProDosOrder (image));

                Analyze (ordered, isProDosOrder, analysis);
                plan = DiskRepair::Plan (analysis, {});

                Assert::IsTrue  (analysis.verdict == FileSystemVerdict::Ok, L"an undamaged volume is OK");
                Assert::AreEqual (0, CountFileSystemFindings (analysis, FindingWeight::Damage));
                Assert::AreEqual (0, CountFileSystemFindings (analysis, FindingWeight::Suspect));
                Assert::AreEqual (0, CountFileSystemFindings (analysis, FindingWeight::Note), L"and Findings doesn't list a file-system finding");
                Assert::IsFalse (plan.canRepair, L"Repair is unavailable");
            }
        }
    }



    TEST_METHOD (EachPlantedDamageIsBadAndANextTrackOffTheDiskIsSuspect)
    {
        vector<RepairFixture>  fixtures = RepairFixtures::MakeAll();



        Assert::AreEqual (static_cast<size_t> (10), fixtures.size());

        for (const RepairFixture & fixture : fixtures)
        {
            FileSystemVerdict  expected = (fixture.name == L"DOS 3.3 next track off the disk") ? FileSystemVerdict::Suspect : FileSystemVerdict::Bad;

            Assert::IsTrue (VerdictOf (fixture.damaged, fixture.isProDosOrder) == expected, fixture.name.c_str());
            Assert::IsTrue (VerdictOf (fixture.before,  fixture.isProDosOrder) == FileSystemVerdict::Ok, fixture.name.c_str());
        }
    }



    TEST_METHOD (EachRepairedCopyIsTheImageBeforeTheDamage)
    {
        int  repaired = 0;



        for (const RepairFixture & fixture : RepairFixtures::MakeAll())
        {
            DiskAnalysis  analysis;
            RepairPlan    plan;
            vector<Byte>  copy     = ToDosOrder (fixture.damaged, fixture.isProDosOrder);
            vector<Byte>  expected = ToDosOrder (fixture.before,  fixture.isProDosOrder);

            if (!fixture.canRepair)
            {
                continue;
            }

            Analyze (fixture.damaged, fixture.isProDosOrder, analysis);
            plan = DiskRepair::Plan (analysis, {});

            Assert::IsTrue  (plan.canRepair, (fixture.name + L": " + plan.reason).c_str());
            Assert::IsFalse (plan.changes.empty(), fixture.name.c_str());
            Assert::IsTrue  (plan.after == FileSystemVerdict::Ok, fixture.name.c_str());

            DiskRepair::Apply (plan, copy);

            Assert::IsTrue (copy == expected, (fixture.name + L": the copy is the image before the damage").c_str());
            Assert::IsTrue (VerdictOf (copy, false) == FileSystemVerdict::Ok, fixture.name.c_str());
            repaired++;
        }

        Assert::AreEqual (8, repaired, L"every repairable fixture was repaired");
    }



    TEST_METHOD (AChainOffTheDiskOrACrossLinkIsNotRepaired)
    {
        for (const RepairFixture & fixture : { RepairFixtures::MakeDos33CrossLink(), RepairFixtures::MakeDos33ChainOffDisk() })
        {
            DiskAnalysis  analysis;
            RepairPlan    plan;

            Analyze (fixture.damaged, fixture.isProDosOrder, analysis);
            plan = DiskRepair::Plan (analysis, {});

            Assert::IsFalse (plan.canRepair, fixture.name.c_str());
            Assert::IsTrue  (plan.reason.find (L"Repairing this would change a file's data: ") != std::wstring::npos, plan.reason.c_str());
            Assert::IsTrue  (plan.changes.empty() && plan.sectors.empty());
        }
    }



    TEST_METHOD (TheChangesHaveALineEach)
    {
        DiskAnalysis  bitmap;
        DiskAnalysis  loop;
        DiskAnalysis  header;
        DiskAnalysis  count;
        RepairPlan    plan;



        Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, bitmap);
        plan = DiskRepair::Plan (bitmap, {});
        Assert::AreEqual (static_cast<size_t> (2), plan.changes.size());
        Assert::AreEqual (std::wstring (L"Mark 1 sector of HELLO as used in the bitmap"),     plan.changes[0]);
        Assert::AreEqual (std::wstring (L"Mark 1 sector not in any file as free in the bitmap"),   plan.changes[1]);

        Analyze (RepairFixtures::MakeDos33CatalogLoop().damaged, false, loop);
        plan = DiskRepair::Plan (loop, {});
        Assert::AreEqual (static_cast<size_t> (1), plan.changes.size());
        Assert::AreEqual (std::wstring (L"End the catalog at sector $1"), plan.changes[0]);

        Analyze (RepairFixtures::MakeDos33HeaderZeroed().damaged, false, header);
        Assert::IsTrue (HasKind (header, FindingKind::Dos33HeaderLost));
        plan = DiskRepair::Plan (header, {});
        Assert::AreEqual (static_cast<size_t> (1), plan.changes.size());
        Assert::AreEqual (std::wstring (L"Set the VTOC header to DOS 3.3's standard values"), plan.changes[0]);

        Analyze (RepairFixtures::MakeDos33CountDiffers().damaged, false, count);
        plan = DiskRepair::Plan (count, {});
        Assert::AreEqual (static_cast<size_t> (1), plan.changes.size());
        Assert::AreEqual (std::wstring (L"Set the catalog sector count of HELLO to 4"), plan.changes[0]);
    }



    TEST_METHOD (ABlocksUsedCountThatDiffersIsRepaired)
    {
        DiskAnalysis  analysis;
        RepairPlan    plan;



        Analyze (RepairFixtures::MakeProDosBlocksUsed().damaged, true, analysis);
        plan = DiskRepair::Plan (analysis, {});

        Assert::IsTrue   (analysis.verdict == FileSystemVerdict::Bad);
        Assert::AreEqual (static_cast<size_t> (1), plan.changes.size());
        Assert::AreEqual (std::wstring (L"Set the blocks-used count of SEED to 1"), plan.changes[0]);
    }



    TEST_METHOD (ACrossLinkedCpmVolumeHasTheCrossLinkAsItsReason)
    {
        vector<Byte>  shared = Images::MakeCpm();
        vector<Byte>  odd    = Images::MakeCpm();
        DiskAnalysis  analysis;
        RepairPlan    plan;



        //  NOTES.TXT takes SPARSE.DAT's first block.
        Images::GetCpmSector (shared, 3, 0)[3 * 32 + 16] = 20;
        Analyze (shared, false, analysis);
        plan = DiskRepair::Plan (analysis, {});

        Assert::IsTrue  (analysis.verdict == FileSystemVerdict::Bad);
        Assert::IsFalse (plan.canRepair);
        Assert::IsTrue  (plan.reason.starts_with (L"Repairing this would change a file's data: "), plan.reason.c_str());

        //  An entry with user 40: Suspect, and no cross-link to give.
        Images::GetCpmSector (odd, 3, 0)[6 * 32] = 40;
        Analyze (odd, false, analysis);
        plan = DiskRepair::Plan (analysis, {});

        Assert::IsTrue   (analysis.verdict == FileSystemVerdict::Suspect);
        Assert::AreEqual (std::wstring (L"Only DOS 3.3, ProDOS and Apple Pascal volumes can be repaired."), plan.reason);
    }



    //  The finding for a planted change, its verdict, and, for a change
    //  Repair sets right, the copy byte for byte the image before it.
    static void AssertPlanted (const vector<Byte> & before, const vector<Byte> & damaged, bool isProDosOrder, FindingKind kind,
                               FileSystemVerdict verdict, const wchar_t * change)
    {
        DiskAnalysis  analysis;
        RepairPlan    plan;
        vector<Byte>  copy     = ToDosOrder (damaged, isProDosOrder);



        Analyze (damaged, isProDosOrder, analysis);
        plan = DiskRepair::Plan (analysis, {});

        Assert::IsTrue (HasKind (analysis, kind), change);
        Assert::IsTrue (analysis.verdict == verdict, change);

        if (*change == L'\0')
        {
            Assert::IsFalse (plan.canRepair, plan.reason.c_str());
        }
        else
        {
            Assert::IsTrue   (plan.canRepair, plan.reason.c_str());
            Assert::IsTrue   (std::find (plan.changes.begin(), plan.changes.end(), std::wstring (change)) != plan.changes.end(), change);
            DiskRepair::Apply (plan, copy);
            Assert::IsTrue   (copy == ToDosOrder (before, isProDosOrder), change);
        }
    }



    TEST_METHOD (AFileThatUsesTheVolumesStructureIsDamage)
    {
        vector<Byte>  dos    = RepairFixtures::MakeDos33();
        vector<Byte>  prodos = RepairFixtures::MakeProDos();
        vector<Byte>  apple  = RepairFixtures::MakePascal();



        //  HELLO's first data sector is catalog sector $F.
        RepairFixtures::GetDosSector (dos, 18, 15)[0x0C] = 17;
        RepairFixtures::GetDosSector (dos, 18, 15)[0x0D] = 15;
        AssertPlanted (RepairFixtures::MakeDos33(), dos, false, FindingKind::FileUsesStructure, FileSystemVerdict::Bad, L"");

        //  SEED's block is the volume bitmap.
        Images::GetBlock (prodos, 2)[4 + 0x27 + 0x11] = 6;
        AssertPlanted (RepairFixtures::MakeProDos(), prodos, true, FindingKind::FileUsesStructure, FileSystemVerdict::Bad, L"");

        //  SYSTEM.PASCAL starts in the directory, at block 4.
        Images::GetBlock (apple, 2)[26] = 4;
        AssertPlanted (RepairFixtures::MakePascal(), apple, true, FindingKind::FileUsesStructure, FileSystemVerdict::Bad, L"");
    }



    TEST_METHOD (ACatalogSectorMarkedFreeIsRepaired)
    {
        vector<Byte>  image = RepairFixtures::MakeDos33();
        Byte *        bits  = RepairFixtures::GetDosSector (image, 17, 0) + 0x38 + 17 * 4;



        bits[0] |= 0x40;
        AssertPlanted (RepairFixtures::MakeDos33(), image, false, FindingKind::Dos33StructureMarkedFree, FileSystemVerdict::Bad,
                       L"Mark 1 VTOC and catalog sector as used in the bitmap");
    }



    TEST_METHOD (ProDosPointersAndTheBitmapsEndAreRepaired)
    {
        vector<Byte>  pointer = RepairFixtures::MakeProDos();
        vector<Byte>  parent  = RepairFixtures::MakeProDos();
        vector<Byte>  end     = RepairFixtures::MakeProDos();



        //  SEED's entry points to block 3 for its directory.
        Images::GetBlock (pointer, 2)[4 + 0x27 + 0x25] = 3;
        AssertPlanted (RepairFixtures::MakeProDos(), pointer, true, FindingKind::ProDosDirectoryPointer, FileSystemVerdict::Suspect,
                       L"Set the directory pointer of SEED to 2");

        //  DIR1's header has entry 9 of block 2 as its parent entry.
        Images::GetBlock (parent, 20)[0x29] = 9;
        AssertPlanted (RepairFixtures::MakeProDos(), parent, true, FindingKind::ProDosParentPointer, FileSystemVerdict::Suspect,
                       L"Set the parent entry number of DIR1 to 7");

        //  Eight blocks past the volume's 280 marked free.
        Images::GetBlock (end, 6)[40] = 0xFF;
        AssertPlanted (RepairFixtures::MakeProDos(), end, true, FindingKind::ProDosBitmapPastEnd, FileSystemVerdict::Suspect,
                       L"Mark 8 blocks past the end of the volume as used in the bitmap");
    }



    TEST_METHOD (ProDosAndPascalEntriesAreChecked)
    {
        vector<Byte>  eof        = RepairFixtures::MakeProDos();
        vector<Byte>  name       = RepairFixtures::MakeProDos();
        vector<Byte>  lastBytes  = RepairFixtures::MakePascal();
        vector<Byte>  pascalName = RepairFixtures::MakePascal();



        //  SEED, a seedling file, with an end of file of 1,000.
        Images::GetBlock (eof, 2)[4 + 0x27 + 0x15] = 0xE8;
        Images::GetBlock (eof, 2)[4 + 0x27 + 0x16] = 0x03;
        AssertPlanted (RepairFixtures::MakeProDos(), eof, true, FindingKind::ProDosEofPastStorage, FileSystemVerdict::Bad, L"");

        //  SEED's name starting with a digit.
        Images::GetBlock (name, 2)[4 + 0x27 + 1] = '1';
        AssertPlanted (RepairFixtures::MakeProDos(), name, true, FindingKind::ProDosNameInvalid, FileSystemVerdict::Suspect, L"");

        //  NOTES.TEXT's last block with 600 bytes.
        Images::GetBlock (lastBytes, 2)[2 * 26 + 22] = 0x58;
        Images::GetBlock (lastBytes, 2)[2 * 26 + 23] = 0x02;
        AssertPlanted (RepairFixtures::MakePascal(), lastBytes, true, FindingKind::PascalLastBytesInvalid, FileSystemVerdict::Suspect, L"");

        //  NOTES.TEXT with a $ in its name.
        Images::GetBlock (pascalName, 2)[2 * 26 + 7] = '$';
        AssertPlanted (RepairFixtures::MakePascal(), pascalName, true, FindingKind::PascalNameInvalid, FileSystemVerdict::Suspect, L"");
    }



    TEST_METHOD (ACopyProtectedDiskIsNotRepaired)
    {
        DiskAnalysis  analysis;
        Finding       sign;



        Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, analysis);
        Assert::IsFalse (FileSystemCheck::IsCopyProtected (analysis), L"a disk made from a sector image has no protection");

        sign.kind = FindingKind::AddressEpilogueNonstandard;
        analysis.findings.push_back (sign);

        Assert::IsTrue   (FileSystemCheck::IsCopyProtected (analysis));
        Assert::AreEqual (std::wstring (L"The disk is copy-protected."), DiskRepair::Plan (analysis, {}).reason);
    }



    TEST_METHOD (RandomBitsOnAQuarterOfTheTracksAreCopyProtection)
    {
        DiskAnalysis  analysis;
        Finding       bits;



        Analyze (RepairFixtures::MakeDos33(), false, analysis);
        bits.kind = FindingKind::RandomBitsOnFormattedTrack;

        for (int track = 0; track < 8; track++)
        {
            bits.quarterTrack = track * 4;
            analysis.findings.push_back (bits);
        }

        Assert::IsFalse (FileSystemCheck::IsCopyProtected (analysis), L"8 of 35 tracks, as a capture has a few");

        bits.quarterTrack = 8 * 4;
        analysis.findings.push_back (bits);
        Assert::IsTrue (FileSystemCheck::IsCopyProtected (analysis), L"9 of 35 tracks");
    }



    TEST_METHOD (SectorsNoFileUsesStayUsedWhenKept)
    {
        RepairFixture  fixture  = RepairFixtures::MakeDos33Bitmap();
        DiskAnalysis   analysis;
        RepairPlan     plan;
        vector<Byte>   copy     = fixture.damaged;
        RepairOptions  options;
        Byte           bits     = 0;



        options.isKeepingUnowned = true;
        Analyze (fixture.damaged, false, analysis);
        plan = DiskRepair::Plan (analysis, options);

        Assert::IsTrue   (plan.canRepair);
        Assert::AreEqual (static_cast<size_t> (1), plan.changes.size());
        DiskRepair::Apply (plan, copy);

        bits = RepairFixtures::GetDosSector (copy, 17, 0)[0x38 + RepairFixtures::kUnusedTrack * 4 + 1];
        Assert::AreEqual (0, (bits >> RepairFixtures::kUnusedSector) & 1, L"the sector no file uses is marked used");
        Assert::IsTrue   (VerdictOf (copy, false) == FileSystemVerdict::Ok, L"a sector no file uses is a note");
    }



    TEST_METHOD (ACatalogDos33DoesntWriteIsNonstandardNotDamaged)
    {
        vector<Byte>  image   = RepairFixtures::MakeDos33();
        Byte *        catalog = RepairFixtures::GetDosSector (image, 17, 15);
        DiskAnalysis  analysis;



        //  BIG's type $03 and SPARSE's name with its high bits clear, as a
        //  protection writes them.
        catalog[0x0B + 0x23 + 0x02] = 0x03;

        for (int i = 0; i < 6; i++)
        {
            catalog[0x0B + 2 * 0x23 + 0x03 + i] &= 0x7F;
        }

        Analyze (image, false, analysis);

        Assert::IsTrue   (HasKind (analysis, FindingKind::Dos33CatalogNonstandard));
        Assert::IsTrue   (analysis.verdict == FileSystemVerdict::Suspect);
        Assert::AreEqual (0, CountFileSystemFindings (analysis, FindingWeight::Damage), L"its files aren't counted as damage");
        Assert::IsTrue   (analysis.fileMaps[0].files.empty(), L"and aren't followed");
        Assert::IsFalse  (DiskRepair::Plan (analysis, {}).canRepair);
    }



    TEST_METHOD (OneOddEntryInACatalogIsReportedOnItsOwn)
    {
        vector<Byte>  image = RepairFixtures::MakeDos33();
        DiskAnalysis  analysis;



        //  Seven more entries, so one odd one in ten is under 15%.
        for (int e = 0; e < 7; e++)
        {
            Byte *  entry = RepairFixtures::GetDosSector (image, 17, 14) + 0x0B + e * 0x23;

            memcpy (entry, RepairFixtures::GetDosSector (image, 17, 15) + 0x0B, 0x23);
            entry[0x03 + 1] = static_cast<Byte> (0xC1 + e);
        }

        RepairFixtures::GetDosSector (image, 17, 15)[0x0B + 0x23 + 0x02] = 0x03;
        Analyze (image, false, analysis);

        Assert::IsFalse (HasKind (analysis, FindingKind::Dos33CatalogNonstandard));
        Assert::IsTrue  (HasKind (analysis, FindingKind::Dos33EntryTypeUnknown));
    }



    TEST_METHOD (TheVtocHeaderIsChecked)
    {
        struct Case { int offset; Byte value; FindingKind kind; FileSystemVerdict verdict; };

        const Case  cases[] =
        {
            { 0x27, 100,  FindingKind::Dos33PairsDiffer,       FileSystemVerdict::Suspect },
            { 0x37, 0x02, FindingKind::Dos33SectorBytesDiffer, FileSystemVerdict::Suspect },
            { 0x31, 0x00, FindingKind::Dos33NextTrackOffDisk,  FileSystemVerdict::Suspect },
            { 0x03, 2,    FindingKind::Dos33ReleaseDiffers,    FileSystemVerdict::Ok      },
            { 0x3A, 0x01, FindingKind::Dos33BitmapPadding,     FileSystemVerdict::Ok      },
        };



        for (const Case & c : cases)
        {
            vector<Byte>  image = RepairFixtures::MakeDos33();
            DiskAnalysis  analysis;

            RepairFixtures::GetDosSector (image, 17, 0)[c.offset] = c.value;
            Analyze (image, false, analysis);

            Assert::IsTrue (HasKind (analysis, c.kind), std::format (L"offset ${:02X}", c.offset).c_str());
            Assert::IsTrue (analysis.verdict == c.verdict, std::format (L"offset ${:02X}", c.offset).c_str());
        }
    }



    TEST_METHOD (ACountIsComparedOneEitherWayAndZeroIsNotCompared)
    {
        for (Byte recorded : { Byte (3), Byte (5), Byte (0) })
        {
            vector<Byte>  image = RepairFixtures::MakeDos33();
            DiskAnalysis  analysis;

            RepairFixtures::GetDosSector (image, 17, 15)[0x0B + 0x21] = recorded;
            Analyze (image, false, analysis);

            Assert::IsFalse (HasKind (analysis, FindingKind::FileCountDiffers), std::format (L"a count of {}", recorded).c_str());
        }
    }



    TEST_METHOD (AStoredLengthOrAListOffsetThatDiffersIsANote)
    {
        vector<Byte>  length  = RepairFixtures::MakeDos33();
        vector<Byte>  offset  = RepairFixtures::MakeDos33();
        DiskAnalysis  lengthAnalysis;
        DiskAnalysis  offsetAnalysis;



        //  HELLO's stored length of 2,000 bytes takes 8 sectors; it has 3.
        RepairFixtures::GetDosSector (length, 18, 14)[0] = 0xD0;
        RepairFixtures::GetDosSector (length, 18, 14)[1] = 0x07;
        Analyze (length, false, lengthAnalysis);
        Assert::IsTrue (HasKind (lengthAnalysis, FindingKind::Dos33StoredLengthDiffers));
        Assert::IsTrue (lengthAnalysis.verdict == FileSystemVerdict::Ok);

        //  BIG's first list starts at sector 5 of the file.
        RepairFixtures::GetDosSector (offset, 18, 11)[0x05] = 5;
        Analyze (offset, false, offsetAnalysis);
        Assert::IsTrue (HasKind (offsetAnalysis, FindingKind::Dos33ListOffsetDiffers));
        Assert::IsTrue (offsetAnalysis.verdict == FileSystemVerdict::Ok);
    }



    TEST_METHOD (AFileOfOneByteIsSuspectAndAnAlmostBlankImageIsDamage)
    {
        vector<Byte>  repeated = RepairFixtures::MakeDos33();
        vector<Byte>  blank    = Images::MakeDos33();
        DiskAnalysis  base;
        DiskAnalysis  repeatedAnalysis;
        DiskAnalysis  blankAnalysis;



        for (int s : { 14, 13, 12 })
        {
            std::fill (RepairFixtures::GetDosSector (repeated, 18, s), RepairFixtures::GetDosSector (repeated, 18, s) + 256, Byte (0));
        }

        Analyze (repeated, false, repeatedAnalysis);
        Assert::IsTrue (HasKind (repeatedAnalysis, FindingKind::FileRepeatedByte));
        Assert::IsTrue (repeatedAnalysis.verdict == FileSystemVerdict::Suspect);

        //  Every file's data sectors zeroed: only the VTOC, the catalog and
        //  the lists aren't blank.
        Analyze (blank, false, base);

        for (const MappedFile & file : base.fileMaps[0].files)
        {
            for (const FilePlace & place : file.sectors)
            {
                if (place.cell >= 0 && place.role == SectorRole::FileData)
                {
                    std::fill (RepairFixtures::GetDosSector (blank, place.cell / 16, place.cell % 16), RepairFixtures::GetDosSector (blank, place.cell / 16, place.cell % 16) + 256, Byte (0));
                }
            }
        }

        Analyze (blank, false, blankAnalysis);
        Assert::IsTrue (HasKind (blankAnalysis, FindingKind::ImageAlmostBlank));
        Assert::IsTrue (blankAnalysis.verdict == FileSystemVerdict::Bad);
    }



    TEST_METHOD (AnEmptyProDosFileWithABlankBlockIsNotDamage)
    {
        vector<Byte>  image = RepairFixtures::MakeProDos();
        Byte *        seed  = Images::GetBlock (image, 2) + 4 + 0x27;
        DiskAnalysis  analysis;



        //  SEED's end of file is 0, and its block is zeros, as a file has
        //  after ProDOS's CREATE.
        seed[0x15] = 0;
        std::fill (Images::GetBlock (image, 7), Images::GetBlock (image, 7) + Images::kBlockBytes, Byte (0));
        Analyze (image, true, analysis);

        Assert::IsFalse (HasKind (analysis, FindingKind::FileRepeatedByte));
        Assert::IsTrue  (analysis.verdict == FileSystemVerdict::Ok);
    }



    TEST_METHOD (ADiskWithNoStandardFileSystemIsNotRepaired)
    {
        vector<Byte>  image (Images::kImageBytes, 0);
        DiskAnalysis  analysis;
        RepairPlan    plan;



        Analyze (image, false, analysis);
        plan = DiskRepair::Plan (analysis, {});

        Assert::IsTrue  (analysis.verdict == FileSystemVerdict::None);
        Assert::AreEqual (std::wstring (L"No standard file system"), FileSystemCheck::FormatVerdict (analysis.verdict));
        Assert::AreEqual (0, CountFileSystemFindings (analysis, FindingWeight::Damage));
        Assert::IsFalse (plan.canRepair);
    }



    TEST_METHOD (TheVerdictFindingIsTheFirstOfDamage)
    {
        DiskAnalysis     analysis;
        const Finding *  finding = nullptr;



        Analyze (RepairFixtures::MakeDos33Bitmap().damaged, false, analysis);
        finding = FileSystemCheck::GetVerdictFinding (analysis);

        Assert::IsNotNull (finding);
        Assert::IsTrue    (finding->weight == FindingWeight::Damage);
        Assert::IsTrue    (finding->kind == FindingKind::FileOwnedMarkedFree);
    }



    TEST_METHOD (TheCopyIsWrittenBesideTheImageAndTheImageIsNotChanged)
    {
        FakeDiskFileIo  io;
        RepairFixture   fixture   = RepairFixtures::MakeDos33Bitmap();
        DiskAnalysis    analysis;
        RepairPlan      plan;
        std::string     path      = "C:\\Disks\\Game.dsk";
        std::string     written;
        std::wstring    reason;
        HRESULT         hr        = S_OK;



        io.files[path] = fixture.damaged;
        Analyze (fixture.damaged, false, analysis);
        plan = DiskRepair::Plan (analysis, {});

        hr = RepairCopy::Write (io, path, "", plan, written, reason);
        Assert::IsTrue   (SUCCEEDED (hr), reason.c_str());
        Assert::AreEqual (std::string ("C:\\Disks\\Game (repaired).dsk"), written);
        Assert::IsTrue   (io.files[written] == fixture.before, L"the copy is the image before the damage");
        Assert::IsTrue   (io.files[path] == fixture.damaged, L"the image is as it was");

        hr = RepairCopy::Write (io, path, "", plan, written, reason);
        Assert::IsTrue   (SUCCEEDED (hr), reason.c_str());
        Assert::AreEqual (std::string ("C:\\Disks\\Game (repaired 2).dsk"), written, L"a number when the name exists");

        hr = RepairCopy::Write (io, path, written, plan, written, reason);
        Assert::IsTrue   (FAILED (hr), L"a copy path that exists isn't replaced");
    }



    TEST_METHOD (TheCopyOfAProDosOrderImageIsInProDosOrder)
    {
        FakeDiskFileIo  io;
        RepairFixture   fixture   = RepairFixtures::MakePascalCount();
        DiskAnalysis    analysis;
        RepairPlan      plan;
        std::string     path      = "C:\\Disks\\Pascal.po";
        std::string     written;
        std::wstring    reason;
        HRESULT         hr        = S_OK;



        io.files[path] = fixture.damaged;
        Analyze (fixture.damaged, true, analysis);
        plan = DiskRepair::Plan (analysis, {});

        hr = RepairCopy::Write (io, path, "", plan, written, reason);
        Assert::IsTrue   (SUCCEEDED (hr), reason.c_str());
        Assert::AreEqual (std::string ("C:\\Disks\\Pascal (repaired).po"), written);
        Assert::IsTrue   (io.files[written] == fixture.before);
    }
};
