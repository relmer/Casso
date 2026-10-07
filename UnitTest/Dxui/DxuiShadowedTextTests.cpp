#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShadowedTextTests
//
//  The marquee: a line too long for its rect is drawn whole, twice, one
//  period apart, clipped to the rect, and slid by the offset. Without one the
//  line is drawn once, aligned in the rect, unclipped.
//
//  The ink pass is the last draw of each copy, after its shadow rings, and
//  is the one these tests read.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiShadowedTextTests)
{
public:

    static constexpr int  kReach = 2;

    static void Setup (DxuiShadowedText & label, const std::wstring & text)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        label.SetText        (text);
        label.SetGlowReachPx (kReach);
        label.SetVisible     (true);
        label.Layout         (RECT { 100, 50, 200, 70 }, scaler);
    }

    static std::vector<RecordedTextCall> InkCalls (const MockDxuiTextRenderer & r)
    {
        std::vector<RecordedTextCall>  ink;
        int                            rings = 0;

        for (const RecordedTextCall & call : r.Calls())
        {
            if (call.kind != RecordedTextKind::DrawString)
            {
                continue;
            }

            if (++rings > kReach * DxuiShadowedText::kRingSamples)
            {
                ink.push_back (call);
                rings = 0;
            }
        }

        return ink;
    }


    TEST_METHOD (WithoutAMarqueeTheLineIsDrawnOnceAlignedAndUnclipped)
    {
        DxuiShadowedText      label;
        MockDxuiTextRenderer  r;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;



        Setup (label, L"short");
        label.Paint (painter, r, theme);

        Assert::AreEqual ((size_t) 1, InkCalls (r).size());
        Assert::AreEqual (100.0f, InkCalls (r)[0].x);
        Assert::IsTrue (InkCalls (r)[0].hAlign == DxuiTextHAlign::Center);

        for (const RecordedTextCall & call : r.Calls())
        {
            Assert::IsTrue (call.kind != RecordedTextKind::PushClipRect);
        }
    }


    TEST_METHOD (AMarqueeDrawsTwoCopiesAPeriodApartSlidByTheOffset)
    {
        DxuiShadowedText                label;
        MockDxuiTextRenderer            r;
        MockDxuiPainter                 painter;
        MockDxuiTheme                   theme;
        std::vector<RecordedTextCall>   ink;



        Setup (label, L"a name much too long for the rect");
        label.SetMarquee (300.0f, 40.0f);
        label.Paint (painter, r, theme);

        ink = InkCalls (r);

        Assert::AreEqual ((size_t) 2, ink.size());
        Assert::AreEqual (100.0f + kReach - 40.0f,          ink[0].x);
        Assert::AreEqual (100.0f + kReach - 40.0f + 300.0f, ink[1].x);
        Assert::IsTrue (ink[0].hAlign == DxuiTextHAlign::Left);

        Assert::IsTrue (r.Calls().front().kind == RecordedTextKind::PushClipRect);
        Assert::AreEqual (100.0f, r.Calls().front().x);
        Assert::AreEqual (100.0f, r.Calls().front().width);
        Assert::IsTrue (r.Calls().back().kind == RecordedTextKind::PopClipRect);
    }
};
