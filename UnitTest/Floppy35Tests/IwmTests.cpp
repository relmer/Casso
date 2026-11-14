#include "Pch.h"

#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/Iwm.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDriveBus
//
//  A drive whose disk is a loop of bits, recording what is written and the
//  phases it was given.
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
    uint32_t  GetCellNs() const override { return kCellNs; }

    static constexpr uint32_t  kCellNs = 2000;

    std::vector<Byte>    loop;
    std::vector<Byte>    written;
    std::array<bool, 4>  phases  = {};
    size_t               at      = 0;
    uint64_t             elapsed = 0;
    bool                 sense   = true;
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
};
