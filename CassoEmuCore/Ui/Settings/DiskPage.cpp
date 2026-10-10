#include "Pch.h"

#include "DiskPage.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Settings/SettingsPanelState.h"





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
//  audio sub-rows, two checkboxes on the write-protect row). The labels get
//  their text here, because Layout measures them before it places anything.
//
////////////////////////////////////////////////////////////////////////////////

DiskPage::DiskPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{


    m_diskHeading.SetText        (L"Disk drives");
    m_wpLabel.SetText            (L"Write protect:");
    m_writeModeLabel.SetText     (L"Write mode:");
    m_audioLabel.SetText         (L"Drive audio:");
    m_mechLabel.SetText          (L"Mechanism:");
    m_motorLabel.SetText         (L"Motor volume:");
    m_headLabel.SetText          (L"Head volume:");
    m_doorLabel.SetText          (L"Door volume:");
    m_panOneLabel.SetText        (L"Drive 1 pan:");
    m_panTwoLabel.SetText        (L"Drive 2 pan:");
    m_tapeHeading.SetText        (L"Cassette tape");
    m_tapeLabel.SetText          (L"Fast tape loading:");
    m_tapeVolumeLabel.SetText    (L"Tape volume:");
    m_tapeAutoStopLabel.SetText  (L"Stop at end of tape:");
    m_tapeIdleStopLabel.SetText  (L"Stop when loading ends:");
    m_tapeWavFormatLabel.SetText (L"New tape WAV format:");

    Adopt (m_wpLabel);
    Adopt (m_writeModeLabel);
    Adopt (m_audioLabel);
    Adopt (m_mechLabel);
    Adopt (m_motorLabel);
    Adopt (m_headLabel);
    Adopt (m_doorLabel);
    Adopt (m_panOneLabel);
    Adopt (m_panTwoLabel);
    Adopt (m_tapeLabel);
    Adopt (m_diskHeading);
    Adopt (m_tapeHeading);
    Adopt (m_tapeDivider);

    // Each kind of storage gets a heading of its own over its rows.
    m_diskHeading.SetTextRole   (DxuiTextRole::Heading);
    m_diskHeading.SetFontWeight (DxuiFontWeight::SemiBold);
    m_tapeHeading.SetTextRole   (DxuiTextRole::Heading);
    m_tapeHeading.SetFontWeight (DxuiFontWeight::SemiBold);

    Adopt (m_writeMode);
    Adopt (m_mechanism);
    Adopt (m_driveAudio);
    Adopt (m_fastTape);
    Adopt (m_tapeAutoStop);
    Adopt (m_tapeIdleStop);
    Adopt (m_tapeWavFormat);
    Adopt (m_tapeVolume);
    Adopt (m_tapeVolumeLabel);
    Adopt (m_tapeAutoStopLabel);
    Adopt (m_tapeIdleStopLabel);
    Adopt (m_tapeWavFormatLabel);
    Adopt (m_tapeWavFormatInfo);
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
//  aligned form. That x clears the widest label, so no label wraps, and a
//  label's info tip follows its text rather than the column's edge.
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
    int  labelWidth   = GetLabelColumnPx (scaler);
    int  checkWidth   = scaler.ToPx (s_kCheckWidthDp);
    int  dropWidth    = scaler.ToPx (s_kDropdownWidthDp);
    int  sectionGap   = scaler.ToPx (s_kSectionGapDp);
    int  childIndent  = scaler.ToPx (DxuiTreeView::kIndentDip);
    int  x            = rect.left + pad;
    int  y            = rect.top  + pad;
    int  controlsX    = x + labelWidth;
    int  playSize     = rowHeight;
    int  playX        = controlsX + dropWidth + scaler.ToPx (s_kPlayGapDp);
    int  resetW       = scaler.ToPx (s_kResetWidthDp);
    int  right        = rect.right - pad;



    m_diskHeading.SetRect (MakeRect (x, y, right - x, rowHeight));
    y += rowHeight + sectionGap;

    m_wpLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_writeProtect[0].SetRect (MakeRect (controlsX,                y, checkWidth, rowHeight));
    m_writeProtect[0].SetLabel (L"Drive 1");
    m_writeProtect[1].SetRect (MakeRect (controlsX + checkWidth,   y, checkWidth, rowHeight));
    m_writeProtect[1].SetLabel (L"Drive 2");
    y += rowHeight + sectionGap;

    m_writeModeLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_writeMode.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_writeMode.SetItems ({ L"Buffer and flush", L"Copy on write" });
    y += rowHeight + sectionGap;

    m_audioLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_driveAudio.SetRect (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    // Mechanism is a child of Drive audio: indent the label by the
    // same childIndent used elsewhere (one DxuiTreeView nesting step).
    m_mechLabel.SetRect  (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_mechanism.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_mechanism.SetItems ({ L"Shugart", L"Alps" });
    y += rowHeight + sectionGap;

    // Per-sound volume sliders, also children of Drive audio. Each gets
    // a play button to its right that auditions the sound at the dialed
    // level.
    m_motorLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    ConfigureVolumeSlider (m_motorVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_motorPlay.SetGlyph (s_kpszMdl2Play);
    m_motorPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_headLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    ConfigureVolumeSlider (m_headVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_headPlay.SetGlyph (s_kpszMdl2Play);
    m_headPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_doorLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    ConfigureVolumeSlider (m_doorVol, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_doorPlay.SetGlyph (s_kpszMdl2Play);
    m_doorPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_panOneLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    ConfigurePanSlider (m_panOne, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_panOnePlay.SetGlyph (s_kpszMdl2Play);
    m_panOnePlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;

    m_panTwoLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    ConfigurePanSlider (m_panTwo, MakeRect (controlsX, y, dropWidth, rowHeight));
    m_panTwoPlay.SetGlyph (s_kpszMdl2Play);
    m_panTwoPlay.Layout   (MakeRect (playX, y, playSize, rowHeight), scaler);
    y += rowHeight + sectionGap;


    // The rule runs margin to margin and is laid out again on every resize,
    // so it always spans the page as it is now.
    m_tapeDivider.SetRect (MakeRect (x, y, right - x, sectionGap));
    y += sectionGap * 2;

    m_tapeHeading.SetRect (MakeRect (x, y, right - x, rowHeight));
    y += rowHeight + sectionGap;

    // Off loads tapes at the selected speed with the tape audible, as a real
    // load would sound.
    m_tapeLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_fastTape.SetRect  (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    // Heard only when loading at real speed.
    m_tapeVolumeLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    ConfigureVolumeSlider (m_tapeVolume, MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    // Off leaves the deck running on past the end until Stop is pressed.
    m_tapeAutoStopLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_tapeAutoStop.SetRect      (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    // Not something the hardware did -- the Apple II has no motor control, so
    // a person pressed Stop -- but nobody misses doing it.
    m_tapeIdleStopLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_tapeIdleStop.SetRect      (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    // 16-bit is the common format. Some tools, among them CiderPress II, read
    // only 8-bit; recording onto an existing tape keeps its bit depth.
    m_tapeWavFormatLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_tapeWavFormatInfo.SetRect  (GetInfoTipRect (m_tapeWavFormatLabel, scaler));
    m_tapeWavFormat.SetRect      (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_tapeWavFormat.SetItems     ({ L"16-bit", L"8-bit" });
    m_tapeWavFormatInfo.SetText  (L"Casso reads 8-bit and 16-bit tapes alike. Some other tools, such as "
                                 L"CiderPress II, read only 8-bit WAV files. This applies only to new blank "
                                 L"tapes; recording onto an existing tape keeps its bit depth.");
    y += rowHeight + sectionGap;

    // Below both sections, because it restores the whole page.
    m_reset.SetLabel (L"Restore defaults");
    m_reset.Layout   (MakeRect (controlsX, y, resetW, rowHeight));

    m_wpLabel.SetDpi         (dpi);
    m_writeModeLabel.SetDpi  (dpi);
    m_audioLabel.SetDpi      (dpi);
    m_mechLabel.SetDpi       (dpi);
    m_writeMode.SetDpi       (dpi);
    m_mechanism.SetDpi       (dpi);
    m_driveAudio.SetDpi      (dpi);
    m_fastTape.SetDpi        (dpi);
    m_tapeAutoStop.SetDpi    (dpi);
    m_tapeVolume.SetDpi      (dpi);
    m_tapeVolumeLabel.SetDpi (dpi);
    m_tapeAutoStopLabel.SetDpi (dpi);
    m_tapeIdleStop.SetDpi      (dpi);
    m_tapeIdleStopLabel.SetDpi (dpi);
    m_tapeWavFormat.SetDpi      (dpi);
    m_tapeWavFormatLabel.SetDpi (dpi);
    m_tapeWavFormatInfo.SetDpi  (dpi);
    m_tapeLabel.SetDpi       (dpi);
    m_diskHeading.SetDpi     (dpi);
    m_tapeHeading.SetDpi     (dpi);
    m_tapeDivider.SetDpi     (dpi);
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

    // Every control here is a fixed height, so the lowest one is where the
    // content ends, whatever the rect.
    SetContentHeightPx (GetLowestChildBottomPx() + scaler.ToPx (s_kPagePadDp) - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::GetMeasuringRenderer
//
//  The renderer Layout measures text with: one set for the page, else the
//  popup host's, asked each time because the host rebuilds it when the
//  device is lost. Null before the host has one.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiTextRenderer * DiskPage::GetMeasuringRenderer() const
{
    if (m_textRenderer != nullptr)
    {
        return m_textRenderer;
    }

    return (m_popupHost != nullptr) ? m_popupHost->GetTextRenderer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::MeasureLabelPx
//
//  How wide the label's text draws, in the face and size it draws in. With
//  nothing to measure with, or a measurement that fails, an average glyph
//  width per character stands in.
//
////////////////////////////////////////////////////////////////////////////////

int DiskPage::MeasureLabelPx (const DxuiLabel & label, const DxuiDpiScaler & scaler) const
{
    constexpr int  kGlyphWidthDp = 7;



    HRESULT              hr       = S_OK;
    IDxuiTextRenderer  * text     = GetMeasuringRenderer();
    float                widthPx  = 0.0f;
    float                heightPx = 0.0f;



    if (text != nullptr)
    {
        hr = text->MeasureString (label.GetText().c_str(), scaler.ToPxf (label.GetFontSizeDip()), DxuiTheme::kBodyFace, widthPx, heightPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    // A failed measurement leaves the width at zero, as no renderer does.
    if (widthPx <= 0.0f)
    {
        widthPx = (float) (label.GetText().size() * scaler.ToPx (kGlyphWidthDp));
    }

    return (int) std::ceil (widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::GetLabelColumnPx
//
//  Wide enough that no label wraps: the widest label at its indent, with its
//  info tip after it where it has one, and then the gap before the controls.
//  Never narrower than the design width, so short labels keep the page's
//  usual alignment.
//
////////////////////////////////////////////////////////////////////////////////

int DiskPage::GetLabelColumnPx (const DxuiDpiScaler & scaler) const
{
    constexpr int  kLabelGapDp = 12;



    const DxuiLabel  * topLevel[] = { &m_wpLabel, &m_writeModeLabel, &m_audioLabel, &m_tapeLabel,
                                      &m_tapeVolumeLabel, &m_tapeAutoStopLabel, &m_tapeIdleStopLabel };
    const DxuiLabel  * nested[]   = { &m_mechLabel, &m_motorLabel, &m_headLabel, &m_doorLabel, &m_panOneLabel, &m_panTwoLabel };
    const DxuiLabel  * tipped[]   = { &m_tapeWavFormatLabel };
    int                indent     = scaler.ToPx (DxuiTreeView::kIndentDip);
    int                tipReach   = scaler.ToPx (kInfoTipGapDp) + (int) std::ceil (scaler.ToPxf (DxuiInfoTip::kGlyphDip));
    int                widest     = 0;



    for (const DxuiLabel * label : topLevel)
    {
        widest = std::max (widest, MeasureLabelPx (*label, scaler));
    }

    for (const DxuiLabel * label : nested)
    {
        widest = std::max (widest, indent + MeasureLabelPx (*label, scaler));
    }

    for (const DxuiLabel * label : tipped)
    {
        widest = std::max (widest, MeasureLabelPx (*label, scaler) + tipReach);
    }

    return std::max (scaler.ToPx (s_kLabelWidthDp), widest + scaler.ToPx (kLabelGapDp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::GetInfoTipRect
//
//  A row-high square whose glyph starts a small gap past the end of the
//  label's text. The glyph is centered in the square, so the square starts
//  half its margin before that point. It drops by a fraction of the label's
//  font size to put the ring's center on the capitals' center.
//
////////////////////////////////////////////////////////////////////////////////

RECT DiskPage::GetInfoTipRect (const DxuiLabel & label, const DxuiDpiScaler & scaler) const
{
    // Centering the two boxes leaves the ring riding high, because the icon
    // font's ink sits higher in its box than the body face's capitals do in
    // theirs. Measured from rendered captures.
    constexpr float  kInfoTipDropEm = 0.05f;



    RECT  row     = label.GetRect();
    int   size    = row.bottom - row.top;
    int   glyphPx = (int) std::ceil (scaler.ToPxf (DxuiInfoTip::kGlyphDip));
    int   glyphX  = row.left + MeasureLabelPx (label, scaler) + scaler.ToPx (kInfoTipGapDp);
    int   drop    = (int) std::lround (scaler.ToPxf (label.GetFontSizeDip()) * kInfoTipDropEm);



    return MakeRect (glyphX - (size - glyphPx) / 2, row.top + drop, size, size);
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
    m_fastTape.SetChecked   (state->GetPrefs().fastTapeLoading);
    m_tapeAutoStop.SetChecked (state->GetPrefs().tapeAutoStop);
    m_tapeIdleStop.SetChecked (state->GetPrefs().tapeIdleStop);
    m_tapeWavFormat.SetSelected (state->GetPrefs().tapeEightBit ? 1 : 0);
    m_tapeVolume.SetValue   (state->GetPrefs().tapeVolume * 100.0f);
    m_writeProtect[0].SetChecked (state->GetPrefs().writeProtect[0]);
    m_writeProtect[1].SetChecked (state->GetPrefs().writeProtect[1]);
    m_motorVol.SetValue     (state->GetPrefs().driveMotorVolume * 100.0f);
    m_headVol.SetValue      (state->GetPrefs().driveHeadVolume  * 100.0f);
    m_doorVol.SetValue      (state->GetPrefs().driveDoorVolume  * 100.0f);
    m_panOne.SetValue       (state->GetPrefs().driveOnePan * 100.0f);
    m_panTwo.SetValue       (state->GetPrefs().driveTwoPan * 100.0f);
    ApplyDriveAudioChildEnabled (state->GetPrefs().floppySoundEnabled);

    m_writeMode.SetSelect    ([state] (int idx) { state->SetWriteMode ((SettingsWriteMode) idx); });
    m_mechanism.SetSelect    ([state] (int idx) { state->SetMechanism (idx == 1 ? "alps" : "shugart"); });
    m_driveAudio.SetOnChange ([this, state] (bool checked)
    {
        state->SetFloppySound (checked);
        ApplyDriveAudioChildEnabled (checked);
    });
    m_fastTape.SetOnChange ([state] (bool checked) { state->SetFastTapeLoading (checked); });
    m_tapeAutoStop.SetOnChange ([state] (bool checked) { state->SetTapeAutoStop (checked); });
    m_tapeIdleStop.SetOnChange ([state] (bool checked) { state->SetTapeIdleStop (checked); });
    m_tapeWavFormat.SetSelect   ([state] (int idx) { state->SetTapeEightBit (idx == 1); });
    m_tapeVolume.SetOnChange ([state] (float v) { state->SetTapeVolume (v / 100.0f); });
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
    m_reset.SetOnClick      ([this] { ResetPageToDefaults(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::SetPopupHost
//
//  Routes each owned dropdown's menu through the supplied host's popup pool
//  so the menu HWND escapes the page's clipping bounds. Pass nullptr to
//  revert to the in-panel PaintMenu path. Layout measures the labels with the
//  host's renderer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::SetPopupHost (DxuiHwndSource * host)
{
    m_popupHost = host;

    m_writeMode.SetPopupHost         (host);
    m_mechanism.SetPopupHost         (host);
    m_tapeWavFormat.SetPopupHost     (host);
    m_tapeWavFormatInfo.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPage::ApplyDriveAudioChildEnabled
//
//  Enables / disables every control nested under the Drive-audio toggle
//  (mechanism, the volume + pan sliders and their play buttons) and dims
//  their labels to match.
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
//  DiskPage::ResetPageToDefaults
//
//  Restores every setting on the page, disk drives and cassette tape alike,
//  to its SettingsUiPrefs default, then re-syncs the widgets from the state.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPage::ResetPageToDefaults()
{
    HRESULT                hr       = S_OK;
    const SettingsUiPrefs  defaults;



    CBRA (m_state != nullptr);

    m_state->SetWriteProtect     (0, defaults.writeProtect[0]);
    m_state->SetWriteProtect     (1, defaults.writeProtect[1]);
    m_state->SetWriteMode        (defaults.writeMode);
    m_state->SetFloppySound      (defaults.floppySoundEnabled);
    m_state->SetMechanism        (defaults.floppyMechanism);
    m_state->SetDriveMotorVolume (defaults.driveMotorVolume);
    m_state->SetDriveHeadVolume  (defaults.driveHeadVolume);
    m_state->SetDriveDoorVolume  (defaults.driveDoorVolume);
    m_state->SetDriveOnePan      (defaults.driveOnePan);
    m_state->SetDriveTwoPan      (defaults.driveTwoPan);
    m_state->SetFastTapeLoading  (defaults.fastTapeLoading);
    m_state->SetTapeVolume       (defaults.tapeVolume);
    m_state->SetTapeAutoStop     (defaults.tapeAutoStop);
    m_state->SetTapeIdleStop     (defaults.tapeIdleStop);
    m_state->SetTapeEightBit     (defaults.tapeEightBit);

    Rebuild();

Error:
    return;
}

