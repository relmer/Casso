#include "Pch.h"

#include "Widgets/DxuiPopupMenuLayout.h"
#include "Core/DxuiIconImage.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"
#include "DxuiPopupMenu.h"
#include "DxuiMenuBar.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiSystemSettings.h"
#include "Render/DxuiStroke.h"
#include "Render/DxuiVectorIcon.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::DxuiPopupMenuLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuLayout::DxuiPopupMenuLayout (DxuiPopupMenu & menu)
    : m_menu (menu)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetRowAtOffset
//
//  The row under a vertical offset from the menu's top edge, or -1 in a
//  separator or outside the rows. Rows are not uniform once separators are
//  in the list, so this walks rather than divides.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetRowAtOffset (int relY) const
{
    int  y = m_menu.m_cardPadPx;



    if (relY < 0 || (m_viewportPx > 0 && relY >= m_viewportPx))
    {
        return -1;
    }

    //  The offset is from the menu's visible top; the rows scrolled out above
    //  it still count.
    relY += GetScrollPx();

    for (int i = 0; i < (int) m_menu.m_rows.size(); i++)
    {
        int  h = GetRowHeightPx (i);

        if (relY < y + h)
        {
            return (m_menu.m_rows[(size_t) i].kind == DxuiPopupMenuItem::Kind::Separator ||
                    m_menu.m_rows[(size_t) i].kind == DxuiPopupMenuItem::Kind::Header) ? -1 : i;
        }

        y += h;
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetRowHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetRowHeightPx (int index) const
{
    DxuiPopupMenuItem::Kind  kind = m_menu.m_rows[(size_t) index].kind;



    if (kind == DxuiPopupMenuItem::Kind::IconRow)
    {
        return (int) m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconRowDip);
    }

    return (kind == DxuiPopupMenuItem::Kind::Separator) ? m_menu.m_metrics.separatorHeightPx : m_menu.m_metrics.rowHeightPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetIconButtonPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetIconButtonPx() const
{
    return m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kIconButtonDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetIconDividerPx
//
//  The rule between two buttons: a dip, in whole pixels, which Explorer
//  gives its own place rather than drawing over a button.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetIconDividerPx() const
{
    return (int) std::ceil (m_menu.m_scaler.ToPxf (1.0f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetIconRowLeftPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetIconRowLeftPx() const
{
    return (int) m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconRowLeftDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetRowTopPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetRowTopPx (int index) const
{
    int  y = m_menu.m_cardPadPx;



    for (int i = 0; i < index; i++)
    {
        y += GetRowHeightPx (i);
    }

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetContentHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetContentHeightPx() const
{
    return GetRowTopPx ((int) m_menu.m_rows.size()) + m_menu.m_cardPadPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetScrollPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetScrollPx() const
{
    return (m_viewportPx > 0) ? GetRowTopPx (m_scrollRow) - m_menu.m_cardPadPx : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetMaxScrollRow
//
//  The first row at which the rest of the list fits the menu's height.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetMaxScrollRow() const
{
    int  content = GetContentHeightPx();
    int  row     = 0;



    if (m_viewportPx <= 0)
    {
        return 0;
    }

    while (row < (int) m_menu.m_rows.size() && content - GetRowTopPx (row) > m_viewportPx)
    {
        row++;
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::GetHeightLimitPx
//
//  The pinned limit, or for a hosted menu the room its monitor leaves: below
//  the anchor for a menu hung under one, and the whole work area otherwise.
//  Room below too small for a few rows is no room at all, and the popup
//  host flips such a menu above its anchor instead.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::GetHeightLimitPx (const RECT & anchor, DxuiPopupMenuAnchoring anchoring) const
{
    HWND         owner    = nullptr;
    POINT        top      = { anchor.left, anchor.top    };
    POINT        bottom   = { anchor.left, anchor.bottom };
    HMONITOR     monitor  = nullptr;
    MONITORINFO  info     = { sizeof (info) };
    int          below    = 0;
    int          minRoom  = DxuiPopupMenu::s_kMinRowsBelowAnchor * m_menu.m_metrics.rowHeightPx;



    if (m_maxHeightPx > 0)
    {
        return m_maxHeightPx;
    }

    owner = (m_menu.m_popupHost != nullptr) ? m_menu.m_popupHost->GetHwnd() : nullptr;

    if (owner == nullptr)
    {
        return 0;
    }

    ClientToScreen (owner, &top);
    ClientToScreen (owner, &bottom);
    monitor = MonitorFromPoint (bottom, MONITOR_DEFAULTTONEAREST);

    if (monitor == nullptr || !GetMonitorInfoW (monitor, &info))
    {
        return 0;
    }

    if (anchoring != DxuiPopupMenuAnchoring::Below)
    {
        return info.rcWork.bottom - info.rcWork.top;
    }

    below = (int) (info.rcWork.bottom - bottom.y);

    //  Flipped above, the menu has the room above its anchor instead.
    return (below >= minRoom) ? below : (std::max) (minRoom, (int) (top.y - info.rcWork.top));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::ScrollByRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenuLayout::ScrollByRows (int rows)
{
    int  row = std::clamp (m_scrollRow + rows, 0, GetMaxScrollRow());



    if (row == m_scrollRow)
    {
        return;
    }

    //  A submenu hangs from its row, which has just moved.
    m_menu.CloseChild();

    m_scrollRow = row;

    if (m_menu.m_activePopup != nullptr)
    {
        m_menu.m_activePopup->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::EnsureRowVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenuLayout::EnsureRowVisible (int index)
{
    int  top    = 0;
    int  bottom = 0;
    int  scroll = 0;



    if (m_viewportPx <= 0 || index < 0 || index >= (int) m_menu.m_rows.size())
    {
        return;
    }

    if (index < m_scrollRow)
    {
        ScrollByRows (index - m_scrollRow);
        return;
    }

    top    = GetRowTopPx (index);
    bottom = top + GetRowHeightPx (index);
    scroll = m_scrollRow;

    while (scroll < index && bottom - GetRowTopPx (scroll) > m_viewportPx)
    {
        scroll++;
    }

    ScrollByRows (scroll - m_scrollRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::MeasureRunPx
//
//  One text run in pixels, measured when the renderer can and estimated
//  from a glyph width when it cannot. A renderer mid-resize can report zero
//  for a run it measured fine a frame ago, and a zero-width row would fold
//  the menu to its floor.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::MeasureRunPx (const std::wstring & run, float fontDip, IDxuiTextRenderer & text) const
{
    HRESULT  hr = S_OK;
    float    w  = 0.0f;
    float    h  = 0.0f;



    hr = text.MeasureString (run.c_str(), fontDip, DxuiTheme::GetUiFace(), w, h);

    if (FAILED (hr) || w <= 0.0f)
    {
        w = (float) ((int) run.size() * m_menu.m_scaler.ToPx (DxuiPopupMenu::kFallbackGlyphWidthDip));
    }

    return (int) std::ceil (w);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuLayout::MeasureWidthPx
//
//  Width fitted to content as two independent columns: the label column is
//  the widest label, the accelerator column the widest accelerator or submenu
//  arrow, and a fixed gap separates them. Both column origins are recorded so
//  the painter reads the SAME answer the measurement reached.
//
//  Measuring each row as label-plus-accelerator and taking the widest, which
//  is what this replaces, sized the menu so that the LONGEST row cleared its
//  own accelerator and no other row did: a long label on an accelerator-less
//  row ran under the right-aligned accelerators above it. Separate columns
//  cannot produce that, whatever the mix of rows.
//
//  The check gutter is reserved only for a list that HAS a checkable row, so
//  a menu where nothing can ever check sits flush against its left pad rather
//  than carrying an empty column.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenuLayout::MeasureWidthPx (IDxuiTextRenderer & text)
{
    float  fontPx      = m_menu.m_metrics.fontPx;
    int    widestLabel = 0;
    int    widestAccel = 0;
    int    width       = 0;



    m_hasGutter = false;
    m_hasIcons  = false;

    for (const DxuiPopupMenuItem & row : m_menu.m_rows)
    {
        if (row.command != nullptr && row.command->isChecked)
        {
            m_hasGutter = true;
        }

        if (row.command != nullptr && (row.command->vectorIcon != nullptr || row.command->menuGlyph != nullptr || row.command->menuSvg != nullptr || row.command->menuImage != nullptr))
        {
            m_hasIcons = true;
        }
    }

    m_labelLeftPx = m_menu.m_metrics.leftPadPx + m_menu.m_metrics.gutterGapPx
                        + (m_hasGutter ? m_menu.m_metrics.checkGutterPx : 0)
                        + (m_hasIcons  ? m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kRowIconDip) + m_menu.m_metrics.gutterGapPx : 0);

    //  A menu of icons with no check column sits them as Explorer does.
    if (m_hasIcons && !m_hasGutter)
    {
        m_labelLeftPx = m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kRowIconLeftDip + DxuiPopupMenu::s_kRowIconDip + DxuiPopupMenu::s_kRowIconGapDip);
    }
    else if (m_hasIcons)
    {
        m_labelLeftPx += m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kGutterIconTextShiftDip);
    }

    for (const DxuiPopupMenuItem & row : m_menu.m_rows)
    {
        std::wstring  stripped;
        int           mnIdx = -1;
        wchar_t       mnCh  = 0;
        int           px    = 0;

        if (row.kind == DxuiPopupMenuItem::Kind::Separator || row.command == nullptr)
        {
            continue;
        }

        DxuiPopupMenu::ParseMnemonic (row.command->GetLabelText(), stripped, mnIdx, mnCh);

        px = MeasureRunPx (stripped, fontPx, text);

        if (px > widestLabel)
        {
            widestLabel = px;
        }

        //  A submenu row ends in a drawn chevron, which needs a fixed width
        //  rather than a measured glyph.
        if (row.kind == DxuiPopupMenuItem::Kind::Submenu)
        {
            px = m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kSubmenuChevronBoxDip);
        }
        else if (!row.command->accelerator.empty())
        {
            px = MeasureRunPx (row.command->accelerator, fontPx, text);
        }
        else
        {
            px = 0;
        }

        if (px > widestAccel)
        {
            widestAccel = px;
        }
    }

    m_accelWidthPx = widestAccel;
    m_accelLeftPx  = m_labelLeftPx + widestLabel + ((widestAccel > 0) ? m_menu.m_metrics.accelGapPx : 0);

    width = m_accelLeftPx + widestAccel + m_menu.m_metrics.rightPadPx;

    for (const DxuiPopupMenuItem & row : m_menu.m_rows)
    {
        if (row.kind == DxuiPopupMenuItem::Kind::IconRow)
        {
            width = (std::max) (width, GetIconRowLeftPx() * 2 + (int) row.children.size() * (GetIconButtonPx() + GetIconDividerPx()) - GetIconDividerPx());
        }
    }

    // Under the floor the slack goes between the columns, so the accelerators
    // stay against the right edge rather than stranded mid-row.
    if (width < m_menu.m_metrics.minWidthPx)
    {
        m_accelLeftPx += m_menu.m_metrics.minWidthPx - width;
        width          = m_menu.m_metrics.minWidthPx;
    }

    return width;
}
