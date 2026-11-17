#include "Pch.h"

#include "../EmuTests/WarpRenderHarness.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLayerFlushTests
//
//  IDxuiPainter::FlushLayer on a WARP device, read back. A pass is set up the
//  way DxuiHwndSource::PaintContent sets one up: fills are batched in the
//  painter, text goes into a command list, and the painter's text flush is
//  DxuiTextRenderer::FlushDeferred. The text layer is drawn with
//  IDxuiTextRenderer::FillRect, which goes into the same command list as a
//  string and has no glyph edges to compare.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiLayerFlushTests)
{
public:

    static constexpr int       kSize  = 64;
    static constexpr uint32_t  kClear = 0xFF000000;
    static constexpr uint32_t  kText  = 0xFFFF0000;
    static constexpr uint32_t  kFill  = 0xFF0000FF;
    static constexpr uint32_t  kCover = 0xFF00FF00;



    TEST_METHOD (WithoutAFlushTextIsDrawnOverLaterShapes)
    {
        Pass  pass;



        Begin (pass);

        pass.text.FillRect (0, 0, 32, 32, kText);
        pass.painter.FillRect (0, 0, 32, 32, kCover);

        End (pass);

        Assert::AreEqual (kText, GetPixel (pass, 16, 16), L"all text is drawn over all shapes");
    }



    TEST_METHOD (ShapesAfterAFlushCoverTextBeforeIt)
    {
        Pass  pass;



        Begin (pass);

        pass.painter.FillRect (0, 0, 32, 32, kFill);
        pass.text.FillRect    (0, 0, 32, 32, kText);
        pass.painter.FlushLayer();
        pass.painter.FillRect (0, 0, 16, 32, kCover);

        End (pass);

        Assert::AreEqual (kCover, GetPixel (pass,  8, 16), L"the shape after the flush covers the text");
        Assert::AreEqual (kText,  GetPixel (pass, 24, 16), L"the text before the flush covers the shape before it");
    }



    TEST_METHOD (TextAfterAFlushIsDrawnOverEverythingBeforeIt)
    {
        Pass  pass;



        Begin (pass);

        pass.text.FillRect (0, 0, 32, 32, kFill);
        pass.painter.FlushLayer();
        pass.painter.FillRect (0, 0, 32, 32, kCover);
        pass.text.FillRect    (0, 0, 16, 32, kText);

        End (pass);

        Assert::AreEqual (kText,  GetPixel (pass,  8, 16), L"the text after the flush is on top");
        Assert::AreEqual (kCover, GetPixel (pass, 24, 16), L"the shape after the flush covers the text before it");
    }



    TEST_METHOD (AClipPushedBeforeAFlushStillClipsAfterIt)
    {
        Pass  pass;



        Begin (pass);

        AssertSucceeded (pass.text.PushClipRect (0, 0, 16, 64));
        pass.painter.FlushLayer();
        pass.text.FillRect (0, 0, 64, 64, kText);
        AssertSucceeded (pass.text.PopClipRect());

        End (pass);

        Assert::AreEqual (kText,  GetPixel (pass,  8, 8), L"inside the clip");
        Assert::AreEqual (kClear, GetPixel (pass, 40, 8), L"outside the clip");
    }



    TEST_METHOD (TheOriginSetBeforeAFlushStillAppliesAfterIt)
    {
        Pass  pass;



        Begin (pass);

        pass.text.SetOrigin (32, 32);
        pass.text.FillRect (0, 0, 8, 8, kFill);
        pass.painter.FlushLayer();
        pass.text.FillRect (8, 0, 8, 8, kText);
        pass.text.SetOrigin (0, 0);

        End (pass);

        Assert::AreEqual (kFill,  GetPixel (pass, 36, 36), L"the text before the flush is replayed at its origin, not offset again");
        Assert::AreEqual (kText,  GetPixel (pass, 44, 36), L"the text after the flush is drawn at the origin");
        Assert::AreEqual (kClear, GetPixel (pass,  4,  4), L"nothing at the window origin");
    }



    TEST_METHOD (AFlushOutsideAPassDrawsNothing)
    {
        Pass  pass;



        Prepare (pass);

        pass.painter.FlushLayer();
        AssertSucceeded (pass.text.FlushDeferred());

        AssertSucceeded (pass.harness.ReadBack (pass.texture.Get(), pass.pixels));
        Assert::AreEqual (kClear, GetPixel (pass, 8, 8));
    }



private:

    struct Pass
    {
        WarpRenderHarness               harness { D3D11_CREATE_DEVICE_BGRA_SUPPORT };
        ComPtr<ID3D11Texture2D>         texture;
        ComPtr<ID3D11RenderTargetView>  rtv;
        DxuiPainter                     painter;
        DxuiTextRenderer                text;
        std::vector<uint32_t>           pixels;
    };



    //  A target cleared to kClear, a painter and a text renderer bound to it,
    //  and the painter's text flush. WARP ships with Windows, so a missing
    //  device fails the test rather than skipping it.
    static void Prepare (Pass & pass)
    {
        ComPtr<IDXGISurface>  surface;
        const float           clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };



        Assert::IsTrue (pass.harness.IsAvailable(), L"no WARP device with BGRA support");

        DxuiResetUiThreadIdForTest();

        AssertSucceeded (pass.harness.MakeTarget (kSize, kSize, pass.texture, pass.rtv));
        AssertSucceeded (pass.texture.As (&surface));
        AssertSucceeded (pass.painter.Initialize (pass.harness.GetDevice(), pass.harness.GetContext()));
        AssertSucceeded (pass.text.Initialize (pass.harness.GetDevice()));
        AssertSucceeded (pass.text.BindBackBuffer (surface.Get(), USER_DEFAULT_SCREEN_DPI, USER_DEFAULT_SCREEN_DPI));

        pass.harness.GetContext()->ClearRenderTargetView (pass.rtv.Get(), clear);

        pass.painter.SetTextFlush ([&pass]() { return pass.text.FlushDeferred(); });
    }



    static void Begin (Pass & pass)
    {
        Prepare (pass);

        AssertSucceeded (pass.painter.Begin (kSize, kSize, pass.rtv.Get()));
        AssertSucceeded (pass.text.BeginDrawDeferred());
    }



    //  Fills first, then text, as DxuiHwndSource::PaintContent ends a pass.
    static void End (Pass & pass)
    {
        AssertSucceeded (pass.painter.End (pass.rtv.Get()));
        AssertSucceeded (pass.text.EndDrawDeferred());
        AssertSucceeded (pass.harness.ReadBack (pass.texture.Get(), pass.pixels));
    }



    static uint32_t GetPixel (const Pass & pass, int x, int y)
    {
        return pass.pixels[(size_t) y * kSize + (size_t) x];
    }
};
