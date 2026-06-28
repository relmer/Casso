#include "Pch.h"

#include "DxuiTreeView.h"

#include "UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFlatRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::RebuildFlatRows ()
{
    std::vector<int>  path;
    size_t            i = 0;



    m_flatRows.clear();

    for (i = 0; i < m_nodes.size(); ++i)
    {
        path.clear();
        path.push_back ((int) i);
        FlattenRecursive (m_nodes[i], path, 0);
    }

    if (m_highlight >= (int) m_flatRows.size())
    {
        m_highlight = m_flatRows.empty() ? -1 : (int) m_flatRows.size() - 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlattenRecursive
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::FlattenRecursive (const DxuiTreeNode & node, std::vector<int> & path, int depth)
{
    FlatRow  row;
    size_t   i = 0;



    row.pathStack = path;
    row.depth     = depth;
    m_flatRows.push_back (row);

    if (!node.expanded)
    {
        return;
    }

    for (i = 0; i < node.children.size(); ++i)
    {
        path.push_back ((int) i);
        FlattenRecursive (node.children[i], path, depth + 1);
        path.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PathFor
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> DxuiTreeView::PathFor (int flatIndex) const
{
    std::vector<int>  out;



    if (flatIndex < 0 || flatIndex >= (int) m_flatRows.size())
    {
        return out;
    }

    out = m_flatRows[(size_t) flatIndex].pathStack;
    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NodeAt
//
////////////////////////////////////////////////////////////////////////////////

const DxuiTreeNode * DxuiTreeView::NodeAt (int flatIndex) const
{
    return const_cast<DxuiTreeView *> (this)->NodeAtMutable (flatIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NodeAtMutable
//
////////////////////////////////////////////////////////////////////////////////

DxuiTreeNode * DxuiTreeView::NodeAtMutable (int flatIndex)
{
    std::vector<int>  path;
    DxuiTreeNode        * cursor = nullptr;
    size_t            i      = 0;
    int               idx    = 0;



    path = PathFor (flatIndex);
    if (path.empty())
    {
        return nullptr;
    }

    idx    = path[0];
    cursor = (idx >= 0 && idx < (int) m_nodes.size()) ? &m_nodes[(size_t) idx] : nullptr;

    for (i = 1; i < path.size() && cursor != nullptr; ++i)
    {
        idx    = path[i];
        cursor = (idx >= 0 && idx < (int) cursor->children.size())
                    ? &cursor->children[(size_t) idx]
                    : nullptr;
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInteractive
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::IsInteractive (int flatIndex) const
{
    const DxuiTreeNode * n = NodeAt (flatIndex);

    if (n == nullptr || !m_enabled)
    {
        return false;
    }

    return n->capabilityFlag == DxuiTreeCapabilityFlag::Optional;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTreeView::HitTestRow (int x, int y) const
{
    int  relY = 0;
    int  row  = 0;



    if (!m_enabled)
    {
        return -1;
    }

    if (x < m_boundsDip.left || x >= m_boundsDip.right || y < m_boundsDip.top || y >= m_boundsDip.bottom)
    {
        return -1;
    }

    if (m_rowHeightPx <= 0)
    {
        return -1;
    }

    relY = y - m_boundsDip.top;
    row  = relY / m_rowHeightPx;

    if (row < 0 || row >= (int) m_flatRows.size())
    {
        return -1;
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestTwisty
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::HitTestTwisty (int x, int y, int flatRow) const
{
    int    rowTop   = 0;
    int    rowDepth = 0;
    int    twistyX  = 0;



    UNREFERENCED_PARAMETER (y);

    if (flatRow < 0 || flatRow >= (int) m_flatRows.size())
    {
        return false;
    }

    rowDepth = m_flatRows[(size_t) flatRow].depth;
    rowTop   = m_boundsDip.top + flatRow * m_rowHeightPx;
    twistyX  = m_boundsDip.left + rowDepth * m_indentPx;

    UNREFERENCED_PARAMETER (rowTop);

    return x >= twistyX && x < twistyX + m_twistyPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestCheckbox
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::HitTestCheckbox (int x, int y, int flatRow) const
{
    int  rowDepth   = 0;
    int  checkboxX  = 0;



    UNREFERENCED_PARAMETER (y);

    if (flatRow < 0 || flatRow >= (int) m_flatRows.size())
    {
        return false;
    }

    rowDepth  = m_flatRows[(size_t) flatRow].depth;
    checkboxX = m_boundsDip.left + rowDepth * m_indentPx + m_twistyPx;

    return x >= checkboxX && x < checkboxX + m_checkboxPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::SetMouseHover (int x, int y)
{
    m_hoverRow = HitTestRow (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnLButtonDown (int x, int y)
{
    int  row = HitTestRow (x, y);



    if (row < 0)
    {
        return false;
    }

    m_pressedRow = row;
    m_highlight  = row;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnLButtonUp (int x, int y)
{
    int   row      = HitTestRow (x, y);
    int   pressed  = m_pressedRow;
    bool  consumed = false;



    m_pressedRow = -1;

    if (row < 0 || row != pressed)
    {
        return false;
    }

    if (HitTestTwisty (x, y, row))
    {
        DxuiTreeNode * n = NodeAtMutable (row);

        if (n != nullptr && !n->children.empty())
        {
            n->expanded = !n->expanded;
            RebuildFlatRows();
            consumed = true;
        }
    }
    else if (HitTestCheckbox (x, y, row))
    {
        ToggleRow (row);
        consumed = true;
    }
    else
    {
        consumed = true;   // row selection
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::ToggleRow (int flatRow)
{
    DxuiTreeNode * n = nullptr;



    if (!IsInteractive (flatRow))
    {
        return;
    }

    n = NodeAtMutable (flatRow);
    if (n == nullptr)
    {
        return;
    }

    n->checked = !n->checked;

    if (m_toggle)
    {
        m_toggle (n->label, n->checked);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnKey (WPARAM vk)
{
    DxuiTreeNode * n = nullptr;



    if (!m_enabled || !m_focused || m_flatRows.empty())
    {
        return false;
    }

    if (m_highlight < 0)
    {
        m_highlight = 0;
    }

    switch (vk)
    {
        case VK_UP:
            if (m_highlight > 0) { m_highlight--; }
            return true;

        case VK_DOWN:
            if (m_highlight < (int) m_flatRows.size() - 1) { m_highlight++; }
            return true;

        case VK_RIGHT:
            n = NodeAtMutable (m_highlight);
            if (n != nullptr && !n->children.empty() && !n->expanded)
            {
                n->expanded = true;
                RebuildFlatRows();
            }
            return true;

        case VK_LEFT:
            n = NodeAtMutable (m_highlight);
            if (n != nullptr && !n->children.empty() && n->expanded)
            {
                n->expanded = false;
                RebuildFlatRows();
            }
            return true;

        case VK_SPACE:
        case VK_RETURN:
            ToggleRow (m_highlight);
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    constexpr uint32_t  s_kRowIdle        = 0x00000000;
    constexpr uint32_t  s_kRowHover       = 0x33808080;
    constexpr uint32_t  s_kRowHighlight   = 0x44AACCFF;
    constexpr uint32_t  s_kBoxIdle        = 0xFF606060;
    constexpr uint32_t  s_kBoxLocked      = 0xFF505050;
    constexpr uint32_t  s_kCheckGlyph     = 0xFFFFFFFF;
    constexpr uint32_t  s_kCheckLocked    = 0xFFB0B0B0;
    constexpr uint32_t  s_kTwistyArgb     = 0xFFC0C0C0;
    constexpr uint32_t  s_kTextIdle       = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabled   = 0xFF707070;
    constexpr float     s_kCheckInset     = 3.0f;
    constexpr float     s_kFontDip        = 13.0f;
    constexpr float     s_kTwistyHeight   = 8.0f;

    HRESULT  hr        = S_OK;
    int      i         = 0;
    size_t   n         = m_flatRows.size();
    float    checkInset = m_scaler.Pxf (s_kCheckInset);
    float    fontDip    = m_scaler.Pxf (s_kFontDip);
    float    twistyHt   = m_scaler.Pxf (s_kTwistyHeight);
    float    textGap    = m_scaler.Pxf (4.0f);
    float    twistyPad  = m_scaler.Pxf (4.0f);



    for (i = 0; i < (int) n; ++i)
    {
        const FlatRow   & fr        = m_flatRows[(size_t) i];
        const DxuiTreeNode  * node      = NodeAt (i);
        float             rowY      = (float) (m_boundsDip.top + i * m_rowHeightPx);
        float             rowHeight = (float) m_rowHeightPx;
        float             twistyX   = (float) (m_boundsDip.left + fr.depth * m_indentPx);
        float             checkboxX = twistyX + (float) m_twistyPx;
        float             textX     = checkboxX + (float) m_checkboxPx + textGap;
        uint32_t          rowFill   = (i == m_highlight) ? s_kRowHighlight
                                       : (i == m_hoverRow ? s_kRowHover : s_kRowIdle);
        bool              hasChildren = (node != nullptr) && !node->children.empty();
        bool              interactive = (node != nullptr)
                                          && node->capabilityFlag == DxuiTreeCapabilityFlag::Optional
                                          && m_enabled;
        uint32_t          boxColor  = interactive ? s_kBoxIdle : s_kBoxLocked;
        uint32_t          glyphCol  = interactive ? s_kCheckGlyph : s_kCheckLocked;
        uint32_t          textCol   = interactive ? s_kTextIdle : s_kTextDisabled;

        if (rowFill != 0)
        {
            painter.FillRect ((float) m_boundsDip.left, rowY,
                              (float) (m_boundsDip.right - m_boundsDip.left), rowHeight, rowFill);
        }

        if (hasChildren)
        {
            // Geometric chevron: triangle rendered with horizontal
            // scanlines. Avoids Segoe UI Symbol's chevron glyph,
            // whose visual center sits below the line-box center
            // (no font metrics fix can correct that since the glyph
            // is intentionally drawn there for "play button" style
            // contexts). Triangle apex points right when collapsed,
            // down when expanded.
            //
            // Base spans the full triSize; apex-to-base distance is
            // a shorter triDepth so the triangle reads as a stubbier
            // Fluent-style chevron rather than a tall play button.
            float  triSize    = (float) m_checkboxPx * 0.55f;
            float  triDepth   = triSize * 0.65f;
            float  triCx      = twistyX + (float) m_twistyPx * 0.5f;
            float  triCy      = rowY + rowHeight * 0.5f;
            int    steps      = (int) triDepth;
            int    s          = 0;

            if (node->expanded)
            {
                // Down-pointing triangle: top edge full triSize wide,
                // apex at bottom, total height triDepth.
                float  topY = triCy - triDepth * 0.5f;
                for (s = 0; s < steps; ++s)
                {
                    float  t      = (float) s / (float) steps;
                    float  width  = triSize * (1.0f - t);
                    float  rowY2  = topY + (float) s;
                    painter.FillRect (triCx - width * 0.5f, rowY2, width, 1.0f, s_kTwistyArgb);
                }
            }
            else
            {
                // Right-pointing triangle: left edge full triSize tall,
                // apex at right, total width triDepth.
                float  leftX = triCx - triDepth * 0.5f;
                for (s = 0; s < steps; ++s)
                {
                    float  t      = (float) s / (float) steps;
                    float  height = triSize * (1.0f - t);
                    float  colX   = leftX + (float) s;
                    painter.FillRect (colX, triCy - height * 0.5f, 1.0f, height, s_kTwistyArgb);
                }
            }
        }

        painter.FillRect (checkboxX,
                          rowY + (rowHeight - (float) m_checkboxPx) * 0.5f,
                          (float) m_checkboxPx, (float) m_checkboxPx, boxColor);

        if (node != nullptr && node->checked)
        {
            // Real check-mark glyph, matching the standalone DxuiCheckbox
            // widget. Earlier impl drew a filled inner square which
            // read more like a focus ring than a tick.
            float  boxYPx = rowY + (rowHeight - (float) m_checkboxPx) * 0.5f;
            IGNORE_RETURN_VALUE (hr, text.DrawString (s_kpszCheckMark,
                                                      checkboxX,
                                                      boxYPx,
                                                      (float) m_checkboxPx,
                                                      (float) m_checkboxPx,
                                                      glyphCol,
                                                      (float) m_checkboxPx * 0.95f,
                                                      L"Segoe UI Symbol",
                                                      DxuiTextHAlign::Center,
                                                      DxuiTextVAlign::Center));
        }

        if (node != nullptr)
        {
            IGNORE_RETURN_VALUE (hr, text.DrawString (node->label.c_str(),
                                                      textX,
                                                      rowY,
                                                      (float) m_boundsDip.right - textX,
                                                      rowHeight,
                                                      textCol,
                                                      fontDip,
                                                      L"Segoe UI",
                                                      DxuiTextHAlign::Left,
                                                      DxuiTextVAlign::CenterOnCapHeight));
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTreeView::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTreeView::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (theme);
    static_cast<const DxuiTreeView *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTreeView::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        return false;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTreeView::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return OnKey (ev.vk);
}
