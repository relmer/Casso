#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiToolbar.h"
#include "Window/DxuiHwndSource.h"




// An in-window menu is kept inside the host client rect. Until a host says
// what that is, nothing constrains it.
static constexpr LONG s_kUnboundedClientPx = 1L << 28;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::DxuiToolbar
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar::DxuiToolbar()
{
    m_focusable         = false;
    m_hostClient.left   = -s_kUnboundedClientPx;
    m_hostClient.top    = -s_kUnboundedClientPx;
    m_hostClient.right  =  s_kUnboundedClientPx;
    m_hostClient.bottom =  s_kUnboundedClientPx;

    WireDropDown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::~DxuiToolbar
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar::~DxuiToolbar()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetEntries
//
//  Replaces the table and resets every runtime state, since the old rects
//  and hover flags belong to entries that no longer exist. A menu or flyout
//  open on the old table comes down with it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetEntries (std::vector<Entry> entries)
{
    m_dropdown.Hide();
    CloseFlyout();
    m_focusIndex = -1;

    m_slots.clear();
    m_slots.reserve (entries.size());

    for (Entry & e : entries)
    {
        Slot  slot;

        slot.entry = std::move (e);
        m_slots.push_back (std::move (slot));
    }

    m_labeledCount = (int) m_slots.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetStripColors
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetStripColors (uint32_t stripArgb, uint32_t textArgb)
{
    m_stripColorsSet = true;
    m_stripOverride  = stripArgb;
    m_textOverride   = textArgb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetPopupHost
//
//  A live popup is released before the host is repointed, as the menu bar
//  does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetPopupHost (DxuiHwndSource * host)
{
    if (host != m_dropdown.GetPopupHost())
    {
        m_dropdown.Hide();
    }

    m_dropdown.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::WireDropDown
//
//  The one menu serves every drop-down entry; which picker is open decides
//  where the three callbacks route.
//
//  The menu hides BEFORE it reports a pick, and says on the way out whether
//  a pick is what closed it. That is what lets the settle be unconditional:
//  on a dismissal it puts the row the menu opened on back, and on a pick it
//  stands aside for the commit that is about to arrive.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::WireDropDown()
{
    m_dropdown.SetOnHighlightChange ([this] (int index)
    {
        auto  it = m_pickers.find (m_openPicker);

        if (it != m_pickers.end())
        {
            it->second.previewed = true;

            if (it->second.preview)
            {
                it->second.preview (index);
            }
        }
    });

    m_dropdown.SetOnSelect ([this] (int index)
    {
        auto  it = m_pickers.find (m_openPicker);

        if (it != m_pickers.end())
        {
            it->second.previewed = false;

            if (it->second.commit)
            {
                it->second.commit (index);
            }
        }

        m_openPicker = -1;
    });

    m_dropdown.SetOnClosed ([this] (bool committed)
    {
        auto  it = m_pickers.find (m_openPicker);

        if (it != m_pickers.end())
        {
            if (!committed && it->second.previewed && it->second.openedOn >= 0 && it->second.preview)
            {
                it->second.preview (it->second.openedOn);
            }

            it->second.previewed = false;
        }

        if (!committed)
        {
            m_openPicker = -1;
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetDropDownItems / SetDropDownSinks
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetDropDownItems (int commandId, std::vector<DxuiPopupMenuItem> items)
{
    m_pickers[commandId].items = std::move (items);
}


void DxuiToolbar::SetDropDownSinks (int commandId, ChoiceFn preview, ChoiceFn commit)
{
    m_pickers[commandId].preview = std::move (preview);
    m_pickers[commandId].commit  = std::move (commit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetFlyoutControl
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetFlyoutControl (int commandId, IDxuiControl * control, SIZE panelDp)
{
    CloseFlyout();

    m_flyoutId      = commandId;
    m_flyoutControl = control;
    m_flyoutPanelDp = panelDp;

    LayoutFlyout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OpenFlyout / CloseFlyout
//
//  A flyout opened by keyboard gives its hosted control focus, so the keys
//  that follow reach it, and only Escape or a collapse closes it; the
//  pointer wandering off cannot. Closing takes the focus back.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::OpenFlyout (bool byKeyboard)
{
    if (m_flyoutControl == nullptr)
    {
        return;
    }

    m_flyoutOpen = true;

    if (byKeyboard && !m_flyoutKeyboard)
    {
        m_flyoutKeyboard = true;
        m_flyoutControl->OnFocusChanged (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::CloseFlyout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::CloseFlyout()
{
    if (m_flyoutKeyboard && m_flyoutControl != nullptr)
    {
        m_flyoutControl->OnFocusChanged (false);
    }

    m_flyoutOpen     = false;
    m_flyoutKeyboard = false;
    m_flyoutPressed  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetFocusIndex / ActivateFocused / OwnsKeyboard
//
//  Focus moving off an entry closes a flyout it opened by keyboard, since
//  the panel's keys belong to that entry.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetFocusIndex (int index)
{
    if (index < 0 || index >= (int) m_slots.size())
    {
        index = -1;
    }

    if (index != m_focusIndex && m_flyoutKeyboard)
    {
        CloseFlyout();
    }

    m_focusIndex = index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::ActivateFocused
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::ActivateFocused()
{
    Slot *               slot = (m_focusIndex >= 0 && m_focusIndex < (int) m_slots.size()) ? &m_slots[(size_t) m_focusIndex] : nullptr;
    const DxuiCommand *  cmd  = (slot != nullptr) ? slot->entry.command : nullptr;



    if (cmd == nullptr || !cmd->IsEnabled())
    {
        return;
    }

    switch (slot->entry.kind)
    {
    case Kind::DropDown:
        OpenDropDown (cmd->id);
        break;

    case Kind::Flyout:
        if (cmd->id == m_flyoutId && m_flyoutControl != nullptr)
        {
            OpenFlyout (true);
        }
        else if (cmd->dispatch)
        {
            cmd->dispatch();
        }

        break;

    case Kind::Command:
    case Kind::Toggle:
        if (cmd->dispatch) { cmd->dispatch(); }
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OwnsKeyboard
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::OwnsKeyboard() const
{
    return m_dropdown.IsVisible() || (m_flyoutOpen && m_flyoutKeyboard);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::IsFlyoutOpen
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::IsFlyoutOpen (int commandId) const
{
    return m_flyoutOpen && commandId == m_flyoutId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::IsMenuOpen / HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::IsMenuOpen() const
{
    return m_dropdown.IsVisible();
}


bool DxuiToolbar::HandleKey (WPARAM vk)
{
    bool  handled = false;



    if (m_dropdown.IsVisible())
    {
        handled = m_dropdown.OnKey (vk);
    }
    else if (m_flyoutOpen && m_flyoutKeyboard && m_flyoutControl != nullptr)
    {
        // The panel owns the keyboard: Escape gives it back to the entry,
        // everything else is the hosted control's.
        if (vk == VK_ESCAPE)
        {
            CloseFlyout();
            handled = true;
        }
        else
        {
            DxuiKeyEvent  ev;

            ev.kind  = DxuiKeyEventKind::Down;
            ev.vk    = vk;
            ev.shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
            ev.ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;

            handled = m_flyoutControl->OnKey (ev);
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::IsPointInRect / HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::IsPointInRect (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}


bool DxuiToolbar::HitTest (int x, int y) const
{
    return IsPointInRect (m_barRect, x, y) ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetBandDp
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetBandDp() const
{
    return kBandDp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::FindSlot / IsLabeled
//
////////////////////////////////////////////////////////////////////////////////

const DxuiToolbar::Slot * DxuiToolbar::FindSlot (int commandId) const
{
    for (const Slot & slot : m_slots)
    {
        if (slot.entry.command != nullptr && slot.entry.command->id == commandId)
        {
            return &slot;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::FindSlot
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar::Slot * DxuiToolbar::FindSlot (int commandId)
{
    return const_cast<Slot *> (static_cast<const DxuiToolbar *> (this)->FindSlot (commandId));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::IsLabeled
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::IsLabeled (int commandId) const
{
    const Slot *  slot = FindSlot (commandId);



    return slot != nullptr && slot->labeled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::MeasureLabelPx
//
//  Measured when the renderer can, and estimated from a character width
//  when it cannot, so a plan made before the renderer exists still lands
//  near the truth.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::MeasureLabelPx (const wchar_t * text, float fontPx) const
{
    float    w         = 0.0f;
    float    h         = 0.0f;
    HRESULT  hrMeasure = E_FAIL;



    if (text == nullptr || text[0] == 0)
    {
        return 0;
    }

    if (m_textRenderer != nullptr)
    {
        hrMeasure = m_textRenderer->MeasureString (text, fontPx, DxuiTheme::kBodyFace, w, h);
    }

    if (SUCCEEDED (hrMeasure) && w > 0.0f)
    {
        return (int) (w + 0.5f);
    }

    return (int) ((float) wcslen (text) * kFallbackCharPx * fontPx / kFontDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetEntryWidthPx
//
//  One entry costs its icon plus padding, and its label when it can still
//  afford one. A custom entry states its own cost in both forms.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetEntryWidthPx (const Slot & slot, bool labeled) const
{
    int           padX    = m_scaler.ToPx (kBtnPadXDp);
    int           iconGap = m_scaler.ToPx (kIconGapDp);
    float         fontPx  = m_scaler.ToPxf (kFontDip);
    int           iconW   = (int) (m_scaler.ToPxf (kIconDip) + 0.5f);
    int           width   = 0;
    std::wstring  label;



    if (slot.entry.custom != nullptr)
    {
        return slot.entry.custom->GetWidthPx (labeled, m_scaler, m_textRenderer);
    }

    width = padX * 2 + iconW;

    if (labeled && slot.entry.command != nullptr)
    {
        label  = slot.entry.command->GetShortText();
        width += iconGap + MeasureLabelPx (label.c_str(), fontPx);
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetTotalWidthPx
//
//  Entries in one group sit a button gap apart; a change of group opens the
//  wider group gap instead.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetTotalWidthPx (int labeledCount) const
{
    int  barPad   = m_scaler.ToPx (kBarPadXDp);
    int  btnGap   = m_scaler.ToPx (kBtnGapDp);
    int  groupGap = m_scaler.ToPx (kGroupGapDp);
    int  width    = barPad * 2;
    int  index    = 0;



    for (const Slot & slot : m_slots)
    {
        width += GetEntryWidthPx (slot, index < labeledCount);

        if (index + 1 < (int) m_slots.size())
        {
            width += (m_slots[(size_t) index + 1].entry.group != slot.entry.group) ? groupGap : btnGap;
        }

        index++;
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PlanForWidth
//
//  Drops one label at a time FROM THE RIGHT until the strip fits, so the
//  leftmost entries keep their names longest and nothing is ever pushed off
//  the end. The band thickness is fixed: everything stays on one row, which
//  is what makes a per-entry collapse legible in the first place.
//
//  Once every entry is down to its icon there are no moves left.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::PlanForWidth (int clientWidthPx, const DxuiDpiScaler & scaler)
{
    int  labeled = (int) m_slots.size();



    m_scaler.SetDpi (scaler.GetDpi());

    while (labeled > 0 && GetTotalWidthPx (labeled) > clientWidthPx)
    {
        labeled--;
    }

    m_labeledCount = labeled;

    return kBandDp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::Layout
//
//  Places the entries left to right. The collapse is re-planned against
//  this exact strip width so the plan and the placement can never disagree.
//  A flyout entry that collapses while its flyout is open loses the flyout,
//  and the collapsed icon reopens it on the next dwell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   marginY  = 0;
    int   btnGap   = 0;
    int   groupGap = 0;
    int   barPad   = 0;
    int   x        = 0;
    int   top      = 0;
    int   bottom   = 0;
    int   index    = 0;



    PlanForWidth (boundsDip.right - boundsDip.left, scaler);

    marginY  = m_scaler.ToPx (kBtnMarginYDp);
    btnGap   = m_scaler.ToPx (kBtnGapDp);
    groupGap = m_scaler.ToPx (kGroupGapDp);
    barPad   = m_scaler.ToPx (kBarPadXDp);
    x        = boundsDip.left + barPad;
    top      = boundsDip.top + marginY;
    bottom   = boundsDip.bottom - marginY;

    m_barRect = boundsDip;

    for (Slot & slot : m_slots)
    {
        int   width      = 0;
        bool  wasLabeled = slot.labeled;

        slot.labeled = index < m_labeledCount;
        width        = GetEntryWidthPx (slot, slot.labeled);
        slot.rc      = RECT { x, top, x + width, bottom };
        x           += width;

        if (index + 1 < (int) m_slots.size())
        {
            x += (m_slots[(size_t) index + 1].entry.group != slot.entry.group) ? groupGap : btnGap;
        }

        if (slot.entry.custom != nullptr)
        {
            slot.entry.custom->Layout (slot.rc, slot.labeled, m_scaler);
        }

        if (wasLabeled && !slot.labeled && slot.entry.command != nullptr && slot.entry.command->id == m_flyoutId)
        {
            CloseFlyout();
        }

        index++;
    }

    LayoutFlyout();
    SetBounds (m_barRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::LayoutFlyout
//
//  The flyout hangs under its entry, centered on it and never off the left
//  of the strip; the hosted control fills it inside the padding.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::LayoutFlyout()
{
    const Slot *  slot   = FindSlot (m_flyoutId);
    int           flyW   = m_scaler.ToPx ((int) m_flyoutPanelDp.cx);
    int           flyH   = m_scaler.ToPx ((int) m_flyoutPanelDp.cy);
    int           flyPad = m_scaler.ToPx (kFlyoutPadDp);
    int           drop   = m_scaler.ToPx (kFlyoutDropDp);
    int           fx     = 0;
    RECT          inner  = {};



    if (slot == nullptr || m_flyoutControl == nullptr)
    {
        m_flyoutRc = {};
        return;
    }

    fx = slot->rc.left + ((slot->rc.right - slot->rc.left) - flyW) / 2;
    fx = (std::max) (fx, (int) m_barRect.left);

    m_flyoutRc = RECT { fx, m_barRect.bottom + drop, fx + flyW, m_barRect.bottom + drop + flyH };

    inner = RECT { m_flyoutRc.left + flyPad,  m_flyoutRc.top + flyPad,
                   m_flyoutRc.right - flyPad, m_flyoutRc.bottom - flyPad };

    m_flyoutControl->Layout (inner, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetFlyoutKeepAliveRc
//
//  The union of the flyout entry and its panel, so the pointer can travel
//  between them across the bar's margin without the flyout closing.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbar::GetFlyoutKeepAliveRc() const
{
    const Slot *  slot = FindSlot (m_flyoutId);
    RECT          rc   = m_flyoutRc;



    if (slot != nullptr)
    {
        rc        = slot->rc;
        rc.left   = (std::min) (rc.left,   m_flyoutRc.left);
        rc.right  = (std::max) (rc.right,  m_flyoutRc.right);
        rc.bottom = (std::max) (rc.bottom, m_flyoutRc.bottom);
    }

    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetTooltipAt
//
//  A collapsed entry has no label on the strip, so its full name surfaces
//  as a tooltip (the host owns the tooltip widget and its dwell timing). An
//  entry whose command carries an explicit tip shows it in every form,
//  since that tip says something the label cannot. A custom entry is asked
//  first, so its own parts can carry tips of their own.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiToolbar::GetTooltipAt (int x, int y, RECT & anchor) const
{
    const wchar_t *  tip = nullptr;



    for (const Slot & slot : m_slots)
    {
        const DxuiCommand *  cmd  = slot.entry.command;
        bool                 over = false;

        if (tip != nullptr)
        {
            break;
        }

        if (slot.entry.custom != nullptr)
        {
            tip = slot.entry.custom->GetTooltipAt (x, y, anchor);
        }

        over = tip == nullptr && cmd != nullptr && cmd->IsEnabled() && IsPointInRect (slot.rc, x, y);

        if (!over)
        {
            continue;
        }

        if (!cmd->tip.empty())
        {
            anchor = slot.rc;
            tip    = cmd->tip.c_str();
        }
        else if (!slot.labeled)
        {
            anchor = slot.rc;
            tip    = cmd->label.c_str();
        }
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::ForwardToFlyout
//
//  Hands one pointer event to the hosted control, and remembers a press it
//  took so the flyout stays up until the release even if the pointer wanders
//  out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::ForwardToFlyout (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, bool & handled)
{
    DxuiMouseEvent  ev = {};



    if (m_flyoutControl == nullptr || (!m_flyoutOpen && !m_flyoutPressed))
    {
        return;
    }

    ev.kind        = kind;
    ev.button      = button;
    ev.positionDip = POINT { x, y };

    if (m_flyoutControl->OnMouse (ev))
    {
        handled = true;

        if (kind == DxuiMouseEventKind::Down) { m_flyoutPressed = true; }
    }

    if (kind == DxuiMouseEventKind::Up)
    {
        m_flyoutPressed = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OnToolbarMouseMove
//
//  Host-forwarded pointer motion. The hosted control gets first claim while
//  its flyout is up; otherwise hover states update per entry. An expanded
//  custom entry IS its parts, so the entry itself never draws hover chrome
//  around them.
//
//  The flyout opens on hover over its entry and stays while the pointer
//  remains in the entry-panel corridor -- the union rect, so the travel
//  across the bar's bottom margin cannot close it. A press in progress on
//  the hosted control pins it open regardless.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::OnToolbarMouseMove (int x, int y)
{
    bool  over       = false;
    bool  overFlyout = false;



    ForwardToFlyout (DxuiMouseEventKind::Move, DxuiMouseButton::None, x, y, over);

    for (Slot & slot : m_slots)
    {
        bool  enabled = slot.entry.command != nullptr && slot.entry.command->IsEnabled();

        slot.hovered = enabled && IsPointInRect (slot.rc, x, y);

        if (slot.entry.custom != nullptr)
        {
            over = slot.entry.custom->OnMouseMove (x, y) || over;

            if (slot.labeled) { slot.hovered = false; }
        }

        if (!slot.hovered) { slot.pressed = false; }
        over = over || slot.hovered;

        if (slot.entry.command != nullptr && slot.entry.command->id == m_flyoutId && m_flyoutControl != nullptr)
        {
            overFlyout = slot.hovered;
        }
    }

    if (overFlyout)
    {
        OpenFlyout (false);
    }
    else if (m_flyoutOpen && !m_flyoutKeyboard && !m_flyoutPressed && !IsPointInRect (GetFlyoutKeepAliveRc(), x, y))
    {
        CloseFlyout();
    }

    return over || (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y)) ||
           IsPointInRect (m_barRect, x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OnToolbarMouseLeave
//
//  The pointer left the window entirely; a press on the hosted control can
//  survive that (the host keeps forwarding while captured), so the flyout
//  only closes when idle. An open menu is a separate window the pointer has
//  just moved into, so leaving the strip must not close that either.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::OnToolbarMouseLeave()
{
    for (Slot & slot : m_slots)
    {
        slot.hovered = false;
        slot.pressed = false;

        if (slot.entry.custom != nullptr)
        {
            slot.entry.custom->OnMouseLeave();
        }
    }

    if (!m_flyoutPressed && !m_flyoutKeyboard)
    {
        CloseFlyout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OnToolbarLButtonDown
//
//  Press handling: arm an entry, start a press on the hosted control, or
//  eat the click.
//
//  AN OPEN MENU TAKES THE PRESS AND NOTHING ELSE DOES. Clicking anywhere on
//  the strip while a menu is up dismisses it, which is what makes the
//  drop-down buttons toggle instead of reopening the menu the same click just
//  closed.
//
//  A press only ARMS an entry; the command fires on release. That is what
//  makes press-then-drag-off cancel, the behavior every Windows button has.
//  A custom entry that takes the press for one of its parts is armed the
//  same way, so its OnClick sees the release.
//
//  A press on the bar's DEAD SPACE is still consumed. The toolbar sits over
//  the host's content, so an unclaimed click would otherwise reach whatever
//  is behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::OnToolbarLButtonDown (int x, int y)
{
    bool  handled = false;



    if (m_dropdown.IsVisible())
    {
        m_dropdown.Hide();

        return true;
    }

    ForwardToFlyout (DxuiMouseEventKind::Down, DxuiMouseButton::Left, x, y, handled);

    for (Slot & slot : m_slots)
    {
        bool  enabled = slot.entry.command != nullptr && slot.entry.command->IsEnabled();
        bool  took    = false;

        if (handled || !enabled || !IsPointInRect (slot.rc, x, y))
        {
            continue;
        }

        if (slot.entry.custom != nullptr)
        {
            took = slot.entry.custom->OnLButtonDown (x, y);
        }

        // An expanded custom entry is armed only through one of its parts;
        // everywhere else the entry itself is the button.
        if (took || slot.entry.custom == nullptr || !slot.labeled)
        {
            slot.pressed = true;
            handled      = true;
        }
    }

    return handled || IsPointInRect (m_barRect, x, y) ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OnToolbarLButtonUp
//
//  Release handling: act on a completed click, and clear every pressed visual.
//
//  The loop clears EVERY entry's pressed state regardless of where the release
//  landed, because a press that ends elsewhere is a cancel and must leave
//  nothing stuck down. Only a press and release on the SAME entry acts.
//
//  What "act" means is the entry's kind, unless a custom entry consumed the
//  click itself. A drop-down opens after the loop, so opening a menu cannot
//  disturb the pressed-state sweep that is still in progress.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::OnToolbarLButtonUp (int x, int y)
{
    bool  handled    = false;
    bool  hasOpening = false;
    int   opening    = 0;



    ForwardToFlyout (DxuiMouseEventKind::Up, DxuiMouseButton::Left, x, y, handled);

    for (Slot & slot : m_slots)
    {
        const DxuiCommand *  cmd        = slot.entry.command;
        bool                 wasPressed = slot.pressed;
        bool                 consumed   = false;

        slot.pressed = false;

        if (handled || !wasPressed || cmd == nullptr || !cmd->IsEnabled() || !IsPointInRect (slot.rc, x, y))
        {
            continue;
        }

        if (slot.entry.custom != nullptr)
        {
            consumed = slot.entry.custom->OnClick (x, y);
        }

        if (!consumed)
        {
            switch (slot.entry.kind)
            {
            case Kind::DropDown:
                hasOpening = true;
                opening    = cmd->id;
                break;

            case Kind::Command:
            case Kind::Toggle:
            case Kind::Flyout:
                if (cmd->dispatch) { cmd->dispatch(); }
                break;
            }
        }

        handled = true;
    }

    if (hasOpening)
    {
        OpenDropDown (opening);
    }

    return handled || IsPointInRect (m_barRect, x, y) ||
           (m_flyoutOpen && IsPointInRect (m_flyoutRc, x, y));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OpenDropDown
//
//  Hangs the one menu under a drop-down entry with that entry's rows. The
//  row the menu opened on is the checked one, which is what a dismissal
//  replays. The menu's own reopen guard refuses a show that follows its
//  dismissal on the same click, which is what makes the button toggle.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::OpenDropDown (int commandId)
{
    const Slot *  slot = FindSlot (commandId);
    auto          it   = m_pickers.find (commandId);



    if (slot == nullptr || it == m_pickers.end() || it->second.items.empty() || m_textRenderer == nullptr)
    {
        return;
    }

    it->second.openedOn  = -1;
    it->second.previewed = false;

    for (size_t i = 0; i < it->second.items.size() && it->second.openedOn < 0; i++)
    {
        const DxuiCommand *  row = it->second.items[i].command;

        if (row != nullptr && row->IsChecked())
        {
            it->second.openedOn = (int) i;
        }
    }

    m_openPicker = commandId;
    m_dropdown.SetOnClickOutside (m_onDropDownClickOutside);
    m_dropdown.ShowAt (slot->rc.left, m_barRect.bottom, it->second.items, *m_textRenderer, m_hostClient);

    if (!m_dropdown.IsVisible())
    {
        m_openPicker = -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintEntryIcon
//
//  One glyph in the icon face, left-aligned in its column and vertically
//  centered on the entry.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintEntryIcon (const Slot & slot, IDxuiTextRenderer & text, const DxuiToolbarIconBox & icon, uint32_t ink)
{
    HRESULT          hr    = S_OK;
    const wchar_t *  glyph = (slot.entry.command != nullptr) ? slot.entry.command->glyph : nullptr;



    if (glyph == nullptr || glyph[0] == 0)
    {
        return;
    }

    hr = text.DrawString (glyph, icon.x, icon.top, icon.size + 2.0f, icon.rowH,
                          ink, icon.size, m_iconFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintSlot
//
//  Draws one entry: an icon, and its label beside it when it still has one.
//
//  Background chrome is drawn ONLY when hovered or pressed, or while a
//  toggle's command is checked. An idle toolbar shows bare icons on the bar,
//  which is what keeps a row of ten buttons from reading as ten boxes.
//
//  Disabled entries dim the ink by rewriting its ALPHA rather than
//  substituting a theme color, so the disabled look follows whatever the
//  theme's foreground is instead of needing a matching swatch per theme.
//
//  The decoration is handed the ICON's box, not the entry's rect, so
//  whatever it draws stays pinned to the glyph regardless of how much label
//  space the entry's current form leaves around it. A custom entry paints
//  everything inside the chrome itself.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintSlot (Slot & slot, IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT              hr      = S_OK;
    const DxuiCommand *  cmd     = slot.entry.command;
    bool                 enabled = cmd != nullptr && cmd->IsEnabled();
    bool                 checked = slot.entry.kind == Kind::Toggle && cmd != nullptr && cmd->IsChecked();
    bool                 active  = slot.hovered || slot.pressed || checked;
    float                bl      = (float) slot.rc.left;
    float                bt      = (float) slot.rc.top;
    float                bw      = (float) (slot.rc.right  - slot.rc.left);
    float                bh      = (float) (slot.rc.bottom - slot.rc.top);
    float                fontDip = m_scaler.ToPxf (kFontDip);
    float                iconDip = m_scaler.ToPxf (kIconDip);
    int                  padX    = m_scaler.ToPx (kBtnPadXDp);
    int                  iconGap = m_scaler.ToPx (kIconGapDp);
    uint32_t             ink     = m_stripColorsSet ? m_textOverride : theme.ButtonText();
    float                textX   = 0.0f;
    DxuiToolbarIconBox   icon;
    std::wstring         label;



    if (!enabled)
    {
        ink = (ink & 0x00FFFFFFu) | kDisabledInkAlpha;
    }

    if (active)
    {
        uint32_t  fill = (slot.pressed || checked) ? theme.ButtonPressed()
                                                   : (slot.hovered ? theme.ButtonHover() : theme.ButtonIdle());

        painter.FillRect    (bl, bt, bw, bh, fill);
        painter.OutlineRect (bl, bt, bw, bh, 1.0f, theme.ButtonBorder());
    }

    // The keyboard focus ring sits just outside the entry, as the drive
    // widgets draw theirs, so it never covers the hover chrome.
    if (&slot == &m_slots[(size_t) (std::max) (m_focusIndex, 0)] && m_focusIndex >= 0)
    {
        float  ring = (float) m_scaler.ToPx (2);
        float  pen  = (float) (std::max) (1, m_scaler.ToPx (1));

        painter.OutlineRect (bl - ring, bt - ring, bw + ring * 2.0f, bh + ring * 2.0f, pen, theme.FocusRing());
    }

    if (slot.entry.custom != nullptr)
    {
        slot.entry.custom->Paint (painter, text, theme, slot.hovered, slot.pressed, slot.labeled);
        return;
    }

    // A labeled entry keeps its icon left-padded with the label beside it; a
    // collapsed one centers the icon in what is left.
    icon.x    = slot.labeled ? bl + (float) padX : bl + (bw - iconDip) * 0.5f;
    icon.top  = bt;
    icon.size = iconDip;
    icon.rowH = bh;

    PaintEntryIcon (slot, text, icon, ink);

    if (slot.entry.decoration)
    {
        slot.entry.decoration (painter, theme, icon, !slot.labeled);
    }

    if (slot.labeled && cmd != nullptr)
    {
        label = cmd->GetShortText();

        if (!label.empty())
        {
            textX = bl + (float) padX + iconDip + (float) iconGap;

            hr = text.DrawString (label.c_str(), textX, bt,
                                  (float) slot.rc.right - textX, bh,
                                  ink, fontDip, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left,
                                  DxuiTextVAlign::CenterOnCapHeight);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::Paint
//
//  A bottom hairline separates the strip from whatever is below it; entries
//  paint over the strip fill, frameless until hovered.
//
//  The menu paints into its own popup window, outside this call, so it is
//  handed the theme here; without it the popup comes up as an empty box.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float     bl    = (float) m_barRect.left;
    float     btTop = (float) m_barRect.top;
    float     bw    = (float) (m_barRect.right - m_barRect.left);
    float     bhAll = (float) (m_barRect.bottom - m_barRect.top);
    uint32_t  strip = m_stripColorsSet ? m_stripOverride : theme.Background();



    m_dropdown.SetTheme (&theme);

    if (bw <= 0.0f)
    {
        return;
    }

    painter.FillRect (bl, btTop, bw, bhAll, strip);
    painter.FillRect (bl, (float) m_barRect.bottom - 1.0f, bw, 1.0f, theme.ButtonBorder());

    for (Slot & slot : m_slots)
    {
        PaintSlot (slot, painter, text, theme);
    }

    // The flyout paints LAST: it hangs below the bar over whatever is there,
    // and everything on the bar must be under it. The menu is a real popup
    // window and paints itself.
    if (m_flyoutOpen)
    {
        PaintFlyout (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintFlyout
//
//  A small panel hanging under its entry: strip surface, hairline border,
//  and the hosted control inside. The panel background must be OPAQUE -- it
//  floats over live content, and a translucent flyout would read as a
//  rendering artifact.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintFlyout (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float     fl    = (float) m_flyoutRc.left;
    float     ft    = (float) m_flyoutRc.top;
    float     fw    = (float) (m_flyoutRc.right  - m_flyoutRc.left);
    float     fh    = (float) (m_flyoutRc.bottom - m_flyoutRc.top);
    uint32_t  strip = m_stripColorsSet ? m_stripOverride : theme.Background();



    if (m_flyoutControl == nullptr)
    {
        return;
    }

    painter.FillRect (fl - 1.0f, ft - 1.0f, fw + 2.0f, fh + 2.0f, theme.ButtonBorder());
    painter.FillRect (fl, ft, fw, fh, strip);

    m_flyoutControl->Paint (painter, text, theme);
}