#include "Pch.h"

#include "DialectProfile.h"
#include "DialectRegistry.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceSyntax.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceSyntaxAssemblerTests
    //
    //  Source colored with the grammar of the assembler that wrote it: Merlin's
    //  comment field without a semicolon, its local labels, string delimiters
    //  and HEX operands, ca65's unnamed label references, and which assembler
    //  a file's text and extension point to.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceSyntaxAssemblerTests)
    {
    public:
        using Assembler = SourceSyntax::Assembler;

        static std::wstring Describe (const std::wstring & line, Assembler assembler)
        {
            static const wchar_t * const  s_kNames[] = { L"M", L"D", L"S", L"N", L"T", L"C" };
            std::wstring                  text;



            for (const SourceSyntax::Run & run : SourceSyntax::GetSourceRuns (line, assembler))
            {
                text += std::format (L"{}[{}] ", s_kNames[(int) run.token], line.substr ((size_t) run.start, (size_t) run.length));
            }

            return text;
        }



        TEST_METHOD (MerlinCommentFieldNeedsNoSemicolon)
        {
            Assert::AreEqual (std::wstring (L"M[LDA] N[0] C[clear it] "),
                              Describe (L"        LDA   0       clear it", Assembler::Merlin));
            Assert::AreEqual (std::wstring (L"S[DONE] M[RTS] C[;back] "),
                              Describe (L"DONE    RTS           ;back", Assembler::Merlin));
            Assert::AreEqual (std::wstring (L"C[* a comment] "), Describe (L"* a comment", Assembler::Merlin));
        }



        TEST_METHOD (MerlinLocalLabelsAreSymbols)
        {
            Assert::AreEqual (std::wstring (L"S[:loop] M[BNE] S[:loop] "), Describe (L":loop   BNE   :loop", Assembler::Merlin));
            Assert::AreEqual (std::wstring (L"S[]ptr] D[=] N[$06] "), Describe (L"]ptr    =     $06", Assembler::Merlin));
        }



        TEST_METHOD (MerlinDirectiveOperandsFollowTheDirective)
        {
            Assert::AreEqual (std::wstring (L"D[ASC] T[!say \"hi\"!] "), Describe (L"        ASC   !say \"hi\"!", Assembler::Merlin),
                              L"a string runs to its own delimiter");
            Assert::AreEqual (std::wstring (L"D[HEX] N[FF00] "), Describe (L"        HEX   FF00", Assembler::Merlin),
                              L"a HEX operand is all digits");
        }



        TEST_METHOD (Ca65UnnamedLabelReferencesAreSymbols)
        {
            Assert::AreEqual (std::wstring (L"M[bne] S[:+] "), Describe (L"        bne   :+", Assembler::Ca65));
            Assert::AreEqual (std::wstring (L"S[@wait:] M[bit] N[$C000] "), Describe (L"@wait:  bit   $C000", Assembler::Ca65));
        }



        TEST_METHOD (As65StarInTheFirstColumnIsNotAComment)
        {
            Assert::AreEqual (std::wstring (L"S[start] M[lda] N[$41] C[; A] "), Describe (L"start   lda #$41 ; A", Assembler::As65));
            Assert::AreNotEqual (std::wstring (L"C[* 2] "), Describe (L"* 2", Assembler::As65));
        }



        TEST_METHOD (TheAssemblerComesFromTheText)
        {
            std::vector<std::wstring>  merlin = { L"* CLOCK", L"        ORG   $300", L"]loop   LDA   #0", L"        ASC   \"HI\"" };
            std::vector<std::wstring>  ca65   = { L"        .proc main", L"@loop:  lda   #0", L"        .endproc" };
            std::vector<std::wstring>  plain  = { L"start   lda   #0", L"        rts" };



            Assert::IsTrue (Assembler::Merlin == SourceSyntax::DetectAssembler (merlin));
            Assert::IsTrue (Assembler::Ca65   == SourceSyntax::DetectAssembler (ca65));
            Assert::IsTrue (Assembler::As65   == SourceSyntax::DetectAssembler (plain));
        }



        TEST_METHOD (TheSourcePaneColorsWithTheAssemblerGiven)
        {
            SourcePane::Style               style;
            std::vector<DxuiTextView::Row>  rows;



            style.syntax = { 0xFF000001, 0xFF000002, 0xFF000003, 0xFF000004, 0xFF000005, 0xFF000006 };
            rows = SourcePane::BuildRows ({ L"        LDA   0       clear it" }, 0, {}, {}, style, {}, Assembler::Merlin);

            Assert::AreEqual ((size_t) 3,  rows[0].spans.size());
            Assert::AreEqual (0xFF000006u, rows[0].spans[2].argb, L"the comment field");
        }



        TEST_METHOD (MerlinParseLineRecordsTheCommentColumn)
        {
            const DialectProfile  & merlin = DialectRegistry::Get (DialectId::Merlin);



            Assert::AreEqual (15, merlin.ParseLine ("LABEL LDA #1  note", 1).commentColumn);
            Assert::AreEqual (1,  merlin.ParseLine ("* note", 1).commentColumn);
            Assert::AreEqual (0,  merlin.ParseLine ("      RTS", 1).commentColumn);
        }
    };
}
