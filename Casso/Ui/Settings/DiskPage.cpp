#include "Pch.h"

#include "DiskPage.h"
#include "Devices/Disk/ExternalChangePolicy.h"

#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-local helpers
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
//  DiskPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DiskPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::DiskPage
//
//  Registers each member widget into the page's child list via Adopt so
//  they participate in the IDxuiControl tree (Bounds, Visible, focus, parent
//  pointers). The widgets remain DiskPage-owned members; Adopt is non-owning.
//  Layout positioning happens in Layout() below because the layout does
//  things DxuiFormLayout cannot model (per-row indentation for the drive-
//  audio sub-rows, two checkboxes on the write-protect row).
//
////////////////////////////////////////////////////////////////////////////////

DiskPage::DiskPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{


    Adopt (m_wpLabel);
    Adopt (m_writeModeLabel);
    Adopt (m_externalChangeLabel);
    Adopt (m_audioLabel);
    Adopt (m_mechLabel);
    Adopt (m_motorLabel);
    Adopt (m_headLabel);
    Adopt (m_doorLabel);
    Adopt (m_panOneLabel);
    Adopt (m_panTwoLabel);

    Adopt (m_writeMode);
    Adopt (m_externalChange);
    Adopt (m_mechanism);
    Adopt (m_driveAudio);
    for (DxuiCheckbox & checkbox : m_writeProtect)
    {
        Adopt (checkbox);
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
//  DiskPage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::Layout
//
//  Lays out the Disk page: write protection, write mode, and the drive-audio
//  options.
//
//  A linear top-to-bottom walk with one running y, like the other settings
//  pages, so rows can be added or removed without recomputing anything below
//  them.
//
//  Every control starts at the same x, which is what makes the page read as an
//  aligned form.
//
//  Child rows indent by the same amount as DxuiTreeView, so a sub-option under
//  the audio toggle reads as nested against the rest of the UI rather than by
//  an arbitrary amount.
//
//  The audio preview button is sized SQUARE to the row height and placed after
//  the dropdown, so it reads as an affordance attached to that control rather
//  than as another form field.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi          = scaler.GetDpi();
    int  pad          = scaler.ToPx (s_kPagePadDp);
    int  rowHeight    = scaler.ToPx (s_kRowHeightDp);
    int  labelWidth   = scaler.ToPx (s_kLabelWidthDp);
    int  checkWidth   = scaler.ToPx (s_kCheckWidthDp);
    int  dropWidth    = scaler.ToPx (s_kDropdownWidthDp);
    int  sectionGap   = scaler.ToPx (s_kSectionGapDp);
    int  childIndent  = scaler.ToPx (18);          // matches DxuiTreeView indent
    int  x            = rect.left + pad;
    int  y            = rect.top  + pad;
    int  controlsX    = x + labelWidth;
    int  playSize     = rowHeight;
    int  playX        = controlsX + dropWidth + scaler.ToPx (s_kPlayGapDp);
    int  resetW       = scaler.ToPx (s_kResetWidthDp);



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

    //  What happens when something else changes a mounted image and says
    //  nothing about what it meant -- an editor, a copy, another emulator.
    //  Tools that DO state an intent are obeyed regardless of this.
    m_externalChangeLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_externalChangeLabel.SetText (L"Changed on disk:");
    m_externalChange.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_externalChange.SetItems ({ L"Ask me", L"Take it up", L"Take it up and restart" });
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

    m_wpLabel.SetDpi         (dpi);
    m_writeModeLabel.SetDpi  (dpi);
    m_externalChangeLabel.SetDpi (dpi);
    m_audioLabel.SetDpi      (dpi);
    m_mechLabel.SetDpi       (dpi);
    m_writeMode.SetDpi       (dpi);
    m_externalChange.SetDpi  (dpi);
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
    DxuiPanel::SetBounds (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::Rebuild
//
//  Re-sync widget visible state to the underlying SettingsPanelState
//  and wire each widget's OnChange callback back into the state.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::Rebuild()
{
    SettingsPanelState  * state = m_state;



    if (state == nullptr)
    {
        return;
    }

    m_writeMode.SetSelected ((int) state->GetPrefs().writeMode);
    m_mechanism.SetSelected (state->GetPrefs().floppyMechanism == "alps" ? 1 : 0);
    m_driveAudio.SetChecked (state->GetPrefs().floppySoundEnabled);
    m_writeProtect[0].SetChecked (state->GetPrefs().writeProtect[0]);
    m_writeProtect[1].SetChecked (state->GetPrefs().writeProtect[1]);
    m_motorVol.SetValue     (state->GetPrefs().driveMotorVolume * 100.0f);
    m_headVol.SetValue      (state->GetPrefs().driveHeadVolume  * 100.0f);
    m_doorVol.SetValue      (state->GetPrefs().driveDoorVolume  * 100.0f);
    m_panOne.SetValue       (state->GetPrefs().driveOnePan * 100.0f);
    m_panTwo.SetValue       (state->GetPrefs().driveTwoPan * 100.0f);
    ApplyDriveAudioChildEnabled (state->GetPrefs().floppySoundEnabled);

    m_writeMode.SetSelect    ([state] (int idx) { state->SetWriteMode ((SettingsWriteMode) idx); });

    //  The stored value is a token and its meaning belongs to
    //  ExternalChangePolicy, so the page maps between the token and the row
    //  index and decides nothing else. An unrecognized stored value comes back
    //  as Ask, which is the row this lands on.
    if (m_prefs != nullptr)
    {
        GlobalUserPrefs *  prefs  = m_prefs;
        FallbackAnswer     answer =
            ExternalChangePolicy::ParseFallbackAnswer (prefs->externalChangeAnswer);

        m_externalChange.SetSelected (ExternalChangePolicy::IndexOfFallbackAnswer (answer));

        m_externalChange.SetSelect ([this, prefs] (int idx)
        {
            FallbackAnswer  chosen = ExternalChangePolicy::FallbackAnswerAtIndex (idx);

            prefs->externalChangeAnswer = ExternalChangePolicy::SpellFallbackAnswer (chosen);
            MarkDirty();
        });
    }

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
//  DiskPage::SetPopupHost
//
//  Routes each owned dropdown's menu through the supplied host's popup pool
//  so the menu HWND escapes the page's clipping bounds. Pass nullptr to
//  revert to the in-panel PaintMenu path.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::SetPopupHost (DxuiHwndSource * host)
{
    m_writeMode.SetPopupHost       (host);
    m_externalChange.SetPopupHost (host);
    m_mechanism.SetPopupHost       (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::ApplyDriveAudioChildEnabled
//
//  Enables / disables every control nested under the Drive-audio toggle
//  (mechanism, the volume + pan sliders, their play buttons, and the reset
//  button) and dims their labels to match.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::ApplyDriveAudioChildEnabled (bool enabled)
{
    DxuiTextRole  labelRole = enabled ? DxuiTextRole::Body : DxuiTextRole::Disabled;



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
    m_mechLabel.SetTextRole   (labelRole);
    m_motorLabel.SetTextRole  (labelRole);
    m_headLabel.SetTextRole   (labelRole);
    m_doorLabel.SetTextRole   (labelRole);
    m_panOneLabel.SetTextRole (labelRole);
    m_panTwoLabel.SetTextRole (labelRole);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::ConfigureVolumeSlider
//
//  0-100% linear volume slider with a "%" readout.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect)
{
    constexpr float  s_kVolumeMax = 100.0f;



    slider.SetRect      (rect);
    slider.SetRange     (0.0f, s_kVolumeMax);
    slider.SetStep      (1.0f);
    slider.SetSuffix    (L"%");
    slider.SetDecimalPlaces (0);
    slider.SetShowTicks (true);
    slider.SetTickInterval (10.0f);   // ticks every 10%, not per step-1
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::ConfigurePanSlider
//
//  Bipolar Left..Center..Right pan slider. Range -100 (hard left) ..
//  +100 (hard right), centered detent at 0. The readout names the position
//  ("Left" / "Center" / "Right") and the fill grows from the track center.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::ConfigurePanSlider (DxuiSlider & slider, const RECT & rect)
{
    constexpr float  s_kPanMax = 100.0f;



    slider.SetRect      (rect);
    slider.SetRange     (-s_kPanMax, s_kPanMax);
    slider.SetStep      (5.0f);
    slider.SetShowTicks (true);
    slider.SetTickInterval (25.0f);   // ticks at L/75/50/25/C/25/50/75/R
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
//  DiskPage::ResetDriveAudioToDefaults
//
//  Restores every drive-audio knob to its SettingsUiPrefs default and syncs
//  the slider widgets to match.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::ResetDriveAudioToDefaults()
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

