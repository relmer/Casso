#include "Pch.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"
#include "FakeTextGeometry.h"
#include "FakeTextSelectionRange.h"
#include "FakeTextViewport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPainterTests
//
//  The lines of a 400-pixel view drawn in 8 by 16 pixel cells, the text 6
//  pixels in from its edges.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextPainterTests)
{
public:

    struct Rig
    {
        DxuiTextLayout          layout;
        FakeTextGeometry        geometry  { 400, 400 };
        FakeTextViewport        viewport;
        FakeTextSelectionRange  selection;
        DxuiTextPictures        pictures  { layout, geometry, viewport };
        DxuiTextPainter         painter   { layout, geometry, viewport, selection, pictures };
        MockDxuiPainter         canvas;
        MockDxuiTextRenderer    text;
        MockDxuiTheme           theme;

        void  Paint (std::vector<DxuiTextLayout::Row> rows)
        {
            layout.SetCellSize (8, 16);
            layout.SetRows (std::move (rows));
            layout.BuildColumnCells();
            layout.BuildLines (48, 1.0f);

            painter.Paint (canvas, text, theme, theme.MonospaceFont(), L"Segoe MDL2 Assets", 1.0f);
        }
    };


    static DxuiTextLayout::Row  MakeRow (std::vector<std::wstring> cells)
    {
        DxuiTextLayout::Row  row;

        row.cells = std::move (cells);

        return row;
    }


    //  A picture is drawn once, however many lines it takes.
    TEST_METHOD (Picture_IsDrawnOnce)
    {
        Rig                  rig;
        DxuiTextLayout::Row  row     = MakeRow ({ L"" });
        auto                 picture = std::make_shared<DxuiTextLayout::Picture>();
        int                  drawn   = 0;

        picture->bgra.assign (10 * 40, 0xFF336699);
        picture->width  = 10;
        picture->height = 40;
        row.picture     = picture;

        rig.Paint ({ MakeRow ({ L"above" }), row });

        for (const RecordedTextCall & call : rig.text.Calls())
        {
            drawn += (call.kind == RecordedTextKind::DrawFramebuffer) ? 1 : 0;
        }

        Assert::AreEqual (1,  drawn);
        Assert::AreEqual (10, rig.text.GetLastFramebufferWidth());
    }


    //  A span draws its characters in its color, and the rest of the cell
    //  in the text color, each stretch of one color as one string.
    TEST_METHOD (Span_DrawsItsCharactersInItsColor)
    {
        Rig                  rig;
        DxuiTextLayout::Row  row     = MakeRow ({ L"10", L"LDA #1" });
        bool                 colored = false;
        bool                 plain   = false;

        row.spans.push_back ({ 1, 0, 3, 0xFF112233 });

        rig.Paint ({ row });

        for (const RecordedTextCall & call : rig.text.Calls())
        {
            colored = colored || (call.text == L"LDA" && call.argb == 0xFF112233);
            plain   = plain   || (call.text == L" #1" && call.argb != 0xFF112233);
        }

        Assert::IsTrue (colored, L"The span's characters are in its color");
        Assert::IsTrue (plain,   L"and the rest of the cell in the text color");
    }


    //  An inverse span is filled with its color, and its characters are drawn
    //  in the background color over the fill.
    TEST_METHOD (Span_Inverse_FillsItsColorAndDrawsInTheBackground)
    {
        Rig                   rig;
        DxuiTextLayout::Row   row    = MakeRow ({ L"10", L"PRINT \"^DX\"" });
        DxuiTextLayout::Span  span;
        bool                  filled = false;
        bool                  drawn  = false;

        span.cell    = 1;
        span.start   = 7;
        span.length  = 2;
        span.argb    = 0xFF112233;
        span.inverse = true;
        row.spans.push_back (span);

        rig.Paint ({ row });

        for (const RecordedPaintCall & call : rig.canvas.Calls())
        {
            filled = filled || (call.kind == RecordedPaintKind::FillRect && call.argb == 0xFF112233 && call.width == 16.0f && call.height == 16.0f);
        }

        for (const RecordedTextCall & call : rig.text.Calls())
        {
            drawn = drawn || (call.text == L"^D" && call.argb == rig.theme.ContentBackground());
        }

        Assert::IsTrue (filled, L"The span's two cells are filled with its color");
        Assert::IsTrue (drawn,  L"and its characters are drawn in the background color");
    }


    //  The selected characters are filled with the selection color.
    TEST_METHOD (Selection_FillsTheSelectedCharacters)
    {
        Rig   rig;
        bool  filled = false;

        rig.selection.from = DxuiTextLayout::Position { 0, 2 };
        rig.selection.to   = DxuiTextLayout::Position { 0, 5 };

        rig.Paint ({ MakeRow ({ L"A", L"one" }) });

        for (const RecordedPaintCall & call : rig.canvas.Calls())
        {
            filled = filled || (call.kind == RecordedPaintKind::FillRect && call.argb == rig.theme.SelectionBackground() && call.width == 24.0f);
        }

        Assert::IsTrue (filled, L"The three characters of one are filled");
    }
};
