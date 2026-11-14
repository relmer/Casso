#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/GuestSession.h"
#include "../EmuTests/TestMachine.h"
#include "Core/TextEncoding.h"
#include "DiskCheck/BootProbe.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootProbeTests
//
//  A disk image booted on a machine with no window, and the verdict from
//  its samples.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BootProbeTests)
{
public:

    static BootProbe::Trace  MakeTrace (int slices, int driveStarted, uint64_t firstHash, uint64_t lastHash, int movingUntil)
    {
        BootProbe::Trace  trace;



        for (int slice = 0; slice < slices; slice++)
        {
            BootProbe::Sample  sample;

            sample.frameHash    = slice <= driveStarted ? firstHash : lastHash;
            sample.quarterTrack = slice < movingUntil ? slice : movingUntil;
            sample.isMotorOn    = slice >= driveStarted && slice < movingUntil;

            trace.samples.push_back (sample);
        }

        trace.driveStarted = driveStarted;

        return trace;
    }

    //  The verdict from samples alone: a screen that changed with the drive
    //  stopped is a boot; a screen as it was when the drive started is not;
    //  a head moving at the limit is unclear, and one at rest with the motor
    //  on is a boot; and the text screen's prompts and ProDOS's message are
    //  read before any of those.
    TEST_METHOD (Judge_ReadsTheScreenTheHeadAndThePrompts)
    {
        BootProbe::Limits  limits;
        std::string        reason;
        BootProbe::Trace   booted  = MakeTrace (100, 1, 11, 22, 40);
        BootProbe::Trace   stuck   = MakeTrace (120, 1, 11, 11, 5);
        BootProbe::Trace   loading = MakeTrace (120, 1, 11, 22, 119);
        BootProbe::Trace   idle    = MakeTrace (120, -1, 11, 11, 0);
        BootProbe::Trace   monitor = booted;
        BootProbe::Trace   basic   = booted;
        BootProbe::Trace   prodos  = booted;
        BootProbe::Trace   running = MakeTrace (120, 1, 11, 22, 50);



        for (BootProbe::Sample & sample : running.samples)
        {
            sample.isMotorOn = true;
        }

        monitor.rows = { "", "*" };
        basic.rows   = { "?SYNTAX ERROR", "]" };
        prodos.rows  = { "*** UNABLE TO LOAD PRODOS ***" };

        Assert::IsTrue (BootProbe::Judge (booted, limits, reason) == BootVerdict::Booted, L"The screen changed and the drive stopped");
        Assert::IsTrue (BootProbe::Judge (stuck, limits, reason) == BootVerdict::DidNotBoot, L"The screen never changed");
        Assert::IsTrue (BootProbe::Judge (loading, limits, reason) == BootVerdict::Unclear, L"The head was moving at the limit");
        Assert::IsTrue (BootProbe::Judge (running, limits, reason) == BootVerdict::Booted, L"The motor on and the head at rest at the limit");
        Assert::IsTrue (BootProbe::Judge (idle, limits, reason) == BootVerdict::Unclear, L"The drive never started");
        Assert::IsTrue (BootProbe::Judge (monitor, limits, reason) == BootVerdict::DidNotBoot, L"The monitor's prompt");
        Assert::IsTrue (BootProbe::Judge (basic, limits, reason) == BootVerdict::DidNotBoot, L"A BASIC prompt after an error");
        Assert::IsTrue (BootProbe::Judge (prodos, limits, reason) == BootVerdict::NotBootable, L"ProDOS with no system file");
        Assert::IsFalse (reason.empty(), L"Each verdict has a reason");

        Assert::AreEqual ("did-not-boot", BootProbe::GetVerdictName (BootVerdict::DidNotBoot));
    }

    //  The DOS 3.3 System Master boots on an enhanced //e to a BASIC prompt:
    //  booted, with a frame of the machine's size and the prompt in the rows.
    TEST_METHOD (Probe_OfTheDosMaster_IsBooted_WithItsFrameAndRows)
    {
        TestMachine        machine ("Apple2eEnhanced");
        BootReport         report;
        std::vector<Byte>  master = GuestSession::RequireDos33Master();



        AssertSucceeded (BootProbe::Probe (machine, machine.GetBuilder(), master, DiskFormat::Dsk, BootProbe::Limits(), report));

        Assert::IsTrue (report.verdict == BootVerdict::Booted, TextEncoding::NarrowToWide (report.reason).c_str());
        Assert::AreEqual ((size_t) report.width * (size_t) report.height, report.frame.size());
        Assert::IsTrue (report.width > 0 && report.height > 0);
        Assert::AreEqual ((size_t) BootProbe::kTextRows, report.rows.size());
        Assert::IsTrue (report.isTextMode);
    }

    //  An image of one repeated byte has no boot sector the drive can read:
    //  the screen is as it was when the drive started.
    TEST_METHOD (Probe_OfAnImageOfOneByte_DidNotBoot)
    {
        TestMachine        machine ("Apple2eEnhanced");
        BootReport         report;
        std::vector<Byte>  image (NibblizationLayer::kImageByteSize, 0xE5);
        BootProbe::Limits  limits;



        limits.slices = 30;

        AssertSucceeded (BootProbe::Probe (machine, machine.GetBuilder(), image, DiskFormat::Dsk, limits, report));

        Assert::IsTrue (report.verdict == BootVerdict::DidNotBoot, TextEncoding::NarrowToWide (report.reason).c_str());
    }
};
