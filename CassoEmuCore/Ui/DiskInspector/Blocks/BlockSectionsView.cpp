#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockSectionsView.h"
#include "Ui/DiskInspector/ZoomStops.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView::SetSections
//
//  Each section is a heading line and a line for each of its rows.
//
////////////////////////////////////////////////////////////////////////////////

void BlockSectionsView::SetSections (vector<BlockSection> sections)
{
    m_sections = std::move (sections);
    m_top      = 0;
    m_hover    = -1;

    m_lines.clear();

    for (const BlockSection & section : m_sections)
    {
        m_lines.push_back ({ &section, nullptr });

        for (const BlockField & row : section.rows)
        {
            m_lines.push_back ({ &section, &row });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView::GetLinesShown
//
////////////////////////////////////////////////////////////////////////////////

int BlockSectionsView::GetLinesShown() const
{
    RECT   bounds = GetBounds();
    float  line   = m_scaler.ToPxf (kRowDip);



    return line <= 0.0f ? 0 : std::max (0, (int) ((bounds.bottom - bounds.top) / line));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView::FindLineAt
//
////////////////////////////////////////////////////////////////////////////////

int BlockSectionsView::FindLineAt (POINT pointPx) const
{
    RECT   bounds = GetBounds();
    float  line   = m_scaler.ToPxf (kRowDip);
    int    index  = 0;



    if (line <= 0.0f || PtInRect (&bounds, pointPx) == FALSE)
    {
        return -1;
    }

    index = m_top + (int) ((pointPx.y - bounds.top) / line);

    return index < (int) m_lines.size() ? index : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView::TryGetRowAt
//
////////////////////////////////////////////////////////////////////////////////

bool BlockSectionsView::TryGetRowAt (POINT pointPx, BlockField & outRow) const
{
    int  line = FindLineAt (pointPx);



    if (line < 0 || m_lines[line].row == nullptr)
    {
        return false;
    }

    outRow = *m_lines[line].row;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockSectionsView::GetCursorForPoint (POINT clientPx) const
{
    int  line = FindLineAt (clientPx);



    return line >= 0 && m_lines[line].row != nullptr && m_lines[line].row->link != BlockField::kNoLink ? IDC_HAND : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool BlockSectionsView::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  bounds  = GetBounds();
    int   line    = FindLineAt (ev.positionDip);
    int   maximum = std::max (0, (int) m_lines.size() - GetLinesShown());
    bool  isLink  = line >= 0 && m_lines[line].row != nullptr && m_lines[line].row->link != BlockField::kNoLink;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Wheel:
            if (PtInRect (&bounds, ev.positionDip) == FALSE || ev.wheelHorizontal)
            {
                return false;
            }

            m_top = std::clamp (m_top - ZoomStops::GetWheelSteps (ev.wheelDelta) * kWheelRows, 0, maximum);
            return true;

        case DxuiMouseEventKind::Leave:
            m_hover = -1;
            return false;

        case DxuiMouseEventKind::Move:
        case DxuiMouseEventKind::Enter:
            m_hover = isLink ? line : -1;
            return false;

        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Left && isLink && m_onFollow)
            {
                m_onFollow ((uint32_t) m_lines[line].row->link);
                return true;
            }

            return PtInRect (&bounds, ev.positionDip) != FALSE;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView::Paint
//
//  A heading in semibold with a divider under it; a row's label muted and
//  its value after it, a link's in the link color and underlined while
//  the pointer is on it.
//
////////////////////////////////////////////////////////////////////////////////

void BlockSectionsView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT      bounds  = GetBounds();
    float     left    = (float) bounds.left;
    float     width   = (float) (bounds.right - bounds.left);
    float     line    = m_scaler.ToPxf (kRowDip);
    float     font    = m_scaler.ToPxf (kFontDip);
    float     heading = m_scaler.ToPxf (kHeadingDip);
    float     label   = std::min (m_scaler.ToPxf (kLabelDip), width / 2.0f);
    float     pad     = m_scaler.ToPxf (kPadDip);
    float     y       = 0.0f;
    float     w       = 0.0f;
    float     h       = 0.0f;
    int       shown   = GetLinesShown();
    int       i       = 0;
    HRESULT   hr      = S_OK;



    if (!IsVisible())
    {
        return;
    }

    painter.PushClipRect (left, (float) bounds.top, width, (float) (bounds.bottom - bounds.top));

    for (i = 0; i < shown && m_top + i < (int) m_lines.size(); i++)
    {
        const Line  & at = m_lines[m_top + i];

        y = bounds.top + line * i;

        if (at.row == nullptr)
        {
            hr = text.DrawString (at.section->heading.c_str(), left, y, width, line, theme.HeadingForeground(), heading, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Bottom, DxuiFontWeight::SemiBold, false);
            IGNORE_RETURN_VALUE (hr, S_OK);

            painter.FillRect (left, y + line - 1.0f, width, 1.0f, theme.Divider());
            continue;
        }

        hr = text.DrawString (at.row->label.c_str(), left + pad, y, label - pad, line, theme.ForegroundMuted(), font, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (at.row->value.c_str(), left + label, y, width - label, line,
                              at.row->link != BlockField::kNoLink ? theme.Accent() : theme.Foreground(), font, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (m_top + i == m_hover)
        {
            text.MeasureString (at.row->value.c_str(), font, DxuiTheme::kBodyFace, w, h);
            painter.FillRect (left + label, y + (line + h) / 2.0f, std::min (w, width - label), 1.0f, theme.Accent());
        }
    }

    painter.PopClipRect();
}
