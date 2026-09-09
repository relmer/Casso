#include "Pch.h"

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
//  documents. A checked-in golden is the stronger assertion and is left for a
//  person to bless, since blessing one means looking at it.
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
        HRESULT                           hr         = S_OK;
        WarpRenderHarness                 harness;
        CrtPostProcess                    chain;
        CrtParams                         adjusted   = params;
        std::vector<uint32_t>             source ((size_t) s_kSourceW * s_kSourceH, sourceColor);
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
