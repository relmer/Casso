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
//  Resolves a flat row index to the node it names, for modification.
//
//  The tree is stored as nested vectors while the view addresses rows by a
//  single flat index, so every lookup has to walk a child PATH down from the
//  roots. That path is what PathFor computes; this function is the descent.
//
//  It cannot be written in terms of the const NodeAt without casting away
//  constness, and duplicating the descent is the lesser evil -- the walk is
//  short and the cast would be a lie about the object's constness.
//
//  Every index is bounds-checked at each level, so a stale flat index left
//  over from a collapsed or reloaded tree yields null rather than reading past
//  a child vector.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTreeNode * DxuiTreeView::NodeAtMutable (int flatIndex)
{
    std::vector<int>    path;
    DxuiTreeNode      * cursor = nullptr;
    size_t              i      = 0;
    int                 idx    = 0;



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
//  Which visible row a point falls on, or -1.
//
//  A single divide suffices here -- unlike the menu bar's varying entry
//  heights -- because every tree row is the same height.
//
//  A point BELOW the last populated row is a miss, not the last row. Without
//  that clamp, clicking the empty space under a short tree would select its
//  final item, which reads as the control selecting something the user did not
//  click on.
//
//  A disabled tree reports a miss for every point, so the enabled test lives
//  here rather than at each call site.
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
//  Acts on the release: expand / collapse the twisty, toggle the checkbox, or
//  select the row.
//
//  A release only counts on the row the PRESS started on. Pressing one row,
//  dragging to another, and releasing there does nothing -- the standard
//  cancel gesture, and the reason the pressed row is remembered at all.
//
//  The pressed row is cleared FIRST, before any of the branches, so it cannot
//  be left set by an early exit and leak into the next click.
//
//  Which of the three actions fires is decided by sub-region: twisty, then
//  checkbox, then anything else is a selection. Ordering matters because the
//  twisty and checkbox both sit inside the row's own rect.
//
//  A twisty click on a childless node is consumed but does nothing, rather
//  than falling through to selection -- clicking where an expander would be is
//  not a request to select.
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
//  Keyboard navigation: the standard tree bindings.
//
//  Left and Right COLLAPSE and EXPAND rather than moving the highlight, which
//  is what every tree control does and what makes a keyboard user able to
//  reach a nested node at all. Up and Down move through the flat row list, so
//  they naturally traverse into and out of expanded subtrees without knowing
//  anything about depth.
//
//  The highlight is seeded to row 0 on the first key while focused, so a tree
//  that has never been clicked still responds to an arrow press instead of
//  requiring a mouse first.
//
//  Space and Enter toggle through the same ToggleRow the mouse uses, so the
//  interactivity rule -- only Optional nodes may be changed -- is enforced in
//  one place for both input paths.
//
//  Expanding or collapsing rebuilds the flat rows immediately, since every
//  index in play refers to that list.
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
//  Draws every visible row: background, twisty, checkbox, and label.
//
//  Indentation comes from the flat row's own recorded DEPTH, so painting never
//  walks the tree -- the flat list already carries everything a row needs to
//  place itself.
//
//  Row x-positions are derived left to right (twisty, then checkbox, then
//  label) so the three regions cannot disagree with the hit tests, which
//  compute the same offsets the same way.
//
//  The twisty is drawn GEOMETRICALLY, as a triangle filled with one-pixel
//  scanlines, rather than with Segoe UI Symbol's chevron glyph. That glyph's
//  visual center sits below its line-box center -- deliberately, for
//  play-button contexts -- and no font metric corrects it, so a glyph chevron
//  always paints slightly low against the row. The proportions here (a base
//  spanning the full size, a shorter apex depth) make it read as a stubby
//  Fluent chevron rather than a tall play button.
//
//  The check mark IS a glyph, matching the standalone checkbox widget so the
//  two controls agree. An earlier version drew a filled inner square, which
//  read as a focus ring rather than a tick.
//
//  Interactivity -- Optional capability plus an enabled tree -- selects the
//  box, glyph, and text colors together, so a locked node reads as locked in
//  every part of the row instead of only in its label.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTreeView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    uint32_t         s_kRowIdle      = 0x00000000;
    uint32_t         s_kRowHover     = (theme.HoverBackground()    & 0x00FFFFFFu) | 0x33000000u;
    uint32_t         s_kRowHighlight = (theme.SelectionBackground() & 0x00FFFFFFu) | 0x44000000u;
    uint32_t         s_kBoxIdle      = theme.ButtonIdle();
    uint32_t         s_kBoxLocked    = DxuiColor::TintForContrast (theme.Background(), 1.6f);
    uint32_t         s_kCheckGlyph   = theme.ButtonText();
    uint32_t         s_kCheckLocked  = theme.ForegroundDisabled();
    uint32_t         s_kTwistyArgb   = theme.ForegroundMuted();
    uint32_t         s_kTextIdle     = theme.Foreground();
    uint32_t         s_kTextDisabled = theme.ForegroundDisabled();
    constexpr float  s_kCheckInset   = 3.0f;
    constexpr float  s_kFontDip      = 13.0f;
    constexpr float  s_kTwistyHeight = 8.0f;



    HRESULT  hr         = S_OK;
    int      i          = 0;
    size_t   n          = m_flatRows.size();
    float    checkInset = m_scaler.Pxf (s_kCheckInset);
    float    fontDip    = m_scaler.Pxf (s_kFontDip);
    float    twistyHt   = m_scaler.Pxf (s_kTwistyHeight);
    float    textGap    = m_scaler.Pxf (4.0f);
    float    twistyPad  = m_scaler.Pxf (4.0f);



    for (i = 0; i < (int) n; ++i)
    {
        const FlatRow       & fr          = m_flatRows[(size_t) i];
        const DxuiTreeNode  * node        = NodeAt (i);
        float                 rowY        = (float) (m_boundsDip.top + i * m_rowHeightPx);
        float                 rowHeight   = (float) m_rowHeightPx;
        float                 twistyX     = (float) (m_boundsDip.left + fr.depth * m_indentPx);
        float                 checkboxX   = twistyX + (float) m_twistyPx;
        float                 textX       = checkboxX + (float) m_checkboxPx + textGap;
        bool                  hasChildren = false;
        uint32_t              boxColor    = 0;
        uint32_t              glyphCol    = 0;
        uint32_t              textCol     = 0;
        uint32_t          rowFill   = (i == m_highlight) ? s_kRowHighlight
                                       : (i == m_hoverRow ? s_kRowHover : s_kRowIdle);
        hasChildren = (node != nullptr) && !node->children.empty();
        bool              interactive = (node != nullptr)
                                          && node->capabilityFlag == DxuiTreeCapabilityFlag::Optional
                                          && m_enabled;
        boxColor = interactive ? s_kBoxIdle : s_kBoxLocked;
        glyphCol = interactive ? s_kCheckGlyph : s_kCheckLocked;
        textCol = interactive ? s_kTextIdle : s_kTextDisabled;

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
            hr = text.DrawString (s_kpszCheckMark,
                                  checkboxX,
                                  boxYPx,
                                  (float) m_checkboxPx,
                                  (float) m_checkboxPx,
                                  glyphCol,
                                  (float) m_checkboxPx * 0.95f,
                                  L"Segoe UI Symbol",
                                  DxuiTextHAlign::Center,
                                  DxuiTextVAlign::Center);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (node != nullptr)
        {
            hr = text.DrawString (node->label.c_str(),
                                  textX,
                                  rowY,
                                  (float) m_boundsDip.right - textX,
                                  rowHeight,
                                  textCol,
                                  fontDip,
                                  DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left,
                                  DxuiTextVAlign::CenterOnCapHeight);
            IGNORE_RETURN_VALUE (hr, S_OK);
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
//  DxuiTreeView::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move only updates hover and is reported UNHANDLED, so passing the pointer
//  across the tree does not consume moves other widgets may want. The tree
//  takes no drag, so there is no drag state to route around.
//
//  Only the left button acts; a right-click belongs to the host.
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
