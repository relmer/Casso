#include "Pch.h"

#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MameComparisonTests
//
//  Casso's half of the comparisons with MAME 0.289 in
//  specs/048-smartport-drives/validation.md: the same image files, made by
//  `CassoCli disk create --bootable`, booted on the same cards, and the same
//  BASIC loop timed on a //c and a //c Plus. The images are not in the tree,
//  so the tests run only with CASSO_MAME_COMPARE set to their folder; each
//  logs the screen at the end of its boot and its times.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MameComparisonTests)
{
public:

    static constexpr uint64_t      kBootCap  = 80'000'000ULL;
    static constexpr uint64_t      kSlice    = 100'000ULL;
    static constexpr Word          kFlag     = 768;
    static constexpr const char *  kLoop     = "POKE 768,2:FOR I=1 TO 5000:NEXT:POKE 768,1";


    static bool GetFolder (std::filesystem::path & outFolder)
    {
        wchar_t  buffer[MAX_PATH] = {};
        bool     isSet            = GetEnvironmentVariableW (L"CASSO_MAME_COMPARE", buffer, MAX_PATH) > 0;



        if (!isSet)
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_MAME_COMPARE to the folder of comparison images");
        }

        outFolder = buffer;

        return isSet;
    }


    static std::string Load (FakeDiskFileIo & io, const std::filesystem::path & folder, const std::string & name)
    {
        std::ifstream      in (folder / name, std::ios::binary);
        std::vector<Byte>  bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
        std::string        path = "C:\\cmp\\" + name;



        Assert::IsFalse (bytes.empty(), TextEncoding::NarrowToWide (name).c_str());

        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 1 };

        return path;
    }


    //  Runs until `needle` is on the screen; returns the emulated seconds, or
    //  a negative number past the cap.
    static double RunUntilScreenShows (TestMachine & machine, const std::string & needle)
    {
        std::vector<std::string>  rows;
        uint64_t                  spent = 0;
        bool                      found = false;



        while (!found && spent < kBootCap)
        {
            spent += machine.RunCycles (kSlice);
            rows.clear();
            GuestSession::CollectRows (machine, rows);
            found = GuestSession::AnyRowContains (rows, needle);
        }

        return found ? (double) spent / machine.GetConfig().clockSpeed : -1.0;
    }


    static void BootAndLog (TestMachine & machine, const std::string & label)
    {
        double  seconds = 0.0;



        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        seconds = RunUntilScreenShows (machine, "PRODOS BASIC");

        Logger::WriteMessage (std::format ("{}: {}\n", label, seconds < 0 ? std::string ("no ProDOS BASIC prompt") : std::format ("ProDOS BASIC prompt at {:.2f} s", seconds)).c_str());
        Assert::IsTrue (seconds >= 0, TextEncoding::NarrowToWide (label).c_str());
    }


    //  The loop's emulated seconds, from its first POKE to its last.
    static double TimeTheLoop (TestMachine & machine)
    {
        uint64_t  started = 0;
        uint64_t  spent   = 0;



        machine.GetMemoryBus().WriteByte (kFlag, 0);
        // The line, then Return without InjectLine's run afterward, which
        // would spend the loop's first half-second before the timing starts.
        KeystrokeInjector::InjectString (machine, kLoop);
        AssertSucceeded (KeystrokeInjector::InjectKey (machine, KeystrokeInjector::kAppleReturn));

        while (machine.GetMemoryBus().ReadByte (kFlag) != 1 && spent < kBootCap)
        {
            uint64_t  ran = machine.StepOne();



            spent += ran;

            if (started == 0 && machine.GetMemoryBus().ReadByte (kFlag) == 2)
            {
                started = spent;
            }
        }

        Assert::IsTrue (started != 0 && spent < kBootCap, L"the loop runs");

        return (double) (spent - started) / machine.GetConfig().clockSpeed;
    }


    TEST_METHOD (ScsiCard_BootsEachImage)
    {
        std::filesystem::path  folder;



        if (!GetFolder (folder))
        {
            return;
        }

        for (const char * name : { "800k.hdv", "5m.hdv", "32m.hdv", "32m.2mg" })
        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports (AppleScsiCard::kTargets);



            ports[6].device = kpszHd20scDevice;
            ports[6].image  = Load (io, folder, name);

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 7, kpszAppleScsiCardDevice, ports } }, io);

            Assert::IsTrue (machine.GetBlockMediaProblems().empty());
            BootAndLog (machine, std::string ("SCSI card, ") + name);
        }
    }


    TEST_METHOD (Apple35Card_BootsEachImage)
    {
        std::filesystem::path  folder;



        if (!GetFolder (folder))
        {
            return;
        }

        for (const char * name : { "800k.po", "800k.dc42" })
        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports (2);



            ports[0].device = kpszApple35DriveDevice;
            ports[0].image  = Load (io, folder, name);

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

            Assert::IsTrue (machine.GetBlockMediaProblems().empty());
            BootAndLog (machine, std::string ("3.5 controller card, ") + name);
        }
    }


    TEST_METHOD (Apple2cPlus_BootsAndRunsTheLoopFourTimesAsFast)
    {
        std::filesystem::path  folder;
        FakeDiskFileIo         plusIo;
        FakeDiskFileIo         cIo;
        std::vector<PortConfig> internal (1);
        std::vector<Byte>      floppy;
        double                 cSeconds    = 0.0;
        double                 plusSeconds = 0.0;



        if (!GetFolder (folder))
        {
            return;
        }

        internal[0].name   = "internal";
        internal[0].device = kpszApple35DriveDevice;
        internal[0].image  = Load (plusIo, folder, "800k.po");

        TestMachine  plus ("Apple2cPlus", internal, plusIo);

        BootAndLog (plus, "//c Plus internal drive, 800k.po");
        plusSeconds = TimeTheLoop (plus);

        floppy = cIo.files[Load (cIo, folder, "140k.po")];

        TestMachine  c ("Apple2c");

        AssertSucceeded (c.GetDiskStore().MountFromBytes (6, 0, "140k.po", DiskFormat::Po, floppy));
        BootAndLog (c, "//c, 140k.po");
        cSeconds = TimeTheLoop (c);

        Logger::WriteMessage (std::format ("loop: //c {:.3f} s, //c Plus {:.3f} s, ratio {:.2f}\n", cSeconds, plusSeconds, cSeconds / plusSeconds).c_str());
        Assert::IsTrue (cSeconds / plusSeconds > 3.8 && cSeconds / plusSeconds < 4.0, L"the //c Plus runs the loop 3.92 times as fast, as on MAME");
    }
};
