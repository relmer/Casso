#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEndEdgesTests
//
//  A movable strip: a docked one draws hairlines across its short ends and
//  none along its long sides, one laid out at its natural length keeps every
//  entry on the strip, and the window it floats in runs its owner's frame
//  on every tick of the move loop.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarEndEdgesTests)
{
public:

    static std::vector<DxuiToolbar::Entry> MakeEntries()
    {
        std::vector<DxuiToolbar::Entry>  entries (6);

        for (int i = 0; i < 6; i++)
        {
            auto  command = std::make_shared<DxuiCommand>();

            command->id    = i + 1;
            command->label = L"Command";
            command->glyph = L"x";

            entries[(size_t) i].command = command;
            entries[(size_t) i].group   = i / 2;
        }

        return entries;
    }


    static std::vector<RecordedPaintCall> GetEdgeFills (const MockDxuiPainter & painter, uint32_t edgeArgb)
    {
        std::vector<RecordedPaintCall>  edges;

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.argb == edgeArgb)
            {
                edges.push_back (call);
            }
        }

        return edges;
    }


    TEST_METHOD (ADockedStripDrawsOnlyItsShortEnds)
    {
        DxuiToolbar                     bar;
        DxuiDpiScaler                   scaler;
        MockDxuiPainter                 painter;
        MockDxuiTextRenderer            text;
        MockDxuiTheme                   theme;
        std::vector<RecordedPaintCall>  edges;


        scaler.SetDpi       (96);
        bar.SetTextRenderer (&text);
        bar.SetGrabHandle   (true);
        bar.SetEndEdges     (true);
        bar.SetEntries      (MakeEntries());
        bar.Layout          (RECT { 100, 50, 700, 92 }, scaler);
        bar.Paint           (painter, text, theme);

        edges = GetEdgeFills (painter, theme.ContentEdge());

        Assert::AreEqual ((size_t) 2, edges.size(), L"one hairline at each end");

        for (const RecordedPaintCall & edge : edges)
        {
            Assert::AreEqual (1.0f,  edge.width,  L"a hairline across the end, not along a side");
            Assert::AreEqual (42.0f, edge.height);
        }
    }


    TEST_METHOD (AVerticalDockedStripDrawsOnlyItsShortEnds)
    {
        DxuiToolbar                     bar;
        DxuiDpiScaler                   scaler;
        MockDxuiPainter                 painter;
        MockDxuiTextRenderer            text;
        MockDxuiTheme                   theme;
        std::vector<RecordedPaintCall>  edges;


        scaler.SetDpi       (96);
        bar.SetTextRenderer (&text);
        bar.SetVertical     (true);
        bar.SetEndEdges     (true);
        bar.SetEntries      (MakeEntries());
        bar.Layout          (RECT { 0, 50, 42, 600 }, scaler);
        bar.Paint           (painter, text, theme);

        edges = GetEdgeFills (painter, theme.ContentEdge());

        Assert::AreEqual ((size_t) 2, edges.size());

        for (const RecordedPaintCall & edge : edges)
        {
            Assert::AreEqual (1.0f,  edge.height);
            Assert::AreEqual (42.0f, edge.width);
        }
    }


    TEST_METHOD (AStripAtItsNaturalLengthOverflowsNothing)
    {
        DxuiToolbar           bar;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        int                   length = 0;


        scaler.SetDpi       (144);
        bar.SetTextRenderer (&text);
        bar.SetGrabHandle   (true);
        bar.SetLabels       (false);
        bar.SetEntries      (MakeEntries());

        length = bar.GetNaturalLengthPx (scaler);
        bar.Layout (RECT { 0, 0, length, scaler.ToPx (DxuiToolbar::GetBandDip()) }, scaler);

        for (int id = 1; id <= 6; id++)
        {
            Assert::IsFalse (bar.IsInSeeMore (id), L"a floating strip sized to its natural length keeps every entry");
        }
    }


    TEST_METHOD (TheMoveLoopRunsTheOwnersFrame)
    {
        DxuiToolbarWindow  window;
        int                frames = 0;


        window.SetOnMoveLoopFrame ([&frames] { frames++; });
        window.RunModalLoopTick();

        Assert::AreEqual (1, frames, L"a drag of the floating bar keeps the owner's frames, and its machine, going");
    }
};
