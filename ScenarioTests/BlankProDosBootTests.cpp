#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "MachineIdle.h"
#include "TestMachine.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlankProDosBootTests
//
//  An enhanced //e boots a new 140K ProDOS disk that has no PRODOS file. The
//  boot code in block 0 prints a message in the middle of the screen and
//  loops in place.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlankProDosBootTests)
{
public:

    static constexpr const char *  kMessage    = "*** UNABLE TO LOAD PRODOS ***";
    static constexpr Word          kHangLoop   = 0x0822;
    static constexpr Word          kRomStart   = 0xC000;
    static constexpr int           kSlot6      = 6;
    static constexpr int           kDrive1     = 0;
    static constexpr uint64_t      kBootCycles = 40'000'000ULL;


    TEST_METHOD (BlankProDosDisk_PrintsUnableToLoadProDos)
    {
        TestMachine               machine ("Apple2eEnhanced");
        BlankDiskSpec             spec;
        std::vector<Byte>         image;
        std::vector<std::string>  rows;
        uint64_t                  ran = 0;
        Word                      pc  = 0;



        spec.format     = DiskFormat::Po;
        spec.contents   = BlankDiskContents::ProDos;
        spec.volumeName = "BLANK";
        AssertSucceeded (BlankDiskBuilder::Build (spec, {}, image));

        machine.PowerCycle();
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (kSlot6, kDrive1, "blank.po", DiskFormat::Po, image));
        GuestSession::StartFromPowerOn (machine);
        ran = MachineIdle::RunUntilIdle (machine, kBootCycles);
        IGNORE_RETURN_VALUE (ran, 0);

        GuestSession::CollectRows (machine, rows);

        for (const std::string & row : rows)
        {
            Logger::WriteMessage ((row + "\n").c_str());
        }

        pc = machine.GetCpu()->GetPC();

        Assert::IsTrue   (GuestSession::AnyRowContains (rows, kMessage), L"the screen has the message");
        Assert::IsTrue   (pc < kRomStart,                                L"the processor is outside the monitor ROM");
        Assert::AreEqual ((int) kHangLoop, (int) pc,                     L"the processor loops at the end of the boot code");
    }
};
