#include "Pch.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Theme/DxuiTheme.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiPaneMetrics.h"
#include "Core/DxuiTip.h"

#include "DxuiToolbar.h"
#include "DxuiMenuBar.h"
#include "Window/DxuiHwndSource.h"
#include "Render/DxuiShadow.h"




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

    RefreshMetrics();
    WireDropDown();

    s_toolbars.push_back (this);

    //  Every strip overflows: what does not fit even as icons goes into a
    //  "..." menu. A host may still call EnableSeeMore for its own glyph.
    EnableSeeMore (s_kpszMdl2More, L"See more");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::~DxuiToolbar
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar::~DxuiToolbar()
{
    std::erase (s_toolbars, this);
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

        //  The See more button closes the leading entries, before the first
        //  trailing one.
        if (m_seeMore != nullptr && e.trailing && (m_slots.empty() || !m_slots.back().entry.trailing))
        {
            Slot  more;

            more.entry.command    = m_seeMore;
            more.entry.group      = kSeeMoreId;
            more.entry.iconOnly   = true;
            more.entry.vectorIcon = m_seeMoreIcon;
            m_slots.push_back (std::move (more));
        }

        slot.entry = std::move (e);
        m_slots.push_back (std::move (slot));
    }

    if (m_seeMore != nullptr && (m_slots.empty() || !m_slots.back().entry.trailing))
    {
        Slot  more;

        more.entry.command    = m_seeMore;
        more.entry.group      = kSeeMoreId;
        more.entry.iconOnly   = true;
        more.entry.vectorIcon = m_seeMoreIcon;
        m_slots.push_back (std::move (more));
    }

    m_labeledCount = (int) m_slots.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::EnableSeeMore
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::EnableSeeMore (const wchar_t * glyph, const wchar_t * tip, const DxuiVectorIcon * icon)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->id       = kSeeMoreId;
    command->label    = L"See more";
    command->glyph    = glyph;
    command->tip      = tip;
    command->dispatch = [this]() { OpenSeeMore(); };

    m_seeMore     = std::move (command);
    m_seeMoreIcon = icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::IsInSeeMore
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::IsInSeeMore (int commandId) const
{
    const Slot *  slot = FindSlot (commandId);



    return slot != nullptr && slot->hidden;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PlanSeeMore
//
//  Everything the menu always holds goes there first. Then, while the strip
//  is too wide with every label showing, the rightmost leading entry still
//  on the strip joins them, and the button appears. Custom entries shrink
//  toward their minimums first; one of a fixed width then goes the same way,
//  and one that can shrink stays. A pinned entry always stays. Once no leading entry is left, the
//  trailing ones follow, rightmost first, so a window too narrow for them
//  does not cut them off at its edge.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PlanSeeMore (int clientWidthPx)
{
    Slot *  moreSlot = nullptr;
    bool    any      = false;



    for (Slot & slot : m_slots)
    {
        bool  isMore = slot.entry.command != nullptr && slot.entry.command->id == kSeeMoreId;

        if (isMore)
        {
            moreSlot = &slot;
            continue;
        }

        slot.hidden   = slot.entry.seeMoreOnly;
        slot.shrinkPx = 0;
        any           = any || slot.hidden;
    }

    if (moreSlot == nullptr)
    {
        ShrinkCustomEntries (clientWidthPx);
        return;
    }

    moreSlot->hidden = !any;
    m_overflowed     = false;

    while (GetTotalWidthPx (m_labels ? (int) m_slots.size() : 0) > clientWidthPx)
    {
        Slot *  victim = nullptr;
        int     room   = 0;

        for (Slot & slot : m_slots)
        {
            if (!slot.hidden && !slot.entry.trailing && !slot.entry.pinned && slot.entry.custom == nullptr && &slot != moreSlot)
            {
                victim = &slot;
            }
        }

        //  With only custom entries left to move, they shrink toward their
        //  minimums first. One that can shrink stays on the strip, at its
        //  minimum however narrow the strip gets, as an address bar does; one
        //  of a fixed width moves when the shrinking isn't room enough.
        for (const Slot & slot : m_slots)
        {
            room += (!slot.hidden && slot.entry.custom != nullptr) ? GetCustomRoomPx (slot) : 0;
        }

        if (victim == nullptr && GetTotalWidthPx (m_labels ? (int) m_slots.size() : 0) - room <= clientWidthPx)
        {
            break;
        }

        for (Slot & slot : m_slots)
        {
            if (!slot.hidden && !slot.entry.trailing && !slot.entry.pinned && slot.entry.custom != nullptr && GetCustomRoomPx (slot) == 0 && &slot != moreSlot)
            {
                victim = (victim == nullptr || victim->entry.custom != nullptr) ? &slot : victim;
            }
        }

        for (Slot & slot : m_slots)
        {
            if (victim == nullptr || victim->entry.trailing)
            {
                if (!slot.hidden && slot.entry.trailing && !slot.entry.pinned)
                {
                    victim = &slot;
                }
            }
        }

        if (victim == nullptr)
        {
            break;
        }

        victim->hidden   = true;
        moreSlot->hidden = false;
        m_overflowed     = true;
    }

    ShrinkCustomEntries (clientWidthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::ShrinkCustomEntries
//
//  The strip's overrun after every movable entry is in See more comes out
//  of the custom entries, in proportion to how far each can shrink.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::ShrinkCustomEntries (int clientWidthPx)
{
    int  over = GetTotalWidthPx (m_labels ? (int) m_slots.size() : 0) - clientWidthPx;
    int  room = 0;



    if (over <= 0)
    {
        return;
    }

    for (const Slot & slot : m_slots)
    {
        if (!slot.hidden && slot.entry.custom != nullptr)
        {
            room += GetCustomRoomPx (slot);
        }
    }

    if (room <= 0)
    {
        return;
    }

    for (Slot & slot : m_slots)
    {
        if (!slot.hidden && slot.entry.custom != nullptr)
        {
            int  share = GetCustomRoomPx (slot);

            slot.shrinkPx = (std::min) (share, (over * share + room - 1) / room);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetCustomRoomPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetCustomRoomPx (const Slot & slot) const
{
    return (std::max) (0, slot.entry.custom->GetWidthPx    (true, m_scaler, m_textRenderer)
                        - slot.entry.custom->GetMinWidthPx (true, m_scaler, m_textRenderer));
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
    Slot *                              slot = (m_focusIndex >= 0 && m_focusIndex < (int) m_slots.size()) ? &m_slots[(size_t) m_focusIndex] : nullptr;
    std::shared_ptr<const DxuiCommand>  cmd;



    // The slot is tested on its own, ahead of the command it carries. A
    // command reached through the slot is proof enough for a reader that the
    // slot exists, but not for the analyzer, which reads the switch below as
    // a dereference of a pointer that was allowed to be null.
    if (slot == nullptr)
    {
        return;
    }

    cmd = slot->entry.command;

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
            ev.shift = DxuiInputState::IsKeyDown (VK_SHIFT);
            ev.ctrl  = DxuiInputState::IsKeyDown (VK_CONTROL);

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
//  DxuiToolbar::GetSpacingDp
//
//  The strip's spacing: the command bar's, or a tool window's much tighter
//  one while compact.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetSpacingDp (Spacing which) const
{
    switch (which)
    {
        case Spacing::BtnMarginY: return m_compact ? 2 : kBtnMarginYDp;
        case Spacing::BtnPadX:    return m_compact ? 4 : kBtnPadXDp;
        case Spacing::BarPadX:    return m_compact ? 2 : m_barPadDp;
        case Spacing::BtnGap:     return m_compact ? 1 : kBtnGapDp;
        case Spacing::GroupGap:   return m_compact ? 8 : m_groupGapDp;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetBandDp
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetBandDp() const
{
    if (m_bandDp > 0)
    {
        return m_bandDp;
    }

    return m_compact ? kCompactBandDp : kBandDp;
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
        hrMeasure = m_textRenderer->MeasureString (text, fontPx, DxuiTheme::GetUiFace(), w, h);
    }

    if (SUCCEEDED (hrMeasure) && w > 0.0f)
    {
        return (int) (w + 0.5f);
    }

    return (int) ((float) wcslen (text) * kFallbackCharPx * fontPx / kFallbackFontDip);
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
    int           padX    = GetButtonPadPx();
    int           iconGap = (int) std::lround (m_scaler.ToPxf (m_iconGapDip));
    float         fontPx  = GetChromeFontPx();
    int           iconW   = (int) (m_scaler.ToPxf (m_iconDip) + 0.5f);
    int           width   = 0;
    std::wstring  label;



    if (slot.entry.custom != nullptr)
    {
        return (std::max) (slot.entry.custom->GetMinWidthPx (labeled, m_scaler, m_textRenderer),
                           slot.entry.custom->GetWidthPx    (labeled, m_scaler, m_textRenderer) - slot.shrinkPx);
    }

    labeled = labeled || !HasGlyph (slot);
    width   = GetLeadPadPx (labeled) + padX + (HasGlyph (slot) ? iconW : 0);

    if (labeled && slot.entry.command != nullptr)
    {
        label  = GetFittedButtonText (*slot.entry.command, m_textRenderer, fontPx);
        width += (HasGlyph (slot) ? iconGap : 0) + MeasureLabelPx (label.c_str(), fontPx);
    }

    if (slot.entry.kind == Kind::DropDown && (m_chevronOnIcons || !HasGlyph (slot)))
    {
        width += (int) std::lround (m_scaler.ToPxf (m_chevronGapDip)) + m_scaler.ToPx (kChevronDp);
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetButtonText
//
//  A command without a short label falls back to its menu label, which marks
//  the Alt mnemonic with '&'. The menu bar consumes that marker; a toolbar
//  button or its tooltip has no mnemonic, so it is stripped here or it shows
//  as a literal ampersand ("&Full screen").
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiToolbar::GetButtonText (const DxuiCommand & cmd)
{
    std::wstring  stripped;
    int           mnIdx    = -1;
    wchar_t       mnCh     = 0;



    DxuiMenuBar::ParseMnemonic (cmd.GetShortText(), stripped, mnIdx, mnCh);

    return stripped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetFittedButtonText
//
//  The button text shortened to the command's label fit, when it has one.
//  Measuring and painting both take their text from here, so the strip
//  reserves exactly the width of the string it draws. Without a renderer
//  there is nothing to measure the fit against, and the text is left whole.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiToolbar::GetFittedButtonText (
    const DxuiCommand  & cmd,
    IDxuiTextRenderer  * text,
    float                fontPx) const
{
    std::wstring          label    = GetButtonText (cmd);
    const DxuiLabelFit  * fit      = nullptr;
    size_t                keptLen  = 0;
    bool                  isFitted = cmd.labelFit.has_value() && text != nullptr;



    if (!isFitted)
    {
        return label;
    }

    fit = &cmd.labelFit.value();

    if (!fit->keptSuffix.empty() && label.ends_with (fit->keptSuffix))
    {
        keptLen = fit->keptSuffix.size();
    }

    for (const std::wstring & suffix : fit->keptSuffixes)
    {
        if (suffix.size() > keptLen && label.ends_with (suffix))
        {
            keptLen = suffix.size();
        }
    }

    return DxuiTextElide::ToWidth (*text, label, fontPx, DxuiTheme::GetUiFace(),
                                   m_scaler.ToPxf (fit->maxWidthDip), fit->mode, keptLen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetEntryLeadPx
//
//  From an entry's left to its first ink. A button's icon or label starts
//  past the button padding; a custom entry gives its own, or the button
//  padding when its GetLeadPx returns -1.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetEntryLeadPx (const Slot & slot) const
{
    int  lead = -1;



    if (slot.entry.custom != nullptr)
    {
        lead = slot.entry.custom->GetLeadPx (m_scaler);
    }

    if (lead < 0)
    {
        lead = GetButtonPadPx();
    }

    return lead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetPaneInsetOffsetPx
//
//  How far from the bar padding a pane's strip moves its first entry, so
//  that entry's first ink lands exactly on the pane's text inset: further in
//  for an entry whose lead is short, and back toward the edge, past the bar
//  padding if need be, for one whose lead is long, such as a text box, whose
//  text rather than its border sits on the inset. Zero for any other strip,
//  and for one standing up.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetPaneInsetOffsetPx() const
{
    int  barPad = m_scaler.ToPx (GetSpacingDp (Spacing::BarPadX));
    int  inset  = DxuiPaneMetrics::GetContentTextInsetPx (m_scaler);
    int  offset = 0;



    if (!m_paneTextInset || m_vertical)
    {
        return 0;
    }

    //  Trailing entries come last, so the first one shown that is not
    //  trailing is the one at the strip's leading end.
    for (const Slot & slot : m_slots)
    {
        if (slot.hidden || slot.entry.trailing)
        {
            continue;
        }

        offset = inset - GetEntryLeadPx (slot) - barPad;
        break;
    }

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetTotalWidthPx
//
//  Entries in one group sit a button gap apart; a change of group opens the
//  wider group gap instead. A pane's strip also counts the room its first
//  entry moves by, so See more plans against where the entries land.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetTotalWidthPx (int labeledCount) const
{
    int  barPad   = m_scaler.ToPx (GetSpacingDp (Spacing::BarPadX));
    int  btnGap   = m_scaler.ToPx (GetSpacingDp (Spacing::BtnGap));
    int  groupGap = m_scaler.ToPx (GetSpacingDp (Spacing::GroupGap));
    int  width    = barPad * 2 + GetPaneInsetOffsetPx();
    int  index    = 0;



    const Slot *  previous = nullptr;



    for (const Slot & slot : m_slots)
    {
        if (slot.hidden)
        {
            index++;
            continue;
        }

        if (previous != nullptr)
        {
            width += (previous->entry.group != slot.entry.group) ? groupGap : btnGap;
        }

        width   += GetEntryWidthPx (slot, !slot.entry.iconOnly && index < labeledCount);
        previous = &slot;

        index++;
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetChromeFontPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiToolbar::GetChromeFontPx() const
{
    return (m_labelFontDip > 0.0f) ? m_scaler.ToPxf (m_labelFontDip) : m_scaler.ToPxf (DxuiTheme::kBodySizeDip) * m_labelScale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::RefreshMetrics
//
//  Re-reads the chrome font for the scaler's DPI, only when the DPI has
//  changed. The strip asks for the size on every width plan and every
//  paint, and the metrics come from a system-parameters query that has no
//  business running per frame.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::RefreshMetrics()
{
    if (m_metricsDpi == m_scaler.GetDpi())
    {
        return;
    }

    m_metrics    = DxuiMenuMetrics::FromSystem (m_scaler.GetDpi());
    m_metricsDpi = m_scaler.GetDpi();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PlanForWidth
//
//  Moves whole entries into See more FROM THE RIGHT until the strip fits,
//  so the leftmost entries stay longest and nothing is ever pushed off the
//  end. Every entry keeps its label. The band thickness is fixed:
//  everything stays on one row.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::PlanForWidth (int clientWidthPx, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    RefreshMetrics();

    for (Slot & slot : m_slots)
    {
        slot.shrinkPx = 0;
    }

    PlanSeeMore (clientWidthPx);

    m_labeledCount = m_labels ? (int) m_slots.size() : 0;
    GrowToFill (clientWidthPx);

    return GetBandDp();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GrowToFill
//
//  The length the entries leave over goes to the first entry that fills,
//  so it runs to the strip's far end.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::GrowToFill (int clientWidthPx)
{
    //  Standing up, the entries show as icons, so labels take no room.
    int  spare = clientWidthPx - GetTotalWidthPx (m_vertical ? 0 : m_labeledCount);



    if (spare <= 0)
    {
        return;
    }

    for (Slot & slot : m_slots)
    {
        if (!slot.hidden && slot.entry.fill && slot.entry.custom != nullptr)
        {
            slot.shrinkPx -= spare;
            break;
        }
    }
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
    int           marginY         = 0;
    int           btnGap          = 0;
    int           groupGap        = 0;
    int           barPad          = 0;
    int           x               = 0;
    int           top             = 0;
    int           bottom          = 0;
    int           index           = 0;
    int           freeLeft        = 0;
    int           freeRight       = 0;
    const Slot  * previousVisible = nullptr;
    RECT          bounds          = boundsDip;
    int           grip            = m_grip ? scaler.ToPx (kGripDp) : 0;



    //  The grab handle takes the strip's leading end; the entries lay out in
    //  what is left.
    m_gripRect = {};

    if (m_grip && m_vertical)
    {
        m_gripRect  = RECT { bounds.left, bounds.top, bounds.right, bounds.top + grip };
        bounds.top += grip;
    }
    else if (m_grip)
    {
        m_gripRect   = RECT { bounds.left, bounds.top, bounds.left + grip, bounds.bottom };
        bounds.left += grip;
    }

    m_barRect = boundsDip;

    if (m_vertical)
    {
        PlanForWidth   (bounds.bottom - bounds.top, scaler);
        LayoutVertical (bounds);
        SetBounds      (m_barRect);
        return;
    }

    PlanForWidth (bounds.right - bounds.left, scaler);

    marginY  = m_scaler.ToPx (GetSpacingDp (Spacing::BtnMarginY));
    btnGap   = m_scaler.ToPx (GetSpacingDp (Spacing::BtnGap));
    groupGap = m_scaler.ToPx (GetSpacingDp (Spacing::GroupGap));
    barPad   = m_scaler.ToPx (GetSpacingDp (Spacing::BarPadX));
    x        = bounds.left + barPad + GetPaneInsetOffsetPx();
    top      = bounds.top + marginY + (int) std::lround (m_scaler.ToPxf (m_contentDropDip));
    bottom   = bounds.bottom - marginY + (int) std::lround (m_scaler.ToPxf (m_contentDropDip));

    for (Slot & slot : m_slots)
    {
        int  width = 0;

        //  An entry off the strip has nowhere to hang a flyout from.
        if (slot.hidden)
        {
            if (slot.entry.command != nullptr && slot.entry.command->id == m_flyoutId)
            {
                CloseFlyout();
            }

            slot.rc      = RECT {};
            slot.hovered = false;
            slot.pressed = false;
            index++;
            continue;
        }

        if (previousVisible != nullptr)
        {
            x += (previousVisible->entry.group != slot.entry.group) ? groupGap : btnGap;
        }

        slot.labeled    = !slot.entry.iconOnly && (index < m_labeledCount || !HasGlyph (slot));
        width           = GetEntryWidthPx (slot, slot.labeled);
        slot.rc         = RECT { x, top, x + width, bottom };
        x              += width;
        previousVisible = &slot;

        index++;
    }

    PlaceTrailingEntries (bounds.right - barPad);

    freeLeft  = bounds.left + barPad;
    freeRight = bounds.right - barPad;

    for (const Slot & slot : m_slots)
    {
        if (slot.hidden)
        {
            continue;
        }

        if (slot.entry.trailing)
        {
            freeRight = (std::min) (freeRight, (int) slot.rc.left - groupGap);
        }
        else
        {
            freeLeft = (std::max) (freeLeft, (int) slot.rc.right + groupGap);
        }
    }

    m_freeRect = (freeRight > freeLeft) ? RECT { freeLeft, top, freeRight, bottom } : RECT {};

    for (Slot & slot : m_slots)
    {
        if (slot.entry.custom != nullptr)
        {
            slot.entry.custom->Layout (slot.rc, slot.labeled, m_scaler);
        }
    }

    LayoutFlyout();
    SetBounds (m_barRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::LayoutVertical
//
//  Places the entries top to bottom, each as its icon in a cell as tall as
//  the collapsed entry is wide, so the See more plan made for the strip's
//  height holds. An entry with no icon keeps its label, as it has nothing
//  else to show.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::LayoutVertical (const RECT & bounds)
{
    int           margin   = m_scaler.ToPx (GetSpacingDp (Spacing::BtnMarginY));
    int           btnGap   = m_scaler.ToPx (GetSpacingDp (Spacing::BtnGap));
    int           groupGap = m_scaler.ToPx (GetSpacingDp (Spacing::GroupGap));
    int           barPad   = m_scaler.ToPx (GetSpacingDp (Spacing::BarPadX));
    int           y        = bounds.top + barPad;
    const Slot  * previous = nullptr;



    for (Slot & slot : m_slots)
    {
        int  height = 0;

        if (slot.hidden)
        {
            slot.rc      = RECT {};
            slot.hovered = false;
            slot.pressed = false;
            continue;
        }

        if (previous != nullptr)
        {
            y += (previous->entry.group != slot.entry.group) ? groupGap : btnGap;
        }

        slot.labeled = !HasGlyph (slot);
        height       = GetEntryWidthPx (slot, false);
        slot.rc      = RECT { bounds.left + margin, y, bounds.right - margin, y + height };
        y           += height;
        previous     = &slot;

        if (slot.entry.custom != nullptr)
        {
            slot.entry.custom->Layout (slot.rc, slot.labeled, m_scaler);
        }
    }

    m_freeRect = {};

    if (m_flyoutOpen)
    {
        CloseFlyout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetNaturalLengthPx
//
//  Planned against a length nothing overflows, so only the entries that
//  always live in See more are left off, and an entry that fills counts
//  only its own length, not the spare length it would take.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbar::GetNaturalLengthPx (const DxuiDpiScaler & scaler)
{
    int  grip = m_grip ? scaler.ToPx (kGripDp) : 0;



    PlanForWidth (INT_MAX / 4, scaler);

    //  Nothing shrinks at that length, so the only adjustment is the growth.
    for (Slot & slot : m_slots)
    {
        slot.shrinkPx = 0;
    }

    return grip + GetTotalWidthPx ((m_vertical || !m_labels) ? 0 : (int) m_slots.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PlaceTrailingEntries
//
//  Moves the trailing entries right so the last one ends at `rightPx`. They
//  never move left, so a strip too narrow for the gap keeps its order.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PlaceTrailingEntries (int rightPx)
{
    int  shift = 0;



    if (m_slots.empty() || !m_slots.back().entry.trailing)
    {
        return;
    }

    shift = (std::max) (0, rightPx - (int) m_slots.back().rc.right);

    for (Slot & slot : m_slots)
    {
        if (slot.entry.trailing)
        {
            slot.rc.left  += shift;
            slot.rc.right += shift;
        }
    }
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
//  DxuiToolbar::TryGetTip
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::TryGetTip (POINT pointPx, DxuiTip & tip) const
{
    RECT             anchor = {};
    const wchar_t  * text   = nullptr;



    if (!IsVisible())
    {
        return false;
    }

    text = GetTooltipAt (pointPx.x, pointPx.y, anchor);

    if (text == nullptr || *text == L'\0')
    {
        return IDxuiControl::TryGetTip (pointPx, tip);
    }

    tip        = DxuiTip();
    tip.text   = text;
    tip.anchor = anchor;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetTooltipAt
//
//  A collapsed entry has no label on the strip, so its full name surfaces
//  as a tooltip (the host owns the tooltip widget and its dwell timing). An
//  entry whose command carries an explicit tip shows it in every form,
//  since that tip says something the label cannot. A custom entry is asked
//  first, so its own parts can carry tips of their own. A disabled entry
//  shows its tip too, so a grayed button still says what it would do.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiToolbar::GetTooltipAt (int x, int y, RECT & anchor) const
{
    const wchar_t *  tip = nullptr;



    for (const Slot & slot : m_slots)
    {
        const DxuiCommand *  cmd  = slot.entry.command.get();
        bool                 over = false;

        if (tip != nullptr)
        {
            break;
        }

        if (slot.entry.custom != nullptr)
        {
            tip = slot.entry.custom->GetTooltipAt (x, y, anchor);
        }

        over = tip == nullptr && cmd != nullptr && IsPointInRect (slot.rc, x, y);

        if (!over)
        {
            continue;
        }

        m_tipText = cmd->GetTipText();

        if (!m_tipText.empty())
        {
            anchor = slot.rc;
            tip    = m_tipText.c_str();
        }
        else if (!slot.labeled)
        {
            m_tipText = GetButtonText (*cmd);
            anchor    = slot.rc;
            tip       = m_tipText.c_str();
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
        bool  inside  = enabled && IsPointInRect (slot.rc, x, y);

        slot.hovered = inside;

        if (slot.entry.custom != nullptr)
        {
            over = slot.entry.custom->OnMouseMove (x, y) || over;

            if (slot.labeled) { slot.hovered = false; }
        }

        //  Only leaving the entry cancels its press. An expanded custom entry
        //  never shows the hover, and Windows sends moves between a press and
        //  its release even when the pointer has not left it.
        if (!inside) { slot.pressed = false; }
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
    ClearHover();

    if (!m_flyoutPressed && !m_flyoutKeyboard)
    {
        CloseFlyout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::OnMouse
//
//  The host routes pointer input to the toolbar through OnToolbarMouseMove
//  and the calls beside it. A DxuiMouseEventKind::Leave event from the
//  window, sent when the pointer moves off the strip, clears the buttons'
//  hover. An open flyout is closed only by the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        ClearHover();
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::ClearHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::ClearHover()
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



    //  A press with a menu open closes it. On another drop-down button it
    //  also opens that one in the same click, as Explorer's command bar does;
    //  on the open menu's own button it only closes, so the button toggles.
    if (m_dropdown.IsVisible())
    {
        int  open = m_openPicker;

        m_dropdown.Hide();

        for (const Slot & slot : m_slots)
        {
            bool  other = slot.entry.command != nullptr && slot.entry.command->id != open && slot.entry.command->IsEnabled();

            //  See more opens a menu too, and switches the same way.
            if (other && slot.entry.command->id == kSeeMoreId && IsPointInRect (slot.rc, x, y))
            {
                OpenSeeMore();
            }
            else if (other && slot.entry.kind == Kind::DropDown && IsPointInRect (slot.rc, x, y))
            {
                OpenDropDown (slot.entry.command->id);
            }
        }

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
        if (slot.entry.custom != nullptr)
        {
            slot.entry.custom->OnLButtonUp (x, y);
        }
    }

    for (Slot & slot : m_slots)
    {
        std::shared_ptr<const DxuiCommand>  cmd        = slot.entry.command;
        bool                                wasPressed = slot.pressed;
        bool                                consumed   = false;

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
//  DxuiToolbar::OpenSeeMore
//
//  The entries off the strip in their strip order, a drop-down as a submenu
//  of its own rows, then the ones that always live here.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::OpenSeeMore()
{
    const Slot *                    moreSlot = FindSlot (kSeeMoreId);
    std::vector<DxuiPopupMenuItem>  overflow;
    std::vector<DxuiPopupMenuItem>  pinned;
    std::vector<DxuiPopupMenuItem>  rows;



    if (moreSlot == nullptr || m_textRenderer == nullptr)
    {
        return;
    }

    for (const Slot & slot : m_slots)
    {
        std::vector<DxuiPopupMenuItem> &  into   = slot.entry.seeMoreOnly ? pinned : overflow;
        auto                              picker = m_pickers.end();

        if (!slot.hidden || slot.entry.command == nullptr || slot.entry.command->id == kSeeMoreId)
        {
            continue;
        }

        picker = m_pickers.find (slot.entry.command->id);

        if (slot.entry.kind == Kind::DropDown && picker != m_pickers.end())
        {
            into.push_back (DxuiPopupMenuItem::ForSubmenu (slot.entry.command, picker->second.items));
        }
        else
        {
            into.push_back (DxuiPopupMenuItem::ForCommand (slot.entry.command));
        }
    }

    if (m_seeMoreRows)
    {
        rows = m_seeMoreRows();
    }

    //  The host's rows, with no separator next to another.
    for (DxuiPopupMenuItem & item : rows)
    {
        if (overflow.empty() || item.kind != DxuiPopupMenuItem::Kind::Separator || overflow.back().kind != DxuiPopupMenuItem::Kind::Separator)
        {
            overflow.push_back (std::move (item));
        }
    }

    if (!overflow.empty() && !pinned.empty() && overflow.back().kind != DxuiPopupMenuItem::Kind::Separator)
    {
        overflow.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (DxuiPopupMenuItem & item : pinned)
    {
        overflow.push_back (std::move (item));
    }

    if (overflow.empty())
    {
        return;
    }

    RECT  anchor = { moreSlot->rc.left, m_barRect.top, moreSlot->rc.right, m_barRect.bottom };

    m_openPicker = kSeeMoreId;
    m_dropdown.SetOnClickOutside (m_onDropDownClickOutside);
    m_dropdown.ShowUnder (anchor, std::move (overflow), *m_textRenderer, m_hostClient);

    if (!m_dropdown.IsVisible())
    {
        m_openPicker = -1;
    }
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
        const DxuiCommand *  row = it->second.items[i].command.get();

        if (row != nullptr && row->IsChecked())
        {
            it->second.openedOn = (int) i;
        }
    }

    // UNDER THE ENTRY, not at a point. A point-placed popup is never flipped,
    // only slid back onto the monitor, so a window near the bottom of the
    // screen opened its drop-down downward anyway and left it lying against
    // the taskbar. Anchored to the entry, it opens above the bar instead when
    // the room below runs out, the way the menu bar's menus already do.
    RECT  anchor = { slot->rc.left, m_barRect.top, slot->rc.right, m_barRect.bottom };

    m_openPicker = commandId;

    //  A press that dismisses the menu by landing on another drop-down, on
    //  this strip or another in the window, opens that one, as every
    //  application's drop-downs do; one on this menu's own button only
    //  closes it. An owner with somewhere else to send the press says so.
    if (m_onDropDownClickOutside)
    {
        m_dropdown.SetOnClickOutside (m_onDropDownClickOutside);
    }
    else
    {
        m_dropdown.SetOnClickOutside ([this, commandId] (POINT screenPx) { (void) TryOpenDropDownAt (screenPx, this, commandId); });
    }

    m_dropdown.ShowUnder (anchor, it->second.items, *m_textRenderer, m_hostClient);

    if (!m_dropdown.IsVisible())
    {
        m_openPicker = -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::TryOpenDropDownAt
//
//  The drop-down entry of any strip shown in the window under a point in
//  screen pixels, opened; the one just closed (`closing`'s `closedId`) is
//  left closed, so its button toggles.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::TryOpenDropDownAt (POINT screenPx, const DxuiToolbar * closing, int closedId)
{
    HWND  under = DxuiDisplay::Get().WindowFromPoint (screenPx);



    for (DxuiToolbar * strip : s_toolbars)
    {
        DxuiHwndSource  * host  = strip->m_dropdown.GetPopupHost();
        POINT             local = screenPx;

        if (!strip->IsVisible() || host == nullptr || host->GetHwnd() != under)
        {
            continue;
        }

        ScreenToClient (under, &local);

        for (const Slot & slot : strip->m_slots)
        {
            bool  isDropDown = slot.entry.kind == Kind::DropDown && slot.entry.command != nullptr;

            if (!isDropDown || slot.hidden || !slot.entry.command->IsEnabled() || !IsPointInRect (slot.rc, local.x, local.y))
            {
                continue;
            }

            if (strip == closing && slot.entry.command->id == closedId)
            {
                return false;
            }

            strip->OpenDropDown (slot.entry.command->id);
            return strip->IsMenuOpen();
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::Flatten
//
//  `ink` composited over `behind`, so a translucent color becomes the opaque
//  color it would have looked like. Alpha in `behind` is ignored: it is what
//  is already on the surface.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiToolbar::Flatten (uint32_t ink, uint32_t behind)
{
    unsigned  alpha = (ink >> 24) & 0xFFu;
    unsigned  out   = 0xFF000000u;



    if (alpha == 0xFFu)
    {
        return ink;
    }

    for (int shift = 0; shift <= 16; shift += 8)
    {
        unsigned  front = (ink    >> shift) & 0xFFu;
        unsigned  back  = (behind >> shift) & 0xFFu;

        out |= (((front * alpha + back * (255u - alpha) + 127u) / 255u) & 0xFFu) << shift;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintFaceText
//
//  The lines are measured at a reference size and scaled together to the
//  smaller of two sizes: the widest line as wide as the button, or the lines
//  as tall as it. The block of lines is centered.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintFaceText (const Slot & slot, IDxuiTextRenderer & text, float left, float top, float width, float height, uint32_t ink)
{
    HRESULT                    hr       = S_OK;
    std::vector<std::wstring>  lines;
    std::wstring_view          face     = slot.entry.faceText;
    const wchar_t            * font     = (slot.entry.faceFont != nullptr) ? slot.entry.faceFont : kFaceFont;
    float                      margin   = m_scaler.ToPxf (kFillMarginDip);
    float                      widest   = 0.0f;
    float                      lineH    = 0.0f;
    float                      scale    = 0.0f;
    float                      size     = 0.0f;
    float                      y        = 0.0f;
    size_t                     start    = 0;



    while (start <= face.size())
    {
        size_t  end = face.find (L'\n', start);

        end = (end == std::wstring_view::npos) ? face.size() : end;
        lines.emplace_back (face.substr (start, end - start));
        start = end + 1;
    }

    for (const std::wstring & line : lines)
    {
        float  w = 0.0f;
        float  h = 0.0f;

        hr = text.MeasureString (line.c_str(), kFaceReferencePx, font, w, h);
        IGNORE_RETURN_VALUE (hr, S_OK);

        widest = (std::max) (widest, w);
        lineH  = (std::max) (lineH, h);
    }

    if (widest <= 0.0f || lineH <= 0.0f)
    {
        return;
    }

    scale = (std::min) ((width - 2.0f * margin) / widest, (height - 2.0f * margin) / (lineH * (float) lines.size()));
    size  = kFaceReferencePx * scale;
    y     = top + (height - lineH * scale * (float) lines.size()) * 0.5f;

    for (const std::wstring & line : lines)
    {
        hr = text.DrawString (line.c_str(), left, y, width, lineH * scale, ink, size, font,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Top, DxuiFontWeight::SemiBold, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        y += lineH * scale;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintEntryIcon
//
//  One glyph in the icon face, left-aligned in its column and vertically
//  centered on the entry -- or, for an entry with a vector icon, that icon
//  in the same square, its outline in the ink and its accent part in the
//  accent.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintEntryIcon (const Slot & slot, IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiToolbarIconBox & icon, uint32_t ink, uint32_t accent)
{
    HRESULT          hr    = S_OK;
    const wchar_t *  glyph = (slot.entry.command != nullptr) ? slot.entry.command->glyph : nullptr;



    //  A drawn icon stands in for the glyph, in the same box and the same
    //  ink: whatever this path decided about hover, disabling and theme
    //  reaches both kinds of icon or neither.
    if (slot.entry.icon)
    {
        slot.entry.icon (painter, icon);
        return;
    }

    if (slot.entry.vectorIcon != nullptr)
    {
        hr = text.FillVectorIcon (*slot.entry.vectorIcon, icon.x, icon.top + (icon.rowH - icon.size) * 0.5f, icon.size, ink, accent);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    //  The command's own SVG, as Explorer draws its command bar, where the
    //  renderer can draw one; the glyph stands in where it cannot.
    if (slot.entry.command != nullptr && slot.entry.command->menuSvg != nullptr)
    {
        hr = text.DrawSvgIcon (*slot.entry.command->menuSvg, icon.x, icon.top + (icon.rowH - icon.size) * 0.5f, icon.size);

        if (SUCCEEDED (hr))
        {
            return;
        }
    }

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
//  Disabled entries take the theme's own disabled foreground, so a theme
//  that wants a particular dimmed color gets it rather than an alpha the
//  strip chose. A strip with overridden colors keeps its own hue at that
//  color's alpha.
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
    const DxuiCommand *  cmd     = slot.entry.command.get();
    bool                 enabled = cmd != nullptr && cmd->IsEnabled();
    bool                 checked = slot.entry.kind == Kind::Toggle && cmd != nullptr && cmd->IsChecked();
    //  The entry whose menu is open is not lit as hovered, as Explorer's is not.
    bool                menuOpen = m_dropdown.IsVisible() && cmd != nullptr && cmd->id == m_openPicker;
    bool                hovered  = slot.hovered && !menuOpen;
    bool                active   = hovered || slot.pressed || checked;
    float               bl       = (float) slot.rc.left;
    float               bt       = (float) slot.rc.top;
    float               bw       = (float) (slot.rc.right  - slot.rc.left);
    float               bh       = (float) (slot.rc.bottom - slot.rc.top);
    float               fontDip  = GetChromeFontPx();
    float               iconDip  = m_scaler.ToPxf (m_iconDip);
    int                 padX     = GetButtonPadPx();
    int                 iconGap  = (int) std::lround (m_scaler.ToPxf (m_iconGapDip));
    uint32_t            ink      = m_stripColorsSet ? m_textOverride : theme.ButtonText();
    uint32_t            accent   = theme.Accent();
    float               textX    = 0.0f;
    DxuiToolbarIconBox  icon;
    std::wstring        label;



    //  A disabled icon dims both its tones alike, as Explorer's do.
    if (!enabled)
    {
        ink    = (ink    & 0x00FFFFFFu) | kDisabledInkAlpha;
        accent = (accent & 0x00FFFFFFu) | kDisabledInkAlpha;
    }

    if (active && m_flat)
    {
        //  WinUI's subtle fills, and its secondary text while pressed.
        bool  bright = DxuiColor::ComputeRelativeLuminance (ink) > 0.5f;

        painter.FillRoundedRect (bl, bt, bw, bh, m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip),
                                 (slot.pressed || checked) ? theme.SystemButtonPressed() : theme.SystemButtonHover());

        if (slot.pressed)
        {
            ink = (ink & 0x00FFFFFFu) | (bright ? kPressedInkAlphaDark : kPressedInkAlphaLight);
        }
    }
    else if (active)
    {
        uint32_t  fill = (slot.pressed || checked) ? theme.ButtonPressed()
                                                   : (hovered ? theme.ButtonHover() : theme.ButtonIdle());

        painter.FillRoundedRect    (bl, bt, bw, bh, m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip), fill);
        painter.OutlineRoundedRect (bl, bt, bw, bh, m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip), 1.0f, theme.ButtonBorder());
    }

    // The keyboard focus ring sits just outside the entry, as the drive
    // widgets draw theirs, so it never covers the hover chrome. Its radius is
    // the entry's plus the gap between them, which keeps the ring parallel to
    // the rounded entry; the entry's own radius would pinch in at the corners.
    if (&slot == &m_slots[(size_t) (std::max) (m_focusIndex, 0)] && m_focusIndex >= 0)
    {
        float  ring = (float) m_scaler.ToPx (2);
        float  pen  = (float) (std::max) (1, m_scaler.ToPx (1));

        painter.OutlineRoundedRect (bl - ring, bt - ring, bw + ring * 2.0f, bh + ring * 2.0f,
                                    m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip) + ring,
                                    pen, theme.FocusRing());
    }

    if (slot.entry.custom != nullptr)
    {
        slot.entry.custom->Paint (painter, text, theme, slot.hovered, slot.pressed, slot.labeled);
        return;
    }

    // A labeled entry keeps its icon left-padded with the label beside it; a
    // collapsed one centers the icon in what is left.
    icon.x    = slot.labeled ? bl + (float) GetLeadPadPx (true) : bl + (bw - iconDip) * 0.5f;
    icon.top  = bt;
    icon.size = iconDip;
    icon.rowH = bh;

    icon.enabled = enabled;

    if (slot.entry.fillsButton && !slot.labeled)
    {
        icon.size = (std::min) (bw, bh) - 2.0f * m_scaler.ToPxf (kFillMarginDip);
        icon.x    = bl + (bw - icon.size) * 0.5f;
    }

    //  A DECORATION IS GIVEN AN OPAQUE INK. Text draws a partly transparent
    //  color once, but strokes are laid down as overlapping rects, and each
    //  overlap blends again -- the same color would come out solid. So the
    //  ink is flattened against what it will be drawn on first.
    icon.ink  = Flatten (ink, active ? ((slot.pressed || checked) ? theme.ButtonPressed()
                                                                  : (hovered ? theme.ButtonHover() : theme.ButtonIdle()))
                                     : (m_stripColorsSet ? m_stripOverride : theme.Background()));

    if (slot.entry.faceText != nullptr && !slot.labeled)
    {
        PaintFaceText (slot, text, bl, bt, bw, bh, ink);
    }
    else
    {
        PaintEntryIcon (slot, painter, text, icon, ink, accent);
    }

    if (slot.entry.decoration)
    {
        slot.entry.decoration (painter, theme, icon, !slot.labeled);
    }

    if (slot.labeled && cmd != nullptr)
    {
        label = GetFittedButtonText (*cmd, &text, fontDip);

        if (!label.empty())
        {
            textX = bl + (float) GetLeadPadPx (true) + (HasGlyph (slot) ? iconDip + (float) iconGap : 0.0f);

            hr = text.DrawString (label.c_str(), textX, bt,
                                  (float) slot.rc.right - textX, bh,
                                  ink, fontDip, DxuiTheme::GetUiFace(),
                                  DxuiTextHAlign::Left,
                                  DxuiTextVAlign::CenterOnCapHeight);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    //  The font's ChevronDown rather than two drawn lines, which stair-step:
    //  the glyph is smoothed the way the icon beside it is. It is drawn in
    //  the secondary ink File Explorer uses, a step dimmer than the label.
    if (slot.entry.kind == Kind::DropDown && (m_chevronOnIcons || !HasGlyph (slot)))
    {
        float     size      = m_scaler.ToPxf ((float) kChevronDp);
        float     left      = (float) slot.rc.right - (float) padX - size;
        uint32_t  secondary = (ink & 0x00FFFFFFu) | ((((ink >> 24) & 0xFFu) * kChevronAlphaPercent / 100u) << 24);

        hr = text.DrawString (s_kpszMdl2ChevronDown, left, bt, size, bh,
                              secondary, m_scaler.ToPxf (kChevronFontDip), m_iconFace,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintGrip
//
//  Visual Studio's grab handle: a short band of single-pixel dots, centered
//  across the strip's leading end, in the theme's disabled ink faded halfway
//  to the background, or its muted ink while the strip is dragged. The hit
//  area is the whole grip rect.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintGrip (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    float     dot    = 1.0f;
    float     length = (float) (m_vertical ? m_gripRect.right - m_gripRect.left : m_gripRect.bottom - m_gripRect.top);
    float     margin = (std::max) ((float) m_scaler.ToPx (GetSpacingDp (Spacing::BtnMarginY)) + dot, (length - m_scaler.ToPxf (kGripLengthDip)) * 0.5f);
    uint32_t  ink    = m_lifted ? theme.ForegroundMuted() : DxuiColor::Mix (theme.ForegroundDisabled(), theme.Background(), 0.5f);



    if (!m_grip || m_gripRect.right <= m_gripRect.left)
    {
        return;
    }

    for (const GripDot & each : GetGripDots (m_gripRect, m_vertical, dot, margin))
    {
        painter.FillRect (each.x, each.y, each.size, each.size, ink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetGripDots
//
//  Across a horizontal strip's handle the columns run top to bottom; along
//  a vertical strip's they run left to right. The dots repeat every four
//  dots down a column, and the second column starts two dots lower, so the
//  columns alternate.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::GripDot> DxuiToolbar::GetGripDots (const RECT & grip, bool vertical, float dotPx, float marginPx)
{
    std::vector<GripDot>  dots;
    float                 step   = dotPx * 4.0f;
    float                 cx     = std::floor ((float) (grip.left + grip.right) * 0.5f);
    float                 cy     = std::floor ((float) (grip.top  + grip.bottom) * 0.5f);
    float                 first  = vertical ? (float) grip.left  + marginPx : (float) grip.top    + marginPx;
    float                 last   = vertical ? (float) grip.right - marginPx : (float) grip.bottom - marginPx;



    if (dotPx <= 0.0f)
    {
        return dots;
    }

    for (int column = 0; column < 2; column++)
    {
        float  across = (vertical ? cy : cx) - dotPx * 1.5f + dotPx * 2.0f * (float) column;

        for (float along = first + step * 0.5f * (float) column; along + dotPx <= last; along += step)
        {
            dots.push_back (vertical ? GripDot { along, across, dotPx } : GripDot { across, along, dotPx });
        }
    }

    return dots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetLifted
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetLifted (bool lifted, int64_t nowMs, bool isAnimated)
{
    if (lifted == m_lifted)
    {
        return;
    }

    m_lifted    = lifted;
    m_liftSlide = lifted ? DxuiSlide::Start (1.0f, nowMs, isAnimated) : DxuiSlide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetLiftLevel
//
//  0 on its place, 1 fully lifted, and the rise in between.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiToolbar::GetLiftLevel (int64_t nowMs) const
{
    return m_lifted ? 1.0f - m_liftSlide.GetOffset (nowMs) : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::GetCursorForPoint
//
//  The move cursor over the grab handle; elsewhere, whatever cursor a
//  custom entry shows there.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiToolbar::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = nullptr;



    if (IsOnGrip (clientPx.x, clientPx.y))
    {
        return IDC_SIZEALL;
    }

    for (const Slot & slot : m_slots)
    {
        if (slot.hidden || slot.entry.custom == nullptr)
        {
            continue;
        }

        cursor = slot.entry.custom->GetCursorAt (clientPx.x, clientPx.y);

        if (cursor != nullptr)
        {
            break;
        }
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintLift
//
//  The raised surface: a shadow under a card a little larger than the
//  strip, in the theme's raised surface color, with an accent edge. The
//  card grows and the shadow deepens with `level`.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintLift (IDxuiPainter & painter, const IDxuiTheme & theme, float level)
{
    constexpr float  kRadiusDip = 4.0f;



    float  grow   = m_scaler.ToPxf (kLiftGrowDip) * level;
    float  left   = (float) m_barRect.left - grow;
    float  top    = (float) m_barRect.top  - grow;
    float  width  = (float) (m_barRect.right  - m_barRect.left) + grow * 2.0f;
    float  height = (float) (m_barRect.bottom - m_barRect.top)  + grow * 2.0f;
    float  line   = (std::max) (1.0f, m_scaler.ToPxf (1.0f));



    DxuiShadow::Paint (painter, left, top, width, height, m_scaler.ToPxf (kRadiusDip), m_scaler.ToPxf (1.0f) * level, theme.LiftShadow());

    painter.FillRect    (left, top, width, height, theme.BackgroundElevated());
    painter.OutlineRect (left, top, width, height, line, theme.Accent());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::PaintDropSlot
//
//  The band the carried toolbar is in: an accent tint across the band's
//  whole stretch, over any toolbar sharing it, with an accent outline, so a
//  band of its own reads as a whole row and a shared band as the row it is
//  joining.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintDropSlot (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    constexpr uint32_t  kTintAlpha = 0x30u << 24;



    float     left   = (float) m_dropSlot.left;
    float     top    = (float) m_dropSlot.top;
    float     width  = (float) (m_dropSlot.right  - m_dropSlot.left);
    float     height = (float) (m_dropSlot.bottom - m_dropSlot.top);
    float     line   = (std::max) (1.0f, m_scaler.ToPxf (1.0f));
    uint32_t  tint   = (theme.Accent() & 0x00FFFFFFu) | kTintAlpha;



    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    painter.FillRect    (left, top, width, height, tint);
    painter.OutlineRect (left, top, width, height, line, theme.Accent());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::Paint
//
//  A bottom hairline separates the strip from whatever is below it, or, for
//  a strip with end edges, a hairline closes each short end instead; entries
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
    float     lift  = GetLiftLevel ((int64_t) GetTickCount64());



    m_dropdown.SetTheme (&theme);

    if (bw <= 0.0f)
    {
        return;
    }

    if (lift > 0.0f)
    {
        PaintDropSlot (painter, theme);
    }

    painter.FillRect (bl, btTop, bw, bhAll, strip);

    if (lift > 0.0f)
    {
        PaintLift (painter, theme, lift);
    }
    else if (m_endEdges && m_vertical)
    {
        painter.FillRect (bl, btTop,                bw, 1.0f, theme.ContentEdge());
        painter.FillRect (bl, btTop + bhAll - 1.0f, bw, 1.0f, theme.ContentEdge());
    }
    else if (m_endEdges)
    {
        painter.FillRect (bl,             btTop, 1.0f, bhAll, theme.ContentEdge());
        painter.FillRect (bl + bw - 1.0f, btTop, 1.0f, bhAll, theme.ContentEdge());
    }
    else if (!m_vertical && m_bottomEdge)
    {
        painter.FillRect (bl, (float) (m_barRect.bottom - GetEdgePx (m_scaler)), bw, (float) GetEdgePx (m_scaler),
                          (m_edgeOverride != 0) ? m_edgeOverride : theme.ContentEdge());
    }

    PaintGrip (painter, theme);

    for (Slot & slot : m_slots)
    {
        if (!slot.hidden)
        {
            PaintSlot (slot, painter, text, theme);
        }
    }

    if (m_groupSeparators)
    {
        PaintGroupSeparators (painter, theme);
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
//  DxuiToolbar::PaintGroupSeparators
//
//  A line centered in each gap where one group gives way to the next. Only
//  a gap of the ordinary group width gets one: the stretch that pushes a
//  group to the far end of the bar is space, not a division.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::PaintGroupSeparators (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    const Slot  * previous = nullptr;
    int           groupGap = m_scaler.ToPx (GetSpacingDp (Spacing::GroupGap));
    float         top      = (float) m_barRect.top + m_scaler.ToPxf (kSeparatorTopDip);
    float         height   = (float) m_barRect.bottom - m_scaler.ToPxf (kSeparatorBottomDip) - top;
    float         width    = (std::max) (1.0f, std::round (m_scaler.ToPxf (1.0f)));



    for (const Slot & slot : m_slots)
    {
        int   gap     = 0;
        bool  seeMore = slot.entry.command != nullptr && slot.entry.command->id == kSeeMoreId;

        if (slot.hidden)
        {
            continue;
        }

        if (previous != nullptr && previous->entry.group != slot.entry.group && !(seeMore && m_overflowed))
        {
            gap = (int) slot.rc.left - (int) previous->rc.right;

            if (gap > 0 && gap <= groupGap + groupGap / 2)
            {
                painter.FillRect ((float) previous->rc.right + std::floor (((float) gap - width) * 0.5f), top, width, height, theme.Divider());
            }
        }

        previous = &slot;
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

    // The flyout floats over the window like a popup, so it wears the same
    // shadow and overlay radius a popup's card does -- drawn here in the
    // window, since the flyout is not a separate surface.
    DxuiShadow::Paint (painter, fl, ft, fw, fh,
                       m_scaler.ToPxf (DxuiTheme::kOverlayCornerRadiusDip),
                       m_scaler.ToPxf (1.0f));
    painter.FillRoundedRect (fl - 1.0f, ft - 1.0f, fw + 2.0f, fh + 2.0f,
                             m_scaler.ToPxf (DxuiTheme::kOverlayCornerRadiusDip) + 1.0f,
                             theme.ButtonBorder());
    painter.FillRoundedRect (fl, ft, fw, fh,
                             m_scaler.ToPxf (DxuiTheme::kOverlayCornerRadiusDip),
                             strip);

    m_flyoutControl->Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::TryGetEntryRect
//
//  Where an entry is laid out, for a host hanging a menu from it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbar::TryGetEntryRect (int commandId, RECT & outRect) const
{
    const Slot *  slot = FindSlot (commandId);



    if (slot != nullptr)
    {
        outRect = slot->rc;
    }

    return slot != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar::SetEntryVectorIcon
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbar::SetEntryVectorIcon (int commandId, const DxuiVectorIcon * icon)
{
    for (Slot & slot : m_slots)
    {
        if (slot.entry.command != nullptr && slot.entry.command->id == commandId)
        {
            slot.entry.vectorIcon = icon;
        }
    }
}
