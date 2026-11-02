#include "Pch.h"

#include "Machines/Apple2/Common/NativeListing.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NativeListingTests
//
//  The rows are the formats verified on the Apple: the Filer's Extended list
//  and STAT *.*.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NativeListingTests)
{
public:

    static FileEntry  MakePascalFile (const char * name, int first, uint32_t blocks, Byte kind, int day, int month, int year)
    {
        FileEntry  entry;

        entry.name           = name;
        entry.firstBlock     = first;
        entry.sizeUnits      = blocks;
        entry.lastBlockBytes = 512;
        entry.type           = kind;
        entry.hasModified    = year != 0 && ProDosVolume::TryToUnixTime ((Word) ((year << 9) | (month << 5) | day), 0, entry.modifiedUnix);

        return entry;
    }

    static FileEntry  MakeCpmFile (const char * name, int user, uint32_t records, uint32_t blocks, uint32_t extents = 1)
    {
        FileEntry  entry;

        entry.name      = name;
        entry.user      = user;
        entry.records   = records;
        entry.sizeUnits = blocks;
        entry.extents   = extents;

        return entry;
    }

    TEST_METHOD (PascalMatchesTheFilersExtendedList)
    {
        VolumeListing             listing;
        std::vector<std::string>  lines;

        listing.volumeName     = "APPLE3";
        listing.totalUnits     = 280;
        listing.freeUnits      = 6;
        listing.directoryFiles = 3;
        listing.entries        = { MakePascalFile ("SYSTEM.APPLE",  6,  50,  PascalVolume::kKindData, 25, 12, 83),
                                   MakePascalFile ("LINEFEED.TEXT", 56, 4,   PascalVolume::kKindText, 17, 5,  82),
                                   MakePascalFile ("TAIL.CODE",     60, 214, PascalVolume::kKindCode, 0,  0,  0) };

        listing.entries[0].sizeUnits = 32;

        NativeListing::FormatPascal (listing, lines);

        Assert::AreEqual ((size_t) 7, lines.size());
        Assert::AreEqual (std::string ("APPLE3:"), lines[0]);
        Assert::AreEqual (std::string ("SYSTEM.APPLE      32 25-Dec-83     6   512  Datafile"), lines[1]);
        Assert::AreEqual (std::string ("< UNUSED >        18              38"), lines[2]);
        Assert::AreEqual (std::string ("LINEFEED.TEXT      4 17-May-82    56   512  Textfile"), lines[3]);
        Assert::AreEqual (std::string ("TAIL.CODE        214              60   512  Codefile"), lines[4]);
        Assert::AreEqual (std::string ("< UNUSED >         6             274"), lines[5]);
        Assert::AreEqual (std::string ("3/3 files<listed/in-dir>, 274 blocks used, 6 unused, 18 in largest"), lines[6]);
    }

    TEST_METHOD (PascalCountsWhatTheDirectoryListsButTheListingCannot)
    {
        VolumeListing             listing;
        std::vector<std::string>  lines;

        listing.volumeName     = "DAMAGED";
        listing.totalUnits     = 280;
        listing.freeUnits      = 274;
        listing.directoryFiles = 2;

        NativeListing::FormatPascal (listing, lines);

        Assert::AreEqual (std::string ("0/2 files<listed/in-dir>, 6 blocks used, 274 unused, 274 in largest"), lines.back());
    }

    TEST_METHOD (EveryPascalKindHasAWord)
    {
        Assert::AreEqual ("Badblocks", NativeListing::GetPascalKindName (PascalVolume::kKindBad));
        Assert::AreEqual ("Codefile",  NativeListing::GetPascalKindName (PascalVolume::kKindCode));
        Assert::AreEqual ("Textfile",  NativeListing::GetPascalKindName (PascalVolume::kKindText));
        Assert::AreEqual ("Infofile",  NativeListing::GetPascalKindName (PascalVolume::kKindInfo));
        Assert::AreEqual ("Datafile",  NativeListing::GetPascalKindName (PascalVolume::kKindData));
        Assert::AreEqual ("Graffile",  NativeListing::GetPascalKindName (PascalVolume::kKindGraf));
        Assert::AreEqual ("Fotofile",  NativeListing::GetPascalKindName (PascalVolume::kKindFoto));
    }

    TEST_METHOD (CpmMatchesStat)
    {
        VolumeListing             listing;
        std::vector<std::string>  lines;

        listing.freeUnits = 3;
        listing.entries   = { MakeCpmFile ("PIP.COM", 0, 56, 7), MakeCpmFile ("ASM.COM", 0, 64, 8) };

        listing.entries[0].isReadOnly = true;
        listing.entries[0].isSystem   = true;

        NativeListing::FormatCpm (listing, lines);

        Assert::AreEqual ((size_t) 4, lines.size());
        Assert::AreEqual (std::string ("Recs  Bytes  Ext Acc"), lines[0]);
        Assert::AreEqual (std::string ("  64     8k    1 R/W A:ASM.COM"), lines[1], L"In name order");
        Assert::AreEqual (std::string ("  56     7k    1 R/O (A:PIP.COM)"), lines[2], L"A system file is in parentheses");
        Assert::AreEqual (std::string ("Bytes Remaining On A: 3k"), lines[3]);
    }

    TEST_METHOD (CpmNameOrderIsThePaddedDirectoryOrder)
    {
        VolumeListing             listing;
        std::vector<std::string>  lines;

        listing.entries = { MakeCpmFile ("A2.COM", 0, 1, 1), MakeCpmFile ("A.TXT", 0, 1, 1) };

        NativeListing::FormatCpm (listing, lines);

        Assert::IsTrue (lines[1].ends_with ("A:A.TXT"), L"A space sorts before a digit");
        Assert::IsTrue (lines[2].ends_with ("A:A2.COM"));
    }

    TEST_METHOD (CpmOtherUsersFollowUnderTheirOwnHeadings)
    {
        VolumeListing             listing;
        std::vector<std::string>  lines;

        listing.freeUnits = 3;
        listing.entries   = { MakeCpmFile ("31:cp/am.sys", 31, 96, 12), MakeCpmFile ("2:OTHER", 2, 8, 1), MakeCpmFile ("PIP.COM", 0, 56, 7) };

        NativeListing::FormatCpm (listing, lines);

        Assert::AreEqual ((size_t) 11, lines.size());
        Assert::AreEqual (std::string ("  56     7k    1 R/W A:PIP.COM"), lines[1]);
        Assert::AreEqual (std::string ("Bytes Remaining On A: 3k"), lines[2]);
        Assert::IsTrue   (lines[3].empty());
        Assert::AreEqual (std::string ("USER 2"), lines[4]);
        Assert::AreEqual (std::string ("Recs  Bytes  Ext Acc"), lines[5]);
        Assert::AreEqual (std::string ("   8     1k    1 R/W A:OTHER"), lines[6]);
        Assert::IsTrue   (lines[7].empty());
        Assert::AreEqual (std::string ("USER 31"), lines[8]);
        Assert::AreEqual (std::string ("  96    12k    1 R/W A:cp/am.sys"), lines[10]);
    }

    TEST_METHOD (CpmWithNothingInUserZero)
    {
        VolumeListing             listing;
        std::vector<std::string>  lines;

        listing.freeUnits = 126;

        NativeListing::FormatCpm (listing, lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string ("File Not Found"), lines[0]);
        Assert::AreEqual (std::string ("Bytes Remaining On A: 126k"), lines[1]);
    }
};
