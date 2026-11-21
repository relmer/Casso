#include "Pch.h"

#include "SlotsPage.h"

#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Ui/Settings/HardwarePage.h"
#include "Ui/Settings/SlotCards.h"





static constexpr int  s_kPagePadDp      = 16;
static constexpr int  s_kListWidthDp    = 270;
static constexpr int  s_kPanelGapDp     = 16;
static constexpr int  s_kRowHeightDp    = 28;
static constexpr int  s_kRowGapDp       = 8;
static constexpr int  s_kLabelWidthDp   = 90;
static constexpr int  s_kDropdownWidthDp = 260;
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
    Adopt (m_list);
    Adopt (m_cardLabel);
    Adopt (m_cardDropdown);
    Adopt (m_promptLabel);
    Adopt (m_replaceButton);
    Adopt (m_cancelButton);
    Adopt (m_firmware);
    Adopt (m_drivesHeading);

    for (size_t i = 0; i < kMaxDrives; i++)
    {
        Adopt (m_driveLabels[i]);
        Adopt (m_changeButtons[i]);
        Adopt (m_ejectButtons[i]);
        Adopt (m_removeButtons[i]);
        Adopt (m_timingBoxes[i]);

        m_changeButtons[i].SetLabel (L"Change...");
        m_ejectButtons[i].SetLabel  (L"Eject");
        m_removeButtons[i].SetLabel (L"Remove");
        m_timingBoxes[i].SetLabel   (L"Real spin-up and seek times");

        m_changeButtons[i].SetOnClick ([this, i] () { AttachImage (i); });
        m_ejectButtons[i].SetOnClick  ([this, i] () { EjectImage (i);  });
        m_removeButtons[i].SetOnClick ([this, i] () { RemoveDrive (i); });
        m_timingBoxes[i].SetOnChange  ([this, i] (bool checked)
        {
            int  entry = GetSelectedEntry();

            if (entry >= 0)
            {
                OnEdited (m_state->SetPortRealTiming ((size_t) entry, i, checked));
            }
        });
    }

    Adopt (m_addButton);
    Adopt (m_protectLabel);

    for (DxuiCheckbox & checkbox : m_writeProtect)
    {
        Adopt (checkbox);
    }

    Adopt (m_serial);

    m_list.SetShowCheckboxes (false);
    m_list.SetHorizontalScrollEnabled (false);
    m_list.SetOnSelect ([this] (const std::wstring & id)
    {
        int  slot = 0;

        if (swscanf_s (id.c_str(), L"slot:%d", &slot) == 1 && slot != m_selectedSlot)
        {
            SelectSlot (slot);
        }
    });

    m_cardLabel.SetText       (L"Card:");
    m_drivesHeading.SetText   (L"Drives");
    m_drivesHeading.SetTextRole (DxuiTextRole::Heading);
    m_protectLabel.SetText    (L"Write protect:");
    m_firmware.SetLabel       (L"Casso's fast firmware");
    m_addButton.SetLabel      (L"Add drive");
    m_cancelButton.SetLabel   (L"Cancel");
    m_writeProtect[0].SetLabel (L"Drive 1");
    m_writeProtect[1].SetLabel (L"Drive 2");

    m_addButton.SetOnClick     ([this] () { AddDrive();       });
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
    std::vector<DxuiTreeNode>  nodes;



    for (int slot = kFirstSlot; slot <= kLastSlot; slot++)
    {
        DxuiTreeNode  node;



        node.id     = std::format (L"slot:{}", slot);
        node.label  = (m_state != nullptr) ? MakeSlotLabel (m_state->GetHardware(), slot) : std::format (L"Slot {}: Empty", slot);
        node.dimmed = m_state == nullptr || SlotCards::FindSlot (m_state->GetHardware(), slot) < 0;

        nodes.push_back (std::move (node));
    }

    m_list.SetNodes  (std::move (nodes));
    m_list.SelectRow (m_selectedSlot - kFirstSlot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::RebuildDetail
//
//  The selected slot's card and the options that card has. A card the
//  machine requires can't be changed; the prompt has the reason.
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



    m_driveRows = 0;

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

    if (isLocked && m_prompt.empty())
    {
        m_promptLabel.SetText (Widen (selected->lockReason));
    }
    else
    {
        m_promptLabel.SetText (m_prompt);
    }

    if (card != nullptr)
    {
        m_firmware.SetChecked (selected->cassoFirmware);
        m_driveRows = std::min (selected->ports.size(), kMaxDrives);

        for (size_t port = 0; port < m_driveRows; port++)
        {
            const BlockDriveSpec  * drive    = BlockDeviceCatalog::FindDrive (selected->ports[port]);
            bool                    hasImage = port < selected->portImages.size() && !selected->portImages[port].empty();

            m_driveLabels[port].SetText    (HardwarePage::MakeDriveLabel (*selected, port));
            m_ejectButtons[port].SetEnabled (hasImage);
            m_timingBoxes[port].SetChecked (port < selected->portRealTiming.size() && selected->portRealTiming[port]);
            m_timingBoxes[port].SetVisible (drive != nullptr && drive->hasRealTiming);
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
    bool                    isChain    = card != nullptr && card->isChain;
    int                     listBottom = 0;



    if (rect.right <= rect.left)
    {
        return;
    }

    m_list.SetDpi  (dpi);
    m_list.SetRect (MakeRect (rect.left + pad, rect.top + pad, listW, (kLastSlot - kFirstSlot + 2) * m_list.GetRowHeight()));
    listBottom = m_list.GetBounds().bottom;

    m_cardLabel.SetDpi       (dpi);
    m_cardDropdown.SetDpi    (dpi);
    m_cardLabel.SetRect      (MakeRect (x, y, labelW, rowH));
    m_cardDropdown.SetRect   (MakeRect (controlsX, y, std::min (s.ToPx (s_kDropdownWidthDp), right - controlsX), rowH));
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

    // A storage card: its firmware switch and its drives.
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

    for (size_t port = 0; port < kMaxDrives; port++)
    {
        bool  show    = showBlock && port < m_driveRows;
        int   buttonX = x + indent;

        m_driveLabels[port].SetDpi       (dpi);
        m_changeButtons[port].SetDpi     (dpi);
        m_ejectButtons[port].SetDpi      (dpi);
        m_removeButtons[port].SetDpi     (dpi);
        m_timingBoxes[port].SetDpi       (dpi);
        m_driveLabels[port].SetVisible   (show);
        m_changeButtons[port].SetVisible (show);
        m_ejectButtons[port].SetVisible  (show);
        m_removeButtons[port].SetVisible (show && isChain);

        if (!show)
        {
            m_timingBoxes[port].SetVisible (false);
            continue;
        }

        // The drive and its image on one line, its buttons under it.
        m_driveLabels[port].SetRect     (MakeRect (x, y, right - x, rowH));
        y += rowH + gap;

        m_changeButtons[port].Layout    (MakeRect (buttonX, y, buttonW, rowH));
        m_ejectButtons[port].Layout     (MakeRect (buttonX + buttonW + gap, y, buttonW, rowH));
        m_removeButtons[port].Layout    (MakeRect (buttonX + 2 * (buttonW + gap), y, buttonW, rowH));
        y += rowH + gap;

        if (m_timingBoxes[port].IsVisible())
        {
            m_timingBoxes[port].SetRect (MakeRect (x + indent, y, right - x - indent, rowH));
            y += rowH + gap;
        }
    }

    m_addButton.SetDpi     (dpi);
    m_addButton.SetVisible (showBlock && isChain);
    m_addButton.Layout     (MakeRect (x, y, buttonW, rowH));

    if (m_addButton.IsVisible())
    {
        y += rowH + gap;
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
//  SlotsPage::IsChainSelected
//
////////////////////////////////////////////////////////////////////////////////

bool SlotsPage::IsChainSelected() const
{
    int                    entry = GetSelectedEntry();
    const BlockCardSpec  * card  = (entry >= 0) ? BlockDeviceCatalog::FindCard (m_state->GetHardware()[(size_t) entry].type) : nullptr;



    return card != nullptr && card->isChain;
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
//  SlotsPage::AttachImage
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::AttachImage (size_t port)
{
    int          entry = GetSelectedEntry();
    std::string  path;
    std::string  device;



    if (entry < 0 || !m_onBrowseImage)
    {
        return;
    }

    {
        const HardwareEntry  & e = m_state->GetHardware()[(size_t) entry];

        path   = (port < e.portImages.size()) ? e.portImages[port] : std::string();
        device = (port < e.ports.size())      ? e.ports[port]      : std::string();
    }

    if (m_onBrowseImage (device, path))
    {
        OnEdited (m_state->SetPortImage ((size_t) entry, port, path));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::EjectImage
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::EjectImage (size_t port)
{
    int  entry = GetSelectedEntry();



    if (entry >= 0)
    {
        OnEdited (m_state->SetPortImage ((size_t) entry, port, std::string()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::AddDrive
//
//  At the chain's limit, the prompt has the limit.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::AddDrive()
{
    int           entry = GetSelectedEntry();
    std::wstring  error;



    if (entry >= 0 && IsChainSelected())
    {
        OnEdited (m_state->AddChainDrive ((size_t) entry, error), error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::RemoveDrive
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::RemoveDrive (size_t port)
{
    int  entry = GetSelectedEntry();



    if (entry >= 0 && IsChainSelected())
    {
        OnEdited (m_state->RemoveChainDrive ((size_t) entry, port));
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
