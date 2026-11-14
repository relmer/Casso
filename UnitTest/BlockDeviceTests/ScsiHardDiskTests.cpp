#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceEventRing.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiHardDiskTests
//
//  The hard disk target's commands, sense data, write protection, real
//  timing and model label, called as the bus calls them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ScsiHardDiskTests)
{
public:

    static constexpr size_t    kBlock  = 512;
    static constexpr uint32_t  kBlocks = 100;
    static constexpr uint32_t  kClock  = 1'000'000;


    static void AttachBlocks (FakeDiskFileIo & io, ScsiHardDisk & disk, uint32_t blocks, const WriteProtectInfo & protect = {})
    {
        std::vector<Byte>  image ((size_t) blocks * kBlock);
        std::wstring       error;



        for (size_t i = 0; i < image.size(); i++)
        {
            image[i] = (Byte) (i / kBlock);
        }

        io.files["h.po"]  = image;
        io.stamps["h.po"] = FileStamp { image.size(), 1 };

        Assert::AreEqual (S_OK, disk.GetMedia().Attach (io, "h.po", 65535, protect, error));
    }


    static ScsiCommandResult Run (ScsiHardDisk & disk, std::vector<Byte> cdb, uint64_t now = 0)
    {
        ScsiCommandResult  result;



        disk.BeginCommand (cdb, now, result);

        return result;
    }


    //  A DRIVE WITH NO IMAGE DOES NOT ANSWER SELECTION; with one, TEST UNIT
    //  READY is GOOD.
    TEST_METHOD (SelectableOnlyWithAnImage)
    {
        FakeDiskFileIo  io;
        ScsiHardDisk    disk (6, 7);



        Assert::IsFalse (disk.IsSelectable());

        AttachBlocks (io, disk, kBlocks);

        Assert::IsTrue   (disk.IsSelectable());
        Assert::AreEqual ((int) ScsiStatus::Good, (int) Run (disk, { 0x00, 0, 0, 0, 0, 0 }).status);
    }


    //  AN UNKNOWN COMMAND IS ILLEGAL REQUEST, which REQUEST SENSE returns
    //  once and then clears.
    TEST_METHOD (RequestSenseReturnsAndClears)
    {
        FakeDiskFileIo     io;
        ScsiHardDisk       disk (6, 7);
        ScsiCommandResult  sense;



        AttachBlocks (io, disk, kBlocks);

        Assert::AreEqual ((int) ScsiStatus::CheckCondition, (int) Run (disk, { 0xC7, 0, 0, 0, 0, 0 }).status);

        sense = Run (disk, { 0x03, 0, 0, 0, 18, 0 });

        Assert::AreEqual ((size_t) 18, sense.dataIn.size());
        Assert::AreEqual ((Byte) 0x70, sense.dataIn[0]);
        Assert::AreEqual (ScsiHardDisk::kSenseIllegalRequest, sense.dataIn[2]);
        Assert::AreEqual (ScsiHardDisk::kAscInvalidOpcode,    sense.dataIn[12]);

        sense = Run (disk, { 0x03, 0, 0, 0, 18, 0 });
        Assert::AreEqual (ScsiHardDisk::kSenseNoSense, sense.dataIn[2], L"the sense is cleared after it is read");
    }


    //  INQUIRY: a direct-access device, cut to the allocation length.
    TEST_METHOD (InquiryIsADirectAccessDevice)
    {
        FakeDiskFileIo     io;
        ScsiHardDisk       disk (6, 7);
        ScsiCommandResult  inquiry;



        AttachBlocks (io, disk, kBlocks);

        inquiry = Run (disk, { 0x12, 0, 0, 0, 36, 0 });

        Assert::AreEqual ((size_t) 36, inquiry.dataIn.size());
        Assert::AreEqual ((Byte) 0, inquiry.dataIn[0]);
        Assert::AreEqual ((size_t) 5, Run (disk, { 0x12, 0, 0, 0, 5, 0 }).dataIn.size());
    }


    //  READ(6) AND READ(10) RETURN THE BLOCKS, and a read past the end is
    //  ILLEGAL REQUEST with LOGICAL BLOCK ADDRESS OUT OF RANGE.
    TEST_METHOD (ReadsAndOutOfRange)
    {
        FakeDiskFileIo     io;
        ScsiHardDisk       disk (6, 7);
        ScsiCommandResult  read6;
        ScsiCommandResult  read10;



        AttachBlocks (io, disk, kBlocks);

        read6  = Run (disk, { 0x08, 0, 0, 3, 2, 0 });
        read10 = Run (disk, { 0x28, 0, 0, 0, 0, 97, 0, 0, 3, 0 });

        Assert::AreEqual ((size_t) 2 * kBlock, read6.dataIn.size());
        Assert::AreEqual ((Byte) 4, read6.dataIn[kBlock]);
        Assert::AreEqual ((Byte) 99, read10.dataIn[2 * kBlock]);

        Assert::AreEqual ((int) ScsiStatus::CheckCondition, (int) Run (disk, { 0x28, 0, 0, 0, 0, 98, 0, 0, 3, 0 }).status);
        Assert::AreEqual (ScsiHardDisk::kSenseIllegalRequest, disk.GetSenseKey());
    }


    //  WRITE(6) AND WRITE(10) TAKE THEIR DATA and put it in the image; on a
    //  protected image the status is DATA PROTECT.
    TEST_METHOD (WritesAndProtection)
    {
        FakeDiskFileIo     io;
        ScsiHardDisk       disk (6, 7);
        ScsiHardDisk       locked (5, 7);
        ScsiCommandResult  begin;
        WriteProtectInfo   readOnly;
        std::vector<Byte>  data (2 * kBlock, 0x3C);
        std::vector<Byte>  block (kBlock);
        uint64_t           delay = 0;



        AttachBlocks (io, disk, kBlocks);

        begin = Run (disk, { 0x2A, 0, 0, 0, 0, 10, 0, 0, 2, 0 });
        Assert::AreEqual (data.size(), begin.dataOutLength);
        Assert::AreEqual ((int) ScsiStatus::Good, (int) disk.FinishDataOut (data, 0, delay));
        Assert::AreEqual (S_OK, disk.GetMedia().ReadBlock (11, block));
        Assert::AreEqual ((Byte) 0x3C, block[7]);

        readOnly.readOnlyFile = true;
        io.files["l.po"]  = std::vector<Byte> (kBlocks * kBlock);
        io.stamps["l.po"] = FileStamp { kBlocks * kBlock, 1 };
        {
            std::wstring  error;
            Assert::AreEqual (S_OK, locked.GetMedia().Attach (io, "l.po", 65535, readOnly, error));
        }

        begin = Run (locked, { 0x0A, 0, 0, 1, 1, 0 });
        Assert::AreEqual ((int) ScsiStatus::CheckCondition, (int) locked.FinishDataOut (std::span<const Byte> (data).first (kBlock), 0, delay));
        Assert::AreEqual (ScsiHardDisk::kSenseDataProtect, locked.GetSenseKey());
    }


    //  READ CAPACITY RETURNS THE LAST BLOCK AND 512, and MODE SENSE reports
    //  write protection.
    TEST_METHOD (CapacityAndModeSense)
    {
        FakeDiskFileIo     io;
        ScsiHardDisk       disk (6, 7);
        ScsiCommandResult  capacity;
        ScsiCommandResult  mode;



        AttachBlocks (io, disk, kBlocks);
        disk.GetMedia().SetUserWriteProtected (true);

        capacity = Run (disk, { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
        mode     = Run (disk, { 0x1A, 0, 0, 0, 12, 0 });

        Assert::AreEqual ((Byte) (kBlocks - 1), capacity.dataIn[3]);
        Assert::AreEqual ((Byte) 2,             capacity.dataIn[6], L"512-byte blocks");
        Assert::AreEqual ((Byte) 0x80,          mode.dataIn[2], L"the write-protect bit");
    }


    //  WITH REAL TIMING, the drive is NOT READY until it spins up, and a read
    //  waits for a seek; without it, both are immediate.
    TEST_METHOD (RealTiming)
    {
        FakeDiskFileIo     io;
        ScsiHardDisk       disk (6, 7);



        AttachBlocks (io, disk, kBlocks);

        Assert::AreEqual ((uint64_t) 0, Run (disk, { 0x08, 0, 0, 0, 1, 0 }).delayCycles);

        disk.SetRealTiming (true, kClock, 0);

        Assert::AreEqual ((int) ScsiStatus::CheckCondition, (int) Run (disk, { 0x00, 0, 0, 0, 0, 0 }, kClock).status);
        Assert::AreEqual (ScsiHardDisk::kSenseNotReady, disk.GetSenseKey());

        Assert::AreEqual ((int) ScsiStatus::Good, (int) Run (disk, { 0x00, 0, 0, 0, 0, 0 }, 60ULL * kClock).status);
        Assert::IsTrue   (Run (disk, { 0x08, 0, 0, 0, 1, 0 }, 60ULL * kClock).delayCycles > 0);
    }


    //  THE MODEL LABEL IS THE IMAGE'S SIZE IN MB THEN "SC", and "20SC" with
    //  no image.
    TEST_METHOD (ModelLabelFollowsTheImage)
    {
        FakeDiskFileIo  io;
        ScsiHardDisk    disk (6, 7);



        Assert::AreEqual (std::wstring (L"20SC"), disk.GetModelLabel());

        AttachBlocks (io, disk, 65535);
        Assert::AreEqual (std::wstring (L"32SC"), disk.GetModelLabel());

        disk.GetMedia().Detach();
        AttachBlocks (io, disk, 10240);
        Assert::AreEqual (std::wstring (L"5SC"), disk.GetModelLabel());
    }


    //  EACH COMMAND IS AN EVENT, with its blocks and result.
    TEST_METHOD (CommandsAreEvents)
    {
        FakeDiskFileIo        io;
        ScsiHardDisk          disk (6, 7);
        BlockDeviceEventRing  ring;
        BlockDeviceEvent      event;



        AttachBlocks (io, disk, kBlocks);
        disk.SetBlockDeviceEventSink (&ring);

        Run (disk, { 0x08, 0, 0, 12, 4, 0 });

        Assert::IsTrue   (ring.TryPop (event));
        Assert::AreEqual ((int) BlockDeviceOp::Read, (int) event.op);
        Assert::AreEqual (12u, event.firstBlock);
        Assert::AreEqual (4u,  event.blockCount);
        Assert::AreEqual ((Byte) 6, event.unit);
    }
};
