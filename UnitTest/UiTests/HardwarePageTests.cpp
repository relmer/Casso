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


    // //c external drive (T034a): when the machine supports the optional
    // external drive, BuildNodes appends a top-level checkable "External
    // drive" leaf whose checked state mirrors the connected pref. It is
    // Optional (interactive) so the user can connect/disconnect it, and has
    // no children (a leaf -- no expand twisty).
    TEST_METHOD (BuildNodes_AppendsExternalDriveNodeWhenSupported)
    {
        std::vector<HardwareEntry>  entries;
        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice, "kbd", CapabilityFlag::Required, true));

        std::vector<DxuiTreeNode>  nodes =
            HardwarePage::BuildNodes (entries, /*supportsExternalDrive*/ true, /*connected*/ true);

        Assert::AreEqual<size_t> (2u, nodes.size(),
            L"Internal-devices group + the external-drive leaf.");
        const DxuiTreeNode & ext = nodes.back();
        Assert::AreEqual (std::wstring (L"External drive"), ext.label);
        Assert::IsTrue (ext.capabilityFlag == DxuiTreeCapabilityFlag::Optional,
            L"External drive must be interactive (Optional).");
        Assert::IsTrue (ext.checked, L"Connected pref -> checked node.");
        Assert::IsTrue (ext.children.empty(), L"External drive is a leaf.");
    }


    TEST_METHOD (BuildNodes_ExternalDriveNodeReflectsDisconnected)
    {
        std::vector<DxuiTreeNode>  nodes =
            HardwarePage::BuildNodes ({}, /*supportsExternalDrive*/ true, /*connected*/ false);

        Assert::AreEqual<size_t> (1u, nodes.size(), L"Just the external-drive leaf.");
        Assert::AreEqual (std::wstring (L"External drive"), nodes[0].label);
        Assert::IsFalse (nodes[0].checked, L"Not-connected pref -> unchecked node.");
    }


    TEST_METHOD (BuildNodes_NoExternalDriveNodeWhenUnsupported)
    {
        // Default (supportsExternalDrive = false): no external-drive leaf, so
        // //e / ][ machines are unchanged.
        std::vector<HardwareEntry>  entries;
        entries.push_back (MakeEntry (HardwareEntryKind::Slot, "Slot 6: disk-ii", CapabilityFlag::Optional, true));

        std::vector<DxuiTreeNode>  nodes = HardwarePage::BuildNodes (entries);

        for (const DxuiTreeNode & n : nodes)
        {
            Assert::IsFalse (n.label == L"External drive",
                L"External-drive node must not appear on unsupported machines.");
        }
    }
};
