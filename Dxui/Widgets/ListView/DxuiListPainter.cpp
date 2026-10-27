#include "Pch.h"

#include "Widgets/ListView/DxuiListPainter.h"
#include "Theme/DxuiRowLook.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::DxuiListPainter
//
////////////////////////////////////////////////////////////////////////////////

DxuiListPainter::DxuiListPainter (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePalette
//
//  Derives the per-element ARGB colors from the active theme. Called
//  once per paint; assumes the theme has already been null-checked.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::Palette DxuiListPainter::MakePalette() const
{
    DxuiListViewTypes::Palette  pal = {};



    pal.fg       = m_list.m_theme->Foreground();
    pal.fgDim    = (pal.fg & 0x00FFFFFFu) | 0xA0000000u;
    pal.hdrFg    = m_list.m_theme->HeadingForeground();
    pal.bgRow    = m_list.m_theme->ContentBackground();
    pal.bgHover  = m_list.m_theme->ContentHover();
    pal.bgSel    = m_textSelectionColors ? m_list.m_theme->SelectionBackground() : m_list.m_theme->ContentSelection();
    pal.edgeSel  = m_textSelectionColors ? 0u : m_list.m_theme->ContentSelectionEdge();
    pal.bgHeader = (pal.bgRow & 0x00FFFFFFu) | 0xFF000000u;
    pal.border   = m_list.m_theme->ContentHeaderDivider();
    pal.matchBg  = (m_list.m_theme->Accent() & 0x00FFFFFFu) | 0x80000000u;

    return pal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintDataRows
//
//  Paints the visible data rows in [firstRow, lastRow): selection and
//  hover backgrounds followed by each cell's text.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListPainter::PaintDataRows (
    IDxuiPainter           & painter,
    IDxuiTextRenderer    & text,
    const DxuiListViewTypes::Palette          & pal,
    float                    x,
    float                    y,
    float                    layoutW,
    int                      firstRow,
    int                      lastRow,
    const std::vector<int> & colXPx,
    const std::vector<int> & colWPx) const
{
    HRESULT                                   hr        = S_OK;
    float                                     rowH      = (float) m_list.GetRowHeightPx();
    float                                     headerH   = (float) (m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0);
    float                                     hdrGap    = (float) (m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0);
    float                                     cellPadR  = (float) m_list.m_scaler.ToPx (DxuiListView::s_kCellPadRightDip);
    float                                     fontPx    = (float) m_list.m_scaler.ToPxf (m_list.m_fontDip);
    float                                     colOff    = m_list.m_hScrollEnabled ? -(float) m_list.m_leftPx : 0.0f;
    float                                     boxInsetY = m_textSelectionColors ? 0.0f : (float) m_list.m_scaler.ToPx (DxuiListView::s_kRowBoxInsetYDip);
    float                                     boxW      = layoutW;
    float                                     boxL      = (float) m_list.GetDetailsLeftPx();
    float                                     lastRight = 0.0f;
    float                                     clipTop   = 0.0f;
    float                                     clipH     = 0.0f;
    float                                     top       = 0.0f;
    bool                                      clipped   = false;
    std::vector<DxuiListViewTypes::LineSpot>  spots;



    //  A row's highlight ends where its columns do, less Explorer's margin,
    //  rather than running on across the empty width past them.
    for (size_t c = 0; c < m_list.m_columns.size() && c < colXPx.size() && c < colWPx.size(); ++c)
    {
        if (m_list.m_columns[c].visible && colWPx[c] > 0)
        {
            lastRight = (std::max) (lastRight, colOff + (float) (colXPx[c] + colWPx[c]));
        }
    }

    if (!m_textSelectionColors && lastRight > 0.0f)
    {
        boxW = std::clamp (lastRight - (float) m_list.m_scaler.ToPx (s_kRowBoxEndInsetDip), 0.0f, layoutW);
    }

    //  Where firstRow is drawn: the rows lifted at the end of the list or
    //  after a page down, and moved with a sliding page.
    top   = y + headerH + hdrGap - (float) m_list.m_scrollbars.GetRowShiftPx() + (float) m_list.m_items.GetPageSlidePx() - (float) (m_list.m_topRow - firstRow) * rowH;
    spots = m_list.m_grouping.PlaceDataLines (top, firstRow, lastRow, clipTop, clipH);

    for (const DxuiListViewTypes::LineSpot & spot : spots)
    {
        int                        r     = spot.row;
        float                      ry    = spot.y;

        if (spot.slid && !clipped)
        {
            hr = text.PushClipRect (x, clipTop, layoutW, clipH);
            IGNORE_RETURN_VALUE (hr, S_OK);
            painter.PushClipRect (x, clipTop, layoutW, clipH);
            clipped = true;
        }

        if (r < 0)
        {
            m_list.m_grouping.PaintGroupHeader (painter, text, pal, spot.group, x + colOff, x, ry, layoutW);
            continue;
        }

        const std::vector<DxuiListViewTypes::Cell>  & cells = m_list.GetRowCells (r);
        bool                                          isHov = (r == m_hovered);
        bool                       isSel = ((m_list.m_listFocused || m_alwaysShowSelection) &&
                                            (m_list.m_multiSelect ? m_list.IsRowSelected (r) : r == m_list.m_selectedRow));
        DxuiRowLook                look  = DxuiRowLook::Resolve (*m_list.m_theme, isSel, r == m_list.m_selectedRow && m_list.m_focusGroup < 0, isHov, m_list.m_listFocused);

        //  A text-like list (a hex dump) selects as text does: the text
        //  selection color, no outline.
        if (m_textSelectionColors)
        {
            look.fill = isSel ? pal.bgSel : (isHov ? pal.bgHover : 0u);
            look.edge = 0;
        }

        //  Square, as Explorer's are, and like Explorer's a little shorter
        //  than the row and ending short of the last column's edge.
        //  A drop target is drawn as a selected row other than the keyboard's:
        //  its fill, and the accent outline.
        if (r == m_dropRow)
        {
            look.fill = m_list.m_theme->ContentSelectionMulti();
            look.edge = m_list.m_theme->ContentSelectionMultiEdge();
        }

        if (look.fill != 0)
        {
            painter.FillRect (x + boxL, ry + boxInsetY, boxW - boxL, rowH - boxInsetY * 2.0f, look.fill);
        }

        if (look.edge != 0)
        {
            painter.OutlineRect (x + boxL, ry + boxInsetY, boxW - boxL, rowH - boxInsetY * 2.0f, DxuiRowLook::GetOutlinePx (m_list.m_scaler.ToPxf (1.0f)), look.edge);
        }

        for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); ++c)
        {
            uint32_t      argb      = cells[c].dim ? pal.fgDim : (cells[c].argb != 0 ? cells[c].argb : pal.fg);
            float         padL      = (float) m_list.m_columnSizing.GetCellPadLeftPx (c);
            float         iconShift = 0.0f;
            float         alpha     = text.GetGlobalAlpha();
            std::wstring  shown;

            //  Explorer draws the columns after the name a step dimmer than it.
            if (m_list.m_explorerDetails && c > 0 && !cells[c].dim && cells[c].argb == 0)
            {
                argb = (pal.fg & 0x00FFFFFFu) | s_kSecondaryColumnAlpha;
            }

            if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
            {
                continue;
            }

            if (cells[c].icon && !cells[c].icon->bgraPremul.empty())
            {
                float  iconPx = m_list.m_scaler.ToPxf ((float) DxuiListView::s_kCellIconDip);

                if (cells[c].iconGhosted)
                {
                    text.SetGlobalAlpha (alpha * s_kGhostedIconAlpha);
                }

                hr = text.DrawIconBitmap (cells[c].icon->bgraPremul.data(), cells[c].icon->width, cells[c].icon->height,
                                          x + colOff + (float) colXPx[c] + padL,
                                          ry + (rowH - iconPx) * 0.5f,
                                          iconPx, iconPx);
                IGNORE_RETURN_VALUE (hr, S_OK);

                text.SetGlobalAlpha (alpha);

                if (cells[c].iconBroken)
                {
                    PaintBrokenBadge (text, x + colOff + (float) colXPx[c] + padL, ry + (rowH - iconPx) * 0.5f, iconPx, m_list.m_scaler.ToPxf (DxuiListView::s_kBrokenBadgeMinDip));
                }

                iconShift = iconPx + (float) m_list.GetCellIconGapPx();
            }
            else if ((int) c == m_list.m_iconColumn)
            {
                //  Its name still lines up with the others' names.
                iconShift = (float) (m_list.m_scaler.ToPx (DxuiListView::s_kCellIconDip) + m_list.GetCellIconGapPx());
            }

            // Search-match highlight: an accent band behind each matched
            // character range, painted before the text. Left-aligned columns
            // only -- the offset math measures prefixes from the cell's left.
            if (!cells[c].matches.empty() && m_list.m_columns[c].align == DxuiTextHAlign::Left)
            {
                const std::wstring &  cellText  = cells[c].text;
                float                 cellX     = x + colOff + (float) colXPx[c] + padL + iconShift;
                float                 cellMaxW  = (float) colWPx[c] - padL - cellPadR - iconShift;
                float                 bandInset = rowH * 0.14f;

                for (const std::pair<int, int> & mr : cells[c].matches)
                {
                    int      s       = std::clamp (mr.first,  0, (int) cellText.size());
                    int      e       = std::clamp (mr.second, s, (int) cellText.size());
                    float    wS      = 0.0f;
                    float    wE      = 0.0f;
                    float    hIgnore = 0.0f;
                    HRESULT  hrM     = S_OK;
                    float    hx      = 0.0f;
                    float    hw      = 0.0f;

                    if (s > 0)
                    {
                        hrM = text.MeasureString (cellText.substr (0, (size_t) s).c_str(),
                                                  fontPx, m_list.GetBodyFace(), wS, hIgnore);
                        IGNORE_RETURN_VALUE (hrM, S_OK);
                    }

                    hrM = text.MeasureString (cellText.substr (0, (size_t) e).c_str(),
                                              fontPx, m_list.GetBodyFace(), wE, hIgnore);
                    IGNORE_RETURN_VALUE (hrM, S_OK);

                    hx = cellX + wS;
                    hw = wE - wS;

                    if (hx < cellX)                 { hw -= (cellX - hx); hx = cellX; }
                    if (hx + hw > cellX + cellMaxW) { hw  = cellX + cellMaxW - hx;    }

                    if (hw > 0.0f)
                    {
                        painter.FillRect (hx, ry + bandInset, hw, rowH - 2.0f * bandInset, pal.matchBg);
                    }
                }
            }

            // Muted ranges: the text is drawn a run at a time, each run placed
            // by measuring the text before it, the way a match band is placed.
            if (!cells[c].dimRanges.empty() && m_list.m_columns[c].align == DxuiTextHAlign::Left)
            {
                const std::wstring &  cellText = cells[c].text;
                float                 cellX    = x + colOff + (float) colXPx[c] + padL + iconShift;
                float                 cellMaxW = (float) colWPx[c] - padL - cellPadR - iconShift;
                int                   pos      = 0;
                size_t                next     = 0;

                while (pos < (int) cellText.size())
                {
                    bool      inRange = next < cells[c].dimRanges.size() && pos >= cells[c].dimRanges[next].first;
                    int       end     = inRange ? cells[c].dimRanges[next].second
                                                : (next < cells[c].dimRanges.size() ? cells[c].dimRanges[next].first : (int) cellText.size());
                    float     offset  = 0.0f;
                    float     hIgnore = 0.0f;
                    HRESULT   hrM     = S_OK;

                    end = std::clamp (end, pos + 1, (int) cellText.size());

                    if (pos > 0)
                    {
                        hrM = text.MeasureString (cellText.substr (0, (size_t) pos).c_str(), fontPx, m_list.GetBodyFace(), offset, hIgnore);
                        IGNORE_RETURN_VALUE (hrM, S_OK);
                    }

                    if (offset >= cellMaxW)
                    {
                        break;
                    }

                    hr = text.DrawString (cellText.substr ((size_t) pos, (size_t) (end - pos)).c_str(),
                                          cellX + offset,
                                          ry,
                                          cellMaxW - offset,
                                          rowH,
                                          inRange ? pal.fgDim : argb,
                                          fontPx,
                                          m_list.GetBodyFace(),
                                          DxuiTextHAlign::Left,
                                          DxuiTextVAlign::CenterOnCapHeight,
                                          DxuiFontWeight::Normal,
                                          false);
                    IGNORE_RETURN_VALUE (hr, S_OK);

                    next += inRange ? 1 : 0;
                    pos   = end;
                }

                continue;
            }

            //  Text wider than its column ends in an ellipsis, as Explorer's does,
            //  rather than being cut through a letter.
            shown = DxuiTextElide::ToWidth (text, cells[c].text, fontPx, m_list.GetBodyFace(),
                                            (float) colWPx[c] - padL - cellPadR - iconShift, DxuiElide::Tail);

            hr = text.DrawString (shown.c_str(),
                                  x + colOff + (float) colXPx[c] + padL + iconShift,
                                  ry,
                                  (float) colWPx[c] - padL - cellPadR - iconShift,
                                  rowH,
                                  argb,
                                  fontPx,
                                  m_list.GetBodyFace(),
                                  m_list.m_columns[c].align,
                                  DxuiTextVAlign::CenterOnCapHeight,
                                  DxuiFontWeight::Normal,
                                  false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (clipped)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PopClipRect();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::PaintSmallArtFrame
//
//  Explorer's frame around a big icon box whose art is smaller than it,
//  measured at 150% in the dark theme: two pixels a shade darker than the
//  background at the box's edge, then three pixels inside them that fade from
//  well lighter than the background back to it. The light theme takes the
//  same steps the other way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListPainter::PaintSmallArtFrame (IDxuiPainter & painter, float x, float y, float sizePx, uint32_t bg)
{
    static constexpr int  kRing[] = { 69, 46, 23 };
    int                   r       = (int) ((bg >> 16) & 0xFF);
    int                   g       = (int) ((bg >>  8) & 0xFF);
    int                   b       = (int) ( bg        & 0xFF);
    bool                  dark    = (r * 299 + g * 587 + b * 114) / 1000 < 128;
    int                   sign    = dark ? 1 : -1;
    int                   inset   = 2;



    painter.OutlineRect (x, y, sizePx, sizePx, 2.0f, ShiftGray (bg, -4 * sign));

    for (int step : kRing)
    {
        painter.OutlineRect (x + (float) inset, y + (float) inset, sizePx - (float) (2 * inset), sizePx - (float) (2 * inset), 1.0f, ShiftGray (bg, step * sign));
        inset++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::ShiftGray
//
//  An opaque color this much lighter (or, negative, darker) in each channel.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiListPainter::ShiftGray (uint32_t argb, int delta)
{
    int  r = std::clamp ((int) ((argb >> 16) & 0xFF) + delta, 0, 255);
    int  g = std::clamp ((int) ((argb >>  8) & 0xFF) + delta, 0, 255);
    int  b = std::clamp ((int) ( argb        & 0xFF) + delta, 0, 255);



    return 0xFF000000u | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::PaintItems
//
//  Each item in sight: the selection or hover card, the icon at the view's
//  size, the name, and for Tiles and Content the other columns in the muted
//  color on the lines below.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListPainter::PaintItems (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, float x, float y) const
{
    HRESULT                         hr      = S_OK;
    DxuiListViewTypes::ItemMetrics  metrics = DxuiListItemLayout::GetItemMetrics (m_list.m_view);
    DxuiListViewTypes::ItemGrid     grid    = m_list.m_items.GetItemGrid();
    int                             rows    = m_list.GetRowCount();
    int                             first   = 0;
    int                             last    = rows;
    int                             pad     = m_list.m_scaler.ToPx (DxuiListView::s_kItemPadDip);
    int                             iconPx  = DxuiListItemLayout::GetItemIconPx (m_list.m_view, m_list.m_scaler.GetDpi());
    int                             lineH   = m_list.m_scaler.ToPx (metrics.lineDip);
    float                           fontPx  = m_list.m_scaler.ToPxf (m_list.m_fontDip);
    bool                            listGrp = m_list.m_flow.HasListGroups();
    bool                            itemGrp = !listGrp && m_list.m_items.UsesItemLayout();
    //  Built once here: building either per item made a frame cost the square
    //  of the item count.
    const DxuiListViewTypes::ListLayout & listLayout = listGrp ? m_list.m_flow.GetListLayout() : DxuiListView::s_kNoListLayout;
    //  A group sliding open or shut paints from a layout that holds it open.
    DxuiListViewTypes::ItemSlide  slide   = itemGrp ? m_list.m_items.PlaceItemSlide() : DxuiListViewTypes::ItemSlide {};
    int                           pageY   = 0;
    bool                          clipped = false;
    //  The items on screen and their cells, found before any row's cells are
    //  asked for: a host that builds rows on demand builds only these.
    std::vector<std::pair<int, RECT>>  onScreen;



    m_nameCut.clear();

    //  The captions' line pitch, which the layout's row heights are built from;
    //  a change moves every row, so the layout is laid out again.
    {
        float  linePx   = 0.0f;
        int    previous = m_list.m_captionLinePx;

        hr = text.GetLineHeightGdi (fontPx, DxuiTheme::kBodyFace, linePx);
        m_list.m_captionLinePx = (SUCCEEDED (hr) && linePx > 0.0f) ? (int) std::lround (linePx) : 0;

        if (m_list.m_captionLinePx != previous)
        {
            m_list.m_itemTextVersion++;
        }
    }

    //  Measured before anything is drawn: a name that wraps past two lines
    //  makes its row taller and moves everything under it, headers included.
    if (itemGrp)
    {
        m_list.m_items.CollectItemsToMeasure (onScreen);
        m_list.m_items.MeasureItemTextLines  (text, onScreen);

        //  A page sliding into view draws everything this far from where it
        //  rests, found once the names in sight are measured, since a row they
        //  make taller moves where the view rests.
        pageY = m_list.m_items.GetPageSlidePx();
    }

    onScreen.clear();

    if (listGrp)
    {
        const DxuiListViewTypes::ListLayout & layout = listLayout;

        for (const DxuiListViewTypes::ListBlock & block : layout.blocks)
        {
            float  left = x + (float) (block.left - m_list.m_leftPx);

            if (left + (float) block.width > x && left < x + (float) (m_list.m_boundsDip.right - m_list.m_boundsDip.left))
            {
                m_list.m_grouping.PaintGroupHeader (painter, text, pal, block.group, left, left, y, (float) block.width);
            }
        }

        first = 0;
        last  = rows;
    }
    else if (metrics.columns)
    {
        int  firstColumn = (std::max) (0, m_list.m_flow.FindListColumn (m_list.m_leftPx));
        int  lastColumn  = m_list.m_flow.FindListColumn (m_list.m_leftPx + (m_list.m_boundsDip.right - m_list.m_boundsDip.left));

        first = firstColumn * grid.perLine;
        last  = (lastColumn < 0) ? rows : (std::min) (rows, (lastColumn + 1) * grid.perLine);
    }
    else if (itemGrp)
    {
        const DxuiListViewTypes::ItemLayout  & layout = slide.active ? slide.layout : m_list.m_items.GetItemLayout();
        int                                    topPx  = m_list.m_items.GetItemTopPx (m_list.m_items.GetItemLayout());
        float                                  viewW  = (float) m_list.m_items.ComputeItemScrollLayout().viewportW;

        for (size_t at = 0; at < layout.lines.size(); at++)
        {
            const DxuiListViewTypes::ItemLine  & line  = layout.lines[at];
            int                                  lineY = line.top - topPx + pageY - ((slide.active && (int) at > slide.header) ? slide.lift : 0);

            if (lineY + line.height <= 0 || lineY >= m_list.m_boundsDip.bottom - m_list.m_boundsDip.top)
            {
                continue;
            }

            if (line.group >= 0)
            {
                m_list.m_grouping.PaintGroupHeader (painter, text, pal, line.group, x, x, y + (float) lineY, viewW);
            }

            //  Only the items of lines in sight are placed: placing every item
            //  made each frame cost the whole folder.
            m_list.m_items.CollectLineItemsInSight (layout, line, slide, pageY, onScreen);
        }

        first = 0;
        last  = 0;
    }
    else
    {
        first = m_list.m_topRow;
        last  = (std::min) (rows, first + (grid.visible + 1) * grid.perLine);
    }

    for (int item = first; item < last; item++)
    {
        RECT  cell   = {};
        bool  placed = listGrp ? m_list.m_flow.GetListItemRectPx (listLayout, item, cell)
                               : m_list.m_items.GetItemRectPx     (item, cell);

        if (placed && cell.bottom > 0 && cell.top < m_list.m_boundsDip.bottom - m_list.m_boundsDip.top &&
            cell.right > 0 && cell.left < m_list.m_boundsDip.right - m_list.m_boundsDip.left)
        {
            onScreen.emplace_back (item, cell);
        }
    }

    //  The sliding group's items last, so its clip is pushed once.
    if (slide.active)
    {
        std::stable_partition (onScreen.begin(), onScreen.end(), [&slide] (const std::pair<int, RECT> & place)
        {
            return place.first < slide.start || place.first >= slide.end;
        });
    }

    for (const std::pair<int, RECT> & place : onScreen)
    {
        int                                           item  = place.first;
        RECT                                          cell  = place.second;
        RECT                                          label = {};
        const std::vector<DxuiListViewTypes::Cell>  & cells = m_list.GetRowCells (item);
        bool                        isSel                    = (m_list.m_listFocused || m_alwaysShowSelection) &&
                                             (m_list.m_multiSelect ? m_list.IsRowSelected (item) : item == m_list.m_selectedRow);
        float                       iconX  = 0.0f;
        float                       iconY  = 0.0f;

        if (slide.active && !clipped && item >= slide.start && item < slide.end)
        {
            hr = text.PushClipRect (x, y + (float) slide.clipTop, (float) (m_list.m_boundsDip.right - m_list.m_boundsDip.left), (float) slide.clipH);
            IGNORE_RETURN_VALUE (hr, S_OK);
            painter.PushClipRect (x, y + (float) slide.clipTop, (float) (m_list.m_boundsDip.right - m_list.m_boundsDip.left), (float) slide.clipH);
            clipped = true;
        }

        if (cells.empty())
        {
            continue;
        }

        OffsetRect (&cell, (int) x, (int) y);

        //  A tile's box, and a big icon's, stops short of its row.
        if (m_list.m_view == DxuiListViewTypes::View::Tiles)
        {
            cell.bottom -= (int) std::lround (m_list.m_scaler.ToPxf (s_kTileBoxShortDip));
        }
        else if (metrics.labelBelow)
        {
            cell.bottom -= (int) std::lround (m_list.m_scaler.ToPxf (DxuiListView::s_kItemRowGapDip));
        }

        //  Square, and drawn as Details draws a row: the fill, and the outline
        //  a focused or multiple selection takes.
        if (m_list.m_theme != nullptr)
        {
            DxuiRowLook  look = DxuiRowLook::Resolve (*m_list.m_theme, isSel, item == m_list.m_selectedRow && m_list.m_focusGroup < 0, item == m_hovered, m_list.m_listFocused);

            float  fillW = (float) (cell.right - cell.left);

            if (look.fill != 0)
            {
                painter.FillRect ((float) cell.left, (float) cell.top, fillW, (float) (cell.bottom - cell.top), look.fill);
            }

            if (look.edge != 0)
            {
                painter.OutlineRect ((float) cell.left, (float) cell.top, fillW, (float) (cell.bottom - cell.top),
                                     DxuiRowLook::GetOutlinePx (m_list.m_scaler.ToPxf (1.0f)), look.edge);
            }
        }

        //  Content's rule between items, though not under a group's header.
        if (m_list.m_view == DxuiListViewTypes::View::Content && m_list.m_theme != nullptr && item > 0 && (!m_list.m_items.HasItemGroups() || !m_list.m_items.IsFirstOfGroup (item)))
        {
            float  ruleLeft  = (float) cell.left  + m_list.m_scaler.ToPxf (s_kContentRuleInsetDip);
            float  ruleRight = (float) cell.right - m_list.m_scaler.ToPxf (s_kContentRuleEndDip);

            painter.FillRect (std::round (ruleLeft), (float) cell.top, std::round (ruleRight) - std::round (ruleLeft), 1.0f, m_list.m_theme->Divider());
        }

        if (item == m_dropRow && m_list.m_theme != nullptr)
        {
            painter.OutlineRect ((float) cell.left, (float) cell.top, (float) (cell.right - cell.left), (float) (cell.bottom - cell.top),
                                 DxuiRowLook::GetOutlinePx (m_list.m_scaler.ToPxf (1.0f)), m_list.m_theme->ContentSelectionMultiEdge());
        }

        if (metrics.labelBelow)
        {
            iconX = (float) (cell.left + ((cell.right - cell.left) - iconPx) / 2);
            iconY = (float) (cell.top + m_list.m_scaler.ToPx (DxuiListView::GetIconTopDip (m_list.m_view)));
        }
        else
        {
            iconX = (float) (cell.left + m_list.m_items.GetItemIconLeftPx());
            iconY = (float) cell.top + ((float) (cell.bottom - cell.top) - (float) iconPx) * 0.5f;
        }

        //  Explorer frames a big icon whose art is smaller than its box.
        if (DxuiListItemLayout::GetItemMetrics (m_list.m_view).labelBelow && cells[0].icon && !cells[0].icon->bgraPremul.empty() && cells[0].icon->width < iconPx)
        {
            PaintSmallArtFrame (painter, iconX, iconY, (float) iconPx, pal.bgRow);
        }

        if (cells[0].icon && !cells[0].icon->bgraPremul.empty())
        {
            float  alpha = text.GetGlobalAlpha();

            if (cells[0].iconGhosted)
            {
                text.SetGlobalAlpha (alpha * s_kGhostedIconAlpha);
            }

            //  An image smaller than the icon box, a type with no art at this
            //  size, is drawn at its own size in the middle, as Explorer draws it.
            float  drawPx = (float) (std::min) (iconPx, (std::max) (cells[0].icon->width, 1));
            float  inset  = ((float) iconPx - drawPx) * 0.5f;

            hr = text.DrawIconBitmap (cells[0].icon->bgraPremul.data(), cells[0].icon->width, cells[0].icon->height,
                                      iconX + inset, iconY + inset, drawPx, drawPx);
            IGNORE_RETURN_VALUE (hr, S_OK);

            text.SetGlobalAlpha (alpha);
        }

        if (cells[0].iconBroken)
        {
            PaintBrokenBadge (text, iconX, iconY, (float) iconPx, m_list.m_scaler.ToPxf (DxuiListView::s_kBrokenBadgeMinDip));
        }

        label = m_list.m_items.GetItemLabelRectPx (cell);

        //  Explorer's Content: the name with a line under it, then a middle
        //  column and a right column of two lines each. A middle or right line
        //  is drawn only where it fits, as Explorer drops one rather than cut
        //  it short, and the middle column only where it starts short of the
        //  right one.
        if (m_list.m_view == DxuiListViewTypes::View::Content)
        {
            float         left   = (float) (cell.left - m_list.GetItemsLeftPx());
            float         edge   = (float) cell.right + m_list.m_scaler.ToPxf (s_kContentEdgeDip) - left;
            float         rightX = left + std::round ((std::min) (m_list.m_scaler.ToPxf (s_kContentRightMaxDip),
                                                                   (std::max) (edge * s_kContentRightShare + m_list.m_scaler.ToPxf (s_kContentRightNudgeDip),
                                                                               edge - m_list.m_scaler.ToPxf (s_kContentRightRoomDip))));
            float         midX    = (float) label.left + std::round (m_list.m_scaler.ToPxf (s_kContentMiddleDip));
            float         top     = (float) cell.top + ((float) (cell.bottom - cell.top) - (float) (2 * lineH)) * 0.5f;
            bool          middle  = false;
            float         leftEnd = 0.0f;
            std::wstring  name;

            for (const DxuiListViewTypes::Cell & line : cells[0].contentMiddle)
            {
                middle = middle || (midX < rightX && !line.text.empty() && MeasureContentLine (text, line, fontPx) <= rightX - midX);
            }

            //  The name and the line under it end where the next column drawn
            //  starts.
            leftEnd = middle ? midX : rightX;
            name    = DxuiTextElide::ToWidth (text, cells[0].text, fontPx * s_kContentNameScale, DxuiTheme::kBodyFace, leftEnd - (float) label.left,
                                              DxuiElide::Tail, 0, true);

            //  The name in Explorer's larger face, 11 points to the body's 9,
            //  sitting on the same baseline as it would in the body face.
            hr = text.DrawString (name.c_str(), (float) label.left, top - fontPx * (s_kContentNameScale - 1.0f), leftEnd - (float) label.left,
                                  (float) lineH * s_kContentNameScale,
                                  cells[0].dim ? pal.fgDim : (cells[0].argb != 0 ? cells[0].argb : pal.fg), fontPx * s_kContentNameScale, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);


            for (size_t at = 0; at < cells[0].contentLeft.size(); at++)
            {
                float  lineX = (float) label.left;

                //  A result's folder leads the first line, cut short so the
                //  line after it still fits with a gap before the column beside it.
                if (at == 0 && !cells[0].contentPath.empty())
                {
                    float         restW = 0.0f;
                    float         restH = 0.0f;
                    float         pathW = 0.0f;
                    std::wstring  path;

                    restW = MeasureContentLine (text, cells[0].contentLeft[0], fontPx);

                    path = DxuiTextElide::ToWidth (text, cells[0].contentPath, fontPx, DxuiTheme::kBodyFace,
                                                   (std::max) (0.0f, leftEnd - lineX - restW - 2.0f * m_list.m_scaler.ToPxf (s_kContentPathGapDip)), DxuiElide::Tail, 0, true);

                    hr = text.MeasureString (path.c_str(), fontPx, DxuiTheme::kBodyFace, pathW, restH);
                    IGNORE_RETURN_VALUE (hr, S_OK);

                    hr = text.DrawString (path.c_str(), lineX, top + (float) lineH, leftEnd - lineX, (float) lineH,
                                          pal.fg, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                    IGNORE_RETURN_VALUE (hr, S_OK);

                    lineX += pathW + m_list.m_scaler.ToPxf (s_kContentPathGapDip);
                }

                PaintContentLine (text, pal, cells[0].contentLeft[at], lineX, top + (float) ((at + 1) * lineH), leftEnd - lineX, (float) lineH, fontPx);
            }


            for (size_t at = 0; middle && at < cells[0].contentMiddle.size() && at < 2; at++)
            {
                if (MeasureContentLine (text, cells[0].contentMiddle[at], fontPx) <= rightX - midX)
                {
                    PaintContentLine (text, pal, cells[0].contentMiddle[at], midX, top + (float) (at * lineH), rightX - midX, (float) lineH, fontPx);
                }
            }

            for (size_t at = 0; at < cells[0].contentRight.size() && at < 2; at++)
            {
                if (MeasureContentLine (text, cells[0].contentRight[at], fontPx) <= (float) cell.right - rightX)
                {
                    PaintContentLine (text, pal, cells[0].contentRight[at], rightX, top + (float) (at * lineH), (float) cell.right - rightX, (float) lineH, fontPx);
                }
            }

            continue;
        }

        //  Explorer's tiles: the name on up to two lines, the second ending in
        //  an ellipsis when it runs on, then the row's own lines (type and
        //  size) while three lines last, the whole block centered in the box.
        if (m_list.m_view == DxuiListViewTypes::View::Tiles)
        {
            std::vector<std::wstring>  name     = DxuiTextElide::WrapToLines (text, cells[0].text, fontPx, DxuiTheme::kBodyFace,
                                                                               (float) (label.right - label.left), 2, true, true);
            std::vector<const DxuiListViewTypes::Cell *>  details;
            float                                         top      = 0.0f;
            int                                           lineStep = m_list.m_items.GetTileLinePx();

            for (size_t at = 0; !cells[0].tileNameOnly && at < cells[0].tileLines.size() && name.size() + details.size() < (size_t) DxuiListView::s_kTileMaxLines; at++)
            {
                details.push_back (&cells[0].tileLines[at]);
            }

            //  The block is centered as at least two lines, as a name alone is.
            top = (float) cell.top + (float) ((cell.bottom - cell.top) - (std::max) (2, (int) (name.size() + details.size())) * lineStep) * 0.5f;

            hr = text.PushClipRect ((float) label.left, (float) cell.top, (float) (label.right - label.left), (float) (cell.bottom - cell.top));
            IGNORE_RETURN_VALUE (hr, S_OK);

            for (const std::wstring & line : name)
            {
                hr = text.DrawString (line.c_str(), (float) label.left, top, (float) (cell.right - label.left), (float) lineH,
                                      cells[0].dim ? pal.fgDim : (cells[0].argb != 0 ? cells[0].argb : pal.fg), fontPx, DxuiTheme::kBodyFace,
                                      DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
                top += (float) lineStep;
            }

            for (const DxuiListViewTypes::Cell * detail : details)
            {
                if (detail->meter >= 0.0f)
                {
                    PaintMeter (painter, (float) label.left, top, (float) (label.right - label.left), (float) lineH, detail->meter);
                }
                else
                {
                    hr = text.DrawString (detail->text.c_str(), (float) label.left, top, (float) (cell.right - label.left), (float) lineH,
                                          pal.fgDim, fontPx, DxuiTheme::kBodyFace,
                                          DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                    IGNORE_RETURN_VALUE (hr, S_OK);
                }

                top += (float) lineStep;
            }

            hr = text.PopClipRect();
            IGNORE_RETURN_VALUE (hr, S_OK);
            continue;
        }

        //  Explorer's captions: under a big icon, wrapped to two lines, or one
        //  for Extra large; beside a small one, one line. Either way a name
        //  that runs on ends in an ellipsis inside its cell.
        {
            int                        lines = metrics.labelBelow ? DxuiListView::s_kItemNameMaxLines : 1;
            std::vector<std::wstring>  shown = DxuiTextElide::WrapToLines (text, cells[0].text, fontPx, DxuiTheme::kBodyFace,
                                                                            (float) (label.right - label.left), lines, true, metrics.labelBelow);

            if (!shown.empty() && shown.back().ends_with (L"\x2026") && !cells[0].text.ends_with (L"\x2026"))
            {
                m_nameCut.push_back (item);
            }

            for (size_t at = 0; at < shown.size(); at++)
            {
                hr = text.DrawString (shown[at].c_str(), (float) label.left, (float) label.top + (float) (at * m_list.m_items.GetCaptionLinePx()),
                                      (float) (label.right - label.left), (float) lineH,
                                      cells[0].dim ? pal.fgDim : (cells[0].argb != 0 ? cells[0].argb : pal.fg), fontPx, DxuiTheme::kBodyFace,
                                      metrics.labelBelow ? DxuiTextHAlign::Center : DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                                      DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }

        //  Tiles and Content show the other columns underneath, one a line,
        //  or the row's own tile lines when it has them.
        for (int line = 1; !metrics.labelBelow && !(m_list.m_view == DxuiListViewTypes::View::Tiles && cells[0].tileNameOnly) && line < metrics.textLines; line++)
        {
            bool                                          own    = !cells[0].tileLines.empty();
            const std::vector<DxuiListViewTypes::Cell>  & source = own ? cells[0].tileLines : cells;
            size_t                                        index  = own ? (size_t) (line - 1) : (size_t) line;
            float                                         top    = (float) (label.top + line * lineH);

            if (index >= source.size())
            {
                break;
            }

            if (source[index].meter >= 0.0f)
            {
                PaintMeter (painter, (float) label.left, top, (float) (label.right - label.left), (float) lineH, source[index].meter);
                continue;
            }

            hr = text.DrawString (source[index].text.c_str(), (float) label.left, top,
                                  (float) (label.right - label.left), (float) lineH,
                                  pal.fgDim, fontPx, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (clipped)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PopClipRect();
    }

    PaintSelectionBand (painter, pal, x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::PaintSelectionBand
//
//  The rubber band, over everything it selects.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListPainter::PaintSelectionBand (IDxuiPainter & painter, const DxuiListViewTypes::Palette & pal, float x, float y) const
{
    RECT      band      = m_list.m_items.GetSelectionBandPx();
    COLORREF  highlight = GetSysColor (COLOR_HIGHLIGHT);
    uint32_t  rgb       = ((uint32_t) GetRValue (highlight) << 16) | ((uint32_t) GetGValue (highlight) << 8) | GetBValue (highlight);



    UNREFERENCED_PARAMETER (pal);

    if (!m_list.m_bandActive)
    {
        return;
    }

    OffsetRect (&band, (int) x, (int) y);

    //  Explorer's band is the system highlight, #0078D7 by default: a solid
    //  edge over a fill of about a third, measured.
    painter.FillRect    ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                         s_kBandFillAlpha | rgb);
    painter.OutlineRect ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top),
                         1.0f, 0xFF000000u | rgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::PaintBrokenBadge
//
//  A red disc with a white cross over the icon's lower-left corner, about
//  half the icon across and never under the size given, so it reads on a
//  16 dip icon.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListPainter::PaintBrokenBadge (IDxuiTextRenderer & text, float iconX, float iconY, float iconPx, float minPx)
{
    HRESULT  hr    = S_OK;
    float    d     = (std::max) (minPx, iconPx * 0.45f);
    float    cx    = iconX + d * 0.5f;
    float    cy    = iconY + iconPx - d * 0.5f;
    float    arm   = d * 0.2f;
    float    thick = (std::max) (1.0f, d * 0.12f);



    hr = text.FillEllipse (cx, cy, d * 0.5f, d * 0.5f, s_kBrokenBadgeArgb);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx - arm, cy - arm, cx + arm, cy + arm, thick, 0xFFFFFFFF);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawLine (cx - arm, cy + arm, cx + arm, cy - arm, thick, 0xFFFFFFFF);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::MeasureContentLine
//
//  A Content line's width: its label and a space, then its text.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiListPainter::MeasureContentLine (IDxuiTextRenderer & text, const DxuiListViewTypes::Cell & line, float fontPx)
{
    std::wstring  whole  = line.label.empty() ? line.text : line.label + L" " + line.text;
    float         width  = 0.0f;
    float         height = 0.0f;
    HRESULT       hr     = text.MeasureString (whole.c_str(), fontPx, DxuiTheme::kBodyFace, width, height);



    IGNORE_RETURN_VALUE (hr, S_OK);

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::PaintContentLine
//
//  The label muted and the value after it in the text color, as Explorer
//  draws "Size: 3 bytes"; a line with no label is muted throughout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListPainter::PaintContentLine (IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, const DxuiListViewTypes::Cell & line, float x, float y, float w, float lineH, float fontPx)
{
    std::wstring  prefix = line.label.empty() ? std::wstring() : line.label + L" ";
    float         labelW = 0.0f;
    float         probeW = 0.0f;
    float         height = 0.0f;
    HRESULT       hr     = S_OK;



    //  The space's width counts: measured with a letter after it, then less
    //  that letter, since a measure can omit trailing white space.
    if (!prefix.empty())
    {
        hr = text.MeasureString ((prefix + L"x").c_str(), fontPx, DxuiTheme::kBodyFace, labelW, height);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.MeasureString (L"x", fontPx, DxuiTheme::kBodyFace, probeW, height);
        IGNORE_RETURN_VALUE (hr, S_OK);

        labelW -= probeW;

        hr = text.DrawString (prefix.c_str(), x, y, w, lineH, pal.fgDim, fontPx, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.DrawString (line.text.c_str(), x + labelW, y, (std::max) (0.0f, w - labelW), lineH, prefix.empty() ? pal.fgDim : pal.fg, fontPx,
                          DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter::PaintMeter
//
//  Explorer's usage bar: an edged track, filled from the left to the
//  fraction, centered in its line and no wider than the line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListPainter::PaintMeter (IDxuiPainter & painter, float x, float y, float w, float lineH, float fraction) const
{
    float     edge   = (std::max) (1.0f, m_list.m_scaler.ToPxf (1.0f));
    float     width  = (std::min) (w, m_list.m_scaler.ToPxf ((float) s_kMeterWidthDip));
    float     height = m_list.m_scaler.ToPxf ((float) s_kMeterHeightDip);
    float     top    = y + (lineH - height) * 0.5f;
    float     fill   = std::clamp (fraction, 0.0f, 1.0f);
    uint32_t  argb   = (fill >= s_kMeterFullAt) ? s_kMeterFullArgb : s_kMeterFillArgb;



    if (width <= edge * 2.0f)
    {
        return;
    }

    painter.FillRect (x, top, width, height, s_kMeterEdgeArgb);
    painter.FillRect (x + edge, top + edge, width - edge * 2.0f, height - edge * 2.0f, s_kMeterTrackArgb);
    painter.FillRect (x + edge, top + edge, (width - edge * 2.0f) * fill, height - edge * 2.0f, argb);
}
