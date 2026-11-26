#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibbleFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33CatalogTests
//
//  The catalog chain over an in-memory formatted volume: finding slots,
//  writing and tombstoning entries, lookups by file name, and file name encoding.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33CatalogTests)
{
public:

    static constexpr Byte      kVolumeNumber       = 254;
    static constexpr int       kCatalogTrack       = 17;
    static constexpr int       kCatalogFirstSector = 15;
    static constexpr size_t    kEntryBase          = 0x0B;
    static constexpr size_t    kEntrySize          = 0x23;
    static constexpr int       kListTrack          = 18;
    static constexpr int       kListSector         = 15;
    static constexpr Byte      kTypeBinary         = 0x04;
    static constexpr uint16_t  kSectorCount        = 2;

    static vector<Byte> MakeFormattedVolume()
    {
        vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));

        return buffer;
    }

    static void AddEntry (vector<Byte> & buffer, const string & name, uint16_t sectorCount)
    {
        vector<Byte>       source  = buffer;
        Dos33SectorAccess  access (source);
        Dos33Catalog       catalog (access);
        vector<Byte>       nameBytes;
        int                track   = 0;
        int                sector  = 0;
        size_t             offset  = 0;

        Assert::IsTrue (Dos33Catalog::TryEncodeName (name, nameBytes));
        Assert::IsTrue (catalog.TryFindFreeSlot (track, sector, offset));

        catalog.WriteEntry (buffer, track, sector, offset, access.ToUnit (kListTrack, kListSector),
                            kTypeBinary, sectorCount, nameBytes);
    }

    static vector<Dos33CatalogEntry> ListEntries (const vector<Byte> & buffer, bool & outFullyParsed)
    {
        Dos33SectorAccess          access (buffer);
        Dos33Catalog               catalog (access);
        vector<Dos33CatalogEntry>  entries;
        vector<std::string>        damage;

        catalog.CollectEntries (entries, damage, outFullyParsed);

        return entries;
    }

    TEST_METHOD (TryFindFreeSlot_OnABlankVolume_IsTheFirstSlotOfTheFirstSector)
    {
        vector<Byte>       buffer  = MakeFormattedVolume();
        Dos33SectorAccess  access (buffer);
        Dos33Catalog       catalog (access);
        int                track   = 0;
        int                sector  = 0;
        size_t             offset  = 0;

        Assert::IsTrue (catalog.TryFindFreeSlot (track, sector, offset));
        Assert::AreEqual (kCatalogTrack, track);
        Assert::AreEqual (kCatalogFirstSector, sector);
        Assert::AreEqual (kEntryBase, offset);
    }

    TEST_METHOD (WriteEntry_IsReadBackByCollectEntries)
    {
        vector<Byte>               buffer      = MakeFormattedVolume();
        bool                       fullyParsed = false;
        vector<Dos33CatalogEntry>  entries;

        AddEntry (buffer, "prog", kSectorCount);

        entries = ListEntries (buffer, fullyParsed);

        Assert::IsTrue (fullyParsed);
        Assert::AreEqual (size_t (1), entries.size());
        Assert::AreEqual (string ("PROG"), entries[0].name, L"the catalog stores upper case");
        Assert::AreEqual (kListTrack,  entries[0].tsListTrack);
        Assert::AreEqual (kListSector, entries[0].tsListSector);
        Assert::AreEqual (kTypeBinary, entries[0].typeByte);
        Assert::AreEqual (kSectorCount, entries[0].sectorCount);
    }

    TEST_METHOD (TryFindFreeSlot_PassesOverAnEntryOccupyingNoSectors)
    {
        vector<Byte>       buffer  = MakeFormattedVolume();
        int                track   = 0;
        int                sector  = 0;
        size_t             offset  = 0;

        AddEntry (buffer, "HEADING", 0);

        {
            Dos33SectorAccess  access (buffer);
            Dos33Catalog       catalog (access);

            Assert::IsTrue (catalog.TryFindFreeSlot (track, sector, offset));
        }

        Assert::AreEqual (kEntryBase + kEntrySize, offset, L"a heading entry occupies its slot");
    }

    TEST_METHOD (WriteTombstone_RemovesTheEntryAndFreesItsSlot)
    {
        vector<Byte>               buffer      = MakeFormattedVolume();
        vector<Byte>               result;
        bool                       fullyParsed = false;
        vector<Dos33CatalogEntry>  entries;
        int                        track       = 0;
        int                        sector      = 0;
        size_t                     offset      = 0;

        AddEntry (buffer, "PROG", kSectorCount);

        entries = ListEntries (buffer, fullyParsed);
        result  = buffer;

        {
            Dos33SectorAccess  access (buffer);
            Dos33Catalog       catalog (access);

            catalog.WriteTombstone (result, entries[0]);
        }

        Assert::AreEqual (size_t (0), ListEntries (result, fullyParsed).size());

        {
            Dos33SectorAccess  access (result);
            Dos33Catalog       catalog (access);

            Assert::IsTrue (catalog.TryFindFreeSlot (track, sector, offset));
        }

        Assert::AreEqual (kEntryBase, offset);
    }

    TEST_METHOD (FindEntry_ASharedName_IsAmbiguousUnlessIndexed)
    {
        vector<Byte>               buffer      = MakeFormattedVolume();
        bool                       fullyParsed = false;
        uint16_t                   owner       = 0;
        vector<Dos33CatalogEntry>  entries;

        AddEntry (buffer, "TWIN", kSectorCount);
        AddEntry (buffer, "TWIN", kSectorCount);

        entries = ListEntries (buffer, fullyParsed);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DUP_NAME),
                          Dos33Catalog::FindEntry (entries, FilePath::Parse ("TWIN"), owner));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                          Dos33Catalog::FindEntry (entries, FilePath::Parse ("OTHER"), owner));
    }

    TEST_METHOD (TryFindEntry_IgnoresCase)
    {
        vector<Byte>               buffer      = MakeFormattedVolume();
        bool                       fullyParsed = false;
        uint16_t                   owner       = 1;
        vector<Dos33CatalogEntry>  entries;

        AddEntry (buffer, "PROG", kSectorCount);

        entries = ListEntries (buffer, fullyParsed);

        Assert::IsTrue (Dos33Catalog::TryFindEntry (entries, "prog", owner));
        Assert::AreEqual (uint16_t (0), owner);
    }

    TEST_METHOD (TryEncodeName_RejectsNamesDosCannotOpen)
    {
        vector<Byte>  bytes;

        Assert::IsFalse (Dos33Catalog::TryEncodeName ("",        bytes));
        Assert::IsFalse (Dos33Catalog::TryEncodeName ("1PROG",   bytes));
        Assert::IsFalse (Dos33Catalog::TryEncodeName ("A,B",     bytes));
        Assert::IsFalse (Dos33Catalog::IsValidName   (string (31, 'A')));
        Assert::IsTrue  (Dos33Catalog::IsValidName   ("MY GAME"));
    }

    TEST_METHOD (TryEncodeName_IsHighAsciiPaddedWithSpaces)
    {
        vector<Byte>  bytes;

        Assert::IsTrue (Dos33Catalog::TryEncodeName ("ab", bytes));
        Assert::AreEqual (size_t (30), bytes.size());
        Assert::AreEqual (Byte (0xC1), bytes[0]);
        Assert::AreEqual (Byte (0xC2), bytes[1]);
        Assert::AreEqual (Byte (0xA0), bytes[29]);
    }

    TEST_METHOD (ReadStoredName_ReturnsTheStoredBytes)
    {
        vector<Byte>               buffer      = MakeFormattedVolume();
        bool                       fullyParsed = false;
        vector<Dos33CatalogEntry>  entries;
        vector<Byte>               encoded;
        vector<Byte>               stored;

        AddEntry (buffer, "PROG", kSectorCount);

        entries = ListEntries (buffer, fullyParsed);

        Assert::IsTrue (Dos33Catalog::TryEncodeName ("PROG", encoded));

        {
            Dos33SectorAccess  access (buffer);
            Dos33Catalog       catalog (access);

            catalog.ReadStoredName (entries[0], stored);
        }

        Assert::IsTrue (stored == encoded);
    }
};





