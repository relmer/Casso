#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "CassoExplorer/Model/CollectionResults.h"
#include "CassoExplorer/Model/CollectionWalk.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionScaleTests
//
//  Disk list and Find duplicates over 5,000 images in subfolders, a set of
//  them copies of others in another sector order: the walk lists every
//  image, finds every copy and nothing else, in under five minutes. Each
//  image is a DOS 3.3 disk with its number in a sector no file uses, stored
//  as a gzip so the set fits in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CollectionScaleTests)
{
public:

    static constexpr size_t  kImages      = 5000;
    static constexpr size_t  kCopies      = 100;
    static constexpr size_t  kPerFolder   = 500;
    static constexpr size_t  kCopyStride  = 37;
    static constexpr size_t  kMarkOffset  = 20 * 16 * 256;
    static constexpr double  kLimitSecs   = 300.0;

    static std::vector<Byte> MakeVariant (const std::vector<Byte> & blank, size_t number)
    {
        std::vector<Byte>  sectors = blank;



        for (size_t at = 0; at < sizeof (uint32_t); at++)
        {
            sectors[kMarkOffset + at] = (Byte) (number >> (8 * at));
        }

        return sectors;
    }

    static void AddImage (FakeDiskFileIo & io, const std::string & path, const std::string & innerName, const std::vector<Byte> & bytes)
    {
        std::vector<Byte>  gzip;



        GzipFile::Build (bytes, innerName, gzip);

        io.files[path]  = gzip;
        io.stamps[path] = FileStamp { gzip.size(), 1 };
    }


    TEST_METHOD (FiveThousandImages_ListedAndEveryCopyFound_InUnderFiveMinutes)
    {
        FakeDiskFileIo                  io;
        FakeDiskFileIo                  maker;
        DiskOperations                  operations (maker);
        DiskOperations::NewDiskRequest  request;
        std::vector<Byte>               blank;
        std::vector<CollectionDisk>     disks;
        std::vector<DuplicateSet>       sets;
        CollectionWalk::Visitor         visitor;
        std::set<std::string>           planted;
        std::set<std::string>           found;
        double                          seconds = 0.0;



        request.formatName = "dos33";
        Assert::IsTrue (operations.Create ("C:\\Make\\blank.dsk", request).Succeeded());
        blank = maker.files["C:\\Make\\blank.dsk"];

        for (size_t number = 0; number < kImages - kCopies; number++)
        {
            AddImage (io, std::format ("C:\\Collection\\shelf{}\\disk{}.dsk.gz", number / kPerFolder, number), std::format ("disk{}.dsk", number), MakeVariant (blank, number));
        }

        //  Each copy in ProDOS order, which loads as the same volume.
        for (size_t copy = 0; copy < kCopies; copy++)
        {
            std::vector<Byte>  po;
            size_t             original = copy * kCopyStride;

            VolumeImage::DosLogicalToProDosFile (MakeVariant (blank, original), po);
            AddImage (io, std::format ("C:\\Collection\\copies\\copy{}.po.gz", copy), std::format ("copy{}.po", copy), po);

            planted.insert (std::format ("C:\\Collection\\copies\\copy{}.po.gz", copy) + "=" + std::format ("C:\\Collection\\shelf{}\\disk{}.dsk.gz", original / kPerFolder, original));
        }

        visitor.onDisk = [&disks] (CollectionDisk && disk) { disks.push_back (std::move (disk)); };

        {
            auto  start = std::chrono::steady_clock::now();

            AssertSucceeded (CollectionWalk::Walk (io, "C:\\Collection", visitor));
            CollectionResults::FindDuplicates (disks, sets);

            seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        }

        Logger::WriteMessage (std::format ("{} disks and {} sets in {:.1f} s\n", disks.size(), sets.size(), seconds).c_str());

        for (const DuplicateSet & set : sets)
        {
            Assert::IsFalse  (set.isFile, L"Blank disks have no files to match");
            Assert::AreEqual ((size_t) 2, set.members.size());

            found.insert (std::min (disks[set.members[0].disk].path, disks[set.members[1].disk].path) + "=" + std::max (disks[set.members[0].disk].path, disks[set.members[1].disk].path));
        }

        Assert::AreEqual (kImages, disks.size(), L"Every image is listed");
        Assert::IsTrue   (found == planted, L"Every copy is found, and nothing else");
        Assert::IsTrue   (seconds < kLimitSecs, L"Within five minutes");
    }
};
