#include "Pch.h"

#include "Core/UnicodeSymbols.h"
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
    };
}
