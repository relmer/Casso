#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStackTests
//
//  Rows dragged by their grippers: the gap the other rows open as a row is
//  dragged, the reorder a drop reports, the slide after it, and the rows a
//  drag can't move. At 96 DPI, so pixels and DIPs are the same.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiReorderStackTests)
{
public:

    //  A row's control: where it was laid out, and the clicks it was sent.
    struct Row : public IDxuiControl
    {
        RECT  bounds = {};
        int   clicks = 0;

        void  Layout (const RECT & boundsDip, const DxuiDpiScaler &) override { bounds = boundsDip; }
        void  Paint  (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &) override {}

        bool  OnMouse (const DxuiMouseEvent & ev) override
        {
            POINT  point = ev.positionDip;

            if (ev.kind == DxuiMouseEventKind::Down && PtInRect (&bounds, point))
            {
                clicks++;
                return true;
            }

            return false;
        }
    };

    //  Four rows of 40 with gaps of 6, so a row's slot is 46 below the last.
    struct Rig
    {
        std::array<Row, 4>                      rows;
        DxuiReorderStack                        stack;
        DxuiDpiScaler                           scaler;
        int64_t                                 now     = 1000;
        std::vector<std::pair<size_t, size_t>>  moves;

        Rig (size_t movable)
        {
            stack.SetRows         ({ &rows[0], &rows[1], &rows[2], &rows[3] });
            stack.SetMovableCount (movable);
            stack.SetClock        ([this]() { return now; });
            stack.SetOnReorder    ([this] (size_t from, size_t to) { moves.emplace_back (from, to); });
            stack.Layout          (RECT { 0, 0, 300, stack.MeasureHeightPx (scaler) }, scaler);
        }

        void Mouse (DxuiMouseEventKind kind, int x, int y)
        {
            DxuiMouseEvent  ev;

            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = { x, y };

            stack.OnMouse (ev);
        }
    };

    static constexpr int  kPitch = 46;
    static constexpr int  kGrip  = 10;


    TEST_METHOD (Rows_AreLaidOutAfterTheGripperInTheirSlots)
    {
        Rig  rig (4);

        Assert::AreEqual (4 * 40 + 3 * 6, rig.stack.MeasureHeightPx (rig.scaler));
        Assert::AreEqual ((LONG) DxuiReorderStack::kGripDip, rig.rows[0].bounds.left);
        Assert::AreEqual ((LONG) (2 * kPitch + DxuiReorderStack::kPaddingDip), rig.rows[2].bounds.top);
    }


    //  Row 1 dragged down by two slots: rows 2 and 3 close up as it passes
    //  them, the drop reports 1 to 3, and the row slides from where it was
    //  let go to its new slot.
    TEST_METHOD (Drag_OpensAGapAndReportsTheMove)
    {
        Rig  rig (4);

        rig.Mouse (DxuiMouseEventKind::Down, kGrip, kPitch + 20);
        Assert::AreEqual (1, rig.stack.GetDraggedRow());
        Assert::IsTrue   (rig.stack.IsAnimating());

        rig.Mouse (DxuiMouseEventKind::Move, kGrip, kPitch + 20 + kPitch);
        rig.now += DxuiReorderStack::kSlideMs;
        rig.stack.Layout (RECT { 0, 0, 300, rig.stack.MeasureHeightPx (rig.scaler) }, rig.scaler);
        Assert::AreEqual ((LONG) (kPitch + DxuiReorderStack::kPaddingDip), rig.rows[2].bounds.top, L"row 2 moved up into the gap");

        rig.Mouse (DxuiMouseEventKind::Move, kGrip, kPitch + 20 + 2 * kPitch - 5);
        rig.Mouse (DxuiMouseEventKind::Up,   kGrip, kPitch + 20 + 2 * kPitch - 5);

        Assert::AreEqual ((size_t) 1, rig.moves.size());
        Assert::AreEqual ((size_t) 1, rig.moves[0].first);
        Assert::AreEqual ((size_t) 3, rig.moves[0].second);
        Assert::AreEqual (-1, rig.stack.GetDraggedRow());
        Assert::IsTrue   (rig.stack.IsAnimating(), L"the dropped row slides to its slot");

        rig.now += DxuiReorderStack::kSlideMs;
        Assert::IsFalse  (rig.stack.IsAnimating());
    }


    //  A drag past the last movable row stops there, and rows past the
    //  movable count have no gripper, so a press there goes to the row.
    TEST_METHOD (Drag_StaysAmongTheMovableRows)
    {
        Rig  rig (2);

        rig.Mouse (DxuiMouseEventKind::Down, kGrip, 20);
        rig.Mouse (DxuiMouseEventKind::Move, kGrip, 20 + 3 * kPitch);
        rig.Mouse (DxuiMouseEventKind::Up,   kGrip, 20 + 3 * kPitch);

        Assert::AreEqual ((size_t) 1, rig.moves.size());
        Assert::AreEqual ((size_t) 1, rig.moves[0].second, L"a drag stops at row 1");

        rig.Mouse (DxuiMouseEventKind::Down, kGrip, 3 * kPitch + 20);
        Assert::AreEqual (-1, rig.stack.GetDraggedRow());

        rig.Mouse (DxuiMouseEventKind::Down, 100, 3 * kPitch + 20);
        Assert::AreEqual (1, rig.rows[3].clicks, L"the row's control takes a press past the gripper");
    }


    //  With one movable row there is nothing to reorder, so no gripper.
    TEST_METHOD (OneMovableRow_HasNoGripper)
    {
        Rig  rig (1);

        rig.Mouse (DxuiMouseEventKind::Down, kGrip, 20);
        Assert::AreEqual (-1, rig.stack.GetDraggedRow());
    }
};
