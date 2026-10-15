#include "Pch.h"

#include "Ui/Settings/SerialCardPage.h"
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

static constexpr const wchar_t *  s_kAttachmentLabels[] =
{
    L"Nothing",
    L"Loopback plug",
    L"File (receives what the card sends)",
    L"File (sent into the card)",
};





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
        Adopt (m_rowCombos[i]);

        // Wired once, by row: Rebuild changes the setting in a row but never
        // replaces a handler, which could be running when it is called.
        m_rowCombos[i].SetSelect ([this, i] (int picked) { OnRowPicked (i, picked); });
    }

    Adopt (m_readoutSw1);
    Adopt (m_readoutSw2);
    Adopt (m_attachLabel);
    Adopt (m_attach);
    Adopt (m_pathLabel);
    Adopt (m_pathValue);
    Adopt (m_browse);

    m_attach.SetItems   ({ std::begin (s_kAttachmentLabels), std::end (s_kAttachmentLabels) });
    m_attach.SetSelect  ([this] (int picked) { OnAttachPicked (picked); });
    m_browse.SetOnClick ([this] () { OnBrowse(); });
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
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::SetPopupHost (DxuiHwndSource * host)
{
    for (int i = 0; i < kMaxRows; ++i)
    {
        m_rowCombos[i].SetPopupHost (host);
    }

    m_attach.SetPopupHost (host);
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
//  SerialCardPage::GetReadoutText
//
////////////////////////////////////////////////////////////////////////////////

const std::wstring & SerialCardPage::GetReadoutText (SscBank bank) const
{
    return (bank == SscBank::Sw1) ? m_readoutTextSw1 : m_readoutTextSw2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::Layout
//
//  The firmware banner first, since its height follows its text; then the
//  switch settings for the selected mode, the switch readout, and the serial
//  connector. Rows the mode does not use are hidden.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi          = scaler.GetDpi();
    int   pad          = scaler.ToPx (s_kPagePadDp);
    int   rowHeight    = scaler.ToPx (s_kRowHeightDp);
    int   labelWidth   = scaler.ToPx (s_kLabelWidthDp);
    int   dropWidth    = scaler.ToPx (s_kDropdownWidthDp);
    int   buttonWidth  = scaler.ToPx (s_kButtonWidthDp);
    int   sectionGap   = scaler.ToPx (s_kSectionGapDp);
    int   rowGap       = scaler.ToPx (s_kRowGapDp);
    int   x            = rect.left + pad;
    int   y            = rect.top  + pad;
    int   controlsX    = x + labelWidth;
    int   bannerWidth  = (rect.right - rect.left) - pad * 2;
    int   bannerHeight = static_cast<int> (std::ceil (m_firmwareBanner.GetPreferredHeightPx (static_cast<float> (bannerWidth), scaler)));
    int   fileChoice   = m_attach.GetSelectedIndex();
    bool  showPath     = fileChoice == kFileOutChoice || fileChoice == kFileInChoice;



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

    m_switchesHeading.SetText (L"Switches");
    m_switchesHeading.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_switchesHeading.SetDpi  (dpi);
    y += rowHeight;

    for (int i = 0; i < kMaxRows; ++i)
    {
        bool  used = i < GetRowCount();

        m_rowLabels[i].SetVisible (used);
        m_rowCombos[i].SetVisible (used);

        if (used)
        {
            m_rowLabels[i].SetText (SscSettingsModel::GetLabel (m_rowSettings[static_cast<size_t> (i)]) + L":");
            m_rowLabels[i].SetRect (MakeRect (x, y, labelWidth, rowHeight));
            m_rowCombos[i].SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
            y += rowHeight + rowGap;
        }

        m_rowLabels[i].SetDpi (dpi);
        m_rowCombos[i].SetDpi (dpi);
    }

    m_readoutSw1.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_readoutSw1.SetDpi  (dpi);
    y += rowHeight;
    m_readoutSw2.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_readoutSw2.SetDpi  (dpi);
    y += rowHeight + sectionGap;

    m_attachLabel.SetText (L"Serial connector:");
    m_attachLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_attachLabel.SetDpi  (dpi);
    m_attach.SetRect      (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_attach.SetDpi       (dpi);
    y += rowHeight + rowGap;

    m_pathLabel.SetVisible (showPath);
    m_pathValue.SetVisible (showPath);
    m_browse.SetVisible    (showPath);
    m_pathLabel.SetText    (L"File:");
    m_pathLabel.SetRect    (MakeRect (x, y, labelWidth, rowHeight));
    m_pathLabel.SetDpi     (dpi);
    m_pathValue.SetText    (m_path.empty() ? std::wstring (L"(none chosen)") : TextEncoding::NarrowToWide (m_path));
    m_pathValue.SetRect    (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_pathValue.SetDpi     (dpi);
    m_browse.SetLabel      (L"Browse...");
    m_browse.Layout        (MakeRect (controlsX + dropWidth + rowGap, y, buttonWidth, rowHeight));
    m_browse.SetDpi        (dpi);

    DxuiPanel::SetBounds (rect);
    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
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
    int          index    = (m_state != nullptr) ? m_state->FindSerialCard() : -1;
    int          attach   = 0;



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
    }

    SyncReadout (switches);

    if (index >= 0)
    {
        const HardwareEntry &  entry  = m_state->GetHardware()[static_cast<size_t> (index)];
        std::string            device = entry.ports.empty() ? std::string() : entry.ports.front();

        for (int i = 0; i < static_cast<int> (std::size (kAttachmentDevices)); ++i)
        {
            if (device == kAttachmentDevices[i])
            {
                attach = i;
            }
        }

        m_path = entry.portPath;
    }

    m_attach.SetSelected (attach);
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
//  SerialCardPage::OnAttachPicked
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::OnAttachPicked (int choice)
{
    StageAttachment (choice, m_path);
    Relayout();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::OnBrowse
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::OnBrowse()
{
    int          choice = m_attach.GetSelectedIndex();
    std::string  path   = m_path;



    if (m_onBrowse && m_onBrowse (choice == kFileOutChoice, path))
    {
        StageAttachment (choice, path);
        Relayout();
        MarkDirty();
    }
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

    SyncReadout (switches);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::StageAttachment
//
//  Only the file attachments have a path; the others stage it empty.
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::StageAttachment (int choice, const std::string & path)
{
    HRESULT  hr     = S_OK;
    bool     isFile = choice == kFileOutChoice || choice == kFileInChoice;
    bool     valid  = choice >= 0 && choice < static_cast<int> (std::size (kAttachmentDevices));



    m_path = isFile ? path : std::string();

    if (m_state != nullptr && valid)
    {
        hr = m_state->SetSerialCardAttachment (kAttachmentDevices[choice], m_path);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage::SyncReadout
//
////////////////////////////////////////////////////////////////////////////////

void SerialCardPage::SyncReadout (const SscSwitches & switches)
{
    m_readoutTextSw1 = SscSettingsModel::GetReadout (switches, SscBank::Sw1);
    m_readoutTextSw2 = SscSettingsModel::GetReadout (switches, SscBank::Sw2);

    m_readoutSw1.SetText (m_readoutTextSw1);
    m_readoutSw2.SetText (m_readoutTextSw2);
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
