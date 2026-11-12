#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SosVolumeTests
//
//  SOS volumes, which are ProDOS volumes as SOS writes them: a directory
//  whose header has fewer than 13 entries a block is read and written
//  within that count.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SosVolumeTests)
{
public:

    static constexpr size_t  kImageBytes   = 143360;
    static constexpr size_t  kEntryLength  = 0x27;
    static constexpr size_t  kFirstEntry   = 0x04;
    static constexpr Byte    kTwelve       = 12;
    static constexpr Word    kLoadAddress  = 0x2000;

    static vector<Byte> MakeVolume (Byte entriesPerBlock)
    {
        vector<Byte>  sectors (kImageBytes, 0);



        AssertSucceeded (ProDosSkeleton::Write (sectors, "SOSDISK"));

        sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), ProDosSkeleton::kDirKeyBlock, ProDosSkeleton::kEntriesPerBlockAt)] = entriesPerBlock;

        return sectors;
    }


    static FilePayload MakePayload()
    {
        FilePayload  payload;



        payload.bytes.assign (100, 0x5A);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = kLoadAddress;
        payload.hasLoadAddress = true;

        return payload;
    }


    static Byte GetStorage (const vector<Byte> & sectors, int block, size_t slot)
    {
        return (Byte) (sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), block, kFirstEntry + slot * kEntryLength)] & 0xF0);
    }


    TEST_METHOD (TwelveEntriesABlock_IsDetectedAsProDos)
    {
        vector<Byte>  twelve = MakeVolume (kTwelve);
        vector<Byte>  zero   = MakeVolume (0);
        vector<Byte>  over   = MakeVolume (14);



        Assert::IsTrue (VolumeImage::DetectFilesystem (twelve) == VolumeKind::ProDos);
        Assert::IsTrue (VolumeImage::DetectFilesystem (zero)   != VolumeKind::ProDos);
        Assert::IsTrue (VolumeImage::DetectFilesystem (over)   != VolumeKind::ProDos);
    }


    TEST_METHOD (TwelveEntriesABlock_FilesAreWrittenAndListedWithinTheCount)
    {
        constexpr int  kFiles = 14;

        vector<Byte>   sectors = MakeVolume (kTwelve);
        vector<Byte>   next;
        VolumeListing  listing;
        int            i       = 0;



        for (i = 0; i < kFiles; i++)
        {
            AssertSucceeded (ProDosVolume (sectors).Write (FilePath::Parse (std::format ("FILE{}", i)), MakePayload(), next));
            sectors = std::move (next);
        }

        AssertSucceeded (ProDosVolume (sectors).Enumerate (listing));

        Assert::AreEqual ((size_t) kFiles, listing.entries.size());
        Assert::AreEqual ((Byte) 0, GetStorage (sectors, ProDosSkeleton::kDirKeyBlock, kTwelve), L"the key block's 13th slot stays empty");
        Assert::AreEqual ((Byte) 0, GetStorage (sectors, ProDosSkeleton::kDirKeyBlock + 1, kTwelve), L"the next block's 13th slot stays empty");
        Assert::AreNotEqual ((Byte) 0, GetStorage (sectors, ProDosSkeleton::kDirKeyBlock + 1, 0));
    }


    TEST_METHOD (TwelveEntriesABlock_AnEntryInTheThirteenthSlotIsNotListed)
    {
        vector<Byte>   sectors = MakeVolume (kTwelve);
        vector<Byte>   next;
        VolumeListing  listing;
        size_t         at      = ProDosSkeleton::GetBlockByteOffset (kImageBytes, ProDosSkeleton::kDirKeyBlock, kFirstEntry + kTwelve * kEntryLength);



        AssertSucceeded (ProDosVolume (sectors).Write (FilePath::Parse ("REAL"), MakePayload(), next));
        sectors = std::move (next);

        //  A stray record past the header's count, as SOS would never read.
        sectors[at]     = 0x14;
        sectors[at + 1] = 'G';
        sectors[at + 2] = 'H';
        sectors[at + 3] = 'O';
        sectors[at + 4] = 'S';

        AssertSucceeded (ProDosVolume (sectors).Enumerate (listing));

        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::AreEqual (std::string ("REAL"), listing.entries[0].name);
    }
};
