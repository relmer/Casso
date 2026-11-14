#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/Woz35Builder.h"

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


    //  The SuperDrive's lines are low on an 800K drive: not a SuperDrive, not
    //  in MFM mode, no index pulse, no 1.44 MB disk. The disk switched line
    //  is set by an insert and cleared by its command.
    TEST_METHOD (SuperDriveLines_ReadLowOnAnEightHundredKDrive)
    {
        constexpr int  kRegIndex0     = 0x1;
        constexpr int  kRegIndex1     = 0x3;
        constexpr int  kRegSuperDrive = 0x5;
        constexpr int  kRegMfmMode    = 0x7;
        constexpr int  kRegSwitched   = 0xC;
        constexpr int  kRegHighDens   = 0xF;
        constexpr int  kCmdClear      = 0x3;

        FakeDiskFileIo  io;
        Sony35Drive     drive (0);



        Insert (io, drive, Gcr35Track::kBlocksTwoSides);

        for (int reg : { kRegIndex0, kRegIndex1, kRegSuperDrive, kRegMfmMode, kRegHighDens })
        {
            Assert::IsFalse (Sense (drive, reg), std::format (L"register {:X}", reg).c_str());
        }

        Assert::IsTrue (Sense (drive, kRegSwitched), L"set by the insert");
        Command (drive, kCmdClear);
        Assert::IsFalse (Sense (drive, kRegSwitched), L"cleared by the command");
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
            bit = drive.ReadCell (0);
        }

        Gcr35Track::DecodeTrack (0, 0, bits, decoded, found);

        Assert::AreEqual ((size_t) 12, (size_t) std::count (found.begin(), found.end(), true));
        Assert::IsTrue (std::all_of (decoded.begin(), decoded.end(), [] (Byte b) { return b == 0xA5; }));
    }


    static void InsertWoz (FakeDiskFileIo & io, Sony35Drive & drive, const std::vector<Byte> & woz)
    {
        std::wstring  error;



        io.files[kPath]  = woz;
        io.stamps[kPath] = FileStamp { woz.size(), 1 };

        Assert::AreEqual (S_OK, drive.Insert (io, kPath, {}, error));
    }


    static void ReadRevolution (Sony35Drive & drive, std::vector<Byte> & outBits)
    {
        outBits.resize (Gcr35Track::GetTrackBits (0));

        for (Byte & bit : outBits)
        {
            bit = drive.ReadCell (0);
        }
    }


    //  A 3.5-INCH WOZ IMAGE'S TRACKS are its bit streams, TMAP entry track *
    //  2 + side.
    TEST_METHOD (WozImage_ReadsItsTracks)
    {
        FakeDiskFileIo     io;
        Sony35Drive        drive (0);
        std::vector<Byte>  bits;
        std::vector<Byte>  decoded;
        std::vector<bool>  found;



        InsertWoz (io, drive, Woz35Builder::Build (std::vector<Byte> (Gcr35Track::kBlocksTwoSides * Gcr35Track::kDataBytes, 0x6B)));
        Assert::IsTrue (drive.IsWoz());

        Command (drive, kCmdMotorOn);
        drive.SetSel (true);
        ReadRevolution (drive, bits);

        Gcr35Track::DecodeTrack (0, 1, bits, decoded, found);

        Assert::AreEqual ((size_t) 12, (size_t) std::count (found.begin(), found.end(), true));
        Assert::IsTrue (std::all_of (decoded.begin(), decoded.end(), [] (Byte b) { return b == 0x6B; }));
    }


    //  A track written to a WOZ image is in the WOZ file once the motor
    //  stops, and the file is a 3.5-inch WOZ.
    TEST_METHOD (WozImage_WriteGoesBackToTheFile)
    {
        FakeDiskFileIo          io;
        Sony35Drive             drive (0);
        std::vector<Byte>       data (12 * Gcr35Track::kDataBytes, 0x2E);
        std::vector<Byte>       bits;
        std::vector<Byte>       decoded;
        std::vector<bool>       found;
        WozLoader::Description  about;
        DiskImage               reloaded;
        int                     slot = 0;



        InsertWoz (io, drive, Woz35Builder::Build (std::vector<Byte> (Gcr35Track::kBlocksTwoSides * Gcr35Track::kDataBytes, 0x00)));
        Command (drive, kCmdMotorOn);

        Gcr35Track::BuildTrack (0, 0, Gcr35Track::kFormatTwoSides, data, bits);

        for (Byte bit : bits)
        {
            drive.WriteCell (bit, 0);
        }

        Command (drive, kCmdMotorOff);

        WozLoader::Describe (io.files[kPath], about);
        Assert::AreEqual (WozLoader::kDiskType35, about.diskType);

        reloaded.LoadFromBytes (DiskFormat::Woz, io.files[kPath], "");
        slot = reloaded.GetMappedSlot (0);
        Assert::IsTrue (slot >= 0);

        bits.assign (reloaded.GetTrackBitCount (slot), 0);

        for (size_t i = 0; i < bits.size(); i++)
        {
            bits[i] = reloaded.ReadBit (slot, i);
        }

        Gcr35Track::DecodeTrack (0, 0, bits, decoded, found);
        Assert::AreEqual ((size_t) 12, (size_t) std::count (found.begin(), found.end(), true));
        Assert::AreEqual ((Byte) 0x2E, decoded[0]);
    }


    TEST_METHOD (WozImage_RejectsA525Disk)
    {
        FakeDiskFileIo     io;
        Sony35Drive        drive (0);
        std::vector<Byte>  woz = Woz35Builder::Build (std::vector<Byte> (Gcr35Track::kBlocksTwoSides * Gcr35Track::kDataBytes, 0), {}, WozLoader::kDiskType525);
        std::wstring       error;



        io.files[kPath]  = woz;
        io.stamps[kPath] = FileStamp { woz.size(), 1 };

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT), drive.Insert (io, kPath, {}, error));
        Assert::IsFalse (drive.HasDisk());
    }


    //  A 400K disk has side 0 only: its tracks are marked one-sided, and the
    //  other head reads only zeros.
    TEST_METHOD (OneSidedDisk_HasSideZeroOnly)
    {
        FakeDiskFileIo     io;
        Sony35Drive        drive (0);
        std::vector<Byte>  bits (Gcr35Track::GetTrackBits (0));
        std::vector<Byte>  decoded;
        std::vector<bool>  found;



        Insert (io, drive, Gcr35Track::kBlocksOneSide, 0x77);
        Command (drive, kCmdMotorOn);

        for (Byte & bit : bits)
        {
            bit = drive.ReadCell (0);
        }

        Gcr35Track::DecodeTrack (0, 0, bits, decoded, found);
        Assert::AreEqual ((size_t) 12, (size_t) std::count (found.begin(), found.end(), true));

        drive.SetSel (true);

        for (Byte & bit : bits)
        {
            bit = drive.ReadCell (0);
        }

        Assert::IsTrue (std::all_of (bits.begin(), bits.end(), [] (Byte b) { return b == 0; }));
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
            drive.WriteCell (bit, 0);
        }

        Assert::AreEqual ((Byte) 0, io.files[kPath][0], L"the write is cached until the motor stops");

        Command (drive, kCmdMotorOff);

        Assert::AreEqual ((Byte) 0x3C, io.files[kPath][0]);
        Assert::AreEqual ((Byte) 0x3C, io.files[kPath][12 * Gcr35Track::kDataBytes - 1]);
        Assert::AreEqual ((Byte) 0,    io.files[kPath][12 * Gcr35Track::kDataBytes], L"side 1 is untouched");
    }
};
