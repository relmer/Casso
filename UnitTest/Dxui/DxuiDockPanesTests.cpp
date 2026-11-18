#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockPanesTests
//
//  A dock site's panes by name: each pane's control and title, its tab's
//  marks, and the setters' results for a pane never added or a value the
//  pane has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiDockPanesTests)
{
public:

    TEST_METHOD (Add_FindsThePaneByNameAndByControl)
    {
        DxuiDockPanes    panes;
        MockDxuiControl  code;
        MockDxuiControl  regs;



        panes.Add (L"code", L"Disassembly", &code);
        panes.Add (L"regs", L"Registers",   &regs);

        Assert::IsNotNull (panes.Find (L"code"));
        Assert::IsTrue    (panes.GetContent (L"regs") == &regs);
        Assert::AreEqual  (std::wstring (L"regs"), panes.GetPaneOf (&regs));
        Assert::AreEqual  (std::wstring (L"Disassembly"), panes.GetTitle (L"code"));
    }


    TEST_METHOD (APaneNeverAdded_IsFoundNowhere)
    {
        DxuiDockPanes    panes;
        MockDxuiControl  stray;



        Assert::IsNull   (panes.Find (L"code"));
        Assert::IsNull   (panes.GetContent (L"code"));
        Assert::AreEqual (std::wstring(), panes.GetPaneOf (&stray));
        Assert::AreEqual (std::wstring(), panes.GetPaneOf (nullptr));
        Assert::AreEqual (std::wstring (L"code"), panes.GetTitle (L"code"), L"the title falls back to the pane's name");
        Assert::AreEqual (std::wstring(), panes.GetTip (L"code"));
        Assert::IsFalse  (panes.HasIndicator (L"code"));
    }


    TEST_METHOD (Setters_ReturnFalseForAPaneNeverAdded)
    {
        DxuiDockPanes  panes;



        Assert::IsFalse (panes.SetTitle       (L"code", L"Code"));
        Assert::IsFalse (panes.SetIndicator   (L"code", true));
        Assert::IsFalse (panes.SetLeadingMark (L"code", DxuiTabGroup::LeadingMark()));
        Assert::IsFalse (panes.SetTabTip      (L"code", L"tip"));
        Assert::IsFalse (panes.SetTitleExtra  (L"code", 40));
    }


    TEST_METHOD (Setters_KeepTheValuesForThePane)
    {
        DxuiDockPanes    panes;
        MockDxuiControl  code;



        panes.Add (L"code", L"Disassembly", &code);

        Assert::IsTrue (panes.SetTitle      (L"code", L"Code"));
        Assert::IsTrue (panes.SetIndicator  (L"code", true));
        Assert::IsTrue (panes.SetTabTip     (L"code", L"C:\\code.s"));
        Assert::IsTrue (panes.SetTitleExtra (L"code", 40));

        Assert::AreEqual (std::wstring (L"Code"), panes.GetTitle (L"code"));
        Assert::IsTrue   (panes.HasIndicator (L"code"));
        Assert::AreEqual (std::wstring (L"C:\\code.s"), panes.GetTip (L"code"));
        Assert::AreEqual (40, panes.Find (L"code")->titleExtraDip);
    }


    //  A title extra or leading mark set to the value the pane has changes
    //  nothing, so the site skips laying out again.
    TEST_METHOD (SameTitleExtraOrMark_ReturnsFalse)
    {
        DxuiDockPanes              panes;
        MockDxuiControl            code;
        DxuiTabGroup::LeadingMark  mark;



        mark.glyph = L"*";

        panes.Add (L"code", L"Disassembly", &code);

        Assert::IsTrue  (panes.SetTitleExtra (L"code", 40));
        Assert::IsFalse (panes.SetTitleExtra (L"code", 40));
        Assert::IsFalse (panes.SetLeadingMark (L"code", DxuiTabGroup::LeadingMark()), L"a pane starts with no mark");
        Assert::IsTrue  (panes.SetLeadingMark (L"code", mark));
        Assert::IsFalse (panes.SetLeadingMark (L"code", mark));
    }
};
