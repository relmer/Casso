#include "Pch.h"

#include "Core/DxuiDockDropZones.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDropZonesTests
//
//  Where a dragged pane can land (FR-039): the compass on each group, the
//  window's edge squares, what the overlay shades for each, and the layout
//  operation each one is.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockDropZonesTests
{
    static const RECT  s_kArea = { 0, 0, 1000, 600 };



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
                                                                              s_kArea, L"trace");



            Assert::AreEqual ((size_t) (4 + 2 * 5), zones.size());
            Assert::IsNotNull (Find (zones, DxuiDockDropZone::Kind::Tab,  DxuiDockSide::Left,  L"code"));
            Assert::IsNotNull (Find (zones, DxuiDockDropZone::Kind::Side, DxuiDockSide::Top,   L"regs"));
            Assert::IsNotNull (Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Right, L""));
        }


        TEST_METHOD (AGroupOfOnlyTheDraggedPaneHasNoCompass)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"regs");



            Assert::AreEqual ((size_t) (4 + 5), zones.size());
            Assert::IsNull   (Find (zones, DxuiDockDropZone::Kind::Tab, DxuiDockSide::Left, L"regs"));
        }


        TEST_METHOD (TheCompassSitsAtTheGroupsMiddle)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"trace");
            const DxuiDockDropZone       * tab    = Find (zones, DxuiDockDropZone::Kind::Tab, DxuiDockSide::Left, L"code");



            Assert::AreEqual ((long) 250, (tab->target.left + tab->target.right) / 2);
            Assert::AreEqual ((long) 300, (tab->target.top + tab->target.bottom) / 2);
            Assert::AreEqual ((long) DxuiDockDropZones::kSquareDip, tab->target.right - tab->target.left);
        }


        TEST_METHOD (EachZoneShadesWhereThePaneWouldGo)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"trace");



            Assert::AreEqual ((long) 300, Find (zones, DxuiDockDropZone::Kind::Side, DxuiDockSide::Bottom, L"code")->preview.top, L"the group's lower half");
            Assert::AreEqual ((long) 500, Find (zones, DxuiDockDropZone::Kind::Tab,  DxuiDockSide::Left,   L"code")->preview.right, L"the whole group");
            Assert::AreEqual ((long) 250, Find (zones, DxuiDockDropZone::Kind::Edge, DxuiDockSide::Left,   L"")->preview.right, L"a quarter of the window");
        }


        TEST_METHOD (HitTestFindsTheSquareUnderThePoint)
        {
            DxuiPaneLayout                 layout = MakeSample();
            std::vector<DxuiDockDropZone>  zones  = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr),
                                                                              s_kArea, L"trace");
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
            zones = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr), s_kArea, L"trace");

            Assert::IsTrue   (DxuiDockDropZones::Apply (*Find (zones, DxuiDockDropZone::Kind::Tab, DxuiDockSide::Left, L"code"), layout, L"trace"));
            Assert::AreEqual ((size_t) 2, layout.GetGroup (L"code").size());

            zones = DxuiDockDropZones::Build (layout.Arrange (s_kArea, nullptr, nullptr), s_kArea, L"trace");
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
