#include "Pch.h"

#include "GoldenImage.h"
#include "WarpRenderHarness.h"

#include "CrtPostProcess.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CrtPostProcessTests
//
//  The CRT chain, rendered and read back in a unit test.
//
//  Before this, changing a post-process pass meant taking a screenshot and
//  squinting at it. The chain touches Direct3D, which is exactly why it was
//  the code most likely to be waved through as untestable -- but a WARP device
//  needs no window and no display, and what it draws can be read back and
//  asserted like any other output.
//
//  These pin behavior that is true by construction rather than by blessing an
//  image: that the chain is deterministic, that it fills the target it is
//  given, and that a parameter moves the picture in the direction it
//  documents. The checked-in golden is the stronger assertion: a test pattern
//  through the whole chain, compared pixel for pixel with no tolerance, so
//  any change to any pass is a change someone looks at before it ships.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CrtPostProcessTests)
{
public:

    TEST_METHOD (AWarpDeviceIsAvailableToRenderThrough)
    {
        WarpRenderHarness  harness;

        Assert::IsTrue (harness.IsAvailable(),
                        L"WARP is part of the Windows SDK; without it the rest cannot run");
    }


    TEST_METHOD (TheChainRendersAndTheResultCanBeReadBack)
    {
        std::vector<uint32_t>  output;
        HRESULT                hr = S_OK;

        hr = RenderFlatField (0xFF4080C0, CrtParams(), output);

        AssertSucceeded (hr, L"the chain must run on a software adapter");
        Assert::AreEqual ((size_t) (s_kTargetW * s_kTargetH), output.size(),
                          L"read-back must cover the whole target");
    }


    TEST_METHOD (TheSameInputRenderedTwice_ProducesIdenticalPixels)
    {
        std::vector<uint32_t>  first;
        std::vector<uint32_t>  second;
        HRESULT                hr = S_OK;

        //  Determinism is what makes a golden possible at all, so it is the
        //  first thing worth knowing.
        hr = RenderFlatField (0xFF204060, CrtParams(), first);
        AssertSucceeded (hr);

        hr = RenderFlatField (0xFF204060, CrtParams(), second);
        AssertSucceeded (hr);

        Assert::IsTrue (first == second, L"two runs of the same input must agree exactly");
    }


    TEST_METHOD (Brightness_MovesThePictureInTheDirectionItDocuments)
    {
        std::vector<uint32_t>  dim;
        std::vector<uint32_t>  bright;
        CrtParams              dimParams;
        CrtParams              brightParams;
        HRESULT                hr = S_OK;

        dimParams.brightness    = 0.5f;
        brightParams.brightness = 1.5f;

        hr = RenderFlatField (0xFF808080, dimParams, dim);
        AssertSucceeded (hr);

        hr = RenderFlatField (0xFF808080, brightParams, bright);
        AssertSucceeded (hr);

        Assert::IsTrue (TotalLuminance (bright) > TotalLuminance (dim),
                        L"raising brightness must not darken the picture");
    }


    TEST_METHOD (Scanlines_DarkenThePictureAsTheirIntensityRises)
    {
        std::vector<uint32_t>  off;
        std::vector<uint32_t>  full;
        CrtParams              offParams;
        CrtParams              fullParams;
        HRESULT                hr = S_OK;

        //  A scanline is a darkened row between lit ones. More intensity is
        //  darker rows, and the picture as a whole loses light -- it cannot
        //  gain any, because the pass only ever multiplies by at most one.
        offParams.scanlineIntensity  = 0.0f;
        fullParams.scanlineIntensity = 1.0f;

        hr = RenderFlatField (0xFFC0C0C0, offParams, off);
        AssertSucceeded (hr);

        hr = RenderFlatField (0xFFC0C0C0, fullParams, full);
        AssertSucceeded (hr);

        Assert::IsTrue (TotalLuminance (full) < TotalLuminance (off),
                        L"scanlines at full intensity must take light out of the picture");
    }


    TEST_METHOD (Gamma_AboveOneLiftsTheMidtones)
    {
        std::vector<uint32_t>  linear;
        std::vector<uint32_t>  lifted;
        CrtParams              linearParams;
        CrtParams              liftedParams;
        HRESULT                hr = S_OK;

        //  The pass raises each channel to 1/gamma. A gamma above one is a
        //  fractional exponent, which moves a midtone toward white; a value
        //  of one is the documented bypass and leaves it where it was.
        linearParams.gamma = 1.0f;
        liftedParams.gamma = 2.0f;

        hr = RenderFlatField (0xFF404040, linearParams, linear);
        AssertSucceeded (hr);

        hr = RenderFlatField (0xFF404040, liftedParams, lifted);
        AssertSucceeded (hr);

        Assert::IsTrue (TotalLuminance (lifted) > TotalLuminance (linear),
                        L"a gamma above one must brighten a midtone");
    }


    TEST_METHOD (Contrast_AboveOnePushesADarkToneDarker)
    {
        std::vector<uint32_t>  flat;
        std::vector<uint32_t>  pushed;
        CrtParams              flatParams;
        CrtParams              pushedParams;
        HRESULT                hr = S_OK;

        //  Contrast pivots about mid-gray: what is below it goes down as the
        //  setting goes up. A tone well under the pivot is the unambiguous
        //  case -- a midtone right at it would not move at all.
        flatParams.contrast   = 1.0f;
        pushedParams.contrast = 1.5f;

        hr = RenderFlatField (0xFF202020, flatParams, flat);
        AssertSucceeded (hr);

        hr = RenderFlatField (0xFF202020, pushedParams, pushed);
        AssertSucceeded (hr);

        Assert::IsTrue (TotalLuminance (pushed) < TotalLuminance (flat),
                        L"raising contrast must push a dark tone darker");
    }


    TEST_METHOD (ATestPatternThroughTheWholeChain_MatchesItsGolden)
    {
        std::vector<uint32_t>  output;
        CrtParams              params;
        HRESULT                hr = S_OK;

        //  Every pass doing something, so a regression in any of them lands
        //  in the picture. The values are a plausible monitor rather than
        //  extremes, because extremes saturate and hide each other.
        params.brightness        = 1.1f;
        params.contrast          = 1.1f;
        params.gamma             = 1.2f;
        params.scanlineIntensity = 0.35f;
        params.bloomRadius       = 2.0f;
        params.bloomStrength     = 0.4f;
        params.bloomThreshold    = 0.3f;
        params.colorBleedWidth   = 2.0f;

        hr = RenderPattern (params, output);
        AssertSucceeded (hr, L"the chain must run on a software adapter");

        GoldenImage::AssertMatches (output, s_kTargetW, s_kTargetH, L"CrtChain_TestPattern");
    }


    TEST_METHOD (ADifferentSourceColor_ProducesADifferentPicture)
    {
        std::vector<uint32_t>  red;
        std::vector<uint32_t>  blue;
        HRESULT                hr = S_OK;

        //  Guards against a chain that renders something constant: every
        //  assertion above would still pass if the output ignored its input.
        hr = RenderFlatField (0xFFFF0000, CrtParams(), red);
        AssertSucceeded (hr);

        hr = RenderFlatField (0xFF0000FF, CrtParams(), blue);
        AssertSucceeded (hr);

        Assert::IsFalse (red == blue, L"the output must depend on the input");
    }


private:

    static constexpr int  s_kSourceW = 560;
    static constexpr int  s_kSourceH = 384;
    static constexpr int  s_kTargetW = 280;
    static constexpr int  s_kTargetH = 192;


    //
    //  Runs one synthetic frame of a single color through the whole chain and
    //  hands back the read-back target.
    //
    static HRESULT RenderFlatField (uint32_t                 sourceColor,
                                    const CrtParams        & params,
                                    std::vector<uint32_t>  & outPixels)
    {
        std::vector<uint32_t>  source ((size_t) s_kSourceW * s_kSourceH, sourceColor);

        return RenderSource (source, params, outPixels);
    }


    //
    //  The pattern the golden was blessed from: color bars across the top
    //  half, a one-pixel grid below them, and a run of alternating columns
    //  at the bottom that the bleed and bloom passes have something to do
    //  with. Deterministic by construction -- no clock, no random source.
    //
    static HRESULT RenderPattern (const CrtParams        & params,
                                  std::vector<uint32_t>  & outPixels)
    {
        static constexpr uint32_t  kBars[] =
        {
            0xFFFFFFFF, 0xFFFFFF00, 0xFF00FFFF, 0xFF00FF00,
            0xFFFF00FF, 0xFFFF0000, 0xFF0000FF, 0xFF000000,
        };

        std::vector<uint32_t>  source ((size_t) s_kSourceW * s_kSourceH, 0xFF000000);

        for (int y = 0; y < s_kSourceH; y++)
        {
            for (int x = 0; x < s_kSourceW; x++)
            {
                uint32_t  pixel = 0xFF000000;

                if (y < s_kSourceH / 2)
                {
                    pixel = kBars[(x * 8) / s_kSourceW];
                }
                else if (y < (s_kSourceH * 3) / 4)
                {
                    pixel = ((x % 14) == 0 || (y % 8) == 0) ? 0xFF80FF80 : 0xFF000000;
                }
                else
                {
                    pixel = ((x & 1) == 0) ? 0xFFFF8000 : 0xFF0040FF;
                }

                source[(size_t) y * s_kSourceW + x] = pixel;
            }
        }

        return RenderSource (source, params, outPixels);
    }


    //
    //  One synthetic frame through the whole chain, read back from the target.
    //
    static HRESULT RenderSource (const std::vector<uint32_t> & source,
                                 const CrtParams             & params,
                                 std::vector<uint32_t>       & outPixels)
    {
        HRESULT                           hr         = S_OK;
        WarpRenderHarness                 harness;
        CrtPostProcess                    chain;
        CrtParams                         adjusted   = params;
        ComPtr<ID3D11ShaderResourceView>  srv;
        ComPtr<ID3D11Texture2D>           target;
        ComPtr<ID3D11RenderTargetView>    rtv;
        RECT                              viewport   = { 0, 0, s_kTargetW, s_kTargetH };
        bool                              fInit      = false;
        bool                              fAvailable = harness.IsAvailable();

        CBRA (fAvailable);

        hr = harness.MakeSource (s_kSourceW, s_kSourceH, source, srv);
        CHR (hr);

        hr = harness.MakeTarget (s_kTargetW, s_kTargetH, target, rtv);
        CHR (hr);

        hr = chain.Initialize (harness.GetDevice(), harness.GetContext());
        CHR (hr);
        fInit = true;

        adjusted.outputW = (float) s_kTargetW;
        adjusted.outputH = (float) s_kTargetH;

        hr = chain.Process (srv.Get(), rtv.Get(), adjusted, viewport,
                            s_kTargetW, s_kTargetH, s_kSourceW, s_kSourceH);
        CHR (hr);

        hr = harness.ReadBack (target.Get(), outPixels);
        CHR (hr);

    Error:
        if (fInit)
        {
            chain.Shutdown();
        }

        return (hr);
    }


    static double TotalLuminance (const std::vector<uint32_t> & pixels)
    {
        double  total = 0.0;

        for (uint32_t pixel : pixels)
        {
            double  b = (double) ( pixel        & 0xFF);
            double  g = (double) ((pixel >>  8) & 0xFF);
            double  r = (double) ((pixel >> 16) & 0xFF);

            total += (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
        }

        return (total);
    }
};
