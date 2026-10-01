#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceSyntax.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceListingSyntaxTests
    //
    //  The assembler from a file's text over its extension, a Merlin or ca65
    //  listing's address and bytes colored ahead of its source, and an
    //  operand's result shown after "Result: " in the result color.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceListingSyntaxTests)
    {
    public:
        using Assembler = SourceSyntax::Assembler;
        using Listing   = SourceSyntax::Listing;

        static std::wstring Describe (const std::wstring & line, Assembler assembler, Listing listing)
        {
            static const wchar_t * const  s_kNames[] = { L"M", L"D", L"S", L"N", L"T", L"C", L"A", L"B" };
            std::wstring                  text;



            for (const SourceSyntax::Run & run : SourceSyntax::GetLineRuns (line, assembler, listing))
            {
                text += std::format (L"{}[{}] ", s_kNames[(int) run.token], line.substr ((size_t) run.start, (size_t) run.length));
            }

            return text;
        }



        TEST_METHOD (AnOriginOrATieIsAs65)
        {
            std::vector<std::wstring>  as65 = { L"        *= $300", L"start   lda   #0", L"        rts" };
            std::vector<std::wstring>  tied = { L"]loop   LDA   #0", L"@next:  rts" };



            Assert::IsTrue (Assembler::As65 == SourceSyntax::DetectAssembler (as65), L"an as65 origin");
            Assert::IsTrue (Assembler::As65 == SourceSyntax::DetectAssembler (tied), L"a tie of signs goes to as65");
        }



        TEST_METHOD (AListingIsRecognizedByItsRows)
        {
            std::vector<std::wstring>  merlin = { L"                1    * PI",
                                                  L"8016: 38        23            SEC",
                                                  L"8017: A5 15     24            LDA   DEFLEN+1" };
            std::vector<std::wstring>  ca65   = { L"ca65 V2.19",
                                                  L"",
                                                  L"000000r 1                       .proc main",
                                                  L"000000r 1  A9 41        @loop:  lda #$41",
                                                  L"000005r 1  4C rr rr             jmp @loop" };
            std::vector<std::wstring>  plain  = { L"start   lda   #0", L"        rts" };



            Assert::IsTrue (Listing::Merlin == SourceSyntax::DetectListing (merlin));
            Assert::IsTrue (Listing::Ca65   == SourceSyntax::DetectListing (ca65), L"a heading line does not hide a listing");
            Assert::IsTrue (Listing::None   == SourceSyntax::DetectListing (plain));
            Assert::IsTrue (Assembler::Merlin == SourceSyntax::DetectAssembler (merlin), L"read from the listing's source");
            Assert::IsTrue (Assembler::Ca65   == SourceSyntax::DetectAssembler (ca65));
        }



        TEST_METHOD (AListingsAddressAndBytesAreColored)
        {
            Assert::AreEqual (std::wstring (L"A[8017] B[A5 15] M[LDA] S[DEFLEN] N[1] "),
                              Describe (L"8017: A5 15     24            LDA   DEFLEN+1", Assembler::Merlin, Listing::Merlin));
            Assert::AreEqual (std::wstring (L"C[* PI] "),
                              Describe (L"                1    * PI", Assembler::Merlin, Listing::Merlin));
            Assert::AreEqual (std::wstring (L"A[000005] B[4C rr rr] M[jmp] S[@loop] "),
                              Describe (L"000005r 1  4C rr rr             jmp @loop", Assembler::Ca65, Listing::Ca65));
            Assert::AreEqual (std::wstring(), Describe (L"ca65 V2.19", Assembler::Ca65, Listing::Ca65), L"a heading is not a row");
        }



        TEST_METHOD (TheSourcePaneColorsAListingsColumns)
        {
            SourcePane::Style               style;
            std::vector<DxuiTextView::Row>  rows;



            style.syntax.mnemonic = 0xFF000001;
            style.syntax.address  = 0xFF000002;
            style.syntax.bytes    = 0xFF000003;
            rows = SourcePane::BuildRows ({ L"8016: 38        23            SEC" }, 0, {}, {}, style, {},
                                          Assembler::Merlin, Listing::Merlin);

            Assert::IsTrue (std::any_of (rows[0].spans.begin(), rows[0].spans.end(), [] (const DxuiTextView::Span & span)
                            {
                                return span.cell == 2 && span.start == 0 && span.length == 4 && span.argb == 0xFF000002u;
                            }), L"the address");
            Assert::IsTrue (std::any_of (rows[0].spans.begin(), rows[0].spans.end(), [] (const DxuiTextView::Span & span)
                            {
                                return span.cell == 2 && span.start == 6 && span.length == 2 && span.argb == 0xFF000003u;
                            }), L"the bytes");
            Assert::AreEqual (std::wstring (L"8016: 38        23            SEC"), rows[0].cells[2], L"nothing is stripped");
        }



        TEST_METHOD (TheDisassemblysResultIsLabeledAndColored)
        {
            DxuiListView::Cell  cell = DebuggerWindow::GetOperandAndResultCell ("A=00", "A=41", 0xFF00FFFF);



            Assert::AreEqual (std::wstring (L"A=00  Result: A=41"), cell.text);
            Assert::AreEqual ((size_t) 1, cell.colorRanges.size());
            Assert::AreEqual (6,  std::get<0> (cell.colorRanges[0]));
            Assert::AreEqual (18, std::get<1> (cell.colorRanges[0]));
            Assert::AreEqual (0xFF00FFFFu, std::get<2> (cell.colorRanges[0]));
            Assert::IsTrue   (cell.dimRanges.empty(), L"the result is not muted");
        }



        TEST_METHOD (TheSourcesResultIsLabeledAndColored)
        {
            SourcePane::Style               style;
            std::vector<DxuiTextView::Row>  rows;



            style.resultArgb = 0xFF00FFFF;
            rows = SourcePane::BuildRows ({ L"  lda $10" }, 1, {}, {}, style, { { 1, { L"$10: 07", L"A=07" } } });

            Assert::AreEqual (std::wstring (L"$10: 07  Result: A=07"), rows[0].cells[3]);
            Assert::IsTrue   (std::any_of (rows[0].spans.begin(), rows[0].spans.end(), [] (const DxuiTextView::Span & span)
                              {
                                  return span.cell == 3 && span.start == 9 && span.length == 12 && span.argb == 0xFF00FFFFu;
                              }), L"the result color over the label and the result");
        }
    };
}
