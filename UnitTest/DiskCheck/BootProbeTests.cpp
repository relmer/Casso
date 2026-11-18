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
        BootProbe::Trace   data    = booted;
        BootProbe::Trace   running = MakeTrace (120, 1, 11, 22, 50);



        for (BootProbe::Sample & sample : running.samples)
        {
            sample.isMotorOn = true;
        }

        monitor.rows = { "", "*" };
        basic.rows   = { "?SYNTAX ERROR", "]" };
        prodos.rows  = { "*** UNABLE TO LOAD PRODOS ***" };
        data.rows    = { "This is a data disk, not a startup disk.", "", "]" };

        Assert::IsTrue (BootProbe::Judge (booted, limits, reason) == BootVerdict::Booted, L"The screen changed and the drive stopped");

        //  The same boot, ended in a loop of disk nibbles, and in one that
        //  reads the keyboard.
        booted.loopBytes = { 0xD5, 0xAA, 0xD5, 0xAA, 0xFF, 0xFF };
        Assert::IsTrue (BootProbe::Judge (booted, limits, reason) == BootVerdict::DidNotBoot, L"A short loop with no I/O");
        Assert::AreEqual (std::string ("The program did not appear to be running valid code: it stopped in a short loop with no input or output."), reason);

        booted.loopBytes = { 0xAD, 0x00, 0xC0, 0x10, 0xFB };
        Assert::IsTrue (BootProbe::Judge (booted, limits, reason) == BootVerdict::Booted, L"A loop that reads the keyboard");
        booted.loopBytes.clear();

        //  The same boot, ended on a nearly blank screen.
        booted.isBlank = true;
        Assert::IsTrue (BootProbe::Judge (booted, limits, reason) == BootVerdict::Unclear, L"A nearly blank screen at the end");
        booted.isBlank = false;

        Assert::IsTrue (BootProbe::Judge (stuck, limits, reason) == BootVerdict::DidNotBoot, L"The screen never changed");
        Assert::IsTrue (BootProbe::Judge (loading, limits, reason) == BootVerdict::Unclear, L"The head was moving at the limit");
        Assert::IsTrue (BootProbe::Judge (running, limits, reason) == BootVerdict::Booted, L"The motor on and the head at rest at the limit");
        Assert::IsTrue (BootProbe::Judge (idle, limits, reason) == BootVerdict::Unclear, L"The drive never started");
        Assert::IsTrue (BootProbe::Judge (monitor, limits, reason) == BootVerdict::DidNotBoot, L"The monitor's prompt");
        Assert::IsTrue (BootProbe::Judge (basic, limits, reason) == BootVerdict::DidNotBoot, L"A BASIC prompt after an error");
        Assert::IsTrue (BootProbe::Judge (prodos, limits, reason) == BootVerdict::NotBootable, L"ProDOS with no system file");
        Assert::IsTrue (BootProbe::Judge (data, limits, reason) == BootVerdict::NotBootable, L"A later formatter's data disk");
        Assert::IsFalse (reason.empty(), L"Each verdict has a reason");

        Assert::AreEqual ("did-not-boot", BootProbe::GetVerdictName (BootVerdict::DidNotBoot));
    }

    //  A text screen is blank with fewer than four characters and no prompt,
    //  as the Wizardry I boot disk's sector image ends, with an ampersand in
    //  the middle; a bare prompt is a boot to BASIC. A graphics screen is
    //  blank with less than half a percent of its pixels lit.
    TEST_METHOD (IsBlank_IsAFewCharactersWithNoPrompt_OrAFewLitPixels)
    {
        static constexpr uint32_t  kBlack = 0xFF000000;
        static constexpr uint32_t  kWhite = 0xFFFFFFFF;
        std::vector<uint32_t>      dark (1000, kBlack);
        std::vector<uint32_t>      drawn (1000, kBlack);



        std::fill_n (dark.begin(), 4, kWhite);
        std::fill_n (drawn.begin(), 100, kWhite);

        Assert::IsTrue  (BootProbe::IsBlank ({ "", "                     &" }, true, {}), L"An ampersand alone");
        Assert::IsFalse (BootProbe::IsBlank ({ "]", "]" }, true, {}), L"A bare prompt");
        Assert::IsFalse (BootProbe::IsBlank ({ "    DOS VERSION 3.3  SYSTEM MASTER" }, true, {}), L"A line of text");
        Assert::IsTrue  (BootProbe::IsBlank ({}, false, dark), L"Four lit pixels in a thousand");
        Assert::IsFalse (BootProbe::IsBlank ({}, false, drawn), L"A tenth of the pixels lit");
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
