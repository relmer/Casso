#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/Iwm.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDriveBus
//
//  A drive whose disk is a loop of bits, recording what is written and the
//  phases it was given. `quiet` is what GetQuietUntil returns.
//
////////////////////////////////////////////////////////////////////////////////

class FakeDriveBus : public IDiskDriveBus
{
public:
    void      SetPhases (bool ph0, bool ph1, bool ph2, bool ph3) override { phases = { ph0, ph1, ph2, ph3 }; }
    bool      GetSense() const override { return sense; }
    Byte      ReadCell (uint64_t) override { Byte b = loop.empty() ? (Byte) 0 : loop[at % loop.size()]; at++; return b; }
    void      WriteCell (Byte bit, uint64_t) override { written.push_back (bit); }
    void      Advance   (uint64_t elapsedNs) override { elapsed += elapsedNs; }
    void      SetEnabled (bool isEnabled) override { enabled = isEnabled; }
    uint32_t  GetCellNs() const override { return kCellNs; }
    uint64_t  GetQuietUntil() const override { return quiet; }

    static constexpr uint32_t  kCellNs = 2000;

    std::vector<Byte>    loop;
    std::vector<Byte>    written;
    std::array<bool, 4>  phases  = {};
    size_t               at      = 0;
    uint64_t             elapsed = 0;
    uint64_t             quiet   = 0;
    bool                 sense   = true;
    bool                 enabled = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IwmTests
//
//  The IWM's switches and registers, bits moving at the drive's cell time,
//  and the SWIM's ISM mode as Apple's 3.5 controller firmware checks it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IwmTests)
{
public:

    static constexpr int  kMotorOff = 0x8;
    static constexpr int  kMotorOn  = 0x9;
    static constexpr int  kQ6Off    = 0xC;
    static constexpr int  kQ6On     = 0xD;
    static constexpr int  kQ7Off    = 0xE;
    static constexpr int  kQ7On     = 0xF;


    static void AppendByte (std::vector<Byte> & bits, Byte value)
    {
        for (int bit = 7; bit >= 0; bit--)
        {
            bits.push_back ((Byte) ((value >> bit) & 1));
        }
    }


    static void WriteMode (Iwm & iwm, Byte mode)
    {
        iwm.Access (kQ6On, false, 0, 0);
        iwm.Access (kQ7On, true, mode, 0);
        iwm.Access (kQ7Off, false, 0, 0);
        iwm.Access (kQ6Off, false, 0, 0);
    }


    TEST_METHOD (Status_ReportsModeSenseAndEnable)
    {
        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          status = 0;



        iwm.AttachDrive (0, &drive);
        WriteMode (iwm, 0x0F);

        iwm.Access (kQ6On, false, 0, 0);
        status = iwm.Access (kQ7Off, false, 0, 0);
        Assert::AreEqual ((Byte) 0x8F, status, L"sense high, disabled, mode $0F");

        drive.sense = false;
        iwm.Access (kMotorOn, false, 0, 0);
        status = iwm.Access (kQ7Off, false, 0, 0);
        Assert::AreEqual ((Byte) 0x2F, status, L"sense low, enabled");
    }


    TEST_METHOD (Phases_ReachTheDrive)
    {
        Iwm           iwm;
        FakeDriveBus  drive;



        iwm.AttachDrive (0, &drive);

        iwm.Access (1, false, 0, 0);
        iwm.Access (7, false, 0, 0);

        Assert::IsTrue  (drive.phases[0]);
        Assert::IsFalse (drive.phases[1]);
        Assert::IsFalse (drive.phases[2]);
        Assert::IsTrue  (drive.phases[3]);
    }


    //  With the drive enabled, cells move at the drive's cell time; a byte is
    //  complete when its top bit is in bit 7.
    TEST_METHOD (Read_AssemblesBytesFromCells)
    {
        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          data = 0;



        AppendByte (drive.loop, 0xD5);
        AppendByte (drive.loop, 0xAA);
        iwm.AttachDrive (0, &drive);
        iwm.Access (kMotorOn, false, 0, 0);

        data = iwm.Access (kQ6Off, false, 0, 8 * FakeDriveBus::kCellNs);
        Assert::AreEqual ((Byte) 0xD5, data);

        data = iwm.Access (kQ6Off, false, 0, 16 * FakeDriveBus::kCellNs);
        Assert::AreEqual ((Byte) 0xAA, data);
        Assert::AreEqual (16 * (uint64_t) FakeDriveBus::kCellNs, drive.elapsed);
    }


    //  In sync mode a finished byte reads with its top bit set only until the
    //  next byte's second bit and the delay after it; from the third bit on,
    //  the bits shifted in so far read, top bit clear. A Disk II read loop
    //  depends on that to read each byte once.
    TEST_METHOD (Read_SyncModeShowsTheNextByteAsItArrives)
    {
        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          data = 0;



        AppendByte (drive.loop, 0xD5);
        AppendByte (drive.loop, 0xAA);
        iwm.AttachDrive (0, &drive);
        iwm.Access (kMotorOn, false, 0, 0);

        data = iwm.Access (kQ6Off, false, 0, 9 * FakeDriveBus::kCellNs);
        Assert::AreEqual ((Byte) 0xD5, data, L"one bit of the next byte in");

        data = iwm.Access (kQ6Off, false, 0, 11 * FakeDriveBus::kCellNs);
        Assert::AreEqual ((Byte) 0x05, data, L"three bits in: 101");
    }


    //  With the motor switch turned off, a write stops for the second's
    //  delay: the disk turns, and nothing is written to it.
    TEST_METHOD (Write_StopsWithTheMotorSwitchOff)
    {
        Iwm           iwm;
        FakeDriveBus  drive;



        iwm.AttachDrive (0, &drive);
        iwm.Access (kMotorOn, false, 0, 0);
        iwm.Access (kMotorOff, false, 0, 0);
        iwm.Access (kQ6On, false, 0, 0);
        iwm.Access (kQ7On, true, 0x0F, 0);
        iwm.AdvanceTo (16 * FakeDriveBus::kCellNs);

        Assert::IsTrue (drive.written.empty(), L"nothing written in the delay");
        Assert::AreEqual ((Byte) 0x0F, iwm.GetMode(), L"the byte went to the mode register");
        Assert::AreEqual ((size_t) 16, drive.at, L"the disk turned");
    }


    //  The status register's enable bit follows the motor switch, so it
    //  drops at once when the motor is turned off and the drive runs on.
    TEST_METHOD (Status_EnableBitFollowsTheMotorSwitch)
    {
        Iwm           iwm;
        FakeDriveBus  drive;



        iwm.AttachDrive (0, &drive);
        iwm.Access (kMotorOn, false, 0, 0);
        iwm.Access (kMotorOff, false, 0, 0);
        iwm.Access (kQ6On, false, 0, 0);

        Assert::AreEqual ((Byte) 0x00, (Byte) (iwm.Access (kQ7Off, false, 0, 0) & 0x20));
        Assert::IsTrue (iwm.IsEnabled(), L"the drive runs through the delay");
    }


    //  A drive is enabled while the IWM's enable is on and the drive is
    //  selected; the other drive isn't.
    TEST_METHOD (Enable_ReachesTheSelectedDrive)
    {
        constexpr int  kDrive1 = 0xA;
        constexpr int  kDrive2 = 0xB;

        Iwm           iwm;
        FakeDriveBus  one;
        FakeDriveBus  two;



        iwm.AttachDrive (0, &one);
        iwm.AttachDrive (1, &two);
        WriteMode (iwm, 0x07);

        iwm.Access (kMotorOn, false, 0, 0);
        Assert::IsTrue (one.enabled && !two.enabled, L"drive 1 is selected");

        iwm.Access (kDrive2, false, 0, 0);
        Assert::IsTrue (!one.enabled && two.enabled, L"drive 2 is selected");

        iwm.Access (kMotorOff, false, 0, 0);
        Assert::IsTrue (!one.enabled && !two.enabled, L"with the mode's timer off, the enable drops at once");

        iwm.Access (kDrive1, false, 0, 0);
        Assert::IsFalse (one.enabled, L"selecting a drive doesn't enable it");
    }


    //  While the drive's cells read 0 and the shift register is empty, the
    //  IWM passes over them without reading them, up to the drive's quiet
    //  time, and then reads cells again.
    TEST_METHOD (Read_PassesOverQuietCells)
    {
        constexpr uint64_t  kQuietCells = 500;

        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          data = 0;



        drive.quiet = kQuietCells * FakeDriveBus::kCellNs;
        AppendByte (drive.loop, 0xD5);
        iwm.AttachDrive (0, &drive);
        iwm.Access (kMotorOn, false, 0, 0);

        iwm.AdvanceTo (kQuietCells * FakeDriveBus::kCellNs);
        Assert::AreEqual ((size_t) 0, drive.at, L"no quiet cell is read");

        drive.quiet = 0;
        data        = iwm.Access (kQ6Off, false, 0, (kQuietCells + 8) * FakeDriveBus::kCellNs);

        Assert::AreEqual ((size_t) 8, drive.at, L"the cells after the quiet time are read");
        Assert::AreEqual ((Byte) 0xD5, data);
    }


    //  A shift register with bits in it reads every cell, quiet or not, until
    //  the byte is complete.
    TEST_METHOD (Read_ReadsQuietCellsWhileAByteIsInTheShiftRegister)
    {
        Iwm           iwm;
        FakeDriveBus  drive;



        drive.loop = { 1 };
        iwm.AttachDrive (0, &drive);
        iwm.Access (kMotorOn, false, 0, 0);
        iwm.AdvanceTo (FakeDriveBus::kCellNs);

        drive.loop  = { 0 };
        drive.quiet = IDiskDriveBus::kQuietForever;
        iwm.AdvanceTo (20 * FakeDriveBus::kCellNs);

        Assert::AreEqual ((size_t) 8, drive.at, L"the shift register's bit is in bit 7 after seven more cells");
    }


    //  In async mode a byte read from the data register clears it, so the
    //  next read waits for a new byte.
    TEST_METHOD (Read_AsyncModeClearsAfterRead)
    {
        Iwm           iwm;
        FakeDriveBus  drive;



        AppendByte (drive.loop, 0x96);
        AppendByte (drive.loop, 0x00);
        iwm.AttachDrive (0, &drive);
        WriteMode (iwm, Iwm::kModeAsync);
        iwm.Access (kMotorOn, false, 0, 0);

        Assert::AreEqual ((Byte) 0x96, iwm.Access (kQ6Off, false, 0, 8 * FakeDriveBus::kCellNs));
        Assert::AreEqual ((Byte) 0x00, iwm.Access (kQ6Off, false, 0, 9 * FakeDriveBus::kCellNs));
    }


    TEST_METHOD (Write_ShiftsBufferedBytesOut)
    {
        Iwm                iwm;
        FakeDriveBus       drive;
        std::vector<Byte>  stream;



        iwm.AttachDrive (0, &drive);
        iwm.Access (kMotorOn, false, 0, 0);
        iwm.Access (kQ6On, false, 0, 0);
        iwm.Access (kQ7On, true, 0xD5, 0);

        Assert::IsTrue ((iwm.Access (kQ6Off, false, 0, 0) & 0x80) == 0, L"the buffer is full");

        iwm.Access (kQ6On, false, 0, 0);
        iwm.Access (kQ7On, true, 0xAA, FakeDriveBus::kCellNs);
        iwm.Access (kQ7Off, false, 0, 17 * FakeDriveBus::kCellNs);

        AppendByte (stream, 0xD5);
        AppendByte (stream, 0xAA);
        drive.written.resize (std::min (drive.written.size(), stream.size()));

        Assert::IsTrue (drive.written == stream);
    }


    //  After the enable is turned off, the drive runs for a second unless
    //  the mode's timer bit is set.
    TEST_METHOD (MotorOff_WaitsASecond)
    {
        constexpr uint64_t  kSecondNs = 1'000'000'000;

        Iwm  iwm;



        iwm.Access (kMotorOn, false, 0, 0);
        iwm.Access (kMotorOff, false, 0, 0);
        Assert::IsTrue (iwm.IsEnabled());

        iwm.AdvanceTo (kSecondNs + 1);
        Assert::IsFalse (iwm.IsEnabled());

        WriteMode (iwm, Iwm::kModeNoTimer);
        iwm.Access (kMotorOn, false, 0, kSecondNs + 2);
        iwm.Access (kMotorOff, false, 0, kSecondNs + 3);
        Assert::IsFalse (iwm.IsEnabled());
    }


    //  The 3.5 controller firmware's SWIM check: mode bit 6 written 1, 0, 1,
    //  1 enters the ISM mode; the phase and mode registers read their reset
    //  values; the parameter RAM has sixteen bytes; clearing bit 6 returns
    //  to the IWM mode.
    TEST_METHOD (Swim_IsmModeSwitchAndParameterRam)
    {
        constexpr int  kParamWrite = 0x3;
        constexpr int  kZeros      = 0x6;
        constexpr int  kParamRead  = 0xB;
        constexpr int  kPhaseRead  = 0xC;
        constexpr int  kModeRead   = 0xE;

        Iwm  iwm;



        iwm.Access (kQ6On, false, 0, 0);
        iwm.Access (kQ7On, true, 0x4F, 0);
        iwm.Access (kQ7On, true, 0x0F, 0);
        iwm.Access (kQ7On, true, 0x4F, 0);
        Assert::IsFalse (iwm.IsIsmMode());

        iwm.Access (kQ7On, true, 0x4F, 0);
        Assert::IsTrue (iwm.IsIsmMode());

        Assert::AreEqual ((Byte) 0xF0, iwm.Access (kPhaseRead, false, 0, 0));
        Assert::AreEqual ((Byte) 0x40, (Byte) (iwm.Access (kModeRead, false, 0, 0) & 0xC0));

        iwm.Access (kZeros, true, 0, 0);

        for (int i = 0; i < 16; i++)
        {
            iwm.Access (kParamWrite, true, (Byte) (i * 17), 0);
        }

        iwm.Access (kZeros, true, 0, 0);

        for (int i = 0; i < 16; i++)
        {
            Assert::AreEqual ((Byte) (i * 17), iwm.Access (kParamRead, false, 0, 0));
        }

        iwm.Access (kZeros, true, 0xF8, 0);
        Assert::IsFalse (iwm.IsIsmMode());
    }

    //  A peek at the data register returns the byte a read returns, and
    //  does not clear it: a read after any number of peeks gets it, and
    //  clears it in the async mode.
    TEST_METHOD (Peek_DataByteStaysFresh)
    {
        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          peeked = 0;



        AppendByte (drive.loop, 0xD5);
        iwm.AttachDrive (0, &drive);
        WriteMode (iwm, Iwm::kModeAsync);
        iwm.Access (kMotorOn, false, 0, 0);
        iwm.AdvanceTo (8 * FakeDriveBus::kCellNs);

        Assert::IsTrue   (iwm.TryPeekAccess (kQ6Off, peeked));
        Assert::AreEqual ((Byte) 0xD5, peeked);
        Assert::IsTrue   (iwm.TryPeekAccess (kQ6Off, peeked));
        Assert::AreEqual ((Byte) 0xD5, peeked, L"a second peek has the byte too");

        Assert::AreEqual ((Byte) 0xD5, iwm.Access (kQ6Off, false, 0, 8 * FakeDriveBus::kCellNs));
        Assert::IsTrue   (iwm.TryPeekAccess (kQ6Off, peeked));
        Assert::AreEqual ((Byte) 0, peeked, L"the read cleared the byte");
    }


    //  Every switch's peek is the byte the read just after it returns, and
    //  none of them moves a switch.
    TEST_METHOD (Peek_EachSwitchIsTheByteItsReadReturns)
    {
        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          peeked = 0;
        Byte          read   = 0;



        iwm.AttachDrive (0, &drive);
        WriteMode (iwm, 0x0F);

        for (int offset = 0; offset < 16; offset++)
        {
            Assert::IsTrue (iwm.TryPeekAccess (offset, peeked), std::format (L"offset {}", offset).c_str());

            read = iwm.Access (offset, false, 0, 0);

            Assert::AreEqual (read, peeked, std::format (L"offset {}", offset).c_str());
        }
    }


    //  A status peek at the motor switch has the enable bit the switch sets,
    //  and the enable is not set.
    TEST_METHOD (Peek_StatusShowsTheSwitchWithoutThrowingIt)
    {
        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          peeked = 0;



        iwm.AttachDrive (0, &drive);
        WriteMode (iwm, 0x0F);
        iwm.Access (kQ6On, false, 0, 0);

        Assert::IsTrue   (iwm.TryPeekAccess (kMotorOn, peeked));
        Assert::AreEqual ((Byte) 0xAF, peeked, L"sense high, enabled, mode $0F");
        Assert::IsFalse  (iwm.IsEnabled(),     L"the peek did not enable the IWM");
        Assert::IsFalse  (drive.enabled,       L"the drive was not enabled");

        Assert::IsTrue   (iwm.TryPeekAccess (kQ6Off, peeked));
        Assert::IsTrue   (iwm.IsQ6(),          L"the peek did not clear Q6");
    }


    //  A status read at a phase switch returns the sense line of the phases
    //  the switch sets, which a peek cannot work out without setting them.
    TEST_METHOD (Peek_StatusAtAPhaseSwitchHasNoByte)
    {
        Iwm           iwm;
        FakeDriveBus  drive;
        Byte          peeked = 0;



        iwm.AttachDrive (0, &drive);
        iwm.Access (kQ6On, false, 0, 0);

        Assert::IsFalse (iwm.TryPeekAccess (0x1, peeked), L"phase 0 on");
        Assert::IsFalse (drive.phases[0],                 L"the phase was not set");
        Assert::IsTrue  (iwm.TryPeekAccess (0x0, peeked), L"phase 0 is off already");
    }


    //  In the ISM mode a peek at the parameter RAM does not advance the
    //  counter that a read advances.
    TEST_METHOD (Peek_ParameterRamCounterStaysPut)
    {
        constexpr int  kParamWrite = 0x3;
        constexpr int  kZeros      = 0x6;
        constexpr int  kParamRead  = 0xB;

        Iwm   iwm;
        Byte  peeked = 0;



        iwm.Access (kQ6On, false, 0, 0);
        iwm.Access (kQ7On, true, 0x4F, 0);
        iwm.Access (kQ7On, true, 0x0F, 0);
        iwm.Access (kQ7On, true, 0x4F, 0);
        iwm.Access (kQ7On, true, 0x4F, 0);
        Assert::IsTrue (iwm.IsIsmMode());

        iwm.Access (kZeros, true, 0, 0);

        for (int i = 0; i < 16; i++)
        {
            iwm.Access (kParamWrite, true, (Byte) (i * 17), 0);
        }

        iwm.Access (kZeros, true, 0, 0);

        Assert::IsTrue   (iwm.TryPeekAccess (kParamRead, peeked));
        Assert::AreEqual ((Byte) 0, peeked);
        Assert::IsTrue   (iwm.TryPeekAccess (kParamRead, peeked));
        Assert::AreEqual ((Byte) 0, peeked, L"the counter did not move");

        Assert::AreEqual ((Byte) 0, iwm.Access (kParamRead, false, 0, 0));
        Assert::IsTrue   (iwm.TryPeekAccess (kParamRead, peeked));
        Assert::AreEqual ((Byte) 17, peeked, L"the read moved it");
    }


    //  An idle IWM's last time is saved against its owner's time, so it
    //  saves the same bytes after that time moves on, and the same bytes
    //  again after a load at another time.
    TEST_METHOD (SaveState_IsTheSameWhileIdle)
    {
        constexpr uint64_t  kFirstNs  = 1'000;
        constexpr uint64_t  kSecondNs = 250'000;
        constexpr uint64_t  kLoadNs   = 9'000'000;

        Iwm                iwm;
        Iwm                loaded;
        std::vector<Byte>  first;
        std::vector<Byte>  second;



        iwm.AdvanceTo (kFirstNs);
        first = SaveAt (iwm, kFirstNs);

        iwm.AdvanceTo (kSecondNs);
        second = SaveAt (iwm, kSecondNs);

        Assert::IsTrue (first == second, L"the same bytes later");

        AssertSucceeded (LoadAt (loaded, second, kLoadNs));
        Assert::IsTrue  (SaveAt (loaded, kLoadNs) == first, L"the same bytes after a load");
    }


    static std::vector<Byte> SaveAt (const Iwm & iwm, uint64_t nowNs)
    {
        StateWriter  writer;



        iwm.SaveState (writer, nowNs);
        return writer.GetBytes();
    }


    static HRESULT LoadAt (Iwm & iwm, const std::vector<Byte> & bytes, uint64_t nowNs)
    {
        StateReader  reader (bytes);



        return iwm.LoadState (reader, nowNs);
    }

};
