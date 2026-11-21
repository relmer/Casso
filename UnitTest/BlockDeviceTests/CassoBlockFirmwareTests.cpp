#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/CassoBlockFirmware.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoBlockFirmwareTests
//
//  Casso's block firmware on the SCSI and ProFile cards: the ProDOS block
//  device page, and the registers that copy a block between the image and
//  the Apple's memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoBlockFirmwareTests)
{
public:

    static constexpr int       kSlot    = 7;
    static constexpr Word      kIoBase  = 0xC0F0;
    static constexpr size_t    kBlock   = 512;
    static constexpr uint32_t  kBlocks  = 100;
    static constexpr Word      kBuffer  = 0x0800;


    //  RAM at $0000-$BFFF, read and written through the bus.
    struct Ram
    {
        std::vector<Byte>  bytes = std::vector<Byte> (0xC000, 0);

        void MapInto (MemoryBus & bus)
        {
            for (int page = 0; page < 0xC0; page++)
            {
                bus.SetReadPage  (page, bytes.data() + page * 0x100);
                bus.SetWritePage (page, bytes.data() + page * 0x100);
            }
        }
    };


    //  Each byte of block b is b.
    static void AttachImage (FakeDiskFileIo & io, BlockDeviceMedia & media, const char * path, const WriteProtectInfo & protect = {})
    {
        std::vector<Byte>  image ((size_t) kBlocks * kBlock);
        std::wstring       error;



        for (size_t i = 0; i < image.size(); i++)
        {
            image[i] = (Byte) (i / kBlock);
        }

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 1 };

        Assert::AreEqual (S_OK, media.Attach (io, path, 65535, protect, error));
    }


    static Byte RunCommand (MemoryDevice & io, Byte command, Byte unit, Word block)
    {
        io.Write (kIoBase + CassoBlockFirmware::kCommandPort, command);
        io.Write (kIoBase + CassoBlockFirmware::kUnitPort,    unit);
        io.Write (kIoBase + CassoBlockFirmware::kBufferLow,   (Byte) (kBuffer & 0xFF));
        io.Write (kIoBase + CassoBlockFirmware::kBufferHigh,  (Byte) (kBuffer >> 8));
        io.Write (kIoBase + CassoBlockFirmware::kBlockLow,    (Byte) (block & 0xFF));
        io.Write (kIoBase + CassoBlockFirmware::kBlockHigh,   (Byte) (block >> 8));

        return io.Read (kIoBase + CassoBlockFirmware::kRunPort);
    }


    //  THE PAGE IS A SMARTPORT DEVICE'S: the ID bytes the boot ROM's slot
    //  scan looks for, extended calls, two volumes that read, write, format
    //  and report status, and the ProDOS entry, which jumps to the code that
    //  loads ProDOS's command from $42. SmartPort's entry is 3 bytes on.
    TEST_METHOD (Page_HasTheProDosBlockDeviceIds)
    {
        std::vector<Byte>  page;
        std::string        errors;



        Assert::AreEqual (S_OK, CassoBlockFirmware::AssemblePage (kSlot, page, errors), TextEncoding::NarrowToWide (errors).c_str());
        Assert::AreEqual ((size_t) 0x100, page.size());

        Assert::AreEqual ((Byte) 0x20, page[0x01]);
        Assert::AreEqual ((Byte) 0x00, page[0x03]);
        Assert::AreEqual ((Byte) 0x03, page[0x05]);
        Assert::AreEqual ((Byte) 0x00, page[0x07], L"SmartPort");
        Assert::AreEqual ((Byte) 0x80, page[0xFB], L"extended calls");
        Assert::AreEqual ((Byte) 0x00, page[0xFC]);
        Assert::AreEqual ((Byte) 0x00, page[0xFD]);
        Assert::AreEqual ((Byte) 0x1F, page[0xFE]);
        Assert::AreNotEqual ((Byte) 0x00, page[0xFF], L"not a Disk II's $00");

        Byte  entry  = page[0xFF];
        Byte  prodos = page[entry + 1];

        Assert::AreEqual ((Byte) 0x4C, page[entry],      L"JMP");
        Assert::AreEqual ((Byte) 0xC7, page[entry + 2],  L"within the page");
        Assert::AreEqual ((Byte) 0xA5, page[prodos],     L"LDA zero page");
        Assert::AreEqual ((Byte) 0x42, page[prodos + 1], L"ProDOS's command");
        Assert::AreEqual ((Byte) 0x68, page[entry + 3],  L"SmartPort's entry pulls the return address");
    }


    //  A READ COPIES THE BLOCK TO THE BUFFER, and the count registers return
    //  the image's size. The slot page is the firmware's, and the $C800
    //  window isn't selected.
    TEST_METHOD (ScsiCard_ReadsABlockIntoMemory)
    {
        MemoryBus       bus;
        Ram             ram;
        FakeDiskFileIo  io;
        AppleScsiCard   card (kSlot, bus);



        ram.MapInto (bus);
        AttachImage (io, card.GetTarget (6).GetMedia(), "a.po");
        Assert::AreEqual (S_OK, card.UseCassoFirmware());

        Assert::AreEqual ((Byte) 0x20, card.OnSlotPageAccess (0xC701, false, 0));
        Assert::IsFalse  (card.IsExpansionRomSelected());

        Assert::AreEqual (CassoBlockFirmware::kErrNone, RunCommand (card, 1, 0x70, 3));
        Assert::AreEqual ((Byte) 3, ram.bytes[kBuffer]);
        Assert::AreEqual ((Byte) 3, ram.bytes[kBuffer + kBlock - 1]);
        Assert::AreEqual ((Byte) 0, ram.bytes[kBuffer + kBlock], L"one block only");

        Assert::AreEqual ((Byte) kBlocks, card.Read (kIoBase + CassoBlockFirmware::kCountLow));
        Assert::AreEqual ((Byte) 0,       card.Read (kIoBase + CassoBlockFirmware::kCountHigh));
        Assert::AreNotEqual ((uint64_t) 0, card.GetTarget (6).GetLastActivityCycle(), L"the lamp sees the read");
    }


    //  A WRITE COPIES THE BUFFER TO THE IMAGE; a write-protected image and a
    //  missing drive 2 return ProDOS's errors.
    TEST_METHOD (ScsiCard_WritesABlock_AndReportsErrors)
    {
        MemoryBus          bus;
        Ram                ram;
        FakeDiskFileIo     io;
        AppleScsiCard      card (kSlot, bus);
        WriteProtectInfo   locked;
        std::vector<Byte>  block (kBlock);



        ram.MapInto (bus);
        AttachImage (io, card.GetTarget (0).GetMedia(), "a.po");
        Assert::AreEqual (S_OK, card.UseCassoFirmware());

        std::fill (ram.bytes.begin() + kBuffer, ram.bytes.begin() + kBuffer + kBlock, (Byte) 0x5A);

        Assert::AreEqual (CassoBlockFirmware::kErrNone, RunCommand (card, 2, 0x70, 4));
        Assert::AreEqual (S_OK, card.GetTarget (0).GetMedia().ReadBlock (4, block));
        Assert::AreEqual ((Byte) 0x5A, block[0]);
        Assert::AreEqual ((Byte) 0x5A, block[kBlock - 1]);

        Assert::AreEqual (CassoBlockFirmware::kErrNoDevice, RunCommand (card, 0, 0xF0, 0), L"no drive 2");

        locked.userSetting = true;
        AttachImage (io, card.GetTarget (1).GetMedia(), "b.po", locked);
        Assert::AreEqual (CassoBlockFirmware::kErrWriteProtected, RunCommand (card, 2, 0xF0, 0), L"drive 2 is locked");
        Assert::AreEqual (CassoBlockFirmware::kErrNone,           RunCommand (card, 1, 0xF0, 0), L"and reads");
    }


    //  Runs a SmartPort call: the command byte and the parameter list's
    //  address follow the caller's JSR at $0300, and the list is at $0310.
    static Byte RunSmartPort (MemoryDevice & io, Ram & ram, Byte command, std::initializer_list<Byte> list)
    {
        Word  call = 0x0302;



        ram.bytes[call + 1] = command;
        ram.bytes[call + 2] = 0x10;
        ram.bytes[call + 3] = 0x03;
        ram.bytes[call + 4] = 0x00;
        ram.bytes[call + 5] = 0x00;
        std::copy (list.begin(), list.end(), ram.bytes.begin() + 0x0310);

        io.Write (kIoBase + CassoBlockFirmware::kReturnLow,  (Byte) (call & 0xFF));
        io.Write (kIoBase + CassoBlockFirmware::kReturnHigh, (Byte) (call >> 8));

        return io.Read (kIoBase + CassoBlockFirmware::kSmartPort);
    }


    //  SMARTPORT SERVES EVERY TARGET WITH AN IMAGE: the card's status
    //  returns the unit count, a unit's status its block count, and the
    //  device information block the card's name. The return address skips
    //  the command and the parameter list's address.
    TEST_METHOD (SmartPort_ReportsEveryUnit)
    {
        MemoryBus       bus;
        Ram             ram;
        FakeDiskFileIo  io;
        AppleScsiCard   card (kSlot, bus);
        Word            reply = 0x0400;



        ram.MapInto (bus);
        AttachImage (io, card.GetTarget (0).GetMedia(), "a.po");
        AttachImage (io, card.GetTarget (2).GetMedia(), "b.po");
        AttachImage (io, card.GetTarget (5).GetMedia(), "c.po");
        Assert::AreEqual (S_OK, card.UseCassoFirmware());

        Assert::AreEqual (CassoBlockFirmware::kErrNone, RunSmartPort (card, ram, 0x00, { 3, 0, 0x00, 0x04, 0 }));
        Assert::AreEqual ((Byte) 3,    ram.bytes[reply],     L"three units");
        Assert::AreEqual ((Byte) 0x05, card.Read (kIoBase + CassoBlockFirmware::kResumeLow));
        Assert::AreEqual ((Byte) 0x03, card.Read (kIoBase + CassoBlockFirmware::kResumeHigh));
        Assert::AreEqual ((Byte) 8,    card.Read (kIoBase + CassoBlockFirmware::kCountLow));

        Assert::AreEqual (CassoBlockFirmware::kErrNone, RunSmartPort (card, ram, 0x00, { 3, 3, 0x00, 0x04, 3 }));
        Assert::AreEqual ((Byte) 0xF8,    ram.bytes[reply],     L"a block device, online, that reads, writes and formats");
        Assert::AreEqual ((Byte) kBlocks, ram.bytes[reply + 1]);
        Assert::AreEqual ((Byte) 7,       ram.bytes[reply + 4], L"the name's length");
        Assert::AreEqual ((Byte) 'S',     ram.bytes[reply + 5]);
        Assert::AreEqual ((Byte) ' ',     ram.bytes[reply + 20], L"padded");
        Assert::AreEqual ((Byte) 0x03,    ram.bytes[reply + 21], L"a SCSI hard disk");
        Assert::AreEqual ((Byte) 25,      card.Read (kIoBase + CassoBlockFirmware::kCountLow));

        Assert::AreEqual (CassoBlockFirmware::kErrBadUnit, RunSmartPort (card, ram, 0x00, { 3, 4, 0x00, 0x04, 0 }));
    }


    //  SMARTPORT READS AND WRITES ANY UNIT: a standard call has a 3-byte
    //  block, an extended call 4-byte buffer and block fields. A block past
    //  the image's end returns SmartPort's bad block error.
    TEST_METHOD (SmartPort_ReadsAndWritesAnyUnit)
    {
        MemoryBus          bus;
        Ram                ram;
        FakeDiskFileIo     io;
        AppleScsiCard      card (kSlot, bus);
        std::vector<Byte>  block (kBlock);



        ram.MapInto (bus);
        AttachImage (io, card.GetTarget (1).GetMedia(), "a.po");
        AttachImage (io, card.GetTarget (3).GetMedia(), "b.po");
        AttachImage (io, card.GetTarget (4).GetMedia(), "c.po");
        Assert::AreEqual (S_OK, card.UseCassoFirmware());

        Assert::AreEqual (CassoBlockFirmware::kErrNone, RunSmartPort (card, ram, 0x01, { 3, 3, 0x00, 0x08, 42, 0, 0 }));
        Assert::AreEqual ((Byte) 42, ram.bytes[kBuffer]);
        Assert::AreEqual ((Byte) 42, ram.bytes[kBuffer + kBlock - 1]);
        Assert::AreEqual ((Byte) 0x02, card.Read (kIoBase + CassoBlockFirmware::kCountHigh), L"512 bytes");

        std::fill (ram.bytes.begin() + kBuffer, ram.bytes.begin() + kBuffer + kBlock, (Byte) 0xA5);
        Assert::AreEqual (CassoBlockFirmware::kErrNone, RunSmartPort (card, ram, 0x42, { 3, 3, 0x00, 0x08, 0, 0, 7, 0, 0, 0 }));
        Assert::AreEqual ((Byte) 0x07, card.Read (kIoBase + CassoBlockFirmware::kResumeLow), L"an extended call is 5 bytes");
        Assert::AreEqual (S_OK, card.GetTarget (4).GetMedia().ReadBlock (7, block));
        Assert::AreEqual ((Byte) 0xA5, block[0]);

        Assert::AreEqual (CassoBlockFirmware::kErrBadBlock,      RunSmartPort (card, ram, 0x01, { 3, 1, 0x00, 0x08, kBlocks, 0, 0 }));
        Assert::AreEqual (CassoBlockFirmware::kErrBadParamCount, RunSmartPort (card, ram, 0x01, { 2, 1, 0x00, 0x08, 0, 0, 0 }));
        Assert::AreEqual (CassoBlockFirmware::kErrBadCommand,    RunSmartPort (card, ram, 0x08, { 4, 1, 0x00, 0x08, 0, 0, 0 }));
    }


    //  THE PROFILE CARD'S DRIVE IS DRIVE 1.
    TEST_METHOD (ProFileCard_ReadsItsDrive)
    {
        MemoryBus       bus;
        Ram             ram;
        FakeDiskFileIo  io;
        ProFileCard     card (5, bus);
        Word            ioBase = 0xC0D0;



        ram.MapInto (bus);
        AttachImage (io, card.GetDrive().GetMedia(), "p.po");
        Assert::AreEqual (S_OK, card.UseCassoFirmware());
        Assert::IsTrue (card.IsCassoFirmware());

        card.Write (ioBase + CassoBlockFirmware::kCommandPort, 1);
        card.Write (ioBase + CassoBlockFirmware::kUnitPort,    0x50);
        card.Write (ioBase + CassoBlockFirmware::kBufferLow,   0x00);
        card.Write (ioBase + CassoBlockFirmware::kBufferHigh,  0x08);
        card.Write (ioBase + CassoBlockFirmware::kBlockLow,    9);
        card.Write (ioBase + CassoBlockFirmware::kBlockHigh,   0);

        Assert::AreEqual (CassoBlockFirmware::kErrNone, card.Read (ioBase + CassoBlockFirmware::kRunPort));
        Assert::AreEqual ((Byte) 9, ram.bytes[kBuffer]);
    }
};





