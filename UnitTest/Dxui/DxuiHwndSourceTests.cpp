#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace
{
    constexpr LONG   s_kClientWidthDip       = 1024;
    constexpr LONG   s_kClientHeightDip      = 768;
    constexpr float  s_kResizeBorderDip      = 6.0f;

    constexpr LONG   s_kCaptionHeightDip     = 32;
    constexpr LONG   s_kSystemButtonWidthDip = 46;


    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }


    POINT  MakePoint (LONG x, LONG y)
    {
        POINT  out = {};
        out.x = x; out.y = y;
        return out;
    }


    //
    //  Builds a synthetic DxuiHwndSource with a caption bar wired up
    //  with three system buttons (min/max/close) anchored to the
    //  right edge. Returns the host plus the per-button bounds so the
    //  test can hit-test their centers.
    //
    struct SyntheticHost
    {
        std::unique_ptr<DxuiHwndSource>  host;
        RECT                             minRectDip   = {};
        RECT                             maxRectDip   = {};
        RECT                             closeRectDip = {};
        RECT                             captionRectDip = {};
    };


    SyntheticHost  BuildSyntheticHost ()
    {
        SyntheticHost            result;
        std::unique_ptr<DxuiPanel>     root        = std::make_unique<DxuiPanel>();
        DxuiCaptionBar &               caption     = root->Add<DxuiCaptionBar>();
        DxuiSystemButton &             minBtn      = caption.Add<DxuiSystemButton> (DxuiSystemButtonKind::Min);
        DxuiSystemButton &             maxBtn      = caption.Add<DxuiSystemButton> (DxuiSystemButtonKind::Max);
        DxuiSystemButton &             closeBtn    = caption.Add<DxuiSystemButton> (DxuiSystemButtonKind::Close);



        result.captionRectDip = MakeRect (0, 0, s_kClientWidthDip, s_kCaptionHeightDip);
        result.closeRectDip   = MakeRect (s_kClientWidthDip - s_kSystemButtonWidthDip,
                                          0,
                                          s_kClientWidthDip,
                                          s_kCaptionHeightDip);
        result.maxRectDip     = MakeRect (s_kClientWidthDip - 2 * s_kSystemButtonWidthDip,
                                          0,
                                          s_kClientWidthDip - s_kSystemButtonWidthDip,
                                          s_kCaptionHeightDip);
        result.minRectDip     = MakeRect (s_kClientWidthDip - 3 * s_kSystemButtonWidthDip,
                                          0,
                                          s_kClientWidthDip - 2 * s_kSystemButtonWidthDip,
                                          s_kCaptionHeightDip);

        caption.SetBounds  (result.captionRectDip);
        minBtn.SetBounds   (result.minRectDip);
        maxBtn.SetBounds   (result.maxRectDip);
        closeBtn.SetBounds (result.closeRectDip);

        result.host = std::make_unique<DxuiHwndSource> (
            MakeRect (0, 0, s_kClientWidthDip, s_kClientHeightDip),
            s_kResizeBorderDip,
            std::move (root));

        return result;
    }
}





