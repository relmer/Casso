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

namespace
{
    constexpr const char *  s_kpszKeyA = "AAAAAAAAAAAAAAAA";
    constexpr const char *  s_kpszKeyB = "BBBBBBBBBBBBBBBB";
    constexpr int           s_kSavedX  = 120;
    constexpr int           s_kSavedY  = 96;
    constexpr int           s_kSavedW  = 1280;
    constexpr int           s_kSavedH  = 720;
}


TEST_CLASS (WindowPlacementProfileTests)
{
public:

    TEST_METHOD (TryLoad_EmptyStore_ReturnsFalse)
    {
        GlobalUserPrefs                  prefs;
        WindowPlacementProfile           profile (prefs);
        WindowPlacementProfile::Bounds   bounds;

        Assert::IsFalse (profile.TryLoad (s_kpszKeyA, bounds),
                         L"empty prefs should fall back to default-centered placement");
    }


    TEST_METHOD (Save_ThenLoad_RoundTripsBounds)
    {
        GlobalUserPrefs                  prefs;
        WindowPlacementProfile           profile (prefs);
        WindowPlacementProfile::Bounds   saved   = { s_kSavedX, s_kSavedY, s_kSavedW, s_kSavedH };
        WindowPlacementProfile::Bounds   loaded;

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
        GlobalUserPrefs                  prefs;
        WindowPlacementProfile           profile (prefs);
        WindowPlacementProfile::Bounds   saved   = { 10, 20, 800, 600 };

        profile.Save (s_kpszKeyA, saved);

        Assert::AreEqual (size_t (1), prefs.window.placements.size());
        Assert::AreEqual (10,  prefs.window.placements[s_kpszKeyA].x);
        Assert::AreEqual (20,  prefs.window.placements[s_kpszKeyA].y);
        Assert::AreEqual (800, prefs.window.placements[s_kpszKeyA].w);
        Assert::AreEqual (600, prefs.window.placements[s_kpszKeyA].h);
    }


    TEST_METHOD (PerTopology_IsolatedFromOtherTopologies)
    {
        GlobalUserPrefs                  prefs;
        WindowPlacementProfile           profile (prefs);
        WindowPlacementProfile::Bounds   boundsA = { 100, 100, 1024, 768 };
        WindowPlacementProfile::Bounds   boundsB = { 500, 250,  640, 480 };
        WindowPlacementProfile::Bounds   loaded;

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
        WindowPlacementProfile           profile (prefs);
        WindowPlacementProfile::Bounds   loaded;

        prefs.window.placements[s_kpszKeyA] = { 0, 0, 0, 600 };

        Assert::IsFalse (profile.TryLoad (s_kpszKeyA, loaded));
    }


    TEST_METHOD (Save_OverwritesPreviousBoundsForSameTopology)
    {
        GlobalUserPrefs                  prefs;
        WindowPlacementProfile           profile (prefs);
        WindowPlacementProfile::Bounds   first   = { 10, 10, 100, 100 };
        WindowPlacementProfile::Bounds   second  = { 50, 60, 700, 400 };
        WindowPlacementProfile::Bounds   loaded;

        profile.Save (s_kpszKeyA, first);
        profile.Save (s_kpszKeyA, second);

        Assert::IsTrue (profile.TryLoad (s_kpszKeyA, loaded));
        Assert::AreEqual (50,  loaded.x);
        Assert::AreEqual (60,  loaded.y);
        Assert::AreEqual (700, loaded.w);
        Assert::AreEqual (400, loaded.h);
    }
};
