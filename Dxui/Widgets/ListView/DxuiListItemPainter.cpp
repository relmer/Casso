#include "Pch.h"
#include "Theme/DxuiColor.h"

#include "Widgets/ListView/DxuiListItemPainter.h"
#include "Widgets/ListView/DxuiListIcons.h"
#include "Theme/DxuiRowLook.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemPainter::DxuiListItemPainter
//
////////////////////////////////////////////////////////////////////////////////

DxuiListItemPainter::DxuiListItemPainter (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemPainter::PaintItems
//
//  Each item in sight: the selection or hover card, the icon at the view's
//  size, the name, and for Tiles and Content the other columns in the muted
//  color on the lines below.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemPainter::PaintItems (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, float x, float y) const
{
    HRESULT                         hr      = S_OK;
    DxuiListViewTypes::ItemMetrics  metrics = DxuiListItemLayout::GetItemMetrics (m_list.m_view);
    DxuiListViewTypes::ItemGrid     grid    = m_list.m_items.GetItemGrid();
    int                             rows    = m_list.GetRowCount();
    int                             first   = 0;
    int                             last    = rows;
    int                             pad     = m_list.m_scaler.ToPx (DxuiListView::s_kItemPadDip);
    int                             iconPx  = m_list.m_items.GetViewIconPx();
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
    m_homeCut.clear();

    //  The captions' line pitch, which the layout's row heights are built from;
    //  a change moves every row, so the layout is laid out again.
    {
        float  linePx   = 0.0f;
        int    previous = m_list.m_items.m_captionLinePx;

        hr = text.GetLineHeightGdi (fontPx, DxuiTheme::kBodyFace, linePx);
        m_list.m_items.m_captionLinePx = (SUCCEEDED (hr) && linePx > 0.0f) ? (int) std::lround (linePx) : 0;

        if (m_list.m_items.m_captionLinePx != previous)
        {
            m_list.m_items.m_itemTextVersion++;
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
        bool                        isSel                    = (m_list.m_listFocused || m_list.m_alwaysShowSelection) &&
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
            DxuiRowLook  look = DxuiRowLook::Resolve (*m_list.m_theme, isSel, item == m_list.m_selectedRow && m_list.m_focusGroup < 0, item == m_list.m_hovered, m_list.m_listFocused);

            float  fillW = (float) (cell.right - cell.left);

            //  Home's items are rounded.
            if (m_list.m_view == DxuiListViewTypes::View::Home)
            {
                float  radius = m_list.m_scaler.ToPxf (4.0f);

                //  A hovered row's box is short of its pitch, in a neutral gray.
                float     inset = m_list.m_items.IsHomeTile (item) ? 0.0f : m_list.m_scaler.ToPxf (2.5f);
                uint32_t  fill  = (item == m_list.m_hovered && !isSel) ? DxuiColor::Mix (m_list.m_theme->ContentBackground(), m_list.m_theme->Foreground(), 0.07f) : isSel ? DxuiColor::Mix (m_list.m_theme->ContentBackground(), m_list.m_theme->Foreground(), 0.03f) : look.fill;

                if (fill != 0)
                {
                    painter.FillRoundedRect ((float) cell.left, (float) cell.top + inset, fillW, (float) (cell.bottom - cell.top) - 2.0f * inset, radius, fill);
                }

                if (isSel)
                {
                    painter.OutlineRoundedRect ((float) cell.left, (float) cell.top, fillW, (float) (cell.bottom - cell.top), radius, (std::round) (m_list.m_scaler.ToPxf (1.34f)), m_list.m_theme->Accent());
                }

                //  The button under the pointer has a box.
                if (item == m_list.m_hovered && m_list.m_homeHoverAction >= 0)
                {
                    RECT  button = {};
                    bool  found  = m_list.m_items.GetHomeActionRect (cell, m_list.m_homeHoverAction, button);

                    if (found)
                    {
                        painter.FillRoundedRect ((float) button.left, (float) button.top, (float) (button.right - button.left), (float) (button.bottom - button.top), radius, DxuiColor::Mix (m_list.m_theme->ContentBackground(), m_list.m_theme->Foreground(), 0.14f));
                    }
                }

                PaintHomeItem (text, pal, item, cell, cells);
                continue;
            }

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

        if (item == m_list.m_dropRow && m_list.m_theme != nullptr)
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
            DxuiListIcons::PaintSmallArtFrame (painter, iconX, iconY, (float) iconPx, pal.bgRow);
        }

        if (cells[0].icon && !cells[0].icon->bgraPremul.empty())
        {
            float  alpha = text.GetGlobalAlpha();

            if (cells[0].iconGhosted)
            {
                text.SetGlobalAlpha (alpha * DxuiListIcons::s_kGhostedIconAlpha);
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
            DxuiListIcons::PaintBrokenBadge (text, iconX, iconY, (float) iconPx, m_list.m_scaler.ToPxf (DxuiListView::s_kBrokenBadgeMinDip));
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

    m_list.m_band.Paint (painter, pal, x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemPainter::PaintHomeItem
//
//  A tile has its icon, then its name, location and pin. A row has a cloud
//  when it is in the cloud, its icon, its name over its location, then its
//  columns.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemPainter::PaintHomeItem (IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, int item, const RECT & cell, const std::vector<DxuiListViewTypes::Cell> & cells) const
{
    static constexpr const wchar_t *  s_kGlyphFace  = L"Segoe Fluent Icons";
    static constexpr const wchar_t *  s_kPinGlyph   = L"\xE840";
    static constexpr const wchar_t *  s_kPinFill    = L"\xE842";
    static constexpr const wchar_t *  s_kCloudGlyph = L"\xE753";
    static constexpr uint32_t         s_kCloudArgb  = 0xFF0F6CBD;
    static constexpr uint32_t         s_kPinArgb    = 0xFF8C8C8C;
    static constexpr uint32_t         s_kPathArgb   = 0xFFC8C8C8;



    bool          tile    = m_list.m_items.IsHomeTile (item);
    float         fontPx  = m_list.m_scaler.ToPxf (m_list.m_fontDip) * 1.02f;
    float         iconPx  = m_list.m_scaler.ToPxf (tile ? 48.0f : 32.0f);
    float         iconX   = (float) cell.left + m_list.m_scaler.ToPxf (tile ? 75.0f : 74.0f);
    float         iconY   = (float) cell.top + ((float) (cell.bottom - cell.top) - iconPx) * 0.5f;
    float         textX   = (float) cell.left + m_list.m_scaler.ToPxf (tile ? 136.0f : 120.0f);
    float         lineH   = m_list.m_scaler.ToPxf (16.0f);
    int           lines   = 1 + (cells[0].tileLines.empty() ? 0 : 1) + ((tile && cells[0].homePinned) ? 1 : 0);
    float         top     = (float) cell.top + ((float) (cell.bottom - cell.top) - (float) lines * lineH) * 0.5f;
    float         width   = (float) (cell.right - cell.left);
    float         nameEnd = (float) cell.right - (tile ? m_list.m_scaler.ToPxf (14.0f) : 0.0f);
    HRESULT       hr      = S_OK;
    std::wstring  shown;



    if (cells[0].icon && !cells[0].icon->bgraPremul.empty())
    {
        hr = text.DrawIconBitmap (cells[0].icon->bgraPremul.data(), cells[0].icon->width, cells[0].icon->height, iconX, iconY, iconPx, iconPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (!tile && cells[0].homeCloud)
    {
        hr = text.DrawString (s_kCloudGlyph, (float) cell.left + m_list.m_scaler.ToPxf (46.0f), (float) cell.top, m_list.m_scaler.ToPxf (24.0f), (float) (cell.bottom - cell.top),
                              s_kCloudArgb, fontPx * 1.4f, s_kGlyphFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  A cloud item is a shortcut to its file, with the shell's arrow.
        if (cells[0].homeBadge && !cells[0].homeBadge->bgraPremul.empty())
        {
            hr = text.DrawIconBitmap (cells[0].homeBadge->bgraPremul.data(), cells[0].homeBadge->width, cells[0].homeBadge->height, iconX, iconY, iconPx, iconPx);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    for (size_t at = 1; !tile && at < cells.size() && at - 1 < m_list.m_homeHeader.columnAt.size(); at++)
    {
        float  left = (float) cell.left + m_list.m_items.GetHomeColumnPx ((int) at - 1, width);
        float  end  = (float) cell.left + m_list.m_items.GetHomeColumnPx ((int) at, width);

        nameEnd = (at == 1) ? left - m_list.m_scaler.ToPxf (30.0f) : nameEnd;

        shown = DxuiTextElide::ToWidth (text, cells[at].text, fontPx, DxuiTheme::kBodyFace, end - left - fontPx, DxuiElide::Tail, 0, true);

        hr = text.DrawString (shown.c_str(), left, (float) cell.top, end - left, (float) (cell.bottom - cell.top), pal.fg, fontPx, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  The hovered row's buttons, which its text ends short of.
    if (!tile && item == m_list.m_hovered)
    {
        static constexpr const wchar_t *  s_kActionGlyphs[] = { L"\xE838", L"\xE99A", L"\xE712" };

        for (int action = 0; action < 3; action++)
        {
            RECT  place = {};
            bool  has   = m_list.m_items.GetHomeActionRect (cell, action, place);

            if (has)
            {
                nameEnd = (action == 0) ? (float) place.left - fontPx : nameEnd;

                const std::string *  svg  = (action == m_list.m_homeHoverAction && m_list.m_homeHeader.actionHotSvgs[action] != nullptr) ? m_list.m_homeHeader.actionHotSvgs[action] : m_list.m_homeHeader.actionSvgs[action];
                float                side = m_list.m_scaler.ToPxf (16.0f);

                hr = (svg != nullptr)
                   ? text.DrawSvgIcon (*svg, (float) place.left + ((float) (place.right - place.left) - side) * 0.5f, (float) place.top + ((float) (place.bottom - place.top) - side) * 0.5f, side)
                   : text.DrawString (s_kActionGlyphs[action], (float) place.left, (float) place.top, (float) (place.right - place.left), (float) (place.bottom - place.top),
                                      pal.fg, fontPx * 1.2f, s_kGlyphFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    }

    shown = DxuiTextElide::ToWidth (text, cells[0].text, fontPx, DxuiTheme::kBodyFace, nameEnd - textX, DxuiElide::Tail, 0, true);

    if (shown != cells[0].text)
    {
        m_homeCut.emplace_back (item, 0);
    }

    hr = text.DrawString (shown.c_str(), textX, top, nameEnd - textX, lineH, pal.fg, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    top += lineH;

    if (!cells[0].tileLines.empty())
    {
        shown = DxuiTextElide::ToWidth (text, cells[0].tileLines[0].text, fontPx, DxuiTheme::kBodyFace, nameEnd - textX, DxuiElide::Tail, 0, true);

        if (shown != cells[0].tileLines[0].text)
        {
            m_homeCut.emplace_back (item, 1);
        }

        hr = text.DrawString (shown.c_str(), textX, top, nameEnd - textX, lineH, m_list.m_theme->ContentBackground() < 0xFF808080 ? s_kPathArgb : pal.fgDim, fontPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        top += lineH;
    }

    if (tile && cells[0].homePinned)
    {
        hr = text.DrawString (s_kPinFill, textX, top, lineH, lineH, s_kPinArgb, fontPx * 0.75f, s_kGlyphFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (s_kPinGlyph, textX, top, lineH, lineH, s_kPinArgb, fontPx * 0.75f, s_kGlyphFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemPainter::MeasureContentLine
//
//  A Content line's width: its label and a space, then its text.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiListItemPainter::MeasureContentLine (IDxuiTextRenderer & text, const DxuiListViewTypes::Cell & line, float fontPx)
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
//  DxuiListItemPainter::PaintContentLine
//
//  The label muted and the value after it in the text color, as Explorer
//  draws "Size: 3 bytes"; a line with no label is muted throughout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemPainter::PaintContentLine (IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, const DxuiListViewTypes::Cell & line, float x, float y, float w, float lineH, float fontPx)
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
//  DxuiListItemPainter::PaintMeter
//
//  Explorer's usage bar: an edged track, filled from the left to the
//  fraction, centered in its line and no wider than the line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListItemPainter::PaintMeter (IDxuiPainter & painter, float x, float y, float w, float lineH, float fraction) const
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
