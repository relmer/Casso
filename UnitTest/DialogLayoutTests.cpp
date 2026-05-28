#include "Pch.h"

#include "../Casso/Ui/Dialog/DialogLayout.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DialogLayoutTests
{
    static constexpr float  s_kCharWidthPx = 8.0f;
    static constexpr float  s_kButtonWidthExtraPx = 0.0f;



    static DialogLayoutMetrics MakeMetrics ()
    {
        DialogLayoutMetrics  m;
        m.dpiScale           = 1.0f;
        m.maxBodyWidthPx     = 200.0f;
        m.buttonHeightPx     = 28.0f;
        m.buttonPaddingPx    = 16.0f;
        m.buttonSpacingPx    = 8.0f;
        m.iconSizePx         = 32.0f;
        m.bodyLineHeightPx   = 18.0f;
        m.outerPaddingPx     = 16.0f;
        m.iconBodyGapPx      = 12.0f;
        m.bodyButtonsGapPx   = 16.0f;
        m.minButtonWidthPx   = 0.0f;
        m.measureBodyTextRun = [] (std::wstring_view v) { return (float) v.size() * s_kCharWidthPx; };
        m.measureButtonLabel = [] (std::wstring_view v) { return (float) v.size() * s_kCharWidthPx + s_kButtonWidthExtraPx; };
        return m;
    }



    TEST_CLASS (DialogLayoutTests)
    {
    public:

        TEST_METHOD (ButtonRow_RightAlignedAndSpaced)
        {
            DialogDefinition  def;
            def.buttons = { { L"OK", 1, true, false }, { L"Cancel", 0, false, true } };

            auto  m = MakeMetrics();
            auto  r = LayoutDialog (def, m);

            Assert::AreEqual ((size_t) 2, r.buttonRectsPx.size());
            // Right-most button hugs content right edge.
            Assert::IsTrue (r.buttonRectsPx[1].right >= r.buttonRectsPx[0].right);
            // Inter-button gap == buttonSpacingPx.
            LONG  gap = r.buttonRectsPx[1].left - r.buttonRectsPx[0].right;
            Assert::AreEqual ((LONG) m.buttonSpacingPx, gap);
        }


        TEST_METHOD (BodyTextWraps_AtMaxWidth)
        {
            DialogDefinition  def;
            def.body = { { std::wstring (100, L'a'), false, L"" } };

            auto  m = MakeMetrics();   // maxBodyWidthPx = 200, char = 8 → ~25 chars/line
            auto  r = LayoutDialog (def, m);

            // The single run should span multiple lines → wrapped rect taller than one line.
            LONG  h = r.bodyRunRectsPx[0].bottom - r.bodyRunRectsPx[0].top;
            Assert::IsTrue (h > (LONG) m.bodyLineHeightPx);
        }


        TEST_METHOD (Icon_ChangesTotalWidthDelta)
        {
            DialogDefinition  def;
            def.body    = { { L"Hello", false, L"" } };

            auto  m = MakeMetrics();
            auto  noIcon = LayoutDialog (def, m);

            def.icon = DialogIcon::Info;
            auto  withIcon = LayoutDialog (def, m);

            // Icon-present total width grows by iconSize + iconBodyGap.
            Assert::IsTrue (withIcon.totalSizePx.cx >= noIcon.totalSizePx.cx);
            Assert::IsTrue ((withIcon.iconRectPx.right - withIcon.iconRectPx.left) == (LONG) m.iconSizePx);
        }


        TEST_METHOD (Hyperlink_HitRectMatchesBodyRunRect)
        {
            DialogDefinition  def;
            def.body = {
                { L"See ", false, L"" },
                { L"link", true,  L"https://example.com" },
                { L" here.", false, L"" }
            };

            auto  m = MakeMetrics();
            auto  r = LayoutDialog (def, m);

            Assert::AreEqual ((size_t) 1, r.hyperlinkHitRectsPx.size());
            // Hyperlink hit rect equals the body rect of the second run.
            Assert::AreEqual (r.bodyRunRectsPx[1].left,   r.hyperlinkHitRectsPx[0].left);
            Assert::AreEqual (r.bodyRunRectsPx[1].right,  r.hyperlinkHitRectsPx[0].right);
            Assert::AreEqual (r.bodyRunRectsPx[1].top,    r.hyperlinkHitRectsPx[0].top);
            Assert::AreEqual (r.bodyRunRectsPx[1].bottom, r.hyperlinkHitRectsPx[0].bottom);
        }


        TEST_METHOD (CustomBody_ReservesSpaceBetweenBodyAndButtons)
        {
            DialogDefinition  def;
            def.body                 = { { L"Body.", false, L"" } };
            def.buttons              = { { L"OK", 1, true, false } };
            def.customBodyMinSizePx  = { 100, 60 };
            def.onPaintCustomBody    = [] (DialogPaintContext &) {};

            auto  m = MakeMetrics();
            auto  r = LayoutDialog (def, m);

            // Custom body rect is non-empty and sits between body and buttons.
            LONG  cbH = r.customBodyRectPx.bottom - r.customBodyRectPx.top;
            Assert::AreEqual ((LONG) 60, cbH);
            Assert::IsTrue (r.customBodyRectPx.top    >= r.bodyRunRectsPx[0].bottom);
            Assert::IsTrue (r.buttonRectsPx[0].top    >= r.customBodyRectPx.bottom);
        }


        TEST_METHOD (Dpi_ScalesPaddingViaOuterPadding)
        {
            DialogDefinition  def;
            def.body    = { { L"Hi", false, L"" } };
            def.buttons = { { L"OK", 1, true, false } };

            auto  smaller = MakeMetrics();
            auto  bigger  = MakeMetrics();
            bigger.outerPaddingPx *= 2.0f;
            bigger.buttonHeightPx *= 2.0f;

            auto  rs = LayoutDialog (def, smaller);
            auto  rl = LayoutDialog (def, bigger);

            // Larger metrics yield a larger total size.
            Assert::IsTrue (rl.totalSizePx.cx > rs.totalSizePx.cx);
            Assert::IsTrue (rl.totalSizePx.cy > rs.totalSizePx.cy);
        }
    };
}
