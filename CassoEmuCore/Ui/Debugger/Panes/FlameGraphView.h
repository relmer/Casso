#pragma once

#include "Pch.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FlameGraphView
//
//  The profile's call tree as rows of bars, Top level across the top and
//  each routine under its caller, as wide as its total. A zoomed routine
//  spans the width under its callers' bars. A click on a bar selects its
//  routine, outlining the bar, and moves the disassembly to it; a
//  double-click zooms to it, so a double-click on Top level zooms out.
//
//  Bounds are pixels, like every Dxui control.
//
////////////////////////////////////////////////////////////////////////////////

class FlameGraphView : public IDxuiControl
{
public:
    struct Bar
    {
        size_t  node  = 0;
        int     row   = 0;
        float   x     = 0.0f;
        float   width = 0.0f;
    };

    using ActivateFn = std::function<void (Word address)>;
    using SelectFn   = std::function<void (size_t routine)>;
    using ZoomFn     = std::function<void (size_t node)>;

    static constexpr int  kRowDip   = 20;
    static constexpr int  kLabelDip = 24;

    void  SetData       (const std::optional<ProfileData> & profile);
    void  SetOnActivate (ActivateFn activate) { m_activate = std::move (activate); }
    void  SetOnSelect   (SelectFn select)     { m_select   = std::move (select); }
    void  SetOnZoom     (ZoomFn zoom)         { m_onZoom   = std::move (zoom); }
    void  SetZoom       (size_t node);

    size_t                    GetZoom () const { return m_zoom; }
    std::optional<size_t>     GetSelected () const { return m_selected; }
    const std::vector<Bar> &  GetBars () const { return m_bars; }

    //  The bars over a width: the zoomed node's callers full width, one row
    //  each, then the zoomed node, then its callees by their share of its
    //  total. A bar under a pixel wide is left out, with its callees.
    static std::vector<Bar>  LayoutBars (const std::vector<ProfileTreeEntry> & tree, size_t zoom, float left, float width);

    //  A routine's color, one of a few hues by its place in the routines.
    static uint32_t  GetRoutineColor (size_t routine);

    std::optional<size_t>  FindBarAt   (POINT point) const;
    bool                   TryGetTipAt (POINT point, std::wstring & text) const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Flame graph"; }

private:
    struct Click
    {
        ULONGLONG  time = 0;
        POINT      at   = {};
        size_t     node = 0;
    };

    void   Relayout   ();

    static std::vector<uint64_t>  GetPath  (const std::vector<ProfileTreeEntry> & tree, const std::vector<ProfileRoutineEntry> & routines, size_t node);
    static std::optional<size_t>  FindPath (const std::vector<ProfileTreeEntry> & tree, const std::vector<ProfileRoutineEntry> & routines, const std::vector<uint64_t> & path);
    float  GetRowTop  (int row) const;

    std::vector<ProfileTreeEntry>     m_tree;
    std::vector<ProfileRoutineEntry>  m_routines;
    std::vector<Bar>                  m_bars;
    size_t                            m_zoom  = 0;
    std::optional<size_t>             m_selected;
    std::optional<Click>              m_click;
    ActivateFn                        m_activate;
    SelectFn                          m_select;
    ZoomFn                            m_onZoom;
    DxuiDpiScaler                     m_scaler;
};
