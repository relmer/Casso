#include "Pch.h"

#include "Machines/Apple3/Apple3TimeBase.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3TimeBaseTests
//
//  The cost of a CPU cycle in ticks under each term of the 1 MHz rule, the
//  line and frame lengths, device time across a speed switch, and the
//  calibrated averages with the screen off and on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3TimeBaseTests)
{
public:

    //  State 26 is displayed and is not a refresh state; state 0 is a
    //  refresh state.
    static constexpr uint32_t  kDisplayedTick = 26 * Apple3TimeBase::kTicksPerState;
    static constexpr uint32_t  kFrames        = 10;


    static void MoveToTick (Apple3TimeBase & timeBase, uint32_t tick)
    {
        while (timeBase.GetTickInLine() != tick)
        {
            timeBase.ChargeCycle (Apple3Access::Rom);
        }
    }


    static double MeasureMhz (bool screenOn)
    {
        Apple3TimeBase  timeBase;
        uint64_t        cycles  = 0;
        double          seconds = 0;



        timeBase.SetScreenOn (screenOn);

        while (timeBase.GetTicks() < Apple3TimeBase::kTicksPerFrame * kFrames)
        {
            timeBase.ChargeCycle (Apple3Access::Ram);
            cycles++;
        }

        seconds = (double) timeBase.GetMasterClocks() / Apple3TimeBase::kMasterClockHz;

        return (double) cycles / seconds / 1e6;
    }


    TEST_METHOD (RamOutsideTheDisplay_IsOneTick)
    {
        Apple3TimeBase  timeBase;



        MoveToTick (timeBase, kDisplayedTick);

        Assert::IsFalse (timeBase.IsDisplayActive());
        Assert::AreEqual (1u, timeBase.ChargeCycle (Apple3Access::Ram));
    }


    TEST_METHOD (OneMhzMode_IsTwoTicksForEveryAccess)
    {
        Apple3TimeBase  timeBase;



        timeBase.SetOneMhz (true);

        Assert::AreEqual (2u, timeBase.ChargeCycle (Apple3Access::Rom));
        Assert::AreEqual (2u, timeBase.ChargeCycle (Apple3Access::Io));
        Assert::AreEqual (2u, timeBase.ChargeCycle (Apple3Access::Ram));
    }


    TEST_METHOD (Fspace_IsTwoTicks)
    {
        Apple3TimeBase  timeBase;



        MoveToTick (timeBase, kDisplayedTick);

        Assert::AreEqual (2u, timeBase.ChargeCycle (Apple3Access::Fspace));
    }


    TEST_METHOD (RamDuringTheDisplay_IsTwoTicks_RomAndIoAreOne)
    {
        Apple3TimeBase  timeBase;



        timeBase.SetScreenOn (true);
        MoveToTick (timeBase, kDisplayedTick);

        Assert::IsTrue   (timeBase.IsDisplayActive());
        Assert::AreEqual (1u, timeBase.ChargeCycle (Apple3Access::Rom));

        MoveToTick (timeBase, kDisplayedTick);
        Assert::AreEqual (1u, timeBase.ChargeCycle (Apple3Access::Io));

        MoveToTick (timeBase, kDisplayedTick);
        Assert::AreEqual (2u, timeBase.ChargeCycle (Apple3Access::Ram));
    }


    TEST_METHOD (RamDuringRefresh_IsTwoTicks_WithTheScreenOff)
    {
        Apple3TimeBase  timeBase;



        Assert::IsTrue   (Apple3TimeBase::IsRefreshState (0));
        Assert::AreEqual (2u, timeBase.ChargeCycle (Apple3Access::Ram));
    }


    TEST_METHOD (RefreshStates_AreTheCalibratedCountPerLine)
    {
        uint32_t  count = 0;



        for (uint32_t state = 0; state < Apple3TimeBase::kStatesPerLine; state++)
        {
            count += Apple3TimeBase::IsRefreshState (state) ? 1 : 0;
        }

        Assert::AreEqual (Apple3TimeBase::kRefreshStatesPerLine, count);
    }


    TEST_METHOD (Line_Is130Ticks_And912MasterClocks)
    {
        Apple3TimeBase  timeBase;



        timeBase.ChargeCycles (Apple3TimeBase::kTicksPerLine, Apple3Access::Rom);

        Assert::AreEqual (1u,             timeBase.GetLine());
        Assert::AreEqual (0u,             timeBase.GetTickInLine());
        Assert::AreEqual ((uint64_t) 912, timeBase.GetMasterClocks());
    }


    TEST_METHOD (Frame_Is262Lines)
    {
        Apple3TimeBase  timeBase;



        timeBase.ChargeCycles (Apple3TimeBase::kTicksPerLine * (Apple3TimeBase::kLinesPerFrame - 1), Apple3Access::Rom);
        Assert::AreEqual (261u, timeBase.GetLine());

        timeBase.ChargeCycles (Apple3TimeBase::kTicksPerLine, Apple3Access::Rom);
        Assert::AreEqual (0u, timeBase.GetLine());
        Assert::AreEqual ((uint64_t) 912 * 262, timeBase.GetMasterClocks());
    }


    TEST_METHOD (DeviceTime_FollowsTicksAcrossASpeedSwitch)
    {
        Apple3TimeBase  timeBase;



        MoveToTick (timeBase, kDisplayedTick);
        timeBase.ChargeCycles (10, Apple3Access::Rom);

        timeBase.SetOneMhz (true);
        timeBase.ChargeCycles (10, Apple3Access::Rom);

        timeBase.SetOneMhz (false);
        timeBase.ChargeCycles (10, Apple3Access::Rom);

        Assert::AreEqual ((uint64_t) kDisplayedTick + 40, timeBase.GetTicks());
        Assert::AreEqual ((uint64_t) (kDisplayedTick + 40) * 7, timeBase.GetMasterClocks());
    }


    TEST_METHOD (RamLoop_AveragesTheDocumentedRates)
    {
        double  screenOff = MeasureMhz (false);
        double  screenOn  = MeasureMhz (true);



        Logger::WriteMessage (std::format ("RAM loop: {:.3f} MHz with the screen off, {:.3f} MHz with it on\n", screenOff, screenOn).c_str());

        Assert::AreEqual (1.8, screenOff, 0.05);
        Assert::AreEqual (1.4, screenOn,  0.05);
    }
};
