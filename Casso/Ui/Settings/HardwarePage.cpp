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
    constexpr int     s_kInfoRowHeightDp  = 24;
    constexpr int     s_kInfoValueGapDp   = 8;
    constexpr int     s_kBigSectionGapDp  = 18;
    constexpr size_t  s_kMachineRow       = 0;
    constexpr size_t  s_kCpuRow           = 1;
    constexpr size_t  s_kClockRow         = 2;
    constexpr size_t  s_kMemoryRow        = 3;
    constexpr size_t  s_kDevicesRow       = 4;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    TreeCapabilityFlag MapFlag (CapabilityFlag flag)
    {
        switch (flag)
        {
            case CapabilityFlag::Optional:        return TreeCapabilityFlag::Optional;
            case CapabilityFlag::Required:        return TreeCapabilityFlag::Required;
            case CapabilityFlag::PlatformLocked:  return TreeCapabilityFlag::PlatformLocked;
        }
        return TreeCapabilityFlag::Required;
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

void HardwarePage::SetRect (const RECT & rect, const DpiScaler & scaler)
{
    UINT dpi         = scaler.Dpi();
    int  labelWidth  = scaler.Px (s_kInfoLabelWidthDp);
    int  rowHeight   = scaler.Px (s_kInfoRowHeightDp);
    int  valueGap    = scaler.Px (s_kInfoValueGapDp);
    int  sectionGap  = scaler.Px (s_kBigSectionGapDp);
    int  valueX      = rect.left + labelWidth + valueGap;
    int  y           = rect.top;
    int  i           = 0;
    RECT treeRect    = rect;



    m_infoLabels[s_kMachineRow].SetText (L"Machine:");
    m_infoLabels[s_kCpuRow].SetText     (L"CPU:");
    m_infoLabels[s_kClockRow].SetText   (L"Clock speed:");
    m_infoLabels[s_kMemoryRow].SetText  (L"Memory regions:");
    m_infoLabels[s_kDevicesRow].SetText (L"Devices:");

    for (i = 0; i < (int) kInfoRowCount; ++i)
    {
        m_infoLabels[(size_t) i].SetRect (MakeRect (rect.left, y, labelWidth, rowHeight));
        m_infoValues[(size_t) i].SetRect (MakeRect (valueX,
                                                    y,
                                                    rect.right - valueX,
                                                    rowHeight));
        m_infoLabels[(size_t) i].SetDpi (dpi);
        m_infoValues[(size_t) i].SetDpi (dpi);
        y += rowHeight;
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
    std::vector<TreeNode>        nodes;
    SettingsPanelState         * state = m_state;
    const SettingsMachineInfo  * info  = nullptr;



    if (state != nullptr)
    {
        info    = &state->MachineInfo();
        entries = state->Hardware();
        m_infoValues[s_kMachineRow].SetText (Widen (info->name));
        m_infoValues[s_kCpuRow].SetText     (Widen (info->cpu));
        m_infoValues[s_kClockRow].SetText   (std::format (L"{} Hz", info->clockSpeed));
        m_infoValues[s_kMemoryRow].SetText  (std::format (L"{}", info->memoryRegions));
        m_infoValues[s_kDevicesRow].SetText (std::format (L"{}", info->devices));
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

void HardwarePage::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    int  i = 0;



    for (i = 0; i < (int) kInfoRowCount; ++i)
    {
        m_infoLabels[(size_t) i].Paint (painter, text);
        m_infoValues[(size_t) i].Paint (painter, text);
    }

    m_tree.Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePage::BuildNodes
//
////////////////////////////////////////////////////////////////////////////////

std::vector<TreeNode> HardwarePage::BuildNodes (const std::vector<HardwareEntry> & entries)
{
    std::vector<TreeNode>  out;
    TreeNode               internalGroup;
    TreeNode               slotsGroup;
    size_t                 i        = 0;
    bool                   anyInternal = false;
    bool                   anySlot     = false;



    internalGroup.label          = L"Internal devices";
    internalGroup.capabilityFlag = TreeCapabilityFlag::Required;
    internalGroup.checked        = true;
    internalGroup.expanded       = true;

    slotsGroup.label             = L"Slots";
    slotsGroup.capabilityFlag    = TreeCapabilityFlag::Required;
    slotsGroup.checked           = true;
    slotsGroup.expanded          = true;

    for (i = 0; i < entries.size(); ++i)
    {
        const HardwareEntry & e = entries[i];
        TreeNode              row;

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
