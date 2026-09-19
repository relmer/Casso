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


        TEST_METHOD (AFloatingPaneRestoresFloatingAndDocksBack)
        {
            DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();
            DxuiPaneLayout  restored;



            Assert::IsTrue (layout.Float (DebuggerLayout::kRegisters, L"\\\\.\\DISPLAY2", RECT { 10, 20, 410, 370 }));

            restored = DebuggerLayout::Restore (layout.ToText());

            Assert::IsTrue  (restored.IsFloating (DebuggerLayout::kRegisters));
            Assert::IsFalse (restored.IsDocked   (DebuggerLayout::kRegisters));
            Assert::IsTrue  (restored.DockBack   (DebuggerLayout::kRegisters));
            Assert::IsTrue  (restored.IsDocked   (DebuggerLayout::kRegisters));
        }


        TEST_METHOD (AnAutoHiddenPaneRestoresHidden)
        {
            DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();
            DxuiPaneLayout  restored;



            Assert::IsTrue (layout.AutoHide (DebuggerLayout::kWatches, DxuiDockSide::Right));

            restored = DebuggerLayout::Restore (layout.ToText());

            Logger::WriteMessage (layout.ToText().c_str());
            Assert::IsTrue (restored.IsAutoHidden (DebuggerLayout::kWatches));
            Assert::AreEqual (layout.ToText(), restored.ToText());
        }


        TEST_METHOD (UnreadableTextGivesTheDefault)
        {
            Assert::AreEqual (DebuggerLayout::MakeDefault().ToText(), DebuggerLayout::Restore (L"").ToText());
            Assert::AreEqual (DebuggerLayout::MakeDefault().ToText(), DebuggerLayout::Restore (L"dxui-layout 99\n").ToText());
        }


        TEST_METHOD (ASavedLayoutKeepsItsArrangementAndGainsMissingPanes)
        {
            DxuiPaneLayout  restored = DebuggerLayout::Restore (
                L"dxui-layout 1\ntree (split h 0.6000 (tabs 0 \"code\" \"gone\") (tabs 0 \"registers\" \"memory1\"))\n");



            Assert::IsFalse  (restored.Contains (L"gone"), L"a pane this build lacks is dropped");
            Assert::AreEqual ((size_t) 1, restored.GetGroup (DebuggerLayout::kCode).size());

            for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
            {
                Assert::IsTrue (restored.IsDocked (pane), pane.c_str());
            }

            Assert::AreEqual ((size_t) 5, restored.GetGroup (DebuggerLayout::kRegisters).size(),
                              L"registers and the four memory windows");
        }    };
}