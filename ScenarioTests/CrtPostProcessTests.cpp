#include "Pch.h"

#include "GoldenImage.h"
#include "WarpRenderHarness.h"

#include "CrtPostProcess.h"
#include "HResultAssert.h"

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
//  This file has the golden comparison, which writes the rendered image to the
//  system temp folder on a mismatch. The rest are in the unit suite's
//  CrtPostProcessTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CrtPostProcessTests)
{
public:

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


private:

    static constexpr int  s_kSourceW = 560;
    static constexpr int  s_kSourceH = 384;
    static constexpr int  s_kTargetW = 280;
    static constexpr int  s_kTargetH = 192;


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


};
