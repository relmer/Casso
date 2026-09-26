#include "Pch.h"

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
