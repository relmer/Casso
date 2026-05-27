#include "Pch.h"

#include "TreeView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFlatRows
//
////////////////////////////////////////////////////////////////////////////////

void TreeView::RebuildFlatRows ()
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

void TreeView::FlattenRecursive (const TreeNode & node, std::vector<int> & path, int depth)
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

std::vector<int> TreeView::PathFor (int flatIndex) const
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

const TreeNode * TreeView::NodeAt (int flatIndex) const
{
    return const_cast<TreeView *> (this)->NodeAtMutable (flatIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NodeAtMutable
//
////////////////////////////////////////////////////////////////////////////////

TreeNode * TreeView::NodeAtMutable (int flatIndex)
{
    std::vector<int>  path;
    TreeNode        * cursor = nullptr;
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

bool TreeView::IsInteractive (int flatIndex) const
{
    const TreeNode * n = NodeAt (flatIndex);

    if (n == nullptr || !m_enabled)
    {
        return false;
    }

    return n->capabilityFlag == TreeCapabilityFlag::Optional;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestRow
//
////////////////////////////////////////////////////////////////////////////////

int TreeView::HitTestRow (int x, int y) const
{
    int  relY = 0;
    int  row  = 0;



    if (!m_enabled)
    {
        return -1;
    }

    if (x < m_rect.left || x >= m_rect.right || y < m_rect.top || y >= m_rect.bottom)
    {
        return -1;
    }

    if (m_rowHeightPx <= 0)
    {
        return -1;
    }

    relY = y - m_rect.top;
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

bool TreeView::HitTestTwisty (int x, int y, int flatRow) const
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
    rowTop   = m_rect.top + flatRow * m_rowHeightPx;
    twistyX  = m_rect.left + rowDepth * m_indentPx;

    UNREFERENCED_PARAMETER (rowTop);

    return x >= twistyX && x < twistyX + m_twistyPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestCheckbox
//
////////////////////////////////////////////////////////////////////////////////

bool TreeView::HitTestCheckbox (int x, int y, int flatRow) const
{
    int  rowDepth   = 0;
    int  checkboxX  = 0;



    UNREFERENCED_PARAMETER (y);

    if (flatRow < 0 || flatRow >= (int) m_flatRows.size())
    {
        return false;
    }

    rowDepth  = m_flatRows[(size_t) flatRow].depth;
    checkboxX = m_rect.left + rowDepth * m_indentPx + m_twistyPx;

    return x >= checkboxX && x < checkboxX + m_checkboxPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void TreeView::SetMouseHover (int x, int y)
{
    m_hoverRow = HitTestRow (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool TreeView::OnLButtonDown (int x, int y)
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

bool TreeView::OnLButtonUp (int x, int y)
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
        TreeNode * n = NodeAtMutable (row);

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

void TreeView::ToggleRow (int flatRow)
{
    TreeNode * n = nullptr;



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

bool TreeView::OnKey (WPARAM vk)
{
    TreeNode * n = nullptr;



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

void TreeView::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
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
        const TreeNode  * node      = NodeAt (i);
        float             rowY      = (float) (m_rect.top + i * m_rowHeightPx);
        float             rowHeight = (float) m_rowHeightPx;
        float             twistyX   = (float) (m_rect.left + fr.depth * m_indentPx);
        float             checkboxX = twistyX + (float) m_twistyPx;
        float             textX     = checkboxX + (float) m_checkboxPx + textGap;
        uint32_t          rowFill   = (i == m_highlight) ? s_kRowHighlight
                                       : (i == m_hoverRow ? s_kRowHover : s_kRowIdle);
        bool              hasChildren = (node != nullptr) && !node->children.empty();
        bool              interactive = (node != nullptr)
                                          && node->capabilityFlag == TreeCapabilityFlag::Optional
                                          && m_enabled;
        uint32_t          boxColor  = interactive ? s_kBoxIdle : s_kBoxLocked;
        uint32_t          glyphCol  = interactive ? s_kCheckGlyph : s_kCheckLocked;
        uint32_t          textCol   = interactive ? s_kTextIdle : s_kTextDisabled;

        if (rowFill != 0)
        {
            painter.FillRect ((float) m_rect.left, rowY,
                              (float) (m_rect.right - m_rect.left), rowHeight, rowFill);
        }

        if (hasChildren)
        {
            // Fluent-style chevron: '⏷' (U+23F7) when expanded, '⏵'
            // (U+23F5) when collapsed. Replaces the previous filled
            // square placeholder. Sized to match the checkbox so it
            // reads as a comparable affordance, not a separator.
            const wchar_t *  chevron     = node->expanded ? L"\u23F7" : L"\u23F5";
            float            chevronH    = (float) m_checkboxPx;
            float            chevronYPx  = rowY + (rowHeight - chevronH) * 0.5f;
            IGNORE_RETURN_VALUE (hr, text.DrawString (chevron,
                                                      twistyX,
                                                      chevronYPx,
                                                      (float) m_twistyPx,
                                                      chevronH,
                                                      s_kTwistyArgb,
                                                      chevronH,
                                                      L"Segoe UI Symbol",
                                                      DwriteTextRenderer::HAlign::Center,
                                                      DwriteTextRenderer::VAlign::Center));
        }

        painter.FillRect (checkboxX,
                          rowY + (rowHeight - (float) m_checkboxPx) * 0.5f,
                          (float) m_checkboxPx, (float) m_checkboxPx, boxColor);

        if (node != nullptr && node->checked)
        {
            // Real check-mark glyph, matching the standalone Checkbox
            // widget. Earlier impl drew a filled inner square which
            // read more like a focus ring than a tick.
            float  boxYPx = rowY + (rowHeight - (float) m_checkboxPx) * 0.5f;
            IGNORE_RETURN_VALUE (hr, text.DrawString (L"\u2713",
                                                      checkboxX,
                                                      boxYPx,
                                                      (float) m_checkboxPx,
                                                      (float) m_checkboxPx,
                                                      glyphCol,
                                                      (float) m_checkboxPx * 0.95f,
                                                      L"Segoe UI Symbol",
                                                      DwriteTextRenderer::HAlign::Center,
                                                      DwriteTextRenderer::VAlign::Center));
        }

        if (node != nullptr)
        {
            IGNORE_RETURN_VALUE (hr, text.DrawString (node->label.c_str(),
                                                      textX,
                                                      rowY,
                                                      (float) m_rect.right - textX,
                                                      rowHeight,
                                                      textCol,
                                                      fontDip,
                                                      L"Segoe UI",
                                                      DwriteTextRenderer::HAlign::Left,
                                                      DwriteTextRenderer::VAlign::Center));
        }
    }
}
