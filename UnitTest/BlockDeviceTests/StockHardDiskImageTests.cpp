#include "Pch.h"

#include "Devices/BlockDevice/StockHardDiskImage.h"
#include "Devices/Disk/BlockImage.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StockHardDiskImageTests
//
//  The stock hard disk image: made on first use, never written over, and a
//  failed create reported.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StockHardDiskImageTests)
{
public:

    static constexpr const char *  kFolder = "C:\\Casso\\Disks";


    //  FIRST USE MAKES AN EMPTY PRODOS VOLUME OF 65,535 BLOCKS with a blank
    //  block 0.
    TEST_METHOD (FirstUseMakesAnEmptyVolume)
    {
        FakeDiskFileIo     io;
        std::string        path;
        VolumeListing      listing;
        bool               blankBoot = true;



        Assert::AreEqual (S_OK, StockHardDiskImage::Ensure (io, kFolder, path));
        Assert::IsTrue   (io.Exists (path));
        Assert::AreEqual ((size_t) StockHardDiskImage::kBlocks * 512, io.files[path].size());

        for (size_t i = 0; i < 512; i++)
        {
            blankBoot = blankBoot && io.files[path][i] == 0;
        }

        Assert::IsTrue   (blankBoot, L"block 0 is blank, so the card's ROM passes over it");
        Assert::AreEqual (S_OK, ProDosVolume (BlockImage (io.files[path])).Enumerate (listing));
        Assert::AreEqual (std::string (StockHardDiskImage::kpszVolumeName), listing.volumeName);
        Assert::IsTrue   (listing.entries.empty());
    }


    //  A SECOND USE LEAVES AN EDITED IMAGE BYTE-IDENTICAL.
    TEST_METHOD (SecondUseLeavesTheImageAlone)
    {
        FakeDiskFileIo     io;
        std::string        path;
        std::vector<Byte>  edited;



        Assert::AreEqual (S_OK, StockHardDiskImage::Ensure (io, kFolder, path));

        io.files[path][5000] = 0x5A;
        edited = io.files[path];

        Assert::AreEqual (S_OK, StockHardDiskImage::Ensure (io, kFolder, path));
        Assert::IsTrue   (io.files[path] == edited);
    }


    //  A CREATE THAT FAILS IS REPORTED, and nothing is left at the path.
    TEST_METHOD (FailedCreateIsReported)
    {
        FakeDiskFileIo  io;
        std::string     path;
        HRESULT         hr = S_OK;



        io.failNextWrite = true;

        hr = StockHardDiskImage::Ensure (io, kFolder, path);

        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (io.Exists (path));
    }
};
