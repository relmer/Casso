#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Ui/DiskInspector/PlatterRenderer.h"
#include "WarpRenderHarness.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterRendererTests
//
//  The platter drawn through Dxui's custom draw (R19): it covers the fills
//  painted before it, the fills painted after it cover it, each ring shows
//  its own cells by angle clockwise from 12 o'clock, and a pending ring shows
//  its pattern across its rings. The last test times a zoom from fit to 600x and back on the
//  hardware device for SC-004 and logs it; it asserts only where a hardware
//  device exists.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PlatterRendererTests)
{
public:

    static constexpr int       kSize         = 512;
    static constexpr uint32_t  kBackground   = 0xFF102030;
    static constexpr uint32_t  kMarker       = 0xFFFF00FF;
    static constexpr uint32_t  kCells        = 51200;
    static constexpr int       kPendingRing  = 40;
    static constexpr int       kPendingRings = 8;



    //  Every cell sync, except the first quarter of the turn, which is data.
    static std::shared_ptr<const PlatterRenderer::Levels> MakeQuarterData()
    {
        vector<Byte>                              cells (kCells, PlatterCells::Encode (PlatterKind::Sync));
        std::shared_ptr<PlatterRenderer::Levels>  levels = std::make_shared<PlatterRenderer::Levels>();



        std::fill (cells.begin(), cells.begin() + kCells / 4, PlatterCells::Encode (PlatterKind::DataField));
        PlatterCells::BuildLevels (cells, *levels);

        return levels;
    }



    static void FillRings (PlatterRenderer & renderer, std::shared_ptr<const PlatterRenderer::Levels> levels)
    {
        int  ring = 0;



        for (ring = 0; ring < PlatterRenderer::kRingCount; ring++)
        {
            renderer.SetRing (ring, ring >= kPendingRing && ring < kPendingRing + kPendingRings ? PlatterRingState::Pending : PlatterRingState::Data, levels);
        }
    }



    //  The pixel at the given angle, in degrees clockwise from 12 o'clock, and
    //  radius from the center.
    static uint32_t PixelAt (const vector<uint32_t> & pixels, float degrees, float radius)
    {
        float  a = degrees * 3.14159265f / 180.0f;
        int    x = static_cast<int> (kSize / 2 + radius * std::sin (a));
        int    y = static_cast<int> (kSize / 2 - radius * std::cos (a));



        return pixels[static_cast<size_t> (y) * kSize + x] | 0xFF000000;
    }



    TEST_METHOD (ThePlatterDrawsBetweenTheFillsBeforeAndAfterIt)
    {
        WarpRenderHarness                 harness;
        ComPtr<ID3D11Texture2D>           texture;
        ComPtr<ID3D11RenderTargetView>    rtv;
        DxuiPainter                       painter;
        PlatterRenderer                   renderer;
        PlatterView                       view;
        DiskInspectorPalette              palette  = DiskInspectorPalette::MakeFallback (true);
        vector<uint32_t>                  pixels;
        RECT                              rect     = { 0, 0, kSize, kSize };
        HRESULT                           drawn    = E_FAIL;
        float                             outer    = kSize / 2.0f - 6.0f;
        float                             ringPx   = (1.0f - PlatterRenderer::kInnerFraction) * outer / PlatterRenderer::kRingCount;



        if (!harness.IsAvailable())
        {
            Logger::WriteMessage (L"No WARP device; skipped");
            return;
        }

        DxuiResetUiThreadIdForTest();

        AssertSucceeded (harness.MakeTarget (kSize, kSize, texture, rtv));
        AssertSucceeded (painter.Initialize (harness.GetDevice(), harness.GetContext()));
        AssertSucceeded (renderer.Initialize (harness.GetDevice()));

        renderer.SetPalette (palette);
        FillRings (renderer, MakeQuarterData());

        view.centerXPx     = kSize / 2.0f;
        view.centerYPx     = kSize / 2.0f;
        view.outerRadiusPx = outer;

        AssertSucceeded (painter.Begin (kSize, kSize, rtv.Get()));
        painter.FillRect   (0, 0, kSize, kSize, kBackground);
        painter.DrawCustom (rect, [&] (const DxuiCustomDrawArgs & args) { drawn = renderer.Render (args, view); });
        painter.FillRect   (kSize / 2.0f - 4, kSize / 2.0f - 4, 8, 8, kMarker);
        AssertSucceeded (painter.End (rtv.Get()));

        AssertSucceeded (drawn);
        AssertSucceeded (harness.ReadBack (texture.Get(), pixels));

        Assert::AreEqual (kBackground, pixels[0] | 0xFF000000, L"outside the disk the fill before shows");
        Assert::AreEqual (kMarker, PixelAt (pixels, 0, 0), L"the fill after covers the hub");
        Assert::AreEqual (palette.GetKindColor (PlatterKind::DataField), PixelAt (pixels, 45,  outer - 20.5f * ringPx), L"the first quarter turn is data");
        Assert::AreEqual (palette.GetKindColor (PlatterKind::Sync),      PixelAt (pixels, 200, outer - 20.5f * ringPx), L"the rest is sync");
    }



    TEST_METHOD (APendingRingShowsItsPattern)
    {
        WarpRenderHarness                 harness;
        ComPtr<ID3D11Texture2D>           texture;
        ComPtr<ID3D11RenderTargetView>    rtv;
        DxuiPainter                       painter;
        PlatterRenderer                   renderer;
        PlatterView                       view;
        DiskInspectorPalette              palette  = DiskInspectorPalette::MakeFallback (true);
        vector<uint32_t>                  pixels;
        RECT                              rect     = { 0, 0, kSize, kSize };
        std::set<uint32_t>                seen;
        float                             outer    = kSize / 2.0f - 6.0f;
        float                             ringPx   = (1.0f - PlatterRenderer::kInnerFraction) * outer / PlatterRenderer::kRingCount;
        float                             degrees  = 0;



        if (!harness.IsAvailable())
        {
            return;
        }

        DxuiResetUiThreadIdForTest();

        AssertSucceeded (harness.MakeTarget (kSize, kSize, texture, rtv));
        AssertSucceeded (painter.Initialize (harness.GetDevice(), harness.GetContext()));
        AssertSucceeded (renderer.Initialize (harness.GetDevice()));

        renderer.SetPalette (palette);
        FillRings (renderer, MakeQuarterData());

        view.centerXPx     = kSize / 2.0f;
        view.centerYPx     = kSize / 2.0f;
        view.outerRadiusPx = outer;

        AssertSucceeded (painter.Begin (kSize, kSize, rtv.Get()));
        painter.DrawCustom (rect, [&] (const DxuiCustomDrawArgs & args) { (void) renderer.Render (args, view); });
        AssertSucceeded (painter.End (rtv.Get()));
        AssertSucceeded (harness.ReadBack (texture.Get(), pixels));

        for (degrees = 0; degrees < 360; degrees += 1)
        {
            seen.insert (PixelAt (pixels, degrees, outer - (kPendingRing + kPendingRings / 2.0f) * ringPx));
        }

        Assert::IsTrue (seen.contains (palette.colors.pendingPattern | 0xFF000000));
        Assert::IsTrue (seen.contains (palette.colors.nothingRecorded | 0xFF000000));
        Assert::IsFalse (seen.contains (palette.GetKindColor (PlatterKind::DataField)), L"a pending ring shows no cells");
    }



    //  SC-004 on this machine's GPU: 160 rings of 51,200 cells each, none
    //  shared, drawn at 1400 pixels across from fit to 600x about a point on
    //  track 0 and back, timed by GPU timestamps.
    TEST_METHOD (ZoomFromFitTo600xStaysWithinAFrame)
    {
        static constexpr int    kTarget    = 1400;
        static constexpr int    kSteps     = 120;
        static constexpr double kMaxZoom   = 600.0;
        static constexpr double kRefreshMs = 1000.0 / 60.0;

        ComPtr<ID3D11Device>              device;
        ComPtr<ID3D11DeviceContext>       context;
        ComPtr<ID3D11Texture2D>           texture;
        ComPtr<ID3D11RenderTargetView>    rtv;
        ComPtr<ID3D11Query>               disjoint;
        PlatterRenderer                   renderer;
        PlatterView                       view;
        D3D11_TEXTURE2D_DESC              desc     = {};
        D3D11_QUERY_DESC                  query    = {};
        DxuiCustomDrawArgs                args;
        vector<double>                    times;
        int                               step     = 0;
        int                               ring     = 0;
        HRESULT                           created  = E_FAIL;



        created = D3D11CreateDevice (nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);

        if (FAILED (created))
        {
            Logger::WriteMessage (L"No hardware device; SC-004 not measured");
            return;
        }

        desc.Width            = kTarget;
        desc.Height           = kTarget;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags        = D3D11_BIND_RENDER_TARGET;

        AssertSucceeded (device->CreateTexture2D (&desc, nullptr, &texture));
        AssertSucceeded (device->CreateRenderTargetView (texture.Get(), nullptr, &rtv));
        AssertSucceeded (renderer.Initialize (device.Get()));

        for (ring = 0; ring < PlatterRenderer::kRingCount; ring++)
        {
            renderer.SetRing (ring, PlatterRingState::Data, MakeQuarterData());
        }

        args.device         = device.Get();
        args.context        = context.Get();
        args.target         = rtv.Get();
        args.targetWidthPx  = kTarget;
        args.targetHeightPx = kTarget;
        args.rectPx         = { 0, 0, kTarget, kTarget };
        args.clipPx         = args.rectPx;

        query.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        AssertSucceeded (device->CreateQuery (&query, &disjoint));

        for (step = 0; step <= 2 * kSteps; step++)
        {
            ComPtr<ID3D11Query>                  begin;
            ComPtr<ID3D11Query>                  end;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT  freq    = {};
            UINT64                               t0      = 0;
            UINT64                               t1      = 0;
            int                                  k       = step <= kSteps ? step : 2 * kSteps - step;
            double                               zoom    = std::pow (kMaxZoom, static_cast<double> (k) / kSteps);
            float                                fit     = kTarget / 2.0f;

            query.Query = D3D11_QUERY_TIMESTAMP;
            AssertSucceeded (device->CreateQuery (&query, &begin));
            AssertSucceeded (device->CreateQuery (&query, &end));

            //  Zoom about the point on track 0 at 12 o'clock.
            view.outerRadiusPx = static_cast<float> (fit * zoom);
            view.centerXPx     = fit;
            view.centerYPx     = static_cast<float> (fit - fit * 0.98 + fit * 0.98 * zoom);

            context->Begin (disjoint.Get());
            context->End   (begin.Get());
            AssertSucceeded (renderer.Render (args, view));
            context->End   (end.Get());
            context->End   (disjoint.Get());

            while (context->GetData (disjoint.Get(), &freq, sizeof (freq), 0) == S_FALSE) {}
            while (context->GetData (begin.Get(), &t0, sizeof (t0), 0) == S_FALSE) {}
            while (context->GetData (end.Get(), &t1, sizeof (t1), 0) == S_FALSE) {}

            if (!freq.Disjoint && step > 0)
            {
                times.push_back (1000.0 * static_cast<double> (t1 - t0) / static_cast<double> (freq.Frequency));
            }
        }

        std::sort (times.begin(), times.end());
        Logger::WriteMessage (std::format (L"platter GPU ms: median {:.3f}, max {:.3f} over {} frames", times[times.size() / 2], times.back(), times.size()).c_str());

        Assert::IsTrue (times[times.size() / 2] <= kRefreshMs);
        Assert::IsTrue (times.back() <= 33.0);
    }
};
