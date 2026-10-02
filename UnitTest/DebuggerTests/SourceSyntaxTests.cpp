#include "Pch.h"

#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceSyntaxTests
    //
    //  Syntax colors in the source pane, for as65, Merlin and ca65 lines, and
    //  in the disassembly: the runs each line is read into, and that a view
    //  draws each run in its color.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceSyntaxTests)
    {
    public:
        using Token = SourceSyntax::Token;

        static std::wstring Describe (const std::wstring & line, const std::vector<SourceSyntax::Run> & runs)
        {
            static const wchar_t * const  s_kNames[] = { L"M", L"D", L"S", L"N", L"T", L"C" };
            std::wstring                  text;



            for (const SourceSyntax::Run & run : runs)
            {
                text += std::format (L"{}[{}] ", s_kNames[(int) run.token], line.substr ((size_t) run.start, (size_t) run.length));
            }

            return text;
        }



        TEST_METHOD (As65LinesAreRead)
        {
            std::wstring  line = L"start   lda #$41      ; letter A";



            Assert::AreEqual (std::wstring (L"S[start] M[lda] N[$41] C[; letter A] "), Describe (line, SourceSyntax::GetSourceRuns (line)));

            line = L"        sta $0400,x";
            Assert::AreEqual (std::wstring (L"M[sta] N[$0400] "), Describe (line, SourceSyntax::GetSourceRuns (line)), L"the register is not a symbol");

            line = L"*= $300";
            Assert::AreEqual (std::wstring (L"N[$300] "), Describe (line, SourceSyntax::GetSourceRuns (line)), L"a star that sets the origin");

            line = L"msg     .byte \"HI\",0";
            Assert::AreEqual (std::wstring (L"S[msg] D[.byte] T[\"HI\"] N[0] "), Describe (line, SourceSyntax::GetSourceRuns (line)));
        }



        TEST_METHOD (MerlinLinesAreRead)
        {
            std::wstring  line = L"* A comment";



            Assert::AreEqual (std::wstring (L"C[* A comment] "), Describe (line, SourceSyntax::GetSourceRuns (line)));

            line = L"]loop   DEX";
            Assert::AreEqual (std::wstring (L"S[]loop] M[DEX] "), Describe (line, SourceSyntax::GetSourceRuns (line)));

            line = L"        ORG $2000";
            Assert::AreEqual (std::wstring (L"D[ORG] N[$2000] "), Describe (line, SourceSyntax::GetSourceRuns (line)));

            line = L"        BNE ]loop";
            Assert::AreEqual (std::wstring (L"M[BNE] S[]loop] "), Describe (line, SourceSyntax::GetSourceRuns (line)));
        }



        TEST_METHOD (Ca65LinesAreRead)
        {
            std::wstring  line = L"  @wait: bit $C000";



            Assert::AreEqual (std::wstring (L"S[@wait:] M[bit] N[$C000] "), Describe (line, SourceSyntax::GetSourceRuns (line)));

            line = L".segment \"CODE\"";
            Assert::AreEqual (std::wstring (L"D[.segment] T[\"CODE\"] "), Describe (line, SourceSyntax::GetSourceRuns (line)));

            line = L"lda #<(table + 1)";
            Assert::AreEqual (std::wstring (L"M[lda] S[table] N[1] "), Describe (line, SourceSyntax::GetSourceRuns (line)), L"an opcode in the first column");
        }



        TEST_METHOD (AnInstructionIsAMnemonicAndAnOperand)
        {
            std::wstring  line = L"JSR COUT";



            Assert::AreEqual (std::wstring (L"M[JSR] S[COUT] "), Describe (line, SourceSyntax::GetInstructionRuns (line)));

            line = L"LDA ($06),Y";
            Assert::AreEqual (std::wstring (L"M[LDA] N[$06] "), Describe (line, SourceSyntax::GetInstructionRuns (line)));
            Assert::IsTrue   (SourceSyntax::IsMnemonic (L"bbr3"));
            Assert::IsFalse  (SourceSyntax::IsMnemonic (L"ORG"));
        }



        TEST_METHOD (TheSourcePaneColorsEachRun)
        {
            SourcePane::Style               style;
            std::vector<DxuiTextView::Row>  rows;



            style.syntax = { 0xFF000001, 0xFF000002, 0xFF000003, 0xFF000004, 0xFF000005, 0xFF000006 };
            rows = SourcePane::BuildRows ({ L"        lda #1 ; c" }, 0, {}, {}, style);

            Assert::AreEqual ((size_t) 3, rows[0].spans.size());
            Assert::AreEqual (2,           rows[0].spans[0].cell, L"in the text, not the number");
            Assert::AreEqual (0xFF000001u, rows[0].spans[0].argb);
            Assert::AreEqual (0xFF000004u, rows[0].spans[1].argb);
            Assert::AreEqual (0xFF000006u, rows[0].spans[2].argb);
        }



        TEST_METHOD (ATextViewDrawsASpanInItsColor)
        {
            DxuiTextView          view;
            DxuiTextView::Row     row;
            DxuiDpiScaler         scaler;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            bool                  found = false;



            row.cells = { L"lda #1" };
            row.spans = { { 0, 0, 3, 0xFF123456 } };
            scaler.SetDpi (96);
            view.SetCellSize (8, 16);
            view.Layout      (RECT { 0, 0, 400, 400 }, scaler);
            view.SetRows     ({ row });
            view.Paint       (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                found = found || (call.text == L"lda" && call.argb == 0xFF123456);
            }

            Assert::IsTrue (found);
        }



        TEST_METHOD (AListCellDrawsAColorRangeInItsColor)
        {
            DxuiListView                     list;
            DxuiListView::Cell               cell;
            DxuiDpiScaler                    scaler;
            MockDxuiPainter                  painter;
            MockDxuiTextRenderer             text;
            MockDxuiTheme                    theme;
            bool                             found = false;



            cell.text        = L"LDA #$41";
            cell.colorRanges = { { 0, 3, 0xFF123456 } };
            scaler.SetDpi (96);
            list.SetColumns ({ { L"Instruction", 200, false, DxuiTextHAlign::Left } });
            list.Layout     (RECT { 0, 0, 400, 400 }, scaler);
            list.SetRows    ({ { cell } });
            list.Paint      (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                found = found || (call.text == L"LDA" && call.argb == 0xFF123456);
            }

            Assert::IsTrue (found);
        }
    };
}
