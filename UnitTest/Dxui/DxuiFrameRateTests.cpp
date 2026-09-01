#include "Pch.h"

#include "Core/DxuiFrameRate.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The averaging window. What matters is that a figure only appears once a
// window has closed, that it reflects the window's own elapsed time rather
// than the nominal second, and that a clock which stalls or runs backwards
// cannot corrupt it.
namespace DxuiFrameRateTests
{
    // `count` frames at a steady `delta`, which is how a locked frame rate
    // arrives.
    static void Run (DxuiFrameRate & rate, int count, float delta)
    {
        for (int i = 0; i < count; i++)
        {
            rate.Tick (delta);
        }
    }


    // Ticks at a steady `delta` until the window closes, and answers the
    // rate it reported.
    //
    // NOT a fixed count of frames. Sixty ticks of 1/60 accumulate to
    // 0.99999994 in float, one ulp short of the window, so a test that
    // counted out exactly one second of frames would read zero and blame
    // the counter. Where the boundary falls is not part of the contract;
    // that a window eventually closes is.
    static float RunUntilWindowCloses (DxuiFrameRate & rate, float delta)
    {
        float   before = rate.GetFramesPerSecond();
        int     guard  = 0;

        while (rate.GetFramesPerSecond() == before && guard < 100000)
        {
            rate.Tick (delta);
            guard++;
        }

        Assert::IsTrue (guard < 100000, L"a window should have closed");

        return rate.GetFramesPerSecond();
    }


    TEST_CLASS (DxuiFrameRateTests)
    {
    public:

        TEST_METHOD (ReportsNothingBeforeTheFirstWindowCloses)
        {
            DxuiFrameRate   rate;



            Run (rate, 30, 1.0f / 60.0f);   // half a second

            Assert::AreEqual (0.0f, rate.GetFramesPerSecond(), L"no window yet");
        }


        TEST_METHOD (AveragesSixtyFramesASecond)
        {
            DxuiFrameRate   rate;



            Assert::AreEqual (60.0f, RunUntilWindowCloses (rate, 1.0f / 60.0f), 0.01f);
        }


        TEST_METHOD (AveragesAThirtyFrameSecond)
        {
            DxuiFrameRate   rate;



            Assert::AreEqual (30.0f, RunUntilWindowCloses (rate, 1.0f / 30.0f), 0.01f);
        }


        // The frame that closes the window overshoots it. Dividing by the
        // nominal second rather than the elapsed one would report a rate the
        // machine did not achieve, and the coarser the frame the worse it
        // gets: four frames at 0.3s span 1.2 seconds, which is 3.33 fps.
        TEST_METHOD (DividesByElapsedRatherThanTheNominalWindow)
        {
            DxuiFrameRate   rate;



            Run (rate, 4, 0.3f);

            Assert::AreEqual (4.0f / 1.2f, rate.GetFramesPerSecond(), 0.01f);
        }


        // A clock that did not move cannot be divided by, and on a resumed
        // process it can go backwards.
        TEST_METHOD (IgnoresANonPositiveDelta)
        {
            DxuiFrameRate   rate;



            RunUntilWindowCloses (rate, 1.0f / 60.0f);

            rate.Tick (0.0f);
            rate.Tick (-5.0f);

            Assert::AreEqual (60.0f, rate.GetFramesPerSecond(), 0.01f,
                              L"a bad delta changes nothing");
        }


        // Reset abandons the window in progress but keeps the figure on
        // screen: blanking it because a modal loop ended would show a wrong
        // number rather than a stale one.
        TEST_METHOD (ResetKeepsTheLastAverageAndDropsThePartialWindow)
        {
            DxuiFrameRate   rate;



            Assert::AreEqual (60.0f, RunUntilWindowCloses (rate, 1.0f / 60.0f), 0.01f);

            Run (rate, 30, 1.0f / 60.0f);   // half of the next window
            rate.Reset();

            Assert::AreEqual (60.0f, rate.GetFramesPerSecond(), 0.01f, L"kept");

            // The abandoned half does not count toward the next average.
            Assert::AreEqual (30.0f, RunUntilWindowCloses (rate, 1.0f / 30.0f), 0.01f,
                              L"clean window");
        }


        // A stall inside a window drags the average down, which is the point:
        // the readout is there to show it.
        TEST_METHOD (AStallShowsUpInTheAverage)
        {
            DxuiFrameRate   rate;



            Run (rate, 29, 1.0f / 60.0f);   // 0.4833s
            rate.Tick (0.6f);               // one long frame closes the window

            Assert::IsTrue (rate.GetFramesPerSecond() < 60.0f, L"stall pulls it down");
            Assert::AreEqual (30.0f / 1.0833f, rate.GetFramesPerSecond(), 0.05f);
        }
    };
}
