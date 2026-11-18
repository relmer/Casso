#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Devices/BlockDevice/ProFileDrive.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "Machines/Apple3/Apple3ProFileCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3ProFileTests
//
//  The ProFile's card in an Apple /// slot, driven as Apple's SOS .PROFILE
//  driver drives it: the CMD line's handshake, the six command bytes, a read
//  and a write of a block through the data registers, the status register's
//  BSY bit, and the reset line, on the 5 MB and the 10 MB drive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3ProFileTests)
{
public:

    static constexpr int       kSlot     = 4;
    static constexpr Word      kDataOut  = 0xC0C0;
    static constexpr Word      kDataIn   = 0xC0C1;
    static constexpr Word      kStatus   = 0xC0C2;
    static constexpr Word      kPage     = 0xC400;
    static constexpr uint32_t  kBlocks   = 64;
    static constexpr size_t    kBlock    = 512;
    static constexpr uint32_t  k5Mb      = 9728;
    static constexpr uint32_t  k10Mb     = 19456;


    struct Rig
    {
        MemoryBus           bus;
        FakeDiskFileIo      io;
        Apple3ProFileCard   card;

        explicit Rig (uint32_t capacity) : card (kSlot, bus)
        {
            std::vector<Byte>  image ((size_t) kBlocks * kBlock);
            std::wstring       error;



            for (size_t i = 0; i < image.size(); i++)
            {
                image[i] = (Byte) (i / kBlock);
            }

            io.files["p.po"]  = image;
            io.stamps["p.po"] = FileStamp { image.size(), 1 };

            card.SetDrive (std::make_unique<ProFileDrive> (kSlot, capacity));
            Assert::AreEqual (S_OK, card.GetDrive().GetMedia().Attach (io, "p.po", capacity, {}, error));
        }

        //  The driver's SNDCMD: wait for BSY idle, raise CMD, wait for BSY,
        //  read the response, answer $55 and drop CMD.
        Byte Handshake()
        {
            Byte  response = 0;



            Assert::AreEqual ((int) Apple3ProFileCard::kStatusIdle, (int) (card.Read (kStatus) & 0x80), L"BSY is idle before the handshake");

            (void) card.Read (kPage + Apple3ProFileCard::kCmdHigh);
            Assert::AreEqual (0, (int) (card.Read (kStatus) & 0x80), L"BSY is asserted");

            response = card.Read (kDataIn);
            card.Write (kDataOut, ProFileDrive::kAck);
            (void) card.Read (kPage + Apple3ProFileCard::kCmdLow);

            return response;
        }

        void SendCommand (Byte op, uint32_t block)
        {
            for (Byte value : { op, (Byte) (block >> 16), (Byte) (block >> 8), (Byte) block, (Byte) 10, (Byte) 3 })
            {
                card.Write (kDataOut, value);
            }
        }
    };


    static void CheckReadAndWrite (uint32_t capacity)
    {
        Rig  rig (capacity);



        Assert::AreEqual (ProFileDrive::kStateCommand, rig.Handshake());
        rig.SendCommand (0, 7);
        Assert::AreEqual (ProFileDrive::kStateRead, rig.Handshake());

        for (int i = 0; i < 4; i++)
        {
            Assert::AreEqual ((Byte) 0, rig.card.Read (kDataIn), L"status is zero for success");
        }

        for (size_t i = 0; i < kBlock; i++)
        {
            Assert::AreEqual ((Byte) 7, rig.card.Read (kDataIn));
        }

        Assert::AreEqual (ProFileDrive::kStateCommand, rig.Handshake());
        rig.SendCommand (2, 9);
        Assert::AreEqual (ProFileDrive::kStateWrite, rig.Handshake(), L"a write with verify");

        for (size_t i = 0; i < kBlock; i++)
        {
            rig.card.Write (kDataOut, 0x5E);
        }

        Assert::AreEqual (ProFileDrive::kStateWritten, rig.Handshake());
        Assert::AreEqual ((Byte) 0,    rig.card.Read (kDataIn));
        Assert::AreEqual ((Byte) 0x5E, rig.io.files["p.po"][9 * kBlock + 3]);
    }


    TEST_METHOD (ReadAndWrite_5Mb)
    {
        CheckReadAndWrite (k5Mb);
    }


    TEST_METHOD (ReadAndWrite_10Mb)
    {
        CheckReadAndWrite (k10Mb);
    }


    //  The reset line returns the drive to waiting for a command in the
    //  middle of an exchange.
    TEST_METHOD (ResetLine_ReturnsTheDriveToACommand)
    {
        Rig  rig (k5Mb);



        Assert::AreEqual (ProFileDrive::kStateCommand, rig.Handshake());
        rig.SendCommand (0, 7);

        (void) rig.card.Read (kPage + Apple3ProFileCard::kResetHigh);
        (void) rig.card.Read (kPage + Apple3ProFileCard::kResetLow);

        Assert::AreEqual (ProFileDrive::kStateCommand, rig.Handshake());
    }


    //  The direction lines don't start a handshake: BSY is idle after each.
    TEST_METHOD (DirectionLines_DontStartAHandshake)
    {
        Rig  rig (k5Mb);



        for (Byte offset : { (Byte) 1, (Byte) 5, (Byte) 3, (Byte) 7 })
        {
            (void) rig.card.Read ((Word) (kPage + offset));
            Assert::AreEqual ((int) Apple3ProFileCard::kStatusIdle, (int) (rig.card.Read (kStatus) & 0x80));
        }
    }
};
