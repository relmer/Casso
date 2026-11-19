#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Update/ZipArchive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedBlockImageTests
//
//  Block mode over an image in a gzip file and in a ZIP: the image opens, a
//  committed write is in the host file again, compressed, and a change to
//  the host file by something else is detected. A repaired copy of a gzip
//  image has both extensions, and one of an image in a ZIP is a copy of
//  the ZIP. An 800K .dsk image opens in block mode, and a 140K one
//  doesn't.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WrappedBlockImageTests)
{
public:

    static constexpr size_t    kBlockBytes = 512;
    static constexpr size_t    kBlocks     = 1600;
    static constexpr uint32_t  kEdited     = 1000;
    static constexpr int64_t   kTime       = 1'700'000'000;

    static vector<Byte> MakeImage()
    {
        vector<Byte>  image (kBlocks * kBlockBytes, 0);



        AssertSucceeded (ProDosSkeleton::Write (image, "WRAPPED"));

        return image;
    }


    //  Opens the path, writes 0x5A over block kEdited and commits.
    static void EditAndCommit (FakeDiskFileIo & io, const std::string & path)
    {
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;
        vector<Byte>                     block (kBlockBytes, 0x5A);



        Assert::IsTrue (BlockImageFile::IsBlockImage (io, path), L"the image opens in block mode");

        AssertSucceeded (BlockImageFile::Open (io, path, file, error));
        Assert::AreEqual ((uint32_t) kBlocks, file->GetBlockCount());
        Assert::IsNotNull (file->GetWrapped());

        AssertSucceeded (file->GetStore().WriteBlocks (kEdited, block));
        AssertSucceeded (file->Commit());

        Assert::IsTrue (file->IsFileUnchanged(), L"the commit isn't taken as a change by something else");

        io.stamps[RepairCopy::GetHostPath (path)].modifiedUnix += 100;

        Assert::IsFalse (file->IsFileUnchanged(), L"a change by something else is detected");
    }


    TEST_METHOD (GzipImage_IsEditedAndWrittenBackCompressed)
    {
        FakeDiskFileIo  io;
        vector<Byte>    image = MakeImage();
        vector<Byte>    gzip;
        vector<Byte>    expanded;
        std::string     name;
        std::string     error;



        GzipFile::Build (image, "disk.po", gzip);
        io.files["C:\\d\\disk.po.gz"]  = gzip;
        io.stamps["C:\\d\\disk.po.gz"] = { gzip.size(), kTime };

        EditAndCommit (io, "C:\\d\\disk.po.gz");

        AssertSucceeded (GzipFile::Expand (io.files["C:\\d\\disk.po.gz"], expanded, name, error));
        Assert::AreEqual (image.size(), expanded.size());
        Assert::AreEqual ((Byte) 0x5A, expanded[kEdited * kBlockBytes], L"the write is in the gzip file");
        Assert::IsTrue   (io.files["C:\\d\\disk.po.gz"].size() < image.size() / 20, L"the image is compressed");
    }


    TEST_METHOD (ZipImage_IsEditedAndWrittenBackIntoTheZip)
    {
        static constexpr Byte  kEmptyZip[22] = { 0x50, 0x4B, 0x05, 0x06 };

        FakeDiskFileIo     io;
        vector<Byte>       image = MakeImage();
        vector<Byte>       zip;
        vector<ZipEntry>   entries;



        AssertSucceeded (ZipArchive::Replace (kEmptyZip, "disks/a.po", image, zip));
        io.files["C:\\d\\set.zip"]  = zip;
        io.stamps["C:\\d\\set.zip"] = { zip.size(), kTime };

        EditAndCommit (io, "C:\\d\\set.zip|disks/a.po");

        AssertSucceeded (ZipArchive::Extract (io.files["C:\\d\\set.zip"], entries));
        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::AreEqual ((Byte) 0x5A, entries[0].data[kEdited * kBlockBytes], L"the write is in the ZIP");
        Assert::IsTrue   (io.files["C:\\d\\set.zip"].size() < image.size() / 20, L"the image is deflated");
    }


    TEST_METHOD (RepairedCopies_KeepTheirContainers)
    {
        FakeDiskFileIo  io;



        Assert::AreEqual (std::string ("C:\\d\\disk (repaired).po.gz"),        RepairCopy::GetCopyPath (io, "C:\\d\\disk.po.gz"));
        Assert::AreEqual (std::string ("C:\\d\\set (repaired).zip|disks/a.po"), RepairCopy::GetHostCopyPath (io, "C:\\d\\set.zip|disks/a.po"));
        Assert::AreEqual (std::string ("C:\\d\\plain (repaired).po"),          RepairCopy::GetHostCopyPath (io, "C:\\d\\plain.po"));

        io.files["C:\\d\\disk (repaired).po.gz"] = { 0 };

        Assert::AreEqual (std::string ("C:\\d\\disk (repaired 2).po.gz"), RepairCopy::GetCopyPath (io, "C:\\d\\disk.po.gz"));
    }


    TEST_METHOD (An800KDsk_OpensInBlockMode_And140KDoesnt)
    {
        FakeDiskFileIo  io;



        io.files["C:\\d\\big.dsk"]    = MakeImage();
        io.stamps["C:\\d\\big.dsk"]   = { kBlocks * kBlockBytes, kTime };
        io.files["C:\\d\\small.dsk"]  = vector<Byte> (143360, 0);
        io.stamps["C:\\d\\small.dsk"] = { 143360, kTime };

        Assert::IsTrue  (BlockImageFile::IsBlockImage (io, "C:\\d\\big.dsk"));
        Assert::IsFalse (BlockImageFile::IsBlockImage (io, "C:\\d\\small.dsk"));
    }
};
