#include "Pch.h"

#include "Ui/Settings/SerialCardPage.h"
#include "Ui/Settings/SettingsInfoTip.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Core/TextEncoding.h"




// Layout metrics (DIP), matching the Printing page.
static constexpr int    s_kRowHeightDp     = 28;
static constexpr int    s_kLabelWidthDp    = 210;
static constexpr int    s_kDropdownWidthDp = 260;
static constexpr int    s_kButtonWidthDp   = 130;
static constexpr int    s_kSectionGapDp    = 14;
static constexpr int    s_kRowGapDp        = 8;
static constexpr int    s_kPagePadDp       = 16;
static constexpr int    s_kNoteHeightDp    = 36;     // two lines of note text
static constexpr float  s_kNoteFontDp      = 12.0f;
static constexpr int    s_kRestoreWidthDp  = 130;    // as the Storage page's Restore defaults





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SerialCardPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::SerialCardPage
//
//  Every widget is a member, adopted into the page's tree here; Layout
//  positions them and hides the rows the selected mode does not use.
//
////////////////////////////////////////////////////////////////////////////////

SerialCardPage::SerialCardPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_firmwareBanner);
    Adopt (m_download);
    Adopt (m_switchesHeading);

    for (int i = 0; i < kMaxRows; ++i)
    {
        Adopt (m_rowLabels[i]);
        Adopt (m_rowTips[i]);
        Adopt (m_rowCombos[i]);

        // Wired once, by row: Rebuild changes the setting in a row but never
        // replaces a handler, which could be running when it is called.
        m_rowCombos[i].SetSelect ([this, i] (int picked) { OnRowPicked (i, picked); });
    }

    for (int i = 0; i < kMaxRows; ++i)
    {
        Adopt (m_rowNotes[i]);
        m_rowNotes[i].SetTextRole   (DxuiTextRole::Muted);
        m_rowNotes[i].SetFontSizeDip (s_kNoteFontDp);
    }

    Adopt (m_switchesView);
    m_attachment.AdoptInto (*this);
    Adopt (m_restore);

    m_switchesView.SetOnToggle ([this] (SscBank bank, int number) { OnSwitchToggled (bank, number); });
    m_attachment.SetOnChange   ([this] (const SerialAttachmentChoice & choice) { OnAttachmentChanged (choice); });
    m_restore.SetOnClick       ([this] () { OnRestoreDefaults(); });
    m_download.SetOnClick ([this] ()
    {
        if (m_onDownload)
        {
            m_onDownload();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::SetPopupHost
//
//  Layout measures the labels with the host's renderer.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::SetPopupHost (DxuiHwndSource * host)
{
    m_popupHost = host;

    for (int i = 0; i < kMaxRows; ++i)
    {
        m_rowCombos[i].SetPopupHost (host);
        m_rowTips[i].SetPopupHost   (host);
    }

    m_attachment.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::SetFirmwareStatus
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::SetFirmwareStatus (const std::wstring & text, bool offerDownload)
{
    m_firmwareBanner.SetText     (text);
    m_firmwareBanner.SetSeverity (offerDownload ? DxuiInfoBanner::Severity::Warning
                                                : DxuiInfoBanner::Severity::Info);
    m_offerDownload = offerDownload;
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::Layout
//
//  The firmware banner first, since its height follows its text; then the
//  serial connector with its note, the switch settings for the selected
//  mode, each followed by its note when it has one, and the drawing of the
//  switches. Rows the mode does not use are hidden.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi          = scaler.GetDpi();
    int   pad          = scaler.ToPx (s_kPagePadDp);
    int   rowHeight    = scaler.ToPx (s_kRowHeightDp);
    int   labelWidth   = GetLabelColumnPx (scaler);
    int   buttonWidth  = scaler.ToPx (s_kButtonWidthDp);
    int   sectionGap   = scaler.ToPx (s_kSectionGapDp);
    int   rowGap       = scaler.ToPx (s_kRowGapDp);
    int   noteHeight   = scaler.ToPx (s_kNoteHeightDp);
    int   x            = rect.left + pad;
    int   y            = rect.top  + pad;
    int   controlsX    = x + labelWidth;
    int   dropWidth    = GetDropdownWidthPx (scaler, rect.right - pad - controlsX);
    int   noteWidth    = std::max (dropWidth, scaler.ToPx (s_kDropdownWidthDp) + rowGap + buttonWidth);
    int   bannerWidth  = (rect.right - rect.left) - pad * 2;
    int   bannerHeight = static_cast<int> (std::ceil (m_firmwareBanner.GetPreferredHeightPx (static_cast<float> (bannerWidth), scaler)));



    m_hasLayout  = true;
    m_lastRect   = rect;
    m_lastScaler = scaler;

    m_firmwareBanner.SetRect (MakeRect (x, y, bannerWidth, bannerHeight));
    m_firmwareBanner.SetDpi  (dpi);
    y += bannerHeight + rowGap;

    m_download.SetLabel   (L"Download firmware");
    m_download.SetVisible (m_offerDownload);
    m_download.Layout     (MakeRect (x, y, buttonWidth, rowHeight));
    m_download.SetDpi     (dpi);

    if (m_offerDownload)
    {
        y += rowHeight;
    }

    y += sectionGap;

    y = m_attachment.Layout (x, y, labelWidth, noteWidth, { rowHeight, rowGap, sectionGap, noteHeight }, scaler);

    y += sectionGap;

    m_switchesHeading.SetText (L"Switches");
    m_switchesHeading.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_switchesHeading.SetDpi  (dpi);
    y += rowHeight;

    for (int i = 0; i < kMaxRows; ++i)
    {
        bool          used    = i < GetRowCount();
        bool          hasNote = used && !m_rowNoteText[i].empty();
        std::wstring  tip     = used ? SscSettingsModel::GetInfoTip (m_rowSettings[static_cast<size_t> (i)]) : std::wstring();

        m_rowLabels[i].SetVisible (used);
        m_rowCombos[i].SetVisible (used);
        m_rowNotes[i].SetVisible  (hasNote);
        m_rowTips[i].SetVisible   (!tip.empty());

        if (used)
        {
            m_rowLabels[i].SetText (SscSettingsModel::GetLabel (m_rowSettings[static_cast<size_t> (i)]) + L":");
            m_rowLabels[i].SetRect (MakeRect (x, y, labelWidth, rowHeight));
            m_rowTips[i].SetText   (tip);
            m_rowTips[i].SetRect   (SettingsInfoTip::GetRect (m_rowLabels[i], scaler, GetMeasuringRenderer()));
            m_rowCombos[i].SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
            y += rowHeight + rowGap;
        }

        if (hasNote)
        {
            m_rowNotes[i].SetText (m_rowNoteText[i]);
            m_rowNotes[i].SetRect (MakeRect (controlsX, y, noteWidth, noteHeight));
            y += noteHeight + sectionGap;
        }

        m_rowLabels[i].SetDpi (dpi);
        m_rowTips[i].SetDpi   (dpi);
        m_rowCombos[i].SetDpi (dpi);
        m_rowNotes[i].SetDpi  (dpi);
    }

    m_switchesView.Layout (MakeRect (controlsX, y, scaler.ToPx (SscSwitchesView::kWidthDip), scaler.ToPx (SscSwitchesView::kHeightDip)), scaler);
    y += scaler.ToPx (SscSwitchesView::kHeightDip) + sectionGap;

    // Below everything, because it restores the whole page.
    m_restore.SetLabel (L"Restore defaults");
    m_restore.Layout   (MakeRect (controlsX, y, scaler.ToPx (s_kRestoreWidthDp), rowHeight));
    m_restore.SetDpi   (dpi);

    DxuiPanel::SetBounds (rect);
    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::GetMeasuringRenderer
//
//  The popup host's renderer, fetched on every call because the host
//  rebuilds it after a device loss. Null before the host creates one.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiTextRenderer * SerialCardPage::GetMeasuringRenderer() const
{
    return (m_popupHost != nullptr) ? m_popupHost->GetTextRenderer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::GetLabelColumnPx
//
//  Wide enough that no row's label wraps, with its info tip after it where
//  it has one, and then the gap before the controls. Never narrower than the
//  design width, so the page lines up with the Printing page.
//
////////////////////////////////////////////////////////////////////////////////

int SerialCardPage::GetLabelColumnPx (const DxuiDpiScaler & scaler) const
{
    constexpr int  kLabelGapDp = 12;



    IDxuiTextRenderer  * text     = GetMeasuringRenderer();
    int                  tipReach = scaler.ToPx (SettingsInfoTip::kGapDp) + (int) std::ceil (scaler.ToPxf (DxuiInfoTip::kGlyphDip));
    int                  widest   = 0;
    DxuiLabel            probe;



    for (SscSetting setting : m_rowSettings)
    {
        int  width = 0;

        probe.SetText (SscSettingsModel::GetLabel (setting) + L":");
        width = SettingsInfoTip::MeasureLabelPx (probe, scaler, text);

        if (!SscSettingsModel::GetInfoTip (setting).empty())
        {
            width += tipReach;
        }

        widest = std::max (widest, width);
    }

    return std::max (scaler.ToPx (s_kLabelWidthDp), widest + scaler.ToPx (kLabelGapDp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::GetDropdownWidthPx
//
//  One width for every drop-down, so they line up: wide enough for the
//  longest item any of them lists, and never past `maxPx`.
//
////////////////////////////////////////////////////////////////////////////////

int SerialCardPage::GetDropdownWidthPx (const DxuiDpiScaler & scaler, int maxPx) const
{
    IDxuiTextRenderer  * text  = GetMeasuringRenderer();
    int                  width = scaler.ToPx (s_kDropdownWidthDp);



    for (int i = 0; text != nullptr && i < GetRowCount(); ++i)
    {
        width = std::max (width, (int) std::ceil (m_rowCombos[i].GetFitWidthPx (*text)));
    }

    if (text != nullptr)
    {
        width = std::max (width, (int) std::ceil (m_attachment.GetAttachFitWidthPx (*text)));
    }

    return std::min (width, std::max (maxPx, scaler.ToPx (s_kDropdownWidthDp)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::Rebuild
//
//  Syncs the widgets to the staged slot entry: the settings the mode offers,
//  their choices, and the connector's attachment.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::Rebuild()
{
    SscSwitches  switches = ReadStagedSwitches();



    m_rowSettings = SscSettingsModel::GetSettings (switches.GetMode());

    for (int i = 0; i < GetRowCount(); ++i)
    {
        SscSetting            setting = m_rowSettings[static_cast<size_t> (i)];
        std::vector<wstring>  items   = SscSettingsModel::GetChoices (setting);
        int                   choice  = SscSettingsModel::GetChoiceIndex (setting, switches);

        if (choice == SscSettingsModel::kCustom)
        {
            items.push_back (L"Custom");
            choice = static_cast<int> (items.size()) - 1;
        }

        m_rowCombos[i].SetItems    (items);
        m_rowCombos[i].SetSelected (choice);
        m_rowNoteText[i] = SscSettingsModel::GetNote (setting, switches);
    }

    for (int i = GetRowCount(); i < kMaxRows; ++i)
    {
        m_rowNoteText[i].clear();
    }

    SyncSwitches (switches);

    m_attachment.SetChoice ((m_state != nullptr) ? m_state->GetSerialAttachment (SerialPortId::Card) : SerialAttachmentChoice {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::OnRowPicked
//
//  A choice writes its switches into the staged entry. A mode change alters
//  the settings offered, so the rows are synced and laid out again.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::OnRowPicked (int row, int choice)
{
    SscSwitches  updated = ReadStagedSwitches();
    SscSetting   setting = SscSetting::Mode;
    bool         inRange = row >= 0 && row < GetRowCount();



    if (inRange)
    {
        setting = m_rowSettings[static_cast<size_t> (row)];

        SscSettingsModel::ApplyChoice (setting, choice, updated);
        StageSwitches (updated);

        if (setting == SscSetting::Mode)
        {
            Rebuild();
            Relayout();
        }

        MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::OnSwitchToggled
//
//  A switch flipped on the drawing is staged like a choice, and the settings
//  are synced to it; a flipped mode switch changes the settings offered.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::OnSwitchToggled (SscBank bank, int number)
{
    SscSwitches  updated = ReadStagedSwitches();



    updated.SetOn (bank, number, !updated.IsOn (bank, number));
    StageSwitches (updated);
    Rebuild();
    Relayout();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::OnRestoreDefaults
//
//  The switches as the card ships and nothing on the connector, staged like
//  any other change.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::OnRestoreDefaults()
{
    HRESULT  hr = S_OK;



    StageSwitches (SscSwitches::MakeDefault());

    if (m_state != nullptr)
    {
        hr = m_state->SetSerialAttachment (SerialPortId::Card, {});
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    Rebuild();
    Relayout();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::ReadStagedSwitches
//
//  The staged entry's switches, or the card's default for a bank the entry
//  does not list or that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

SscSwitches SerialCardPage::ReadStagedSwitches() const
{
    SscSwitches  switches = SscSwitches::MakeDefault();
    int          index    = (m_state != nullptr) ? m_state->FindSerialCard() : -1;
    bool         parsed   = false;



    if (index >= 0)
    {
        const HardwareEntry &  entry = m_state->GetHardware()[static_cast<size_t> (index)];

        parsed = switches.TryParseBankText (SscBank::Sw1, entry.switchesSw1);
        IGNORE_RETURN_VALUE (parsed, false);
        parsed = switches.TryParseBankText (SscBank::Sw2, entry.switchesSw2);
        IGNORE_RETURN_VALUE (parsed, false);
    }

    return switches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::StageSwitches
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::StageSwitches (const SscSwitches & switches)
{
    HRESULT  hr = S_OK;



    if (m_state != nullptr)
    {
        hr = m_state->SetSerialCardSwitches (switches.GetBankText (SscBank::Sw1),
                                             switches.GetBankText (SscBank::Sw2));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    SyncSwitches (switches);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::OnAttachmentChanged
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::OnAttachmentChanged (const SerialAttachmentChoice & choice)
{
    HRESULT  hr = S_OK;



    if (m_state != nullptr)
    {
        hr = m_state->SetSerialAttachment (SerialPortId::Card, choice);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    Relayout();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::SyncSwitches
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::SyncSwitches (const SscSwitches & switches)
{
    m_switchesView.SetSwitches (switches);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::Relayout
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::Relayout()
{
    if (m_hasLayout)
    {
        Layout (m_lastRect, m_lastScaler);
    }
}
