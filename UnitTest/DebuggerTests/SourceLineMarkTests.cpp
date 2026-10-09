#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceLineMarkTests
    //
    //  The source pane marks the PC's line and breakpoints as the disassembly
    //  pane does: the PC's row fill and marker color, and the breakpoint icon,
    //  filled or a ring, in the gutter rather than a bullet in the text.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceLineMarkTests)
    {
    public:
        TEST_METHOD (ThePcLineAndBreakpointsAreMarkedAsInTheDisassembly)
        {
            SourcePane::Style               style;
            std::vector<std::wstring>       lines (5, L"x");
            std::vector<DxuiTextView::Row>  rows;



            style.pcMarkerArgb = 0xFFFFE34D;
            style.pcRowArgb    = 0x50C8A000;
            style.enabledIcon  = std::make_shared<DxuiIconImage>();
            style.disabledIcon = std::make_shared<DxuiIconImage>();

            rows = SourcePane::BuildRows (lines, 2, { 2, 4 }, { 4 }, style);

            Assert::AreEqual (std::wstring (s_kpszTriangleRight), rows[1].cells[0], L"no bullet in the text");
            Assert::AreEqual (std::wstring (L" "),                rows[3].cells[0]);
            Assert::IsTrue   (rows[1].icon == style.enabledIcon);
            Assert::IsTrue   (rows[3].icon == style.disabledIcon, L"a disabled breakpoint is the ring");
            Assert::IsTrue   (rows[0].icon == nullptr);
            Assert::AreEqual (style.pcRowArgb, rows[1].background);
            Assert::AreEqual (0u,              rows[3].background);
            Assert::AreEqual ((size_t) 1,      rows[1].spans.size());
            Assert::AreEqual (style.pcMarkerArgb, rows[1].spans[0].argb);
        }


        //  With the disassembly's PC arrow in the style, the marked line's
        //  arrow is in the view's glyph margin, where the breakpoints are, and
        //  drawn over the line's breakpoint when it has one; the marker cell
        //  holds nothing on any line, so the line numbers follow the margin.
        TEST_METHOD (ThePcsArrowIsInTheGlyphMarginOverTheBreakpoint)
        {
            constexpr int                   kInArrowX = 14;   // inside the arrow, left of its tip
            constexpr int                   kPastTipX = 39;   // inside the dot, past the arrow's tip
            constexpr int                   kMiddle   = GutterGlyph::kSizePx / 2;
            SourcePane::Style               style;
            std::vector<std::wstring>       lines (5, L"x");
            std::vector<DxuiTextView::Row>  rows;
            const DxuiIconImage           * marked    = nullptr;



            style.pcMarkerArgb = 0xFFFFE34D;
            style.pcRowArgb    = 0x50C8A000;
            style.enabledIcon  = GutterGlyph::MakeDot   (0xFFF4524D, true);
            style.disabledIcon = GutterGlyph::MakeDot   (0xFFF4524D, false);
            style.pcIcon       = GutterGlyph::MakeArrow (style.pcMarkerArgb);

            rows = SourcePane::BuildRows (lines, 2, { 4 }, {}, style);

            Assert::IsTrue   (rows[1].icon == style.pcIcon,      L"the arrow itself on a line with no breakpoint");
            Assert::IsTrue   (rows[3].icon == style.enabledIcon, L"a breakpoint's dot on its own line");
            Assert::AreEqual (style.pcRowArgb, rows[1].background, L"the PC's row fill");
            Assert::IsTrue   (rows[1].spans.empty(), L"no triangle to color in the text");

            for (const DxuiTextView::Row & row : rows)
            {
                Assert::IsTrue (row.cells[0].empty(), L"no marker in the text");
            }

            rows   = SourcePane::BuildRows (lines, 4, { 4 }, {}, style);
            marked = rows[3].icon.get();

            Assert::IsNotNull (marked, L"the marked line with a breakpoint shows both");
            Assert::AreEqual  (style.pcIcon->bgraPremul[(size_t) (kMiddle * marked->width + kInArrowX)],      marked->bgraPremul[(size_t) (kMiddle * marked->width + kInArrowX)], L"the arrow over the dot");
            Assert::AreEqual  (style.enabledIcon->bgraPremul[(size_t) (kMiddle * marked->width + kPastTipX)], marked->bgraPremul[(size_t) (kMiddle * marked->width + kPastTipX)], L"the dot past the arrow's tip");
        }
    };
}
