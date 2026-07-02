#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWidgetIDxuiControlTests
//
//  Verifies every Dxui/Widgets/* primitive correctly satisfies the
//  IDxuiControl contract: derives from the base, can be Add'd into a
//  DxuiPanel, has a working Layout that updates Bounds(), has a Paint
//  that is callable through the base virtual, and reports a sensible
//  AccessibleRole. These tests exist to lock the Phase 12.5 retrofit
//  in place so Phase 13's page conversions can lean on Add<T> /
//  Adopt patterns without worrying about widget plumbing.
//
////////////////////////////////////////////////////////////////////////////////



namespace
{
    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};


        out.left   = l;
        out.top    = t;
        out.right  = r;
        out.bottom = b;
        return out;
    }


    std::vector<std::vector<DxuiListView::Cell>>  MakeRows (int count)
    {
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        int                                           i = 0;



        for (i = 0; i < count; i++)
        {
            rows.push_back ({ DxuiListView::Cell{ L"row", false } });
        }

        return rows;
    }


    template <class TWidget>
    void  VerifyLayoutSetsBounds ()
    {
        TWidget        widget;
        DxuiDpiScaler  scaler;
        RECT           bounds = MakeRect (10, 20, 110, 60);


        scaler.SetDpi (96);
        static_cast<IDxuiControl &> (widget).Layout (bounds, scaler);

        Assert::AreEqual (bounds.left,   widget.Bounds().left);
        Assert::AreEqual (bounds.top,    widget.Bounds().top);
        Assert::AreEqual (bounds.right,  widget.Bounds().right);
        Assert::AreEqual (bounds.bottom, widget.Bounds().bottom);
    }


    template <class TWidget>
    void  VerifyPaintIsCallable ()
    {
        TWidget              widget;
        DxuiDpiScaler        scaler;
        MockDxuiPainter      painter;
        MockDxuiTextRenderer text;
        MockDxuiTheme        theme;
        RECT                 bounds = MakeRect (0, 0, 80, 24);


        scaler.SetDpi (96);
        static_cast<IDxuiControl &> (widget).Layout (bounds, scaler);
        // Must not throw; we don't assert on call counts because some
        // widgets short-circuit Paint when they hold no items / no
        // visible state.
        static_cast<IDxuiControl &> (widget).Paint (painter, text, theme);
    }


    template <class TWidget>
    void  VerifyAddsIntoPanel ()
    {
        DxuiPanel   panel;
        TWidget &   child = panel.Add<TWidget>();


        Assert::AreEqual ((size_t) 1, panel.ChildCount());
        Assert::AreEqual (static_cast<void *> (&child), static_cast<void *> (panel.Child (0)));
        Assert::AreEqual (static_cast<void *> (&panel), static_cast<void *> (child.Parent()));
    }
}





