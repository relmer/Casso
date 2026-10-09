#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockCompassVisibilityTests
//
//  As Visual Studio does, a drag shows the four window-edge guides for the
//  whole drag and one cross, over the group under the pointer. The cross
//  stays while the pointer is on it, even past its own pane; only what is
//  shown can be hit, and every hit square is the button drawn there.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockCompassVisibilityTests
{
    //  The documents' group on the left over the console's, and the
    //  registers tabbed with the stack on the right.
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        MockDxuiControl  stack;
        DxuiDpiScaler    scaler;
        DxuiDarkTheme    theme;

        explicit Rig (long height, UINT dpi = 96)
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            scaler.SetDpi     (dpi);
            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
            layout.Add        (L"stack",   L"regs");

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.AddPane       (L"stack",   L"Stack",       &stack);
            site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            site.SetPaneLayout (layout);
            site.Layout        (RECT { 0, 0, 1000, height }, scaler);
        }

        RECT GetGroupRect (const MockDxuiControl & content)
        {
            RECT  rect = {};

            for (size_t i = 0; i < site.GetGroupCount(); i++)
            {
                rect = (site.GetGroup (i)->IndexOf (const_cast<MockDxuiControl *> (&content)) >= 0) ? site.GetGroup (i)->GetBounds() : rect;
            }

            return rect;
        }

        std::vector<DxuiDockDragMark> GetGuides()
        {
            std::vector<DxuiDockDragMark>  guides;

            for (const DxuiDockDragMark & mark : site.GetDragMarks (theme))
            {
                if (mark.image != nullptr)
                {
                    guides.push_back (mark);
                }
            }

            return guides;
        }

        //  The cross among the guides shown, larger than an edge guide's box,
        //  or an empty rect for none. It is not always the last: the guide
        //  holding the button under the pointer comes first.
        RECT GetCrossRect()
        {
            RECT  cross = {};
            long  edge  = DxuiDockGuide::GetSizePx (DxuiDockGuideKind::Edge, scaler).cx;

            for (const DxuiDockDragMark & guide : GetGuides())
            {
                cross = (guide.rect.right - guide.rect.left > edge) ? guide.rect : cross;
            }

            return cross;
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



    static POINT GetCrossOrigin (DxuiDockGuideKind kind, const RECT & group, const DxuiDpiScaler & scaler)
    {
        return DxuiDockGuide::GetOrigin (kind, Center (group), scaler);
    }



    TEST_CLASS (DxuiDockCompassVisibilityTests)
    {
    public:

        TEST_METHOD (ADragThatHasNotMovedShowsOnlyTheEdgeGuides)
        {
            Rig                            rig (600);
            std::vector<DxuiDockDragMark>  guides;
            SIZE                           edge  = DxuiDockGuide::GetSizePx (DxuiDockGuideKind::Edge, rig.scaler);



            rig.site.BeginDrag (L"stack");
            guides = rig.GetGuides();

            Assert::AreEqual ((size_t) 4, guides.size(), L"the four edge guides and no cross");
            Assert::IsNull   (rig.site.GetHoveredZone());

            for (const DxuiDockDragMark & guide : guides)
            {
                Assert::AreEqual (edge.cx, guide.rect.right - guide.rect.left);
                Assert::AreEqual (edge.cy, guide.rect.bottom - guide.rect.top);
                Assert::AreEqual (edge.cx, (long) guide.image->width, L"the picture fills its mark");
            }
        }


        TEST_METHOD (AMoveOverAGroupShowsThatGroupsCrossAlone)
        {
            Rig                            rig (600);
            RECT                           code    = rig.GetGroupRect (rig.code);
            RECT                           console = rig.GetGroupRect (rig.console);
            POINT                          origin  = {};
            std::vector<DxuiDockDragMark>  guides;



            rig.site.BeginDrag (L"stack");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { code.left + 20, code.bottom - 20 }));
            guides = rig.GetGuides();
            origin = GetCrossOrigin (DxuiDockGuideKind::LargeCross, code, rig.scaler);

            Assert::AreEqual ((size_t) 5, guides.size(), L"the edges and one cross");
            Assert::AreEqual (origin.x, rig.GetCrossRect().left, L"the documents' large cross");
            Assert::AreEqual (origin.y, rig.GetCrossRect().top);

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, POINT { console.left + 20, console.bottom - 20 }));
            guides = rig.GetGuides();
            origin = GetCrossOrigin (DxuiDockGuideKind::SmallCross, console, rig.scaler);

            Assert::AreEqual ((size_t) 5, guides.size());
            Assert::AreEqual (origin.x, rig.GetCrossRect().left, L"the cross moved to the console's group");
            Assert::AreEqual (origin.y, rig.GetCrossRect().top);

            rig.site.CancelDrag();
            Assert::IsTrue (rig.GetGuides().empty());
        }


        //  Between two panes, over neither, only the edge guides show, as in
        //  Visual Studio.
        TEST_METHOD (OverNoPaneOnlyTheEdgeGuidesShow)
        {
            Rig    rig (600, 120);
            RECT   code    = {};
            RECT   regs    = {};
            RECT   cross   = {};
            POINT  between = {};



            rig.site.SetPaneGap (DxuiDockSite::kPaneGapDip, DxuiDockSite::kPaneMarginDip);
            code    = rig.GetGroupRect (rig.code);
            regs    = rig.GetGroupRect (rig.regs);
            between = POINT { (code.right + regs.left) / 2, Center (code).y };

            rig.site.BeginDrag (L"stack");
            rig.site.OnMouse   (Mouse (DxuiMouseEventKind::Move, Center (code)));
            Assert::AreEqual ((size_t) 5, rig.GetGuides().size(), L"over a pane, its cross");

            rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, between));
            cross = rig.GetCrossRect();

            Assert::AreEqual ((size_t) 4, rig.GetGuides().size(), L"in the gap, the four edge guides alone");
            Assert::IsTrue   (IsRectEmpty (&cross) != FALSE,     L"and no cross");
        }


        //  A short window: the documents' large cross reaches past the bottom
        //  of their pane, over the console's.
        TEST_METHOD (AMoveOntoACrossButtonPastItsPaneKeepsThatCross)
        {
            Rig                        rig (300);
            RECT                       code    = rig.GetGroupRect (rig.code);
            RECT                       console = rig.GetGroupRect (rig.console);
            POINT                      origin  = GetCrossOrigin (DxuiDockGuideKind::LargeCross, code, rig.scaler);
            RECT                       bottom  = DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::DockBottom, origin, rig.scaler);
            POINT                      past    = { Center (bottom).x, bottom.bottom - 2 };
            const DxuiDockDropZone   * hover   = nullptr;



            Assert::IsTrue (DxuiDockSite::Contains (console, past), L"the button reaches over the console's pane");

            rig.site.BeginDrag (L"stack");
            rig.site.OnMouse   (Mouse (DxuiMouseEventKind::Move, Center (code)));
            rig.site.OnMouse   (Mouse (DxuiMouseEventKind::Move, past));
            hover = rig.site.GetHoveredZone();

            if (hover == nullptr)
            {
                Assert::Fail (L"the documents' bottom button is hit");
                return;
            }

            Assert::IsTrue   (hover->kind == DxuiDockDropZone::Kind::Side && hover->side == DxuiDockSide::Bottom);
            Assert::AreEqual (std::wstring (L"code"), hover->targetPane);
            Assert::AreEqual (origin.y, rig.GetCrossRect().top, L"the documents' cross still shows");
        }


        //  The same point reached straight from outside: the console's cross
        //  shows there, and the documents' button under the point, hidden,
        //  cannot be hit.
        TEST_METHOD (AHiddenCrossIsNotHit)
        {
            Rig    rig (300);
            RECT   code    = rig.GetGroupRect (rig.code);
            RECT   console = rig.GetGroupRect (rig.console);
            POINT  origin  = GetCrossOrigin (DxuiDockGuideKind::LargeCross, code, rig.scaler);
            RECT   bottom  = DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::DockBottom, origin, rig.scaler);
            POINT  past    = { Center (bottom).x, bottom.bottom - 2 };



            Assert::IsTrue (DxuiDockSite::Contains (console, past), L"the button reaches over the console's pane");

            rig.site.BeginDrag (L"stack");
            rig.site.OnMouse   (Mouse (DxuiMouseEventKind::Move, past));

            Assert::IsNull   (rig.site.GetHoveredZone(), L"no button of the console's cross is there");
            Assert::AreEqual (GetCrossOrigin (DxuiDockGuideKind::SmallCross, console, rig.scaler).y, rig.GetCrossRect().top,
                              L"the console's cross shows");
        }


        TEST_METHOD (ADropWithNoMoveDocksThroughTheGroupUnderThePoint)
        {
            Rig  rig (600);



            rig.site.BeginDrag (L"stack");
            Assert::IsTrue (rig.site.EndDrag (Center (rig.GetGroupRect (rig.console))), L"the console's center button takes the drop");

            Assert::IsTrue (rig.site.GetPaneLayout().GetGroup (L"console") == std::vector<std::wstring> { L"console", L"stack" });
        }


        //  Every button of every guide shown, edges and cross alike, is hit
        //  exactly where it is drawn, at 100%, 125% and 150%: 32, 40 and 48
        //  px square.
        TEST_METHOD (EachHitSquareIsTheDrawnButton)
        {
            constexpr UINT  kDpis[]    = { 96, 120, 144 };
            constexpr long  kButtons[] = { 32, 40, 48 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                Rig                            rig (600, kDpis[i]);
                RECT                           code    = rig.GetGroupRect (rig.code);
                RECT                           cross   = {};
                std::vector<DxuiDockDragMark>  guides;
                size_t                         checked = 0;
                std::wstring                   at      = std::format (L"{} dpi", kDpis[i]);

                rig.site.BeginDrag (L"stack");
                rig.site.OnMouse   (Mouse (DxuiMouseEventKind::Move, Center (code)));
                guides = rig.GetGuides();
                cross  = rig.GetCrossRect();

                for (size_t g = 0; g < guides.size(); g++)
                {
                    bool                              isCross = EqualRect (&guides[g].rect, &cross) != FALSE;
                    DxuiDockGuideKind                 kind    = isCross ? DxuiDockGuideKind::LargeCross : DxuiDockGuideKind::Edge;
                    POINT                             origin  = { guides[g].rect.left, guides[g].rect.top };
                    std::vector<DxuiDockGuideButton>  buttons = isCross ? DxuiDockGuide::GetButtons (kind, DxuiDockSide::Left)
                                                                        : std::vector<DxuiDockGuideButton> { DxuiDockGuideButton::DockLeft };

                    for (DxuiDockGuideButton button : buttons)
                    {
                        RECT                       drawn = DxuiDockGuide::GetButtonRect (kind, button, origin, rig.scaler);
                        const DxuiDockDropZone   * hover = nullptr;

                        rig.site.OnMouse (Mouse (DxuiMouseEventKind::Move, Center (drawn)));
                        hover = rig.site.GetHoveredZone();

                        if (hover == nullptr)
                        {
                            Assert::Fail ((L"every drawn button is hit, " + at).c_str());
                            return;
                        }

                        Assert::AreEqual (kButtons[i],  drawn.right - drawn.left, (L"the drawn button, " + at).c_str());
                        Assert::AreEqual (drawn.left,   hover->target.left,       at.c_str());
                        Assert::AreEqual (drawn.top,    hover->target.top,        at.c_str());
                        Assert::AreEqual (drawn.right,  hover->target.right,      at.c_str());
                        Assert::AreEqual (drawn.bottom, hover->target.bottom,     at.c_str());
                        checked++;
                    }
                }

                Assert::AreEqual ((size_t) (4 + 9), checked, (L"four edge buttons and the large cross's nine, " + at).c_str());
            }
        }
    };
}
