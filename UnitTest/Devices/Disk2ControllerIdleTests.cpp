#include "Pch.h"
#include "Devices/Disk2Controller.h"

// Disk2Controller carries two DiskImage instances; per-test heap
// allocation would otherwise blow the C6262 stack-frame budget.
#pragma warning (disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2ControllerIdleTests
//
//  The idle hook a pick-up reaches an untouched machine through.
//
//  THE SPINDOWN HOOK CANNOT SERVE THIS, and that is the headline scenario
//  rather than an edge. Spindown fires only on a motor-on to motor-off
//  transition with the timer expiring, so a guest sitting at a BASIC prompt --
//  which is exactly where a build loop leaves it -- never reaches it and would
//  never learn that its disk changed. Without these tests the feature fails its
//  own acceptance scenario and looks like a wiring bug.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2ControllerIdleTests)
{
public:

    static constexpr int  kSlot = 6;



    TEST_METHOD (AnIdleMachineThatNeverSpunItsMotorStillGetsATurn)
    {
        Disk2Controller  controller (kSlot);
        int              turns = 0;



        controller.SetIdleCallback ([&turns] () { turns++; });

        //  No motor, no access, no spindown -- a machine sitting at a prompt.
        controller.Tick (Disk2Controller::kIdleCallbackCycles);

        Assert::IsTrue (turns > 0,
                        L"the build loop's whole case is a guest that is not "
                        L"touching the drive");
    }



    TEST_METHOD (TheCallbackIsRateLimitedToOncePerEmulatedFrame)
    {
        Disk2Controller  controller (kSlot);
        int              turns  = 0;
        uint32_t         cycles = 0;



        controller.SetIdleCallback ([&turns] () { turns++; });

        //  Tick is pumped per instruction and "nothing in flight" is true
        //  nearly always, so an ungated callback would be an indirect dispatch
        //  on essentially every instruction the machine executes.
        for (cycles = 0; cycles < Disk2Controller::kIdleCallbackCycles; cycles++)
        {
            controller.Tick (1);
        }

        Assert::AreEqual (1, turns, L"one frame, one turn");

        for (cycles = 0; cycles < Disk2Controller::kIdleCallbackCycles; cycles++)
        {
            controller.Tick (1);
        }

        Assert::AreEqual (2, turns);
    }



    TEST_METHOD (NoCallbackInstalledIsNotAnError)
    {
        Disk2Controller  controller (kSlot);



        //  Headless hosts and most tests install nothing.
        controller.Tick (Disk2Controller::kIdleCallbackCycles * 4);
    }



    TEST_METHOD (TheTurnIsWithheldWhileTheDriveIsComingUpToSpeed)
    {
        Disk2Controller  controller (kSlot);
        int              turns = 0;



        //  Spin-up is the one window where the controller is suppressing real
        //  data from the guest, which makes it the one window where swapping
        //  the bytes underneath would be seen.
        controller.Read (0xC0E9);   //  motor on

        controller.SetIdleCallback ([&turns] () { turns++; });

        //  Spin-up is a few hundred cycles inside a frame-long window, so the
        //  window has to REMEMBER that it was busy: sampling at its end would
        //  find the drive quiet again and hand out a turn anyway.
        for (uint32_t cycle = 0; cycle < Disk2Controller::kIdleCallbackCycles; cycle++)
        {
            controller.Tick (1);
        }

        Assert::AreEqual (0, turns, L"nothing may land mid-operation");

        //  Once it is up to speed, the machine is quiet again even with the
        //  motor running: between accesses is as good a moment as motor-off.
        controller.Tick (Disk2Controller::kIdleCallbackCycles * 4);

        Assert::IsTrue (turns > 0);
    }
};
