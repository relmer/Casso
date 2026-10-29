#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeAttributesTests
//
//  SetAttributes on both filesystems, over the scratch fixtures. Each case
//  reads the file back, so a change that only appears in the listing, or one
//  that moved the data, fails.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VolumeAttributesTests)
{
public:

    static constexpr const char *  kDos33Fixture  = "CassoExplorer/dos33.dsk";
    static constexpr const char *  kProDosFixture = "CassoExplorer/prodos.po";



    static void LoadSectors (const char * fixture, vector<Byte> & outSectors)
    {
        FixtureProvider     fixtures;
        vector<Byte>        bytes;
        SectorDecodeReport  report;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (VolumeImage::Load (bytes, fixture, outSectors, report));
    }



    static const FileEntry & FindEntry (const VolumeListing & listing, const char * name)
    {
        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == name)
            {
                return entry;
            }
        }

        Assert::Fail (L"entry not found");

        return listing.entries[0];
    }



    //  A binary written to the DOS 3.3 fixture, for the load address cases.
    static void AddDos33Binary (vector<Byte> & sectors, Word loadAddress)
    {
        FilePayload   payload;
        vector<Byte>  written;

        payload.bytes          = { 0xA9, 0x01, 0x60 };
        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = loadAddress;
        payload.hasLoadAddress = true;

        AssertSucceeded (Dos33Volume (sectors).Write (FilePath::Parse ("GAME"), payload, written));
        sectors = written;
    }



    TEST_METHOD (Dos33_TypeAndLock_AreTheCatalogsTypeByte)
    {
        vector<Byte>    sectors;
        vector<Byte>    changed;
        VolumeListing   listing;
        FileAttributes  attributes;



        LoadSectors (kDos33Fixture, sectors);

        attributes.type      = Dos33Volume::kTypeText;
        attributes.hasType   = true;
        attributes.access    = FilePayload::kAccessLocked;
        attributes.hasAccess = true;

        AssertSucceeded (Dos33Volume (sectors).SetAttributes (FilePath::Parse ("HELLO"), attributes, changed));
        AssertSucceeded (Dos33Volume (changed).Enumerate (listing));

        Assert::AreEqual ((int) Dos33Volume::kTypeText, (int) FindEntry (listing, "HELLO").type);
        Assert::IsTrue   (FindEntry (listing, "HELLO").isLocked);

        //  Unlocking a locked file is itself a change, so the lock doesn't block it.
        attributes.hasType = false;
        attributes.access  = FilePayload::kAccessUnlocked;

        AssertSucceeded (Dos33Volume (changed).SetAttributes (FilePath::Parse ("HELLO"), attributes, sectors));
        AssertSucceeded (Dos33Volume (sectors).Enumerate (listing));

        Assert::IsFalse (FindEntry (listing, "HELLO").isLocked);
    }



    TEST_METHOD (Dos33_LoadAddress_IsRewrittenAndTheDataKept)
    {
        vector<Byte>    sectors;
        vector<Byte>    changed;
        FilePayload     before;
        FilePayload     after;
        FileAttributes  attributes;



        LoadSectors (kDos33Fixture, sectors);
        AddDos33Binary (sectors, 0x0803);
        AssertSucceeded (Dos33Volume (sectors).Read (FilePath::Parse ("GAME"), before));

        attributes.auxType    = 0x2000;
        attributes.hasAuxType = true;

        AssertSucceeded (Dos33Volume (sectors).SetAttributes (FilePath::Parse ("GAME"), attributes, changed));
        AssertSucceeded (Dos33Volume (changed).Read (FilePath::Parse ("GAME"), after));

        Assert::AreEqual ((int) 0x2000, (int) after.loadAddress);
        Assert::IsTrue   (after.bytes == before.bytes, L"only the address changes");
    }



    TEST_METHOD (Dos33_DatesAndAnAddressOnAnApplesoftFile_AreRefused)
    {
        vector<Byte>    sectors;
        vector<Byte>    changed;
        FileAttributes  dated;
        FileAttributes  address;



        LoadSectors (kDos33Fixture, sectors);

        dated.hasModified = true;
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), Dos33Volume (sectors).SetAttributes (FilePath::Parse ("HELLO"), dated, changed));

        address.auxType    = 0x0803;
        address.hasAuxType = true;
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), Dos33Volume (sectors).SetAttributes (FilePath::Parse ("HELLO"), address, changed));
    }



    TEST_METHOD (ProDos_EveryField_IsWrittenAndTheDataKept)
    {
        vector<Byte>    sectors;
        vector<Byte>    changed;
        VolumeListing   listing;
        FilePayload     before;
        FilePayload     after;
        FileAttributes  attributes;



        LoadSectors (kProDosFixture, sectors);
        AssertSucceeded (ProDosVolume (sectors).Read (FilePath::Parse ("HELLO"), before));

        attributes.type         = ProDosVolume::kTypeBinary;
        attributes.auxType      = 0x0803;
        attributes.access       = FilePayload::kAccessLocked;
        attributes.modifiedDate = (Word) ((88 << 9) | (3 << 5) | 5);
        attributes.modifiedTime = (Word) ((14 << 8) | 54);
        attributes.hasType      = true;
        attributes.hasAuxType   = true;
        attributes.hasAccess    = true;
        attributes.hasModified  = true;

        AssertSucceeded (ProDosVolume (sectors).SetAttributes (FilePath::Parse ("HELLO"), attributes, changed));
        AssertSucceeded (ProDosVolume (changed).Enumerate (listing));
        AssertSucceeded (ProDosVolume (changed).Read (FilePath::Parse ("HELLO"), after));

        Assert::AreEqual ((int) ProDosVolume::kTypeBinary, (int) FindEntry (listing, "HELLO").type);
        Assert::AreEqual ((int) 0x0803, (int) FindEntry (listing, "HELLO").auxType);
        Assert::IsTrue   (FindEntry (listing, "HELLO").isLocked);
        Assert::IsTrue   (after.hasDates && after.modifiedDate == attributes.modifiedDate && after.modifiedTime == attributes.modifiedTime);
        Assert::IsTrue   (after.bytes == before.bytes, L"no block moves");
    }
};
