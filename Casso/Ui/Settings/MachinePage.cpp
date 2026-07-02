#include "Pch.h"

#include "MachinePage.h"

#include "Core/UnicodeSymbols.h"





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
    constexpr int    s_kPlayGapDp       = 8;
    constexpr int    s_kResetWidthDp    = 130;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::MachinePage
//
//  Registers each member widget into the panel's child list via
//  Adopt so they participate in the IDxuiControl tree (Bounds,
//  Visible, focus, parent pointers). The widgets remain MachinePage-
//  owned members; Adopt is non-owning. Layout positioning still
//  happens in Layout() below via the legacy SetRect calls because
//  the existing layout code does things DxuiFormLayout cannot model
//  (per-row indentation for the mechanism sub-row, two checkboxes
//  on a single row for the write-protect pair). SettingsPanel still
//  drives input/paint through the bespoke shims below; collapsing
//  the duality is deferred to the SettingsPanel atomic conversion.
//
////////////////////////////////////////////////////////////////////////////////

MachinePage::MachinePage()
{
    size_t  i = 0;


    Adopt (m_machineLabel);
    Adopt (m_speedLabel);
    Adopt (m_wpLabel);
    Adopt (m_writeModeLabel);
    Adopt (m_audioLabel);
    Adopt (m_mechLabel);
    Adopt (m_motorLabel);
    Adopt (m_headLabel);
    Adopt (m_doorLabel);
    Adopt (m_panOneLabel);
    Adopt (m_panTwoLabel);

    Adopt (m_machineDropdown);
    Adopt (m_speed);
    Adopt (m_writeMode);
    Adopt (m_mechanism);
    Adopt (m_driveAudio);
    for (i = 0; i < m_writeProtect.size(); ++i)
    {
        Adopt (m_writeProtect[i]);
    }

    Adopt (m_motorVol);
    Adopt (m_headVol);
    Adopt (m_doorVol);
    Adopt (m_panOne);
    Adopt (m_panTwo);
    Adopt (m_motorPlay);
    Adopt (m_headPlay);
    Adopt (m_doorPlay);
    Adopt (m_panOnePlay);
    Adopt (m_panTwoPlay);
    Adopt (m_reset);

    m_motorPlay.SetAccessibleName  (L"Audition motor sound");
    m_headPlay.SetAccessibleName   (L"Audition head sound");
    m_doorPlay.SetAccessibleName   (L"Audition door sound");
    m_panOnePlay.SetAccessibleName (L"Audition Drive 1 pan");
    m_panTwoPlay.SetAccessibleName (L"Audition Drive 2 pan");
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
    // same childIndent used elsewhere (matches DxuiTreeView's 18 dp).
    m_mechLabel.SetRect  (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_mechLabel.SetText  (L"Mechanism:");
    m_mechanism.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_mechanism.SetItems ({ L"Shugart", L"Alps" });
    y += rowHeight + sectionGap;

    // Per-sound volume sliders, also children of Drive audio. Each gets
    // a play button to its right that auditions the sound at the dialed
    // level.
    m_motorLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_motorLabel.SetText (L"Motor volume:");
    ConfigureVolumeSlider (m_motorVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_motorPlay.SetGlyph (s_kpszMdl2Play);
    m_motorPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_headLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_headLabel.SetText (L"Head volume:");
    ConfigureVolumeSlider (m_headVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_headPlay.SetGlyph (s_kpszMdl2Play);
    m_headPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_doorLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_doorLabel.SetText (L"Door volume:");
    ConfigureVolumeSlider (m_doorVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_doorPlay.SetGlyph (s_kpszMdl2Play);
    m_doorPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_panOneLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_panOneLabel.SetText (L"Drive 1 pan:");
    ConfigurePanSlider (m_panOne, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_panOnePlay.SetGlyph (s_kpszMdl2Play);
    m_panOnePlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_panTwoLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_panTwoLabel.SetText (L"Drive 2 pan:");
    ConfigurePanSlider (m_panTwo, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_panTwoPlay.SetGlyph (s_kpszMdl2Play);
    m_panTwoPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_reset.SetLabel (L"Restore defaults");
    m_reset.Layout   (MakeRect (controlsX, y, resetW, rowHeight));

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
    m_motorLabel.SetDpi      (dpi);
    m_headLabel.SetDpi       (dpi);
    m_doorLabel.SetDpi       (dpi);
    m_panOneLabel.SetDpi     (dpi);
    m_panTwoLabel.SetDpi     (dpi);
    m_motorVol.SetDpi        (dpi);
    m_headVol.SetDpi         (dpi);
    m_doorVol.SetDpi         (dpi);
    m_panOne.SetDpi          (dpi);
    m_panTwo.SetDpi          (dpi);
    m_reset.SetDpi           (dpi);

    // Mirror the page's footprint into the IDxuiControl tree so future
    // centralized walks see this page as a panel covering `rect`.
    // Adopted children already have their bounds written via the
    // SetRect calls above (SetRect mirrors through SetBounds).
    DxuiPanel::SetBounds (rect);
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
    m_motorVol.SetValue     (state->Prefs().driveMotorVolume * 100.0f);
    m_headVol.SetValue      (state->Prefs().driveHeadVolume  * 100.0f);
    m_doorVol.SetValue      (state->Prefs().driveDoorVolume  * 100.0f);
    m_panOne.SetValue       (state->Prefs().driveOnePan * 100.0f);
    m_panTwo.SetValue       (state->Prefs().driveTwoPan * 100.0f);
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
    m_writeProtect[0].SetOnChange ([state] (bool checked) { state->SetWriteProtect (0, checked); });
    m_writeProtect[1].SetOnChange ([state] (bool checked) { state->SetWriteProtect (1, checked); });

    m_motorVol.SetOnChange ([state] (float v) { state->SetDriveMotorVolume (v / 100.0f); });
    m_headVol.SetOnChange  ([state] (float v) { state->SetDriveHeadVolume  (v / 100.0f); });
    m_doorVol.SetOnChange  ([state] (float v) { state->SetDriveDoorVolume  (v / 100.0f); });
    m_panOne.SetOnChange   ([state] (float v) { state->SetDriveOnePan (v / 100.0f); });
    m_panTwo.SetOnChange   ([state] (float v) { state->SetDriveTwoPan (v / 100.0f); });

    // Volume previews play balanced at the midpoint (centered); the pan
    // buttons play at each drive's dialed position.
    m_motorPlay.SetOnClick  ([this] { if (m_onTestSound) { m_onTestSound (0, 0, true);  } });
    m_headPlay.SetOnClick   ([this] { if (m_onTestSound) { m_onTestSound (0, 1, true);  } });
    m_doorPlay.SetOnClick   ([this] { if (m_onTestSound) { m_onTestSound (0, 2, true);  } });
    m_panOnePlay.SetOnClick ([this] { if (m_onTestSound) { m_onTestSound (0, 1, false); } });
    m_panTwoPlay.SetOnClick ([this] { if (m_onTestSound) { m_onTestSound (1, 1, false); } });
    m_reset.SetOnClick      ([this] { ResetDriveAudioToDefaults(); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::SetPopupHost
//
//  Routes each owned dropdown's menu through the supplied host's
//  popup pool so the menu HWND escapes the page's clipping bounds.
//  Pass nullptr to revert to the in-panel PaintMenu path.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::SetPopupHost (DxuiHostWindow * host)
{
    m_machineDropdown.SetPopupHost (host);
    m_speed.SetPopupHost           (host);
    m_writeMode.SetPopupHost       (host);
    m_mechanism.SetPopupHost       (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage::ApplyDriveAudioChildEnabled
//
//  Enables / disables every control nested under the Drive-audio
//  toggle (mechanism, the volume + pan sliders, their play buttons,
//  and the reset button) and dims their labels to match.
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
//  MachinePage::ConfigureVolumeSlider
//
//  0-100% linear volume slider with a "%" readout.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect)
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
//  Bipolar Left..Center..Right pan slider. Range -100 (hard left) ..
//  +100 (hard right), centered detent at 0. The readout names the
//  position ("Left" / "Center" / "Right") and the fill grows from the
//  track center.
//
////////////////////////////////////////////////////////////////////////////////

void MachinePage::ConfigurePanSlider (DxuiSlider & slider, const RECT & rect)
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
//  MachinePage::ResetDriveAudioToDefaults
//
//  Restores every drive-audio knob to its SettingsUiPrefs default and
//  syncs the slider widgets to match.
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
//  Bespoke input + paint shims (OnLButtonDown / OnLButtonUp /
//  OnMouseHover / OnKey / Paint(painter,text,theme) / CollectFocusables /
//  AnyDropdownOpen) used to live here. SettingsPanel now dispatches
//  via IDxuiControl::OnMouse / OnKey through DxuiPanel auto fan-out
//  (input) and queries individual widgets directly (focus, dropdowns).
//  Paint runs through the inherited DxuiPanel::Paint walk over the
//  Adopt'd child list. Out-of-panel popup hosting (Sub-step B)
//  collapses the legacy PaintBase-then-PaintMenu z-slot, so the auto
//  walk preserves the correct stacking.
//
////////////////////////////////////////////////////////////////////////////////
