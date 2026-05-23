#include "Pch.h"

#include "InMemoryRegistry.h"

#include "Config/WindowPlacementProfile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfileTests
//
//  Exercises the per-monitor-topology registry persistence shape
//  without touching HKCU. The topology-hash builder is itself a Win32
//  function (EnumDisplayMonitors) and is excluded from unit-test
//  coverage; the load/save round-trip is what these tests pin.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr LPCWSTR  s_kpszSubkeyA = L"WindowPlacement\\v1\\AAAAAAAAAAAAAAAA";
    constexpr LPCWSTR  s_kpszSubkeyB = L"WindowPlacement\\v1\\BBBBBBBBBBBBBBBB";
    constexpr LONG     s_kSavedX     = 120;
    constexpr LONG     s_kSavedY     = 96;
    constexpr int      s_kSavedW     = 1280;
    constexpr int      s_kSavedH     = 720;
}


TEST_CLASS (WindowPlacementProfileTests)
{
public:

    TEST_METHOD (TryLoad_EmptyStore_ReturnsFalse)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   bounds;

        Assert::IsFalse (profile.TryLoad (s_kpszSubkeyA, bounds),
                         L"empty registry should fall back to default-centered placement");
    }


    TEST_METHOD (Save_ThenLoad_RoundTripsBounds)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   saved   = { s_kSavedX, s_kSavedY, s_kSavedW, s_kSavedH };
        WindowPlacementProfile::Bounds   loaded;
        HRESULT                          hr      = S_OK;

        hr = profile.Save (s_kpszSubkeyA, saved);

        Assert::IsTrue (SUCCEEDED (hr),
                        L"Save must succeed against InMemoryRegistry");
        Assert::IsTrue (profile.TryLoad (s_kpszSubkeyA, loaded),
                        L"TryLoad must report the freshly saved bounds");
        Assert::AreEqual (s_kSavedX, loaded.x);
        Assert::AreEqual (s_kSavedY, loaded.y);
        Assert::AreEqual (s_kSavedW, loaded.w);
        Assert::AreEqual (s_kSavedH, loaded.h);
    }


    TEST_METHOD (Save_WritesAllFourComponents)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   saved   = { 10, 20, 800, 600 };
        HRESULT                          hr      = S_OK;

        IGNORE_RETURN_VALUE (hr, profile.Save (s_kpszSubkeyA, saved));

        Assert::IsTrue (reg.HasString (s_kpszSubkeyA, L"x"));
        Assert::IsTrue (reg.HasString (s_kpszSubkeyA, L"y"));
        Assert::IsTrue (reg.HasString (s_kpszSubkeyA, L"w"));
        Assert::IsTrue (reg.HasString (s_kpszSubkeyA, L"h"));

        Assert::AreEqual (std::wstring (L"10"),  reg.GetString (s_kpszSubkeyA, L"x"));
        Assert::AreEqual (std::wstring (L"600"), reg.GetString (s_kpszSubkeyA, L"h"));
    }


    TEST_METHOD (PerTopology_IsolatedFromOtherTopologies)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   boundsA = { 100, 100, 1024, 768 };
        WindowPlacementProfile::Bounds   boundsB = { 500, 250,  640, 480 };
        WindowPlacementProfile::Bounds   loaded;
        HRESULT                          hr      = S_OK;

        IGNORE_RETURN_VALUE (hr, profile.Save (s_kpszSubkeyA, boundsA));
        IGNORE_RETURN_VALUE (hr, profile.Save (s_kpszSubkeyB, boundsB));

        Assert::IsTrue (profile.TryLoad (s_kpszSubkeyA, loaded));
        Assert::AreEqual (100,  (int) loaded.x);
        Assert::AreEqual (1024, loaded.w);

        Assert::IsTrue (profile.TryLoad (s_kpszSubkeyB, loaded));
        Assert::AreEqual (500, (int) loaded.x);
        Assert::AreEqual (640, loaded.w);
    }


    TEST_METHOD (TryLoad_MissingOneComponent_ReturnsFalse)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   loaded;
        HRESULT                          hr      = S_OK;

        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"x", L"100"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"y", L"100"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"w", L"800"));

        Assert::IsFalse (profile.TryLoad (s_kpszSubkeyA, loaded));
    }


    TEST_METHOD (TryLoad_NonNumericValue_ReturnsFalse)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   loaded;
        HRESULT                          hr      = S_OK;

        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"x", L"oops"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"y", L"0"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"w", L"100"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"h", L"100"));

        Assert::IsFalse (profile.TryLoad (s_kpszSubkeyA, loaded));
    }


    TEST_METHOD (TryLoad_NonPositiveSize_ReturnsFalse)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   loaded;
        HRESULT                          hr      = S_OK;

        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"x", L"0"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"y", L"0"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"w", L"0"));
        IGNORE_RETURN_VALUE (hr, reg.WriteString (s_kpszSubkeyA, L"h", L"600"));

        Assert::IsFalse (profile.TryLoad (s_kpszSubkeyA, loaded));
    }


    TEST_METHOD (Save_OverwritesPreviousBoundsForSameTopology)
    {
        InMemoryRegistry                 reg;
        WindowPlacementProfile           profile (reg);
        WindowPlacementProfile::Bounds   first   = { 10, 10, 100, 100 };
        WindowPlacementProfile::Bounds   second  = { 50, 60, 700, 400 };
        WindowPlacementProfile::Bounds   loaded;
        HRESULT                          hr      = S_OK;

        IGNORE_RETURN_VALUE (hr, profile.Save (s_kpszSubkeyA, first));
        IGNORE_RETURN_VALUE (hr, profile.Save (s_kpszSubkeyA, second));

        Assert::IsTrue (profile.TryLoad (s_kpszSubkeyA, loaded));
        Assert::AreEqual (50,  (int) loaded.x);
        Assert::AreEqual (60,  (int) loaded.y);
        Assert::AreEqual (700, loaded.w);
        Assert::AreEqual (400, loaded.h);
    }
};
