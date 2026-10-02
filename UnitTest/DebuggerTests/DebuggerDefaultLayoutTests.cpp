#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerViewState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerDefaultLayoutTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerDefaultLayoutTests
    //
    //  The layout the debugger opens with when nothing was saved: the
    //  disassembly over the console, which carries the trace and the device
    //  panels as tabs, on the left; on the right the registers beside the
    //  stack, then watches, call stack, breakpoints and memory, top to bottom.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerDefaultLayoutTests)
    {
    public:

        static RECT  FindGroup (const std::vector<DxuiPaneLayout::GroupRect> & groups, const std::wstring & pane)
        {
            for (const DxuiPaneLayout::GroupRect & group : groups)
            {
                if (std::find (group.panes.begin(), group.panes.end(), pane) != group.panes.end())
                {
                    return group.rect;
                }
            }

            Assert::Fail ((L"no group holds " + pane).c_str());
            return RECT {};
        }


        static bool  IsInGroup (const DxuiPaneLayout & layout, const std::wstring & member, const std::wstring & pane)
        {
            std::vector<std::wstring>  group = layout.GetGroup (member);



            return std::find (group.begin(), group.end(), pane) != group.end();
        }


        TEST_METHOD (TheConsoleCarriesTheTraceAndTheDevicePanels)
        {
            DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();



            Assert::IsTrue (IsInGroup (layout, DebuggerLayout::kConsole, DebuggerLayout::kTrace));

            for (const DebuggerLayout::DiagnosticsPanel & panel : DebuggerLayout::GetDiagnosticsPanels())
            {
                Assert::IsTrue (IsInGroup (layout, DebuggerLayout::kConsole, DebuggerLayout::GetDiagnosticsPaneId (panel.id)));
            }

            Assert::AreEqual ((size_t) 1, layout.GetGroup (DebuggerLayout::kCallStack).size(), L"the call stack is a group of its own");
            Assert::AreEqual ((size_t) 1, layout.GetGroup (DebuggerLayout::kStack).size(),     L"so is the stack");
        }


        TEST_METHOD (TheRightColumnRunsTopToBottom)
        {
            DxuiPaneLayout                          layout = DebuggerLayout::MakeDefault();
            std::vector<DxuiPaneLayout::GroupRect>  groups = layout.Arrange (RECT { 0, 0, 1900, 1760 }, nullptr, nullptr);
            RECT                                    code   = FindGroup (groups, DebuggerLayout::kCode);
            RECT                                    cons   = FindGroup (groups, DebuggerLayout::kConsole);
            RECT                                    regs   = FindGroup (groups, DebuggerLayout::kRegisters);
            RECT                                    stack  = FindGroup (groups, DebuggerLayout::kStack);
            RECT                                    watch  = FindGroup (groups, DebuggerLayout::kWatches);
            RECT                                    calls  = FindGroup (groups, DebuggerLayout::kCallStack);
            RECT                                    bps    = FindGroup (groups, DebuggerLayout::kBreakpoints);
            RECT                                    memory = FindGroup (groups, DebuggerLayout::GetMemoryPaneId (1));



            Assert::IsTrue (cons.top    >= code.bottom,  L"console below the disassembly");
            Assert::IsTrue (cons.bottom == 1760,         L"the console reaches the bottom");
            Assert::IsTrue (regs.left   >= code.right,   L"registers on the right");
            Assert::IsTrue (stack.left  >= regs.right,   L"stack beside the registers");
            Assert::IsTrue (stack.top   == regs.top,     L"on the same row");
            Assert::IsTrue (watch.top   >= regs.bottom,  L"watches below them");
            Assert::IsTrue (watch.left  == regs.left && watch.right == stack.right, L"across the column");
            Assert::IsTrue (calls.top   >= watch.bottom, L"then the call stack");
            Assert::IsTrue (bps.top     >= calls.bottom, L"then breakpoints");
            Assert::IsTrue (memory.top  >= bps.bottom,   L"then memory");
            Assert::IsTrue (memory.bottom == 1760,       L"which reaches the bottom");
            Assert::IsTrue (code.bottom < 1760 / 2,      L"the disassembly takes less height than the console");
        }


        TEST_METHOD (TheMemoryWindowsAreTabsOfTheFirst)
        {
            DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();



            Assert::AreEqual ((size_t) DebuggerViewState::kMaxMemoryWindows, layout.GetGroup (DebuggerLayout::GetMemoryPaneId (1)).size());
        }
    };
}
