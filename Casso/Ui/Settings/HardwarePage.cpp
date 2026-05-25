#include "Pch.h"

#include "HardwarePage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
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
    m_tree.SetRect (rect);
    m_tree.SetDpi  (scaler.Dpi());
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
    std::vector<HardwareEntry>  entries;
    std::vector<TreeNode>       nodes;
    SettingsPanelState        * state = m_state;



    if (state != nullptr)
    {
        entries = state->Hardware();
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
