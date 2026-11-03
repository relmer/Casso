#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewWrapTests
//
//  Word wrap in the stretch column. The list is 400x300 px at 96 DPI with
//  the header hidden: a 100 px first column and a 300 px stretch column, so
//  a wrapped cell is 272 px wide after its padding. The mock renderer
//  measures 7 px a glyph and 16 px a line.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewWrapTests)
{
public:

    static constexpr int  kRowPx  = 30;
    static constexpr int  kLinePx = 16;



    static void ConfigureList (DxuiListView & list, bool isWrapped)
    {
        DxuiDpiScaler                      scaler;
        std::vector<DxuiListView::Column>  cols;
        DxuiListView::Column               item;
        DxuiListView::Column               value;



        scaler.SetDpi (96);

        item.title     = L"Item";
        item.widthDip  = 100;
        value.title    = L"Value";
        value.stretch  = true;
        value.elide    = DxuiElide::Tail;
        cols.push_back (item);
        cols.push_back (value);

        list.SetColumns    (std::move (cols));
        list.SetShowHeader (false);
        list.SetWrapsText  (isWrapped);
        list.Layout        (RECT { 0, 0, 400, 300 }, scaler);
    }



    static std::wstring MakeLongText()
    {
        std::wstring  text;



        //  Twenty-five four-letter words: 124 glyphs, 868 px on one line.
        for (int i = 0; i < 25; i++)
        {
            text += (i == 0) ? L"word" : L" word";
        }

        return text;
    }



    static std::vector<std::vector<DxuiListView::Cell>> MakeRows (const std::wstring & longText)
    {
        std::vector<std::vector<DxuiListView::Cell>>  rows;



        rows.push_back ({ DxuiListView::Cell { L"Long" }, DxuiListView::Cell { longText } });
        rows.push_back ({ DxuiListView::Cell { L"Short" }, DxuiListView::Cell { L"word" } });

        return rows;
    }



    static const RecordedTextCall * FindCall (const MockDxuiTextRenderer & text, const std::wstring & prefix)
    {
        const RecordedTextCall *  found = nullptr;



        for (const RecordedTextCall & call : text.Calls())
        {
            if (found == nullptr && call.kind == RecordedTextKind::DrawString && call.text.starts_with (prefix))
            {
                found = &call;
            }
        }

        return found;
    }



    TEST_METHOD (AWrappedCellIsDrawnWholeAndItsRowGrowsByTheExtraLines)
    {
        DxuiListView              list;
        MockDxuiPainter           painter;
        MockDxuiTextRenderer      text;
        MockDxuiTheme             theme;
        std::wstring              longText = MakeLongText();
        const RecordedTextCall *  drawn    = nullptr;



        ConfigureList (list, true);
        list.SetRows  (MakeRows (longText));
        static_cast<IDxuiControl &> (list).Paint (painter, text, theme);

        drawn = FindCall (text, L"word word");

        Assert::IsNotNull (drawn);
        Assert::AreEqual  (longText, drawn->text, L"the cell is wrapped, not elided");
        Assert::AreEqual  (4.0f * kLinePx, drawn->height, L"four lines of seven, seven, seven and four words");
        Assert::AreEqual  (0, list.HitTestRow (150, kRowPx + 3 * kLinePx - 1), L"the long row covers its extra lines");
        Assert::AreEqual  (1, list.HitTestRow (150, kRowPx + 3 * kLinePx + 1), L"the next row starts below them");
        Assert::AreEqual  (2 * kRowPx + 3 * kLinePx, list.GetRequiredHeightPx());
    }



    TEST_METHOD (WithoutWrapTheCellIsElidedAndRowsKeepTheirHeight)
    {
        DxuiListView              list;
        MockDxuiPainter           painter;
        MockDxuiTextRenderer      text;
        MockDxuiTheme             theme;
        const RecordedTextCall *  drawn    = nullptr;



        ConfigureList (list, false);
        list.SetRows  (MakeRows (MakeLongText()));
        static_cast<IDxuiControl &> (list).Paint (painter, text, theme);

        drawn = FindCall (text, L"word word");

        Assert::IsNotNull (drawn);
        Assert::AreNotEqual (MakeLongText(), drawn->text, L"the cell is elided");
        Assert::AreEqual    (1, list.HitTestRow (150, kRowPx + 1));
    }
};
