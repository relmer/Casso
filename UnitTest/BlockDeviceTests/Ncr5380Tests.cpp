#include "Pch.h"

#include "Devices/BlockDevice/Ncr5380.h"
#include "Devices/BlockDevice/ScsiBus.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Ncr5380Tests
//
//  The 5380's registers, arbitration, selection, the phase-match bit, and
//  transfers by programmed I/O and by DMA, against a hard disk target on the
//  bus. The sequences are the data sheet's initiator procedures.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Ncr5380Tests)
{
public:

    static constexpr int       kOwnId    = 7;
    static constexpr int       kTargetId = 6;
    static constexpr uint32_t  kBlocks   = 64;
    static constexpr size_t    kBlock    = 512;

    //  Register numbers.
    static constexpr int  kRegData   = 0;
    static constexpr int  kRegIcr    = 1;
    static constexpr int  kRegMode   = 2;
    static constexpr int  kRegTcr    = 3;
    static constexpr int  kRegStatus = 4;
    static constexpr int  kRegBsr    = 5;
    static constexpr int  kRegIdr    = 6;
    static constexpr int  kRegReset  = 7;

    //  Target command register phases: MSG, C/D, I/O.
    static constexpr Byte  kPhaseDataOut = 0x00;
    static constexpr Byte  kPhaseDataIn  = 0x01;
    static constexpr Byte  kPhaseCommand = 0x02;
    static constexpr Byte  kPhaseStatus  = 0x03;
    static constexpr Byte  kPhaseMsgIn   = 0x07;


    struct Rig
    {
        FakeDiskFileIo  io;
        ScsiBus         bus;
        Ncr5380         chip { bus };
        ScsiHardDisk    disk { kTargetId, 7 };

        Rig()
        {
            std::vector<Byte>  image ((size_t) kBlocks * kBlock);
            std::wstring       error;



            for (size_t i = 0; i < image.size(); i++)
            {
                image[i] = (Byte) (i / kBlock);
            }

            io.files["d.po"]  = image;
            io.stamps["d.po"] = FileStamp { image.size(), 1 };

            Assert::AreEqual (S_OK, disk.GetMedia().Attach (io, "d.po", 65535, {}, error));
            bus.AttachTarget (&disk);
        }
    };


    //  Arbitration then selection of `id`, as the data sheet's procedure has
    //  it. True when the target answered with BSY.
    static bool Select (Ncr5380 & chip, int id)
    {
        bool  answered = false;



        chip.WriteRegister (kRegData, (Byte) (1 << kOwnId));
        chip.WriteRegister (kRegMode, Ncr5380::kModeArbitrate);
        Assert::IsTrue ((chip.ReadRegister (kRegIcr) & Ncr5380::kIcrAip) != 0, L"arbitration in progress");

        chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertSel | Ncr5380::kIcrAssertBsy);
        chip.WriteRegister (kRegData, (Byte) ((1 << kOwnId) | (1 << id)));
        chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertSel | Ncr5380::kIcrAssertBsy | Ncr5380::kIcrAssertData);
        chip.WriteRegister (kRegMode, 0);
        chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertSel | Ncr5380::kIcrAssertData);

        answered = (chip.ReadRegister (kRegStatus) & Ncr5380::kBusBsy) != 0;

        chip.WriteRegister (kRegIcr, 0);

        return answered;
    }


    //  One byte out by programmed I/O in `phase`.
    static void SendByte (Ncr5380 & chip, Byte phase, Byte value)
    {
        chip.WriteRegister (kRegTcr, phase);
        Assert::IsTrue ((chip.ReadRegister (kRegStatus) & Ncr5380::kBusReq) != 0, L"the target requests a byte");

        chip.WriteRegister (kRegData, value);
        chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertData);
        chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertData | Ncr5380::kIcrAssertAck);
        chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertData);
        chip.WriteRegister (kRegIcr,  0);
    }


    //  One byte in by programmed I/O in `phase`.
    static Byte ReceiveByte (Ncr5380 & chip, Byte phase)
    {
        Byte  value = 0;



        chip.WriteRegister (kRegTcr, phase);
        Assert::IsTrue ((chip.ReadRegister (kRegBsr) & Ncr5380::kBsrPhaseMatch) != 0, L"the bus is in the expected phase");

        value = chip.ReadRegister (kRegData);
        chip.WriteRegister (kRegIcr, Ncr5380::kIcrAssertAck);
        chip.WriteRegister (kRegIcr, 0);

        return value;
    }


    static void SendCommand (Ncr5380 & chip, std::initializer_list<Byte> cdb)
    {
        for (Byte value : cdb)
        {
            SendByte (chip, kPhaseCommand, value);
        }
    }


    //  REGISTERS READ BACK what was written, in the bits the chip keeps.
    TEST_METHOD (RegistersReadBack)
    {
        Rig  rig;



        rig.chip.WriteRegister (kRegMode, Ncr5380::kModeDma);
        rig.chip.WriteRegister (kRegTcr,  0xFF);
        rig.chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertAtn);

        Assert::AreEqual ((Byte) Ncr5380::kModeDma,      rig.chip.ReadRegister (kRegMode));
        Assert::AreEqual ((Byte) 0x0F,                   rig.chip.ReadRegister (kRegTcr), L"the target command register has four bits");
        Assert::AreEqual ((Byte) Ncr5380::kIcrAssertAtn, rig.chip.ReadRegister (kRegIcr));
        Assert::IsTrue   ((rig.chip.ReadRegister (kRegBsr) & Ncr5380::kBsrAtn) != 0, L"ATN is on the bus");
    }


    //  ARBITRATION PUTS THE CHIP'S ID ON THE BUS, and a selection with no
    //  target at the ID is not answered.
    TEST_METHOD (ArbitrationAndSelection)
    {
        Rig  rig;



        rig.chip.WriteRegister (kRegData, (Byte) (1 << kOwnId));
        rig.chip.WriteRegister (kRegMode, Ncr5380::kModeArbitrate);

        Assert::AreEqual ((Byte) (1 << kOwnId), rig.chip.ReadRegister (kRegData));
        Assert::IsTrue   ((rig.chip.ReadRegister (kRegIcr) & Ncr5380::kIcrLa) == 0, L"arbitration is never lost");

        rig.chip.WriteRegister (kRegMode, 0);

        Assert::IsFalse (Select (rig.chip, 2), L"no target at ID 2");
        Assert::IsTrue  (Select (rig.chip, kTargetId));
        Assert::AreEqual ((int) ScsiPhase::Command, (int) rig.bus.GetPhase());
    }


    //  THE PHASE-MATCH BIT COMPARES THE BUS'S PHASE WITH THE TARGET COMMAND
    //  REGISTER.
    TEST_METHOD (PhaseMatchFollowsTheTargetCommandRegister)
    {
        Rig  rig;



        Assert::IsTrue (Select (rig.chip, kTargetId));

        rig.chip.WriteRegister (kRegTcr, kPhaseCommand);
        Assert::IsTrue ((rig.chip.ReadRegister (kRegBsr) & Ncr5380::kBsrPhaseMatch) != 0);

        rig.chip.WriteRegister (kRegTcr, kPhaseDataIn);
        Assert::IsTrue ((rig.chip.ReadRegister (kRegBsr) & Ncr5380::kBsrPhaseMatch) == 0);
    }


    //  TEST UNIT READY BY PROGRAMMED I/O: command, status GOOD, message
    //  COMMAND COMPLETE, and the bus is free.
    TEST_METHOD (TestUnitReadyByProgrammedIo)
    {
        Rig  rig;



        Assert::IsTrue (Select (rig.chip, kTargetId));
        SendCommand (rig.chip, { 0x00, 0, 0, 0, 0, 0 });

        Assert::AreEqual ((Byte) 0, ReceiveByte (rig.chip, kPhaseStatus));
        Assert::AreEqual ((Byte) 0, ReceiveByte (rig.chip, kPhaseMsgIn));
        Assert::AreEqual ((int) ScsiPhase::BusFree, (int) rig.bus.GetPhase());
        Assert::IsTrue   ((rig.chip.ReadRegister (kRegStatus) & Ncr5380::kBusBsy) == 0);
    }


    //  A READ BY DMA: DRQ for each byte, the bytes of the block through the
    //  DACK port, then a phase mismatch at status raises the interrupt, which
    //  register 7 clears.
    TEST_METHOD (ReadByDma)
    {
        Rig    rig;
        size_t count = 0;



        Assert::IsTrue (Select (rig.chip, kTargetId));
        SendCommand (rig.chip, { 0x08, 0, 0, 5, 1, 0 });

        rig.chip.WriteRegister (kRegTcr,   kPhaseDataIn);
        rig.chip.WriteRegister (kRegMode,  Ncr5380::kModeDma);
        rig.chip.WriteRegister (kRegReset, 0);

        while (rig.chip.IsDrq())
        {
            Assert::AreEqual ((Byte) 5, rig.chip.ReadDma());
            count++;
        }

        Assert::AreEqual (kBlock, count);
        Assert::IsTrue   (rig.chip.IsIrq(), L"the status phase is a phase mismatch");
        Assert::AreEqual ((Byte) 5, rig.chip.ReadRegister (kRegIdr), L"the input data register has the last byte");

        rig.chip.ReadRegister (kRegReset);
        rig.chip.WriteRegister (kRegMode, 0);
        Assert::IsFalse  (rig.chip.IsIrq());
        Assert::AreEqual ((Byte) 0, ReceiveByte (rig.chip, kPhaseStatus));
        Assert::AreEqual ((Byte) 0, ReceiveByte (rig.chip, kPhaseMsgIn));
    }


    //  A WRITE BY DMA: the bytes through the DACK port reach the image.
    TEST_METHOD (WriteByDma)
    {
        Rig    rig;
        size_t count = 0;



        Assert::IsTrue (Select (rig.chip, kTargetId));
        SendCommand (rig.chip, { 0x0A, 0, 0, 9, 1, 0 });

        rig.chip.WriteRegister (kRegTcr,  kPhaseDataOut);
        rig.chip.WriteRegister (kRegIcr,  Ncr5380::kIcrAssertData);
        rig.chip.WriteRegister (kRegMode, Ncr5380::kModeDma);
        rig.chip.WriteRegister (kRegBsr,  0);

        while (rig.chip.IsDrq())
        {
            rig.chip.WriteDma (0xEE);
            count++;
        }

        rig.chip.WriteRegister (kRegMode, 0);
        rig.chip.WriteRegister (kRegIcr,  0);

        Assert::AreEqual (kBlock, count);
        Assert::AreEqual ((Byte) 0, ReceiveByte (rig.chip, kPhaseStatus));
        Assert::AreEqual ((Byte) 0, ReceiveByte (rig.chip, kPhaseMsgIn));
        Assert::AreEqual (S_OK, rig.disk.GetMedia().Flush());
        Assert::AreEqual ((Byte) 0xEE, rig.io.files["d.po"][9 * kBlock + 100]);
    }


    //  RST FREES THE BUS mid-command, and the chip's reset clears its
    //  registers.
    TEST_METHOD (ResetLineAndChipReset)
    {
        Rig  rig;



        Assert::IsTrue (Select (rig.chip, kTargetId));

        rig.chip.WriteRegister (kRegIcr, Ncr5380::kIcrAssertRst);
        rig.chip.WriteRegister (kRegIcr, 0);

        Assert::AreEqual ((int) ScsiPhase::BusFree, (int) rig.bus.GetPhase());

        rig.chip.WriteRegister (kRegMode, Ncr5380::kModeDma);
        rig.chip.Reset();

        Assert::AreEqual ((Byte) 0, rig.chip.ReadRegister (kRegMode));
    }
};
