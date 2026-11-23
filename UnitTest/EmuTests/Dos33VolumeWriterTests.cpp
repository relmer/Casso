#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/Dos33VolumeInterfaces.h"
#include "Machines/Apple2/Common/Dos33VolumeWriter.h"
#include "Machines/Apple2/Common/NibbleFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeWriterTests
//
//  Placement, rename and attribute changes over an in-memory volume, with a
//  fake removal for the replacement path.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33VolumeWriterTests)
{
public:

    static constexpr Byte  kVolumeNumber = 254;
    static constexpr Word  kLoadAddress  = 0x0300;

    class FakeRemoval : public IDos33FileRemoval
    {
    public:
        HRESULT  Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome &) const override
        {
            paths.push_back (path.GetLeaf());
            outBuffer = staged;

            return S_OK;
        }

        vector<Byte>              staged;
        mutable vector<string>    paths;
    };

    static vector<Byte> MakeFormattedVolume()
    {
        vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));

        return buffer;
    }

    static vector<Byte> MakeVolumeWithHello()
    {
        vector<Byte>  buffer = MakeFormattedVolume();

        AssertSucceeded (Dos33FileWriter::WriteHello (buffer));

        return buffer;
    }

    static FilePayload MakeBinary()
    {
        FilePayload  payload;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.bytes          = { 0x60 };
        payload.loadAddress    = kLoadAddress;
        payload.hasLoadAddress = true;

        return payload;
    }

    static vector<Dos33CatalogEntry> ListEntries (const vector<Byte> & buffer)
    {
        Dos33SectorAccess          access (buffer);
        Dos33Catalog               catalog (access);
        vector<Dos33CatalogEntry>  entries;
        vector<std::string>        damage;
        bool                       fullyParsed = false;

        catalog.CollectEntries (entries, damage, fullyParsed);

        return entries;
    }

    TEST_METHOD (Write_ANewFile_AddsItsEntryWithoutRemovingAnything)
    {
        vector<Byte>       buffer = MakeVolumeWithHello();
        vector<Byte>       result;
        Dos33SectorAccess  access (buffer);
        FakeRemoval        removal;
        Dos33VolumeWriter  writer (access, removal);

        AssertSucceeded (writer.Write (FilePath::Parse ("PROG"), MakeBinary(), result));

        Assert::AreEqual (size_t (2), ListEntries (result).size());
        Assert::AreEqual (size_t (0), removal.paths.size());
    }

    TEST_METHOD (Write_AnExistingFile_PlacesTheFileOverTheStagedRemoval)
    {
        vector<Byte>               buffer = MakeVolumeWithHello();
        vector<Byte>               result;
        Dos33SectorAccess          access (buffer);
        FakeRemoval                removal;
        Dos33VolumeWriter          writer (access, removal);
        vector<Dos33CatalogEntry>  entries;

        removal.staged = MakeFormattedVolume();

        AssertSucceeded (writer.Write (FilePath::Parse ("HELLO"), MakeBinary(), result));

        entries = ListEntries (result);

        Assert::AreEqual (size_t (1), removal.paths.size());
        Assert::AreEqual (string ("HELLO"), removal.paths[0]);
        Assert::AreEqual (size_t (1), entries.size());
        Assert::AreEqual (Dos33Volume::kTypeBinary, entries[0].typeByte);
    }

    TEST_METHOD (Write_OverALockedFile_IsDeniedBeforeAnyRemoval)
    {
        vector<Byte>       buffer = MakeVolumeWithHello();
        vector<Byte>       result;
        FakeRemoval        removal;
        size_t             typeAt = Dos33Skeleton::GetSectorOffset (Dos33Skeleton::kVtocTrack, 15) + 0x0B + 0x02;

        buffer[typeAt] = (Byte) (buffer[typeAt] | Dos33Volume::kLockedBit);

        {
            Dos33SectorAccess  access (buffer);
            Dos33VolumeWriter  writer (access, removal);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
                              writer.Write (FilePath::Parse ("HELLO"), MakeBinary(), result));
        }

        Assert::AreEqual (size_t (0), removal.paths.size());
    }

    TEST_METHOD (Rename_ChangesTheName)
    {
        vector<Byte>       buffer = MakeVolumeWithHello();
        vector<Byte>       result;
        Dos33SectorAccess  access (buffer);
        FakeRemoval        removal;
        Dos33VolumeWriter  writer (access, removal);

        AssertSucceeded (writer.Rename (FilePath::Parse ("HELLO"), "START", result));

        Assert::AreEqual (string ("START"), ListEntries (result)[0].name);
    }

    TEST_METHOD (Rename_OntoAnotherFilesName_IsRejected)
    {
        vector<Byte>       buffer = MakeVolumeWithHello();
        vector<Byte>       withProg;
        vector<Byte>       result;
        FakeRemoval        removal;

        {
            Dos33SectorAccess  access (buffer);
            Dos33VolumeWriter  writer (access, removal);

            AssertSucceeded (writer.Write (FilePath::Parse ("PROG"), MakeBinary(), withProg));
        }

        {
            Dos33SectorAccess  access (withProg);
            Dos33VolumeWriter  writer (access, removal);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),
                              writer.Rename (FilePath::Parse ("HELLO"), "PROG", result));
        }
    }

    TEST_METHOD (SetAttributes_Lock_SetsTheLockBit)
    {
        vector<Byte>       buffer     = MakeVolumeWithHello();
        vector<Byte>       result;
        Dos33SectorAccess  access (buffer);
        FakeRemoval        removal;
        Dos33VolumeWriter  writer (access, removal);
        FileAttributes     attributes;

        attributes.hasAccess = true;
        attributes.access    = 0;

        AssertSucceeded (writer.SetAttributes (FilePath::Parse ("HELLO"), attributes, result));

        Assert::AreEqual (Dos33Volume::kLockedBit, (Byte) (ListEntries (result)[0].typeByte & Dos33Volume::kLockedBit));
    }
};





