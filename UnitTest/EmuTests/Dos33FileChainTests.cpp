#include "Pch.h"

#include "Devices/Disk/ChainWalkGuard.h"
#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33FileChain.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/NibbleFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileChainTests
//
//  Track/sector lists laid out and walked over an in-memory buffer.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33FileChainTests)
{
public:

    static constexpr int     kListTrack  = 18;
    static constexpr int     kListSector = 15;
    static constexpr int     kDataTrack  = 19;
    static constexpr size_t  kUnitCount  = 560;
    static constexpr Byte    kFill       = 0x5A;

    //  A list followed by `dataCount` data sectors, in allocation order.
    static vector<uint32_t> MakeUnits (const Dos33SectorAccess & access, int dataCount)
    {
        vector<uint32_t>  units = { access.ToUnit (kListTrack, kListSector) };
        int               i     = 0;

        for (i = 0; i < dataCount; i++)
        {
            units.push_back (access.ToUnit (kDataTrack, 15 - i));
        }

        return units;
    }

    static Dos33CatalogEntry MakeEntry (uint16_t sectorCount)
    {
        Dos33CatalogEntry  entry;

        entry.tsListTrack  = kListTrack;
        entry.tsListSector = kListSector;
        entry.sectorCount  = sectorCount;

        return entry;
    }

    TEST_METHOD (PlaceFile_IsWalkedBackByCollectDataSectors)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, 0);
        vector<Byte>       result = buffer;
        Dos33SectorAccess  access (result);
        Dos33FileChain     chain (access);
        vector<uint32_t>   units  = MakeUnits (access, 2);
        vector<Byte>       stored (NibbleFormat::kSectorByteSize + 1, kFill);
        vector<uint32_t>   dataUnits;
        vector<uint32_t>   listUnits;
        ChainWalkGuard     guard (kUnitCount);

        chain.PlaceFile (result, units, 2, stored);

        Assert::IsTrue (chain.CollectDataSectors (MakeEntry (3), dataUnits, listUnits, guard));
        Assert::AreEqual (size_t (1), listUnits.size());
        Assert::AreEqual (units[0], listUnits[0]);
        Assert::AreEqual (size_t (2), dataUnits.size());
        Assert::AreEqual (units[1], dataUnits[0]);
        Assert::AreEqual (units[2], dataUnits[1]);
        Assert::AreEqual (kFill, access.ReadByte (kDataTrack, 14, 0), L"the second sector has the last stored byte");
        Assert::AreEqual (Byte (0), access.ReadByte (kDataTrack, 14, 1));
    }

    TEST_METHOD (CollectDataSectors_AnEntryOccupyingNoSectors_HasNoChain)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, 0);
        Dos33SectorAccess  access (buffer);
        Dos33FileChain     chain (access);
        vector<uint32_t>   dataUnits;
        vector<uint32_t>   listUnits;
        ChainWalkGuard     guard (kUnitCount);

        Assert::IsTrue (chain.CollectDataSectors (MakeEntry (0), dataUnits, listUnits, guard));
        Assert::AreEqual (size_t (0), listUnits.size());
        Assert::AreEqual (size_t (0), dataUnits.size());
    }

    TEST_METHOD (CollectDataSectors_AListPointingAtItself_StopsAndReportsIt)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, 0);
        Dos33SectorAccess  access (buffer);
        Dos33FileChain     chain (access);
        vector<uint32_t>   dataUnits;
        vector<uint32_t>   listUnits;
        ChainWalkGuard     guard (kUnitCount);
        size_t             listAt = access.GetGeometry().GetSectorOffset (kListTrack, kListSector);

        buffer[listAt + 0x01] = kListTrack;
        buffer[listAt + 0x02] = kListSector;

        Assert::IsFalse (chain.CollectDataSectors (MakeEntry (1), dataUnits, listUnits, guard));
    }

    TEST_METHOD (ZeroUnits_ClearsEachSector)
    {
        vector<Byte>       buffer (NibbleFormat::kImageByteSize, kFill);
        vector<Byte>       result = buffer;
        Dos33SectorAccess  access (buffer);
        Dos33FileChain     chain (access);
        uint32_t           unit   = access.ToUnit (kDataTrack, 0);
        size_t             at     = access.GetGeometry().GetSectorOffset (kDataTrack, 0);

        chain.ZeroUnits (result, { unit });

        Assert::AreEqual (Byte (0), result[at]);
        Assert::AreEqual (Byte (0), result[at + NibbleFormat::kSectorByteSize - 1]);
        Assert::AreEqual (kFill, result[at + NibbleFormat::kSectorByteSize]);
        Assert::AreEqual (kFill, buffer[at], L"the source buffer is left alone");
    }
};





