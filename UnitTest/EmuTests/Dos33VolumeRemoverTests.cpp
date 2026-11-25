#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/Dos33VolumeRemover.h"
#include "Machines/Apple2/Common/NibbleFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeRemoverTests
//
//  Deletes over an in-memory volume with the HELLO greeting on it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33VolumeRemoverTests)
{
public:

    static constexpr Byte      kVolumeNumber      = 254;
    static constexpr uint32_t  kFreeOnBlankVolume = 31 * 16;
    static constexpr size_t    kEntryTypeOffset   = 0x0B + 0x02;
    static constexpr int       kCatalogSector     = 15;

    static vector<Byte> MakeVolumeWithHello()
    {
        vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));
        AssertSucceeded (Dos33FileWriter::WriteHello (buffer));

        return buffer;
    }

    static size_t CountEntries (const vector<Byte> & buffer)
    {
        Dos33SectorAccess          access (buffer);
        Dos33Catalog               catalog (access);
        vector<Dos33CatalogEntry>  entries;
        vector<std::string>        damage;
        bool                       fullyParsed = false;

        catalog.CollectEntries (entries, damage, fullyParsed);

        return entries.size();
    }

    TEST_METHOD (Delete_FreesTheSectorsAndTombstonesTheEntry)
    {
        vector<Byte>        buffer = MakeVolumeWithHello();
        vector<Byte>        result;
        Dos33SectorAccess   access (buffer);
        Dos33VolumeRemover  remover (access);
        DeleteOutcome       outcome;

        AssertSucceeded (remover.Delete (FilePath::Parse ("HELLO"), result, outcome));

        Assert::AreEqual (size_t (2), outcome.freedUnits.size(), L"the list sector and the data sector");
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());
        Assert::AreEqual (size_t (0), outcome.warnings.size());
        Assert::AreEqual (size_t (0), CountEntries (result));
        Assert::AreEqual (size_t (1), CountEntries (buffer), L"the source buffer is left alone");

        {
            Dos33SectorAccess  resultAccess (result);
            Dos33FreeMap       freeMap (resultAccess);

            Assert::AreEqual (kFreeOnBlankVolume, freeMap.CountFreeUnits());
        }
    }

    TEST_METHOD (Delete_ALockedFile_IsDenied)
    {
        vector<Byte>        buffer = MakeVolumeWithHello();
        vector<Byte>        result;
        Dos33SectorAccess   access (buffer);
        Dos33VolumeRemover  remover (access);
        size_t              typeAt = Dos33Skeleton::GetSectorOffset (Dos33Skeleton::kVtocTrack, kCatalogSector) + kEntryTypeOffset;

        buffer[typeAt] = (Byte) (buffer[typeAt] | Dos33Volume::kLockedBit);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
                          remover.Delete (FilePath::Parse ("HELLO"), result));
        Assert::AreEqual (size_t (0), result.size());
    }

    TEST_METHOD (Delete_AMissingFile_IsNotFound)
    {
        vector<Byte>        buffer = MakeVolumeWithHello();
        vector<Byte>        result;
        Dos33SectorAccess   access (buffer);
        Dos33VolumeRemover  remover (access);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                          remover.Delete (FilePath::Parse ("NOPE"), result));
    }

    TEST_METHOD (Delete_ANestedPath_IsAnInvalidName)
    {
        vector<Byte>        buffer = MakeVolumeWithHello();
        vector<Byte>        result;
        Dos33SectorAccess   access (buffer);
        Dos33VolumeRemover  remover (access);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
                          remover.Delete (FilePath::Parse ("DIR/HELLO"), result));
    }
};





