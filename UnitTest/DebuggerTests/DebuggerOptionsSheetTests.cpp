#include "Pch.h"

#include "Ui/Debugger/DebuggerOptionsSheet.h"
#include "Ui/Debugger/DisassemblyOptionsPage.h"
#include "Ui/Debugger/HistoryOptionsPage.h"
#include "Ui/Chrome/CassoTheme.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using Option = DisassemblyOptions::Option;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheetTests
//
//  The debugger's Options sheet: its History and Disassembly pages are
//  filled from the options passed in and return the user's changes, and a
//  saved budget above the slider's range is returned as saved until the
//  slider moves; the Disassembly page's preview follows its check boxes.
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
        sheet.GetDisassemblyPage()->GetCheckbox (Option::Pointers).SetChecked (false);

        Assert::IsTrue  (sheet.GetOptions().disassembly.IsOn (Option::AddressColon));
        Assert::IsFalse (sheet.GetOptions().disassembly.IsOn (Option::Pointers));
        Assert::IsFalse (sheet.GetOptions().disassembly.IsOn (Option::CodeBytes), L"an option the user left alone");
    }

    //  The Disassembly page laid out at 96 DPI and its preview painted.
    static DisassemblyPreview & PaintPreview (DebuggerOptionsSheet & sheet, MockDxuiPainter & painter, const CassoTheme & theme)
    {
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;



        scaler.SetDpi (96);
        painter.Reset();
        sheet.GetDisassemblyPage()->Layout (RECT { 0, 0, 700, 600 }, scaler);
        sheet.GetDisassemblyPage()->GetPreview().Paint (painter, text, theme);

        return sheet.GetDisassemblyPage()->GetPreview();
    }


    //  A press of Space on a focused check box, as the user toggles it.
    static void  Toggle (DxuiCheckbox & box)
    {
        DxuiKeyEvent  ev;



        box.OnFocusChanged (true);

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = VK_SPACE;
        (void) box.OnKey (ev);
    }


    static int  CountLines (const MockDxuiPainter & painter)
    {
        return (int) std::count_if (painter.Calls().begin(), painter.Calls().end(),
                                    [] (const RecordedPaintCall & call) { return call.kind == RecordedPaintKind::DrawLine; });
    }


    TEST_METHOD (ThePreviewShowsEachOptionAsChecked)
    {
        CassoTheme             theme   = CassoTheme::MakeSkeuomorphic();
        DebuggerOptionsSheet   sheet;
        DebuggerOptions        current;
        ReverseBudgetRange     range   = { 1, 1000, 99 };
        MockDxuiPainter        painter;
        DisassemblyPreview   * preview = nullptr;
        int                    lda     = 0;



        sheet.BuildForTest (current, range);
        preview = &PaintPreview (sheet, painter, theme);
        lda     = DisassemblyOptions::GetRowOfLine (preview->GetSampleRows(), 1);

        Assert::AreEqual ((size_t) 12, preview->GetSampleRows().size(),                              L"each instruction under its source line");
        Assert::AreEqual (std::wstring (L"Y=05 ($003E)=$1230 $1235=C1"), preview->GetCellsOfRow (lda)[5].text, L"the default: both");
        Assert::AreEqual (std::wstring (L"0302"),           preview->GetCellsOfRow (lda)[1].text,    L"the default: no colon");
        Assert::AreEqual (std::wstring (L"LDA (PTR),Y"),    preview->GetCellsOfRow (lda)[4].text);

        Toggle (sheet.GetDisassemblyPage()->GetCheckbox (Option::AddressColon));
        Toggle (sheet.GetDisassemblyPage()->GetCheckbox (Option::Symbols));
        Toggle (sheet.GetDisassemblyPage()->GetCheckbox (Option::OperandValues));
        preview = &PaintPreview (sheet, painter, theme);

        Assert::AreEqual (std::wstring (L"Y=05 ($003E)=$1230"), preview->GetCellsOfRow (lda)[5].text, L"the pointer, without the operand's value");
        Assert::AreEqual (std::wstring (L"0302:"),              preview->GetCellsOfRow (lda)[1].text);
        Assert::AreEqual (std::wstring (L"LDA ($3E),Y"),        preview->GetCellsOfRow (lda)[4].text, L"no symbols");

        Toggle (sheet.GetDisassemblyPage()->GetCheckbox (Option::Source));
        preview = &PaintPreview (sheet, painter, theme);

        Assert::AreEqual ((size_t) 6, preview->GetSampleRows().size(), L"no source lines");
    }


    TEST_METHOD (ThePreviewDrawsTheBranchArrowWhileItIsChecked)
    {
        CassoTheme            theme     = CassoTheme::MakeSkeuomorphic();
        DebuggerOptionsSheet  sheet;
        DebuggerOptions       current;
        ReverseBudgetRange    range     = { 1, 1000, 99 };
        MockDxuiPainter       painter;
        int                   withArrow = 0;



        sheet.BuildForTest (current, range);
        (void) PaintPreview (sheet, painter, theme);
        withArrow = CountLines (painter);

        Toggle (sheet.GetDisassemblyPage()->GetCheckbox (Option::BranchArrow));
        (void) PaintPreview (sheet, painter, theme);

        Assert::IsTrue (withArrow > CountLines (painter), L"the arrow's segments go with the check");
    }

    //  A row's address color with the given choice of what to dim.
    static uint32_t  GetAddressArgb (DebuggerOptionsSheet & sheet, DisassemblyOptions::Dim dim, int row, const CassoTheme & theme)
    {
        DebuggerOptions   options;
        MockDxuiPainter   painter;



        options.disassembly.SetDim (dim);
        sheet.GetDisassemblyPage()->SetOptions (options.disassembly);

        return PaintPreview (sheet, painter, theme).GetCellsOfRow (row)[1].argb;
    }


    TEST_METHOD (ThePreviewDimsTheChosenRowsButNotThePcRow)
    {
        CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
        DebuggerOptionsSheet  sheet;
        DebuggerOptions       current;
        ReverseBudgetRange    range  = { 1, 1000, 99 };
        int                   lda    = 0;
        int                   pc     = 0;
        int                   source = 0;
        MockDxuiPainter       painter;



        sheet.BuildForTest (current, range);
        (void) PaintPreview (sheet, painter, theme);
        lda    = DisassemblyOptions::GetRowOfLine (sheet.GetDisassemblyPage()->GetPreview().GetSampleRows(), 1);
        pc     = DisassemblyOptions::GetRowOfLine (sheet.GetDisassemblyPage()->GetPreview().GetSampleRows(), 4);
        source = lda - 1;

        Assert::AreNotEqual (GetAddressArgb (sheet, DisassemblyOptions::Dim::None, lda, theme),
                             GetAddressArgb (sheet, DisassemblyOptions::Dim::Code, lda, theme),    L"a disassembly row");
        Assert::AreEqual    (GetAddressArgb (sheet, DisassemblyOptions::Dim::None, pc, theme),
                             GetAddressArgb (sheet, DisassemblyOptions::Dim::Code, pc, theme),     L"the PC's row");
        Assert::AreEqual    (GetAddressArgb (sheet, DisassemblyOptions::Dim::None, source, theme),
                             GetAddressArgb (sheet, DisassemblyOptions::Dim::Code, source, theme), L"a source row, with disassembly dimmed");
        Assert::AreNotEqual (GetAddressArgb (sheet, DisassemblyOptions::Dim::None, source, theme),
                             GetAddressArgb (sheet, DisassemblyOptions::Dim::Source, source, theme), L"a source row, with source dimmed");
    }
};