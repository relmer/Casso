#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ProFileDrive.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProFileTests
//
//  The ProFile's protocol as its interface card's ROM runs it: handshakes
//  with state bytes, the six-byte command, a read and a write of a block,
//  the identification block, and the card's line latch and registers.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ProFileTests)
{
public:

    static constexpr uint32_t  kBlocks = 64;
    static constexpr size_t    kBlock  = 512;


    static void Attach (FakeDiskFileIo & io, ProFileDrive & drive)
    {
        std::vector<Byte>  image ((size_t) kBlocks * kBlock);
        std::wstring       error;



        for (size_t i = 0; i < image.size(); i++)
        {
            image[i] = (Byte) (i / kBlock);
        }

        io.files["p.po"]  = image;
        io.stamps["p.po"] = FileStamp { image.size(), 1 };

        Assert::AreEqual (S_OK, drive.GetMedia().Attach (io, "p.po", drive.GetCapacity(), {}, error));
    }


    //  One handshake: the state byte the drive offers, then the host's answer.
    static Byte Handshake (ProFileDrive & drive, Byte answer = ProFileDrive::kAck)
    {
        Byte  state = 0;



        drive.StartHandshake();
        Assert::IsTrue (drive.IsBusy(), L"BSY is asserted during a handshake");

        state = drive.ReadData();
        drive.WriteData (answer);
        drive.EndHandshake();

        return state;
    }


    static void SendCommand (ProFileDrive & drive, Byte op, uint32_t block)
    {
        Byte  bytes[] = { op, (Byte) (block >> 16), (Byte) (block >> 8), (Byte) block, 3, 0x0A };



        for (Byte value : bytes)
        {
            drive.WriteData (value);
        }
    }


    //  A READ: state $01, the command, state $02, four zero status bytes, then
    //  the block's 512 bytes; a new handshake drops the tags that follow.
    TEST_METHOD (ReadSendsStatusThenTheBlock)
    {
        FakeDiskFileIo  io;
        ProFileDrive    drive (5, 9728);



        Attach (io, drive);

        Assert::AreEqual (ProFileDrive::kStateCommand, Handshake (drive));
        SendCommand (drive, 0, 7);
        Assert::AreEqual (ProFileDrive::kStateRead, Handshake (drive));

        for (int i = 0; i < 4; i++)
        {
            Assert::AreEqual ((Byte) 0, drive.ReadData(), L"status is zero for success");
        }

        for (size_t i = 0; i < kBlock; i++)
        {
            Assert::AreEqual ((Byte) 7, drive.ReadData());
        }

        Assert::AreEqual (ProFileDrive::kStateCommand, Handshake (drive), L"the next handshake starts a new command");
    }


    //  A WRITE: state $04 after the command, the block, state $06, and the
    //  block is in the image.
    TEST_METHOD (WriteTakesTheBlock)
    {
        FakeDiskFileIo  io;
        ProFileDrive    drive (5, 9728);



        Attach (io, drive);

        Assert::AreEqual (ProFileDrive::kStateCommand, Handshake (drive));
        SendCommand (drive, 2, 9);
        Assert::AreEqual (ProFileDrive::kStateWrite, Handshake (drive));

        for (size_t i = 0; i < kBlock; i++)
        {
            drive.WriteData (0x5E);
        }

        Assert::AreEqual (ProFileDrive::kStateWritten, Handshake (drive));
        Assert::AreEqual ((Byte) 0, drive.ReadData());
        Assert::AreEqual ((Byte) 0x5E, io.files["p.po"][9 * kBlock + 3]);
    }


    //  AN ANSWER OTHER THAN $55 OFFERS THE SAME STATE AGAIN, and the reset
    //  operation, $FF, returns the drive to waiting for a command.
    TEST_METHOD (NakRepeatsAndResetReturns)
    {
        FakeDiskFileIo  io;
        ProFileDrive    drive (5, 9728);



        Attach (io, drive);

        Assert::AreEqual (ProFileDrive::kStateCommand, Handshake (drive, 0xAA));
        Assert::AreEqual (ProFileDrive::kStateCommand, Handshake (drive));
        SendCommand (drive, 0xFF, 0);
        Assert::AreEqual (ProFileDrive::kStateCommand, Handshake (drive));
    }


    //  BLOCK $FFFFFF IS THE IDENTIFICATION BLOCK: the name and the capacity.
    TEST_METHOD (IdentificationBlock)
    {
        FakeDiskFileIo     io;
        ProFileDrive       drive (5, 19456);
        std::vector<Byte>  block;



        Attach (io, drive);

        Handshake (drive);
        SendCommand (drive, 0, 0xFFFFFF);
        Handshake (drive);

        for (int i = 0; i < 4; i++)
        {
            drive.ReadData();
        }

        for (size_t i = 0; i < kBlock; i++)
        {
            block.push_back (drive.ReadData());
        }

        Assert::AreEqual (std::string ("PROFILE"), std::string (block.begin(), block.begin() + 7));
        Assert::AreEqual ((uint32_t) 19456, ((uint32_t) block[18] << 16) | ((uint32_t) block[19] << 8) | block[20]);
    }


    //  THE CARD: line 2 falling with line 3 set starts a handshake and $Cn00
    //  ends it; status bit 7 is BSY, 1 when idle; the ROM's last page is at
    //  $Cn00 and its first $700 bytes at $C800.
    TEST_METHOD (CardLinesAndRegisters)
    {
        constexpr Word     kIo = 0xC0D0;

        MemoryBus          bus;
        ProFileCard        card (5, bus);
        std::vector<Byte>  rom (ProFileCard::kRomSize);



        for (size_t i = 0; i < rom.size(); i++)
        {
            rom[i] = (Byte) (i >> 8);
        }

        Assert::AreEqual (S_OK, card.SetFirmware (rom));
        Assert::AreEqual ((Byte) 0x80, card.Read (kIo + 2), L"idle");

        card.OnSlotPageAccess (0xC504, false, 0);
        Assert::AreEqual ((Byte) 0x80, card.Read (kIo + 2), L"line 3 clear: a fetch at $C504 starts nothing");

        card.OnSlotPageAccess (0xC505, false, 0);
        card.OnSlotPageAccess (0xC507, false, 0);
        Assert::AreEqual ((Byte) 7, card.OnSlotPageAccess (0xC504, false, 0), L"the page is the ROM's last");
        Assert::AreEqual ((Byte) 0x00, card.Read (kIo + 2), L"busy in the handshake");
        Assert::AreEqual (ProFileDrive::kStateCommand, card.Read (kIo + 1));

        card.Write (kIo, ProFileDrive::kAck);
        card.OnSlotPageAccess (0xC500, false, 0);
        Assert::AreEqual ((Byte) 0x80, card.Read (kIo + 2), L"idle again");

        Assert::AreEqual ((Byte) 3, card.ReadExpansionRom (0xCB10));
    }
};
