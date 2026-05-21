#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "CrtPostProcess.h"
#include "Config/GlobalUserPrefs.h"
#include "Ui/ThemeLoader.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



////////////////////////////////////////////////////////////////////////////////
//
//  CrtParameterMappingTests
//
//  P8-T8. Pure-logic round-trip between the prefs / theme JSON model and
//  the `CrtParams` struct uploaded to the shader constant buffer. No D3D
//  device is created; the shader stages are mocked by inspecting the
//  uniforms `MakeCrtParams` produces.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CrtParameterMappingTests)
{
public:

    TEST_METHOD (Load_ClampsOutOfRangeValuesToDocumentedBounds)
    {
        // Hand-built JSON with values outside every documented range. The
        // FromJson clamp logic should pull each one back to its bound so
        // a manually-edited prefs file can't drive the shaders into
        // garbage (NaN multiplies, negative blur radii, etc.).
        constexpr const char *  json =
            "{\n"
            "  \"$cassoGlobalPrefsVersion\": 1,\n"
            "  \"crt\": {\n"
            "    \"brightness\": 5.5,\n"
            "    \"scanlines\":  { \"enabled\": true,  \"intensity\": 2.0 },\n"
            "    \"bloom\":      { \"enabled\": true,  \"radius\": 99.0, \"strength\": -1.0 },\n"
            "    \"colorBleed\": { \"enabled\": true,  \"width\": -3.0 }\n"
            "  }\n"
            "}\n";

        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        JsonValue           parsed;
        JsonParseError      err;
        HRESULT             hr;

        hr = JsonParser::Parse (json, parsed, err);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = prefs.FromJson (parsed);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (2.0f, prefs.crt.brightness);
        Assert::AreEqual (1.0f, prefs.crt.scanlinesIntensity);
        Assert::AreEqual (4.0f, prefs.crt.bloomRadius);
        Assert::AreEqual (0.0f, prefs.crt.bloomStrength);
        Assert::AreEqual (0.0f, prefs.crt.colorBleedWidth);
        Assert::IsTrue   (prefs.crt.userOverride);
    }


    TEST_METHOD (MakeCrtParams_FromDefaultPrefs_HasExpectedShape)
    {
        GlobalUserPrefs  prefs;

        CrtParams  params = MakeCrtParams (prefs.crt, nullptr, 1920.0f, 1080.0f);

        // brightness defaults to 1.0; every "enabled" toggle is off out of
        // the box (FR-038 / FR-040) so all effect magnitudes should be 0
        // even though prefs has nonzero intensity/radius/strength sliders
        // pre-populated (the slider's stored default isn't applied until
        // the user enables the effect).
        Assert::AreEqual (1.0f,    params.brightness);
        Assert::AreEqual (0.0f,    params.scanlineIntensity);
        Assert::AreEqual (0.0f,    params.bloomRadius);
        Assert::AreEqual (0.0f,    params.bloomStrength);
        Assert::AreEqual (0.0f,    params.colorBleedWidth);
        Assert::AreEqual (1920.0f, params.outputW);
        Assert::AreEqual (1080.0f, params.outputH);
    }


    TEST_METHOD (MakeCrtParams_DisabledEffectsZeroOutMagnitudes)
    {
        // Even with the effect *sliders* set to non-defaults, an effect
        // whose `enabled` toggle is false must contribute zero to the
        // shader uniforms; that's how `CrtPostProcess::Process` keeps a
        // single fixed pipeline regardless of which subset is enabled.
        GlobalUserPrefs  prefs;
        prefs.crt.userOverride        = true;
        prefs.crt.brightness          = 1.5f;
        prefs.crt.scanlinesEnabled    = false;
        prefs.crt.scanlinesIntensity  = 0.9f;
        prefs.crt.bloomEnabled        = false;
        prefs.crt.bloomRadius         = 3.0f;
        prefs.crt.bloomStrength       = 0.8f;
        prefs.crt.colorBleedEnabled   = false;
        prefs.crt.colorBleedWidth     = 2.5f;

        CrtParams  params = MakeCrtParams (prefs.crt, nullptr, 800.0f, 600.0f);

        Assert::AreEqual (1.5f, params.brightness);
        Assert::AreEqual (0.0f, params.scanlineIntensity);
        Assert::AreEqual (0.0f, params.bloomRadius);
        Assert::AreEqual (0.0f, params.bloomStrength);
        Assert::AreEqual (0.0f, params.colorBleedWidth);
    }


    TEST_METHOD (MakeCrtParams_EnabledEffectsPropagateSliderValues)
    {
        GlobalUserPrefs  prefs;
        prefs.crt.userOverride        = true;
        prefs.crt.brightness          = 1.2f;
        prefs.crt.scanlinesEnabled    = true;
        prefs.crt.scanlinesIntensity  = 0.7f;
        prefs.crt.bloomEnabled        = true;
        prefs.crt.bloomRadius         = 2.0f;
        prefs.crt.bloomStrength       = 0.4f;
        prefs.crt.colorBleedEnabled   = true;
        prefs.crt.colorBleedWidth     = 1.5f;

        CrtParams  params = MakeCrtParams (prefs.crt, nullptr, 1024.0f, 768.0f);

        Assert::AreEqual (1.2f, params.brightness);
        Assert::AreEqual (0.7f, params.scanlineIntensity);
        Assert::AreEqual (2.0f, params.bloomRadius);
        Assert::AreEqual (0.4f, params.bloomStrength);
        Assert::AreEqual (1.5f, params.colorBleedWidth);
    }


    TEST_METHOD (MakeCrtParams_ThemeOverride_OnlyAppliesWhenUserHasNoOverride)
    {
        // Theme says "scanlines on at 0.8, bloom on at radius=3.0". Two
        // cases:
        //
        //   1. prefs.userOverride == false (first-run; user hasn't touched
        //      anything yet) -> theme defaults take effect.
        //   2. prefs.userOverride == true (user customised at least one
        //      slider) -> theme defaults are ignored entirely, prefs win.
        ThemeCrtDefaults  theme;
        theme.brightness          = 0.85f;
        theme.scanlinesEnabled    = true;
        theme.scanlinesIntensity  = 0.8f;
        theme.bloomEnabled        = true;
        theme.bloomRadius         = 3.0f;
        theme.bloomStrength       = 0.55f;
        theme.colorBleedEnabled   = false;
        theme.colorBleedWidth     = 1.0f;

        // Case 1 -- no user override; theme wins.
        {
            GlobalUserPrefs  prefs;          // userOverride == false
            CrtParams  params = MakeCrtParams (prefs.crt, &theme, 640.0f, 480.0f);

            Assert::AreEqual (0.85f, params.brightness);
            Assert::AreEqual (0.8f,  params.scanlineIntensity);
            Assert::AreEqual (3.0f,  params.bloomRadius);
            Assert::AreEqual (0.55f, params.bloomStrength);
            Assert::AreEqual (0.0f,  params.colorBleedWidth);
        }

        // Case 2 -- user override; prefs win even when theme has different
        // values for the same fields.
        {
            GlobalUserPrefs  prefs;
            prefs.crt.userOverride       = true;
            prefs.crt.brightness         = 1.4f;
            prefs.crt.scanlinesEnabled   = false;
            prefs.crt.scanlinesIntensity = 0.2f;
            prefs.crt.bloomEnabled       = false;
            prefs.crt.bloomRadius        = 1.0f;
            prefs.crt.bloomStrength      = 0.1f;
            prefs.crt.colorBleedEnabled  = false;
            prefs.crt.colorBleedWidth    = 0.5f;

            CrtParams  params = MakeCrtParams (prefs.crt, &theme, 640.0f, 480.0f);

            Assert::AreEqual (1.4f, params.brightness);
            Assert::AreEqual (0.0f, params.scanlineIntensity);  // disabled
            Assert::AreEqual (0.0f, params.bloomRadius);        // disabled
            Assert::AreEqual (0.0f, params.bloomStrength);      // disabled
            Assert::AreEqual (0.0f, params.colorBleedWidth);    // disabled
        }
    }


    TEST_METHOD (FromJson_WithoutCrtSection_LeavesUserOverrideFalseAndDefaults)
    {
        // Document containing every top-level field EXCEPT `crt`. The
        // user-override flag must stay false so the theme defaults path
        // can win on first run, and every CRT field must equal its
        // struct default.
        constexpr const char *  json =
            "{\n"
            "  \"$cassoGlobalPrefsVersion\": 1,\n"
            "  \"activeTheme\": \"Dark Modern\",\n"
            "  \"window\": { \"fullscreen\": false }\n"
            "}\n";

        GlobalUserPrefs     prefs;
        JsonValue           parsed;
        JsonParseError      err;
        HRESULT             hr;

        hr = JsonParser::Parse (json, parsed, err);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = prefs.FromJson (parsed);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsFalse  (prefs.crt.userOverride);
        Assert::AreEqual (1.0f,  prefs.crt.brightness);
        Assert::AreEqual (false, prefs.crt.scanlinesEnabled);
        Assert::AreEqual (0.5f,  prefs.crt.scanlinesIntensity);
        Assert::AreEqual (false, prefs.crt.bloomEnabled);
        Assert::AreEqual (1.0f,  prefs.crt.bloomRadius);
        Assert::AreEqual (0.5f,  prefs.crt.bloomStrength);
        Assert::AreEqual (false, prefs.crt.colorBleedEnabled);
        Assert::AreEqual (1.0f,  prefs.crt.colorBleedWidth);
    }


    TEST_METHOD (ComputeLetterboxRect_HandlesPillarboxAndLetterbox)
    {
        // Wide window -> pillarbox (vertical black bars on left + right).
        RECT  pb = ComputeLetterboxRect (1600, 900);
        Assert::AreEqual (1200L, (long) (pb.right - pb.left));
        Assert::AreEqual (900L,  (long) (pb.bottom - pb.top));
        Assert::AreEqual (200L,  (long) pb.left);
        Assert::AreEqual (0L,    (long) pb.top);

        // Tall window -> letterbox (horizontal bars on top + bottom).
        RECT  lb = ComputeLetterboxRect (800, 800);
        Assert::AreEqual (800L, (long) (lb.right - lb.left));
        Assert::AreEqual (600L, (long) (lb.bottom - lb.top));
        Assert::AreEqual (0L,   (long) lb.left);
        Assert::AreEqual (100L, (long) lb.top);

        // Exact 4:3 -> full window, no bars.
        RECT  ex = ComputeLetterboxRect (1024, 768);
        Assert::AreEqual (0L,    (long) ex.left);
        Assert::AreEqual (0L,    (long) ex.top);
        Assert::AreEqual (1024L, (long) ex.right);
        Assert::AreEqual (768L,  (long) ex.bottom);
    }
};
