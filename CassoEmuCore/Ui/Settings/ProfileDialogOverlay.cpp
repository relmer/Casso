#include "Pch.h"

#include "ProfileDialogOverlay.h"

#include "Ui/Settings/ControllersPageState.h"





// Layout metrics (DIP), matching the color picker's dialog.
static constexpr int    s_kDialogWidthDp   = 460;
static constexpr int    s_kPadDp           = 18;
static constexpr int    s_kRowHeightDp     = 28;
static constexpr int    s_kLineHeightDp    = 20;
static constexpr int    s_kRowGapDp        = 10;
static constexpr int    s_kLabelWidthDp    = 90;
static constexpr int    s_kButtonWidthDp   = 96;
static constexpr int    s_kButtonGapDp     = 12;
static constexpr int    s_kSourceCount     = 3;
static constexpr int    s_kErrorLineCount  = 2;
static constexpr size_t s_kNameMaxLength   = 80;
static constexpr int    s_kKeyDownMask     = 0x8000;
static constexpr float  s_kTitleFontDip    = 15.0f;
static constexpr float  s_kBorderDip       = 1.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT ProfileDialogOverlay::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenNew
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::OpenNew (const std::wstring & currentName, AcceptFn onAccept)
{
    Open (Kind::NewProfile, currentName, std::move (onAccept), nullptr);
    m_name.SetText (L"");
    m_source.SetSelected (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenRename
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::OpenRename (const std::wstring & currentName, AcceptFn onAccept)
{
    Open (Kind::RenameProfile, currentName, std::move (onAccept), nullptr);
    m_name.SetText (currentName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenConfirmDelete
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::OpenConfirmDelete (const std::wstring & name, AcceptFn onAccept)
{
    Open (Kind::ConfirmDelete, name, std::move (onAccept), nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenKeepOrDiscard
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::OpenKeepOrDiscard (const std::wstring & name, AcceptFn onKeep, DeclineFn onDiscard)
{
    Open (Kind::KeepOrDiscard, name, std::move (onKeep), std::move (onDiscard));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Open
//
//  A dialog that takes a name opens with the field focused; the others open
//  on their primary button, so Enter answers them.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::Open (Kind kind, const std::wstring & name, AcceptFn onAccept, DeclineFn onDecline)
{
    m_kind      = kind;
    m_subject   = name;
    m_onAccept  = std::move (onAccept);
    m_onDecline = std::move (onDecline);
    m_open      = true;
    m_focus     = HasNameField() ? Focus::Name : Focus::Primary;

    m_errorLabel.SetText (L"");
    m_errorRule.SetText  (L"");

    if (m_hasLayout)
    {
        Layout (m_panelRect, m_scaler);
    }

    ApplyFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasNameField
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileDialogOverlay::HasNameField() const
{
    return m_kind == Kind::NewProfile || m_kind == Kind::RenameProfile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Centered in the sheet. From the top: the title, then either the name field
//  (with Start from for New) and room for a two-line error, or the prompt's
//  second line; the buttons along the bottom right.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::Layout (const RECT & panelRect, const DxuiDpiScaler & scaler)
{
    int                           dialogW  = scaler.ToPx (s_kDialogWidthDp);
    int                           pad      = scaler.ToPx (s_kPadDp);
    int                           rowH     = scaler.ToPx (s_kRowHeightDp);
    int                           lineH    = scaler.ToPx (s_kLineHeightDp);
    int                           rowGap   = scaler.ToPx (s_kRowGapDp);
    int                           labelW   = scaler.ToPx (s_kLabelWidthDp);
    int                           btnW     = scaler.ToPx (s_kButtonWidthDp);
    int                           btnGap   = scaler.ToPx (s_kButtonGapDp);
    int                           innerW   = dialogW - pad * 2;
    int                           contentH = 0;
    int                           dialogH  = 0;
    int                           left     = 0;
    int                           top      = 0;
    int                           x        = 0;
    int                           y        = 0;
    int                           by       = 0;
    int                           bx       = 0;
    int                           i        = 0;
    UINT                          dpi      = scaler.GetDpi();
    std::vector<DxuiRadioOption>  options;



    m_scaler    = scaler;
    m_panelRect = panelRect;
    m_hasLayout = true;

    if (HasNameField())
    {
        contentH = rowH + rowGap + lineH * s_kErrorLineCount;

        if (m_kind == Kind::NewProfile)
        {
            contentH += rowGap + rowH * s_kSourceCount;
        }
    }
    else if (m_kind == Kind::KeepOrDiscard)
    {
        contentH = lineH;
    }

    dialogH = pad + rowH + rowGap + contentH + rowGap + rowH + pad;
    left    = panelRect.left + (panelRect.right  - panelRect.left - dialogW) / 2;
    top     = panelRect.top  + (panelRect.bottom - panelRect.top  - dialogH) / 2;
    x       = left + pad;
    y       = top  + pad;
    by      = top + dialogH - pad - rowH;
    bx      = left + dialogW - pad - btnW;

    m_dialogRect = MakeRect (left, top, dialogW, dialogH);

    switch (m_kind)
    {
        case Kind::NewProfile:    m_title.SetText (L"New profile");                                break;
        case Kind::RenameProfile: m_title.SetText (L"Rename profile");                             break;
        case Kind::ConfirmDelete: m_title.SetText (L"Delete the profile \"" + m_subject + L"\"?");  break;
        default:                  m_title.SetText (L"Keep your changes to \"" + m_subject + L"\"?"); break;
    }

    m_title.SetRect        (MakeRect (x, y, innerW, rowH));
    m_title.SetFontSizeDip (s_kTitleFontDip);
    m_title.SetFontWeight  (DxuiFontWeight::SemiBold);
    m_title.SetTextRole    (DxuiTextRole::Heading);
    y += rowH + rowGap;

    m_message.SetRect     (MakeRect (x, y, innerW, lineH));
    m_message.SetText     (L"Your changes will be saved when you click OK in Settings.");
    m_message.SetTextRole (DxuiTextRole::Body);

    m_nameLabel.SetRect     (MakeRect (x, y, labelW, rowH));
    m_nameLabel.SetText     (L"Name:");
    m_nameLabel.SetTextRole (DxuiTextRole::Body);
    m_name.SetRect          (MakeRect (x + labelW, y, innerW - labelW, rowH));
    m_name.SetMaxLength     (s_kNameMaxLength);
    y += rowH;

    m_errorLabel.SetRect (MakeRect (x + labelW, y, innerW - labelW, lineH));
    m_errorRule.SetRect  (MakeRect (x + labelW, y + lineH, innerW - labelW, lineH));
    y += lineH * s_kErrorLineCount + rowGap;

    m_sourceLabel.SetRect     (MakeRect (x, y, labelW, rowH));
    m_sourceLabel.SetText     (L"Start from:");
    m_sourceLabel.SetTextRole (DxuiTextRole::Body);

    options.push_back ({ {}, L"Default mapping" });
    options.push_back ({ {}, L"Copy of \"" + m_subject + L"\"" });
    options.push_back ({ {}, L"Paddles" });

    for (i = 0; i < s_kSourceCount; i++)
    {
        options[(size_t) i].rect = MakeRect (x + labelW, y + i * rowH, innerW - labelW, rowH);
    }

    m_source.SetOptions (std::move (options));
    m_source.Layout     (MakeRect (x + labelW, y, innerW - labelW, rowH * s_kSourceCount), scaler);

    m_secondary.Layout   (MakeRect (bx, by, btnW, rowH));
    m_primary.Layout     (MakeRect (bx - btnGap - btnW, by, btnW, rowH));
    m_primary.SetVariant (DxuiButton::Variant::Primary);

    switch (m_kind)
    {
        case Kind::ConfirmDelete: m_primary.SetLabel (L"Delete"); m_secondary.SetLabel (L"Cancel");  break;
        case Kind::KeepOrDiscard: m_primary.SetLabel (L"Keep");   m_secondary.SetLabel (L"Discard"); break;
        default:                  m_primary.SetLabel (L"OK");     m_secondary.SetLabel (L"Cancel");  break;
    }

    m_primary.SetOnClick   ([this] { Accept(); });
    m_secondary.SetOnClick ([this] { Decline(); });

    m_title.SetDpi       (dpi);
    m_message.SetDpi     (dpi);
    m_nameLabel.SetDpi   (dpi);
    m_name.SetDpi        (dpi);
    m_sourceLabel.SetDpi (dpi);
    m_source.SetDpi      (dpi);
    m_errorLabel.SetDpi  (dpi);
    m_errorRule.SetDpi   (dpi);
    m_primary.SetDpi     (dpi);
    m_secondary.SetDpi   (dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Accept
//
//  The callback is taken out before it runs, so a callback that opens another
//  dialog is not closed by this one finishing.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::Accept()
{
    AcceptFn           onAccept = m_onAccept;
    ProfileSource      source   = ProfileSource::DefaultMapping;
    ProfileEditResult  result   = ProfileEditResult::Ok;
    std::wstring       label;
    std::wstring       rule;



    if (!m_open)
    {
        return;
    }

    switch (m_source.GetSelected())
    {
        case 1:  source = ProfileSource::CopyOfProfile; break;
        case 2:  source = ProfileSource::Paddles;       break;
        default: source = ProfileSource::DefaultMapping; break;
    }

    m_open = false;

    if (onAccept)
    {
        result = onAccept (HasNameField() ? m_name.GetText() : m_subject, source);
    }

    if (ControllersPageState::TryDescribeNameError (result, label, rule))
    {
        m_open  = true;
        m_focus = Focus::Name;
        m_errorLabel.SetText (label);
        m_errorRule.SetText  (rule);
        ApplyFocus();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Decline
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::Decline()
{
    DeclineFn  onDecline = m_onDecline;



    if (!m_open)
    {
        return;
    }

    m_open = false;

    if (onDecline)
    {
        onDecline();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFocusOrder
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ProfileDialogOverlay::Focus> ProfileDialogOverlay::GetFocusOrder() const
{
    std::vector<Focus>  order;



    if (HasNameField())
    {
        order.push_back (Focus::Name);
    }

    if (m_kind == Kind::NewProfile)
    {
        order.push_back (Focus::Source);
    }

    order.push_back (Focus::Primary);
    order.push_back (Focus::Secondary);

    return order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveFocus
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::MoveFocus (int delta)
{
    std::vector<Focus>  order   = GetFocusOrder();
    int                 count   = (int) order.size();
    int                 current = 0;
    int                 i       = 0;



    for (i = 0; i < count; i++)
    {
        if (order[(size_t) i] == m_focus)
        {
            current = i;
        }
    }

    m_focus = order[(size_t) (((current + delta) % count + count) % count)];
    ApplyFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyFocus
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::ApplyFocus()
{
    m_name.SetFocused      (m_focus == Focus::Name);
    m_source.SetFocused    (m_focus == Focus::Source);
    m_primary.SetFocused   (m_focus == Focus::Primary);
    m_secondary.SetFocused (m_focus == Focus::Secondary);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::OnLButtonDown (int x, int y)
{
    if (HasNameField() && m_name.OnLButtonDown (x, y))
    {
        m_focus = Focus::Name;
    }
    else if (m_kind == Kind::NewProfile && m_source.OnLButtonDown (x, y))
    {
        m_focus = Focus::Source;
    }
    else if (m_primary.HitTest (x, y))
    {
        m_focus = Focus::Primary;
        m_primary.SetMouse (x, y, true);
    }
    else if (m_secondary.HitTest (x, y))
    {
        m_focus = Focus::Secondary;
        m_secondary.SetMouse (x, y, true);
    }

    ApplyFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::OnLButtonUp (int x, int y)
{
    if (HasNameField())
    {
        (void) m_name.OnLButtonUp (x, y);
    }

    if (m_kind == Kind::NewProfile)
    {
        (void) m_source.OnLButtonUp (x, y);
    }

    if (m_primary.HitTest (x, y))
    {
        m_primary.Click();
    }
    else if (m_secondary.HitTest (x, y))
    {
        m_secondary.Click();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::OnMouseMove (int x, int y)
{
    if (HasNameField())
    {
        m_name.OnMouseMove   (x, y);
        m_name.SetMouseHover (x, y);
    }

    m_source.SetMouseHover (x, y);
    m_primary.SetMouse     (x, y, false);
    m_secondary.SetMouse   (x, y, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  Tab and Shift+Tab move through the controls. Enter runs the focused
//  button, or the primary one from anywhere else. Escape cancels, except on
//  the keep-or-discard prompt, where it keeps: the choice that loses nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileDialogOverlay::OnKey (WPARAM vk)
{
    bool  shiftDown = (GetKeyState (VK_SHIFT) & s_kKeyDownMask) != 0;



    if (vk == VK_ESCAPE)
    {
        if (m_kind == Kind::KeepOrDiscard)
        {
            Accept();
        }
        else
        {
            Decline();
        }
    }
    else if (vk == VK_RETURN)
    {
        if (m_focus == Focus::Secondary)
        {
            Decline();
        }
        else
        {
            Accept();
        }
    }
    else if (vk == VK_TAB)
    {
        MoveFocus (shiftDown ? -1 : 1);
    }
    else
    {
        switch (m_focus)
        {
            case Focus::Name:      (void) m_name.OnKey (vk);      break;
            case Focus::Source:    (void) m_source.OnKey (vk);    break;
            case Focus::Primary:   (void) m_primary.OnKey (vk);   break;
            case Focus::Secondary: (void) m_secondary.OnKey (vk); break;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileDialogOverlay::OnChar (wchar_t ch)
{
    if (HasNameField() && m_focus == Focus::Name)
    {
        (void) m_name.OnChar (ch);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  The sheet is dimmed under the dialog, as it is under the capture prompt,
//  so the page reads as unavailable while the dialog is open.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileDialogOverlay::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr uint32_t  kDimArgb = 0x80000000u;
    float               borderPx = m_scaler.ToPxf (s_kBorderDip);
    float               dl       = (float) m_dialogRect.left;
    float               dt       = (float) m_dialogRect.top;
    float               dw       = (float) (m_dialogRect.right  - m_dialogRect.left);
    float               dh       = (float) (m_dialogRect.bottom - m_dialogRect.top);



    if (!m_open)
    {
        return;
    }

    painter.FillRect    ((float) m_panelRect.left, (float) m_panelRect.top,
                         (float) (m_panelRect.right - m_panelRect.left), (float) (m_panelRect.bottom - m_panelRect.top), kDimArgb);
    painter.FillRect    (dl, dt, dw, dh, theme.BackgroundElevated());
    painter.OutlineRect (dl, dt, dw, dh, borderPx, theme.ButtonBorder());

    m_title.Paint (painter, text, theme);

    if (m_kind == Kind::KeepOrDiscard)
    {
        m_message.Paint (painter, text, theme);
    }

    if (HasNameField())
    {
        m_nameLabel.Paint (painter, text, theme);
        m_name.SetTheme   (&theme);
        m_name.Paint      (painter, text);

        m_errorLabel.SetColor (theme.ErrorForeground());
        m_errorRule.SetColor  (theme.ErrorForeground());
        m_errorLabel.Paint    (painter, text, theme);
        m_errorRule.Paint     (painter, text, theme);
    }

    if (m_kind == Kind::NewProfile)
    {
        m_sourceLabel.Paint (painter, text, theme);
        m_source.Paint      (painter, text, theme);
    }

    m_primary.Paint   (painter, text, theme);
    m_secondary.Paint (painter, text, theme);
}





