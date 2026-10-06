#include "Pch.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewSpanningRowTests
//
//  A cell marked spansRow is a line across the row rather than a value in a
//  column: its text runs on past the columns after it, those cells are not
//  drawn, and no column is fitted to it, by the measured fit or the glyph
//  count. Copying the row takes its text without the cells it covers.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewSpanningRowTests)
{
public:

    static constexpr int       kListWidthPx = 900;
    static constexpr uint32_t  kRgbMask     = 0x00FFFFFFu;
    static constexpr uint32_t  kDimAlpha    = 0xA0000000u;   // the list's dim cell: its text color at this alpha



    struct Fixture
    {
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiListView          list;

        void  Build (bool withNote, bool precise)
        {
            DxuiDpiScaler                                 scaler;
            std::vector<DxuiListView::Column>             cols;
            std::vector<std::vector<DxuiListView::Cell>>  data;
            DxuiListView::Cell                            note;

            scaler.SetDpi (96);

            cols.push_back (DxuiListView::Column { L"Site",    0 });
            cols.push_back (DxuiListView::Column { L"Routine", 0 });
            cols.push_back (DxuiListView::Column { L"Found",   0 });

            data.push_back ({ DxuiListView::Cell { L"$0803" }, DxuiListView::Cell { L"JSR $0900" }, DxuiListView::Cell { L"recorded" } });
            data.push_back ({ DxuiListView::Cell { L"$0900" }, DxuiListView::Cell { L"JSR $0A00" }, DxuiListView::Cell { L"recorded" } });

            if (withNote)
            {
                note.text     = GetNoteText();
                note.dim      = true;
                note.spansRow = true;

                data.push_back ({ note, DxuiListView::Cell {}, DxuiListView::Cell {} });
            }

            list.SetColumns        (std::move (cols));
            list.SetShowHeader     (true);
            list.SetPreciseAutoFit (precise);
            list.SetRows           (std::move (data));
            list.UpdateAutoFitFromRows();
            list.Layout            (RECT { 0, 0, kListWidthPx, 400 }, scaler);
        }

        void  Paint()
        {
            painter.Reset();
            text.Reset();
            list.Paint (painter, text, theme);
        }

        std::vector<int>  GetWidths() const
        {
            std::vector<int>  widths;

            for (size_t c = 0; c < list.GetColumnCount(); c++)
            {
                widths.push_back (list.GetColumnEffectiveWidthPx (c));
            }

            return widths;
        }
    };



    static std::wstring GetNoteText()
    {
        return L"A note far wider than any value in the columns beside it, which would push them aside";
    }



    //  The measured fit, as the debugger's panes use, sizes every column as it
    //  would without the note.
    TEST_METHOD (APreciseFitLeavesASpanningCellOut)
    {
        Fixture  plain;
        Fixture  noted;

        plain.Build (false, true);
        noted.Build (true,  true);
        plain.Paint();
        noted.Paint();

        Assert::IsTrue (plain.GetWidths() == noted.GetWidths(), L"the note widens no column");
    }



    //  The cheaper glyph-count fit leaves it out as well.
    TEST_METHOD (AGlyphCountFitLeavesASpanningCellOut)
    {
        Fixture  plain;
        Fixture  noted;

        plain.Build (false, false);
        noted.Build (true,  false);
        plain.Paint();
        noted.Paint();

        Assert::IsTrue (plain.GetWidths() == noted.GetWidths(), L"the note widens no column");
    }



    //  The text is drawn once, from its own column to the row's end, in the
    //  muted color the list gives a dim cell, and the cells it covers are not
    //  drawn.
    TEST_METHOD (ASpanningCellIsDrawnAcrossTheRow)
    {
        Fixture                                        f;
        std::vector<RecordedTextCall>::const_iterator  drawn;
        int                                            drawnCount = 0;



        f.Build (true, true);
        f.Paint();

        drawn = std::find_if (f.text.Calls().begin(), f.text.Calls().end(), [] (const RecordedTextCall & call)
        {
            return call.kind == RecordedTextKind::DrawString && call.text == GetNoteText();
        });

        drawnCount = (int) std::count_if (f.text.Calls().begin(), f.text.Calls().end(), [] (const RecordedTextCall & call)
        {
            return call.kind == RecordedTextKind::DrawString && call.text == GetNoteText();
        });

        Assert::IsTrue   (drawn != f.text.Calls().end(), L"the note is drawn");
        Assert::AreEqual (1, drawnCount, L"once");
        Assert::IsTrue   (drawn->width > (float) f.list.GetColumnEffectiveWidthPx (0) + (float) f.list.GetColumnEffectiveWidthPx (1),
                          L"wider than the first two columns together");
        Assert::AreEqual ((f.theme.Foreground() & kRgbMask) | kDimAlpha, drawn->argb, L"in the muted color");
    }



    //  Copying the row gives the note alone, without a tab for each cell it
    //  covers.
    TEST_METHOD (CopyingASpanningRowTakesItsTextAlone)
    {
        Fixture  f;

        f.Build (true, true);
        f.list.SetSelectedRow (2);

        Assert::AreEqual (GetNoteText() + L"\r\n", f.list.GetSelectionText());
    }



    //  The cells ahead of a spanning cell are still copied, each before a tab.
    TEST_METHOD (CopyingKeepsTheCellsAheadOfASpan)
    {
        Fixture                                       f;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        DxuiListView::Cell                            span;



        f.Build (false, true);

        span.text     = L"spans";
        span.spansRow = true;

        rows.push_back ({ DxuiListView::Cell { L"$0803" }, span, DxuiListView::Cell { L"hidden" } });
        f.list.SetRows        (std::move (rows));
        f.list.SetSelectedRow (0);

        Assert::AreEqual (std::wstring (L"$0803\tspans\r\n"), f.list.GetSelectionText());
    }
};
