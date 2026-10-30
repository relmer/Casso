#include "Pch.h"
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
//  DxuiPopupMenuItem::ForCommand
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuItem DxuiPopupMenuItem::ForCommand (std::shared_ptr<const DxuiCommand> cmd)
{
    DxuiPopupMenuItem  item;



    item.kind    = Kind::Command;
    item.command = std::move (cmd);

    return item;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuItem::ForSeparator
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuItem DxuiPopupMenuItem::ForSeparator()
{
    DxuiPopupMenuItem  item;



    item.kind = Kind::Separator;

    return item;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuItem::ForSubmenu
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuItem DxuiPopupMenuItem::ForSubmenu (std::shared_ptr<const DxuiCommand> cmd, std::vector<DxuiPopupMenuItem> children)
{
    DxuiPopupMenuItem  item;



    item.kind     = Kind::Submenu;
    item.command  = std::move (cmd);
    item.children = std::move (children);

    return item;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuItem::ForIconRow
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuItem DxuiPopupMenuItem::ForIconRow (std::vector<std::shared_ptr<const DxuiCommand>> commands)
{
    DxuiPopupMenuItem  item;



    item.kind = Kind::IconRow;

    for (std::shared_ptr<const DxuiCommand> & command : commands)
    {
        item.children.push_back (ForCommand (std::move (command)));
    }

    return item;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuItem::ForHeader
//
//  The label rides a command with nothing to dispatch, so a header measures
//  and draws through the same path as every other row's label.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuItem DxuiPopupMenuItem::ForHeader (std::wstring label)
{
    DxuiPopupMenuItem             item;
    std::shared_ptr<DxuiCommand>  cmd  = std::make_shared<DxuiCommand>();



    cmd->label   = std::move (label);
    item.kind    = Kind::Header;
    item.command = std::move (cmd);

    return item;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::DxuiPopupMenu
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenu::DxuiPopupMenu()
{
    m_clock = [] () { return (uint64_t) GetTickCount64(); };
    m_submenuDelayMs = DxuiSystemSettings::Instance().GetMenuShowDelayMs();
    RefreshMetrics();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::RefreshMetrics
//
//  Re-reads the Windows menu settings for the scaler's current DPI. A caller
//  that pinned its own metrics keeps them; nothing else caches, so a menu
//  dragged to a display at another scale is laid out for that display the
//  next time it opens.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::RefreshMetrics()
{
    if (m_metricsPin)
    {
        return;
    }

    m_metrics = DxuiMenuMetrics::FromSystem (m_scaler.GetDpi());
    m_cardPadPx = 0;

    //  Explorer's separators are 10 dip tall: 15 px at 150%.
    m_metrics.separatorHeightPx = (int) std::lround (m_scaler.ToPxf (s_kSeparatorDip));

    //  A command bar's drop-down is half a dip tighter a row than a context
    //  menu, as Explorer's are: 47 px against 48 at 150%. It also keeps
    //  3 dip above its first row and below its last, and its separators are
    //  3 dip tall: 4 px, and 5 px, at 150%. Its separators run from border to
    //  border, and it is 8 dip narrower on the right.
    if (m_compactRows)
    {
        m_metrics.rowHeightPx       -= (int) std::lround (m_scaler.ToPxf (0.5f));
        m_metrics.separatorHeightPx  = (int) std::lround (m_scaler.ToPxf (s_kCompactSeparatorDip));
        m_metrics.separatorInsetPx   = (int) kBorderDip;
        m_metrics.rightPadPx        -= m_scaler.ToPx (s_kCompactRightTrimDip);
        m_cardPadPx                  = (int) m_scaler.ToPxf (s_kCompactCardPadDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::~DxuiPopupMenu
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenu::~DxuiPopupMenu()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::SetColors
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::SetColors (
    uint32_t  bgArgb,
    uint32_t  hoverArgb,
    uint32_t  textArgb,
    uint32_t  accelArgb,
    uint32_t  borderArgb,
    uint32_t  dividerArgb)
{
    m_colors.bg      = bgArgb;
    m_colors.hover   = hoverArgb;
    m_colors.text    = textArgb;
    m_colors.accel   = accelArgb;
    m_colors.border  = borderArgb;
    m_colors.divider = dividerArgb;
    m_colorsSet      = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::HasOpenChild
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::HasOpenChild() const
{
    return m_child != nullptr && m_child->m_visible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::ShowUnder
//
//  Hangs the menu under an anchor rect, left edges aligned, as a menu bar
//  title or a toolbar button does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::ShowUnder (
    const RECT                     & anchor,
    std::vector<DxuiPopupMenuItem>   items,
    IDxuiTextRenderer              & text,
    const RECT                     & hostClient)
{
    ShowCore (anchor.left, anchor.bottom, anchor, Anchoring::Below, std::move (items), text, hostClient);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::ShowAt
//
//  Raises the menu with its top-left at a point, as a right-click does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::ShowAt (
    int                              x,
    int                              y,
    std::vector<DxuiPopupMenuItem>   items,
    IDxuiTextRenderer              & text,
    const RECT                     & hostClient)
{
    RECT  anchor = { x, y, x, y };



    ShowCore (x, y, anchor, Anchoring::AtPoint, std::move (items), text, hostClient);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Show
//
//  The legacy entry: rows given as a label and a checked flag. Each becomes a
//  command the widget owns, with the flag behind an `isChecked` functor so
//  the list reserves its check gutter exactly as the old widget always did,
//  and the row list then goes through the ordinary point show.
//
//  The new commands replace the old ones ONLY IF THE SHOW WENT THROUGH. A
//  show the reopen guard swallowed leaves the previous rows in place, and
//  those still point at the previous commands.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Show (
    int                   anchorX,
    int                   anchorY,
    std::vector<Item>     items,
    IDxuiTextRenderer   & text,
    const RECT          & hostClient)
{
    std::vector<std::shared_ptr<DxuiCommand>>  owned;
    std::vector<DxuiPopupMenuItem>             rows;



    owned.reserve (items.size());
    rows.reserve  (items.size());

    for (const Item & it : items)
    {
        std::shared_ptr<DxuiCommand>  cmd     = std::make_shared<DxuiCommand>();
        bool                          checked = it.checked;

        cmd->label     = it.label;
        cmd->isChecked = [checked] () { return checked; };

        rows.push_back (DxuiPopupMenuItem::ForCommand (cmd));
        owned.push_back (std::move (cmd));
    }

    ShowAt (anchorX, anchorY, std::move (rows), text, hostClient);

    if (m_visible)
    {
        m_legacyItems   = std::move (items);
        m_ownedCommands = std::move (owned);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::ShowCore
//
//  Sizes the menu to its rows, clamps it inside the host client rect so it
//  never paints off-screen, and acquires a pooled popup when a host is wired.
//
//  Only a ROOT consults the reopen guard. A child re-shown a moment after it
//  closed is the pointer wandering off a submenu row and back, which must
//  reopen it every time.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::ShowCore (
    int                              originX,
    int                              originY,
    const RECT                     & anchor,
    Anchoring                        anchoring,
    std::vector<DxuiPopupMenuItem>   items,
    IDxuiTextRenderer              & text,
    const RECT                     & hostClient)
{
    int  width   = 0;
    int  height  = 0;
    int  limit   = 0;
    int  visible = 0;
    int  left    = originX;
    int  top     = originY;



    if (m_parent == nullptr && m_reopenGuard && IsReopenSuppressed (anchor))
    {
        return;
    }

    CloseChild();

    m_rows       = std::move (items);
    m_hover      = -1;
    m_pressed    = -1;
    m_visible    = true;
    m_text       = &text;
    m_hostClient = hostClient;
    m_anchor     = anchor;

    // Popup DPI follows its host window, folding what used to be an
    // explicit SetDpi push from the consumer into the show path.
    if (m_popupHost != nullptr)
    {
        m_scaler.SetDpi (m_popupHost->GetScaler().GetDpi());
        RefreshMetrics();
    }

    PlaceIconRow (OpensUpward (originX, originY, m_layout.GetContentHeightPx(), anchoring));
    DropStraySeparators();

    width        = (std::max) (m_layout.MeasureWidthPx (text), m_minWidthPx);
    height       = m_layout.GetContentHeightPx();
    limit        = m_layout.GetHeightLimitPx (anchor, anchoring);
    m_layout.m_viewportPx = 0;
    m_layout.m_scrollRow  = 0;

    //  Too tall: as many whole rows as fit, and the rest scroll.
    if (limit > 0 && height > limit)
    {
        visible = 0;

        while (visible < (int) m_rows.size() && m_layout.GetRowTopPx (visible + 1) <= limit)
        {
            visible++;
        }

        height       = m_layout.GetRowTopPx (visible);
        m_layout.m_viewportPx = height;
    }

    if (left + width  > hostClient.right)  { left = hostClient.right  - width;  }
    if (top  + height > hostClient.bottom) { top  = hostClient.bottom - height; }
    if (left < hostClient.left) { left = hostClient.left; }
    if (top  < hostClient.top)  { top  = hostClient.top;  }

    m_boundsDip.left   = left;
    m_boundsDip.top    = top;
    m_boundsDip.right  = left + width;
    m_boundsDip.bottom = top  + height;

    if (m_popupHost != nullptr && m_activePopup == nullptr)
    {
        AcquirePopup (anchor, anchoring);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::AcquirePopup
//
//  Renders the menu into a top-level popup so it is not clipped by the owner
//  client area. The anchor rect is converted to screen physical pixels; the
//  menu metrics are physical pixels, so the size goes back to DIPs for the
//  host, which scales once by DPI. A child links to its parent's popup so a
//  click inside an ancestor dismisses only the levels above it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::AcquirePopup (const RECT & anchor, Anchoring anchoring)
{
    HRESULT                    hr        = S_OK;
    DxuiPopupHost::ShowParams  params;
    HWND                       owner     = m_popupHost->GetHwnd();
    POINT                      topLeft   = { anchor.left,  anchor.top    };
    POINT                      botRight  = { anchor.right, anchor.bottom };
    UINT                       dpi       = m_scaler.GetDpi();
    int                        width     = m_boundsDip.right  - m_boundsDip.left;
    int                        height    = m_boundsDip.bottom - m_boundsDip.top;
    uint32_t                   bgArgb    = (m_theme != nullptr)
                                              ? m_painter.ResolvePalette().bg
                                              : DxuiPopupHost::kDefaultMenuBackgroundArgb;



    ClientToScreen (owner, &topLeft);
    ClientToScreen (owner, &botRight);

    m_activePopup = m_popupHost->AcquirePopup();
    if (m_activePopup == nullptr)
    {
        return;
    }

    //  Explorer's menus are acrylic: the card tints a blurred view of what is
    //  behind it, unless the system's transparency effects are off.
    m_acrylic = DxuiPopupHost::IsAcrylicAvailable();

    params.ownerHwnd        = owner;
    params.anchorRectScreen = { topLeft.x, topLeft.y, botRight.x, botRight.y };
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::OnClickOutside;
    params.input            = DxuiPopupInput::Interactive;
    params.shadow           = true;
    params.grabsCapture     = m_grabsCapture;
    params.sizeDip.cx       = MulDiv (width,  DxuiDpiScaler::kBaseDpi, (int) dpi);
    params.sizeDip.cy       = MulDiv (height, DxuiDpiScaler::kBaseDpi, (int) dpi);
    params.backgroundArgb   = bgArgb;
    params.acrylic          = m_acrylic;

    //  The host sizes its card from those dips, which can round back a pixel
    //  wider or taller; the menu takes that size, so its border rings the
    //  whole card rather than leaving a strip of the card outside it.
    m_boundsDip.right  = m_boundsDip.left + MulDiv (params.sizeDip.cx, (int) dpi, DxuiDpiScaler::kBaseDpi);
    m_boundsDip.bottom = m_boundsDip.top  + MulDiv (params.sizeDip.cy, (int) dpi, DxuiDpiScaler::kBaseDpi);

    // The open animation. A menu dropped from a bar, and a submenu, unfold;
    // a context menu, opened at a point, fades in quickly, as Explorer's do.
    // The system's animation switch, and the accessibility master switch
    // behind it, turn either off.
    params.revealFade       = anchoring == Anchoring::AtPoint;
    params.revealMs         = (!m_revealSuppressed &&
                               DxuiSystemSettings::Instance().AreMenuAnimationsEnabled())
                                  ? (params.revealFade ? kFadeMs : kRevealMs) : 0;
    params.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderPopupMenu (p, t); };
    params.onMoveInside     = [this] (POINT localPx) { OnPopupMove  (localPx); };
    params.onClickInside    = [this] (POINT localPx) { OnPopupClick (localPx); };
    params.onWheel          = [this] (int delta)     { m_layout.ScrollByRows (-(delta / WHEEL_DELTA) * s_kWheelRows); };
    params.onClosed         = [this] () { Hide(); };
    params.onClickOutside   = m_onClickOutside;

    switch (anchoring)
    {
    case Anchoring::Below:   params.placement = DxuiPopupPlacement::Below;    break;
    case Anchoring::Beside:  params.placement = DxuiPopupPlacement::Right;    break;
    case Anchoring::AtPoint: params.placement = DxuiPopupPlacement::AtCursor; break;
    }

    hr = m_activePopup->Show (std::move (params));
    if (FAILED (hr))
    {
        m_popupHost->ReleasePopup (m_activePopup);
        m_activePopup = nullptr;
        return;
    }

    if (m_parent != nullptr && m_parent->m_activePopup != nullptr)
    {
        m_activePopup->SetParentPopup (m_parent->m_activePopup);
    }

}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::IsReopenSuppressed
//
//  A click on the title or button whose menu is up must close it, and that
//  is harder than it looks: the popup dismisses itself on a click outside
//  its own window, and whether that runs before or after the opener sees the
//  same click is not ours to decide. Either order leaves the menu shut by
//  the time the opener acts, so it would cheerfully open it again and the
//  button would never appear to toggle.
//
//  So a show from the same anchor within a short window of the last hide is
//  refused. The window is short enough that a deliberate second click always
//  lands outside it, and it covers both orderings without either side having
//  to know about the other.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::IsReopenSuppressed (const RECT & anchor) const
{
    uint64_t  now        = 0;
    bool      sameAnchor = false;



    if (!m_hasClosed)
    {
        return false;
    }

    now        = m_clock();
    sameAnchor = EqualRect (&anchor, &m_lastAnchor) != FALSE;

    return sameAnchor && (now - m_closedAtMs) < kReopenGuardMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Hide
//
//  Children close first, so a chain comes down from the leaf. The active
//  popup is cleared BEFORE it is released, because the popup's own closed
//  hook routes back here and must find nothing left to do.
//
//  The closed callback fires on the visible-to-hidden edge only. Hide is
//  reached from the dismiss paths, from a pick and from the popup's hook,
//  several of which run against an already hidden menu; firing every time
//  would hand a previewing caller a settle it never asked for.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Hide()
{
    DxuiPopupHost *  popup      = m_activePopup;
    bool             wasVisible = m_visible;
    bool             committed  = m_committing;



    CloseChild();
    SetIconHover (-1);

    m_visible     = false;
    m_hover       = -1;
    m_pressed     = -1;
    m_activePopup = nullptr;
    m_committing  = false;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }

    if (wasVisible)
    {
        m_closedAtMs = m_clock();
        m_lastAnchor = m_anchor;
        m_hasClosed  = true;

        if (m_onClosed)
        {
            m_onClosed (committed);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::IsPointInRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::IsPointInRect (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::HitTest
//
//  True over this level or any open child, so an owner routing input can ask
//  one question about the whole chain.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::HitTest (int x, int y) const
{
    bool  hit = false;



    if (m_visible)
    {
        hit = IsPointInRect (m_boundsDip, x, y);

        if (!hit && HasOpenChild())
        {
            hit = m_child->HitTest (x, y);
        }
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::HitTestIndex
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::HitTestIndex (int x, int y) const
{
    int  idx = -1;



    if (m_visible && IsPointInRect (m_boundsDip, x, y))
    {
        idx = m_layout.GetRowAtOffset (y - m_boundsDip.top);
    }

    return idx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::SetIconHover
//
//  The icon row is the only row with buttons, so a button index is enough to
//  find its command.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::SetIconHover (int button)
{
    const DxuiCommand  * command = nullptr;



    if (button == m_iconHover)
    {
        return;
    }

    m_iconHover = button;

    for (const DxuiPopupMenuItem & row : m_rows)
    {
        if (button >= 0 && row.kind == DxuiPopupMenuItem::Kind::IconRow && button < (int) row.children.size())
        {
            command = row.children[(size_t) button].command.get();
            break;
        }
    }

    if (m_onIconHover)
    {
        m_onIconHover (command);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::GetIconButtonAt
//
//  The buttons sit side by side from the row's left padding.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::GetIconButtonAt (int index, int localX) const
{
    int  size   = m_layout.GetIconButtonPx() + m_layout.GetIconDividerPx();
    int  offset = localX - m_layout.GetIconRowLeftPx();
    int  button = 0;



    if (index < 0 || index >= (int) m_rows.size() || m_rows[(size_t) index].kind != DxuiPopupMenuItem::Kind::IconRow ||
        size <= 0 || offset < 0)
    {
        return -1;
    }

    button = offset / size;

    return (button < (int) m_rows[(size_t) index].children.size()) ? button : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::CommitIcon
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::CommitIcon (int index, int button)
{
    std::shared_ptr<const DxuiCommand>  cmd;
    DxuiPopupMenu                     * root = nullptr;



    if (button < 0 || index < 0 || index >= (int) m_rows.size() ||
        button >= (int) m_rows[(size_t) index].children.size())
    {
        return;
    }

    cmd = m_rows[(size_t) index].children[(size_t) button].command;

    if (cmd == nullptr || !cmd->IsEnabled())
    {
        return;
    }

    root = GetRoot();

    root->m_committing = true;
    root->Hide();

    if (cmd->dispatch)
    {
        cmd->dispatch();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::DropStraySeparators
//
//  A caller building a menu from conditional groups can leave a separator
//  first, last, or beside another; each draws as an empty band, so they go.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::DropStraySeparators()
{
    std::vector<DxuiPopupMenuItem>  kept;



    for (DxuiPopupMenuItem & row : m_rows)
    {
        bool  isSeparator = row.kind == DxuiPopupMenuItem::Kind::Separator;

        if (isSeparator && (kept.empty() || kept.back().kind == DxuiPopupMenuItem::Kind::Separator))
        {
            continue;
        }

        kept.push_back (std::move (row));
    }

    while (!kept.empty() && kept.back().kind == DxuiPopupMenuItem::Kind::Separator)
    {
        kept.pop_back();
    }

    m_rows = std::move (kept);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OpensUpward
//
//  Whether the pointer ends up nearer the menu's bottom than its top. A menu
//  raised at the pointer hangs down from it, and one that would run off the
//  monitor is slid back up onto it rather than flipped, as the popup host
//  places it -- so it can end up over the pointer, and which end is nearer
//  depends on how far it moved.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OpensUpward (int originX, int originY, int heightPx, Anchoring anchoring) const
{
    HWND         owner   = (m_popupHost != nullptr) ? m_popupHost->GetHwnd() : nullptr;
    POINT        origin  = { originX, originY };
    HMONITOR     monitor = nullptr;
    MONITORINFO  info    = { sizeof (info) };



    if (anchoring != Anchoring::AtPoint)
    {
        return false;
    }

    if (owner == nullptr)
    {
        {
        int  top = (std::max) ((int) m_hostClient.top, (std::min) (originY, (int) m_hostClient.bottom - heightPx));

        return (top + heightPx - originY) < (originY - top);
    }
    }

    ClientToScreen (owner, &origin);
    monitor = MonitorFromPoint (origin, MONITOR_DEFAULTTONEAREST);

    if (monitor == nullptr || !GetMonitorInfoW (monitor, &info))
    {
        return false;
    }

    {
        LONG  top    = (std::max) (info.rcWork.top, (std::min) (origin.y, info.rcWork.bottom - (LONG) heightPx));
        LONG  bottom = top + heightPx;

        return (bottom - origin.y) < (origin.y - top);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::PlaceIconRow
//
//  First for a menu hanging down from the pointer, last for one rising up
//  from it, so the buttons are always the nearest thing to the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::PlaceIconRow (bool atBottom)
{
    DxuiPopupMenuItem               icons;
    std::vector<DxuiPopupMenuItem>  rest;
    bool                            found = false;



    for (DxuiPopupMenuItem & row : m_rows)
    {
        if (!found && row.kind == DxuiPopupMenuItem::Kind::IconRow)
        {
            icons = std::move (row);
            found = true;
        }
        else
        {
            rest.push_back (std::move (row));
        }
    }

    m_rows.clear();

    if (!found)
    {
        m_rows = std::move (rest);
        return;
    }

    //  The separator beside the buttons goes with them.
    if (atBottom)
    {
        if (!rest.empty() && rest.front().kind == DxuiPopupMenuItem::Kind::Separator)
        {
            rest.erase (rest.begin());
        }

        m_rows = std::move (rest);

        if (!m_rows.empty())
        {
            m_rows.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        m_rows.push_back (std::move (icons));
    }
    else
    {
        if (!rest.empty() && rest.back().kind == DxuiPopupMenuItem::Kind::Separator)
        {
            rest.pop_back();
        }

        m_rows.push_back (std::move (icons));

        if (!rest.empty() && rest.front().kind != DxuiPopupMenuItem::Kind::Separator)
        {
            m_rows.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        for (DxuiPopupMenuItem & row : rest)
        {
            m_rows.push_back (std::move (row));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::IsSelectable
//
//  A row the highlight can rest on and Enter can act on: not a separator or a
//  header, and its command enabled.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::IsSelectable (int index) const
{
    const DxuiPopupMenuItem *  row = nullptr;



    if (index < 0 || index >= (int) m_rows.size())
    {
        return false;
    }

    row = &m_rows[(size_t) index];

    return row->kind != DxuiPopupMenuItem::Kind::Separator
        && row->kind != DxuiPopupMenuItem::Kind::Header
        && row->command != nullptr
        && row->command->IsEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::FindNextSelectable
//
//  The next selectable row in a direction, wrapping, skipping separators and
//  disabled rows. From no highlight, Down starts at the first row and Up at
//  the last, so each key lands where a user expects on a fresh menu.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::FindNextSelectable (int from, int direction) const
{
    int  n = (int) m_rows.size();
    int  i = from;



    for (int k = 0; k < n; k++)
    {
        if (i < 0)
        {
            i = (direction > 0) ? 0 : n - 1;
        }
        else
        {
            i = (i + direction + n) % n;
        }

        if (IsSelectable (i))
        {
            return i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::FindFirstSelectable
//
//  The first enabled row; failing that, the first row that is not a
//  separator, so a menu whose every row is disabled still opens with a
//  highlight the user can see and Enter still does nothing.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::FindFirstSelectable() const
{
    int  first = FindNextSelectable (-1, 1);



    if (first < 0)
    {
        for (int i = 0; i < (int) m_rows.size(); i++)
        {
            if (m_rows[(size_t) i].kind != DxuiPopupMenuItem::Kind::Separator &&
                m_rows[(size_t) i].kind != DxuiPopupMenuItem::Kind::Header)
            {
                first = i;
                break;
            }
        }
    }

    return first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::SetHover
//
//  The single place the highlight moves, so the preview notification cannot
//  be forgotten on one of the paths that move it (pointer in the in-window
//  menu, pointer in the hosted popup, and the two arrow keys).
//
//  THE NOTIFICATION GOES FIRST AND THE RE-RENDER SECOND. MarkDirty is
//  synchronous, and a listener that previews a THEME changes the palette this
//  menu is about to paint itself in. Rendering ahead of it drew every row in
//  the palette of the row highlighted before -- correct on the first open and
//  one behind for the rest of the walk.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::SetHover (int index)
{
    if (index == m_hover)
    {
        return;
    }

    // The highlight leaving a row cancels the submenu that row had armed;
    // the caller re-arms if the row moved to is itself a submenu row.
    DisarmChild();

    m_hover = index;

    //  A highlight moved past the menu's edge by a key brings its row into
    //  view; the pointer can only rest on a row already in view.
    m_layout.EnsureRowVisible (index);

    if (index >= 0 && m_onHighlight)
    {
        m_onHighlight (index);
    }

    // Re-read: a listener that resized or reskinned the owner can have taken
    // the popup down, which leaves nothing to render.
    if (m_activePopup != nullptr)
    {
        m_activePopup->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::ArmChild
//
//  A submenu opens after the pointer has RESTED on its row, not the instant
//  the pointer crosses it, which is what lets a diagonal move to a row below
//  pass over a submenu row without opening it. The delay is the system's own
//  menu show delay.
//
//  Arming the row that is already open does nothing: the child stays up and
//  no timer restarts, so sliding along an open submenu's parent row cannot
//  make it flicker.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::ArmChild (int index)
{
    if (index == m_childRow && HasOpenChild())
    {
        return;
    }

    if (m_submenuDelayMs <= 0)
    {
        OpenChild (index, false);
        return;
    }

    m_pendingChild = index;
    m_pendingAtMs  = m_clock() + (uint64_t) m_submenuDelayMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::DisarmChild
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::DisarmChild()
{
    m_pendingChild = -1;
    m_pendingAtMs  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::WantsTick
//
//  Asked by a host whose idle loop parks when no input arrives. A submenu
//  waiting out its delay is precisely the case where no input is coming --
//  the pointer is resting -- so the host has to keep ticking until this goes
//  false or the child will never open.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::WantsTick() const
{
    if (m_pendingChild >= 0)
    {
        return true;
    }

    if (m_activePopup != nullptr && m_activePopup->IsRevealing())
    {
        return true;
    }

    return (m_child != nullptr) && m_child->WantsTick();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Tick
//
//  Opens an armed submenu once its delay has run out, deepest level first so
//  one tick can advance a whole open chain.
//
//  The deadline is measured on the widget's own clock rather than on
//  `nowMs`, which is only passed down the chain. The arm happens in
//  OnMouseMove, which has no frame time to work from, and the reopen guard
//  already reads that clock; one time source is what keeps a test able to
//  inject a clock and step it past a delay without sleeping.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Tick (int64_t nowMs)
{
    if (m_child != nullptr)
    {
        m_child->Tick (nowMs);
    }

    if (m_activePopup != nullptr)
    {
        m_activePopup->AdvanceReveal (nowMs);
    }

    if (m_pendingChild < 0)
    {
        return;
    }

    if (m_clock() >= m_pendingAtMs)
    {
        int  row = m_pendingChild;

        DisarmChild();
        OpenChild (row, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OpenChild
//
//  Raises the child list of a submenu row beside that row. The child is
//  allocated once and reused, which keeps every hosted-popup callback that
//  captured it valid for the life of the parent. It inherits everything the
//  parent was configured with at the moment it opens, so a theme or color
//  change between opens reaches it.
//
//  Opening the row that is already open only moves its highlight, so a
//  repeated hover or Right does not flicker the child.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OpenChild (int index, bool highlightFirst)
{
    const DxuiPopupMenuItem *  row     = nullptr;
    RECT                       rowRect = {};
    int                        rowTop  = 0;



    if (index < 0 || index >= (int) m_rows.size() || m_text == nullptr)
    {
        return;
    }

    row = &m_rows[(size_t) index];

    if (row->kind != DxuiPopupMenuItem::Kind::Submenu)
    {
        return;
    }

    if (index == m_childRow && HasOpenChild())
    {
        if (highlightFirst)
        {
            m_child->SetHover (m_child->FindFirstSelectable());
        }

        return;
    }

    CloseChild();

    if (m_child == nullptr)
    {
        m_child = std::make_unique<DxuiPopupMenu>();
    }

    m_child->m_parent       = this;
    m_child->m_theme        = m_theme;
    m_child->m_popupHost    = m_popupHost;
    m_child->m_grabsCapture = m_grabsCapture;
    m_child->m_showCues     = m_showCues;
    m_child->m_colorsSet    = m_colorsSet;
    m_child->m_colors       = m_colors;
    m_child->m_clock        = m_clock;
    m_child->m_scaler.SetDpi (m_scaler.GetDpi());

    rowTop         = m_layout.GetRowTopPx (index) - m_layout.GetScrollPx();
    rowRect.left   = m_boundsDip.left;
    rowRect.top    = m_boundsDip.top + rowTop;
    rowRect.right  = m_boundsDip.right;
    rowRect.bottom = rowRect.top + m_layout.GetRowHeightPx (index);

    m_child->ShowCore (m_boundsDip.right, rowRect.top, rowRect, Anchoring::Beside,
                       row->children, *m_text, m_hostClient);

    m_childRow = index;

    if (highlightFirst)
    {
        m_child->SetHover (m_child->FindFirstSelectable());
    }

    if (m_activePopup != nullptr)
    {
        m_activePopup->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::CloseChild
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::CloseChild()
{
    if (HasOpenChild())
    {
        m_child->Hide();
    }

    DisarmChild();
    m_childRow = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::GetRoot
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenu * DxuiPopupMenu::GetRoot()
{
    DxuiPopupMenu *  menu = this;



    while (menu->m_parent != nullptr)
    {
        menu = menu->m_parent;
    }

    return menu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Commit
//
//  Picks a row. The whole chain comes down from the ROOT, which is the level
//  the owner installed its callbacks on, so closed fires there once, before
//  select, with the committed flag set.
//
//  The callback and the command are captured before Hide, since Hide can
//  tear down state either lives in. The command is held by a shared copy, so
//  a dispatch that rebuilds the rows it came from cannot free it mid-call. Hide runs BEFORE the callback fires, so
//  a handler that opens a dialog or another menu does not do it underneath a
//  menu still on screen.
//
//  The command dispatches last, and only if it is enabled, which the row
//  already required to be selectable at all.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Commit (int index)
{
    std::shared_ptr<const DxuiCommand>  cmd;
    DxuiPopupMenu                     * root = nullptr;
    SelectFn                            cb;



    if (!IsSelectable (index))
    {
        return;
    }

    cmd  = m_rows[(size_t) index].command;
    root = GetRoot();
    cb   = root->m_onSelect;

    //  A toggle that keeps the menu open runs and redraws its check.
    if (cmd->keepsMenuOpen)
    {
        if (cmd->dispatch)
        {
            cmd->dispatch();
        }

        if (m_activePopup != nullptr)
        {
            m_activePopup->MarkDirty();
        }

        return;
    }

    root->m_committing = true;
    root->Hide();

    if (cb)
    {
        cb (index);
    }

    if (cmd->dispatch)
    {
        cmd->dispatch();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnMouseMove
//
//  In-window path only; a hosted popup owns its own input through the host
//  window. Input over an open child goes to the child. Resting on a submenu
//  row opens its child unhighlighted, and moving to any other row of this
//  level closes it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OnMouseMove (int x, int y)
{
    int  idx = -1;



    if (m_activePopup != nullptr || !m_visible)
    {
        return;
    }

    if (HasOpenChild() && m_child->HitTest (x, y))
    {
        m_child->OnMouseMove (x, y);
        return;
    }

    idx = HitTestIndex (x, y);
    SetIconHover (GetIconButtonAt (idx, x - m_boundsDip.left));

    if (idx >= 0 && idx != m_hover)
    {
        SetHover (idx);

        if (m_rows[(size_t) idx].kind == DxuiPopupMenuItem::Kind::Submenu)
        {
            ArmChild (idx);
        }
        else
        {
            CloseChild();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnLButtonDown
//
//  m_visible is captured before Hide can clear it: a visible menu consumes
//  the click either way, so the result is about the state on ENTRY, not on
//  exit.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnLButtonDown (int x, int y)
{
    bool  wasVisible = m_visible;



    if (!wasVisible || m_activePopup != nullptr)
    {
        return wasVisible;
    }

    if (HasOpenChild() && m_child->HitTest (x, y))
    {
        m_child->OnLButtonDown (x, y);
        return true;
    }

    if (!HitTest (x, y))
    {
        Hide();
    }
    else
    {
        m_pressed = HitTestIndex (x, y);
    }

    return wasVisible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnLButtonUp
//
//  Commits a row on a completed click, opens a submenu row, or dismisses on
//  a click-away. A commit requires the release to land on the SAME row the
//  press did, so a press-then-drag-off cancels rather than selecting
//  whatever it ended over.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnLButtonUp (int x, int y)
{
    bool  wasVisible = m_visible;
    int   idx        = -1;
    int   commit     = -1;



    if (!wasVisible || m_activePopup != nullptr)
    {
        return wasVisible;
    }

    if (HasOpenChild() && m_child->HitTest (x, y))
    {
        m_child->OnLButtonUp (x, y);
        return true;
    }

    idx = HitTestIndex (x, y);

    if (idx >= 0 && idx == m_pressed)
    {
        commit = idx;
    }

    m_pressed = -1;

    if (commit >= 0)
    {
        if (m_rows[(size_t) commit].kind == DxuiPopupMenuItem::Kind::IconRow)
        {
            CommitIcon (commit, GetIconButtonAt (commit, x - m_boundsDip.left));
        }
        else if (m_rows[(size_t) commit].kind == DxuiPopupMenuItem::Kind::Submenu)
        {
            OpenChild (commit, true);
        }
        else
        {
            Commit (commit);
        }
    }
    else if (!HitTest (x, y))
    {
        Hide();
    }

    return wasVisible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnKey
//
//  Keyboard handling for an open menu: navigate, open or close a child,
//  commit, or dismiss.
//
//  A visible menu SWALLOWS EVERY KEY, including ones it does nothing with --
//  that is what the return value reports. A menu is modal in practice, so
//  letting an unhandled key through would type into whatever is behind it.
//
//  With a child open, Left and Escape close only the child, and every other
//  key goes to the child; the deepest open level is the one that acts. A
//  child with a child of its own gets the Left or Escape itself, and applies
//  the same rule one level down.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnKey (WPARAM vk)
{
    bool  isSubmenuRow = false;



    if (!m_visible)
    {
        return false;
    }

    if (HasOpenChild())
    {
        if (!m_child->HasOpenChild() && (vk == VK_LEFT || vk == VK_ESCAPE))
        {
            CloseChild();
            return true;
        }

        return m_child->OnKey (vk);
    }

    isSubmenuRow = m_hover >= 0
                && m_hover < (int) m_rows.size()
                && m_rows[(size_t) m_hover].kind == DxuiPopupMenuItem::Kind::Submenu;

    switch (vk)
    {
    case VK_ESCAPE:
        Hide();
        break;

    case VK_DOWN:
        SetHover (FindNextSelectable (m_hover, 1));
        break;

    case VK_UP:
        SetHover (FindNextSelectable (m_hover, -1));
        break;

    case VK_RIGHT:
        if (isSubmenuRow)
        {
            OpenChild (m_hover, true);
        }

        break;

    case VK_RETURN:
    case VK_SPACE:
        if (isSubmenuRow)
        {
            OpenChild (m_hover, true);
        }
        else
        {
            Commit (m_hover);
        }

        break;

    default:
        break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Paint
//
//  In-window fallback path (no popup host wired up). Draws at the
//  panel-absolute bounds, then any open child that is likewise in-window.
//  Suppressed when a real popup is active; the popup renders itself through
//  RenderPopupMenu.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    if (!m_visible || m_activePopup != nullptr || m_theme == nullptr)
    {
        return;
    }

    m_painter.PaintBody (painter, text, m_boundsDip.left, m_boundsDip.top);

    if (HasOpenChild() && m_child->m_activePopup == nullptr)
    {
        m_child->Paint (painter, text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::RenderPopupMenu
//
//  Popup-local render hook (origin 0,0 = popup top-left). The host already
//  cleared the back buffer to the theme background; this draws the border,
//  hover row and rows on top.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::RenderPopupMenu (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    m_painter.PaintBody (painter, text, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnPopupMove
//
//  Pointer-move inside the hosted popup (popup-local physical pixels). Maps
//  the y to a row and applies the same hover rules as the in-window path.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OnPopupMove (POINT localPx)
{
    int  row    = m_layout.GetRowAtOffset (localPx.y);
    int  button = GetIconButtonAt (row, localPx.x);



    if (button != m_iconHover)
    {
        SetIconHover (button);

        if (m_activePopup != nullptr)
        {
            m_activePopup->MarkDirty();
        }
    }

    if (row >= 0 && row != m_hover)
    {
        SetHover (row);

        if (m_rows[(size_t) row].kind == DxuiPopupMenuItem::Kind::Submenu)
        {
            OpenChild (row, false);
        }
        else
        {
            CloseChild();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnPopupClick
//
//  Left-click inside the hosted popup (popup-local physical pixels). Commits
//  the row under the cursor, or opens it when it is a submenu row.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::OnPopupClick (POINT localPx)
{
    int  row = m_layout.GetRowAtOffset (localPx.y);



    if (row < 0)
    {
        return;
    }

    if (m_rows[(size_t) row].kind == DxuiPopupMenuItem::Kind::IconRow)
    {
        CommitIcon (row, GetIconButtonAt (row, localPx.x));
    }
    else if (m_rows[(size_t) row].kind == DxuiPopupMenuItem::Kind::Submenu)
    {
        OpenChild (row, true);
    }
    else
    {
        Commit (row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Layout  (IDxuiControl override)
//
//  The menu geometry is computed by the show path; the override only records
//  the panel-supplied bounds for IDxuiControl::GetBounds() consumers and
//  updates the DPI scaler.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_theme == nullptr)
    {
        m_theme = &theme;
    }

    static_cast<const DxuiPopupMenu *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A HOSTED popup never reaches this path -- it lives in its own HWND and
//  delivers input through the callbacks installed when it was shown. This
//  serves the in-window fallback menu only.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        OnMouseMove (ev.positionDip.x, ev.positionDip.y);
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
//  DxuiPopupMenu::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPopupMenu::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = OnKey (ev.vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::ParseMnemonic
//
//  One parser for every label with an ampersand in it. The menu bar owns the
//  body today, because its titles needed it first; this widget calls
//  through so the rows and the titles can never disagree about where the
//  underline goes.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::ParseMnemonic (
    const std::wstring  & label,
    std::wstring        & outStripped,
    int                 & outIndex,
    wchar_t             & outLower)
{
    DxuiMenuBar::ParseMnemonic (label, outStripped, outIndex, outLower);
}
