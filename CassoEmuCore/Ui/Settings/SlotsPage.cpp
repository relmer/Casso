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
static constexpr int  s_kIndentDp       = 20;
static constexpr int  s_kPanelRadiusDp  = 6;
static constexpr int  s_kRowInsetDp     = 2;





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
    Adopt (m_firmware);
    Adopt (m_firmwareNote);
    Adopt (m_countLabel);
    Adopt (m_countDropdown);

    for (size_t i = 0; i < kMaxPorts; i++)
    {
        Adopt (m_portLabels[i]);
        Adopt (m_portDropdowns[i]);
        Adopt (m_timingBoxes[i]);

        m_timingBoxes[i].SetLabel (L"Real spin-up and seek times");

        m_portDropdowns[i].SetSelect ([this, i] (int choice) { ChoosePortDrive (i, choice); });
        m_timingBoxes[i].SetOnChange ([this, i] (bool checked)
        {
            int  entry = GetSelectedEntry();

            if (entry >= 0)
            {
                OnEdited (m_state->SetPortRealTiming ((size_t) entry, i, checked));
            }
        });
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
    m_countLabel.SetText        (L"Drives:");
    m_firmware.SetSegments      ({ L"Casso's fast firmware", L"Apple's ROM" });
    m_firmwareNote.SetText      (L"Casso's firmware moves each block straight into memory, as a card with "
                                 L"DMA would, so disks load at once. Apple's ROM moves each byte through "
                                 L"the 6502, as slowly as the real card did.");
    m_firmwareNote.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_firmwareNote.SetTextRole  (DxuiTextRole::Muted);

    m_cardDropdown.SetSelect   ([this] (int index)    { ChooseCard (index);       });
    m_countDropdown.SetSelect  ([this] (int index)    { ChooseDriveCount (index); });
    m_firmware.SetOnChange     ([this] (int index)
    {
        // The pill slides on the sheet's ticks, which run fast only while a
        // page reports a control moving.
        if (m_onAnimate)
        {
            m_onAnimate();
        }

        SetAppleFirmware (index == 1);
    });

    m_cardDropdown.SetOnHighlightChange ([this] (int index)
    {
        bool  isBlocked = index >= 0 && index < (int) m_cardChoices.size() && !m_cardChoices[(size_t) index].allowed;

        if (isBlocked)
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
    m_cardDropdown.SetPopupHost  (host);
    m_countDropdown.SetPopupHost (host);
    m_serial.SetPopupHost        (host);

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
//  machine requires can't be changed; the prompt has the reason. A card
//  that takes one kind of drive, and the Disk II, have a number of drives
//  connected; a card with a choice of drives, or SCSI IDs, has a kind of
//  drive, or none, on each port.
//
//  The Disk II's first drive can't be disconnected, so its count starts at
//  one.
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



    m_portRows  = 0;
    m_isCounted = false;
    m_countBase = 0;

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
    m_cardDropdown.SetEnabled      (!isLocked);

    m_promptLabel.SetText ((isLocked && m_prompt.empty()) ? Widen (selected->lockReason) : m_prompt);

    if (IsDiskIiSelected())
    {
        bool  hasSecond = selected->ports.size() < 2 || !selected->ports[1].empty();

        m_countBase = 1;
        m_countDropdown.SetItems    ({ L"1", L"2" });
        m_countDropdown.SetSelected (hasSecond ? 1 : 0);
    }

    if (card != nullptr)
    {
        std::vector<std::wstring>  driveItems = { L"Not connected" };
        std::vector<std::wstring>  countItems;
        bool                       isScsi     = std::string (card->device) == kpszAppleScsiCardDevice;
        size_t                     connected  = 0;



        for (const char * device : card->drives)
        {
            const BlockDriveSpec  * drive = BlockDeviceCatalog::FindDrive (device);

            driveItems.push_back (Widen ((drive != nullptr) ? drive->label : device));
        }

        // Only when it differs: a selection set here jumps the pill, which
        // would cut short the slide a click started.
        if (m_firmware.GetSelected() != (selected->cassoFirmware ? 0 : 1))
        {
            m_firmware.SetSelected (selected->cassoFirmware ? 0 : 1);
        }

        m_isCounted = card->isChain || (card->drives.size() == 1 && !isScsi);
        m_portRows  = m_isCounted ? 0 : std::min ((size_t) card->portCount, kMaxPorts);

        for (const std::string & device : selected->ports)
        {
            connected += device.empty() ? 0 : 1;
        }

        for (int count = 0; count <= card->portCount; count++)
        {
            countItems.push_back (std::to_wstring (count));
        }

        if (m_isCounted)
        {
            m_countDropdown.SetItems    (countItems);
            m_countDropdown.SetSelected ((int) connected);
        }

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
            m_timingBoxes[port].SetChecked    (port < selected->portRealTiming.size() && selected->portRealTiming[port]);
            m_timingBoxes[port].SetVisible    (drive != nullptr && drive->hasRealTiming);
        }
    }

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
    int                     dropW      = s.ToPx (s_kDropdownWidthDp);
    int                     indent     = s.ToPx (s_kIndentDp);
    int                     x          = rect.left + pad + listW + s.ToPx (s_kPanelGapDp);
    int                     right      = rect.right - pad;
    int                     y          = rect.top + pad;
    int                     controlsX  = x + labelW;
    int                     colRight   = std::min (controlsX + dropW, right);
    int                     entry      = GetSelectedEntry();
    const HardwareEntry   * selected   = (m_state != nullptr && entry >= 0) ? &m_state->GetHardware()[(size_t) entry] : nullptr;
    const BlockCardSpec   * card       = (selected != nullptr) ? BlockDeviceCatalog::FindCard (selected->type) : nullptr;
    bool                    showBlock  = card != nullptr;
    bool                    showCount  = (showBlock && m_isCounted) || IsDiskIiSelected();
    bool                    showSerial = IsSerialSelected();
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
    m_cardDropdown.SetRect (MakeRect (controlsX, y, colRight - controlsX, rowH));
    y += rowH + gap;

    m_promptLabel.SetDpi     (dpi);
    m_promptLabel.SetVisible (!m_promptLabel.GetText().empty());
    m_promptLabel.SetRect    (MakeRect (controlsX, y, right - controlsX, rowH));

    if (m_promptLabel.IsVisible())
    {
        y += rowH + gap;
    }

    // A storage card's firmware, and the difference between the two under
    // the choice.
    {
        bool  showFirmware = showBlock && card->hasCassoFirmware;

        m_firmwareNote.SetDpi     (dpi);
        m_firmware.SetVisible     (showFirmware);
        m_firmwareNote.SetVisible (showFirmware);
        m_firmware.Layout         (MakeRect (x, y, colRight - x, rowH), s);

        if (showFirmware)
        {
            float    noteDip  = 0.0f;
            float    widthDip = (float) (colRight - x) * (float) DxuiDpiScaler::kBaseDpi / (float) dpi;
            int      noteH    = 3 * rowH;
            HRESULT  hrNote   = m_firmwareNote.MeasureWrappedDip (widthDip, noteDip);



            y += rowH + gap;

            if (SUCCEEDED (hrNote))
            {
                noteH = (int) std::ceil (s.ToPxf (noteDip));
            }

            m_firmwareNote.SetRect (MakeRect (x, y, colRight - x, noteH));
            y += noteH + 2 * gap;
        }
    }

    m_countLabel.SetDpi        (dpi);
    m_countDropdown.SetDpi     (dpi);
    m_countLabel.SetVisible    (showCount);
    m_countDropdown.SetVisible (showCount);
    m_countLabel.SetRect       (MakeRect (x, y, labelW, rowH));
    m_countDropdown.SetRect    (MakeRect (controlsX, y, colRight - controlsX, rowH));

    if (showCount)
    {
        y += rowH + gap;
    }

    for (size_t port = 0; port < kMaxPorts; port++)
    {
        bool  isShown = showBlock && port < m_portRows;

        m_portLabels[port].SetDpi        (dpi);
        m_portDropdowns[port].SetDpi     (dpi);
        m_timingBoxes[port].SetDpi       (dpi);
        m_portLabels[port].SetVisible    (isShown);
        m_portDropdowns[port].SetVisible (isShown);

        if (!isShown)
        {
            m_timingBoxes[port].SetVisible (false);
            continue;
        }

        m_portLabels[port].SetRect    (MakeRect (x, y, labelW, rowH));
        m_portDropdowns[port].SetRect (MakeRect (controlsX, y, colRight - controlsX, rowH));
        y += rowH + gap;

        if (m_timingBoxes[port].IsVisible())
        {
            m_timingBoxes[port].SetRect (MakeRect (controlsX + indent, y, right - controlsX - indent, rowH));
            y += rowH + gap;
        }
    }

    // The Super Serial Card: its switches and connection, under the card.
    m_serial.SetVisible (showSerial);

    if (showSerial)
    {
        RECT  serialRect = { x - pad, y - pad, rect.right, rect.bottom };

        m_serial.Layout (serialRect, s);
        y += m_serial.GetContentHeightPx();
    }

    // The surface the selected slot's controls sit on, which the selected
    // row in the list joins.
    m_panelRect = { x - pad, rect.top + pad / 2, colRight + pad, std::max ((int) GetRowRect (kLastSlot - kFirstSlot).bottom, y) + pad / 2 };

    DxuiPanel::SetBounds (rect);
    SetContentHeightPx (std::max (listBottom, y) + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::GetRowRect
//
//  A row's span in the list, in page coordinates; empty when the row is
//  scrolled out of view.
//
////////////////////////////////////////////////////////////////////////////////

RECT SlotsPage::GetRowRect (int row) const
{
    RECT  list = m_list.GetBounds();
    RECT  out  = {};



    for (int y = 0; y < list.bottom - list.top; y++)
    {
        int   hit   = m_list.HitTestRow (1, y);
        bool  isRow = hit == row;

        if (isRow && out.bottom <= out.top)
        {
            out = { list.left, list.top + y, list.right, list.top + y + 1 };
        }
        else if (isRow)
        {
            out.bottom = list.top + y + 1;
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::Paint
//
//  The panel under the selected slot's controls, then the controls, then a
//  bridge in the panel's fill from the selected row to the panel, so the row
//  and the panel read as one surface, like a tab pulled out of it.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    uint32_t  fill   = theme.ContentSelection();
    float     radius = m_scaler.ToPxf (s_kPanelRadiusDp);
    RECT      row    = GetRowRect (m_selectedSlot - kFirstSlot);
    int       inset  = m_scaler.ToPx (s_kRowInsetDp);



    if (m_panelRect.right > m_panelRect.left)
    {
        painter.FillRoundedRect ((float) m_panelRect.left, (float) m_panelRect.top,
                                 (float) (m_panelRect.right - m_panelRect.left), (float) (m_panelRect.bottom - m_panelRect.top),
                                 radius, fill);
    }

    DxuiPanel::Paint (painter, text, theme);

    if (row.bottom > row.top + 2 * inset && m_panelRect.right > m_panelRect.left)
    {
        float  left = (float) row.right - 2.0f * radius;

        painter.FillRect (left, (float) (row.top + inset), (float) m_panelRect.left + radius - left, (float) (row.bottom - row.top - 2 * inset), fill);
    }
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
    m_selectedSlot = std::clamp (slot, kFirstSlot, kLastSlot);
    m_prompt.clear();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::ChooseCard
//
//  A card the machine can't host is not put in; the prompt lists the
//  machines that can. Any other card goes straight in; Cancel on the
//  sheet undoes it.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::ChooseCard (int choice)
{
    int          entry   = GetSelectedEntry();
    std::string  current = (entry >= 0) ? m_state->GetHardware()[(size_t) entry].type : std::string();



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

    // A copy: the edit rebuilds the list of choices.
    ApplyCard (std::string (m_cardChoices[(size_t) choice].device));
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
//  Choice 0 is no drive on the port; the others are the card's kinds of
//  drive, in the order the dropdown lists them.
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
//  SlotsPage::ChooseDriveCount
//
//  The number of drives connected to the card, from the dropdown's
//  position.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::ChooseDriveCount (int choice)
{
    int  entry = GetSelectedEntry();
    int  count = choice + m_countBase;



    if (entry < 0 || choice < 0)
    {
        return;
    }

    if (IsDiskIiSelected())
    {
        m_state->SetSecondDriveAttached (count >= 2);
        OnEdited (S_OK);
    }
    else
    {
        const HardwareEntry  & e    = m_state->GetHardware()[(size_t) entry];
        const BlockCardSpec  * card = BlockDeviceCatalog::FindCard (e.type);
        HRESULT                hr   = S_OK;

        if (card != nullptr && card->isChain)
        {
            hr = m_state->SetChainCount ((size_t) entry, (size_t) count);
        }

        // A card whose ports each take its one kind of drive has the first
        // `count` ports filled.
        for (size_t port = 0; card != nullptr && !card->isChain && SUCCEEDED (hr) && port < (size_t) card->portCount; port++)
        {
            hr = m_state->SetPortDrive ((size_t) entry, port, ((int) port < count) ? std::string (card->drives[0]) : std::string());
        }

        OnEdited (hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::SetAppleFirmware
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::SetAppleFirmware (bool isApple)
{
    int  entry = GetSelectedEntry();



    if (entry >= 0)
    {
        OnEdited (m_state->SetCassoFirmware ((size_t) entry, !isApple));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage::OnEdited
//
//  An edit's error stays in the prompt. A reset the edit takes is reported
//  by the sheet, beside its OK button.
//
////////////////////////////////////////////////////////////////////////////////

void SlotsPage::OnEdited (HRESULT hr, const std::wstring & error)
{
    m_prompt = SUCCEEDED (hr) ? std::wstring() : error;

    Rebuild();
}
