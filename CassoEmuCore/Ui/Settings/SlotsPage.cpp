#include "Pch.h"

#include "SlotsPage.h"

#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Ui/Settings/SlotCards.h"





static constexpr int  s_kPagePadDp      = 16;
static constexpr int  s_kListWidthDp    = 270;
static constexpr int  s_kPanelGapDp     = 16;
static constexpr int  s_kRowHeightDp    = 28;
static constexpr int  s_kRowGapDp       = 8;
static constexpr int  s_kLabelWidthDp   = 90;
static constexpr int  s_kDropdownWidthDp = 240;
static constexpr int  s_kListHeightDp   = 260;
static constexpr int  s_kButtonWidthDp  = 90;
static constexpr int  s_kIndentDp       = 20;

static constexpr wchar_t  s_kResetNote[] = L"Takes effect when the machine resets.";





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SlotsPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::Widen
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SlotsPage::Widen (const std::string & narrow)
{
    return TextEncoding::Utf8ToWide (narrow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::GetCardName
//
//  The card's name without the "Slot N: " its display name starts with.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SlotsPage::GetCardName (const HardwareEntry & entry)
{
    std::string  name  = entry.displayName;
    size_t       colon = name.find (": ");



    return Widen ((colon != std::string::npos) ? name.substr (colon + 2) : name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::MakeSlotLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SlotsPage::MakeSlotLabel (const std::vector<HardwareEntry> & entries, int slot)
{
    int  found = SlotCards::FindSlot (entries, slot);



    return std::format (L"Slot {}: {}", slot, (found >= 0) ? GetCardName (entries[(size_t) found]) : std::wstring (L"Empty"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::SlotsPage
//
////////////////////////////////////////////////////////////////////////////////

SlotsPage::SlotsPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    std::vector<DxuiListView::Column>  columns (1);



    Adopt (m_list);
    Adopt (m_cardLabel);
    Adopt (m_cardDropdown);
    Adopt (m_promptLabel);
    Adopt (m_replaceButton);
    Adopt (m_cancelButton);
    Adopt (m_firmware);
    Adopt (m_drivesHeading);

    for (size_t i = 0; i < kMaxPorts; i++)
    {
        Adopt (m_portLabels[i]);
        Adopt (m_portDropdowns[i]);
        Adopt (m_portToggles[i]);
        Adopt (m_timingBoxes[i]);

        m_timingBoxes[i].SetLabel (L"Real spin-up and seek times");

        m_portDropdowns[i].SetSelect ([this, i] (int choice)   { ChoosePortDrive (i, choice); });
        m_portToggles[i].SetOnChange ([this, i] (bool checked) { SetChainDrive (i, checked);  });
        m_timingBoxes[i].SetOnChange ([this, i] (bool checked)
        {
            int  entry = GetSelectedEntry();

            if (entry >= 0)
            {
                OnEdited (m_state->SetPortRealTiming ((size_t) entry, i, checked));
            }
        });
    }

    Adopt (m_protectLabel);

    for (DxuiCheckbox & checkbox : m_writeProtect)
    {
        Adopt (checkbox);
    }

    Adopt (m_serial);

    columns[0].stretch = true;
    m_list.SetColumns             (std::move (columns));
    m_list.SetShowHeader          (false);
    m_list.SetAlwaysShowSelection (true);
    m_list.SetOnSelectionChanged  ([this] (int row)
    {
        if (row >= 0 && row + kFirstSlot != m_selectedSlot)
        {
            SelectSlot (row + kFirstSlot);
        }
    });

    m_cardLabel.SetText         (L"Card:");
    m_drivesHeading.SetText     (L"Drives");
    m_drivesHeading.SetTextRole (DxuiTextRole::Heading);
    m_protectLabel.SetText      (L"Write protect:");
    m_firmware.SetLabel         (L"Casso's fast firmware");
    m_cancelButton.SetLabel     (L"Cancel");
    m_writeProtect[0].SetLabel  (L"Drive 1");
    m_writeProtect[1].SetLabel  (L"Drive 2");

    m_replaceButton.SetOnClick ([this] () { ConfirmReplace(); });
    m_cancelButton.SetOnClick  ([this] () { CancelReplace();  });
    m_cardDropdown.SetSelect   ([this] (int index) { ChooseCard (index); });

    m_firmware.SetOnChange ([this] (bool checked)
    {
        int  entry = GetSelectedEntry();

        if (entry >= 0)
        {
            OnEdited (m_state->SetCassoFirmware ((size_t) entry, checked));
        }
    });

    m_writeProtect[0].SetOnChange ([this] (bool checked) { if (m_state != nullptr) { m_state->SetWriteProtect (0, checked); } });
    m_writeProtect[1].SetOnChange ([this] (bool checked) { if (m_state != nullptr) { m_state->SetWriteProtect (1, checked); } });

    m_cardDropdown.SetOnHighlightChange ([this] (int index)
    {
        bool  isBlocked = index >= 0 && index < (int) m_cardChoices.size() && !m_cardChoices[(size_t) index].allowed;

        if (isBlocked && !m_replacePending)
        {
            m_prompt = m_cardChoices[(size_t) index].tip;
            m_promptLabel.SetText (m_prompt);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::SetState (SettingsPanelState * state)
{
    m_state = state;
    m_serial.SetState (state);
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::SetPopupHost (DxuiHwndSource * host)
{
    m_cardDropdown.SetPopupHost (host);
    m_serial.SetPopupHost       (host);

    for (DxuiComboBox & dropdown : m_portDropdowns)
    {
        dropdown.SetPopupHost (host);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::Rebuild
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::Rebuild()
{
    RebuildList();
    RebuildDetail();
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::RebuildList
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::RebuildList()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    for (int slot = kFirstSlot; slot <= kLastSlot; slot++)
    {
        DxuiListView::Cell  cell;



        cell.text = (m_state != nullptr) ? MakeSlotLabel (m_state->GetHardware(), slot) : std::format (L"Slot {}: Empty", slot);
        cell.dim  = m_state == nullptr || SlotCards::FindSlot (m_state->GetHardware(), slot) < 0;

        rows.push_back ({ std::move (cell) });
    }

    m_list.SetRows        (std::move (rows));
    m_list.SetSelectedRow (m_selectedSlot - kFirstSlot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::RebuildDetail
//
//  The selected slot's card and the options that card has. A card the
//  machine requires can't be changed; the prompt has the reason. A chain
//  card's drives are each connected or not; every other storage card has a
//  kind of drive, or none, on each port.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::RebuildDetail()
{
    std::vector<std::wstring>    cardItems;
    std::vector<bool>            cardEnabled;
    int                          cardIndex = 0;
    int                          entry     = GetSelectedEntry();
    const HardwareEntry        * selected  = nullptr;
    const BlockCardSpec        * card      = nullptr;
    bool                         isLocked  = false;



    m_portRows = 0;
    m_isChain  = false;

    if (m_state == nullptr)
    {
        return;
    }

    selected      = (entry >= 0) ? &m_state->GetHardware()[(size_t) entry] : nullptr;
    card          = (selected != nullptr) ? BlockDeviceCatalog::FindCard (selected->type) : nullptr;
    isLocked      = selected != nullptr && selected->capability != CapabilityFlag::Optional;
    m_cardChoices = m_state->GetCardChoices();

    // A card in the slot that is not a storage card is listed last, as the
    // slot's card.
    if (selected != nullptr && card == nullptr)
    {
        SlotCardChoice  current;



        current.device = selected->type;
        current.label  = GetCardName (*selected);
        m_cardChoices.push_back (std::move (current));
    }

    for (size_t i = 0; i < m_cardChoices.size(); i++)
    {
        cardItems.push_back   (m_cardChoices[i].label);
        cardEnabled.push_back (m_cardChoices[i].allowed);

        if (selected != nullptr && m_cardChoices[i].device == selected->type)
        {
            cardIndex = (int) i;
        }
    }

    m_cardDropdown.SetItems        (cardItems);
    m_cardDropdown.SetItemsEnabled (cardEnabled);
    m_cardDropdown.SetSelected     (cardIndex);
    m_cardDropdown.SetEnabled      (!isLocked && !m_replacePending);

    m_promptLabel.SetText ((isLocked && m_prompt.empty()) ? Widen (selected->lockReason) : m_prompt);

    if (card != nullptr)
    {
        std::vector<std::wstring>  driveItems = { L"Not connected" };
        bool                       isScsi     = std::string (card->device) == kpszAppleScsiCardDevice;



        for (const char * device : card->drives)
        {
            const BlockDriveSpec  * drive = BlockDeviceCatalog::FindDrive (device);

            driveItems.push_back (Widen ((drive != nullptr) ? drive->label : device));
        }

        m_firmware.SetChecked (selected->cassoFirmware);
        m_isChain  = card->isChain;
        m_portRows = std::min ((size_t) card->portCount, kMaxPorts);

        for (size_t port = 0; port < m_portRows; port++)
        {
            std::string             device = (port < selected->ports.size()) ? selected->ports[port] : std::string();
            const BlockDriveSpec  * drive  = BlockDeviceCatalog::FindDrive (device);
            int                     choice = 0;

            for (size_t i = 0; i < card->drives.size(); i++)
            {
                choice = (device == card->drives[i]) ? (int) i + 1 : choice;
            }

            m_portLabels[port].SetText        (isScsi ? std::format (L"SCSI ID {}:", port) : std::format (L"Drive {}:", port + 1));
            m_portDropdowns[port].SetItems    (driveItems);
            m_portDropdowns[port].SetSelected (choice);
            m_portToggles[port].SetChecked    (drive != nullptr);
            m_portToggles[port].SetLabel      ((drive != nullptr) ? L"Connected" : L"Not connected");
            m_timingBoxes[port].SetChecked    (port < selected->portRealTiming.size() && selected->portRealTiming[port]);
            m_timingBoxes[port].SetVisible    (drive != nullptr && drive->hasRealTiming);
        }
    }

    m_writeProtect[0].SetChecked (m_state->GetPrefs().writeProtect[0]);
    m_writeProtect[1].SetChecked (m_state->GetPrefs().writeProtect[1]);

    if (IsSerialSelected())
    {
        m_serial.Rebuild();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    m_baseRect = rect;
    m_scaler   = scaler;

    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::Relayout
//
//  The list at the left, the selected slot's card at the top of the right,
//  and below it the card's options. Controls a card doesn't have are hidden.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::Relayout()
{
    const DxuiDpiScaler   & s          = m_scaler;
    const RECT            & rect       = m_baseRect;
    UINT                    dpi        = s.GetDpi();
    int                     pad        = s.ToPx (s_kPagePadDp);
    int                     rowH       = s.ToPx (s_kRowHeightDp);
    int                     gap        = s.ToPx (s_kRowGapDp);
    int                     listW      = s.ToPx (s_kListWidthDp);
    int                     labelW     = s.ToPx (s_kLabelWidthDp);
    int                     buttonW    = s.ToPx (s_kButtonWidthDp);
    int                     dropW      = s.ToPx (s_kDropdownWidthDp);
    int                     indent     = s.ToPx (s_kIndentDp);
    int                     x          = rect.left + pad + listW + s.ToPx (s_kPanelGapDp);
    int                     right      = rect.right - pad;
    int                     y          = rect.top + pad;
    int                     controlsX  = x + labelW;
    int                     entry      = GetSelectedEntry();
    const HardwareEntry   * selected   = (m_state != nullptr && entry >= 0) ? &m_state->GetHardware()[(size_t) entry] : nullptr;
    const BlockCardSpec   * card       = (selected != nullptr) ? BlockDeviceCatalog::FindCard (selected->type) : nullptr;
    bool                    showBlock  = card != nullptr && !m_replacePending;
    bool                    showDisk   = IsDiskIiSelected() && !m_replacePending;
    bool                    showSerial = IsSerialSelected() && !m_replacePending;
    int                     listBottom = 0;



    if (rect.right <= rect.left)
    {
        return;
    }

    m_list.SetDpi  (dpi);
    m_list.SetRect (MakeRect (rect.left + pad, rect.top + pad, listW, s.ToPx (s_kListHeightDp)));
    listBottom = m_list.GetBounds().bottom;

    m_cardLabel.SetDpi     (dpi);
    m_cardDropdown.SetDpi  (dpi);
    m_cardLabel.SetRect    (MakeRect (x, y, labelW, rowH));
    m_cardDropdown.SetRect (MakeRect (controlsX, y, std::min (dropW, right - controlsX), rowH));
    y += rowH + gap;

    m_promptLabel.SetDpi     (dpi);
    m_promptLabel.SetVisible (!m_promptLabel.GetText().empty());
    m_promptLabel.SetRect    (MakeRect (controlsX, y, right - controlsX, rowH));

    if (m_promptLabel.IsVisible())
    {
        y += rowH + gap;
    }

    m_replaceButton.SetDpi     (dpi);
    m_cancelButton.SetDpi      (dpi);
    m_replaceButton.SetVisible (m_replacePending);
    m_cancelButton.SetVisible  (m_replacePending);
    m_replaceButton.Layout     (MakeRect (controlsX, y, buttonW, rowH));
    m_cancelButton.Layout      (MakeRect (controlsX + buttonW + gap, y, buttonW, rowH));

    if (m_replacePending)
    {
        y += rowH + gap;
    }

    // A storage card: its firmware switch and the drives connected to it.
    m_firmware.SetDpi     (dpi);
    m_firmware.SetVisible (showBlock && card->hasCassoFirmware);
    m_firmware.SetRect    (MakeRect (controlsX, y, right - controlsX, rowH));

    if (m_firmware.IsVisible())
    {
        y += rowH + gap;
    }

    m_drivesHeading.SetDpi     (dpi);
    m_drivesHeading.SetVisible (showBlock);
    m_drivesHeading.SetRect    (MakeRect (x, y, right - x, rowH));

    if (showBlock)
    {
        y += rowH + gap;
    }

    for (size_t port = 0; port < kMaxPorts; port++)
    {
        bool  isShown = showBlock && port < m_portRows;

        m_portLabels[port].SetDpi        (dpi);
        m_portDropdowns[port].SetDpi     (dpi);
        m_portToggles[port].SetDpi       (dpi);
        m_timingBoxes[port].SetDpi       (dpi);
        m_portLabels[port].SetVisible    (isShown);
        m_portDropdowns[port].SetVisible (isShown && !m_isChain);
        m_portToggles[port].SetVisible   (isShown && m_isChain);

        if (!isShown)
        {
            m_timingBoxes[port].SetVisible (false);
            continue;
        }

        m_portLabels[port].SetRect    (MakeRect (x, y, labelW, rowH));
        m_portDropdowns[port].SetRect (MakeRect (controlsX, y, std::min (dropW, right - controlsX), rowH));
        m_portToggles[port].SetRect   (MakeRect (controlsX, y, right - controlsX, rowH));
        y += rowH + gap;

        if (m_timingBoxes[port].IsVisible())
        {
            m_timingBoxes[port].SetRect (MakeRect (controlsX + indent, y, right - controlsX - indent, rowH));
            y += rowH + gap;
        }
    }

    // The Disk II controller: each drive's write protection.
    m_protectLabel.SetDpi     (dpi);
    m_protectLabel.SetVisible (showDisk);
    m_protectLabel.SetRect    (MakeRect (x, y, labelW + indent, rowH));

    for (size_t drive = 0; drive < m_writeProtect.size(); drive++)
    {
        m_writeProtect[drive].SetDpi     (dpi);
        m_writeProtect[drive].SetVisible (showDisk);
        m_writeProtect[drive].SetRect    (MakeRect (controlsX + indent + (int) drive * buttonW, y, buttonW, rowH));
    }

    if (showDisk)
    {
        y += rowH + gap;
    }

    // The Super Serial Card: its switches and connection, under the card.
    m_serial.SetVisible (showSerial);

    if (showSerial)
    {
        RECT  serialRect = { x - pad, y - pad, rect.right, rect.bottom };

        m_serial.Layout (serialRect, s);
        y += m_serial.GetContentHeightPx();
    }

    DxuiPanel::SetBounds (rect);
    SetContentHeightPx (std::max (listBottom, y) + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::GetSelectedEntry
//
//  The hardware entry of the card in the selected slot, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SlotsPage::GetSelectedEntry() const
{
    return (m_state != nullptr) ? SlotCards::FindSlot (m_state->GetHardware(), m_selectedSlot) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::IsDiskIiSelected
//
////////////////////////////////////////////////////////////////////////////////

bool SlotsPage::IsDiskIiSelected() const
{
    int  entry = GetSelectedEntry();



    return entry >= 0 && m_state->GetHardware()[(size_t) entry].type == "disk-ii";
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::IsSerialSelected
//
////////////////////////////////////////////////////////////////////////////////

bool SlotsPage::IsSerialSelected() const
{
    int  entry = GetSelectedEntry();



    return entry >= 0 && m_state->GetHardware()[(size_t) entry].type == kpszSuperSerialCardDevice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::SelectSlot
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::SelectSlot (int slot)
{
    m_selectedSlot   = std::clamp (slot, kFirstSlot, kLastSlot);
    m_replacePending = false;
    m_prompt.clear();
    m_pendingDevice.clear();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::ChooseCard
//
//  A card the machine can't host is not put in; the prompt lists the
//  machines that can. A card in a filled slot is replaced only after a
//  confirmation.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::ChooseCard (int choice)
{
    int                   entry   = GetSelectedEntry();
    std::string           current = (entry >= 0) ? m_state->GetHardware()[(size_t) entry].type : std::string();
    std::wstring          label;



    if (m_state == nullptr || choice < 0 || choice >= (int) m_cardChoices.size())
    {
        return;
    }

    if (!m_cardChoices[(size_t) choice].allowed)
    {
        m_prompt = m_cardChoices[(size_t) choice].tip;
        Rebuild();
        return;
    }

    if (m_cardChoices[(size_t) choice].device == current)
    {
        return;
    }

    if (current.empty())
    {
        ApplyCard (m_cardChoices[(size_t) choice].device);
        return;
    }

    label            = GetCardName (m_state->GetHardware()[(size_t) entry]);
    m_pendingDevice  = m_cardChoices[(size_t) choice].device;
    m_replacePending = true;

    if (m_pendingDevice.empty())
    {
        m_prompt = std::format (L"Take the {} out of slot {}?", label, m_selectedSlot);
        m_replaceButton.SetLabel (L"Take out");
    }
    else
    {
        m_prompt = std::format (L"Replace the {} in slot {} with the {}?", label, m_selectedSlot, m_cardChoices[(size_t) choice].label);
        m_replaceButton.SetLabel (L"Replace");
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::ConfirmReplace
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::ConfirmReplace()
{
    if (m_replacePending)
    {
        m_replacePending = false;
        ApplyCard (m_pendingDevice);
        m_pendingDevice.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::CancelReplace
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::CancelReplace()
{
    m_replacePending = false;
    m_pendingDevice.clear();
    m_prompt.clear();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::ApplyCard
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::ApplyCard (const std::string & device)
{
    std::wstring  error;
    HRESULT       hr = m_state->SetSlotCard (m_selectedSlot, device, error);



    OnEdited (hr, error);

    if (SUCCEEDED (hr) && !device.empty() && m_onCardChosen)
    {
        m_onCardChosen (device);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::ChoosePortDrive
//
//  Choice 0 is no drive on the port; the others are the card's kinds
//  of drive, in the order the dropdown lists them.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::ChoosePortDrive (size_t port, int choice)
{
    int                    entry  = GetSelectedEntry();
    const BlockCardSpec  * card   = (entry >= 0) ? BlockDeviceCatalog::FindCard (m_state->GetHardware()[(size_t) entry].type) : nullptr;
    std::string            device;



    if (card == nullptr || choice < 0 || (size_t) choice > card->drives.size())
    {
        return;
    }

    device = (choice > 0) ? std::string (card->drives[(size_t) choice - 1]) : std::string();

    OnEdited (m_state->SetPortDrive ((size_t) entry, port, device));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::SetChainDrive
//
//  A chain's drives are connected in order, so connecting a drive connects
//  the ones ahead of it, and disconnecting one disconnects the ones after.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::SetChainDrive (size_t port, bool connected)
{
    int  entry = GetSelectedEntry();



    if (entry >= 0)
    {
        OnEdited (m_state->SetChainCount ((size_t) entry, connected ? port + 1 : port));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::OnEdited
//
//  The prompt after an edit: the reset note when it took, else the error.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::OnEdited (HRESULT hr, const std::wstring & error)
{
    m_prompt = SUCCEEDED (hr) ? std::wstring (s_kResetNote) : error;

    Rebuild();
}
