#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTreeView.h"

#include "Theme/DxuiColor.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFlatRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::RebuildFlatRows()
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



    // An out-of-range index yields an empty path, which NodeAtMutable reads as
    // "no such node".
    if (flatIndex >= 0 && flatIndex < (int) m_flatRows.size())
    {
        out = m_flatRows[(size_t) flatIndex].pathStack;
    }

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

    if (!path.empty())
    {
        idx    = path[0];
        cursor = (idx >= 0 && idx < (int) m_nodes.size()) ? &m_nodes[(size_t) idx] : nullptr;

        for (i = 1; i < path.size() && cursor != nullptr; ++i)
        {
            idx    = path[i];
            cursor = (idx >= 0 && idx < (int) cursor->children.size())
                        ? &cursor->children[(size_t) idx]
                        : nullptr;
        }
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



    return n != nullptr
        && m_enabled
        && n->capabilityFlag == DxuiTreeCapabilityFlag::Optional;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTreeView::HitTestRow (int x, int y) const
{
    int   relY    = 0;
    int   row     = -1;
    bool  inRange = false;



    inRange = m_enabled
              && x >= m_boundsDip.left && x < m_boundsDip.right
              && y >= m_boundsDip.top  && y < m_boundsDip.bottom
              && m_rowHeightPx > 0;

    if (inRange)
    {
        relY = y - m_boundsDip.top;
        row  = relY / m_rowHeightPx;

        // Past the last populated row is a miss, not the last row.
        if (row >= (int) m_flatRows.size())
        {
            row = -1;
        }
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
    bool   isHit    = false;



    UNREFERENCED_PARAMETER (y);

    if (flatRow >= 0 && flatRow < (int) m_flatRows.size())
    {
        rowDepth = m_flatRows[(size_t) flatRow].depth;
        rowTop   = m_boundsDip.top + flatRow * m_rowHeightPx;
        twistyX  = m_boundsDip.left + rowDepth * m_indentPx;
        isHit    = x >= twistyX && x < twistyX + m_twistyPx;

        UNREFERENCED_PARAMETER (rowTop);
    }

    return isHit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestCheckbox
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::HitTestCheckbox (int x, int y, int flatRow) const
{
    int   rowDepth  = 0;
    int   checkboxX = 0;
    bool  isHit     = false;



    UNREFERENCED_PARAMETER (y);

    if (flatRow >= 0 && flatRow < (int) m_flatRows.size())
    {
        rowDepth  = m_flatRows[(size_t) flatRow].depth;
        checkboxX = m_boundsDip.left + rowDepth * m_indentPx + m_twistyPx;
        isHit     = x >= checkboxX && x < checkboxX + m_checkboxPx;
    }

    return isHit;
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
    int   row   = HitTestRow (x, y);
    bool  isHit = (row >= 0);



    if (isHit)
    {
        m_pressedRow = row;
        m_highlight  = row;
    }

    return isHit;
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

    // A release only counts on the row the press started on.
    if (row >= 0 && row == pressed)
    {
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



    if (IsInteractive (flatRow))
    {
        n = NodeAtMutable (flatRow);
    }

    if (n != nullptr)
    {
        n->checked = !n->checked;

        if (m_toggle)
        {
            m_toggle (n->label, n->checked);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnKey (WPARAM vk)
{
    DxuiTreeNode *  n        = nullptr;
    bool            isActive = false;
    bool            handled  = false;



    isActive = m_enabled && m_focused && !m_flatRows.empty();

    if (isActive && m_highlight < 0)
    {
        m_highlight = 0;
    }

    if (isActive)
    {
        handled = true;   // cleared by the default arm below

        switch (vk)
        {
            case VK_UP:
                if (m_highlight > 0) { m_highlight--; }
                break;

            case VK_DOWN:
                if (m_highlight < (int) m_flatRows.size() - 1) { m_highlight++; }
                break;

            case VK_RIGHT:
                n = NodeAtMutable (m_highlight);
                if (n != nullptr && !n->children.empty() && !n->expanded)
                {
                    n->expanded = true;
                    RebuildFlatRows();
                }
                break;

            case VK_LEFT:
                n = NodeAtMutable (m_highlight);
                if (n != nullptr && !n->children.empty() && n->expanded)
                {
                    n->expanded = false;
                    RebuildFlatRows();
                }
                break;

            case VK_SPACE:
            case VK_RETURN:
                ToggleRow (m_highlight);
                break;

            default:
                handled = false;
                break;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    uint32_t  s_kRowIdle        = 0x00000000;
    uint32_t  s_kRowHover       = (theme.HoverBackground()    & 0x00FFFFFFu) | 0x33000000u;
    uint32_t  s_kRowHighlight   = (theme.SelectionBackground() & 0x00FFFFFFu) | 0x44000000u;
    uint32_t  s_kBoxIdle        = theme.ButtonIdle();
    uint32_t  s_kBoxLocked      = DxuiColor::TintForContrast (theme.Background(), 1.6f);
    uint32_t  s_kCheckGlyph     = theme.ButtonText();
    uint32_t  s_kCheckLocked    = theme.ForegroundDisabled();
    uint32_t  s_kTwistyArgb     = theme.ForegroundMuted();
    uint32_t  s_kTextIdle       = theme.Foreground();
    uint32_t  s_kTextDisabled   = theme.ForegroundDisabled();
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
                                                      DxuiTheme::kBodyFace,
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
//  DxuiTreeView::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        break;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        break;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        break;
    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTreeView::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTreeView::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = OnKey (ev.vk);
    }

    return handled;
}
