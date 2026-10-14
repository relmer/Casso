#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFramebufferViewTests
//
//  The copy on SetFramebuffer, the destination rectangle under integer and
//  fit scaling, and the one DrawFramebuffer call Paint makes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiFramebufferViewTests)
{
public:

    static void  LayOut (DxuiFramebufferView & view, LONG width, LONG height)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.Layout (RECT { 0, 0, width, height }, scaler);
    }


    TEST_METHOD (SetFramebuffer_CopiesTheBuffer)
    {
        DxuiFramebufferView    view;
        std::vector<uint32_t>  pixels (4 * 3, 0xFF112233u);

        view.SetFramebuffer (pixels.data(), 4, 3);
        pixels.clear();
        pixels.shrink_to_fit();

        Assert::IsTrue   (view.HasFramebuffer());
        Assert::AreEqual (4, view.GetFramebufferWidth());
        Assert::AreEqual (3, view.GetFramebufferHeight());

        view.SetFramebuffer (nullptr, 4, 3);
        Assert::IsFalse (view.HasFramebuffer());
    }


    TEST_METHOD (IntegerScaling_UsesLargestWholeMultipleCentered)
    {
        DxuiFramebufferView    view;
        std::vector<uint32_t>  pixels (280 * 192);
        RECT                   dest = {};

        view.SetFramebuffer (pixels.data(), 280, 192);
        LayOut (view, 700, 500);

        dest = view.GetDestinationRect();

        Assert::AreEqual (560L, dest.right - dest.left);
        Assert::AreEqual (384L, dest.bottom - dest.top);
        Assert::AreEqual (70L,  dest.left);
        Assert::AreEqual (58L,  dest.top);
    }


    TEST_METHOD (TooSmallForOneX_ShrinksKeepingAspect)
    {
        DxuiFramebufferView    view;
        std::vector<uint32_t>  pixels (560 * 384);
        RECT                   dest = {};

        view.SetFramebuffer (pixels.data(), 560, 384);
        LayOut (view, 280, 300);

        dest = view.GetDestinationRect();

        Assert::AreEqual (280L, dest.right - dest.left);
        Assert::AreEqual (192L, dest.bottom - dest.top);
    }


    TEST_METHOD (FitScaling_WithoutAspect_FillsBounds)
    {
        DxuiFramebufferView    view;
        std::vector<uint32_t>  pixels (100 * 100);
        RECT                   dest = {};

        view.SetFramebuffer (pixels.data(), 100, 100);
        view.SetScaling (false, false);
        LayOut (view, 250, 150);

        dest = view.GetDestinationRect();

        Assert::AreEqual (250L, dest.right - dest.left);
        Assert::AreEqual (150L, dest.bottom - dest.top);
    }


    TEST_METHOD (Paint_DrawsTheBufferOnce)
    {
        DxuiFramebufferView    view;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        std::vector<uint32_t>  pixels (40 * 30);
        int                    draws = 0;

        view.Paint (painter, text, theme);
        Assert::IsTrue (text.Calls().empty());

        view.SetFramebuffer (pixels.data(), 40, 30);
        LayOut (view, 100, 100);
        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawFramebuffer)
            {
                draws++;
                Assert::AreEqual (80.0f, call.width);
                Assert::AreEqual (60.0f, call.height);
            }
        }

        Assert::AreEqual (1,  draws);
        Assert::AreEqual (40, text.GetLastFramebufferWidth());
        Assert::AreEqual (30, text.GetLastFramebufferHeight());
    }


    TEST_METHOD (VirtualHeight_ScrollsByTheWheelAndAnnouncesTheTop)
    {
        DxuiFramebufferView    view;
        DxuiMouseEvent         wheel;
        std::vector<int>       tops;
        std::vector<uint32_t>  pixels (4 * 4);

        LayOut (view, 100, 50);
        view.SetVirtualHeight (500);
        view.SetOnScroll ([&tops] (int top) { tops.push_back (top); });

        wheel.kind       = DxuiMouseEventKind::Wheel;
        wheel.wheelDelta = -1.0f;

        Assert::IsTrue   (view.IsVirtual());
        Assert::IsTrue   (view.CanPan());
        Assert::IsTrue   (view.OnMouse (wheel), L"A notch down scrolls");
        Assert::AreEqual (48, view.GetScrollTop());
        Assert::AreEqual ((size_t) 1, tops.size());
        Assert::AreEqual (48, tops[0], L"and calls back with the new top");

        view.ScrollTo (10000);
        Assert::AreEqual (450, view.GetScrollTop(), L"No further than puts the end at the bottom");

        LayOut (view, 100, 200);
        Assert::AreEqual (300, view.GetScrollTop(), L"A taller view has less to scroll");
        Assert::AreEqual (90, view.GetBandWidth(), L"The band is as wide as the bounds less the scrollbar");

        view.SetFramebuffer (pixels.data(), 4, 4);
        Assert::IsFalse  (view.IsVirtual(), L"A framebuffer ends it");
        Assert::AreEqual (0, view.GetScrollTop());
    }


    TEST_METHOD (Band_IsPaintedWhereItLiesLessTheScroll)
    {
        DxuiFramebufferView    view;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        int                    draws = 0;

        LayOut (view, 100, 50);
        view.SetVirtualHeight (500);
        view.ScrollTo (100);
        view.SetBand (std::vector<uint32_t> (84 * 44), 84, 44, 2, 90);
        view.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawFramebuffer)
            {
                draws++;
                Assert::AreEqual (3.0f,   call.x, L"Centered across the band width, its margin to the left");
                Assert::AreEqual (-12.0f, call.y, L"Ten above the top, less its margin");
                Assert::AreEqual (84.0f,  call.width);
            }
        }

        Assert::AreEqual (1, draws);
    }
};
