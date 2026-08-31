#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Widgets/DxuiOrbitControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControlTests
//
//  The scene compass's press gesture: an arrow turns on the way DOWN, keeps
//  turning while it is held still, and stops repeating the moment the pointer
//  starts aiming instead -- for the rest of that press, however still the
//  hand goes afterward. The orb alone still commits on release.
//
//  The clock is injected through Tick, so none of this waits on real time.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiOrbitControlTests)
{
public:

    // A compass 100 px square at the origin, which puts its center at (50,50)
    // and each arrow comfortably out along its own axis.
    static void Place (DxuiOrbitControl & compass)
    {
        compass.SetRect (RECT { 0, 0, 100, 100 });
        compass.SetDpi  (96);
    }

    // Where the control itself says a part is -- asked rather than assumed,
    // so the arrow geometry can move without this file following it.
    static POINT PointOn (const DxuiOrbitControl & compass, DxuiOrbitControl::Part want)
    {
        for (int y = 0; y < 100; y++)
        {
            for (int x = 0; x < 100; x++)
            {
                if (compass.HitPart (x, y) == want)
                {
                    return POINT { x, y };
                }
            }
        }

        Assert::Fail (L"no pixel of the compass resolves to the requested part");
        return POINT {};
    }


    TEST_METHOD (An_Arrow_Turns_On_The_Way_Down)
    {
        DxuiOrbitControl  compass;
        int               steps = 0;
        POINT             up    = {};



        Place (compass);
        compass.SetOnStep ([&steps] (DxuiOrbitControl::Part) { steps++; });

        up = PointOn (compass, DxuiOrbitControl::Part::Up);

        Assert::IsTrue (compass.OnPointerDown (up.x, up.y));
        Assert::AreEqual (1, steps);

        // ...and the release does NOT fire a second one.
        Assert::IsTrue (compass.OnPointerUp (up.x, up.y));
        Assert::AreEqual (1, steps);
    }


    TEST_METHOD (Held_Still_It_Repeats_After_A_Pause)
    {
        DxuiOrbitControl  compass;
        int               steps = 0;
        POINT             up    = {};



        Place (compass);
        compass.SetOnStep ([&steps] (DxuiOrbitControl::Part) { steps++; });

        up = PointOn (compass, DxuiOrbitControl::Part::Up);

        Assert::IsTrue (compass.OnPointerDown (up.x, up.y));
        Assert::AreEqual (1, steps);

        // The first tick only schedules; the pause has to elapse before a
        // deliberate single click can become two.
        compass.Tick (1000);
        Assert::AreEqual (1, steps);

        compass.Tick (1000 + DxuiOrbitControl::kRepeatDelayMs - 1);
        Assert::AreEqual (1, steps);

        compass.Tick (1000 + DxuiOrbitControl::kRepeatDelayMs);
        Assert::AreEqual (2, steps);

        compass.Tick (1000 + DxuiOrbitControl::kRepeatDelayMs
                           + DxuiOrbitControl::kRepeatIntervalMs);
        Assert::AreEqual (3, steps);

        compass.OnPointerUp (up.x, up.y);
    }


    TEST_METHOD (Moving_Ends_The_Repeat_And_Going_Still_Does_Not_Revive_It)
    {
        DxuiOrbitControl  compass;
        int               steps = 0;
        int               drags = 0;
        POINT             up    = {};



        Place (compass);
        compass.SetOnStep ([&steps] (DxuiOrbitControl::Part) { steps++; });
        compass.SetOnDrag ([&drags] (DxuiOrbitControl::Part, float, float) { drags++; });

        up = PointOn (compass, DxuiOrbitControl::Part::Up);

        Assert::IsTrue (compass.OnPointerDown (up.x, up.y));
        Assert::AreEqual (1, steps);

        // Past the slop: this is aiming now, not clicking.
        Assert::IsTrue (compass.OnPointerMove (up.x, up.y + DxuiOrbitControl::kClickSlopPx + 6));
        Assert::AreEqual (1, drags);
        Assert::IsFalse (compass.WantsTick());

        // However long the hand then holds still, the repeat stays gone --
        // steps fired into the middle of a drag would fight the aim.
        compass.Tick (5000);
        compass.Tick (5000 + DxuiOrbitControl::kRepeatDelayMs * 4);
        Assert::AreEqual (1, steps);

        // Only the release re-arms anything.
        compass.OnPointerUp (up.x, up.y);

        Assert::IsTrue (compass.OnPointerDown (up.x, up.y));
        Assert::AreEqual (2, steps);
        Assert::IsTrue (compass.WantsTick());

        compass.OnPointerUp (up.x, up.y);
    }


    TEST_METHOD (A_Twitch_Inside_The_Slop_Keeps_The_Repeat)
    {
        DxuiOrbitControl  compass;
        int               steps = 0;
        POINT             up    = {};



        Place (compass);
        compass.SetOnStep ([&steps] (DxuiOrbitControl::Part) { steps++; });

        up = PointOn (compass, DxuiOrbitControl::Part::Up);

        Assert::IsTrue (compass.OnPointerDown (up.x, up.y));
        Assert::IsTrue (compass.OnPointerMove (up.x + 1, up.y + 1));
        Assert::IsTrue (compass.WantsTick());

        compass.Tick (2000);
        compass.Tick (2000 + DxuiOrbitControl::kRepeatDelayMs);
        Assert::AreEqual (2, steps);

        compass.OnPointerUp (up.x, up.y);
    }


    TEST_METHOD (The_Orb_Still_Commits_On_Release)
    {
        DxuiOrbitControl  compass;
        int               steps = 0;
        int               homes = 0;
        POINT             orb   = {};



        Place (compass);
        compass.SetOnStep ([&steps] (DxuiOrbitControl::Part) { steps++; });
        compass.SetOnHome ([&homes] () { homes++; });

        orb = PointOn (compass, DxuiOrbitControl::Part::Orb);

        // Home discards the framing the user built, so it keeps the release
        // and the chance to drag off it.
        Assert::IsTrue (compass.OnPointerDown (orb.x, orb.y));
        Assert::AreEqual (0, homes);
        Assert::AreEqual (0, steps);
        Assert::IsFalse (compass.WantsTick());

        compass.Tick (3000);
        compass.Tick (3000 + DxuiOrbitControl::kRepeatDelayMs * 3);
        Assert::AreEqual (0, homes);

        Assert::IsTrue (compass.OnPointerUp (orb.x, orb.y));
        Assert::AreEqual (1, homes);
    }


    TEST_METHOD (A_Press_Off_The_Compass_Is_Not_Taken)
    {
        DxuiOrbitControl  compass;
        int               steps = 0;



        Place (compass);
        compass.SetOnStep ([&steps] (DxuiOrbitControl::Part) { steps++; });

        Assert::IsFalse (compass.OnPointerDown (500, 500));
        Assert::AreEqual (0, steps);
        Assert::IsFalse (compass.WantsTick());
    }
};
