#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockCarryAndCompassTests
//
//  A pane torn off by its tab keeps that tab, in a strip, while it is
//  carried, and the cursor keeps the spot of the tab it pressed. The drop
//  targets follow Visual Studio's: five squares over a tool window group, a
//  larger compass over a document group, and a tab drop that shades the
//  pane's body without covering its tabs.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockCarryAndCompassTests
{
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
            layout.Add        (L"stack",   L"regs");

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.AddPane       (L"stack",   L"Stack",       &stack);
            site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            site.SetOnTearOff  ([] (const std::wstring &, POINT) {});
            site.SetPaneLayout (layout);
            site.Layout (RECT { 0, 0, 1000, 600 }, scaler);
        }

        DxuiTabGroup * GroupOf (const MockDxuiControl & content)
        {
            DxuiTabGroup  * found = nullptr;



            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                found = (site.GetGroup (i)->IndexOf (const_cast<MockDxuiControl *> (&content)) >= 0) ? site.GetGroup (i) : found;
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



    static bool IsEmpty (const RECT & r)
    {
        return r.right <= r.left || r.bottom <= r.top;
    }



    static size_t Count (const std::vector<DxuiDockDropZone> & zones, DxuiDockDropZone::Kind kind)
    {
        return (size_t) std::count_if (zones.begin(), zones.end(), [kind] (const DxuiDockDropZone & zone) { return zone.kind == kind; });
    }



    TEST_CLASS (DxuiDockCarryAndCompassTests)
    {
    public:

        TEST_METHOD (ATearOffByItsTabKeepsWhereTheTabWasPressed)
        {
            Rig             rig;
            DxuiTabGroup  * group = rig.GroupOf (rig.stack);
            RECT            tab   = {};
            POINT           press = {};



            if (group == nullptr)
            {
                Assert::Fail (L"the stack is in a group");
                return;
            }

            tab   = group->GetTabRect (group->IndexOf (&rig.stack));
            press = POINT { tab.left + 7, tab.top + 5 };
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Down, press));
            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { press.x + 40, press.y + 60 }));

            Assert::IsTrue   (rig.site.WasTearOffFromTab());
            Assert::AreEqual (7L, rig.site.GetTearOffGrab().x);
            Assert::AreEqual (5L, rig.site.GetTearOffGrab().y);
        }


        TEST_METHOD (ACarriedLonePaneShowsItsTabUntilTheCarryEnds)
        {
            DxuiDockSite     site;
            MockDxuiControl  console;
            DxuiDpiScaler    scaler;
            DxuiTabGroup   * group = nullptr;
            RECT             tab   = {};



            site.AddPane       (L"console", L"Console", &console);
            site.SetFloating   ([] (const std::wstring &) {});
            site.SetPaneLayout (DxuiPaneLayout::MakeSingle (L"console"));
            site.Layout        (RECT { 0, 0, 400, 300 }, scaler);
            group = site.GetGroup (0);

            Assert::IsTrue (IsEmpty (site.GetCarriedTabRect()));
            Assert::IsTrue (IsEmpty (group->GetTabRect (0)), L"a lone tool window has no tab");

            site.SetCarriedPane (L"console");
            tab = site.GetCarriedTabRect();

            Assert::IsTrue   (tab.right > tab.left && tab.bottom > tab.top, L"the carried pane shows its tab");
            Assert::AreEqual (300L, tab.bottom, L"along the window's bottom");

            site.SetCarriedPane (L"");

            Assert::IsTrue (IsEmpty (site.GetGroup (0)->GetTabRect (0)), L"the strip goes once the carry ends");
        }


        TEST_METHOD (ADocumentGroupGetsTheLargerCompass)
        {
            DxuiPaneLayout                 layout = DxuiPaneLayout::MakeSingle (L"code");
            RECT                           area   = { 0, 0, 1000, 600 };
            std::vector<DxuiDockDropZone>  zones;
            POINT                          center = {};
            const DxuiDockDropZone       * inner  = nullptr;
            const DxuiDockDropZone       * outer  = nullptr;



            layout.Add (L"regs", L"");
            zones = DxuiDockDropZones::Build (layout.Arrange (area, nullptr, nullptr), area, L"trace",
                                              [] (const DxuiPaneLayout::GroupRect & group) { return group.active == L"code"; });

            Assert::AreEqual ((size_t) 4, Count (zones, DxuiDockDropZone::Kind::Split), L"only the document group splits");
            Assert::AreEqual ((size_t) 8, Count (zones, DxuiDockDropZone::Kind::Side));
            Assert::AreEqual ((size_t) 2, Count (zones, DxuiDockDropZone::Kind::Tab));

            for (const DxuiDockDropZone & zone : zones)
            {
                center = (zone.kind == DxuiDockDropZone::Kind::Tab && zone.targetPane == L"code") ? Center (zone.target) : center;
                inner  = (zone.kind == DxuiDockDropZone::Kind::Split && zone.side == DxuiDockSide::Left) ? &zone : inner;
                outer  = (zone.kind == DxuiDockDropZone::Kind::Side && zone.side == DxuiDockSide::Left && zone.targetPane == L"code") ? &zone : outer;
            }

            if (inner == nullptr || outer == nullptr)
            {
                Assert::Fail (L"the document compass has a left split and a left side");
                return;
            }

            Assert::IsTrue (inner->target.right <= center.x && inner->target.right > outer->target.right,
                            L"the split square sits between the tab square and the side square");
        }


        TEST_METHOD (ATabDropShadesThePanesBodyNotItsTabs)
        {
            Rig                       rig;
            DxuiTabGroup            * group = rig.GroupOf (rig.code);
            const DxuiDockDropZone  * hover = nullptr;
            RECT                      body  = {};



            if (group == nullptr)
            {
                Assert::Fail (L"the code is in a group");
                return;
            }

            body = group->GetBodyRect();
            rig.site.BeginDrag (L"console");
            rig.site.OnMouse   (Mouse (DxuiMouseEventKind::Move, Center (group->GetBounds())));
            hover = rig.site.GetHoveredZone();

            if (hover == nullptr || hover->kind != DxuiDockDropZone::Kind::Tab)
            {
                Assert::Fail (L"the middle of the group is its tab square");
                return;
            }

            Assert::AreEqual (body.top,    hover->preview.top, L"the shade starts below the tabs");
            Assert::AreEqual (body.bottom, hover->preview.bottom);
            rig.site.CancelDrag();
        }
    };
}
