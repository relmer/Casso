#include "Pch.h"
#include "../Dxui/MockDxuiTheme.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "ApplesoftTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSourcePreviewTests
//
//  An assembler's source in the preview: each line with its number, right
//  aligned, and its syntax colored by the grammar SourceSyntax reads.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerSourcePreviewTests)
{
public:

    TEST_METHOD (SourceRows_HaveTheirNumbersAndTheirSyntaxColors)
    {
        PreviewContent                  preview;
        SourceSyntax::Colors            colors;
        std::vector<DxuiTextView::Row>  rows;
        bool                            mnemonic = false;
        bool                            comment  = false;

        preview.kind   = PreviewContent::Kind::Text;
        preview.source = true;

        for (int i = 0; i < 9; i++)
        {
            preview.lines.push_back (L"; line");
        }

        preview.lines.push_back (L"start   LDA #$01    ; one");

        colors.mnemonic = 0xFF000001;
        colors.comment  = 0xFF000002;
        colors.number   = 0xFF000003;
        colors.symbol   = 0xFF000004;

        rows = PreviewText::BuildTextRows (preview, false, colors);

        Assert::AreEqual ((size_t) 10, rows.size());
        Assert::AreEqual (std::wstring (L" 1"), rows[0].cells[0], L"Numbers are right aligned to the widest");
        Assert::AreEqual (std::wstring (L"10"), rows[9].cells[0]);
        Assert::AreEqual (preview.lines[9], rows[9].cells[1], L"and the line follows its number");

        for (const DxuiTextView::Span & span : rows[9].spans)
        {
            Assert::AreEqual (1, span.cell, L"Every span is in the line's cell");

            mnemonic = mnemonic || (span.argb == colors.mnemonic && rows[9].cells[1].substr ((size_t) span.start, (size_t) span.length) == L"LDA");
            comment  = comment  || (span.argb == colors.comment  && rows[9].cells[1].substr ((size_t) span.start, (size_t) span.length) == L"; one");
        }

        Assert::IsTrue (mnemonic, L"The mnemonic is in its color");
        Assert::IsTrue (comment,  L"and the comment in its own");
        Assert::IsTrue (PreviewText::BuildTextRows (preview, false)[9].spans.empty(), L"Without colors, no spans");
    }



    TEST_METHOD (BasicListingRows_HaveTheirNumberAndStatementsColored)
    {
        PreviewContent                  preview;
        SourceSyntax::Colors            colors;
        std::vector<DxuiTextView::Row>  rows;
        std::vector<std::wstring>       colored;

        preview.kind  = PreviewContent::Kind::Listing;
        preview.lines = { L"10  PRINT \"HI\";SCORE: GOTO 10: REM LOOP" };

        colors.mnemonic = 0xFF000001;
        colors.comment  = 0xFF000002;
        colors.number   = 0xFF000003;
        colors.symbol   = 0xFF000004;
        colors.address  = 0xFF000005;
        colors.string   = 0xFF000007;

        rows = PreviewText::BuildTextRows (preview, false, colors);

        Assert::AreEqual ((size_t) 2, rows[0].cells.size());

        for (const DxuiTextView::Span & span : rows[0].spans)
        {
            colored.push_back (std::format (L"{:X}:{}", span.argb & 0xFF, rows[0].cells[(size_t) span.cell].substr ((size_t) span.start, (size_t) span.length)));
        }

        Assert::IsTrue (colored == std::vector<std::wstring> { L"5:10", L"1:PRINT", L"7:\"HI\"", L"4:SCORE", L"1:GOTO", L"3:10", L"2:REM LOOP" },
                        L"The number, then keywords, a string, a variable that starts like a keyword, a number and the REM");
    }



    //  A program's cross-reference is a row a line of it; a listing with no
    //  program bytes has its lines.
    TEST_METHOD (CrossReferenceRows_ListTheProgramsVariablesAndLineReferences)
    {
        PreviewContent                  preview;
        ApplesoftListingError           error;
        std::vector<DxuiTextView::Row>  rows;
        std::wstring                    text;

        preview.kind  = PreviewContent::Kind::Listing;
        preview.lines = { L"10  TALLY = 1", L"20  GOTO 10" };

        rows = PreviewText::BuildCrossReferenceRows (preview, false);

        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::AreEqual ((size_t) 2, rows[0].cells.size(), L"The line number and the line");

        AssertSucceeded (ApplesoftTokenizer::Tokenize ("10 TALLY = 1\n20 GOTO 10\n", preview.bytes, error));

        rows = PreviewText::BuildCrossReferenceRows (preview, false);

        for (const DxuiTextView::Row & row : rows)
        {
            Assert::AreEqual ((size_t) 1, row.cells.size());

            text += row.cells[0] + L"\n";
        }

        Assert::IsTrue (text == L"Applesoft BASIC\n\nVariables\n  TALLY  10\n\nLine references\n  10  20\n\nCALL, PEEK and POKE\n  none\n", text.c_str());
    }



    TEST_METHOD (DisassemblyRows_HaveTheirAddressBytesAndInstructionColored)
    {
        PreviewContent                  preview;
        SourceSyntax::Colors            colors;
        std::vector<DxuiTextView::Row>  rows;
        std::wstring                    line = L"2000  A9 01     LDA #$01";
        std::vector<std::wstring>       colored;

        preview.kind        = PreviewContent::Kind::Hex;
        preview.disassembly = true;
        preview.lines       = { line };

        colors.mnemonic = 0xFF000001;
        colors.number   = 0xFF000003;
        colors.address  = 0xFF000005;
        colors.bytes    = 0xFF000006;

        rows = PreviewText::BuildTextRows (preview, false, colors);

        Assert::AreEqual ((size_t) 1, rows.size());
        Assert::AreEqual (line, rows[0].cells[0]);

        for (const DxuiTextView::Span & span : rows[0].spans)
        {
            colored.push_back (std::format (L"{:X}:{}", span.argb & 0xFF, line.substr ((size_t) span.start, (size_t) span.length)));
        }

        Assert::IsTrue (std::ranges::find (colored, std::wstring (L"5:2000"))       != colored.end(), L"The address in its color");
        Assert::IsTrue (std::ranges::find (colored, std::wstring (L"6:A9 01     ")) != colored.end(), L"the bytes in theirs");
        Assert::IsTrue (std::ranges::find (colored, std::wstring (L"1:LDA"))        != colored.end(), L"and the mnemonic in the mnemonic color");
        Assert::IsTrue (PreviewText::BuildTextRows (preview, false)[0].spans.empty(), L"Without colors, no spans");
    }



    //  Visual Studio's dark code colors on a dark background, a listing's
    //  columns in the muted text color, and none without a theme.
    TEST_METHOD (SourceColors_OnADarkBackground_AreVisualStudiosDark)
    {
        MockDxuiTheme         theme;
        SourceSyntax::Colors  colors = PreviewText::GetSourceColors (&theme);

        Assert::AreEqual (0xFF8CBCF0u, colors.mnemonic);
        Assert::AreEqual (0xFF57A64Au, colors.comment);
        Assert::AreEqual (theme.ForegroundMuted(), colors.address);
        Assert::AreEqual (0u, PreviewText::GetSourceColors (nullptr).mnemonic);
    }
};
