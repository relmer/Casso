#include "Pch.h"

#include "HardwarePage.h"

#include "Core/UnicodeSymbols.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/BlockDevice/StockHardDiskImage.h"
#include "Devices/SerialPortDevice.h"
#include "Ui/Settings/SlotCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int     s_kPagePadDp        = 16;        // matches Disk / Display / Theme pages
static constexpr int     s_kInfoLabelWidthDp = 140;
static constexpr int     s_kInfoRowHeightDp  = 28;
static constexpr int     s_kInfoValueGapDp   = 8;
static constexpr int     s_kBigSectionGapDp  = 14;
static constexpr int     s_kDropdownWidthDp  = 200;
static constexpr int     s_kStorageButtonWidthDp = 120;
static constexpr int     s_kFirstSlot            = 1;
static constexpr int     s_kLastSlot             = 7;
static constexpr size_t  s_kCpuRow           = 0;
static constexpr size_t  s_kClockRow         = 1;
static constexpr size_t  s_kMemoryRow        = 2;

// Label of the synthetic Hardware-tree node for the //c mouse peripheral.
// Not backed by a HardwareEntry, so the tree's toggle handler matches this
// label to route it to SetMouseConnected instead of SetHardwareEnabled.
static constexpr wchar_t s_kMouseLabel[]         = L"Mouse";





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT HardwarePage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::MapFlag
//
////////////////////////////////////////////////////////////////////////////////

