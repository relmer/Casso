#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewBackgroundRangeTests
//
//  A cell's background ranges: a fill behind each range of its characters,
//  the row's height tall, side by side in the text's order, with the text
//  over each drawn in an ink that reads on its fill.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewBackgroundRangeTests)
{
public:

    static constexpr uint32_t  kPale = 0xFFF0E68Cu;   // a pale yellow, which light ink can't read on
    static constexpr uint32_t  kDeep = 0xFF8B1A1Au;   // a deep red, which dark ink can't read on

    static DxuiListView::Cell MakeCell()
    {
        DxuiListView::Cell  cell { L"A=1:PRINT A" };



        cell.backgroundRanges = { { 0, 4, kPale }, { 4, 11, kDeep } };
        return cell;
    }

    static void PaintList (const DxuiListView::Cell & cell, MockDxuiPainter & painter, MockDxuiTextRenderer & text)
    {
        MockDxuiTheme   theme;
        DxuiListView    list;
        DxuiDpiScaler   scaler;



        scaler.SetDpi (96);

        list.SetColumns ({ DxuiListView::Column { L"Listing", 400 } });
        list.SetRows    ({ { cell } });
        list.Layout     (RECT { 0, 0, 600, 200 }, scaler);
        list.Paint      (painter, text, theme);
    }



    TEST_METHOD (EachRangesFillIsBehindItsCharactersInOrder)
    {
        MockDxuiPainter                       painter;
        MockDxuiTextRenderer                  text;
        std::optional<RecordedPaintCall>      pale;
        std::optional<RecordedPaintCall>      deep;



        PaintList (MakeCell(), painter, text);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.argb == kPale) { pale = call; }
            if (call.kind == RecordedPaintKind::FillRect && call.argb == kDeep) { deep = call; }
        }

        Assert::IsTrue   (pale.has_value() && deep.has_value(), L"a fill for each range");
        Assert::IsTrue   (pale->width > 0.0f && deep->width > pale->width, L"each as wide as its characters");
        Assert::AreEqual (pale->x + pale->width, deep->x, 0.01f, L"the second starts where the first ends");
        Assert::AreEqual (pale->height, deep->height, L"both the row's height");
    }



    TEST_METHOD (TheTextOverARangeReadsOnItsFill)
    {
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        DxuiListView::Cell     cell  = MakeCell();
        std::vector<uint32_t>  fills = DxuiListView::GetCharacterFills (cell, 0xFF202020u);
        int                    found = 0;



        Assert::AreEqual (kPale, fills[0], L"an opaque range's fill is its color");
        Assert::AreEqual (kDeep, fills[10]);

        cell.backgroundRanges = { { 2, 4, 0x80FFFFFFu } };
        fills                 = DxuiListView::GetCharacterFills (cell, 0xFF000000u);

        Assert::AreEqual (0xFF000000u, fills[0], L"outside every range, the fill under the cell");
        Assert::AreNotEqual (0xFF000000u, fills[2], L"a partly transparent range is drawn over it");

        PaintList (MakeCell(), painter, text);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.text == L"A=1:")
            {
                Assert::IsTrue (DxuiColor::ComputeContrastRatio (call.argb, kPale) >= 4.5f, L"dark enough on the pale fill");
                found++;
            }

            if (call.text == L"PRINT A")
            {
                Assert::IsTrue (DxuiColor::ComputeContrastRatio (call.argb, kDeep) >= 4.5f, L"light enough on the deep fill");
                found++;
            }
        }

        Assert::AreEqual (2, found, L"each range's text is drawn as a separate run");
    }



    //  A range's tip is the one at a caret position in it; the end of the
    //  text counts as in the range that ends there.
    TEST_METHOD (ARangesTipIsTheOneAtThePosition)
    {
        DxuiListView::Cell  cell { L"A=1:PRINT A" };



        cell.rangeTips = { { 0, 4, L"first" }, { 4, 11, L"second" } };

        Assert::AreEqual (std::wstring (L"first"),  *DxuiListView::FindRangeTip (cell, 0));
        Assert::AreEqual (std::wstring (L"first"),  *DxuiListView::FindRangeTip (cell, 3));
        Assert::AreEqual (std::wstring (L"second"), *DxuiListView::FindRangeTip (cell, 4), L"a range starts at its first character");
        Assert::AreEqual (std::wstring (L"second"), *DxuiListView::FindRangeTip (cell, 11), L"the text's end is in the last range");

        cell.rangeTips = { { 4, 9, L"middle" } };
        Assert::IsNull (DxuiListView::FindRangeTip (cell, 2), L"outside every range");
    }
};
