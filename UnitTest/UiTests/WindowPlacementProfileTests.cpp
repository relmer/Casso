#include "Pch.h"

#include "Config/WindowPlacementProfile.h"
#include "Config/GlobalUserPrefs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfileTests
//
//  Exercises the per-monitor-topology persistence shape against a
//  stack-allocated GlobalUserPrefs (no disk, no registry). The topology-
//  hash builder is itself a Win32 function (EnumDisplayMonitors) and is
//  excluded from unit-test coverage; the load/save round-trip is what
//  these tests pin.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (WindowPlacementProfileTests)
{
public:

    static constexpr const char *  s_kpszKeyA = "AAAAAAAAAAAAAAAA";
    static constexpr const char *  s_kpszKeyB = "BBBBBBBBBBBBBBBB";
    static constexpr int           s_kSavedX  = 120;
    static constexpr int           s_kSavedY  = 96;
    static constexpr int           s_kSavedW  = 1280;
    static constexpr int           s_kSavedH  = 720;

    TEST_METHOD (TryLoad_EmptyStore_ReturnsFalse)
    {
        GlobalUserPrefs                  prefs;
        WindowPlacementProfile::Bounds   bounds;
        WindowPlacementProfile           profile (prefs);

        Assert::IsFalse (profile.TryLoad (s_kpszKeyA, bounds),
                         L"empty prefs should fall back to default-centered placement");
    }


    TEST_METHOD (Save_ThenLoad_RoundTripsBounds)
    {
        GlobalUserPrefs                 prefs;
        WindowPlacementProfile::Bounds  loaded;
        WindowPlacementProfile::Bounds  saved  = {};
        WindowPlacementProfile           profile (prefs);
        saved = { s_kSavedX, s_kSavedY, s_kSavedW, s_kSavedH };

        profile.Save (s_kpszKeyA, saved);

        Assert::IsTrue (profile.TryLoad (s_kpszKeyA, loaded),
                        L"TryLoad must report the freshly saved bounds");
        Assert::AreEqual (s_kSavedX, loaded.x);
        Assert::AreEqual (s_kSavedY, loaded.y);
        Assert::AreEqual (s_kSavedW, loaded.w);
        Assert::AreEqual (s_kSavedH, loaded.h);
    }


    TEST_METHOD (Save_LandsInWindowPlacementsMap)
    {
        GlobalUserPrefs                 prefs;
        WindowPlacementProfile::Bounds  saved = {};
        WindowPlacementProfile           profile (prefs);
        saved = { 10, 20, 800, 600 };

        profile.Save (s_kpszKeyA, saved);

        Assert::AreEqual (size_t (1), prefs.window.placements.size());
        Assert::AreEqual (10,  prefs.window.placements[s_kpszKeyA].x);
        Assert::AreEqual (20,  prefs.window.placements[s_kpszKeyA].y);
        Assert::AreEqual (800, prefs.window.placements[s_kpszKeyA].w);
        Assert::AreEqual (600, prefs.window.placements[s_kpszKeyA].h);
    }


    TEST_METHOD (PerTopology_IsolatedFromOtherTopologies)
    {
        GlobalUserPrefs                 prefs;
        WindowPlacementProfile::Bounds  loaded;
        WindowPlacementProfile::Bounds  boundsA = {};
        WindowPlacementProfile::Bounds  boundsB = {};
        WindowPlacementProfile           profile (prefs);
        boundsA = { 100, 100, 1024, 768 };
        boundsB = { 500, 250,  640, 480 };

        profile.Save (s_kpszKeyA, boundsA);
        profile.Save (s_kpszKeyB, boundsB);

        Assert::IsTrue (profile.TryLoad (s_kpszKeyA, loaded));
        Assert::AreEqual (100,  loaded.x);
        Assert::AreEqual (1024, loaded.w);

        Assert::IsTrue (profile.TryLoad (s_kpszKeyB, loaded));
        Assert::AreEqual (500, loaded.x);
        Assert::AreEqual (640, loaded.w);
    }


    TEST_METHOD (TryLoad_NonPositiveSize_ReturnsFalse)
    {
        GlobalUserPrefs                  prefs;
        WindowPlacementProfile::Bounds   loaded;
        WindowPlacementProfile           profile (prefs);

        prefs.window.placements[s_kpszKeyA] = { 0, 0, 0, 600 };

        Assert::IsFalse (profile.TryLoad (s_kpszKeyA, loaded));
    }


    TEST_METHOD (Save_OverwritesPreviousBoundsForSameTopology)
    {
        GlobalUserPrefs                 prefs;
        WindowPlacementProfile::Bounds  loaded;
        WindowPlacementProfile::Bounds  first  = {};
        WindowPlacementProfile::Bounds  second = {};
        WindowPlacementProfile           profile (prefs);
        first = { 10, 10, 100, 100 };
        second = { 50, 60, 700, 400 };

        profile.Save (s_kpszKeyA, first);
        profile.Save (s_kpszKeyA, second);

        Assert::IsTrue (profile.TryLoad (s_kpszKeyA, loaded));
        Assert::AreEqual (50,  loaded.x);
        Assert::AreEqual (60,  loaded.y);
        Assert::AreEqual (700, loaded.w);
        Assert::AreEqual (400, loaded.h);
    }


    //
    //  FitToWorkArea -- the placement rule Ctrl+0 relies on.
    //
    //  Ctrl+0 sizes the window so the emulator sits at 100% inside the whole
    //  desk scene, which can ask for more than the monitor holds. It used to
    //  center that oversized window on the work area, which put the caption's
    //  top-left off the top-left of the screen: a window the pointer could no
    //  longer grab, move, or close.
    //

    static RECT MakeWork (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }


    TEST_METHOD (FitToWorkArea_WindowThatFits_IsCentered)
    {
        RECT  work   = MakeWork (0, 0, 1920, 1080);
        RECT  placed = WindowPlacementProfile::FitToWorkArea (work, 800, 600);

        Assert::AreEqual (560L, placed.left);
        Assert::AreEqual (240L, placed.top);
        Assert::AreEqual (800L, placed.right - placed.left);
        Assert::AreEqual (600L, placed.bottom - placed.top);
    }


    TEST_METHOD (FitToWorkArea_OversizedWindow_ShrinksUniformlyAndKeepsItsShape)
    {
        // 4:3 asked for, 16:9 available. Clamping each axis on its own would
        // hand back the whole 1920x1080 work area and silently change the
        // proportions -- and the desk-scene window is sized so the scene
        // exactly fills it, so the scene would then letterbox itself inside
        // the wrong-shaped frame, leaving dead bands down the sides and
        // along the bottom. Height binds here: 1080/3000 scales width to
        // 1440, and the result is still 4:3.
        RECT  work   = MakeWork (0, 0, 1920, 1080);
        RECT  placed = WindowPlacementProfile::FitToWorkArea (work, 4000, 3000);
        int   w      = (int) (placed.right  - placed.left);
        int   h      = (int) (placed.bottom - placed.top);

        Assert::AreEqual (1440, w);
        Assert::AreEqual (1080, h);
        Assert::AreEqual (4.0 / 3.0, (double) w / (double) h, 0.01);

        Assert::AreEqual (240L, placed.left, L"centered in the room left over");
        Assert::AreEqual (0L,   placed.top);
    }


    TEST_METHOD (FitToWorkArea_TallWindowOnAWideScreen_KeepsItsShape)
    {
        // The shape the desk scene actually asks for once the monitor stands
        // on the drives: taller than it is wide, on a landscape monitor.
        RECT  work   = MakeWork (0, 0, 2880, 1830);
        RECT  placed = WindowPlacementProfile::FitToWorkArea (work, 2400, 2700);
        int   w      = (int) (placed.right  - placed.left);
        int   h      = (int) (placed.bottom - placed.top);

        Assert::AreEqual (1830, h, L"height binds");
        Assert::AreEqual (2400.0 / 2700.0, (double) w / (double) h, 0.01);
        Assert::IsTrue (w < 2880, L"width must come down with it, not stay pinned wide");
    }


    TEST_METHOD (FitToWorkArea_HonorsAWorkAreaThatIsNotAtTheOrigin)
    {
        // A taskbar on the left / top, or a secondary monitor at negative
        // coordinates: every edge is the WORK AREA's, never (0, 0). An
        // oversized request shrinks to fit and centers inside it...
        RECT  work   = MakeWork (-1920, -200, -320, 700);
        RECT  placed = WindowPlacementProfile::FitToWorkArea (work, 5000, 5000);

        Assert::IsTrue (placed.left   >= work.left);
        Assert::IsTrue (placed.top    >= work.top);
        Assert::IsTrue (placed.right  <= work.right);
        Assert::IsTrue (placed.bottom <= work.bottom);
        Assert::AreEqual (900L, placed.bottom - placed.top, L"the short axis binds");

        // ...and when it cannot fit at all, it pins to the work area's own
        // origin rather than the desktop's.
        RECT  pinned = WindowPlacementProfile::FitToWorkArea (work, 100, 100, 4000, 4000);

        Assert::AreEqual (-1920L, pinned.left);
        Assert::AreEqual (-200L,  pinned.top);
    }


    TEST_METHOD (FitToWorkArea_MinimumLargerThanMonitor_KeepsTopLeftOnScreen)
    {
        // The only case that can still overflow: a minimum window size the
        // monitor cannot hold. The overflow must go RIGHT and DOWN -- the
        // caption's top-left corner stays reachable, which is the whole
        // point of the rule.
        RECT  work   = MakeWork (100, 50, 900, 500);
        RECT  placed = WindowPlacementProfile::FitToWorkArea (work, 400, 300, 1600, 1200);

        Assert::AreEqual (100L, placed.left, L"top-left must never leave the work area");
        Assert::AreEqual (50L,  placed.top,  L"top-left must never leave the work area");
        Assert::AreEqual (1600L, placed.right  - placed.left);
        Assert::AreEqual (1200L, placed.bottom - placed.top);
        Assert::IsTrue (placed.right > work.right, L"overflow belongs on the right edge");
        Assert::IsTrue (placed.bottom > work.bottom, L"overflow belongs on the bottom edge");
    }


    TEST_METHOD (FitToWorkArea_NeverPlacesTheOriginAboveOrLeftOfTheWorkArea)
    {
        // Swept across sizes from far smaller to far larger than the work
        // area: no size may produce an unreachable caption.
        RECT  work = MakeWork (0, 0, 1280, 720);

        for (int size = 100; size <= 4000; size += 137)
        {
            RECT  placed = WindowPlacementProfile::FitToWorkArea (work, size, size);

            Assert::IsTrue (placed.left >= work.left, L"caption pushed off the left edge");
            Assert::IsTrue (placed.top  >= work.top,  L"caption pushed off the top edge");
        }
    }
};
