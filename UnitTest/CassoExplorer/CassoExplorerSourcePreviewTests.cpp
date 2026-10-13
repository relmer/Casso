#include "Pch.h"
#include "../Dxui/MockDxuiTheme.h"
#include "CassoExplorer/CassoExplorerWindow.h"

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

        rows = CassoExplorerWindow::BuildTextRows (preview, false, colors);

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
        Assert::IsTrue (CassoExplorerWindow::BuildTextRows (preview, false)[9].spans.empty(), L"Without colors, no spans");
    }



    //  Visual Studio's dark code colors on a dark background, a listing's
    //  columns in the muted text color, and none without a theme.
    TEST_METHOD (SourceColors_OnADarkBackground_AreVisualStudiosDark)
    {
        MockDxuiTheme         theme;
        SourceSyntax::Colors  colors = CassoExplorerWindow::GetSourceColors (&theme);

        Assert::AreEqual (0xFF8CBCF0u, colors.mnemonic);
        Assert::AreEqual (0xFF57A64Au, colors.comment);
        Assert::AreEqual (theme.ForegroundMuted(), colors.address);
        Assert::AreEqual (0u, CassoExplorerWindow::GetSourceColors (nullptr).mnemonic);
    }
};
