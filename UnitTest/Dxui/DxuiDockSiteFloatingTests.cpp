#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteFloatingTests
//
//  A floating pane's window looks like Visual Studio's: one title bar, the
//  pane's own, holding its title, menu, dock and close buttons, with no tab
//  under it; the title bar moves the window. And an auto-hidden pane's edge
//  tab draws its bar against the window's outer edge.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteFloatingTests
{
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



    struct FloatRig
    {
        DxuiDockSite     site;
        MockDxuiControl  source;
        DxuiDpiScaler    scaler;
        std::wstring     docked;

        FloatRig()
        {
            site.AddPane       (L"source", L"Source", &source);
            site.SetDocumentFn ([] (const std::wstring &) { return true; });
            site.SetPaneLayout (DxuiPaneLayout::MakeSingle (L"source"));
            site.SetFloating   ([this] (const std::wstring & pane) { docked = pane; });
            site.Layout        (RECT { 0, 0, 400, 300 }, scaler);
        }
    };



    struct EdgeRig
    {
        DxuiDockSite          site;
        MockDxuiControl       code;
        MockDxuiControl       regs;
        MockDxuiControl       console;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        EdgeRig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.SetPaneLayout (layout);
            site.Layout        (RECT { 0, 0, 1000, 600 }, scaler);
        }

        //  The bar drawn in the edge tab: the fill in the tab's rect, in the
        //  resting color, kEdgeBarDip thick across the tab.
        const RecordedPaintCall * FindBar (const RECT & tab, bool sideways)
        {
            for (const RecordedPaintCall & call : painter.Calls())
            {
                float  across = sideways ? call.width : call.height;

                if (call.kind == RecordedPaintKind::FillRect && call.argb == theme.Border()
                    && across == (float) DxuiDockSite::kEdgeBarDip
                    && call.x >= (float) tab.left && call.x + call.width  <= (float) tab.right
                    && call.y >= (float) tab.top  && call.y + call.height <= (float) tab.bottom)
                {
                    return &call;
                }
            }

            return nullptr;
        }
    };



    TEST_CLASS (DxuiDockSiteFloatingTests)
    {
    public:

        TEST_METHOD (AFloatingSitesPaneHasATitleBarAndNoTab)
        {
            FloatRig  rig;



            Assert::AreEqual ((size_t) 1, rig.site.GetGroupCount());
            Assert::IsTrue   (rig.site.GetGroup (0)->GetKind() == DxuiTabGroup::Kind::ToolWindow, L"a document too has the title bar");
            Assert::AreEqual ((long) DxuiTabGroup::kTitleDip, rig.source.GetBounds().top, L"the pane starts below the title bar");
            Assert::AreEqual ((long) 300, rig.source.GetBounds().bottom, L"no tab strip under it");
        }


        TEST_METHOD (AToolWindowIsLeftOutOfAltTab)
        {
            DxuiHwndSource::CreateParams  params;
            DWORD                         exStyle = 0;



            params.toolWindow = true;
            exStyle           = DxuiHwndSource::GetExtendedStyle (params);

            Assert::IsTrue ((exStyle & WS_EX_TOOLWINDOW) != 0);
            Assert::IsTrue ((exStyle & WS_EX_APPWINDOW)  == 0);

            params.toolWindow = false;
            exStyle           = DxuiHwndSource::GetExtendedStyle (params);

            Assert::IsTrue ((exStyle & WS_EX_APPWINDOW) != 0, L"an ordinary window keeps its taskbar button");
        }


        TEST_METHOD (TheTitleBarOffItsButtonsMovesTheWindow)
        {
            FloatRig        rig;
            DxuiTabGroup  * group = rig.site.GetGroup (0);
            RECT            title = group->GetTitleRect();



            Assert::IsTrue (rig.site.ClassifyHit (POINT { title.left + 20, Center (title).y }) == DxuiHitTestKind::Caption);
            Assert::IsTrue (rig.site.ClassifyHit (Center (group->GetTitleButtonRect (DxuiTabGroup::TitleButton::Menu))) == DxuiHitTestKind::Client);
            Assert::IsTrue (rig.site.ClassifyHit (Center (group->GetTitleButtonRect (DxuiTabGroup::TitleButton::Pin)))  == DxuiHitTestKind::Client);
            Assert::IsTrue (rig.site.ClassifyHit (Center (rig.source.GetBounds())) == DxuiHitTestKind::Client, L"the pane is not the caption");
        }


        TEST_METHOD (ADockedSitesTitleBarIsNotACaption)
        {
            EdgeRig  rig;



            rig.site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            rig.site.Relayout();

            for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
            {
                RECT  title = rig.site.GetGroup (i)->GetTitleRect();

                Assert::IsTrue (rig.site.ClassifyHit (POINT { title.left + 20, Center (title).y }) == DxuiHitTestKind::Client);
            }
        }


        TEST_METHOD (ThePinOnAFloatingSiteDocksThePane)
        {
            FloatRig  rig;
            RECT      pin = rig.site.GetGroup (0)->GetTitleButtonRect (DxuiTabGroup::TitleButton::Pin);



            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, Center (pin)));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Up,   Center (pin)));

            Assert::AreEqual (std::wstring (L"source"), rig.docked);
            Assert::IsFalse  (rig.site.GetPaneLayout().IsAutoHidden (L"source"), L"not hidden inside its own window");
        }


        TEST_METHOD (AnEdgeTabsBarLiesAgainstTheOuterEdge)
        {
            EdgeRig                    rig;
            RECT                       tab = {};
            const RecordedPaintCall  * bar = nullptr;



            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"regs",    DxuiDockSide::Left));
            Assert::IsTrue (rig.site.EditPaneLayout().AutoHide (L"console", DxuiDockSide::Bottom));
            rig.site.Relayout();
            rig.site.Paint (rig.painter, rig.text, rig.theme);

            tab = rig.site.GetEdgeTabRect (L"regs");
            bar = rig.FindBar (tab, true);
            Assert::IsNotNull (bar, L"the left tab has a bar");
            Assert::AreEqual  ((float) tab.left, bar->x, L"on the left of a tab on the left edge");

            tab = rig.site.GetEdgeTabRect (L"console");
            bar = rig.FindBar (tab, false);
            Assert::IsNotNull (bar, L"the bottom tab has a bar");
            Assert::AreEqual  ((float) tab.bottom, bar->y + bar->height, L"along the bottom of a tab on the bottom edge");
        }
    };
}