TEST_CLASS (DxuiWidgetIDxuiControlTests)
{
public:

    TEST_METHOD (DxuiButton_DerivesFromIDxuiControl)
    {
        DxuiButton  btn;

        IDxuiControl *  basePtr = &btn;
        Assert::IsNotNull (basePtr);
        Assert::AreEqual ((int) DxuiAccessibleRole::Button, (int) btn.AccessibleRole());
    }

    TEST_METHOD (DxuiButton_AddsIntoPanel)             { VerifyAddsIntoPanel       <DxuiButton>(); }
    TEST_METHOD (DxuiButton_LayoutSetsBounds)          { VerifyLayoutSetsBounds    <DxuiButton>(); }
    TEST_METHOD (DxuiButton_PaintIsCallable)           { VerifyPaintIsCallable     <DxuiButton>(); }

    TEST_METHOD (DxuiCheckbox_AddsIntoPanel)           { VerifyAddsIntoPanel       <DxuiCheckbox>(); }
    TEST_METHOD (DxuiCheckbox_LayoutSetsBounds)        { VerifyLayoutSetsBounds    <DxuiCheckbox>(); }
    TEST_METHOD (DxuiCheckbox_PaintIsCallable)         { VerifyPaintIsCallable     <DxuiCheckbox>(); }

    TEST_METHOD (DxuiToggle_AddsIntoPanel)             { VerifyAddsIntoPanel       <DxuiToggle>(); }
    TEST_METHOD (DxuiToggle_LayoutSetsBounds)          { VerifyLayoutSetsBounds    <DxuiToggle>(); }
    TEST_METHOD (DxuiToggle_PaintIsCallable)           { VerifyPaintIsCallable     <DxuiToggle>(); }

    TEST_METHOD (DxuiRadioGroup_AddsIntoPanel)         { VerifyAddsIntoPanel       <DxuiRadioGroup>(); }
    TEST_METHOD (DxuiRadioGroup_LayoutSetsBounds)      { VerifyLayoutSetsBounds    <DxuiRadioGroup>(); }
    TEST_METHOD (DxuiRadioGroup_PaintIsCallable)       { VerifyPaintIsCallable     <DxuiRadioGroup>(); }

    TEST_METHOD (DxuiLabel_AddsIntoPanel)              { VerifyAddsIntoPanel       <DxuiLabel>(); }
    TEST_METHOD (DxuiLabel_LayoutSetsBounds)           { VerifyLayoutSetsBounds    <DxuiLabel>(); }
    TEST_METHOD (DxuiLabel_PaintIsCallable)            { VerifyPaintIsCallable     <DxuiLabel>(); }

    TEST_METHOD (DxuiSlider_AddsIntoPanel)             { VerifyAddsIntoPanel       <DxuiSlider>(); }
    TEST_METHOD (DxuiSlider_LayoutSetsBounds)          { VerifyLayoutSetsBounds    <DxuiSlider>(); }
    TEST_METHOD (DxuiSlider_PaintIsCallable)           { VerifyPaintIsCallable     <DxuiSlider>(); }

    TEST_METHOD (DxuiDropdown_AddsIntoPanel)           { VerifyAddsIntoPanel       <DxuiDropdown>(); }
    TEST_METHOD (DxuiDropdown_LayoutSetsBounds)        { VerifyLayoutSetsBounds    <DxuiDropdown>(); }
    TEST_METHOD (DxuiDropdown_PaintIsCallable)         { VerifyPaintIsCallable     <DxuiDropdown>(); }

    TEST_METHOD (DxuiTabStrip_AddsIntoPanel)           { VerifyAddsIntoPanel       <DxuiTabStrip>(); }
    TEST_METHOD (DxuiTabStrip_LayoutSetsBounds)        { VerifyLayoutSetsBounds    <DxuiTabStrip>(); }
    TEST_METHOD (DxuiTabStrip_PaintIsCallable)         { VerifyPaintIsCallable     <DxuiTabStrip>(); }

    TEST_METHOD (DxuiTextInput_AddsIntoPanel)          { VerifyAddsIntoPanel       <DxuiTextInput>(); }
    TEST_METHOD (DxuiTextInput_LayoutSetsBounds)       { VerifyLayoutSetsBounds    <DxuiTextInput>(); }
    TEST_METHOD (DxuiTextInput_PaintIsCallable)        { VerifyPaintIsCallable     <DxuiTextInput>(); }

    TEST_METHOD (DxuiListView_AddsIntoPanel)           { VerifyAddsIntoPanel       <DxuiListView>(); }
    TEST_METHOD (DxuiListView_LayoutSetsBounds)        { VerifyLayoutSetsBounds    <DxuiListView>(); }
    TEST_METHOD (DxuiListView_PaintIsCallable)         { VerifyPaintIsCallable     <DxuiListView>(); }

    TEST_METHOD (DxuiTreeView_AddsIntoPanel)           { VerifyAddsIntoPanel       <DxuiTreeView>(); }
    TEST_METHOD (DxuiTreeView_LayoutSetsBounds)        { VerifyLayoutSetsBounds    <DxuiTreeView>(); }
    TEST_METHOD (DxuiTreeView_PaintIsCallable)         { VerifyPaintIsCallable     <DxuiTreeView>(); }

    TEST_METHOD (DxuiPopupMenu_AddsIntoPanel)          { VerifyAddsIntoPanel       <DxuiPopupMenu>(); }
    TEST_METHOD (DxuiPopupMenu_LayoutSetsBounds)       { VerifyLayoutSetsBounds    <DxuiPopupMenu>(); }
    TEST_METHOD (DxuiPopupMenu_PaintIsCallable)        { VerifyPaintIsCallable     <DxuiPopupMenu>(); }

    TEST_METHOD (DxuiTooltip_AddsIntoPanel)            { VerifyAddsIntoPanel       <DxuiTooltip>(); }
    TEST_METHOD (DxuiTooltip_LayoutSetsBounds)         { VerifyLayoutSetsBounds    <DxuiTooltip>(); }
    TEST_METHOD (DxuiTooltip_PaintIsCallable)          { VerifyPaintIsCallable     <DxuiTooltip>(); }

    TEST_METHOD (DxuiModalScrim_AddsIntoPanel)         { VerifyAddsIntoPanel       <DxuiModalScrim>(); }
    TEST_METHOD (DxuiModalScrim_LayoutSetsBounds)      { VerifyLayoutSetsBounds    <DxuiModalScrim>(); }
    TEST_METHOD (DxuiModalScrim_PaintIsCallable)       { VerifyPaintIsCallable     <DxuiModalScrim>(); }


    TEST_METHOD (DxuiButton_AccessibleNameReturnsLabel)
    {
        DxuiButton  btn;


        btn.SetLabel (L"OK");

        Assert::AreEqual (std::wstring (L"OK"), btn.AccessibleName());
    }


    TEST_METHOD (DxuiCheckbox_AccessibleRoleIsCheckbox)
    {
        DxuiCheckbox  cb;


        Assert::AreEqual ((int) DxuiAccessibleRole::Checkbox, (int) cb.AccessibleRole());
    }


    TEST_METHOD (DxuiDropdown_AccessibleRoleIsDropdown)
    {
        DxuiDropdown  dd;


        Assert::AreEqual ((int) DxuiAccessibleRole::Dropdown, (int) dd.AccessibleRole());
    }


    TEST_METHOD (DxuiSlider_AccessibleRoleIsSlider)
    {
        DxuiSlider  sl;


        Assert::AreEqual ((int) DxuiAccessibleRole::Slider, (int) sl.AccessibleRole());
    }


    TEST_METHOD (Panel_AdoptsExternallyOwnedButton)
    {
        DxuiPanel   panel;
        DxuiButton  external;


        panel.Adopt (external);

        Assert::AreEqual ((size_t) 1, panel.ChildCount());
        Assert::AreEqual (static_cast<void *> (&external), static_cast<void *> (panel.Child (0)));
        Assert::AreEqual (static_cast<void *> (&panel),    static_cast<void *> (external.Parent()));
    }


    TEST_METHOD (Panel_OnMouseDispatchesToWidgetChild)
    {
        DxuiPanel       panel;
        DxuiButton &    btn = panel.Add<DxuiButton>();
        DxuiDpiScaler   scaler;
        DxuiMouseEvent  down = {};
        bool            clicked = false;


        scaler.SetDpi (96);
        btn.SetOnClick ([&clicked]() { clicked = true; });
        // Panel children get their bounds via the panel's layout walk.
        // Here we drive the child layout directly so the test does not
        // need a layout policy installed on the panel.
        static_cast<IDxuiControl &> (btn).Layout (MakeRect (0, 0, 50, 20), scaler);

        down.kind        = DxuiMouseEventKind::Down;
        down.button      = DxuiMouseButton::Left;
        down.positionDip = POINT { 10, 10 };

        DxuiMouseEvent  up = {};
        up.kind        = DxuiMouseEventKind::Up;
        up.button      = DxuiMouseButton::Left;
        up.positionDip = POINT { 10, 10 };

        panel.OnMouse (down);
        panel.OnMouse (up);

        Assert::IsTrue (clicked);
    }


    TEST_METHOD (ListView_MoveWithoutButtonEndsStaleScrollbarDrag)
    {
        DxuiListView    list;
        DxuiDpiScaler   scaler;
        DxuiMouseEvent  down = {};
        DxuiMouseEvent  move = {};
        DxuiListView::ScrollbarMetrics  bar = {};



        scaler.SetDpi (96);
        list.SetColumns ({ DxuiListView::Column{ L"c", 100, false, DxuiTextHAlign::Left, true } });
        list.SetRows (MakeRows (200));
        static_cast<IDxuiControl &> (list).Layout (MakeRect (0, 0, 200, 200), scaler);

        bar = list.GetScrollbarGeometry();
        Assert::IsTrue (bar.visible);

        // Press the vertical thumb to start a drag, then a move with no
        // button held (cursor returned after a release that landed off the
        // dialog) must end the drag rather than keep tracking forever.
        down.kind        = DxuiMouseEventKind::Down;
        down.button      = DxuiMouseButton::Left;
        down.positionDip = POINT { bar.barX + 2, (int) bar.thumbTop + 2 };
        list.OnMouse (down);
        Assert::IsTrue (list.IsInteracting());

        move.kind        = DxuiMouseEventKind::Move;
        move.button      = DxuiMouseButton::None;
        move.positionDip = POINT { bar.barX + 2, (int) bar.thumbTop + 40 };
        list.OnMouse (move);
        Assert::IsFalse (list.IsInteracting());
    }
};
