#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayoutTests
//
//  The debugger opens with the arrangement it had before panes could be
//  docked, and every pane it has is in it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerLayoutTests
{
    static RECT FindGroup (const std::vector<DxuiPaneLayout::GroupRect> & groups, const std::wstring & pane)
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



    TEST_CLASS (DebuggerLayoutTests)
    {
    public:

        TEST_METHOD (EveryPaneIsDocked)
        {
            DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();



            for (const wchar_t * pane : { DebuggerLayout::kCode, DebuggerLayout::kSource, DebuggerLayout::kConsole,
                                          DebuggerLayout::kRegisters, DebuggerLayout::kBreakpoints,
                                          DebuggerLayout::kWatches, DebuggerLayout::kStack })
            {
                Assert::IsTrue (layout.IsDocked (pane), pane);
            }

            Assert::AreEqual ((size_t) 4, layout.GetGroup (DebuggerLayout::GetMemoryPaneId (1)).size());
        }


        TEST_METHOD (PanesSitWhereTheyDidBeforeDocking)
        {
            DxuiPaneLayout                          layout = DebuggerLayout::MakeDefault();
            std::vector<DxuiPaneLayout::GroupRect>  groups = layout.Arrange (RECT { 0, 0, 1000, 800 }, nullptr, nullptr);
            RECT                                    code   = FindGroup (groups, DebuggerLayout::kCode);
            RECT                                    source = FindGroup (groups, DebuggerLayout::kSource);
            RECT                                    cons   = FindGroup (groups, DebuggerLayout::kConsole);
            RECT                                    regs   = FindGroup (groups, DebuggerLayout::kRegisters);
            RECT                                    bps    = FindGroup (groups, DebuggerLayout::kBreakpoints);
            RECT                                    memory = FindGroup (groups, DebuggerLayout::GetMemoryPaneId (1));
            RECT                                    stack  = FindGroup (groups, DebuggerLayout::kStack);



            Assert::IsTrue (source.right <= code.left,    L"source left of code");
            Assert::IsTrue (cons.top     >= code.bottom,  L"console below code");
            Assert::IsTrue (regs.left    >= code.right,   L"registers on the right");
            Assert::IsTrue (bps.top      >= regs.bottom,  L"breakpoints below registers");
            Assert::IsTrue (memory.top   >= cons.bottom,  L"memory across the bottom");
            Assert::IsTrue (stack.left   >= memory.right, L"stack beside memory");
            Assert::AreEqual ((long) 800, stack.bottom);
        }


        TEST_METHOD (TheDefaultSurvivesItsTextForm)
        {
            DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();
            DxuiPaneLayout  parsed;



            Assert::IsTrue   (DxuiPaneLayout::TryParse (layout.ToText(), parsed));
            Assert::AreEqual (layout.ToText(), parsed.ToText());
            Logger::WriteMessage (layout.ToText().c_str());
        }
    };
}