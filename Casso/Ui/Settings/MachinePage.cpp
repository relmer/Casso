#include "Pch.h"

#include "MachinePage.h"

#include "../../UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-local layout constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int    s_kRowHeightDp     = 28;
static constexpr int    s_kLabelWidthDp    = 140;
static constexpr int    s_kCheckWidthDp    = 140;
static constexpr int    s_kDropdownWidthDp = 200;
static constexpr int    s_kSectionGapDp    = 14;
static constexpr int    s_kPagePadDp       = 16;
static constexpr int    s_kPlayGapDp       = 8;
static constexpr int    s_kResetWidthDp    = 130;




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT MachinePage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };

    return rc;
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

void MachinePage::Layout (const RECT & rect, const DpiScaler & scaler)
{
    UINT dpi          = scaler.Dpi();
    int  pad          = scaler.Px (s_kPagePadDp);
    int  rowHeight    = scaler.Px (s_kRowHeightDp);
    int  labelWidth   = scaler.Px (s_kLabelWidthDp);
    int  checkWidth   = scaler.Px (s_kCheckWidthDp);
    int  dropWidth    = scaler.Px (s_kDropdownWidthDp);
    int  sectionGap   = scaler.Px (s_kSectionGapDp);
    int  childIndent  = scaler.Px (18);          // matches TreeView indent
    int  x            = rect.left + pad;
    int  y            = rect.top  + pad;
    int  controlsX    = x + labelWidth;
    int  playSize     = rowHeight;
    int  playX        = controlsX + dropWidth + scaler.Px (s_kPlayGapDp);
    int  resetW       = scaler.Px (s_kResetWidthDp);



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
    // same childIndent used elsewhere (matches TreeView's 18 dp).
    m_mechLabel.SetRect  (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_mechLabel.SetText  (L"Mechanism:");
    m_mechanism.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_mechanism.SetItems ({ L"Shugart", L"Alps" });
    y += rowHeight + sectionGap;

    // Per-sound volume sliders, also children of Drive audio. Each gets a
    // play button to its right that auditions the sound at the dialed level.
    m_motorLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_motorLabel.SetText (L"Motor volume:");
    ConfigureVolumeSlider (m_motorVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_motorPlay.SetRect  (MakeRect (playX, y, playSize, rowHeight));
    m_motorPlay.SetGlyph (s_kpszMdl2Play);
    y += rowHeight + sectionGap;

    m_headLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_headLabel.SetText (L"Head volume:");
    ConfigureVolumeSlider (m_headVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_headPlay.SetRect  (MakeRect (playX, y, playSize, rowHeight));
    m_headPlay.SetGlyph (s_kpszMdl2Play);
    y += rowHeight + sectionGap;

    m_doorLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_doorLabel.SetText (L"Door volume:");
    ConfigureVolumeSlider (m_doorVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_doorPlay.SetRect  (MakeRect (playX, y, playSize, rowHeight));
    m_doorPlay.SetGlyph (s_kpszMdl2Play);
    y += rowHeight + sectionGap;

    m_panOneLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_panOneLabel.SetText (L"Drive 1 pan:");
    ConfigurePanSlider (m_panOne, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_panOnePlay.SetRect  (MakeRect (playX, y, playSize, rowHeight));
    m_panOnePlay.SetGlyph (s_kpszMdl2Play);
    y += rowHeight + sectionGap;

    m_panTwoLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_panTwoLabel.SetText (L"Drive 2 pan:");
    ConfigurePanSlider (m_panTwo, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_panTwoPlay.SetRect  (MakeRect (playX, y, playSize, rowHeight));
    m_panTwoPlay.SetGlyph (s_kpszMdl2Play);
    y += rowHeight + sectionGap;

    // "Restore defaults" for the drive-audio tuning, on its own row,
    // left-aligned with the sliders above it.
    m_reset.Layout   (MakeRect (controlsX, y, resetW, rowHeight));
    m_reset.SetLabel (L"Restore defaults");

    m_machineLabel.SetDpi    (dpi);
    m_speedLabel.SetDpi      (dpi);
    m_wpLabel.SetDpi         (dpi);
    m_writeModeLabel.SetDpi  (dpi);
    m_audioLabel.SetDpi      (dpi);
    m_mechLabel.SetDpi       (dpi);
    m_motorLabel.SetDpi      (dpi);
    m_headLabel.SetDpi       (dpi);
    m_doorLabel.SetDpi       (dpi);
    m_panOneLabel.SetDpi     (dpi);
    m_panTwoLabel.SetDpi     (dpi);
    m_machineDropdown.SetDpi (dpi);
    m_speed.SetDpi           (dpi);
    m_writeMode.SetDpi       (dpi);
    m_mechanism.SetDpi       (dpi);
    m_driveAudio.SetDpi      (dpi);
    m_motorVol.SetDpi        (dpi);
    m_headVol.SetDpi         (dpi);
    m_doorVol.SetDpi         (dpi);
    m_panOne.SetDpi          (dpi);
    m_panTwo.SetDpi          (dpi);
    m_motorPlay.SetDpi       (dpi);
    m_headPlay.SetDpi        (dpi);
    m_doorPlay.SetDpi        (dpi);
    m_panOnePlay.SetDpi      (dpi);
    m_panTwoPlay.SetDpi      (dpi);
    m_reset.SetDpi           (dpi);
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
    m_motorVol.SetValue     (state->Prefs().driveMotorVolume * 100.0f);
    m_headVol.SetValue      (state->Prefs().driveHeadVolume  * 100.0f);
    m_doorVol.SetValue      (state->Prefs().driveDoorVolume  * 100.0f);
    m_panOne.SetValue       (state->Prefs().driveOnePan * 100.0f);
    m_panTwo.SetValue       (state->Prefs().driveTwoPan * 100.0f);
    m_writeProtect[0].SetChecked (state->Prefs().writeProtect[0]);
    m_writeProtect[1].SetChecked (state->Prefs().writeProtect[1]);
    m_machineDropdown.SetSelected (m_activeMachineIndex);
    ApplyDriveAudioChildEnabled (state->Prefs().floppySoundEnabled);

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
        ApplyDriveAudioChildEnabled (checked);
    });
    m_motorVol.SetOnChange ([state] (float v) { state->SetDriveMotorVolume (v / 100.0f); });
    m_headVol.SetOnChange  ([state] (float v) { state->SetDriveHeadVolume  (v / 100.0f); });
    m_doorVol.SetOnChange  ([state] (float v) { state->SetDriveDoorVolume  (v / 100.0f); });
    m_panOne.SetOnChange   ([state] (float v) { state->SetDriveOnePan (v / 100.0f); });
    m_panTwo.SetOnChange   ([state] (float v) { state->SetDriveTwoPan (v / 100.0f); });
    m_writeProtect[0].SetOnChange ([state] (bool checked) { state->SetWriteProtect (0, checked); });
    m_writeProtect[1].SetOnChange ([state] (bool checked) { state->SetWriteProtect (1, checked); });

    // Play buttons audition a sound through the host. The volume buttons
    // play balanced at the midpoint (centered); the pan buttons play a
    // head step on the matching drive at its dialed pan so the stereo
    // position is audible.
    m_motorPlay.SetClick  ([this] { if (m_onTestSound) { m_onTestSound (0, 0, true);  } });
    m_headPlay.SetClick   ([this] { if (m_onTestSound) { m_onTestSound (0, 1, true);  } });
    m_doorPlay.SetClick   ([this] { if (m_onTestSound) { m_onTestSound (0, 2, true);  } });
    m_panOnePlay.SetClick ([this] { if (m_onTestSound) { m_onTestSound (0, 1, false); } });
    m_panTwoPlay.SetClick ([this] { if (m_onTestSound) { m_onTestSound (1, 1, false); } });
    m_reset.SetClick      ([this] { ResetDriveAudioToDefaults(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::ConfigureVolumeSlider
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ConfigureVolumeSlider (Slider & slider, const RECT & rect)
{
    constexpr float  s_kVolumeMax = 100.0f;



    slider.SetRect      (rect);
    slider.SetRange     (0.0f, s_kVolumeMax);
    slider.SetStep      (1.0f);
    slider.SetSuffix    (L"%");
    slider.SetDecimalPlaces (0);
    slider.SetShowTicks (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::ConfigurePanSlider
//
//  Bipolar Left..Center..Right pan slider. Range -100 (hard left) .. +100
//  (hard right), centered detent at 0. The readout names the position
//  ("Left" / "Center" / "Right") and the fill grows from the track center.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ConfigurePanSlider (Slider & slider, const RECT & rect)
{
    constexpr float  s_kPanMax = 100.0f;



    slider.SetRect      (rect);
    slider.SetRange     (-s_kPanMax, s_kPanMax);
    slider.SetStep      (5.0f);
    slider.SetShowTicks (false);
    slider.SetCenterOriginFill (true);
    slider.SetValueFormatter ([] (float v) -> std::wstring
    {
        std::wstring  result;
        int           pct = (int) std::lround (v);

        if (pct == 0)
        {
            result = L"Center";
        }
        else if (pct < 0)
        {
            result = std::to_wstring (-pct) + L"% L";
        }
        else
        {
            result = std::to_wstring (pct) + L"% R";
        }

        return result;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::ApplyDriveAudioChildEnabled
//
//  Enables / disables every control nested under "Drive audio" (mechanism
//  dropdown and the three volume sliders) plus their labels, following the
//  drive-audio master toggle.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ApplyDriveAudioChildEnabled (bool enabled)
{
    constexpr uint32_t  s_kLabelEnabledArgb  = 0xFFE8EEF4;
    constexpr uint32_t  s_kLabelDisabledArgb = 0xFF6A7585;

    uint32_t  labelArgb = enabled ? s_kLabelEnabledArgb : s_kLabelDisabledArgb;



    m_mechanism.SetEnabled (enabled);
    m_motorVol.SetEnabled  (enabled);
    m_headVol.SetEnabled   (enabled);
    m_doorVol.SetEnabled   (enabled);
    m_panOne.SetEnabled    (enabled);
    m_panTwo.SetEnabled    (enabled);
    m_motorPlay.SetEnabled (enabled);
    m_headPlay.SetEnabled  (enabled);
    m_doorPlay.SetEnabled  (enabled);
    m_panOnePlay.SetEnabled (enabled);
    m_panTwoPlay.SetEnabled (enabled);
    m_reset.SetEnabled     (enabled);
    m_mechLabel.SetColorArgb  (labelArgb);
    m_motorLabel.SetColorArgb (labelArgb);
    m_headLabel.SetColorArgb  (labelArgb);
    m_doorLabel.SetColorArgb  (labelArgb);
    m_panOneLabel.SetColorArgb (labelArgb);
    m_panTwoLabel.SetColorArgb (labelArgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::ResetDriveAudioToDefaults
//
//  Restores the per-sound volumes and per-drive pans to their defaults
//  (both the underlying state and the visible sliders). Changes apply to
//  the engine on OK, like every other control on the page.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ResetDriveAudioToDefaults ()
{
    HRESULT  hr = S_OK;



    CBRA (m_state != nullptr);

    m_state->SetDriveMotorVolume (SettingsUiPrefs::kDefaultDriveMotorVolume);
    m_state->SetDriveHeadVolume  (SettingsUiPrefs::kDefaultDriveHeadVolume);
    m_state->SetDriveDoorVolume  (SettingsUiPrefs::kDefaultDriveDoorVolume);
    m_state->SetDriveOnePan      (SettingsUiPrefs::kDefaultDriveOnePan);
    m_state->SetDriveTwoPan      (SettingsUiPrefs::kDefaultDriveTwoPan);

    m_motorVol.SetValue (SettingsUiPrefs::kDefaultDriveMotorVolume * 100.0f);
    m_headVol.SetValue  (SettingsUiPrefs::kDefaultDriveHeadVolume  * 100.0f);
    m_doorVol.SetValue  (SettingsUiPrefs::kDefaultDriveDoorVolume  * 100.0f);
    m_panOne.SetValue   (SettingsUiPrefs::kDefaultDriveOnePan * 100.0f);
    m_panTwo.SetValue   (SettingsUiPrefs::kDefaultDriveTwoPan * 100.0f);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnLButtonDown (int x, int y)
{
    bool  handled = false;
    int   i       = 0;



    handled = m_machineDropdown.OnLButtonDown (x, y)
           || m_speed.OnLButtonDown      (x, y)
           || m_writeMode.OnLButtonDown  (x, y)
           || m_mechanism.OnLButtonDown  (x, y)
           || m_driveAudio.OnLButtonDown (x, y)
           || m_motorVol.OnLButtonDown   (x, y)
           || m_headVol.OnLButtonDown    (x, y)
           || m_doorVol.OnLButtonDown    (x, y)
           || m_panOne.OnLButtonDown     (x, y)
           || m_panTwo.OnLButtonDown     (x, y)
           || m_motorPlay.OnLButtonDown  (x, y)
           || m_headPlay.OnLButtonDown   (x, y)
           || m_doorPlay.OnLButtonDown   (x, y)
           || m_panOnePlay.OnLButtonDown (x, y)
           || m_panTwoPlay.OnLButtonDown (x, y);

    if (!handled && m_reset.HitTest (x, y))
    {
        m_reset.SetMouse (x, y, true);
        handled = true;
    }

    for (i = 0; i < 2 && !handled; ++i)
    {
        handled = m_writeProtect[(size_t) i].OnLButtonDown (x, y);
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
    (void) m_motorVol.OnLButtonUp  (x, y);
    (void) m_headVol.OnLButtonUp   (x, y);
    (void) m_doorVol.OnLButtonUp   (x, y);
    (void) m_panOne.OnLButtonUp    (x, y);
    (void) m_panTwo.OnLButtonUp    (x, y);
    (void) m_motorPlay.OnLButtonUp  (x, y);
    (void) m_headPlay.OnLButtonUp   (x, y);
    (void) m_doorPlay.OnLButtonUp   (x, y);
    (void) m_panOnePlay.OnLButtonUp (x, y);
    (void) m_panTwoPlay.OnLButtonUp (x, y);
    if (m_reset.HitTest (x, y)) { m_reset.Click(); }
    m_reset.SetMouse (x, y, false);
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
    m_motorVol.SetMouseHover  (x, y);
    m_headVol.SetMouseHover   (x, y);
    m_doorVol.SetMouseHover   (x, y);
    m_panOne.SetMouseHover    (x, y);
    m_panTwo.SetMouseHover    (x, y);
    m_motorPlay.SetMouseHover  (x, y);
    m_headPlay.SetMouseHover   (x, y);
    m_doorPlay.SetMouseHover   (x, y);
    m_panOnePlay.SetMouseHover (x, y);
    m_panTwoPlay.SetMouseHover (x, y);
    m_reset.SetMouse          (x, y, false);
    for (i = 0; i < 2; ++i)
    {
        m_writeProtect[(size_t) i].SetMouseHover (x, y);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnMouseMove
//
//  Forwards drag motion to the volume sliders (the only draggable
//  widgets on the page).
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::OnMouseMove (int x, int y)
{
    (void) m_motorVol.OnMouseMove (x, y);
    (void) m_headVol.OnMouseMove  (x, y);
    (void) m_doorVol.OnMouseMove  (x, y);
    (void) m_panOne.OnMouseMove   (x, y);
    (void) m_panTwo.OnMouseMove   (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool MachinePage::OnKey (WPARAM vk)
{
    bool  handled = false;
    int   i       = 0;



    handled = m_machineDropdown.HandleKey (vk)
           || m_speed.HandleKey      (vk)
           || m_writeMode.HandleKey  (vk)
           || m_mechanism.HandleKey  (vk)
           || m_driveAudio.OnKey     (vk)
           || m_motorVol.OnKey       (vk)
           || m_headVol.OnKey        (vk)
           || m_doorVol.OnKey        (vk)
           || m_panOne.OnKey         (vk)
           || m_panTwo.OnKey         (vk);

    for (i = 0; i < 2 && !handled; ++i)
    {
        handled = m_writeProtect[(size_t) i].OnKey (vk);
    }

    return handled;
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
    out.push_back ([this] (bool f) { m_motorVol.SetFocused        (f); });
    out.push_back ([this] (bool f) { m_headVol.SetFocused         (f); });
    out.push_back ([this] (bool f) { m_doorVol.SetFocused         (f); });
    out.push_back ([this] (bool f) { m_panOne.SetFocused          (f); });
    out.push_back ([this] (bool f) { m_panTwo.SetFocused          (f); });
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

void MachinePage::Paint (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme)
{
    int  i = 0;



    m_machineLabel.Paint   (painter, text);
    m_speedLabel.Paint     (painter, text);
    m_wpLabel.Paint        (painter, text);
    m_writeModeLabel.Paint (painter, text);
    m_audioLabel.Paint     (painter, text);
    m_mechLabel.Paint      (painter, text);
    m_motorLabel.Paint     (painter, text);
    m_headLabel.Paint      (painter, text);
    m_doorLabel.Paint      (painter, text);
    m_panOneLabel.Paint    (painter, text);
    m_panTwoLabel.Paint    (painter, text);

    // Dropdown base boxes first; their open menus paint last so they
    // sit on top of any sibling controls below them.
    m_machineDropdown.SetTheme (&theme);
    m_speed.SetTheme           (&theme);
    m_writeMode.SetTheme       (&theme);
    m_mechanism.SetTheme       (&theme);
    m_machineDropdown.PaintBase (painter, text);
    m_speed.PaintBase           (painter, text);
    m_writeMode.PaintBase       (painter, text);
    m_mechanism.PaintBase       (painter, text);
    m_driveAudio.Paint          (painter, text, theme);
    m_motorVol.Paint            (painter, text, theme);
    m_headVol.Paint             (painter, text, theme);
    m_doorVol.Paint             (painter, text, theme);
    m_panOne.Paint              (painter, text, theme);
    m_panTwo.Paint              (painter, text, theme);
    m_motorPlay.Paint           (painter, text, theme);
    m_headPlay.Paint            (painter, text, theme);
    m_doorPlay.Paint            (painter, text, theme);
    m_panOnePlay.Paint          (painter, text, theme);
    m_panTwoPlay.Paint          (painter, text, theme);
    m_reset.Paint               (painter, text, theme);
    for (i = 0; i < 2; ++i)
    {
        m_writeProtect[(size_t) i].Paint (painter, text);
    }

    m_machineDropdown.PaintMenu (painter, text);
    m_speed.PaintMenu           (painter, text);
    m_writeMode.PaintMenu       (painter, text);
    m_mechanism.PaintMenu       (painter, text);
}
