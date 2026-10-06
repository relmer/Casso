#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarLiftTests
//
//  A toolbar carried by its handle: it lifts off while it is carried and
//  settles back when it is put down, with the move cursor over the handle;
//  and a floating toolbar's far end, dragged up to the edge across from it,
//  snaps it into that edge, as its handle does near any edge. Every lift
//  here takes its time and its animation flag as arguments, or the host's
//  animations are pinned, so none of them reads the system's setting.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarLiftTests)
{
public:

    static constexpr int64_t  kStartMs = 5000;
    static constexpr float    kNear    = 0.001f;
    static constexpr RECT     kArea    = { 0, 100, 1000, 800 };
    static constexpr int      kBandPx  = 66;


    static void SetUpBar (DxuiToolbar & bar)
    {
        std::vector<DxuiToolbar::Entry>  entries (1);
        auto                             command = std::make_shared<DxuiCommand>();

        command->id     = 1;
        command->label  = L"Run";
        entries[0].command = command;

        bar.SetGrabHandle (true);
        bar.SetEntries    (std::move (entries));
    }


    TEST_METHOD (ALiftWithAnimationsOffIsWholeAtOnceAndPuttingDownEndsIt)
    {
        DxuiToolbar  bar;

        Assert::AreEqual (0.0f, bar.GetLiftLevel (kStartMs), kNear, L"on its place");

        bar.SetLifted (true, kStartMs, false);
        Assert::IsTrue   (bar.IsLifted());
        Assert::AreEqual (1.0f, bar.GetLiftLevel (kStartMs), kNear, L"animations off: lifted at once");
        Assert::IsFalse  (bar.IsLiftSettling (kStartMs), L"and nothing left to draw toward");

        bar.SetLifted (false, kStartMs + 1, false);
        Assert::AreEqual (0.0f, bar.GetLiftLevel (kStartMs + 1), kNear, L"put down, it is back at once");
    }


    TEST_METHOD (ALiftWithAnimationsOnRisesOverTheMenuTime)
    {
        DxuiToolbar  bar;
        float        midway = 0.0f;

        bar.SetLifted (true, kStartMs, true);

        midway = bar.GetLiftLevel (kStartMs + DxuiPopupMenu::kRevealMs / 2);

        Assert::AreEqual (0.0f, bar.GetLiftLevel (kStartMs), kNear, L"it starts on its place");
        Assert::IsTrue   (midway > 0.0f && midway < 1.0f,      L"it is partway up halfway through");
        Assert::IsTrue   (bar.IsLiftSettling (kStartMs + 1),    L"and asks for frames while it rises");
        Assert::AreEqual (1.0f, bar.GetLiftLevel (kStartMs + DxuiPopupMenu::kRevealMs), kNear, L"lifted once the time is up");
    }


    TEST_METHOD (TheHandleShowsTheMoveCursor)
    {
        DxuiToolbar    bar;
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        SetUpBar (bar);
        bar.Layout (RECT { 0, 0, 600, 42 }, scaler);

        Assert::IsTrue (bar.GetCursorForPoint (POINT { 4, 20 })   == IDC_SIZEALL, L"over the handle");
        Assert::IsTrue (bar.GetCursorForPoint (POINT { 300, 20 }) == nullptr,     L"the entries keep the arrow");
    }


    TEST_METHOD (CarryingADockedToolbarByItsHandleLiftsIt)
    {
        DxuiToolbar      bar;
        DxuiToolbarHost  host;
        DxuiDpiScaler    scaler;
        DxuiMouseEvent   ev;
        RECT             grip = {};

        scaler.SetDpi (96);
        SetUpBar (bar);

        host.Attach               (nullptr, &bar, nullptr, nullptr);
        host.SetAnimationsEnabled (false);
        host.Layout               (kArea, kArea, scaler);

        grip           = bar.GetGripRect();
        ev.kind        = DxuiMouseEventKind::Down;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { (grip.left + grip.right) / 2, (grip.top + grip.bottom) / 2 };

        Assert::IsTrue (host.RouteDrag (ev), L"the handle takes the press");
        Assert::IsTrue (bar.IsLifted(),      L"and the toolbar lifts");

        ev.kind = DxuiMouseEventKind::Up;
        Assert::IsTrue  (host.RouteDrag (ev), L"the release ends the drag");
        Assert::IsFalse (bar.IsLifted(),      L"and puts it down");
    }


    TEST_METHOD (AFarEndDraggedUpToTheEdgeAcrossFromItPicksThatEdge)
    {
        DxuiToolbarDock::Edge  edge   = DxuiToolbarDock::Edge::Top;
        RECT                   before = { 100, 300, 900, 382 };
        RECT                   after  = { 150, 300, 950, 382 };
        RECT                   tallA  = { 300, 120, 382, 700 };
        RECT                   tallB  = { 300, 150, 382, 760 };

        Assert::IsTrue  (DxuiToolbarDock::TryGetFarEndEdge (after, before, false, kArea, kBandPx, edge), L"lying down, its right end reaches the right edge");
        Assert::IsTrue  (edge == DxuiToolbarDock::Edge::Right);

        Assert::IsFalse (DxuiToolbarDock::TryGetFarEndEdge (before, after, false, kArea, kBandPx, edge), L"moving away from the edge is not coming up to it");
        Assert::IsFalse (DxuiToolbarDock::TryGetFarEndEdge (before, RECT { 50, 300, 850, 382 }, false, kArea, kBandPx, edge), L"nor is an end still well short of it");
        Assert::IsFalse (DxuiToolbarDock::TryGetFarEndEdge (RECT { 300, 300, 1100, 382 }, RECT { 250, 300, 1050, 382 }, false, kArea, kBandPx, edge),
                         L"an end already hanging past the edge has gone by it");
        Assert::IsFalse (DxuiToolbarDock::TryGetFarEndEdge (RECT { 150, 900, 950, 982 }, RECT { 100, 900, 900, 982 }, false, kArea, kBandPx, edge),
                         L"a toolbar below the window is not across from its right edge");

        Assert::IsTrue  (DxuiToolbarDock::TryGetFarEndEdge (tallB, tallA, true, kArea, kBandPx, edge), L"standing up, its bottom end reaches the bottom edge");
        Assert::IsTrue  (edge == DxuiToolbarDock::Edge::Bottom);
    }


    TEST_METHOD (APlaceMadeOnAnEdgeKeepsItsOffsetInDips)
    {
        DxuiToolbarDock  dock = DxuiToolbarDock::MakeDocked (DxuiToolbarDock::Edge::Right, 150, 144);

        Assert::IsTrue   (dock.edge == DxuiToolbarDock::Edge::Right);
        Assert::IsFalse  (dock.floating);
        Assert::AreEqual (100, dock.offsetDip, L"150 pixels at 150%");
        Assert::AreEqual (0,   DxuiToolbarDock::MakeDocked (DxuiToolbarDock::Edge::Bottom, -20, 96).offsetDip, L"never before the edge's start");
    }
};
