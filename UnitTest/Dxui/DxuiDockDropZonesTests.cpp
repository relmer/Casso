#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZonesTests
//
//  Where a dragged pane can land: the cross on each group, the window's
//  edge guides, what the overlay shades for each, the layout operation each
//  one is, and the size and place of each target at every scale.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockDropZonesTests
{
    static const RECT           s_kArea   = { 0, 0, 1000, 600 };
    static const DxuiDpiScaler  s_kScaler;



    //  code on the left half, regs on the right.
    static DxuiPaneLayout MakeSample()
    {
        DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");



        layout.Add (L"regs", L"");
        return layout;
    }



    static const DxuiDockDropZone * Find (const std::vector<DxuiDockDropZone> & zones, DxuiDockDropZone::Kind kind,
                                          DxuiDockSide side, const wchar_t * target)
    {
        for (const DxuiDockDropZone & zone : zones)
        {
            bool  sideMatches = (kind == DxuiDockDropZone::Kind::Tab) || zone.side == side;

            if (zone.kind == kind && sideMatches && zone.targetPane == target)
            {
                return &zone;
            }
        }

        return nullptr;
    }



    TEST_CLASS (DxuiDockDropZonesTests)
    {
    public:

        TEST_METHOD (EveryGroupHasACompassAndTheWindowHasFourEdges)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"trace", s_kScaler);



            Assert::AreEqual ((size_t) (4 + 2 * 5), zones.size());
            Assert::IsNotNull (Find (zones, DxuiDockDropZone::Kind::Tab,  DxuiDockSide::Left,  L"code"));
            Assert::IsNotNull (Find (zones, DxuiDockDropZone::Kind::Side, DxuiDockSide::Top,   L"regs"));
            Assert::IsNotNull (Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Right, L""));
        }


        TEST_METHOD (AGroupOfOnlyTheDraggedPaneHasNoCompass)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"regs", s_kScaler);



            Assert::AreEqual ((size_t) (4 + 5), zones.size());
            Assert::IsNull   (Find (zones, DxuiDockDropZone::Kind::Tab, DxuiDockSide::Left, L"regs"));
        }


        TEST_METHOD (TheCompassSitsAtTheGroupsMiddle)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"trace", s_kScaler);
            const DxuiDockDropZone       * tab    = Find (zones, DxuiDockDropZone::Kind::Tab, DxuiDockSide::Left, L"code");



            Assert::AreEqual ((long) 250, (tab->target.left + tab->target.right) / 2);
            Assert::AreEqual ((long) 300, (tab->target.top + tab->target.bottom) / 2);
            Assert::AreEqual ((long) s_kScaler.ToPx (DxuiDockGuide::kButtonDip), tab->target.right - tab->target.left);
        }


        //  At 125% and 150% every target is as big as the button drawn there,
        //  40 and 48 px, on a pitch of 45 and 54 px. An edge guide's box lies
        //  10 DIP in from the area's edge, 13 and 15 px, centered along it,
        //  and its button 4 DIP inside the box: 18 and 21 px in, where Visual
        //  Studio's are 17 to 18 px in at 125%.
        TEST_METHOD (TheTargetsScaleWithTheDpi)
        {
            constexpr UINT  kDpis[]    = { 120, 144 };
            constexpr long  kButtons[] = { 40, 48 };
            constexpr long  kPitches[] = { 45, 54 };
            constexpr long  kInsets[]  = { 18, 21 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                DxuiDpiScaler                  scaler;
                DxuiPaneLayout                 layout = MakeSample();
                std::vector<DxuiDockDropZone>  zones;
                const DxuiDockDropZone       * tab    = nullptr;
                const DxuiDockDropZone       * side   = nullptr;
                const DxuiDockDropZone       * left   = nullptr;
                const DxuiDockDropZone       * top    = nullptr;
                const DxuiDockDropZone       * right  = nullptr;
                std::wstring                   at     = std::format (L"{} dpi", kDpis[i]);

                scaler.SetDpi (kDpis[i]);
                zones = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr), s_kArea, L"trace", scaler);
                tab   = Find (zones, DxuiDockDropZone::Kind::Tab,  DxuiDockSide::Left,  L"code");
                side  = Find (zones, DxuiDockDropZone::Kind::Side, DxuiDockSide::Left,  L"code");
                left  = Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Left,  L"");
                top   = Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Top,   L"");
                right = Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Right, L"");

                if (tab == nullptr || side == nullptr || left == nullptr || top == nullptr || right == nullptr)
                {
                    Assert::Fail ((L"every zone is there, " + at).c_str());
                    return;
                }

                Assert::AreEqual (s_kArea.right - kInsets[i], right->target.right, (L"in from the right edge as far, " + at).c_str());

                Assert::AreEqual (kButtons[i], tab->target.right  - tab->target.left,  (L"a cross's button, " + at).c_str());
                Assert::AreEqual (kButtons[i], tab->target.bottom - tab->target.top,   (L"a cross's button, " + at).c_str());
                Assert::AreEqual (kButtons[i], left->target.right - left->target.left, (L"an edge guide's button, " + at).c_str());
                Assert::AreEqual (kPitches[i], (tab->target.left + tab->target.right) / 2 - (side->target.left + side->target.right) / 2,
                                  (L"the pitch, " + at).c_str());
                Assert::AreEqual (s_kArea.left + kInsets[i], left->target.left, (L"in from the left edge, " + at).c_str());
                Assert::AreEqual (s_kArea.top  + kInsets[i], top->target.top,   (L"in from the top edge, " + at).c_str());
                Assert::AreEqual ((s_kArea.top + s_kArea.bottom) / 2, (left->target.top + left->target.bottom) / 2, (L"centered along its edge, " + at).c_str());
                Assert::AreEqual ((s_kArea.left + s_kArea.right) / 2, (top->target.left + top->target.right) / 2,   (L"centered along its edge, " + at).c_str());
            }
        }


        TEST_METHOD (EachZoneShadesWhereThePaneWouldGo)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"trace", s_kScaler);



            Assert::AreEqual ((long) 300, Find (zones, DxuiDockDropZone::Kind::Side, DxuiDockSide::Bottom, L"code")->preview.top, L"the group's lower half");
            Assert::AreEqual ((long) 500, Find (zones, DxuiDockDropZone::Kind::Tab,  DxuiDockSide::Left,   L"code")->preview.right, L"the whole group");
            Assert::AreEqual ((long) 250, Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Left,   L"")->preview.right, L"a quarter of the window");
        }


        TEST_METHOD (HitTestFindsTheSquareUnderThePoint)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"trace", s_kScaler);
            const DxuiDockDropZone       * left   = Find (zones, DxuiDockDropZone::Kind::Side, DxuiDockSide::Left, L"code");
            POINT                          inside = { left->target.left + 2, left->target.top + 2 };



            Assert::IsTrue (DxuiDockDropZones::HitTest (zones, inside) == left);
            Assert::IsNull (DxuiDockDropZones::HitTest (zones, POINT { 400, 100 }), L"between squares");
        }


        TEST_METHOD (EachZoneIsItsLayoutOperation)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones;



            layout.Add (L"trace", L"regs");
            zones = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr), s_kArea, L"trace", s_kScaler);

            Assert::IsTrue   (DxuiDockDropZones::Apply (*Find (zones, DxuiDockDropZone::Kind::Tab, DxuiDockSide::Left, L"code"), layout, L"trace"));
            Assert::AreEqual ((size_t) 2, layout.GetGroup (L"code").size());

            zones = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr), s_kArea, L"trace", s_kScaler);
            Assert::IsTrue   (DxuiDockDropZones::Apply (*Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Bottom, L""), layout, L"trace"));
            Assert::AreEqual ((size_t) 1, layout.GetGroup (L"trace").size());
            Assert::AreEqual ((long) 600, [&] ()
            {
                for (const DxuiPaneLayout::GroupRect & group : layout.Arrange (s_kArea, nullptr, nullptr))
                {
                    if (group.active == L"trace")
                    {
                        return group.rect.bottom;
                    }
                }

                return 0L;
            } ());
        }
    };
}
