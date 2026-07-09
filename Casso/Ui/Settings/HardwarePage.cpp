#include "Pch.h"

#include "HardwarePage.h"

#include "Core/UnicodeSymbols.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int     s_kPagePadDp        = 16;        // matches Disk / Display / Theme pages
    constexpr int     s_kInfoLabelWidthDp = 140;
    constexpr int     s_kInfoRowHeightDp  = 28;
    constexpr int     s_kInfoValueGapDp   = 8;
    constexpr int     s_kBigSectionGapDp  = 14;
    constexpr int     s_kDropdownWidthDp  = 200;
    constexpr size_t  s_kCpuRow           = 0;
    constexpr size_t  s_kClockRow         = 1;
    constexpr size_t  s_kMemoryRow        = 2;

    // Label of the synthetic Hardware-tree node for the //c optional external
    // drive. Not backed by a HardwareEntry (the //c drive is built-in, not a
    // config slot), so the tree's toggle handler matches this label to route
    // it to SetExternalDriveConnected instead of SetHardwareEnabled.
    constexpr wchar_t s_kExternalDriveLabel[] = L"External drive";


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    DxuiTreeCapabilityFlag MapFlag (CapabilityFlag flag)
    {
        switch (flag)
        {
            case CapabilityFlag::Optional:        return DxuiTreeCapabilityFlag::Optional;
            case CapabilityFlag::Required:        return DxuiTreeCapabilityFlag::Required;
            case CapabilityFlag::PlatformLocked:  return DxuiTreeCapabilityFlag::PlatformLocked;
        }
        return DxuiTreeCapabilityFlag::Required;
    }


    std::wstring Widen (const std::string & narrow)
    {
        std::wstring  w;

        w.reserve (narrow.size());
        for (char c : narrow)
        {
            w.push_back ((wchar_t) (unsigned char) c);
        }
        return w;
    }
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
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SetRect (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi         = scaler.Dpi();
    int  pad         = scaler.Px (s_kPagePadDp);
    int  labelWidth  = scaler.Px (s_kInfoLabelWidthDp);
    int  rowHeight   = scaler.Px (s_kInfoRowHeightDp);
    int  valueGap    = scaler.Px (s_kInfoValueGapDp);
    int  sectionGap  = scaler.Px (s_kBigSectionGapDp);
    int  dropWidth   = scaler.Px (s_kDropdownWidthDp);
    int  x           = rect.left + pad;
    int  controlsX   = x + labelWidth;
    int  valueX      = x + labelWidth + valueGap;
    int  y           = rect.top + pad;
    size_t i         = 0;
    RECT treeRect    = rect;



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
    m_speed.SetItems ({ L"Authentic (1.023 MHz)",
                        L"2x (2.046 MHz)",
                        std::wstring (L"Maximum (full afterburner ") + s_kpszRocket + L")" });
    y += rowHeight + sectionGap;

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
            int  nameW    = scaler.Px (200);
            int  sizeW    = scaler.Px (55);
            int  addrW    = scaler.Px (130);
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

void HardwarePage::Rebuild ()
{
    std::vector<HardwareEntry>   entries;
    std::vector<DxuiTreeNode>        nodes;
    SettingsPanelState         * state = m_state;
    const SettingsMachineInfo  * info  = nullptr;



    if (state != nullptr)
    {
        // Machine + CPU-speed selectors.
        m_speed.SetSelected           ((int) state->Prefs().speedMode);
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

        info    = &state->MachineInfo();
        entries = state->Hardware();

        // Comma-grouped clock speed (e.g. "1,022,727 Hz"). std::format with
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

        std::wstring  cpuDisplay = Widen (info->cpu);
        if (! info->cpuManufacturer.empty())
        {
            cpuDisplay = Widen (info->cpuManufacturer) + L" " + cpuDisplay;
        }

        m_infoValues[s_kCpuRow].SetText    (cpuDisplay);
        m_infoValues[s_kClockRow].SetText  (FormatGrouped (info->clockSpeed) + L" Hz");
        m_infoValues[s_kMemoryRow].SetText (Widen (info->ramSummary));   // RAM total on the header line

        size_t  rowsInUse = std::min<size_t> (info->memoryRegions.size(), kMaxMemoryRows);
        size_t  i         = 0;
        for (i = 0; i < kMaxMemoryRows; ++i)
        {
            size_t  slotIdx = kFixedInfoRowCount + i;
            if (i < rowsInUse)
            {
                const SettingsMemoryRegion &  r = info->memoryRegions[i];
                m_infoLabels[slotIdx].SetText (Widen (r.name));
                m_infoValues[slotIdx].SetText (Widen (r.size));
                m_infoExtras[slotIdx].SetText (Widen (r.addressRange));
            }
            else
            {
                m_infoLabels[slotIdx].SetText (L"");
                m_infoValues[slotIdx].SetText (L"");
                m_infoExtras[slotIdx].SetText (L"");
            }
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
        bool  supportsExternal  = (info != nullptr) && info->supportsExternalDrive;
        bool  externalConnected = (state != nullptr) && state->Prefs().externalDriveConnected;

        nodes = BuildNodes (entries, supportsExternal, externalConnected);
    }
    m_tree.SetNodes (std::move (nodes));

    m_tree.SetOnToggle ([state] (const std::wstring & label, bool checked)
    {
        size_t  i = 0;

        if (state == nullptr)
        {
            return;
        }

        // The synthetic external-drive node is not a HardwareEntry -- it is a
        // live UI pref, so route it to SetExternalDriveConnected (no reset)
        // rather than the hardware-enable path.
        if (label == s_kExternalDriveLabel)
        {
            state->SetExternalDriveConnected (checked);
            return;
        }

        for (i = 0; i < state->Hardware().size(); ++i)
        {
            std::wstring  candidate = Widen (state->Hardware()[i].displayName);

            if (candidate == label)
            {
                HRESULT  hr = state->SetHardwareEnabled (i, checked);
                IGNORE_RETURN_VALUE (hr, S_OK);
                return;
            }
        }
    });
}




////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::BuildNodes
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTreeNode> HardwarePage::BuildNodes (const std::vector<HardwareEntry> & entries,
                                                    bool supportsExternalDrive,
                                                    bool externalDriveConnected)
{
    std::vector<DxuiTreeNode>  out;
    DxuiTreeNode               internalGroup;
    DxuiTreeNode               slotsGroup;
    size_t                 i        = 0;
    bool                   anyInternal = false;
    bool                   anySlot     = false;



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
        const HardwareEntry & e = entries[i];
        DxuiTreeNode              row;

        row.label          = Widen (e.displayName);
        row.lockReason     = Widen (e.lockReason);
        row.capabilityFlag = MapFlag (e.capability);
        row.checked        = e.enabled;
        row.expanded       = true;

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

    // //c external drive: a top-level checkable node modelling the optional
    // 5.25" drive on the disk port. Optional (interactive), so the user can
    // connect/disconnect it; checked mirrors the persisted connected state.
    // Unlike the hardware rows this is not a config device -- toggling it is
    // a live UI pref, so the tree's OnToggle routes this label specially.
    if (supportsExternalDrive)
    {
        DxuiTreeNode  external;

        external.label          = s_kExternalDriveLabel;
        external.capabilityFlag = DxuiTreeCapabilityFlag::Optional;
        external.checked        = externalDriveConnected;
        external.expanded       = false;   // leaf: no children, no twisty
        out.push_back (std::move (external));
    }

    return out;
}