DxuiTreeCapabilityFlag HardwarePage::MapFlag (CapabilityFlag flag)
{
    // Required is the safe default for an unmapped flag: it shows the row
    // without offering a toggle the machine may not support.
    DxuiTreeCapabilityFlag  mapped = DxuiTreeCapabilityFlag::Required;



    switch (flag)
    {
        case CapabilityFlag::Optional:        mapped = DxuiTreeCapabilityFlag::Optional;       break;
        case CapabilityFlag::Required:        mapped = DxuiTreeCapabilityFlag::Required;       break;
        case CapabilityFlag::PlatformLocked:  mapped = DxuiTreeCapabilityFlag::PlatformLocked; break;
    }

    return mapped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::Widen
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HardwarePage::Widen (const std::string & narrow)
{
    return TextEncoding::Utf8ToWide (narrow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::HardwarePage
//
//  Registers each member widget into the page's child list via Adopt so
//  they participate in the IDxuiControl tree (Bounds, Visible, focus, parent
//  pointers). The widgets remain HardwarePage-owned members; Adopt is
//  non-owning. Layout positioning stays in SetRect() below because the
//  layout does things DxuiFormLayout cannot model (memory rows packed
//  three-wide across one row, sub-row layout under the Memory: header).
//
////////////////////////////////////////////////////////////////////////////////

HardwarePage::HardwarePage(std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    size_t  i = 0;



    Adopt (m_machineLabel);
    Adopt (m_speedLabel);
    Adopt (m_machineDropdown);
    Adopt (m_speed);

    for (i = 0; i < kInfoRowCount; ++i)
    {
        Adopt (m_infoLabels[i]);
        Adopt (m_infoValues[i]);
        Adopt (m_infoExtras[i]);
    }

    Adopt (m_tree);

    for (i = 0; i < (size_t) kSerialPortCount; ++i)
    {
        Adopt (m_serialLabels[i]);
        Adopt (m_serialDropdowns[i]);
    }

    Adopt (m_slotLabel);
    Adopt (m_cardLabel);
    Adopt (m_slotDropdown);
    Adopt (m_cardDropdown);
    Adopt (m_attachButton);
    Adopt (m_ejectButton);
    Adopt (m_addButton);
    Adopt (m_removeButton);
    Adopt (m_promptLabel);
    Adopt (m_replaceButton);
    Adopt (m_cancelButton);

    m_slotLabel.SetText     (L"Slot:");
    m_cardLabel.SetText     (L"Card:");
    m_attachButton.SetLabel (L"Choose image...");
    m_ejectButton.SetLabel  (L"Eject");
    m_addButton.SetLabel    (L"Add drive");
    m_removeButton.SetLabel (L"Remove drive");
    m_cancelButton.SetLabel (L"Cancel");

    m_attachButton.SetOnClick  ([this] () { AttachImage();    });
    m_ejectButton.SetOnClick   ([this] () { EjectImage();     });
    m_addButton.SetOnClick     ([this] () { AddDrive();       });
    m_removeButton.SetOnClick  ([this] () { RemoveDrive();    });
    m_replaceButton.SetOnClick ([this] () { ConfirmReplace(); });
    m_cancelButton.SetOnClick  ([this] () { CancelReplace();  });

    m_slotDropdown.SetSelect ([this] (int index) { SelectSlot (index + 1); });
    m_cardDropdown.SetSelect ([this] (int index) { ChooseCard (index); });

    m_cardDropdown.SetOnHighlightChange ([this] (int index)
    {
        bool  isBlocked = index >= 0 && index < (int) m_cardChoices.size() && !m_cardChoices[(size_t) index].allowed;

        if (isBlocked && !m_replacePending)
        {
            m_storagePrompt = m_cardChoices[(size_t) index].tip;
            m_promptLabel.SetText (m_storagePrompt);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::SetMachineList
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SetMachineList (std::vector<std::string>  machineIds,
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
//  HardwarePage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    SetRect (rect, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::SetRect
//
//  Lays out the page: the machine and speed selectors, the read-only spec
//  block, and the hardware tree below them.
//
//  The spec block has TWO row shapes, and that is what most of this function
//  is about. Fixed rows -- CPU, clock, memory -- are label plus value across
//  the page width. Memory sub-rows are a three-column breakdown (name, size,
//  address range) indented under the Memory header, whose own value column
//  carries the total.
//
//  Sub-rows are positioned from the memory header's index plus one rather than
//  from the running y, so the breakdown stays anchored to its header no matter
//  how many regions there are.
//
//  The name column is sized for the longest region label ("RAM (main,
//  bank-switched)") so it never wraps; the size and address columns are placed
//  relative to it, so widening the name shifts them rather than overlapping.
//
//  Layout state -- base rect, row height, gap, scaler -- is cached because the
//  page relayouts itself when the tree grows or shrinks and must reproduce
//  exactly this geometry without being called again.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SetRect (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT    dpi        = scaler.GetDpi();
    int     pad        = scaler.ToPx (s_kPagePadDp);
    int     labelWidth = scaler.ToPx (s_kInfoLabelWidthDp);
    int     rowHeight  = scaler.ToPx (s_kInfoRowHeightDp);
    int     valueGap   = scaler.ToPx (s_kInfoValueGapDp);
    int     sectionGap = scaler.ToPx (s_kBigSectionGapDp);
    int     dropWidth  = scaler.ToPx (s_kDropdownWidthDp);
    int     x          = rect.left + pad;
    int     controlsX  = x + labelWidth;
    int     valueX     = x + labelWidth + valueGap;
    int     y          = rect.top + pad;
    size_t  i          = 0;
    RECT    treeRect   = rect;



    m_baseRect   = rect;
    m_rowHeight  = rowHeight;
    m_sectionGap = sectionGap;
    m_scaler     = scaler;

    // Machine + CPU-speed selectors above the read-only spec block.
    m_machineLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_machineLabel.SetText (L"Machine:");
    m_machineDropdown.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_speedLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_speedLabel.SetText (L"CPU speed:");
    m_speed.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_speed.SetItems ({ L"Authentic (1.020 MHz)",
                        L"2x (2.041 MHz)",
                        std::wstring (L"Maximum (full afterburner ") + s_kpszRocket + L")" });
    y += rowHeight + sectionGap;

    // A //c's two serial ports, below the speed, on a machine that has them.
    for (i = 0; i < (size_t) kSerialPortCount; ++i)
    {
        m_serialLabels[i].SetText    (std::format (L"Serial port {}:", i + 1));
        m_serialLabels[i].SetVisible (m_showSerial);
        m_serialDropdowns[i].SetVisible (m_showSerial);
        m_serialLabels[i].SetDpi     (dpi);
        m_serialDropdowns[i].SetDpi  (dpi);

        if (m_showSerial)
        {
            m_serialLabels[i].SetRect    (MakeRect (x, y, labelWidth, rowHeight));
            m_serialDropdowns[i].SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));
            y += rowHeight + sectionGap;
        }
    }

    // The storage controls, on a machine with slots: the slot and its card,
    // the selected drive's buttons, and the prompt row.
    {
        DxuiLabel   * labels[]  = { &m_slotLabel, &m_cardLabel };
        DxuiButton  * buttons[] = { &m_attachButton, &m_ejectButton, &m_addButton, &m_removeButton };
        DxuiButton  * confirm[] = { &m_replaceButton, &m_cancelButton };
        int           buttonW   = scaler.ToPx (s_kStorageButtonWidthDp);
        int           buttonGap = scaler.ToPx (s_kInfoValueGapDp);
        int           bx        = controlsX;
        int           storageY  = y;

        for (DxuiLabel * label : labels)
        {
            label->SetVisible (m_showStorage);
            label->SetDpi     (dpi);
        }

        m_slotLabel.SetRect    (MakeRect (x, storageY, labelWidth, rowHeight));
        m_slotDropdown.SetRect (MakeRect (controlsX, storageY, dropWidth, rowHeight));
        storageY += rowHeight + sectionGap;

        m_cardLabel.SetRect    (MakeRect (x, storageY, labelWidth, rowHeight));
        m_cardDropdown.SetRect (MakeRect (controlsX, storageY, dropWidth, rowHeight));
        storageY += rowHeight + sectionGap;

        for (DxuiButton * button : buttons)
        {
            button->SetDpi     (dpi);
            button->SetVisible (m_showStorage && !m_replacePending);
            button->Layout     (MakeRect (bx, storageY, buttonW, rowHeight));
            bx += buttonW + buttonGap;
        }

        bx = controlsX;

        for (DxuiButton * button : confirm)
        {
            button->SetDpi     (dpi);
            button->SetVisible (m_showStorage && m_replacePending);
            button->Layout     (MakeRect (bx, storageY, buttonW, rowHeight));
            bx += buttonW + buttonGap;
        }

        storageY += rowHeight + sectionGap;

        m_promptLabel.SetVisible (m_showStorage);
        m_promptLabel.SetDpi     (dpi);
        m_promptLabel.SetRect    (MakeRect (controlsX, storageY, rect.right - controlsX - pad, rowHeight));
        storageY += rowHeight + sectionGap;

        m_slotDropdown.SetVisible (m_showStorage);
        m_cardDropdown.SetVisible (m_showStorage);
        m_slotDropdown.SetDpi     (dpi);
        m_cardDropdown.SetDpi     (dpi);

        if (m_showStorage)
        {
            y = storageY;
        }
    }

    // Hardware spec block starts below the selectors.
    m_infoTop = y;

    m_infoLabels[s_kCpuRow].SetText    (L"CPU:");
    m_infoLabels[s_kClockRow].SetText  (L"Clock speed:");
    m_infoLabels[s_kMemoryRow].SetText (L"Memory:");

    for (i = 0; i < kInfoRowCount; ++i)
    {
        if (i >= kFixedInfoRowCount)
        {
            // Memory sub-rows stack UNDER the "Memory:" header, which now
            // carries the RAM total ("128K RAM") in its value column. So the
            // per-region breakdown starts one row below the header (+1). The
            // name column is wide enough for the longest region label
            // ("RAM (main, bank-switched)") to stay on one line -- there is
            // ample dialog margin to the right, and the size/addr columns are
            // positioned relative to it.
            int  nameW    = scaler.ToPx (200);
            int  sizeW    = scaler.ToPx (55);
            int  addrW    = scaler.ToPx (130);
            int  subIndex = (int) (i - kFixedInfoRowCount);
            int  rowY     = m_infoTop + ((int) s_kMemoryRow + 1 + subIndex) * rowHeight;

            m_infoLabels[i].SetRect (MakeRect (valueX,                 rowY, nameW, rowHeight));
            m_infoValues[i].SetRect (MakeRect (valueX + nameW,         rowY, sizeW, rowHeight));
            m_infoExtras[i].SetRect (MakeRect (valueX + nameW + sizeW, rowY, addrW, rowHeight));
        }
        else
        {
            m_infoLabels[i].SetRect (MakeRect (x, y, labelWidth, rowHeight));
            m_infoValues[i].SetRect (MakeRect (valueX, y, rect.right - valueX, rowHeight));
            m_infoExtras[i].SetRect (MakeRect (rect.right, y, 0, rowHeight));
            y += rowHeight;
        }

        m_infoLabels[i].SetDpi (dpi);
        m_infoValues[i].SetDpi (dpi);
        m_infoExtras[i].SetDpi (dpi);
    }

    // y now sits one row PAST the Memory header (which holds the RAM total).
    // Every region row stacks below that header now, so bump down by the full
    // region count to clear them before the device tree.
    if (m_memoryRowsInUse > 0)
    {
        y += (int) m_memoryRowsInUse * rowHeight;
    }

    treeRect.left = x;
    treeRect.top  = y + sectionGap;
    m_tree.SetRect (treeRect);
    m_tree.SetDpi  (dpi);

    m_machineLabel.SetDpi    (dpi);
    m_speedLabel.SetDpi      (dpi);
    m_machineDropdown.SetDpi (dpi);
    m_speed.SetDpi           (dpi);

    // Mirror the page's footprint into the IDxuiControl tree so future
    // centralized walks see this page as a panel covering `rect`.
    DxuiPanel::SetBounds (rect);

    // The tree is given the rest of the page, so its rows, not its rect, are
    // where the content ends.
    SetContentHeightPx (treeRect.top + m_tree.GetVisibleCount() * m_tree.GetRowHeight() + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SetPopupHost (DxuiHwndSource * host)
{
    m_machineDropdown.SetPopupHost (host);
    m_speed.SetPopupHost           (host);

    for (DxuiComboBox & dropdown : m_serialDropdowns)
    {
        dropdown.SetPopupHost (host);
    }

    m_slotDropdown.SetPopupHost (host);
    m_cardDropdown.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::Rebuild
//
//  Sync the machine + speed selectors to the state, then walk the state's
//  hardware list and rebuild the tree-view nodes. Hook the tree's toggle
//  callback through to `SettingsPanelState::SetHardwareEnabled` so user
//  toggles route back into the canonical state-machine.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::Rebuild()
{
    std::vector<HardwareEntry>    entries;
    std::vector<DxuiTreeNode>     nodes;
    SettingsPanelState          * state   = m_state;
    const SettingsMachineInfo   * info    = nullptr;



    if (state != nullptr)
    {
        std::wstring  cpuDisplay;
        size_t        rowsInUse  = 0;
        size_t        i          = 0;

        // Machine + CPU-speed selectors.
        m_speed.SetSelected           ((int) state->GetPrefs().speedMode);
        m_machineDropdown.SetSelected (m_activeMachineIndex);

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
        m_speed.SetSelect ([state] (int idx) { state->SetSpeedMode ((SettingsSpeedMode) idx); });

        info    = &state->GetMachineInfo();
        entries = state->GetHardware();

        RebuildSerialPorts (*state);
        RebuildStorage();

        // Comma-grouped clock speed (e.g. "1,020,484 Hz"). std::format with
        // the "L" locale-aware flag requires a locale; build the grouped
        // string by hand for predictable output.
        auto FormatGrouped = [] (uint32_t n)
        {
            std::wstring  s = std::to_wstring (n);
            int           i = (int) s.size() - 3;
            while (i > 0)
            {
                s.insert ((size_t) i, L",");
                i -= 3;
            }

            return s;
        };

        cpuDisplay = Widen (info->cpu);
        if (! info->cpuManufacturer.empty())
        {
            cpuDisplay = Widen (info->cpuManufacturer) + L" " + cpuDisplay;
        }

        m_infoValues[s_kCpuRow].SetText    (cpuDisplay);
        m_infoValues[s_kClockRow].SetText  (FormatGrouped (info->cpuClockSpeed != 0 ? info->cpuClockSpeed : info->clockSpeed) + L" Hz");
        m_infoValues[s_kMemoryRow].SetText (Widen (info->ramSummary));   // RAM total on the header line

        // The total is all the page lists of memory; the regions, main and
        // auxiliary RAM, ROM and the banks, are the debugger's.
        rowsInUse = 0;
        for (i = 0; i < kMaxMemoryRows; ++i)
        {
            size_t  slotIdx = kFixedInfoRowCount + i;

            m_infoLabels[slotIdx].SetText (L"");
            m_infoValues[slotIdx].SetText (L"");
            m_infoExtras[slotIdx].SetText (L"");
        }

        if (rowsInUse != m_memoryRowsInUse)
        {
            m_memoryRowsInUse = rowsInUse;
            // Re-run layout with the new row count so the tree shifts down to
            // make space. Only fires when the count changes.
            SetRect (m_baseRect, m_scaler);
        }
    }

    {
        // The //c is the machine with a mouse port; supportsExternalDrive is
        // the flag that distinguishes it.
        bool  supportsMouse  = (info != nullptr) && info->supportsExternalDrive;
        bool  mouseConnected = (state == nullptr) || state->GetPrefs().mouseConnected;

        nodes = BuildNodes (entries, supportsMouse, mouseConnected);
    }

    m_tree.SetNodes (std::move (nodes));
    m_tree.SetOnSelect ([this] (const std::wstring & id) { SelectTreeRow (id); });

    m_tree.SetOnToggle ([this, state] (const std::wstring & label, bool checked)
    {
        size_t  i = 0;

        if (state == nullptr)
        {
            return;
        }

        // The synthetic mouse node is not a HardwareEntry -- it is a live UI
        // pref, so route it to SetMouseConnected (no reset) rather than the
        // hardware-enable path.
        if (label == s_kMouseLabel)
        {
            state->SetMouseConnected (checked);
            return;
        }

        for (i = 0; i < state->GetHardware().size(); ++i)
        {
            const HardwareEntry  & entry = state->GetHardware()[i];

            for (size_t port = 0; port < entry.ports.size(); ++port)
            {
                if (label == MakeTimingLabel (entry, port))
                {
                    HRESULT  hr = state->SetPortRealTiming (i, port, checked);

                    IGNORE_RETURN_VALUE (hr, S_OK);
                    return;
                }
            }
        }

        for (i = 0; i < state->GetHardware().size(); ++i)
        {
            std::wstring  candidate = Widen (state->GetHardware()[i].displayName);

            if (candidate == label)
            {
                HRESULT       hr     = state->SetHardwareEnabled (i, checked);
                std::wstring  notice = state->TakeNotice();

                IGNORE_RETURN_VALUE (hr, S_OK);

                if (!notice.empty() && m_onNotice)
                {
                    m_onNotice (notice);
                }

                return;
            }
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::BuildNodes
//
//  Projects the machine's hardware entries into the tree the page displays:
//  internal devices and slots, each under its own group.
//
//  An EMPTY group is omitted entirely rather than shown with no children. A
//  //c has no slots at all, and a "Slots" heading with nothing under it reads
//  as a bug rather than as an accurate description of the machine.
//
//  The group rows themselves are marked Required so they cannot be unchecked.
//  They are organizational headings, not devices; letting one be toggled would
//  imply the machine could have its internal devices removed as a set.
//
//  Groups start EXPANDED, since the whole point of the page is seeing what the
//  machine has -- a collapsed tree would hide it behind two clicks.
//
//  Each row carries its lock reason through to the tree, so the UI can explain
//  why a platform-locked device cannot be removed instead of merely showing it
//  grayed.
//
//  Written as a static function over plain entries so the projection is
//  unit-testable without a page, a window, or a machine.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTreeNode> HardwarePage::BuildNodes (const std::vector<HardwareEntry> & entries,
                                                    bool supportsMouse,
                                                    bool mouseConnected)
{
    std::vector<DxuiTreeNode>  out;
    DxuiTreeNode               internalGroup;
    DxuiTreeNode               slotsGroup;
    size_t                     i             = 0;
    bool                       anyInternal   = false;
    bool                       anySlot       = false;



    internalGroup.label          = L"Internal devices";
    internalGroup.capabilityFlag = DxuiTreeCapabilityFlag::Required;
    internalGroup.checked        = true;
    internalGroup.expanded       = true;

    slotsGroup.label             = L"Slots";
    slotsGroup.capabilityFlag    = DxuiTreeCapabilityFlag::Required;
    slotsGroup.checked           = true;
    slotsGroup.expanded          = true;

    for (i = 0; i < entries.size(); ++i)
    {
        const HardwareEntry  & e   = entries[i];
        DxuiTreeNode           row;

        // A card taken out of its slot in this visit is not listed.
        if (e.kind == HardwareEntryKind::Slot && e.type.empty() && e.displayName.empty())
        {
            continue;
        }

        row.id             = std::format (L"entry:{}", i);
        row.label          = Widen (e.displayName);
        row.lockReason     = Widen (e.lockReason);
        row.capabilityFlag = MapFlag (e.capability);
        row.checked        = e.enabled;
        row.expanded       = true;

        AddDriveRows (e, row, i);

        if (e.kind == HardwareEntryKind::InternalDevice)
        {
            internalGroup.children.push_back (std::move (row));
            anyInternal = true;
        }
        else
        {
            slotsGroup.children.push_back (std::move (row));
            anySlot = true;
        }
    }

    if (anyInternal)
    {
        out.push_back (std::move (internalGroup));
    }

    if (anySlot)
    {
        out.push_back (std::move (slotsGroup));
    }

    // //c mouse peripheral: a top-level checkable leaf, connectable and
    // connected by default. Unlike the hardware rows this is not a config
    // device -- toggling it is a live change, so the tree's OnToggle routes
    // this label specially.
    if (supportsMouse)
    {
        DxuiTreeNode  mouse;

        mouse.label          = s_kMouseLabel;
        mouse.capabilityFlag = DxuiTreeCapabilityFlag::Optional;
        mouse.checked        = mouseConnected;
        mouse.expanded       = false;   // leaf: no children, no twisty
        out.push_back (std::move (mouse));
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::AddDriveRows
//
//  A SmartPort or hard disk card's drives, as rows under its slot row, each
//  with its image; a hard disk has a real timing row under it. The drive
//  rows cannot be unchecked: a drive is removed by emptying its port.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::AddDriveRows (const HardwareEntry & entry, DxuiTreeNode & slotRow, size_t entryIndex)
{
    const BlockCardSpec  * card = BlockDeviceCatalog::FindCard (entry.type);



    if (card == nullptr)
    {
        return;
    }

    for (size_t port = 0; port < entry.ports.size(); ++port)
    {
        const BlockDriveSpec  * drive = BlockDeviceCatalog::FindDrive (entry.ports[port]);
        DxuiTreeNode            driveRow;



        if (drive == nullptr)
        {
            continue;
        }

        driveRow.id             = std::format (L"drive:{}:{}", entryIndex, port);
        driveRow.label          = MakeDriveLabel (entry, port);
        driveRow.capabilityFlag = DxuiTreeCapabilityFlag::Required;
        driveRow.checked        = true;
        driveRow.expanded       = true;

        if (drive->hasRealTiming)
        {
            DxuiTreeNode  timing;



            timing.label          = MakeTimingLabel (entry, port);
            timing.capabilityFlag = DxuiTreeCapabilityFlag::Optional;
            timing.checked        = port < entry.portRealTiming.size() && entry.portRealTiming[port];
            timing.expanded       = false;
            timing.lockReason     = L"The drive's spin-up and seek times, which make the firmware wait as a real drive does.";

            driveRow.children.push_back (std::move (timing));
        }

        slotRow.children.push_back (std::move (driveRow));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::MakeDriveLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HardwarePage::MakeDriveLabel (const HardwareEntry & entry, size_t port)
{
    const BlockDriveSpec  * drive  = BlockDeviceCatalog::FindDrive (port < entry.ports.size() ? entry.ports[port] : std::string());
    std::string             image  = port < entry.portImages.size() ? entry.portImages[port] : std::string();
    std::wstring            unit   = (entry.type == kpszAppleScsiCardDevice) ? std::format (L"SCSI ID {}", port)
                                                                             : std::format (L"Drive {}", port + 1);
    std::wstring            name   = drive != nullptr ? Widen (drive->label) : std::wstring();
    std::wstring            file   = image.empty()          ? std::wstring (L"no image")
                                   : image == kpszStockImage ? Widen (StockHardDiskImage::kpszFileName)
                                                             : std::filesystem::path (Widen (image)).filename().wstring();



    return std::format (L"{}: {} ({})", unit, name, file);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::MakeTimingLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HardwarePage::MakeTimingLabel (const HardwareEntry & entry, size_t port)
{
    std::wstring  unit = (entry.type == kpszAppleScsiCardDevice) ? std::format (L"SCSI ID {}", port)
                                                                 : std::format (L"drive {}", port + 1);



    return std::format (L"Real timing, slot {} {}", entry.slot, unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::RebuildSerialPorts
//
//  A change in whether the machine has serial ports moves every row below
//  them, so the page is laid out again.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::RebuildSerialPorts (SettingsPanelState & state)
{
    bool  show = state.HasSerialPorts();



    for (int port = 1; port <= kSerialPortCount; port++)
    {
        std::vector<std::wstring>  labels;
        int                        selected = 0;
        size_t                     index    = (size_t) (port - 1);

        BuildSerialChoices (port, m_comPorts, state.GetSerialPortDevice (port), labels, m_serialDevices[index], selected);

        m_serialDropdowns[index].SetItems    (labels);
        m_serialDropdowns[index].SetSelected (selected);
        m_serialDropdowns[index].SetSelect ([this, statePtr = &state, port, index] (int choice)
        {
            if (choice >= 0 && choice < (int) m_serialDevices[index].size())
            {
                statePtr->SetSerialPortDevice (port, m_serialDevices[index][(size_t) choice]);
            }
        });
    }

    if (show != m_showSerial)
    {
        m_showSerial = show;
        SetRect (m_baseRect, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::BuildSerialChoices
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::BuildSerialChoices (int port, const std::vector<std::wstring> & comPorts, const std::string & current,
                                       std::vector<std::wstring> & outLabels, std::vector<std::string> & outDevices, int & outSelected)
{
    SerialPortDevice  tcp;



    outLabels.clear();
    outDevices.clear();

    tcp.kind    = SerialPortDevice::Kind::Tcp;
    tcp.tcpPort = (uint16_t) (kSerialTcpBasePort + port);

    //  The ImageWriter II is connected on the Printing tab, so it is listed
    //  below, with any other device not offered, only while the port has it.
    outDevices.push_back (std::string());
    outDevices.push_back (SerialPortDevice::Format (tcp));

    for (const std::wstring & comPort : comPorts)
    {
        SerialPortDevice  com;

        com.kind    = SerialPortDevice::Kind::Com;
        com.comPort = comPort;
        outDevices.push_back (SerialPortDevice::Format (com));
    }

    if (std::find (outDevices.begin(), outDevices.end(), current) == outDevices.end())
    {
        outDevices.push_back (current);
    }

    for (const std::string & device : outDevices)
    {
        outLabels.push_back (DescribeSerialDevice (device));
    }

    outSelected = (int) (std::find (outDevices.begin(), outDevices.end(), current) - outDevices.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::DescribeSerialDevice
//
//  A device that does not parse is displayed verbatim.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HardwarePage::DescribeSerialDevice (const std::string & device)
{
    SerialPortDevice  parsed;
    HRESULT           hr    = SerialPortDevice::Parse (device, parsed);
    std::wstring      label = Widen (device);



    if (SUCCEEDED (hr) && parsed.kind == SerialPortDevice::Kind::Loopback)
    {
        label = L"Not connected";
    }
    else if (SUCCEEDED (hr) && parsed.kind == SerialPortDevice::Kind::Printer)
    {
        label = L"ImageWriter II";
    }
    else if (SUCCEEDED (hr) && parsed.kind == SerialPortDevice::Kind::Tcp)
    {
        label = std::format (L"TCP port {} on this PC", parsed.tcpPort);
    }
    else if (SUCCEEDED (hr) && parsed.baud != SerialPortDevice::kDefaultBaud)
    {
        label = std::format (L"{} at {} baud", parsed.comPort, parsed.baud);
    }
    else if (SUCCEEDED (hr))
    {
        label = parsed.comPort;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::RebuildStorage
//
//  The slot list with each slot's card, the cards offered for the selected
//  slot, and which of the drive buttons apply to the tree's selection.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::RebuildStorage()
{
    std::vector<std::wstring>    slotItems;
    std::vector<std::wstring>    cardItems;
    std::vector<bool>            cardEnabled;
    int                          cardIndex   = 0;
    int                          entry       = -1;
    const HardwareEntry        * selected    = nullptr;
    bool                         hasDrive    = false;
    bool                         hasImage    = false;
    bool                         showBefore  = m_showStorage;



    m_showStorage = m_state != nullptr && !m_state->GetMachineInfo().supportsExternalDrive;

    if (m_showStorage)
    {
        const std::vector<HardwareEntry>  & entries = m_state->GetHardware();

        if (m_selectedSlot < s_kFirstSlot || m_selectedSlot > s_kLastSlot)
        {
            m_selectedSlot = s_kLastSlot;

            for (int slot = s_kFirstSlot; slot <= s_kLastSlot; ++slot)
            {
                int  found = SlotCards::FindSlot (entries, slot);

                if (found >= 0 && BlockDeviceCatalog::FindCard (entries[(size_t) found].type) != nullptr)
                {
                    m_selectedSlot = slot;
                    break;
                }
            }
        }

        for (int slot = s_kFirstSlot; slot <= s_kLastSlot; ++slot)
        {
            int  found = SlotCards::FindSlot (entries, slot);

            slotItems.push_back (found >= 0 ? Widen (entries[(size_t) found].displayName) : std::format (L"Slot {}: Empty", slot));
        }

        m_cardChoices = m_state->GetCardChoices();
        entry         = GetSelectedEntry();
        selected      = (entry >= 0) ? &entries[(size_t) entry] : nullptr;

        // A card in the slot that is not a storage card is listed last, as
        // the slot's card.
        if (selected != nullptr && BlockDeviceCatalog::FindCard (selected->type) == nullptr)
        {
            SlotCardChoice  current;
            std::string     name = selected->displayName;
            size_t          colon = name.find (": ");



            current.device = selected->type;
            current.label  = Widen (colon != std::string::npos ? name.substr (colon + 2) : name);
            m_cardChoices.push_back (std::move (current));
        }

        for (size_t i = 0; i < m_cardChoices.size(); ++i)
        {
            cardItems.push_back   (m_cardChoices[i].label);
            cardEnabled.push_back (m_cardChoices[i].allowed);

            if (selected != nullptr && m_cardChoices[i].device == selected->type)
            {
                cardIndex = (int) i;
            }
        }

        hasDrive = selected != nullptr && m_selectedPort >= 0 && (size_t) m_selectedPort < selected->ports.size()
                && BlockDeviceCatalog::FindDrive (selected->ports[(size_t) m_selectedPort]) != nullptr;
        hasImage = hasDrive && (size_t) m_selectedPort < selected->portImages.size() && !selected->portImages[(size_t) m_selectedPort].empty();
    }

    m_slotDropdown.SetItems    (slotItems);
    m_slotDropdown.SetSelected (m_selectedSlot - s_kFirstSlot);
    m_cardDropdown.SetItems        (cardItems);
    m_cardDropdown.SetItemsEnabled (cardEnabled);
    m_cardDropdown.SetSelected     (cardIndex);

    m_attachButton.SetEnabled (hasDrive);
    m_ejectButton.SetEnabled  (hasImage);
    m_addButton.SetEnabled    (IsChainSelected());
    m_removeButton.SetEnabled (hasDrive && IsChainSelected());
    m_promptLabel.SetText     (m_storagePrompt);

    if (showBefore != m_showStorage)
    {
        SetRect (m_baseRect, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::GetSelectedEntry
//
//  The hardware entry of the card in the selected slot, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int HardwarePage::GetSelectedEntry() const
{
    return (m_state != nullptr) ? SlotCards::FindSlot (m_state->GetHardware(), m_selectedSlot) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::IsChainSelected
//
////////////////////////////////////////////////////////////////////////////////

bool HardwarePage::IsChainSelected() const
{
    int                    entry = GetSelectedEntry();
    const BlockCardSpec  * card  = (entry >= 0) ? BlockDeviceCatalog::FindCard (m_state->GetHardware()[(size_t) entry].type) : nullptr;



    return card != nullptr && card->isChain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::SelectSlot
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SelectSlot (int slot)
{
    m_selectedSlot   = slot;
    m_selectedPort   = -1;
    m_replacePending = false;
    m_storagePrompt.clear();
    m_pendingDevice.clear();

    RebuildStorage();
    SetRect (m_baseRect, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::ChooseCard
//
//  A card the machine can't host is not put in; the prompt lists the
//  machines that can. A card in a filled slot is replaced only after a
//  confirmation.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::ChooseCard (int choice)
{
    int                  entry   = GetSelectedEntry();
    std::string          current = (entry >= 0) ? m_state->GetHardware()[(size_t) entry].type : std::string();
    std::wstring         label;
    const std::wstring * from    = nullptr;



    if (m_state == nullptr || choice < 0 || choice >= (int) m_cardChoices.size())
    {
        return;
    }

    if (!m_cardChoices[(size_t) choice].allowed)
    {
        m_storagePrompt = m_cardChoices[(size_t) choice].tip;
        RebuildStorage();
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

    for (const SlotCardChoice & c : m_cardChoices)
    {
        if (c.device == current)
        {
            from = &c.label;
        }
    }

    label           = (from != nullptr) ? *from : Widen (current);
    m_pendingDevice = m_cardChoices[(size_t) choice].device;
    m_replacePending = true;

    if (m_pendingDevice.empty())
    {
        m_storagePrompt = std::format (L"Take the {} out of slot {}?", label, m_selectedSlot);
        m_replaceButton.SetLabel (L"Take out");
    }
    else
    {
        m_storagePrompt = std::format (L"Replace the {} in slot {} with the {}?", label, m_selectedSlot, m_cardChoices[(size_t) choice].label);
        m_replaceButton.SetLabel (L"Replace");
    }

    RebuildStorage();
    SetRect (m_baseRect, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::ConfirmReplace
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::ConfirmReplace()
{
    if (m_replacePending)
    {
        m_replacePending = false;
        ApplyCard (m_pendingDevice);
        m_pendingDevice.clear();
        SetRect (m_baseRect, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::CancelReplace
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::CancelReplace()
{
    m_replacePending = false;
    m_pendingDevice.clear();
    m_storagePrompt.clear();

    RebuildStorage();
    SetRect (m_baseRect, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::ApplyCard
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::ApplyCard (const std::string & device)
{
    std::wstring  error;
    HRESULT       hr = m_state->SetSlotCard (m_selectedSlot, device, error);



    m_storagePrompt = SUCCEEDED (hr) ? std::wstring (L"Takes effect when the machine resets.") : error;
    m_selectedPort  = -1;
    OnStorageEdited();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::SelectTreeRow
//
//  Clicking a slot's row selects the slot; clicking a drive's row selects
//  the slot and the drive for the buttons.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SelectTreeRow (const std::wstring & id)
{
    size_t  entry = 0;
    int     port  = -1;
    bool    isDrive = swscanf_s (id.c_str(), L"drive:%zu:%d", &entry, &port) == 2;
    bool    isEntry = !isDrive && swscanf_s (id.c_str(), L"entry:%zu", &entry) == 1;
    bool    isSlot  = (isDrive || isEntry) && m_state != nullptr && entry < m_state->GetHardware().size()
                   && m_state->GetHardware()[entry].kind == HardwareEntryKind::Slot;



    if (!isSlot || m_replacePending)
    {
        return;
    }

    m_selectedSlot = m_state->GetHardware()[entry].slot;
    m_selectedPort = isDrive ? port : -1;
    m_storagePrompt.clear();

    RebuildStorage();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::AttachImage
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::AttachImage()
{
    int          entry = GetSelectedEntry();
    std::string  path;
    HRESULT      hr    = S_OK;



    if (entry < 0 || m_selectedPort < 0 || !m_onBrowseImage)
    {
        return;
    }

    {
        const HardwareEntry  & e = m_state->GetHardware()[(size_t) entry];

        path = (size_t) m_selectedPort < e.portImages.size() ? e.portImages[(size_t) m_selectedPort] : std::string();
    }

    if (m_onBrowseImage (path))
    {
        hr = m_state->SetPortImage ((size_t) entry, (size_t) m_selectedPort, path);
        m_storagePrompt = SUCCEEDED (hr) ? std::wstring (L"Takes effect when the machine resets.") : std::wstring();
        OnStorageEdited();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::EjectImage
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::EjectImage()
{
    int      entry = GetSelectedEntry();
    HRESULT  hr    = S_OK;



    if (entry >= 0 && m_selectedPort >= 0)
    {
        hr = m_state->SetPortImage ((size_t) entry, (size_t) m_selectedPort, std::string());
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_storagePrompt = L"Takes effect when the machine resets.";
        OnStorageEdited();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::AddDrive
//
//  At the chain's limit, the prompt has the limit.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::AddDrive()
{
    int           entry = GetSelectedEntry();
    std::wstring  error;
    HRESULT       hr    = S_OK;



    if (entry >= 0)
    {
        hr = m_state->AddChainDrive ((size_t) entry, error);
        m_storagePrompt = SUCCEEDED (hr) ? std::wstring (L"Takes effect when the machine resets.") : error;
        OnStorageEdited();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::RemoveDrive
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::RemoveDrive()
{
    int      entry = GetSelectedEntry();
    HRESULT  hr    = S_OK;



    if (entry >= 0 && m_selectedPort >= 0 && IsChainSelected())
    {
        hr = m_state->RemoveChainDrive ((size_t) entry, (size_t) m_selectedPort);
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_selectedPort  = -1;
        m_storagePrompt = L"Takes effect when the machine resets.";
        OnStorageEdited();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::OnStorageEdited
//
//  The tree and the controls after a storage edit.
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::OnStorageEdited()
{
    Rebuild();
    SetRect (m_baseRect, m_scaler);
}
