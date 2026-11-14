#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/Woz35Builder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMediaTests
//
//  Attaching, reading, writing and flushing a block device's image over an
//  in-memory file system: each container, the drive's capacity, write
//  protection, damaged images, one writer per path, and a file changed on
//  the host.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockDeviceMediaTests)
{
public:

    static constexpr uint32_t  kBlocks    = 280;
    static constexpr uint32_t  kMaxBlocks = 65535;
    static constexpr size_t    kBlock     = BlockDeviceMedia::kBlockBytes;


    //  Blocks numbered by their first two bytes, so a read shows which block
    //  it came from.
    static std::vector<Byte> MakeBlocks (uint32_t count)
    {
        std::vector<Byte>  bytes ((size_t) count * kBlock, 0);



        for (uint32_t block = 0; block < count; block++)
        {
            bytes[(size_t) block * kBlock]     = (Byte) (block & 0xFF);
            bytes[(size_t) block * kBlock + 1] = (Byte) (block >> 8);
        }

        return bytes;
    }


    static void AddFile (FakeDiskFileIo & io, const std::string & path, const std::vector<Byte> & bytes)
    {
        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 1 };
    }


    static std::vector<Byte> MakeTwoImg (uint32_t format, uint32_t flags)
    {
        TwoImgHeader       header;
        std::vector<Byte>  payload = MakeBlocks (kBlocks);
        std::vector<Byte>  bytes;



        header.format     = format;
        header.flags      = flags;
        header.blockCount = kBlocks;

        TwoImgContainer::Build (header, payload, bytes);

        return bytes;
    }


    static std::vector<Byte> MakeDiskCopy (uint32_t blocks)
    {
        DiskCopy42Header   header;
        std::vector<Byte>  data = MakeBlocks (blocks);
        std::vector<Byte>  bytes;



        header.name     = "Test";
        header.dataSize = (uint32_t) data.size();

        DiskCopy42Container::Build (header, data, bytes);

        return bytes;
    }


    static HRESULT AttachPath (BlockDeviceMedia & media, FakeDiskFileIo & io, const std::string & path, uint32_t maxBlocks = kMaxBlocks)
    {
        std::wstring  error;



        return media.Attach (io, path, maxBlocks, WriteProtectInfo {}, error);
    }


    //  A PRODOS-ORDER FILE, A PRODOS-ORDER 2IMG AND A DISKCOPY 4.2 IMAGE EACH
    //  READ BLOCK N AS BLOCK N.
    TEST_METHOD (EachContainerReadsItsBlocks)
    {
        FakeDiskFileIo        io;
        std::vector<Byte>     block (kBlock);
        const char          * paths[] = { "a.po", "b.hdv", "c.2mg", "d.dsk" };



        AddFile (io, "a.po",  MakeBlocks (kBlocks));
        AddFile (io, "b.hdv", MakeBlocks (kBlocks));
        AddFile (io, "c.2mg", MakeTwoImg (TwoImgHeader::kFormatProDosOrder, 0));
        AddFile (io, "d.dsk", MakeDiskCopy (1600));

        for (const char * path : paths)
        {
            BlockDeviceMedia  media;



            Assert::AreEqual (S_OK, AttachPath (media, io, path), TextEncoding::NarrowToWide (path).c_str());
            Assert::AreEqual (S_OK, media.ReadBlock (257, block));
            Assert::AreEqual ((Byte) 1, block[0]);
            Assert::AreEqual ((Byte) 1, block[1], L"block 257 is $101");
        }
    }


    //  A DOS-ORDER 2IMG, AN IMAGE LARGER THAN THE DRIVE AND AN EMPTY FILE FAIL
    //  TO ATTACH, with a reason, and each file is byte-identical afterward.
    TEST_METHOD (UnfitImagesFailAndAreNotChanged)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   media;
        std::wstring       error;
        std::vector<Byte>  dosOrder = MakeTwoImg (TwoImgHeader::kFormatDosOrder, 0);
        std::vector<Byte>  tooBig   = MakeBlocks (kBlocks);
        HRESULT            hr       = S_OK;



        AddFile (io, "dos.2mg", dosOrder);
        AddFile (io, "big.po",  tooBig);
        AddFile (io, "empty.po", {});

        hr = media.Attach (io, "dos.2mg", kMaxBlocks, {}, error);
        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (error.empty());

        error.clear();
        hr = media.Attach (io, "big.po", kBlocks - 1, {}, error);
        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (error.find (L"279") != std::wstring::npos, L"the error has the drive's capacity");

        hr = AttachPath (media, io, "empty.po");
        Assert::IsTrue (FAILED (hr));
        Assert::IsFalse (media.IsAttached());
        Assert::IsTrue (io.files["dos.2mg"] == dosOrder);
        Assert::IsTrue (io.files["big.po"]  == tooBig);
        Assert::AreEqual (0, io.writeCount);
    }


    //  THE 2IMG LOCKED FLAG AND A READ-ONLY HOST FILE BOTH WRITE-PROTECT THE
    //  DRIVE, and a write to it fails.
    TEST_METHOD (LockedImageAndReadOnlyFileProtect)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   locked;
        BlockDeviceMedia   readOnly;
        WriteProtectInfo   host;
        std::wstring       error;
        std::vector<Byte>  block (kBlock, 0x55);



        AddFile (io, "locked.2mg", MakeTwoImg (TwoImgHeader::kFormatProDosOrder, TwoImgHeader::kFlagLocked));
        AddFile (io, "ro.po",      MakeBlocks (kBlocks));

        host.readOnlyFile = true;

        Assert::AreEqual (S_OK, AttachPath (locked, io, "locked.2mg"));
        Assert::AreEqual (S_OK, readOnly.Attach (io, "ro.po", kMaxBlocks, host, error));

        Assert::IsTrue (locked.GetWriteProtect().imageFlag);
        Assert::IsTrue (readOnly.GetWriteProtect().readOnlyFile);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT), locked.WriteBlock (2, block));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT), readOnly.WriteBlock (2, block));
    }


    //  A WRITE IS IN THE FILE AFTER FLUSH, AND NOT BEFORE, so a reset in the
    //  middle of a guest's write leaves the file's old block whole.
    TEST_METHOD (WritesReachTheFileOnFlush)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   media;
        std::vector<Byte>  block (kBlock, 0xA5);
        std::vector<Byte>  before = MakeBlocks (kBlocks);



        AddFile (io, "w.po", before);

        Assert::AreEqual (S_OK, AttachPath (media, io, "w.po"));
        Assert::AreEqual (S_OK, media.WriteBlock (3, block));
        Assert::IsTrue   (media.HasPendingWrites());
        Assert::IsTrue   (io.files["w.po"] == before, L"nothing is written before the flush");

        Assert::AreEqual (S_OK, media.Flush());
        Assert::IsFalse  (media.HasPendingWrites());
        Assert::AreEqual ((Byte) 0xA5, io.files["w.po"][3 * kBlock]);
        Assert::AreEqual ((Byte) 4,    io.files["w.po"][4 * kBlock], L"the next block is untouched");
    }


    //  A DISKCOPY 4.2 WRITE UPDATES ITS DATA CHECKSUM, and an image whose
    //  checksum does not match fails to attach as damaged.
    TEST_METHOD (DiskCopyChecksums)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   media;
        BlockDeviceMedia   damaged;
        DiskCopy42Header   header;
        std::vector<Byte>  data;
        std::string        parseError;
        std::vector<Byte>  block (kBlock, 0x11);
        std::vector<Byte>  bad   = MakeDiskCopy (1600);
        std::wstring       error;



        AddFile (io, "good.dsk", MakeDiskCopy (1600));
        Assert::AreEqual (S_OK, AttachPath (media, io, "good.dsk"));
        Assert::AreEqual (S_OK, media.WriteBlock (9, block));
        Assert::AreEqual (S_OK, media.Detach());

        Assert::AreEqual (S_OK, DiskCopy42Container::Parse (io.files["good.dsk"], header, data, parseError));
        Assert::IsFalse  (header.isDataChecksumBad, L"the checksum covers the written block");
        Assert::AreEqual ((Byte) 0x11, data[9 * kBlock]);

        bad[DiskCopy42Container::kHeaderSize + 7] ^= 0xFF;
        AddFile (io, "bad.dsk", bad);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), damaged.Attach (io, "bad.dsk", kMaxBlocks, {}, error));
        Assert::IsTrue   (error.find (L"damaged") != std::wstring::npos);
    }


    //  ONE PATH IS ATTACHED TO ONE DRIVE, in any case or slash style, until
    //  that drive detaches it.
    //  A WOZ 3.5-INCH IMAGE ON A BLOCK DEVICE is its decoded blocks, write
    //  protected.
    TEST_METHOD (Woz35_DecodesToBlocks)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   media;
        std::vector<Byte>  blocks (Gcr35Track::kBlocksTwoSides * kBlock);
        std::vector<Byte>  woz;
        std::vector<Byte>  block (kBlock);
        std::wstring       error;



        for (size_t i = 0; i < blocks.size(); i++)
        {
            blocks[i] = (Byte) (i / kBlock);
        }

        woz = Woz35Builder::Build (blocks);
        io.files["d.woz"]  = woz;
        io.stamps["d.woz"] = FileStamp { woz.size(), 1 };

        Assert::AreEqual (S_OK, media.Attach (io, "d.woz", 65535, {}, error));
        Assert::AreEqual (Gcr35Track::kBlocksTwoSides, media.GetBlockCount());
        Assert::IsTrue (media.IsWriteProtected());

        Assert::AreEqual (S_OK, media.ReadBlock (1599, block));
        Assert::AreEqual ((Byte) (1599 & 0xFF), block[0]);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT), media.WriteBlock (0, block));
        Assert::AreEqual (S_OK, media.Detach());
    }


    //  A WOZ IMAGE WITH BLOCKS THAT DON'T DECODE fails to attach, and the
    //  error lists them.
    TEST_METHOD (Woz35_UndecodableBlocksFail)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   media;
        std::vector<Byte>  woz = Woz35Builder::Build (std::vector<Byte> (Gcr35Track::kBlocksTwoSides * kBlock, 0), { 1 });
        std::wstring       error;



        io.files["d.woz"]  = woz;
        io.stamps["d.woz"] = FileStamp { woz.size(), 1 };

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT), media.Attach (io, "d.woz", 65535, {}, error));
        Assert::IsFalse (media.IsAttached());
        Assert::IsTrue (error.find (L"12 of the WOZ image's blocks") != std::wstring::npos, error.c_str());
        Assert::IsTrue (error.find (L"12, 13") != std::wstring::npos, error.c_str());
    }


    TEST_METHOD (OneWriterPerPath)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   first;
        BlockDeviceMedia   second;
        std::wstring       error;



        AddFile (io, "C:/Disks/Shared.po", MakeBlocks (kBlocks));
        AddFile (io, "c:\\disks\\shared.PO", MakeBlocks (kBlocks));

        Assert::AreEqual (S_OK, AttachPath (first, io, "C:/Disks/Shared.po"));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION), second.Attach (io, "c:\\disks\\shared.PO", kMaxBlocks, {}, error));
        Assert::IsTrue   (BlockDeviceMedia::IsPathAttached ("C:/Disks/Shared.po"));

        Assert::AreEqual (S_OK, first.Detach());
        Assert::IsFalse  (BlockDeviceMedia::IsPathAttached ("C:/Disks/Shared.po"));
        Assert::AreEqual (S_OK, AttachPath (second, io, "c:\\disks\\shared.PO"));
    }


    //  A FILE CHANGED ON THE HOST WHILE ATTACHED IS REPORTED, and a flush
    //  writes nothing over it.
    TEST_METHOD (HostChangeBlocksTheFlush)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   media;
        std::vector<Byte>  block (kBlock, 0x77);



        AddFile (io, "h.po", MakeBlocks (kBlocks));

        Assert::AreEqual (S_OK, AttachPath (media, io, "h.po"));
        Assert::AreEqual (S_OK, media.WriteBlock (1, block));

        io.stamps["h.po"].modifiedUnix = 99;

        Assert::IsTrue   (media.HasChangedOnHost());
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_INVALID), media.Flush());
        Assert::AreEqual ((Byte) 1, io.files["h.po"][kBlock], L"the host's file is not overwritten");

        io.stamps["h.po"].modifiedUnix = 1;
    }


    //  A READ PAST THE LAST BLOCK FAILS, and a drive with no image is not
    //  ready.
    TEST_METHOD (OutOfRangeAndEmptyDrive)
    {
        FakeDiskFileIo     io;
        BlockDeviceMedia   media;
        std::vector<Byte>  block (kBlock);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_READY), media.ReadBlock (0, block));

        AddFile (io, "r.po", MakeBlocks (kBlocks));
        Assert::AreEqual (S_OK, AttachPath (media, io, "r.po"));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND), media.ReadBlock (kBlocks, block));
    }
};
