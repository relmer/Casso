#include "Pch.h"

#include "Ui/Debugger/DebuggerOptionsSheet.h"
#include "Ui/Debugger/DisassemblyOptionsPage.h"
#include "Ui/Debugger/HistoryOptionsPage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using Option = DisassemblyOptions::Option;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheetTests
//
//  The debugger's Options sheet: its History and Disassembly pages are
//  filled from the options passed in and return the user's changes, and a
//  saved budget above the slider's range is returned as saved until the
//  slider moves.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerOptionsSheetTests)
{
public:

    TEST_METHOD (TheSheetHasAHistoryPageAndADisassemblyPage)
    {
        DebuggerOptionsSheet  sheet;
        DebuggerOptions       current;
        ReverseBudgetRange    range   = { 1, 1000, 99 };



        sheet.BuildForTest (current, range);

        Assert::IsNotNull (sheet.GetHistoryPage());
        Assert::IsNotNull (sheet.GetDisassemblyPage());
        Assert::AreEqual  (std::wstring (L"History"),     sheet.GetHistoryPage()->GetTitle());
        Assert::AreEqual  (std::wstring (L"Disassembly"), sheet.GetDisassemblyPage()->GetTitle());
    }


    TEST_METHOD (TheHistoryPageReturnsItsSettings)
    {
        DebuggerOptionsSheet  sheet;
        DebuggerOptions       current;
        ReverseBudgetRange    range   = { 1, 1000, 99 };



        current.reverse.isRecording = true;
        current.reverse.budgetMb    = 64;
        sheet.BuildForTest (current, range);

        Assert::AreEqual (64.0f,   sheet.GetHistoryPage()->GetBudgetSlider().GetValue());
        Assert::AreEqual (1000.0f, sheet.GetHistoryPage()->GetBudgetSlider().GetMax());

        sheet.GetHistoryPage()->GetRecordCheckbox().SetChecked (false);
        sheet.GetHistoryPage()->GetBudgetSlider().SetValue (128.0f);

        Assert::IsFalse  (sheet.GetOptions().reverse.isRecording);
        Assert::AreEqual (128, sheet.GetOptions().reverse.budgetMb);
    }


    TEST_METHOD (ASavedBudgetAboveTheRangeIsReturnedUnlessTheSliderMoves)
    {
        DebuggerOptionsSheet  sheet;
        DebuggerOptions       current;
        ReverseBudgetRange    range   = { 1, 1000, 99 };



        current.reverse.budgetMb = 4096;
        sheet.BuildForTest (current, range);

        Assert::AreEqual (1000.0f, sheet.GetHistoryPage()->GetBudgetSlider().GetValue(), L"the slider sits at its maximum");
        Assert::AreEqual (4096,    sheet.GetOptions().reverse.budgetMb,                  L"the saved value is returned");

        sheet.GetHistoryPage()->GetBudgetSlider().SetValue (900.0f);

        Assert::AreEqual (900, sheet.GetOptions().reverse.budgetMb, L"the slider's value once it moves");
    }


    TEST_METHOD (TheDisassemblyPageHasEveryOptionAndReturnsItsSettings)
    {
        DebuggerOptionsSheet  sheet;
        DebuggerOptions       current;
        ReverseBudgetRange    range   = { 1, 1000, 99 };
        int                   i       = 0;



        current.disassembly.Set (Option::CodeBytes, false);
        sheet.BuildForTest (current, range);

        for (i = 0; i < DisassemblyOptions::kOptionCount; i++)
        {
            Assert::AreEqual (current.disassembly.IsOn ((Option) i),
                              sheet.GetDisassemblyPage()->GetCheckbox ((Option) i).IsChecked(),
                              DisassemblyOptions::GetLabel ((Option) i));
        }

        sheet.GetDisassemblyPage()->GetCheckbox (Option::AddressColon).SetChecked (true);
        sheet.GetDisassemblyPage()->GetCheckbox (Option::Pointers).SetChecked (true);

        Assert::IsTrue  (sheet.GetOptions().disassembly.IsOn (Option::AddressColon));
        Assert::IsTrue  (sheet.GetOptions().disassembly.IsOn (Option::Pointers));
        Assert::IsFalse (sheet.GetOptions().disassembly.IsOn (Option::CodeBytes), L"an option the user left alone");
    }
};
