#include "Pch.h"

#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35DriveTests
//
//  The Sony mechanism's registers and commands, disk sizes, and written
//  tracks decoded back into the image.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Sony35DriveTests)
{
public:

    static constexpr const char *  kPath       = "d.po";
    static constexpr int       kRegDiskIn   = 0x2;
    static constexpr int       kRegMotor    = 0x8;
    static constexpr int       kRegTrack0   = 0xA;
    static constexpr int       kRegReady    = 0xB;
    static constexpr int       kRegProtect  = 0x6;
    static constexpr int       kCmdIn       = 0x0;
    static constexpr int       kCmdOut      = 0x1;
    static constexpr int       kCmdStep     = 0x4;
    static constexpr int       kCmdMotorOn  = 0x8;
    static constexpr int       kCmdMotorOff = 0x9;
    static constexpr int       kCmdEject    = 0xD;
    static constexpr uint64_t  kSecondNs    = 1'000'000'000;


    static void SelectRegister (Sony35Drive & drive, int reg, bool lstrb)
    {
        drive.SetSel ((reg & 2) != 0);
        drive.SetPhases ((reg & 4) != 0, (reg & 8) != 0, (reg & 1) != 0, lstrb);
    }


    static bool Sense (Sony35Drive & drive, int reg)
    {
        SelectRegister (drive, reg, false);

        return drive.GetSense();
    }


    static void Command (Sony35Drive & drive, int command)
    {
        SelectRegister (drive, command, false);
        SelectRegister (drive, command, true);
        SelectRegister (drive, command, false);
    }


    static void Insert (FakeDiskFileIo & io, Sony35Drive & drive, size_t blocks, Byte fill = 0)
    {
        std::vector<Byte>  image (blocks * Gcr35Track::kDataBytes, fill);
        std::wstring       error;



        io.files[kPath]  = image;
        io.stamps[kPath] = FileStamp { image.size(), 1 };

        Assert::AreEqual (S_OK, drive.Insert (io, kPath, {}, error));
    }


    TEST_METHOD (DiskInPlace_FollowsInsertAndEject)
    {
        FakeDiskFileIo  io;
        Sony35Drive     drive (0);



        Assert::IsTrue (Sense (drive, kRegDiskIn), L"no disk reads 1");

        Insert (io, drive, Gcr35Track::kBlocksTwoSides);
        Assert::IsFalse (Sense (drive, kRegDiskIn));
        Assert::IsTrue  (Sense (drive, kRegProtect), L"a writable disk reads 1");

        Command (drive, kCmdEject);
        Assert::IsFalse (Sense (drive, kRegDiskIn), L"the eject takes time");

        drive.Advance (kSecondNs);
        Assert::IsTrue (Sense (drive, kRegDiskIn));
        Assert::IsFalse (drive.HasDisk());
    }


    TEST_METHOD (Insert_RejectsOtherSizes)
    {
        FakeDiskFileIo     io;
        Sony35Drive        drive (0);
        std::vector<Byte>  image (280 * Gcr35Track::kDataBytes, 0);
        std::wstring       error;



        io.files[kPath]  = image;
        io.stamps[kPath] = FileStamp { image.size(), 1 };

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT), drive.Insert (io, kPath, {}, error));
        Assert::IsFalse (drive.HasDisk());
        Assert::IsFalse (error.empty());
    }


    TEST_METHOD (Motor_ReadyAfterSpinUp)
    {
        FakeDiskFileIo  io;
        Sony35Drive     drive (0);



        Insert (io, drive, Gcr35Track::kBlocksTwoSides);

        Command (drive, kCmdMotorOn);
        Assert::IsFalse (Sense (drive, kRegMotor), L"motor on reads 0");
        Assert::IsTrue  (Sense (drive, kRegReady), L"not ready while it spins up");

        drive.Advance (kSecondNs);
        Assert::IsFalse (Sense (drive, kRegReady));

        Command (drive, kCmdMotorOff);
        Assert::IsTrue (Sense (drive, kRegMotor));
    }


    TEST_METHOD (Step_MovesInAndOut)
    {
        Sony35Drive  drive (0);



        Assert::IsFalse (Sense (drive, kRegTrack0), L"track 0 reads 0");

        Command (drive, kCmdIn);
        Command (drive, kCmdStep);
        Command (drive, kCmdStep);
        Assert::AreEqual (2, drive.GetTrack());
        Assert::IsTrue (Sense (drive, kRegTrack0));

        Command (drive, kCmdOut);
        Command (drive, kCmdStep);
        Command (drive, kCmdStep);
        Command (drive, kCmdStep);
        Assert::AreEqual (0, drive.GetTrack(), L"the head stops at track 0");
    }


    //  The bits read off track 0 are the GCR of the image's first twelve
    //  blocks.
    TEST_METHOD (ReadCell_StreamsTheImageTrack)
    {
        FakeDiskFileIo     io;
        Sony35Drive        drive (0);
        std::vector<Byte>  bits;
        std::vector<Byte>  decoded;
        std::vector<bool>  found;



        Insert (io, drive, Gcr35Track::kBlocksTwoSides, 0xA5);
        Command (drive, kCmdMotorOn);

        bits.resize (Gcr35Track::GetTrackBits (0));

        for (Byte & bit : bits)
        {
            bit = drive.ReadCell();
        }

        Gcr35Track::DecodeTrack (0, 0, bits, decoded, found);

        Assert::AreEqual ((size_t) 12, (size_t) std::count (found.begin(), found.end(), true));
        Assert::IsTrue (std::all_of (decoded.begin(), decoded.end(), [] (Byte b) { return b == 0xA5; }));
    }


    //  A track written through WriteCell is in the image once the motor turns
    //  off.
    TEST_METHOD (WriteCell_ReachesImageOnMotorOff)
    {
        FakeDiskFileIo     io;
        Sony35Drive        drive (0);
        std::vector<Byte>  data (12 * Gcr35Track::kDataBytes, 0x3C);
        std::vector<Byte>  bits;



        Insert (io, drive, Gcr35Track::kBlocksTwoSides);
        Command (drive, kCmdMotorOn);

        Gcr35Track::BuildTrack (0, 0, Gcr35Track::kFormatTwoSides, data, bits);

        for (Byte bit : bits)
        {
            drive.WriteCell (bit);
        }

        Assert::AreEqual ((Byte) 0, io.files[kPath][0], L"the write is cached until the motor stops");

        Command (drive, kCmdMotorOff);

        Assert::AreEqual ((Byte) 0x3C, io.files[kPath][0]);
        Assert::AreEqual ((Byte) 0x3C, io.files[kPath][12 * Gcr35Track::kDataBytes - 1]);
        Assert::AreEqual ((Byte) 0,    io.files[kPath][12 * Gcr35Track::kDataBytes], L"side 1 is untouched");
    }
};
