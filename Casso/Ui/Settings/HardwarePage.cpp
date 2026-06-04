#include "Pch.h"

#include "HardwarePage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int     s_kInfoLabelWidthDp = 140;
    constexpr int     s_kInfoRowHeightDp  = 28;        // matches Machine / Display / Theme pages
    constexpr int     s_kInfoValueGapDp   = 8;
    constexpr int     s_kBigSectionGapDp  = 14;        // matches other pages' sectionGap
    constexpr size_t  s_kMachineRow       = 0;
    constexpr size_t  s_kCpuRow           = 1;
    constexpr size_t  s_kClockRow         = 2;
    constexpr size_t  s_kMemoryRow        = 3;


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
//  HardwarePage::SetRect
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::SetRect (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi         = scaler.Dpi();
    int  labelWidth  = scaler.Px (s_kInfoLabelWidthDp);
    int  rowHeight   = scaler.Px (s_kInfoRowHeightDp);
    int  valueGap    = scaler.Px (s_kInfoValueGapDp);
    int  sectionGap  = scaler.Px (s_kBigSectionGapDp);
    int  valueX      = rect.left + labelWidth + valueGap;
    int  y           = rect.top;
    size_t i         = 0;
    RECT treeRect    = rect;



    m_baseRect   = rect;
    m_rowHeight  = rowHeight;
    m_sectionGap = sectionGap;
    m_scaler     = scaler;

    m_infoLabels[s_kMachineRow].SetText (L"Machine:");
    m_infoLabels[s_kCpuRow].SetText     (L"CPU:");
    m_infoLabels[s_kClockRow].SetText   (L"Clock speed:");
    m_infoLabels[s_kMemoryRow].SetText  (L"Memory:");

    for (i = 0; i < kInfoRowCount; ++i)
    {
        if (i >= kFixedInfoRowCount)
        {
            // Memory sub-row N sits at the same y as the "Memory:"
            // header for N=0, and stacks below for N>=1. Row 0 thus
            // shares a line with the Memory: header label; the
            // header itself contributes no extra row.
            int  nameW    = scaler.Px (110);
            int  sizeW    = scaler.Px (55);
            int  addrW    = scaler.Px (130);
            int  subIndex = (int) (i - kFixedInfoRowCount);
            int  rowY     = rect.top + ((int) s_kMemoryRow + subIndex) * rowHeight;

            m_infoLabels[i].SetRect (MakeRect (valueX,                 rowY, nameW, rowHeight));
            m_infoValues[i].SetRect (MakeRect (valueX + nameW,         rowY, sizeW, rowHeight));
            m_infoExtras[i].SetRect (MakeRect (valueX + nameW + sizeW, rowY, addrW, rowHeight));
        }
        else
        {
            m_infoLabels[i].SetRect (MakeRect (rect.left, y, labelWidth, rowHeight));
            m_infoValues[i].SetRect (MakeRect (valueX, y, rect.right - valueX, rowHeight));
            m_infoExtras[i].SetRect (MakeRect (rect.right, y, 0, rowHeight));
            y += rowHeight;
        }
        m_infoLabels[i].SetDpi (dpi);
        m_infoValues[i].SetDpi (dpi);
        m_infoExtras[i].SetDpi (dpi);
    }

    // y now sits one row PAST the Memory header. Bump down by the
    // remaining memory rows (rowsInUse - 1, since row 0 shares a
    // line with the header).
    if (m_memoryRowsInUse > 1)
    {
        y += (int) (m_memoryRowsInUse - 1) * rowHeight;
    }

    treeRect.top = y + sectionGap;
    m_tree.SetRect (treeRect);
    m_tree.SetDpi  (dpi);
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
//  HardwarePage::Rebuild
//
//  Walk the state's hardware list and rebuild the tree-view nodes.
//  Hook the tree's toggle callback through to
//  `SettingsPanelState::SetHardwareEnabled` so user toggles route
//  back into the canonical state-machine.
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
        info    = &state->MachineInfo();
        entries = state->Hardware();

        // Comma-grouped clock speed (e.g. "1,022,727 Hz"). std::format
        // with the "L" locale-aware flag requires a locale; build the
        // grouped string by hand for predictable output.
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

        m_infoValues[s_kMachineRow].SetText (Widen (info->name));
        m_infoValues[s_kCpuRow].SetText     (cpuDisplay);
        m_infoValues[s_kClockRow].SetText   (FormatGrouped (info->clockSpeed) + L" Hz");
        m_infoValues[s_kMemoryRow].SetText  (L"");        // header row, value column blank

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
            // Re-run layout with the new row count so the tree shifts
            // down to make space. Only fires when the count changes.
            SetRect (m_baseRect, m_scaler);
        }
    }

    nodes = BuildNodes (entries);
    m_tree.SetNodes (std::move (nodes));

    m_tree.SetOnToggle ([state] (const std::wstring & label, bool checked)
    {
        size_t  i = 0;

        if (state == nullptr)
        {
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
//  HardwarePage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void HardwarePage::Paint (DxuiPainter & painter, DxuiTextRenderer & text) const
{
    int  i = 0;



    for (i = 0; i < (int) kInfoRowCount; ++i)
    {
        m_infoLabels[(size_t) i].Paint (painter, text);
        m_infoValues[(size_t) i].Paint (painter, text);
        m_infoExtras[(size_t) i].Paint (painter, text);
    }

    m_tree.Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::BuildNodes
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTreeNode> HardwarePage::BuildNodes (const std::vector<HardwareEntry> & entries)
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

    return out;
}
