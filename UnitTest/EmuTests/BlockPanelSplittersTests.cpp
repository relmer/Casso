#include "Pch.h"
#include "Ui/DiskInspector/Blocks/BlockPanelSplitters.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using Bar = BlockPanelSplitters::Bar;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplittersTests
//
//  The places of block mode's splitters, and their drags.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockPanelSplittersTests)
{
public:

    static constexpr RECT  kBounds = { 0, 0, 1000, 800 };
    static constexpr RECT  kList   = { 200, 40, 600, 400 };
    static constexpr int   kFoot   = 792;

    static DxuiMouseEvent MakeMouse (DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;



        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = { x, y };

        return ev;
    }


    //  The splitters placed in kBounds, with each bar 6 pixels wide.
    static void Place (BlockPanelSplitters & splitters)
    {
        int  mapX  = 0;
        int  treeX = splitters.PlaceColumns (kBounds, mapX);



        splitters.SetBarRect (Bar::Tree, { treeX, 0, treeX + 6, 800 });
        splitters.SetBarRect (Bar::Map,  { mapX, 0, mapX + 6, 800 });
        splitters.SetBarRect (Bar::List, { 200, 400, 600, 406 });
        splitters.SetBarRect (Bar::Hex,  { 440, 420, 446, 800 });
    }


    TEST_METHOD (PlaceColumns_UsesTheShares)
    {
        BlockPanelSplitters  splitters;
        int                  mapX = 0;



        Assert::AreEqual (180, splitters.PlaceColumns (kBounds, mapX));
        Assert::AreEqual (620, mapX);
    }


    TEST_METHOD (DragTree_MovesTheTreeEdge)
    {
        BlockPanelSplitters  splitters;
        bool                 isMoved = false;
        int                  mapX    = 0;



        Place (splitters);

        Assert::IsTrue (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 182, 300), kBounds, kList, kFoot, isMoved));
        Assert::IsTrue (splitters.GetDragged() == Bar::Tree);
        Assert::IsTrue (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 250, 300), kBounds, kList, kFoot, isMoved));
        Assert::IsTrue (isMoved);
        Assert::AreEqual (250, splitters.PlaceColumns (kBounds, mapX));
        Assert::IsTrue (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Up, 250, 300), kBounds, kList, kFoot, isMoved));
        Assert::IsTrue (splitters.GetDragged() == Bar::None);
    }


    TEST_METHOD (DragTree_StopsAtItsRange)
    {
        BlockPanelSplitters  splitters;
        bool                 isMoved = false;
        int                  mapX    = 0;



        Place (splitters);

        (void) splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 182, 300), kBounds, kList, kFoot, isMoved);
        (void) splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 900, 300), kBounds, kList, kFoot, isMoved);

        Assert::AreEqual ((int) (1000 * BlockPanelSplitters::kMaxTree), splitters.PlaceColumns (kBounds, mapX));
    }


    TEST_METHOD (MapEdge_StaysRightOfTheTree)
    {
        BlockPanelSplitters  splitters;
        bool                 isMoved = false;
        int                  mapX    = 0;



        Place (splitters);

        (void) splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 182, 300), kBounds, kList, kFoot, isMoved);
        (void) splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 350, 300), kBounds, kList, kFoot, isMoved);
        (void) splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Up, 350, 300), kBounds, kList, kFoot, isMoved);
        Place (splitters);
        (void) splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 622, 300), kBounds, kList, kFoot, isMoved);
        (void) splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 0, 300), kBounds, kList, kFoot, isMoved);

        (void) splitters.PlaceColumns (kBounds, mapX);

        Assert::AreEqual ((int) (1000 * (BlockPanelSplitters::kMaxTree + BlockPanelSplitters::kMinTree * 2)), mapX);
    }


    TEST_METHOD (DragList_MovesTheListEnd)
    {
        BlockPanelSplitters  splitters;
        bool                 isMoved = false;
        RECT                 column  = { 200, 40, 600, 792 };



        Place (splitters);

        Assert::IsTrue (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 300, 402), kBounds, kList, kFoot, isMoved));
        Assert::IsTrue (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 300, 416), kBounds, kList, kFoot, isMoved));
        Assert::AreEqual (416, splitters.PlaceListEnd (column));
    }


    TEST_METHOD (DragHex_MovesTheHexEnd)
    {
        BlockPanelSplitters  splitters;
        bool                 isMoved = false;



        Place (splitters);

        Assert::IsTrue (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 442, 500), kBounds, kList, kFoot, isMoved));
        Assert::IsTrue (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 500, 500), kBounds, kList, kFoot, isMoved));
        Assert::AreEqual (500, splitters.PlaceHexEnd (kList));
    }


    TEST_METHOD (PressOffTheBars_IsNotTaken)
    {
        BlockPanelSplitters  splitters;
        bool                 isMoved = false;



        Place (splitters);

        Assert::IsFalse (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 50, 50), kBounds, kList, kFoot, isMoved));
        Assert::IsFalse (splitters.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 60, 50), kBounds, kList, kFoot, isMoved));
        Assert::IsFalse (isMoved);
    }


    TEST_METHOD (Cursor_FollowsTheBar)
    {
        BlockPanelSplitters  splitters;



        Place (splitters);

        Assert::IsTrue (splitters.GetCursor ({ 182, 300 }) == IDC_SIZEWE);
        Assert::IsTrue (splitters.GetCursor ({ 300, 402 }) == IDC_SIZENS);
        Assert::IsTrue (splitters.GetCursor ({ 50, 50 }) == nullptr);
    }


    TEST_METHOD (FitHex_KeepsRoomForTheHexView)
    {
        BlockPanelSplitters  splitters;



        splitters.FitHex (0.20f, 0.50f);

        Assert::AreEqual (400, splitters.PlaceHexEnd (kList));
    }
};
