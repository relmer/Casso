#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"
#include "Theme/DxuiRowLook.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRowLookTests
//
//  Explorer's row states, each resolved against a theme whose colors are all
//  different, so each assertion shows which color a state takes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiRowLookTests)
{
public:

    struct Theme : public MockDxuiTheme
    {
        uint32_t  ContentHover() const override { return 0xFF4D4D4Du; }
        uint32_t  ContentSelection() const override { return 0xFF505050u; }
        uint32_t  ContentSelectionEdge() const override { return 0xFFC3C3C3u; }
        uint32_t  ContentSelectionInactive() const override { return 0xFF333333u; }
        uint32_t  ContentSelectionMulti() const override { return 0xFF626262u; }
        uint32_t  ContentSelectionMultiEdge() const override { return 0xFF60CDFFu; }
    };


    //  The light theme's pane-without-focus outline, which the dark theme has
    //  none of.
    struct LightTheme : public Theme
    {
        uint32_t  ContentSelectionInactiveEdge() const override { return 0xFF949494u; }
    };


    TEST_METHOD (SelectedRow_PaneNotFocused_TakesTheInactiveOutline)
    {
        LightTheme   light;
        Theme        dark;

        Assert::AreEqual (0xFF949494u, DxuiRowLook::Resolve (light, true, true, false, false).edge);
        Assert::AreEqual (0u,          DxuiRowLook::Resolve (dark,  true, true, false, false).edge);
    }


    //  A list row's highlight is Explorer's box: 2 dip shorter than the row at
    //  top and bottom, ending 4 dip short of the last column's right edge
    //  rather than running across the empty width past it.
    TEST_METHOD (ListRowHighlight_EndsShortOfTheLastColumn)
    {
        DxuiListView                                   list;
        MockDxuiPainter                                painter;
        MockDxuiTextRenderer                           text;
        MockDxuiTheme                                  theme;
        DxuiDpiScaler                                  scaler;
        std::vector<std::vector<DxuiListView::Cell>>   rows (3, std::vector<DxuiListView::Cell> { { L"a", false }, { L"b", false } });
        bool                                           found = false;

        scaler.SetDpi (96);
        list.SetColumns     ({ DxuiListView::Column { L"A", 100 }, DxuiListView::Column { L"B", 60 } });
        list.SetShowHeader  (false);
        list.Layout         (RECT { 0, 0, 400, 300 }, scaler);
        list.SetRows        (std::move (rows));
        list.SetListFocused (true);
        list.SetSelectedRow (0);

        static_cast<IDxuiControl &> (list).Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.argb == theme.ContentSelection())
            {
                found = true;
                Assert::AreEqual (156.0f, call.width,  L"to the last column's edge, less 4 dip");
                Assert::AreEqual (26.0f,  call.height, L"2 dip in at top and bottom of a 30 dip row");
                Assert::AreEqual (2.0f,   call.y);
            }
        }

        Assert::IsTrue (found, L"the selected row is filled");
    }


    TEST_METHOD (SelectedKeyboardRow_FocusedPane_SelectionAndOutline)
    {
        Theme        t;
        DxuiRowLook  look = DxuiRowLook::Resolve (t, true, true, false, true);

        Assert::AreEqual (0xFF505050u, look.fill);
        Assert::AreEqual (0xFFC3C3C3u, look.edge);
    }


    TEST_METHOD (OtherSelectedRow_FocusedPane_MultiFillAndAccentOutline)
    {
        Theme        t;
        DxuiRowLook  look = DxuiRowLook::Resolve (t, true, false, false, true);

        Assert::AreEqual (0xFF626262u, look.fill);
        Assert::AreEqual (0xFF60CDFFu, look.edge);
    }


    TEST_METHOD (SelectedRow_PaneNotFocused_DarkerThanHover_NoOutline)
    {
        Theme        t;
        DxuiRowLook  look = DxuiRowLook::Resolve (t, true, true, false, false);

        Assert::AreEqual (0xFF333333u, look.fill);
        Assert::AreEqual (0u,          look.edge);
    }


    TEST_METHOD (SelectedAndHovered_PaneNotFocused_HoverFillAndOutline)
    {
        Theme        t;
        DxuiRowLook  look = DxuiRowLook::Resolve (t, true, true, true, false);

        Assert::AreEqual (0xFF4D4D4Du, look.fill);
        Assert::AreEqual (0xFFC3C3C3u, look.edge);
    }


    TEST_METHOD (KeyboardRowNotSelected_FocusedPane_OutlineOnly)
    {
        Theme        t;
        DxuiRowLook  look = DxuiRowLook::Resolve (t, false, true, false, true);

        Assert::AreEqual (0u,          look.fill);
        Assert::AreEqual (0xFFC3C3C3u, look.edge);
    }


    TEST_METHOD (HoveredOnly_HoverFillNoOutline)
    {
        Theme        t;
        DxuiRowLook  look = DxuiRowLook::Resolve (t, false, false, true, true);

        Assert::AreEqual (0xFF4D4D4Du, look.fill);
        Assert::AreEqual (0u,          look.edge);
    }
};