TEST_CLASS (DxuiHwndSourceTests)
{
public:

    TEST_METHOD_INITIALIZE (Setup)
    {
        // Unit tests run on the MSTest worker thread, not a real UI
        // thread. Reset the captured UI-thread id so DXUI_ASSERT_UI_THREAD
        // accepts whichever thread happens to dispatch us.
        DxuiResetUiThreadIdForTest();
    }



    //
    //  Resize edge classification — eight points, one per edge / corner.
    //

    TEST_METHOD (ResizeEdges_TopLeftCorner_ReturnsHtTopLeft)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (1, 1));

        Assert::AreEqual ((int) HTTOPLEFT, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (ResizeEdges_TopRightCorner_ReturnsHtTopRight)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (s_kClientWidthDip - 1, 1));

        Assert::AreEqual ((int) HTTOPRIGHT, (int) DxuiHwndSource::KindToHt (kind));
    }


    //
    //  Regression (#98): the top-right resize corner must win over the close
    //  button that overlaps it. A point 10 dip in from the top-right is inside
    //  the enlarged corner grab (resizeBorderDip * s_kResizeCornerMult = 12)
    //  but past the 6-dip straight edge, and lands squarely on the close button
    //  rect -- it must classify as the diagonal resize corner, not HTCLOSE, so
    //  the corner stays draggable.
    //
    TEST_METHOD (ResizeCorner_TopRightOverCloseButton_BeatsClose)
    {
        SyntheticHost   sh   = BuildSyntheticHost();
        POINT           pt   = MakePoint (s_kClientWidthDip - 10, 10);

        Assert::IsTrue (pt.x >= sh.closeRectDip.left && pt.x < sh.closeRectDip.right &&
                        pt.y >= sh.closeRectDip.top  && pt.y < sh.closeRectDip.bottom,
                        L"guard: the test point must lie within the close button rect");

        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (pt);

        Assert::AreEqual ((int) HTTOPRIGHT, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (ResizeEdges_BottomLeftCorner_ReturnsHtBottomLeft)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (1, s_kClientHeightDip - 1));

        Assert::AreEqual ((int) HTBOTTOMLEFT, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (ResizeEdges_BottomRightCorner_ReturnsHtBottomRight)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (s_kClientWidthDip - 1,
                                                                      s_kClientHeightDip - 1));

        Assert::AreEqual ((int) HTBOTTOMRIGHT, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (ResizeEdges_LeftEdgeMidHeight_ReturnsHtLeft)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (1, s_kClientHeightDip / 2));

        Assert::AreEqual ((int) HTLEFT, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (ResizeEdges_RightEdgeMidHeight_ReturnsHtRight)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (s_kClientWidthDip - 1,
                                                                      s_kClientHeightDip / 2));

        Assert::AreEqual ((int) HTRIGHT, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (ResizeEdges_TopEdgeMidWidth_ReturnsHtTop)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (s_kClientWidthDip / 2, 1));

        Assert::AreEqual ((int) HTTOP, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (ResizeEdges_BottomEdgeMidWidth_ReturnsHtBottom)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (s_kClientWidthDip / 2,
                                                                      s_kClientHeightDip - 1));

        Assert::AreEqual ((int) HTBOTTOM, (int) DxuiHwndSource::KindToHt (kind));
    }



    //
    //  Caption / system-button / client classification.
    //

    TEST_METHOD (Caption_BlankAreaInTitleStrip_ReturnsHtCaption)
    {
        SyntheticHost  sh   = BuildSyntheticHost();
        // Mid-caption, well inside resize-border inset, away from any button.
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (200, s_kCaptionHeightDip / 2));

        Assert::AreEqual ((int) HTCAPTION, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (SystemButton_MinCenter_ReturnsHtMinButton)
    {
        SyntheticHost  sh = BuildSyntheticHost();
        POINT          pt = MakePoint ((sh.minRectDip.left + sh.minRectDip.right) / 2,
                                       (sh.minRectDip.top  + sh.minRectDip.bottom) / 2);
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (pt);

        Assert::AreEqual ((int) HTMINBUTTON, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (SystemButton_CloseCenter_ReturnsHtClose)
    {
        SyntheticHost  sh = BuildSyntheticHost();
        POINT          pt = MakePoint ((sh.closeRectDip.left + sh.closeRectDip.right) / 2,
                                       (sh.closeRectDip.top  + sh.closeRectDip.bottom) / 2);
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (pt);

        Assert::AreEqual ((int) HTCLOSE, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (Client_PointBelowCaption_ReturnsHtClient)
    {
        SyntheticHost  sh = BuildSyntheticHost();
        // Mid-client, comfortably below the caption strip and inset from
        // the resize border.
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (MakePoint (s_kClientWidthDip / 2,
                                                                      s_kClientHeightDip / 2));

        Assert::AreEqual ((int) HTCLIENT, (int) DxuiHwndSource::KindToHt (kind));
    }



    //
    //  T073 — Win11 snap-layouts trigger. The maximise button MUST
    //  classify as HTMAXBUTTON so Windows fires the hover popover.
    //

    TEST_METHOD (SnapLayouts_MaxButtonCenter_ReturnsHtMaxButton)
    {
        SyntheticHost  sh = BuildSyntheticHost();
        POINT          pt = MakePoint ((sh.maxRectDip.left + sh.maxRectDip.right) / 2,
                                       (sh.maxRectDip.top  + sh.maxRectDip.bottom) / 2);
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (pt);

        Assert::AreEqual ((int) DxuiHitTestKind::MaxButton, (int) kind);
        Assert::AreEqual ((int) HTMAXBUTTON, (int) DxuiHwndSource::KindToHt (kind));
    }


    TEST_METHOD (SnapLayouts_MaxButtonCorner_StillReturnsHtMaxButton)
    {
        // Hit near the bottom-left corner of the max button — still
        // inside its bounds, so Win11 must still get HTMAXBUTTON to
        // surface the snap-layouts popover. Resize-edge check is
        // upstream of the tree walk, so verify the upper-right
        // corner doesn't accidentally trip the top-right resize
        // corner.
        SyntheticHost  sh = BuildSyntheticHost();
        POINT          pt = MakePoint (sh.maxRectDip.left + 2,
                                       sh.maxRectDip.bottom - 2);
        DxuiHitTestKind kind = sh.host->ClassifyHitForTest (pt);

        Assert::AreEqual ((int) HTMAXBUTTON, (int) DxuiHwndSource::KindToHt (kind));
    }



    //
    //  KindToHt mapping — direct table verification.
    //

    TEST_METHOD (KindToHt_MapsEveryEnumValueToExpectedHtCode)
    {
        Assert::AreEqual ((int) HTNOWHERE,    (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::None));
        Assert::AreEqual ((int) HTCLIENT,     (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::Client));
        Assert::AreEqual ((int) HTCAPTION,    (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::Caption));
        Assert::AreEqual ((int) HTMINBUTTON,  (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::MinButton));
        Assert::AreEqual ((int) HTMAXBUTTON,  (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::MaxButton));
        Assert::AreEqual ((int) HTCLOSE,      (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::CloseButton));
        Assert::AreEqual ((int) HTLEFT,       (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeEdgeLeft));
        Assert::AreEqual ((int) HTRIGHT,      (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeEdgeRight));
        Assert::AreEqual ((int) HTTOP,        (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeEdgeTop));
        Assert::AreEqual ((int) HTBOTTOM,     (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeEdgeBottom));
        Assert::AreEqual ((int) HTTOPLEFT,    (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeCornerTL));
        Assert::AreEqual ((int) HTTOPRIGHT,   (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeCornerTR));
        Assert::AreEqual ((int) HTBOTTOMLEFT, (int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeCornerBL));
        Assert::AreEqual ((int) HTBOTTOMRIGHT,(int) DxuiHwndSource::KindToHt (DxuiHitTestKind::ResizeCornerBR));
    }



    //
    //  DxuiCaptionBar default classification — blank area falls
    //  through to Caption even when there are children.
    //

    TEST_METHOD (CaptionBar_BlankAreaWithNoChildren_ReturnsCaption)
    {
        DxuiCaptionBar  bar;

        bar.SetBounds (MakeRect (0, 0, 800, 32));

        Assert::AreEqual ((int) DxuiHitTestKind::Caption,
                          (int) bar.ClassifyHit (MakePoint (400, 16)));
    }


    TEST_METHOD (CaptionBar_ChildAtPoint_DefersToChild)
    {
        DxuiCaptionBar    bar;
        DxuiSystemButton &  closeBtn = bar.Add<DxuiSystemButton> (DxuiSystemButtonKind::Close);

        bar.SetBounds      (MakeRect (0, 0, 800, 32));
        closeBtn.SetBounds (MakeRect (754, 0, 800, 32));

        Assert::AreEqual ((int) DxuiHitTestKind::CloseButton,
                          (int) bar.ClassifyHit (MakePoint (777, 16)));
        Assert::AreEqual ((int) DxuiHitTestKind::Caption,
                          (int) bar.ClassifyHit (MakePoint (100, 16)));
    }



    //
    //  DxuiDragRegion — invisible filler always classifies as
    //  Caption, regardless of point.
    //

    TEST_METHOD (DragRegion_AnyPoint_ReturnsCaption)
    {
        DxuiDragRegion  drag;

        drag.SetBounds (MakeRect (0, 0, 400, 32));

        Assert::AreEqual ((int) DxuiHitTestKind::Caption,
                          (int) drag.ClassifyHit (MakePoint (200, 16)));
        Assert::AreEqual ((int) DxuiHitTestKind::Caption,
                          (int) drag.ClassifyHit (MakePoint (0, 0)));
    }



    //
    //  DxuiSystemButton — Kind round-trips through ClassifyHit.
    //

    TEST_METHOD (SystemButton_KindRoundtripsThroughClassifyHit)
    {
        DxuiSystemButton  minBtn   (DxuiSystemButtonKind::Min);
        DxuiSystemButton  maxBtn   (DxuiSystemButtonKind::Max);
        DxuiSystemButton  closeBtn (DxuiSystemButtonKind::Close);


        Assert::AreEqual ((int) DxuiHitTestKind::MinButton,
                          (int) minBtn.ClassifyHit (MakePoint (0, 0)));
        Assert::AreEqual ((int) DxuiHitTestKind::MaxButton,
                          (int) maxBtn.ClassifyHit (MakePoint (0, 0)));
        Assert::AreEqual ((int) DxuiHitTestKind::CloseButton,
                          (int) closeBtn.ClassifyHit (MakePoint (0, 0)));
    }


    TEST_METHOD (SystemButton_AccessibleName_MatchesKind)
    {
        DxuiSystemButton  minBtn   (DxuiSystemButtonKind::Min);
        DxuiSystemButton  maxBtn   (DxuiSystemButtonKind::Max);
        DxuiSystemButton  closeBtn (DxuiSystemButtonKind::Close);


        Assert::AreEqual (std::wstring (L"Minimize"), minBtn.AccessibleName());
        Assert::AreEqual (std::wstring (L"Maximize"), maxBtn.AccessibleName());
        Assert::AreEqual (std::wstring (L"Close"),    closeBtn.AccessibleName());
    }


    TEST_METHOD (SystemButton_MaximizedMaxButton_PaintsRestoreGlyph)
    {
        DxuiSystemButton    maxBtn (DxuiSystemButtonKind::Max);
        DxuiDpiScaler       scaler;
        MockDxuiPainter     painter;
        MockDxuiTextRenderer text;
        MockDxuiTheme       theme;
        size_t              normalCallCount = 0;


        scaler.SetDpi (96);
        maxBtn.Layout (MakeRect (0, 0, s_kSystemButtonWidthDip, s_kCaptionHeightDip), scaler);
        maxBtn.Paint (painter, text, theme);
        normalCallCount = painter.Calls().size();

        painter.Reset();
        maxBtn.SetMaximized (true);
        maxBtn.Paint (painter, text, theme);

        Assert::IsTrue (painter.Calls().size() > normalCallCount);
        Assert::AreEqual ((int) RecordedPaintKind::OutlineRect,
                          (int) painter.Calls()[0].kind);
        Assert::AreEqual ((int) RecordedPaintKind::OutlineRect,
                          (int) painter.Calls()[1].kind);
    }


    //
    //  SetBeforePresentHook — stores the callback so the host's
    //  WM_PAINT pump can invoke it after the panel-tree Paint walk
    //  and before swap-chain Present. The pump itself lands later in
    //  Phase 11d; this verifies that the storage surface round-trips
    //  the function pointer today so external renderers can register
    //  early.
    //

    TEST_METHOD (BeforePresentHook_StartsNull)
    {
        DxuiHwndSource  host;

        Assert::IsFalse ((bool) host.BeforePresentHook());
    }


    TEST_METHOD (BeforePresentHook_StoresCallback)
    {
        DxuiHwndSource  host;
        int             callCount = 0;


        host.SetBeforePresentHook ([&] () { ++callCount; });

        Assert::IsTrue (((bool) host.BeforePresentHook()));
        host.BeforePresentHook() ();
        Assert::AreEqual (1, callCount);
    }


    TEST_METHOD (BeforePresentHook_NullClearsCallback)
    {
        DxuiHwndSource  host;


        host.SetBeforePresentHook ([] () {});
        Assert::IsTrue ((bool) host.BeforePresentHook());

        host.SetBeforePresentHook (nullptr);
        Assert::IsFalse ((bool) host.BeforePresentHook());
    }



    //
    //  SetTheme / paint-pump prerequisites. The real WM_PAINT pump
    //  needs a live HWND + swap chain so its behavior is only
    //  exercisable from the running app; here we verify the bits the
    //  pump reads (theme pointer, broadcast on theme change) without
    //  poking at the GPU.
    //

    TEST_METHOD (SetTheme_BroadcastsOnThemeChangedToRoot)
    {
        struct ThemeListener : public IDxuiControl
        {
            int  themeChangedCount = 0;

            void  Layout         (const RECT &, const DxuiDpiScaler &) override {}
            void  Paint          (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &) override {}
            void  OnThemeChanged() override { ++themeChangedCount; }
        };

        std::unique_ptr<DxuiPanel>  root     = std::make_unique<DxuiPanel>();
        ThemeListener &             listener = root->Add<ThemeListener>();
        DxuiHwndSource              host (MakeRect (0, 0, s_kClientWidthDip, s_kClientHeightDip),
                                          s_kResizeBorderDip,
                                          std::move (root));
        MockDxuiTheme               theme;


        host.SetTheme (&theme);

        Assert::AreEqual (1, listener.themeChangedCount);
    }


    TEST_METHOD (SetTheme_NullClearsThemeWithoutCrash)
    {
        DxuiHwndSource  host;
        MockDxuiTheme   theme;


        host.SetTheme (&theme);
        host.SetTheme (nullptr);

        // Reaching here without an assertion / crash is the contract.
        Assert::IsTrue (true);
    }


    //
    //  SetContentPanel -- replaces the root panel with a caller-
    //  supplied tree and preserves the existing bounds so the new
    //  panel lays out into the same client rect.
    //

    TEST_METHOD (SetContentPanel_ReplacesRootAndInheritsBounds)
    {
        std::unique_ptr<DxuiPanel>     originalRoot = std::make_unique<DxuiPanel>();
        DxuiHwndSource                 host (MakeRect (0, 0, s_kClientWidthDip, s_kClientHeightDip),
                                             s_kResizeBorderDip,
                                             std::move (originalRoot));
        std::unique_ptr<DxuiPanel>     replacement  = std::make_unique<DxuiPanel>();
        DxuiPanel *                    replacementRaw = replacement.get();


        host.SetContentPanel (std::move (replacement));

        Assert::AreEqual ((const void *) replacementRaw,
                          (const void *) &host.Root());

        RECT  bounds = host.Root().Bounds();

        Assert::AreEqual ((LONG) 0,                  bounds.left);
        Assert::AreEqual ((LONG) 0,                  bounds.top);
        Assert::AreEqual ((LONG) s_kClientWidthDip,  bounds.right);
        Assert::AreEqual ((LONG) s_kClientHeightDip, bounds.bottom);
    }


    TEST_METHOD (SetContentPanel_NewPanelReceivesChildrenViaAdd)
    {
        std::unique_ptr<DxuiPanel>     originalRoot = std::make_unique<DxuiPanel>();
        DxuiHwndSource                 host (MakeRect (0, 0, s_kClientWidthDip, s_kClientHeightDip),
                                             s_kResizeBorderDip,
                                             std::move (originalRoot));
        std::unique_ptr<DxuiPanel>     replacement  = std::make_unique<DxuiPanel>();

        replacement->Add<DxuiCaptionBar>();

        host.SetContentPanel (std::move (replacement));

        Assert::AreEqual ((size_t) 1, host.Root().ChildCount());
    }
};
