#pragma once

#include "Pch.h"
#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows
//
//  The data rows: pressing, dragging and activating them in every view, and
//  drawing them in Details.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListRows
{
public:
    explicit DxuiListRows (DxuiListView & list);

    //  A row's highlight ends this far short of the last column's right edge,
    //  and the columns after the name are drawn at this strength, as
    //  Explorer's are against its full white.
    static constexpr int       s_kRowBoxEndInsetDip    = 4;
    static constexpr uint32_t  s_kSecondaryColumnAlpha = 0xCC000000u;

    //  A press on a row, the drag that extends the selection from it, and the
    //  release that activates it; EndPress ends the press before the release
    //  goes anywhere else.
    bool  OnPress   (const DxuiMouseEvent & ev, int row);
    bool  OnDrag    (int ly);
    bool  EndPress  ();
    bool  OnRelease (int row, int lx, int ly);

    //  A press on a cell's check box, where a cell's box is drawn and hit,
    //  and Space on the selected rows' boxes.
    bool  OnCheckPress   (int lx, int ly, int row);
    bool  GetCheckRectPx (int row, size_t column, RECT & outRect) const;
    bool  ToggleChecks   (bool apply);

    //  Whether a press on a check box other than an item's selects its row.
    void  SetSelectOnCheckPress    (bool select) { m_selectOnCheckPress = select; }

    //  Ends a drag that is extending the selection, keeping what it selected.
    void  EndDragSelect            () { m_dragSelecting = false; }
    bool  IsDragSelecting          () const { return m_dragSelecting; }
    void  ClearRowPress            () { m_rowPressed = false; }
    void  SetActivateOnDoubleClick (bool enabled) { m_activateOnDoubleClick = enabled; }

    void  Paint (IDxuiPainter                     & painter,
                 IDxuiTextRenderer                & text,
                 const DxuiListViewTypes::Palette & pal,
                 float                              x,
                 float                              y,
                 float                              layoutW,
                 int                                firstRow,
                 int                                lastRow,
                 const std::vector<int>           & colXPx,
                 const std::vector<int>           & colWPx) const;

    //  An item check box, centered in the height from rowTop, at left.
    void  PaintCheckBox (IDxuiPainter                     & painter,
                         IDxuiTextRenderer                & text,
                         DxuiListViewTypes::Cell::Check     check,
                         float                              left,
                         float                              rowTop,
                         float                              rowH,
                         bool                               overText = false) const;

private:
    //  Where a cell is drawn in its row, the row's ends and fill, and whether
    //  the cell is the first or last drawn and in the row's highlight.
    struct CellSpot
    {
        float     left    = 0.0f;
        float     width   = 0.0f;
        float     rowLeft = 0.0f;
        float     rowEnd  = 0.0f;
        float     top     = 0.0f;
        float     height  = 0.0f;
        uint32_t  rowFill = 0;
        bool      isFirst = false;
        bool      isLast  = false;
        bool      isLit   = false;
    };

    void      PaintCell      (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, const DxuiListViewTypes::Cell & cell, int row, size_t c, const CellSpot & place) const;
    void      FillTextRange  (IDxuiPainter & painter, IDxuiTextRenderer & text, const std::wstring & cellText, const wchar_t * face, float textX, float textW, int first, int last, float top, float height, float insetY, uint32_t fill, uint32_t edge) const;
    void      PaintTextRuns  (IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, const DxuiListViewTypes::Cell & cell, const wchar_t * face, uint32_t argb, uint32_t under, const std::vector<std::tuple<int, int, uint32_t>> & bands, float textX, float textW, float top, float height) const;
    uint32_t  GetReadableInk (uint32_t ink, uint32_t fill) const;

    //  Extends the selection from the row the drag began on to the row under
    //  the pointer.
    void  DragSelectTo (int ly);

    //  Where a column's check box starts in its cell.
    float GetCheckBoxLeftPx (size_t column, float padL) const;

    DxuiListView  & m_list;

    bool     m_activateOnDoubleClick = false;
    bool     m_dragSelecting         = false;
    bool     m_pressModified         = false;   // Ctrl or Shift was down for the row press
    bool     m_rowPressed            = false;   // set by a press on a row in the list; a release without one is ignored
    int      m_lastClickRow          = -1;
    int64_t  m_lastClickMs           = 0;
    bool     m_selectOnCheckPress    = true;

    //  GetReadableInk's results, by ink and fill.
    mutable std::unordered_map<uint64_t, uint32_t>  m_readableInks;
};
