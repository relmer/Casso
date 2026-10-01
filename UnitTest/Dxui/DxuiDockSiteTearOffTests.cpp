#include "Pch.h"

#include "Widgets/DxuiDockSite.h"
#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteTearOffTests
//
//  A pane's tab dragged off its strip, or a lone pane's title bar dragged,
//  floats the pane at once (FR-125): the site asks the application to tear
//  it off instead of showing its own drag, so the floating window carries
//  the rest of the drag. A group of several panes dragged by its title bar
//  still drags here.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteTearOffTests
{
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;
        std::wstring     torn;
        int              tears = 0;

        explicit Rig (bool toolWindows)
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
            layout.Add        (L"stack",   L"regs");

            site.AddPane (L"code",    L"Disassembly", &code);
            site.AddPane (L"regs",    L"Registers",   &regs);
            site.AddPane (L"console", L"Console",     &console);
            site.AddPane (L"stack",   L"Stack",       &stack);

            if (toolWindows)
            {
                site.SetDocumentFn ([] (const std::wstring &) { return false; });
            }

            site.SetOnTearOff  ([this] (const std::wstring & pane, POINT) { torn = pane; tears++; });
            site.SetPaneLayout (layout);
            site.Layout (RECT { 0, 0, 1000, 600 }, scaler);
        }

        DxuiTabGroup * FindGroup (size_t tabs)
        {
            DxuiTabGroup  * found = nullptr;



            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                found = (found == nullptr && site.GetGroup (i)->GetTabCount() == tabs) ? site.GetGroup (i) : found;
            }

            return found;
        }

        DxuiTabGroup * FindGroupOf (const MockDxuiControl & content)
        {
            DxuiTabGroup  * found = nullptr;



            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                found = (site.GetGroup (i)->GetTabCount() == 1 && site.GetGroup (i)->GetContent (0) == &content) ? site.GetGroup (i) : found;
            }

            return found;
        }
    };



    static DxuiMouseEvent Mouse (DxuiMouseEventKind kind, POINT at)
    {
        DxuiMouseEvent  ev;



        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = at;
        return ev;
    }



    static POINT Center (const RECT & r)
    {
        return POINT { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
    }



    TEST_CLASS (DxuiDockSiteTearOffTests)
    {
    public:

        TEST_METHOD (ATabDraggedOffItsStripTearsOffAtOnce)
        {
            Rig             rig (false);
            DxuiTabGroup  * group = rig.FindGroup (2);
            RECT            tab   = {};



            if (group == nullptr)
            {
                Assert::Fail (L"the rig has a group of two tabs");
                return;
            }

            tab = group->GetTabRect (1);
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (tab)));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { Center (tab).x + 40, Center (tab).y + 40 }));

            Assert::AreEqual (std::wstring (L"stack"), rig.torn);
            Assert::AreEqual (1, rig.tears);
            Assert::IsFalse  (rig.site.IsDragging(), L"the floating window carries the drag, not the site");
        }


        TEST_METHOD (ALonePanesTitleBarDraggedTearsOffAtOnce)
        {
            Rig             rig (true);
            DxuiTabGroup  * group = rig.FindGroupOf (rig.console);
            RECT            title = {};
            POINT           at    = {};



            if (group == nullptr)
            {
                Assert::Fail (L"the console is alone in its group");
                return;
            }

            title = group->GetTitleRect();
            at    = POINT { title.left + 20, Center (title).y };
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, at));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { at.x + 30, at.y + 30 }));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { at.x + 60, at.y + 60 }));

            Assert::AreEqual (std::wstring (L"console"), rig.torn);
            Assert::AreEqual (1, rig.tears, L"torn off once, not on every move after");
            Assert::IsFalse  (rig.site.IsDragging());
        }


        TEST_METHOD (AGroupOfSeveralPanesDraggedByItsTitleStaysInTheSite)
        {
            Rig             rig (true);
            DxuiTabGroup  * group = rig.FindGroup (2);
            RECT            title = {};
            POINT           at    = {};



            if (group == nullptr)
            {
                Assert::Fail (L"the rig has a group of two panes");
                return;
            }

            title = group->GetTitleRect();
            at    = POINT { title.left + 20, Center (title).y };
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, at));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { at.x + 30, at.y + 30 }));

            Assert::AreEqual (0, rig.tears);
            Assert::IsTrue   (rig.site.IsDragging());
        }


        TEST_METHOD (AFloatingWindowsSiteNeverTearsOff)
        {
            Rig             rig (false);
            DxuiTabGroup  * group = nullptr;
            RECT            tab   = {};



            rig.site.SetFloating ([] (const std::wstring &) {});
            rig.site.Relayout();
            group = rig.FindGroup (2);

            if (group == nullptr)
            {
                Assert::Fail (L"the rig has a group of two tabs");
                return;
            }

            tab = group->GetTabRect (1);
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (tab)));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { Center (tab).x + 40, Center (tab).y + 40 }));

            Assert::AreEqual (0, rig.tears);
        }
    };
}
