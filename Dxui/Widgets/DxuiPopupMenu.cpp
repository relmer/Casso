#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiPopupMenu.h"
#include "DxuiMenuBar.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"

#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuItem::ForCommand
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuItem DxuiPopupMenuItem::ForCommand (const DxuiCommand * cmd)
{
    DxuiPopupMenuItem  item;



    item.kind    = Kind::Command;
    item.command = cmd;

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

DxuiPopupMenuItem DxuiPopupMenuItem::ForSubmenu (const DxuiCommand * cmd, std::vector<DxuiPopupMenuItem> children)
{
    DxuiPopupMenuItem  item;



    item.kind     = Kind::Submenu;
    item.command  = cmd;
    item.children = std::move (children);

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
    std::vector<std::unique_ptr<DxuiCommand>>  owned;
    std::vector<DxuiPopupMenuItem>             rows;



    owned.reserve (items.size());
    rows.reserve  (items.size());

    for (const Item & it : items)
    {
        std::unique_ptr<DxuiCommand>  cmd     = std::make_unique<DxuiCommand>();
        bool                          checked = it.checked;

        cmd->label     = it.label;
        cmd->isChecked = [checked] () { return checked; };

        rows.push_back (DxuiPopupMenuItem::ForCommand (cmd.get()));
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
    int  width  = 0;
    int  height = 0;
    int  left   = originX;
    int  top    = originY;



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
    }

    width  = MeasureWidthPx (text);
    height = GetContentHeightPx();

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
                                              ? ResolvePalette().bg
                                              : DxuiPopupHost::kDefaultMenuBackgroundArgb;



    ClientToScreen (owner, &topLeft);
    ClientToScreen (owner, &botRight);

    m_activePopup = m_popupHost->AcquirePopup();
    if (m_activePopup == nullptr)
    {
        return;
    }

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
    params.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderPopupMenu (p, t); };
    params.onMoveInside     = [this] (POINT localPx) { OnPopupMove  (localPx); };
    params.onClickInside    = [this] (POINT localPx) { OnPopupClick (localPx); };
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
//  DxuiPopupMenu::GetRowAtOffset
//
//  The row under a vertical offset from the menu's top edge, or -1 in a
//  separator or outside the rows. Rows are not uniform once separators are
//  in the list, so this walks rather than divides.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::GetRowAtOffset (int relY) const
{
    int  y = 0;



    if (relY < 0)
    {
        return -1;
    }

    for (int i = 0; i < (int) m_rows.size(); i++)
    {
        int  h = GetRowHeightPx (i);

        if (relY < y + h)
        {
            return (m_rows[(size_t) i].kind == DxuiPopupMenuItem::Kind::Separator) ? -1 : i;
        }

        y += h;
    }

    return -1;
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
        idx = GetRowAtOffset (y - m_boundsDip.top);
    }

    return idx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::GetRowHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::GetRowHeightPx (int index) const
{
    bool  isSeparator = m_rows[(size_t) index].kind == DxuiPopupMenuItem::Kind::Separator;



    return m_scaler.ToPx (isSeparator ? kSeparatorHeightDip : kRowHeightDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::GetRowTopPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::GetRowTopPx (int index) const
{
    int  y = 0;



    for (int i = 0; i < index; i++)
    {
        y += GetRowHeightPx (i);
    }

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::GetContentHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::GetContentHeightPx() const
{
    return GetRowTopPx ((int) m_rows.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::MeasureRunPx
//
//  One text run in pixels, measured when the renderer can and estimated
//  from a glyph width when it cannot. A renderer mid-resize can report zero
//  for a run it measured fine a frame ago, and a zero-width row would fold
//  the menu to its floor.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::MeasureRunPx (const std::wstring & run, float fontDip, IDxuiTextRenderer & text) const
{
    HRESULT  hr = S_OK;
    float    w  = 0.0f;
    float    h  = 0.0f;



    hr = text.MeasureString (run.c_str(), fontDip, DxuiTheme::kBodyFace, w, h);

    if (FAILED (hr) || w <= 0.0f)
    {
        w = (float) ((int) run.size() * m_scaler.ToPx (kFallbackGlyphWidthDip));
    }

    return (int) std::ceil (w);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::MeasureWidthPx
//
//  Width fitted to content: the widest label, plus an accelerator column
//  only when some row carries accelerator text, plus a check gutter only
//  when some row is checkable, plus padding, never below the floor. The
//  gutter and column decisions are recorded so the painter reads the SAME
//  answer; a width that reserved space the painter never drew into is the
//  disagreement this replaces.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPopupMenu::MeasureWidthPx (IDxuiTextRenderer & text)
{
    float  fontDip  = m_scaler.ToPxf (kFontDip);
    int    pad      = m_scaler.ToPx (kRowPadDip);
    int    gutter   = m_scaler.ToPx (kCheckGutterDip);
    int    gap      = m_scaler.ToPx (kAccelGapDip);
    int    minWidth = m_scaler.ToPx (kMinWidthDip);
    int    widest   = 0;
    int    width    = 0;



    m_hasGutter = false;
    m_hasAccel  = false;

    for (const DxuiPopupMenuItem & row : m_rows)
    {
        if (row.command == nullptr)
        {
            continue;
        }

        if (row.command->isChecked)          { m_hasGutter = true; }
        if (!row.command->accelerator.empty()) { m_hasAccel  = true; }
    }

    for (const DxuiPopupMenuItem & row : m_rows)
    {
        std::wstring  stripped;
        int           mnIdx = -1;
        wchar_t       mnCh  = 0;
        int           px    = 0;

        if (row.kind == DxuiPopupMenuItem::Kind::Separator || row.command == nullptr)
        {
            continue;
        }

        ParseMnemonic (row.command->GetLabelText(), stripped, mnIdx, mnCh);

        px = MeasureRunPx (stripped, fontDip, text);

        if (row.kind == DxuiPopupMenuItem::Kind::Submenu)
        {
            px += gap + MeasureRunPx (s_kpszTriangleRight, fontDip, text);
        }
        else if (!row.command->accelerator.empty())
        {
            px += gap + MeasureRunPx (row.command->accelerator, fontDip, text);
        }

        if (px > widest)
        {
            widest = px;
        }
    }

    width = pad + (m_hasGutter ? gutter : 0) + widest + pad;

    return (width > minWidth) ? width : minWidth;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::IsSelectable
//
//  A row the highlight can rest on and Enter can act on: not a separator,
//  and its command enabled.
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
            if (m_rows[(size_t) i].kind != DxuiPopupMenuItem::Kind::Separator)
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

    m_hover = index;

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

    rowTop         = GetRowTopPx (index);
    rowRect.left   = m_boundsDip.left;
    rowRect.top    = m_boundsDip.top + rowTop;
    rowRect.right  = m_boundsDip.right;
    rowRect.bottom = rowRect.top + GetRowHeightPx (index);

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
//  tear down state either lives in. Hide runs BEFORE the callback fires, so
//  a handler that opens a dialog or another menu does not do it underneath a
//  menu still on screen.
//
//  The command dispatches last, and only if it is enabled, which the row
//  already required to be selectable at all.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::Commit (int index)
{
    const DxuiCommand *  cmd  = nullptr;
    DxuiPopupMenu     *  root = nullptr;
    SelectFn             cb;



    if (!IsSelectable (index))
    {
        return;
    }

    cmd  = m_rows[(size_t) index].command;
    root = GetRoot();
    cb   = root->m_onSelect;

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

    if (idx >= 0 && idx != m_hover)
    {
        SetHover (idx);

        if (m_rows[(size_t) idx].kind == DxuiPopupMenuItem::Kind::Submenu)
        {
            OpenChild (idx, false);
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
        if (m_rows[(size_t) commit].kind == DxuiPopupMenuItem::Kind::Submenu)
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
//  DxuiPopupMenu::ResolvePalette
//
//  The application's override when one was supplied, else the theme.
//  Disabled text is always the theme's; no override carries one.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenu::Palette DxuiPopupMenu::ResolvePalette() const
{
    Palette  pal;



    pal.bg       = m_colorsSet ? m_colors.bg      : m_theme->BackgroundElevated();
    pal.hover    = m_colorsSet ? m_colors.hover   : m_theme->HoverBackground();
    pal.text     = m_colorsSet ? m_colors.text    : m_theme->Foreground();
    pal.accel    = m_colorsSet ? m_colors.accel   : m_theme->ForegroundMuted();
    pal.border   = m_colorsSet ? m_colors.border  : m_theme->Border();
    pal.divider  = m_colorsSet ? m_colors.divider : m_theme->Divider();
    pal.disabled = m_theme->ForegroundDisabled();

    return pal;
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

    PaintBody (painter, text, m_boundsDip.left, m_boundsDip.top);

    if (HasOpenChild() && m_child->m_activePopup == nullptr)
    {
        m_child->Paint (painter, text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::PaintBody
//
//  Shared menu renderer. Background, border, then each row at the supplied
//  origin. The in-window Paint passes the panel-absolute bounds; the popup
//  render hook passes (0,0).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::PaintBody (IDxuiPainter & painter, IDxuiTextRenderer & text, int originLeft, int originTop) const
{
    Palette  pal;
    float    fontDip = m_scaler.ToPxf (kFontDip);
    float    left    = (float) originLeft;
    float    top     = (float) originTop;
    float    width   = (float) (m_boundsDip.right  - m_boundsDip.left);
    float    height  = (float) (m_boundsDip.bottom - m_boundsDip.top);



    if (m_theme == nullptr)
    {
        return;
    }

    pal = ResolvePalette();

    painter.FillRect    (left, top, width, height, pal.bg);
    painter.OutlineRect (left, top, width, height, (float) kBorderDip, pal.border);

    for (int i = 0; i < (int) m_rows.size(); i++)
    {
        PaintRow (painter, text, pal, i, left, top, width, fontDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::PaintRow
//
//  One row, reading everything from the command AT PAINT TIME: label,
//  checked, enabled, accelerator. A separator is a divider line inset from
//  both edges at the row's vertical midpoint.
//
//  The label owns the full content width and the accelerator or submenu
//  arrow is right aligned within that same width; the menu was sized so the
//  two cannot meet.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::PaintRow (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const Palette      & pal,
    int                  index,
    float                left,
    float                top,
    float                width,
    float                fontDip) const
{
    HRESULT                    hr        = S_OK;
    const DxuiPopupMenuItem &  row       = m_rows[(size_t) index];
    int                        rowTopPx  = GetRowTopPx (index);
    int                        rowH      = GetRowHeightPx (index);
    int                        pad       = m_scaler.ToPx (kRowPadDip);
    int                        padTop    = m_scaler.ToPx (kRowPadTopDip);
    int                        gutter    = m_scaler.ToPx (kCheckGutterDip);
    int                        inset     = m_scaler.ToPx (kSeparatorInsetDip);
    int                        labelLeft = pad + (m_hasGutter ? gutter : 0);
    float                      y         = top + (float) rowTopPx;
    float                      contentW  = width - (float) labelLeft - (float) pad;
    std::wstring               stripped;
    int                        mnIdx     = -1;
    wchar_t                    mnCh      = 0;
    bool                       enabled   = false;
    uint32_t                   labelArgb = 0;
    uint32_t                   accelArgb = 0;



    if (row.kind == DxuiPopupMenuItem::Kind::Separator)
    {
        painter.FillRect (left + (float) inset,
                          top + (float) (rowTopPx + rowH / 2),
                          width - (float) inset - (float) inset,
                          kUnderlineThicknessDip,
                          pal.divider);
        return;
    }

    if (row.command == nullptr)
    {
        return;
    }

    enabled   = row.command->IsEnabled();
    labelArgb = enabled ? pal.text  : pal.disabled;
    accelArgb = enabled ? pal.accel : pal.disabled;

    ParseMnemonic (row.command->GetLabelText(), stripped, mnIdx, mnCh);

    if (index == m_hover)
    {
        painter.FillRect (left, y, width, (float) rowH, pal.hover);
    }

    hr = text.DrawString (stripped.c_str(),
                          left + (float) labelLeft,
                          y + (float) padTop,
                          contentW,
                          (float) rowH,
                          labelArgb,
                          fontDip,
                          DxuiTheme::kBodyFace);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (row.command->IsChecked())
    {
        hr = text.DrawString (s_kpszCheckMark,
                              left + (float) pad,
                              y + (float) padTop,
                              (float) gutter,
                              (float) rowH,
                              labelArgb,
                              fontDip,
                              DxuiTheme::kBodyFace);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_showCues && mnIdx >= 0 && !stripped.empty() && enabled)
    {
        PaintUnderline (painter, text, stripped, mnIdx,
                        left + (float) labelLeft, y + (float) padTop, fontDip, labelArgb);
    }

    if (row.kind == DxuiPopupMenuItem::Kind::Submenu)
    {
        hr = text.DrawString (s_kpszTriangleRight,
                              left + (float) labelLeft,
                              y + (float) padTop,
                              contentW,
                              (float) rowH,
                              accelArgb,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Right,
                              DxuiTextVAlign::Top);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else if (!row.command->accelerator.empty())
    {
        hr = text.DrawString (row.command->accelerator.c_str(),
                              left + (float) labelLeft,
                              y + (float) padTop,
                              contentW,
                              (float) rowH,
                              accelArgb,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Right,
                              DxuiTextVAlign::Top);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu::PaintUnderline
//
//  The mnemonic cue: a one-pixel rule under the marked letter. Its left edge
//  is the width of the text before the letter and its length is the width
//  of the prefix-plus-letter less the prefix, both measured rather than
//  estimated, so it sits under the glyph the parser marked.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenu::PaintUnderline (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const std::wstring  & stripped,
    int                   mnIdx,
    float                 labelX,
    float                 labelY,
    float                 fontDip,
    uint32_t              ink) const
{
    HRESULT       hr       = S_OK;
    float         prefixW  = 0.0f;
    float         withChW  = 0.0f;
    float         fullH    = 0.0f;
    float         ignoredH = 0.0f;
    std::wstring  prefix   = stripped.substr (0, (size_t) mnIdx);
    std::wstring  prefixCh = stripped.substr (0, (size_t) mnIdx + 1);



    if (!prefix.empty())
    {
        hr = text.MeasureString (prefix.c_str(), fontDip, DxuiTheme::kBodyFace, prefixW, fullH);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        std::wstring  oneCh (1, stripped[(size_t) mnIdx]);

        hr = text.MeasureString (oneCh.c_str(), fontDip, DxuiTheme::kBodyFace, prefixW, fullH);
        IGNORE_RETURN_VALUE (hr, S_OK);
        prefixW = 0.0f;
    }

    hr = text.MeasureString (prefixCh.c_str(), fontDip, DxuiTheme::kBodyFace, withChW, ignoredH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.FillRect (labelX + prefixW, labelY + fullH, withChW - prefixW, kUnderlineThicknessDip, ink);
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
    PaintBody (painter, text, 0, 0);
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
    int  row = GetRowAtOffset (localPx.y);



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
    int  row = GetRowAtOffset (localPx.y);



    if (row < 0)
    {
        return;
    }

    if (m_rows[(size_t) row].kind == DxuiPopupMenuItem::Kind::Submenu)
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
