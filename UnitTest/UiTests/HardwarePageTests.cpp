#include "Pch.h"

#include "Ui/Settings/HardwarePage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HardwarePageTests
//
//  Tests the per-row rendering rules HardwarePage applies when it
//  turns the SettingsPanelState hardware list into the DxuiTreeView's
//  DxuiTreeNode tree. The actual paint pass is not exercised (no GPU);
//  we verify the mapping produces the right nodes so the renderer
//  can blindly walk them.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    HardwareEntry MakeEntry (HardwareEntryKind kind,
                             const std::string & name,
                             CapabilityFlag      flag,
                             bool                enabled,
                             const std::string & lockReason = "")
    {
        HardwareEntry  e;
        e.kind        = kind;
        e.displayName = name;
        e.capability  = flag;
        e.enabled     = enabled;
        e.lockReason  = lockReason;
        return e;
    }
}


TEST_CLASS (HardwarePageTests)
{
public:

    TEST_METHOD (BuildNodes_GroupsInternalAndSlotsSeparately)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>       nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice, "keyboard", CapabilityFlag::Required, true));
        entries.push_back (MakeEntry (HardwareEntryKind::Slot,           "Slot 6: disk-ii", CapabilityFlag::Optional, true));

        nodes = HardwarePage::BuildNodes (entries);

        Assert::AreEqual<size_t> (2u, nodes.size(),
            L"Both groups should appear when both kinds are present.");
        Assert::AreEqual (std::wstring (L"Internal devices"), nodes[0].label);
        Assert::AreEqual (std::wstring (L"Slots"),            nodes[1].label);
        Assert::AreEqual<size_t> (1u, nodes[0].children.size());
        Assert::AreEqual<size_t> (1u, nodes[1].children.size());
    }


    TEST_METHOD (BuildNodes_HidesEmptyGroup)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>       nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice, "kbd", CapabilityFlag::Required, true));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::AreEqual<size_t> (1u, nodes.size(),
            L"Empty 'Slots' group must not render when no slots exist.");
        Assert::AreEqual (std::wstring (L"Internal devices"), nodes[0].label);
    }


    TEST_METHOD (BuildNodes_MapsRequiredFlag)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>       nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice, "kbd", CapabilityFlag::Required, true));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::IsTrue (nodes[0].children[0].capabilityFlag == DxuiTreeCapabilityFlag::Required,
            L"Required CapabilityFlag must map to DxuiTreeCapabilityFlag::Required.");
    }


    TEST_METHOD (BuildNodes_MapsOptionalFlag)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>       nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::Slot, "Slot 6: disk-ii", CapabilityFlag::Optional, true));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::IsTrue (nodes[0].children[0].capabilityFlag == DxuiTreeCapabilityFlag::Optional);
    }


    TEST_METHOD (BuildNodes_MapsPlatformLockedFlag_PreservesLockReason)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>       nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice,
                                      "80col-card",
                                      CapabilityFlag::PlatformLocked,
                                      true,
                                      "integrated on Apple //c"));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::IsTrue (nodes[0].children[0].capabilityFlag == DxuiTreeCapabilityFlag::PlatformLocked);
        Assert::AreEqual (std::wstring (L"integrated on Apple //c"),
                          nodes[0].children[0].lockReason);
    }


    TEST_METHOD (BuildNodes_PreservesCheckedStateFromEnabled)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>       nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::Slot, "Slot 4: mockingboard", CapabilityFlag::Optional, false));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::IsFalse (nodes[0].children[0].checked,
            L"Entry with enabled=false must produce an unchecked DxuiTreeNode.");
    }


    TEST_METHOD (BuildNodes_EmptyEntryList_NoGroups)
    {
        std::vector<DxuiTreeNode>  nodes = HardwarePage::BuildNodes ({});

        Assert::IsTrue (nodes.empty(),
            L"Empty entry list must produce no group rows.");
    }
};
