#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;
struct DxuiKeyEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeader
//
//  The column header: its paint, column resizing and reordering by drag
//  and by keyboard, the columns' slides into place, and keyboard focus on
//  a column.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListHeader
{
public:
    explicit DxuiListHeader (DxuiListView & list);

    static constexpr int  s_kHeaderDragDip = 5;   // how far a header moves before it is a drag

    //  A press on a divider or a header, the drag that follows and its
    //  release; each returns whether the header took the event.
    bool  OnPress   (int lx, int ly);
    bool  OnDrag    (int lx);
    bool  OnRelease (int lx, int ly);
    bool  IsInteracting    () const { return m_resizeColumn >= 0 || m_headerPressCol >= 0; }
    bool  IsResizingColumn () const { return m_resizeColumn >= 0; }

    //  The order the columns are shown in, left to right.
    const std::vector<size_t> &  GetColumnOrder   () const { return m_columnOrder; }
    void                         MatchColumnCount (size_t count);

    bool  IsHeaderSliding   (int64_t nowMs) const;

    int   HitTestColumnResize (int xPx, int yPx, int tolerancePx) const;
    int   HitTestHeaderColumn (int xPx, int yPx) const;
    void                         SetColumnOrder (const std::vector<size_t> & order);

    int   GetColumnDropPosition (int xPx) const;
    static constexpr float  s_kExplorerTitleBandDip = 27.33f;   // the titles and dividers, from the bar's top
    static constexpr float  s_kExplorerDividerAlpha = 0.32f;   // the dividers: the text color at this strength
    static constexpr int    s_kKbResizeStepDip      = 8;

    void    EndHeaderPress          (int lx, int ly);
    void    PaintHeader             (IDxuiPainter           & painter,
                                     IDxuiTextRenderer    & text,
                                     const DxuiListViewTypes::Palette          & pal,
                                     float                    x,
                                     float                    y,
                                     float                    layoutW,
                                     const std::vector<int> & colXPx,
                                     const std::vector<int> & colWPx) const;
    void    PaintHeaderFocusMarkers (IDxuiPainter           & painter,
                                     const DxuiListViewTypes::Palette          & pal,
                                     float                    x,
                                     float                    y,
                                     const std::vector<int> & colXPx,
                                     const std::vector<int> & colWPx) const;
    void    ApplyKeyboardColumnFocus ();

    // The sub-focus and the paint markers move as a unit: a marker left
    // behind paints a focus cue on a control that no longer has focus.
    void    ClearColumnFocusMarkers  ();
    void    ReleaseKeyboardColumnFocus ();
    bool    HandleKeyboardColumnKey  (WPARAM vk);
    bool    OnKeyColumnResizeNav     (const DxuiKeyEvent & ev);
    bool    OnKeyBodyHeaderNav       (const DxuiKeyEvent & ev);
    void    ApplyBodyHeaderFocus     ();
    void    MoveHeaderFocus          (int dir);

    //  While a header is dragged, the others slide aside, as Explorer's do,
    //  to open a gap where it would land: each column eases from where it
    //  was to its new offset over a few frames.
    struct HeaderSlide
    {
        float    fromPx  = 0.0f;
        float    toPx    = 0.0f;
        int64_t  startMs = 0;
    };

    static constexpr int64_t  s_kHeaderSlideMs = 150;

    void   UpdateHeaderSlides  ();
    float  GetHeaderSlidePx    (size_t column, int64_t nowMs) const;

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    int                        m_sortColumn        = -1;
    bool                       m_sortDescending    = false;
    int                        m_focusedHeaderCol  = -1;
    int                        m_focusedDividerCol = -1;
    int                        m_kbColFocus        = -1;
    std::function<void (int)>         m_onSortColumn;
    std::function<void (const std::vector<size_t> &)>  m_onColumnsReordered;

    std::vector<HeaderSlide>  m_headerSlides;
    int                       m_headerDropPos     = -1;

    //  A header press: sorts on release in place, or moves its column once
    //  the pointer has traveled far enough to be a drag. A divider being
    //  dragged, and the last press on one, which a second press fits.
    std::vector<size_t>  m_columnOrder;
    int                  m_headerPressCol = -1;
    int                  m_headerPressXPx = 0;
    int                  m_headerDragXPx  = 0;
    bool                 m_headerDragging = false;
    int                  m_resizeColumn   = -1;
    int                  m_resizeStartXPx = 0;
    int                  m_resizeStartWPx = 0;
    int                  m_lastDividerCol = -1;
    int64_t              m_lastDividerMs  = 0;
};
