#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDocumentWellDockTests
//
//  Over a document group the compass offers two different drops on each
//  side, as Visual Studio does: an inner split square makes a new document
//  tab group beside that one group, and the outer side square docks the pane
//  beside the whole document well, every document group together.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDocumentWellDockTests
{
    static constexpr RECT  kArea = { 0, 0, 1000, 600 };



    static bool IsDocumentGroup (const std::vector<std::wstring> & panes)
    {
        bool  document = false;



        for (const std::wstring & pane : panes)
        {
            document = document || pane == L"code" || pane == L"source";
        }

        return document;
    }



    //  Two document groups side by side, code and source, with the
    //  registers to their right and the stack against the left edge.
    static DxuiPaneLayout MakeTwoGroupWell()
    {
        DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");



        layout.Add        (L"regs",   L"");
        layout.Add        (L"source", L"code");
        layout.DockToSide (L"source", L"code", DxuiDockSide::Right);
        layout.Add        (L"stack",  L"");
        layout.DockToEdge (L"stack",  DxuiDockSide::Left);
        return layout;
    }



    static std::vector<DxuiDockDropZone> BuildZones (const DxuiPaneLayout & layout, const std::wstring & pane)
    {
        return DxuiDockDropZones::Build (layout.Arrange (kArea, nullptr, nullptr), kArea, pane, DxuiDpiScaler(),
                                         [] (const DxuiPaneLayout::GroupRect & group) { return IsDocumentGroup (group.panes); });
    }



    static const DxuiDockDropZone * FindZone (const std::vector<DxuiDockDropZone> & zones, DxuiDockDropZone::Kind kind,
                                              DxuiDockSide side, const std::wstring & target)
    {
        const DxuiDockDropZone  * found = nullptr;



        for (const DxuiDockDropZone & zone : zones)
        {
            found = (zone.kind == kind && zone.side == side && zone.targetPane == target) ? &zone : found;
        }

        return found;
    }



    static RECT RectOf (const DxuiPaneLayout & layout, const std::wstring & pane)
    {
        RECT  rect = {};



        for (const DxuiPaneLayout::GroupRect & group : layout.Arrange (kArea, nullptr, nullptr))
        {
            rect = (std::find (group.panes.begin(), group.panes.end(), pane) != group.panes.end()) ? group.rect : rect;
        }

        return rect;
    }





    TEST_CLASS (DxuiDocumentWellDockTests)
    {
    public:

        TEST_METHOD (AnOuterSideSquareDocksBesideTheWholeWell)
        {
            DxuiPaneLayout                 layout = MakeTwoGroupWell();
            std::vector<DxuiDockDropZone>  zones  = BuildZones (layout, L"regs");
            const DxuiDockDropZone       * side   = FindZone (zones, DxuiDockDropZone::Kind::Side, DxuiDockSide::Bottom, L"code");
            RECT                           code   = RectOf (layout, L"code");
            RECT                           source = RectOf (layout, L"source");
            RECT                           regs   = {};



            if (side == nullptr)
            {
                Assert::Fail (L"the code group's compass has a bottom side square");
                return;
            }

            Assert::AreEqual (source.right, side->preview.right, L"the preview spans the whole well");
            Assert::IsTrue   (DxuiDockDropZones::Apply (*side, layout, L"regs", IsDocumentGroup));

            regs   = RectOf (layout, L"regs");
            code   = RectOf (layout, L"code");
            source = RectOf (layout, L"source");
            Assert::AreEqual (code.left,    regs.left,  L"below the well, from its left");
            Assert::AreEqual (source.right, regs.right, L"to its right, under both document groups");
            Assert::IsTrue   (regs.top >= source.bottom && regs.top >= code.bottom);
            Assert::IsTrue   (RectOf (layout, L"stack").bottom > regs.top, L"the stack beside the well keeps its full height");
        }


        TEST_METHOD (AnInnerSplitSquareMakesATabGroupBesideOneGroup)
        {
            DxuiPaneLayout                 layout = MakeTwoGroupWell();
            std::vector<DxuiDockDropZone>  zones  = BuildZones (layout, L"regs");
            const DxuiDockDropZone       * split  = FindZone (zones, DxuiDockDropZone::Kind::Split, DxuiDockSide::Bottom, L"code");
            RECT                           code   = {};
            RECT                           regs   = {};



            if (split == nullptr)
            {
                Assert::Fail (L"the code group's compass has a bottom split square");
                return;
            }

            Assert::IsTrue (DxuiDockDropZones::Apply (*split, layout, L"regs", IsDocumentGroup));

            code = RectOf (layout, L"code");
            regs = RectOf (layout, L"regs");
            Assert::AreEqual (code.left,  regs.left,  L"a new group under the code group only");
            Assert::AreEqual (code.right, regs.right);
            Assert::IsTrue   (RectOf (layout, L"source").bottom > regs.top, L"the source group keeps its full height");
            Assert::AreEqual ((size_t) 1, layout.GetGroup (L"regs").size(), L"a tab group of its own");
        }


        TEST_METHOD (TheOuterSquareDocksBesideTheTargetWhenNoDocumentRemains)
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"regs");
            RECT            regs   = {};



            layout.Add (L"stack", L"");
            Assert::IsTrue (layout.DockBesideWell (L"stack", L"regs", DxuiDockSide::Bottom, IsDocumentGroup));

            regs = RectOf (layout, L"regs");
            Assert::AreEqual (regs.bottom, RectOf (layout, L"stack").top, L"below the target's group");
        }
    };
}
