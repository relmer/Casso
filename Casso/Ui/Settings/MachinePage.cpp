#include "Pch.h"

#include "MachinePage.h"

#include "../../UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeightDp     = 28;
    constexpr int    s_kLabelWidthDp    = 140;
    constexpr int    s_kCheckWidthDp    = 140;
    constexpr int    s_kDropdownWidthDp = 200;
    constexpr int    s_kSectionGapDp    = 14;
    constexpr int    s_kPagePadDp       = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetMachineList
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetMachineList (std::vector<std::string>  machineIds,
                                  std::vector<std::wstring> displayNames,
                                  int                       activeIndex)
{
    m_machines           = std::move (machineIds);
    m_activeMachineIndex = activeIndex;

    if (displayNames.size() != m_machines.size())
    {
        // Caller mismatch — fall back to ids as labels.
        displayNames.clear();
        for (const std::string & id : m_machines)
        {
            displayNames.emplace_back (id.begin(), id.end());
        }
    }

    m_machineDropdown.SetItems    (displayNames);
    m_machineDropdown.SetSelected (m_activeMachineIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi          = scaler.Dpi();
    int  pad          = scaler.Px (s_kPagePadDp);
    int  rowHeight    = scaler.Px (s_kRowHeightDp);
    int  labelWidth   = scaler.Px (s_kLabelWidthDp);
    int  checkWidth   = scaler.Px (s_kCheckWidthDp);
    int  dropWidth    = scaler.Px (s_kDropdownWidthDp);
    int  sectionGap   = scaler.Px (s_kSectionGapDp);
    int  childIndent  = scaler.Px (18);          // matches DxuiTreeView indent
    int  x            = rect.left + pad;
    int  y            = rect.top  + pad;
    int  controlsX    = x + labelWidth;



    m_machineLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_machineLabel.SetText (L"Machine:");
    m_machineDropdown.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_speedLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_speedLabel.SetText (L"CPU speed:");
    m_speed.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_speed.SetItems ({ L"Authentic (1.023 MHz)",
                        L"2x (2.046 MHz)",
                        std::wstring (L"Maximum (full afterburner ") + s_kpszRocket + L")" });
    y += rowHeight + sectionGap;

    m_wpLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_wpLabel.SetText (L"Write protect:");
    m_writeProtect[0].SetRect (MakeRect (controlsX,                y, checkWidth, rowHeight));
    m_writeProtect[0].SetLabel (L"Drive 1");
    m_writeProtect[1].SetRect (MakeRect (controlsX + checkWidth,   y, checkWidth, rowHeight));
    m_writeProtect[1].SetLabel (L"Drive 2");
    y += rowHeight + sectionGap;

    m_writeModeLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_writeModeLabel.SetText (L"Write mode:");
    m_writeMode.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_writeMode.SetItems ({ L"Buffer and flush", L"Copy on write" });
    y += rowHeight + sectionGap;

    m_audioLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_audioLabel.SetText (L"Drive audio:");
    m_driveAudio.SetRect (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    // Mechanism is a child of Drive audio: indent the label by the
    // same childIndent used elsewhere (matches DxuiTreeView's 18 dp).
    m_mechLabel.SetRect  (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_mechLabel.SetText  (L"Mechanism:");
    m_mechanism.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_mechanism.SetItems ({ L"Shugart", L"Alps" });

    m_machineLabel.SetDpi    (dpi);
    m_speedLabel.SetDpi      (dpi);
    m_wpLabel.SetDpi         (dpi);
    m_writeModeLabel.SetDpi  (dpi);
    m_audioLabel.SetDpi      (dpi);
    m_mechLabel.SetDpi       (dpi);
    m_machineDropdown.SetDpi (dpi);
    m_speed.SetDpi           (dpi);
    m_writeMode.SetDpi       (dpi);
    m_mechanism.SetDpi       (dpi);
    m_driveAudio.SetDpi      (dpi);
    m_writeProtect[0].SetDpi (dpi);
    m_writeProtect[1].SetDpi (dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Rebuild
//
//  Re-sync widget visible state to the underlying SettingsPanelState
//  and wire each widget's OnChange callback back into the state.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Rebuild ()
{
    SettingsPanelState  * state = m_state;



    if (state == nullptr)
    {
        return;
    }

    m_speed.SetSelected     ((int) state->Prefs().speedMode);
    m_writeMode.SetSelected ((int) state->Prefs().writeMode);
    m_mechanism.SetSelected (state->Prefs().floppyMechanism == "alps" ? 1 : 0);
    m_driveAudio.SetChecked (state->Prefs().floppySoundEnabled);
    m_writeProtect[0].SetChecked (state->Prefs().writeProtect[0]);
    m_writeProtect[1].SetChecked (state->Prefs().writeProtect[1]);
    m_machineDropdown.SetSelected (m_activeMachineIndex);
    ApplyMechanismEnabled (state->Prefs().floppySoundEnabled);

    m_machineDropdown.SetSelect ([this] (int idx)
    {
        if (idx >= 0 && idx < (int) m_machines.size())
        {
            m_activeMachineIndex = idx;
            if (m_onMachineSelected)
            {
                m_onMachineSelected (m_machines[(size_t) idx]);
            }
        }
    });
    m_speed.SetSelect        ([state] (int idx) { state->SetSpeedMode ((SettingsSpeedMode) idx); });
    m_writeMode.SetSelect    ([state] (int idx) { state->SetWriteMode ((SettingsWriteMode) idx); });
    m_mechanism.SetSelect    ([state] (int idx) { state->SetMechanism (idx == 1 ? "alps" : "shugart"); });
    m_driveAudio.SetOnChange ([this, state] (bool checked)
    {
        state->SetFloppySound (checked);
        ApplyMechanismEnabled (checked);
    });
    m_writeProtect[0].SetOnChange ([state] (bool checked) { state->SetWriteProtect (0, checked); });
    m_writeProtect[1].SetOnChange ([state] (bool checked) { state->SetWriteProtect (1, checked); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::ApplyMechanismEnabled
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ApplyMechanismEnabled (bool enabled)
{
    constexpr uint32_t  s_kLabelEnabledArgb  = 0xFFE8EEF4;
    constexpr uint32_t  s_kLabelDisabledArgb = 0xFF6A7585;

    m_mechanism.SetEnabled (enabled);
    m_mechLabel.SetColorArgb (enabled ? s_kLabelEnabledArgb : s_kLabelDisabledArgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnLButtonDown (int x, int y)
{
    int   i = 0;

    if (m_machineDropdown.OnLButtonDown (x, y)) { return; }
    if (m_speed.OnLButtonDown      (x, y)) { return; }
    if (m_writeMode.OnLButtonDown  (x, y)) { return; }
    if (m_mechanism.OnLButtonDown  (x, y)) { return; }
    if (m_driveAudio.OnLButtonDown (x, y)) { return; }
    for (i = 0; i < 2; ++i)
    {
        if (m_writeProtect[(size_t) i].OnLButtonDown (x, y)) { return; }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnLButtonUp (int x, int y)
{
    int  i = 0;

    (void) m_machineDropdown.OnLButtonUp (x, y);
    (void) m_speed.OnLButtonUp     (x, y);
    (void) m_writeMode.OnLButtonUp (x, y);
    (void) m_mechanism.OnLButtonUp (x, y);
    (void) m_driveAudio.OnLButtonUp (x, y);
    for (i = 0; i < 2; ++i)
    {
        (void) m_writeProtect[(size_t) i].OnLButtonUp (x, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnMouseHover (int x, int y)
{
    int  i = 0;

    m_machineDropdown.SetMouseHover (x, y);
    m_speed.SetMouseHover     (x, y);
    m_writeMode.SetMouseHover (x, y);
    m_mechanism.SetMouseHover (x, y);
    m_driveAudio.SetMouseHover (x, y);
    for (i = 0; i < 2; ++i)
    {
        m_writeProtect[(size_t) i].SetMouseHover (x, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool MachinePage::OnKey (WPARAM vk)
{
    int  i = 0;

    if (m_machineDropdown.HandleKey (vk)) { return true; }
    if (m_speed.HandleKey      (vk)) { return true; }
    if (m_writeMode.HandleKey  (vk)) { return true; }
    if (m_mechanism.HandleKey  (vk)) { return true; }
    if (m_driveAudio.OnKey     (vk)) { return true; }
    for (i = 0; i < 2; ++i)
    {
        if (m_writeProtect[(size_t) i].OnKey (vk)) { return true; }
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::CollectFocusables
//
//  Pushes one focus-setter lambda per focusable widget on the page,
//  in visual tab order. The mechanism dropdown is included whether or
//  not drive audio is enabled; the dropdown ignores keyboard activation
//  while disabled.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::CollectFocusables (std::vector<std::function<void (bool)>> & out)
{
    out.push_back ([this] (bool f) { m_machineDropdown.SetFocused (f); });
    out.push_back ([this] (bool f) { m_speed.SetFocused           (f); });
    out.push_back ([this] (bool f) { m_writeProtect[0].SetFocused (f); });
    out.push_back ([this] (bool f) { m_writeProtect[1].SetFocused (f); });
    out.push_back ([this] (bool f) { m_writeMode.SetFocused       (f); });
    out.push_back ([this] (bool f) { m_driveAudio.SetFocused      (f); });
    out.push_back ([this] (bool f) { m_mechanism.SetFocused       (f); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::AnyDropdownOpen
//
////////////////////////////////////////////////////////////////////////////////

bool MachinePage::AnyDropdownOpen () const
{
    return m_machineDropdown.IsOpen() ||
           m_speed.IsOpen()           ||
           m_writeMode.IsOpen()       ||
           m_mechanism.IsOpen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::Paint (DxuiPainter & painter, DxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    int  i = 0;


    UNREFERENCED_PARAMETER (theme);

    m_machineLabel.Paint   (painter, text);
    m_speedLabel.Paint     (painter, text);
    m_wpLabel.Paint        (painter, text);
    m_writeModeLabel.Paint (painter, text);
    m_audioLabel.Paint     (painter, text);
    m_mechLabel.Paint      (painter, text);

    // DxuiDropdown base boxes first; their open menus paint last so they
    // sit on top of any sibling controls below them.
    m_machineDropdown.PaintBase (painter, text);
    m_speed.PaintBase           (painter, text);
    m_writeMode.PaintBase       (painter, text);
    m_mechanism.PaintBase       (painter, text);
    m_driveAudio.Paint          (painter, text);
    for (i = 0; i < 2; ++i)
    {
        m_writeProtect[(size_t) i].Paint (painter, text);
    }

    m_machineDropdown.PaintMenu (painter, text);
    m_speed.PaintMenu           (painter, text);
    m_writeMode.PaintMenu       (painter, text);
    m_mechanism.PaintMenu       (painter, text);
}
