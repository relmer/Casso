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
//  Pure-logic round-trip between the prefs / theme JSON model and
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
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        JsonValue           parsed;
        JsonParseError      err;
        HRESULT             hr;



        // Hand-built JSON with values outside every documented range. The
        // FromJson clamp logic should pull each one back to its bound so
        // a manually-edited prefs file can't drive the shaders into
        // garbage (NaN multiplies, negative blur radii, etc.).
        constexpr const char *  json =
            "{\n"
            "  \"$cassoGlobalPrefsVersion\": 1,\n"
            "  \"crt\": {\n"
            "    \"color\": {\n"
            "      \"userOverride\": true,\n"
            "      \"brightness\": 5.5,\n"
            "      \"scanlines\":  { \"enabled\": true,  \"intensity\": 2.0 },\n"
            "      \"bloom\":      { \"enabled\": true,  \"radius\": 99.0, \"strength\": -1.0 },\n"
            "      \"colorBleed\": { \"enabled\": true,  \"width\": -3.0 }\n"
            "    }\n"
            "  }\n"
            "}\n";


        hr = JsonParser::Parse (json, parsed, err);
        AssertSucceeded (hr);

        hr = prefs.FromJson (parsed);
        AssertSucceeded (hr);

        Assert::AreEqual (2.0f,  prefs.crtByMode[0].brightness);
        Assert::AreEqual (1.0f,  prefs.crtByMode[0].scanlinesIntensity);
        Assert::AreEqual (4.0f,  prefs.crtByMode[0].bloomRadius);    // clamp 0..4 px
        Assert::AreEqual (0.0f,  prefs.crtByMode[0].bloomStrength);
        Assert::AreEqual (0.0f,  prefs.crtByMode[0].colorBleedWidth);
        Assert::IsTrue   (prefs.crtByMode[0].userOverride);
    }


    TEST_METHOD (MakeCrtParams_FromDefaultPrefs_HasExpectedShape)
    {
        GlobalUserPrefs  prefs;

        CrtParams  params = MakeCrtParams (prefs.crtByMode[0], 0, nullptr, 1920.0f, 1080.0f);

        // With userOverride=false and no theme override, MakeCrtParams
        // pulls from the Color monitor preset (CrtPresets::GetPreset(0)).
        // That preset has bloom on (radius 2 / strength 0.30) and color
        // bleed on (width 3) by design -- those are the defining color
        // CRT looks. Scanlines on at 0.20 for subtle TV-line texture;
        // persistence off (P22 phosphors decay in ~30us).
        Assert::AreEqual (1.0f,    params.brightness);
        Assert::AreEqual (1.0f,    params.contrast);
        Assert::AreEqual (0.20f,   params.scanlineIntensity);
        Assert::AreEqual (1.0f,    params.bloomRadius);
        Assert::AreEqual (0.20f,   params.bloomStrength);
        Assert::AreEqual (3.0f,    params.colorBleedWidth);
        Assert::AreEqual (1920.0f, params.outputW);
        Assert::AreEqual (1080.0f, params.outputH);
        Assert::AreEqual (1.0f,    params.gamma);
        Assert::AreEqual (0.0f,    params.persistence);
    }


    TEST_METHOD (MakeCrtParams_DisabledEffectsZeroOutMagnitudes)
    {
        CrtParams  params = {};



        // Even with the effect *sliders* set to non-defaults, an effect
        // whose `enabled` toggle is false must contribute zero to the
        // shader uniforms; that's how `CrtPostProcess::Process` keeps a
        // single fixed pipeline regardless of which subset is enabled.
        GlobalUserPrefs  prefs;
        prefs.crtByMode[0].userOverride        = true;
        prefs.crtByMode[0].brightness          = 1.5f;
        prefs.crtByMode[0].scanlinesEnabled    = false;
        prefs.crtByMode[0].scanlinesIntensity  = 0.9f;
        prefs.crtByMode[0].bloomEnabled        = false;
        prefs.crtByMode[0].bloomRadius         = 3.0f;
        prefs.crtByMode[0].bloomStrength       = 0.8f;
        prefs.crtByMode[0].colorBleedEnabled   = false;
        prefs.crtByMode[0].colorBleedWidth     = 2.5f;

        params = MakeCrtParams (prefs.crtByMode[0], 0, nullptr, 800.0f, 600.0f);

        Assert::AreEqual (1.5f, params.brightness);
        Assert::AreEqual (0.0f, params.scanlineIntensity);
        Assert::AreEqual (0.0f, params.bloomRadius);
        Assert::AreEqual (0.0f, params.bloomStrength);
        Assert::AreEqual (0.0f, params.colorBleedWidth);
    }


    TEST_METHOD (MakeCrtParams_EnabledEffectsPropagateSliderValues)
    {
        GlobalUserPrefs  prefs;
        CrtParams        params = {};
        prefs.crtByMode[0].userOverride        = true;
        prefs.crtByMode[0].brightness          = 1.2f;
        prefs.crtByMode[0].contrast            = 1.6f;
        prefs.crtByMode[0].scanlinesEnabled    = true;
        prefs.crtByMode[0].scanlinesIntensity  = 0.7f;
        prefs.crtByMode[0].bloomEnabled        = true;
        prefs.crtByMode[0].bloomRadius         = 2.0f;
        prefs.crtByMode[0].bloomStrength       = 0.4f;
        prefs.crtByMode[0].colorBleedEnabled   = true;
        prefs.crtByMode[0].colorBleedWidth     = 1.5f;

        params = MakeCrtParams (prefs.crtByMode[0], 0, nullptr, 1024.0f, 768.0f);

        Assert::AreEqual (1.2f, params.brightness);
        Assert::AreEqual (1.6f, params.contrast);
        Assert::AreEqual (0.7f, params.scanlineIntensity);
        Assert::AreEqual (2.0f, params.bloomRadius);
        Assert::AreEqual (0.4f, params.bloomStrength);
        Assert::AreEqual (1.5f, params.colorBleedWidth);
    }


    TEST_METHOD (MakeCrtParams_ThemeOverride_OnlyAppliesWhenUserHasNoOverride)
    {
        ThemeCrtDefaults  theme;
        theme.brightness          = 0.85f;
        theme.contrast            = 1.15f;
        theme.scanlinesEnabled    = true;
        theme.scanlinesIntensity  = 0.8f;
        theme.bloomEnabled        = true;
        theme.bloomRadius         = 3.0f;
        theme.bloomStrength       = 0.55f;
        theme.colorBleedEnabled   = false;
        theme.colorBleedWidth     = 1.0f;
        // Mark all field groups as theme-declared so the layering
        // logic in MakeCrtParams applies them (mirrors what
        // ThemeLoader does when the JSON has every block).
        theme.hasBrightness       = true;
        theme.hasContrast         = true;
        theme.hasScanlines        = true;
        theme.hasBloom            = true;
        theme.hasColorBleed       = true;

        // Case 1 -- no user override; theme wins.
        {
            GlobalUserPrefs  prefs;   // userOverride == false
            CrtParams        params = MakeCrtParams (prefs.crtByMode[0], 0, &theme, 640.0f, 480.0f);

            Assert::AreEqual (0.85f, params.brightness);
            Assert::AreEqual (1.15f, params.contrast);
            Assert::AreEqual (0.8f,  params.scanlineIntensity);
            Assert::AreEqual (3.0f,  params.bloomRadius);
            Assert::AreEqual (0.55f, params.bloomStrength);
            Assert::AreEqual (0.0f,  params.colorBleedWidth);
        }

        // Case 2 -- user override; prefs win even when theme has different
        // values for the same fields.
        {
            GlobalUserPrefs  prefs;
            CrtParams        params = {};
            prefs.crtByMode[0].userOverride       = true;
            prefs.crtByMode[0].brightness         = 1.4f;
            prefs.crtByMode[0].contrast           = 0.7f;
            prefs.crtByMode[0].scanlinesEnabled   = false;
            prefs.crtByMode[0].scanlinesIntensity = 0.2f;
            prefs.crtByMode[0].bloomEnabled       = false;
            prefs.crtByMode[0].bloomRadius        = 1.0f;
            prefs.crtByMode[0].bloomStrength      = 0.1f;
            prefs.crtByMode[0].colorBleedEnabled  = false;
            prefs.crtByMode[0].colorBleedWidth    = 0.5f;

            params = MakeCrtParams (prefs.crtByMode[0], 0, &theme, 640.0f, 480.0f);

            Assert::AreEqual (1.4f, params.brightness);
            Assert::AreEqual (0.7f, params.contrast);
            Assert::AreEqual (0.0f, params.scanlineIntensity);  // disabled
            Assert::AreEqual (0.0f, params.bloomRadius);        // disabled
            Assert::AreEqual (0.0f, params.bloomStrength);      // disabled
            Assert::AreEqual (0.0f, params.colorBleedWidth);    // disabled
        }
    }


    TEST_METHOD (FromJson_WithoutCrtSection_LeavesUserOverrideFalseAndDefaults)
    {
        GlobalUserPrefs     prefs;
        JsonValue           parsed;
        JsonParseError      err;
        HRESULT             hr;



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


        hr = JsonParser::Parse (json, parsed, err);
        AssertSucceeded (hr);

        hr = prefs.FromJson (parsed);
        AssertSucceeded (hr);

        Assert::IsFalse  (prefs.crtByMode[0].userOverride);
        Assert::AreEqual (1.0f,  prefs.crtByMode[0].brightness);
        Assert::AreEqual (false, prefs.crtByMode[0].scanlinesEnabled);
        Assert::AreEqual (0.5f,  prefs.crtByMode[0].scanlinesIntensity);
        Assert::AreEqual (false, prefs.crtByMode[0].bloomEnabled);
        Assert::AreEqual (1.0f,  prefs.crtByMode[0].bloomRadius);
        Assert::AreEqual (0.5f,  prefs.crtByMode[0].bloomStrength);
        Assert::AreEqual (false, prefs.crtByMode[0].colorBleedEnabled);
        Assert::AreEqual (1.0f,  prefs.crtByMode[0].colorBleedWidth);
    }


    TEST_METHOD (MakeCrtParams_CarriesABloomThresholdThroughEveryLayer)
    {
        GlobalUserPrefs   prefs;
        ThemeCrtDefaults  theme;
        CrtParams         params = {};



        // Only bright pixels feed the bloom, so the threshold has to reach
        // the shader on every path. It is not a per-monitor value and not a
        // prefs field, so a user override and a theme override must both
        // leave it alone rather than zeroing it back to a bloom that lifts
        // every dark pixel next to a lit one.
        params = MakeCrtParams (prefs.crtByMode[0], 0, nullptr, 640.0f, 480.0f);
        Assert::IsTrue (params.bloomThreshold > 0.0f);

        theme.hasBloom       = true;
        theme.bloomEnabled   = true;
        theme.bloomRadius    = 2.0f;
        theme.bloomStrength  = 0.4f;
        params = MakeCrtParams (prefs.crtByMode[0], 0, &theme, 640.0f, 480.0f);
        Assert::IsTrue (params.bloomThreshold > 0.0f);

        prefs.crtByMode[0].userOverride  = true;
        prefs.crtByMode[0].bloomEnabled  = true;
        prefs.crtByMode[0].bloomRadius   = 2.0f;
        prefs.crtByMode[0].bloomStrength = 0.7f;
        params = MakeCrtParams (prefs.crtByMode[0], 0, nullptr, 640.0f, 480.0f);
        Assert::IsTrue (params.bloomThreshold > 0.0f);

        // Below full scale, or nothing would ever clear it and the bloom
        // would vanish rather than pick out the bright areas.
        Assert::IsTrue (params.bloomThreshold < 1.0f);
    }


    TEST_METHOD (Load_ClampsBloomRadiusToTheDocumentedCeiling)
    {
        GlobalUserPrefs  prefs;
        JsonValue        parsed;
        JsonParseError   err;
        HRESULT          hr;



        // Halation runs to about two emulated pixels of sigma, so the useful
        // span ends well under 4 and the schema in spec 007 caps it there.
        // The clamp used to admit 10, which is why the slider offered six
        // stops past anything worth picking.
        constexpr const char *  json =
            "{\n"
            "  \"$cassoGlobalPrefsVersion\": 1,\n"
            "  \"crt\": {\n"
            "    \"green\": {\n"
            "      \"userOverride\": true,\n"
            "      \"bloom\": { \"enabled\": true, \"radius\": 9.0, \"strength\": 0.5 }\n"
            "    }\n"
            "  }\n"
            "}\n";


        hr = JsonParser::Parse (json, parsed, err);
        AssertSucceeded (hr);

        hr = prefs.FromJson (parsed);
        AssertSucceeded (hr);

        Assert::AreEqual (4.0f, prefs.crtByMode[1].bloomRadius);
    }


    TEST_METHOD (MakeCrtParams_LeavesPixelScaleAtUnity)
    {
        GlobalUserPrefs  prefs;

        CrtParams  params = MakeCrtParams (prefs.crtByMode[0], 0, nullptr, 1920.0f, 1080.0f);

        // MakeCrtParams resolves what the effects are worth, not how big a
        // picture they land on; CrtPostProcess::Process fills the scale in
        // from the fitted rect. Unity here means a radius still counts
        // target texels for anyone who never sets it.
        Assert::AreEqual (1.0f, params.pixelScaleX);
        Assert::AreEqual (1.0f, params.pixelScaleY);
    }


    TEST_METHOD (ComputeCrtPixelScale_CountsTargetTexelsPerEmulatedPixel)
    {
        RECT           oneToOne = { 0,   0,   560,  384 };
        RECT           doubled  = { 0,   0,   1120, 768 };
        RECT           offset   = { 200, 100, 760,  484 };
        CrtPixelScale  scale    = {};



        // The emulated framebuffer drawn at its own size: one emulated
        // pixel IS one target texel, which is the reference the shipped
        // radii are numbered against.
        scale = ComputeCrtPixelScale (oneToOne, 560, 384);
        Assert::AreEqual (1.0f, scale.x);
        Assert::AreEqual (1.0f, scale.y);

        scale = ComputeCrtPixelScale (doubled, 560, 384);
        Assert::AreEqual (2.0f, scale.x);
        Assert::AreEqual (2.0f, scale.y);

        // Only the rect's SIZE matters. The desk scene hands the chain a
        // rect anchored at the texture origin and the flat path hands it a
        // letterboxed one, and the same picture must scale the same way.
        scale = ComputeCrtPixelScale (offset, 560, 384);
        Assert::AreEqual (1.0f, scale.x);
        Assert::AreEqual (1.0f, scale.y);
    }


    TEST_METHOD (ComputeCrtPixelScale_MeasuredDeskAndFlatTargetsDisagree)
    {
        RECT           deskSmall = { 0, 0, 371,  255 };
        RECT           deskWide  = { 0, 0, 705,  484 };
        RECT           flat      = { 0, 0, 1000, 685 };
        CrtPixelScale  scale     = {};



        // Three fitted rects measured off the running app at 144 dpi: the
        // desk scene in a small window, the desk scene at the default
        // window, and the flat path in that same small window. They span
        // nearly a factor of three, which is exactly how much the bloom
        // used to change for settings the user never touched.
        scale = ComputeCrtPixelScale (deskSmall, 560, 384);
        Assert::AreEqual (0.6625f, scale.x, 0.0001f);

        scale = ComputeCrtPixelScale (deskWide, 560, 384);
        Assert::AreEqual (1.2589f, scale.x, 0.0001f);

        scale = ComputeCrtPixelScale (flat, 560, 384);
        Assert::AreEqual (1.7857f, scale.x, 0.0001f);
    }


    TEST_METHOD (ComputeCrtPixelScale_KeepsARadiusOnTheSameShareOfThePicture)
    {
        constexpr float  kRadiusEmulatedPx = 3.0f;

        RECT           tightRect = { 0, 0, 371,  255 };
        RECT           wideRect  = { 0, 0, 1000, 685 };
        CrtPixelScale  tightSc   = ComputeCrtPixelScale (tightRect, 560, 384);
        CrtPixelScale  wideSc    = ComputeCrtPixelScale (wideRect, 560, 384);
        float          tightSh   = 0.0f;
        float          wideSh    = 0.0f;



        // The whole point of the change, stated as the property it buys:
        // the blur's reach, divided by the width of the picture it lands
        // on, is the same number at both sizes. Before the scale existed
        // the numerator was constant instead, so this ratio moved with the
        // window.
        tightSh = (kRadiusEmulatedPx * tightSc.x) / (float) (tightRect.right - tightRect.left);
        wideSh  = (kRadiusEmulatedPx * wideSc.x)  / (float) (wideRect.right  - wideRect.left);

        Assert::AreEqual (tightSh, wideSh, 0.00001f);
        Assert::AreEqual (kRadiusEmulatedPx / 560.0f, tightSh, 0.00001f);
    }


    TEST_METHOD (ComputeCrtPixelScale_DegenerateInputsYieldUnity)
    {
        RECT           empty  = {};
        RECT           fitted = { 0, 0, 1120, 768 };
        CrtPixelScale  scale  = {};



        // A frame that is about to draw nothing must not divide by zero,
        // and must not hand the shaders a zero step either -- unity leaves
        // the kernels exactly where they were before the scale existed.
        scale = ComputeCrtPixelScale (empty, 560, 384);
        Assert::AreEqual (1.0f, scale.x);
        Assert::AreEqual (1.0f, scale.y);

        scale = ComputeCrtPixelScale (fitted, 0, 0);
        Assert::AreEqual (1.0f, scale.x);
        Assert::AreEqual (1.0f, scale.y);

        scale = ComputeCrtPixelScale (fitted, -560, -384);
        Assert::AreEqual (1.0f, scale.x);
        Assert::AreEqual (1.0f, scale.y);
    }


    TEST_METHOD (ComputeCrtPixelScale_TracksTheAspectFitTheRendererUses)
    {
        RECT           content = { 0, 0, 1000, 845 };
        RECT           fitted  = ComputeAspectFitRectInRect (content, 560, 384);
        CrtPixelScale  scale   = ComputeCrtPixelScale (fitted, 560, 384);



        // The renderer feeds this the rect ComputeAspectFitRectInRect
        // produced, so the two have to agree. The fit rounds to whole
        // pixels, which is why both axes are carried rather than one.
        Assert::AreEqual (1000L, (long) (fitted.right - fitted.left));
        Assert::AreEqual (685L,  (long) (fitted.bottom - fitted.top));
        Assert::AreEqual (1000.0f / 560.0f, scale.x, 0.00001f);
        Assert::AreEqual (685.0f  / 384.0f, scale.y, 0.00001f);
        Assert::AreNotEqual (scale.x, scale.y);
    }


    TEST_METHOD (MakeCrtParams_LeavesPictureUvSpanningTheWholeTarget)
    {
        GlobalUserPrefs  prefs;

        CrtParams  params = MakeCrtParams (prefs.crtByMode[0], 0, nullptr, 1920.0f, 1080.0f);

        // Same reasoning as the pixel scale: geometry is settled by
        // CrtPostProcess::Process, not by the resolution rules. The full
        // span is what every pass assumed before the rect existed.
        Assert::AreEqual (0.0f, params.pictureV0);
        Assert::AreEqual (1.0f, params.pictureV1);
    }


    TEST_METHOD (ComputeCrtPictureUvRect_LocatesALetterboxedPicture)
    {
        RECT       fitted = { 241, 249, 1258, 947 };
        CrtUvRect  uv     = ComputeCrtPictureUvRect (fitted, 1500, 1195);
        float      span   = uv.v1 - uv.v0;



        // The flat path hands the chain the whole back buffer with the
        // picture letterboxed inside it. The rect is an INPUT rather than
        // something this derives, because D3DRenderer fits the picture
        // into the viewport bounds the chrome leaves free, not into the
        // back buffer. These numbers came off a 1500 px window at 144 dpi.
        Assert::AreEqual (1017L, (long) (fitted.right - fitted.left));
        Assert::AreEqual (698L,  (long) (fitted.bottom - fitted.top));
        Assert::AreEqual (249.0f / 1195.0f, uv.v0, 0.00001f);
        Assert::AreEqual (947.0f / 1195.0f, uv.v1, 0.00001f);
        Assert::AreEqual (698.0f / 1195.0f, span,  0.00001f);
    }


    TEST_METHOD (ComputeCrtPictureUvRect_SpansEverythingWhenTargetIsThePicture)
    {
        RECT       fitted = { 0, 0, 705, 484 };
        CrtUvRect  uv     = ComputeCrtPictureUvRect (fitted, 706, 484);



        // The desk scene renders at picture size, so the picture is very
        // nearly the whole target and the scanline pass keeps the behavior
        // it had before this rect existed. That is why the desk scene was
        // the one path already drawing 192 lines.
        Assert::AreEqual (0.0f, uv.v0);
        Assert::AreEqual (1.0f, uv.v1);
    }


    TEST_METHOD (ComputeCrtPictureUvRect_DegenerateInputsYieldTheWholeTarget)
    {
        RECT       empty   = {};
        RECT       flat    = { 0, 100, 800, 100 };
        RECT       fitted  = { 0, 0, 705, 484 };
        CrtUvRect  uv      = {};



        // A frame with no picture must not hand the scanline pass a
        // zero-height span to divide by. The whole target is the safe
        // answer, and it is what the shader used before.
        uv = ComputeCrtPictureUvRect (empty, 800, 600);
        Assert::AreEqual (0.0f, uv.v0);
        Assert::AreEqual (1.0f, uv.v1);

        uv = ComputeCrtPictureUvRect (flat, 800, 600);
        Assert::AreEqual (0.0f, uv.v0);
        Assert::AreEqual (1.0f, uv.v1);

        uv = ComputeCrtPictureUvRect (fitted, 0, 0);
        Assert::AreEqual (0.0f, uv.v0);
        Assert::AreEqual (1.0f, uv.v1);
    }


    TEST_METHOD (ComputeCrtPictureUvRect_SpansTheDefectTheScanlinePassHad)
    {
        constexpr float  kNativeScanlines = 192.0f;

        RECT       tightFit  = { 284, 150, 615,  377 };
        RECT       wideFit   = { 241, 249, 1258, 947 };
        CrtUvRect  tightUv   = ComputeCrtPictureUvRect (tightFit, 900, 745);
        CrtUvRect  wideUv    = ComputeCrtPictureUvRect (wideFit, 1500, 1195);
        float      tightSpan = tightUv.v1 - tightUv.v0;
        float      wideSpan  = wideUv.v1 - wideUv.v0;



        // Two window sizes measured off the running app, 331x227 of a
        // 900x745 target and 1017x698 of a 1500x1195 one. The picture
        // occupies a very different share of the target in each, which is
        // the whole reason the scanline pass could not be handed the
        // target and left to divide it into 192.
        Assert::AreEqual (0.3047f, tightSpan, 0.001f);
        Assert::AreEqual (0.5841f, wideSpan,  0.001f);

        // The old kernel used the target's own uv, so the count of cycles
        // that actually landed on the picture was 192 scaled by that
        // share. Those two numbers are what a frame capture showed: 59
        // scanlines in the small window and 112 in the large one.
        Assert::AreEqual (58.5f,  kNativeScanlines * tightSpan, 0.5f);
        Assert::AreEqual (112.0f, kNativeScanlines * wideSpan,  0.5f);

        // Dividing by the span is what removes the window from the
        // answer. The shader's linePos runs 0..192 across the picture at
        // both sizes because the span cancels, which is exactly the term
        // this function exists to supply.
        Assert::AreNotEqual (tightSpan, wideSpan);
        Assert::IsTrue (tightSpan > 0.0f);
        Assert::IsTrue (wideSpan  > 0.0f);
    }


    TEST_METHOD (ComputeLetterboxRect_HandlesPillarboxAndLetterbox)
    {
        RECT  lb = {};
        RECT  ex = {};



        // Wide window -> pillarbox (vertical black bars on left + right).
        RECT  pb = ComputeLetterboxRect (1600, 900);
        Assert::AreEqual (1200L, (long) (pb.right - pb.left));
        Assert::AreEqual (900L,  (long) (pb.bottom - pb.top));
        Assert::AreEqual (200L,  (long) pb.left);
        Assert::AreEqual (0L,    (long) pb.top);

        // Tall window -> letterbox (horizontal bars on top + bottom).
        lb = ComputeLetterboxRect (800, 800);
        Assert::AreEqual (800L, (long) (lb.right - lb.left));
        Assert::AreEqual (600L, (long) (lb.bottom - lb.top));
        Assert::AreEqual (0L,   (long) lb.left);
        Assert::AreEqual (100L, (long) lb.top);

        // Exact 4:3 -> full window, no bars.
        ex = ComputeLetterboxRect (1024, 768);
        Assert::AreEqual (0L,    (long) ex.left);
        Assert::AreEqual (0L,    (long) ex.top);
        Assert::AreEqual (1024L, (long) ex.right);
        Assert::AreEqual (768L,  (long) ex.bottom);
    }
};
