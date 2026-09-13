#include "Pch.h"

#include "Core/DxuiStandardCommand.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiListView.h"
#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandAnswerer
//
//  A control that handles one standard command and nothing else, so a test can
//  place it at a chosen depth and check that the router delivers the command.
//
////////////////////////////////////////////////////////////////////////////////

class CommandAnswerer : public MockDxuiControl
{
public:
    DxuiStandardCommand  answers = DxuiStandardCommand::None;
    bool                 enabled = true;
    int                  invoked = 0;

    bool  QueryCommand (DxuiStandardCommand command, bool & outEnabled) const override
    {
        if (command != answers)
        {
            return false;
        }

        outEnabled = enabled;
        return true;
    }

    bool  InvokeCommand (DxuiStandardCommand command) override
    {
        if (command != answers)
        {
            return false;
        }

        invoked++;
        return true;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStandardCommandTests
//
//  The keystroke for each command, how far up the parent chain the router
//  goes, and which commands each widget handles.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiStandardCommandTests)
{
public:

    TEST_METHOD (TranslateKey_TheWindowsKeystrokesAndTheirOlderSpellings)
    {
        using Command = DxuiStandardCommand;

        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('C', true, false, false) == Command::Copy);
        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('X', true, false, false) == Command::Cut);
        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('V', true, false, false) == Command::Paste);
        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('A', true, false, false) == Command::SelectAll);
        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('Z', true, false, false) == Command::Undo);
        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('Z', true, false, true)  == Command::Redo);

        Assert::IsTrue (DxuiCommandRouter::TranslateKey (VK_INSERT, true,  false, false) == Command::Copy,
            L"Ctrl+Insert is the older spelling of Copy");
        Assert::IsTrue (DxuiCommandRouter::TranslateKey (VK_INSERT, false, false, true)  == Command::Paste,
            L"and Shift+Insert of Paste");

        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('C', false, false, false) == Command::None,
            L"An unmodified letter is typing, not a command");
        Assert::IsTrue (DxuiCommandRouter::TranslateKey ('C', true,  true,  false) == Command::None,
            L"and Alt makes it the application's, not the library's");
    }


    TEST_METHOD (Invoke_StopsAtTheFirstControlThatAnswers)
    {
        CommandAnswerer  outer;
        CommandAnswerer  inner;


        inner.SetParent (&outer);
        outer.answers = DxuiStandardCommand::Copy;
        inner.answers = DxuiStandardCommand::Copy;

        Assert::IsTrue   (DxuiCommandRouter::Invoke (&inner, DxuiStandardCommand::Copy));
        Assert::AreEqual (1, inner.invoked, L"The focused control answers first");
        Assert::AreEqual (0, outer.invoked, L"and the one containing it is not asked");
    }


    TEST_METHOD (Invoke_CarriesOnOutToAControlThatDoesAnswer)
    {
        CommandAnswerer  outer;
        CommandAnswerer  inner;


        inner.SetParent (&outer);
        outer.answers = DxuiStandardCommand::Copy;
        inner.answers = DxuiStandardCommand::SelectAll;

        Assert::IsTrue   (DxuiCommandRouter::Invoke (&inner, DxuiStandardCommand::Copy));
        Assert::AreEqual (1, outer.invoked,
            L"A control that does not know the command lets the one containing it answer");
    }


    TEST_METHOD (Query_SeparatesGrayedFromUnclaimed)
    {
        CommandAnswerer  control;
        bool             enabled = false;


        control.answers = DxuiStandardCommand::Copy;
        control.enabled = false;

        Assert::IsTrue  (DxuiCommandRouter::Query (&control, DxuiStandardCommand::Copy, enabled),
            L"The command is claimed");
        Assert::IsFalse (enabled, L"but cannot be run right now, so the row is grayed");

        Assert::IsFalse (DxuiCommandRouter::Query (&control, DxuiStandardCommand::Paste, enabled),
            L"Nothing claims paste at all");
    }


    TEST_METHOD (Query_NothingFocusedClaimsNothing)
    {
        bool  enabled = true;

        Assert::IsFalse (DxuiCommandRouter::Query (nullptr, DxuiStandardCommand::Copy, enabled));
        Assert::IsFalse (enabled, L"and the row is not left enabled from a previous answer");
        Assert::IsFalse (DxuiCommandRouter::Invoke (nullptr, DxuiStandardCommand::Copy));
    }


    TEST_METHOD (ListView_ClaimsSelectAllOnlyWhenItHoldsMoreThanOne)
    {
        DxuiListView  list;
        bool          enabled = false;


        list.SetRows ({ { DxuiListView::Cell { L"a", false } },
                        { DxuiListView::Cell { L"b", false } } });

        Assert::IsFalse (list.QueryCommand (DxuiStandardCommand::SelectAll, enabled),
            L"A single-selection list has no select all to offer");

        list.SetMultiSelect (true);

        Assert::IsTrue (list.QueryCommand (DxuiStandardCommand::SelectAll, enabled));
        Assert::IsTrue (enabled, L"and a list with rows can run it");

        Assert::IsFalse (list.QueryCommand (DxuiStandardCommand::Copy, enabled),
            L"What Copy means over a set of rows is the host's, so the list does not claim it");
    }


    TEST_METHOD (HexView_ClaimsCopyAndSelectAllButNotPaste)
    {
        DxuiHexView  view;
        bool         enabled = true;


        Assert::IsTrue  (view.QueryCommand (DxuiStandardCommand::Copy, enabled));
        Assert::IsFalse (enabled, L"Copy is the view's, and grayed with nothing selected");

        Assert::IsTrue  (view.QueryCommand (DxuiStandardCommand::SelectAll, enabled));
        Assert::IsFalse (enabled, L"as is select all, grayed with no bytes to select");

        Assert::IsFalse (view.QueryCommand (DxuiStandardCommand::Paste, enabled),
            L"The bytes cannot be written, so paste is not the view's to answer");
    }
};
