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



TEST_CLASS (HardwarePageTests)
{
public:

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

    TEST_METHOD (BuildNodes_GroupsInternalAndSlotsSeparately)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>   nodes;

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


    //  Display names come from machine JSON, which is UTF-8. Hex escapes,
    //  because the source files are CP-1252.
    TEST_METHOD (BuildNodes_DecodesUtf8DisplayNames)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>   nodes;



        entries.push_back (MakeEntry (HardwareEntryKind::Slot, "Br\xC3\xB8" "derbund \xCE\xA9\xE6\x97\xA5",
                                      CapabilityFlag::PlatformLocked, true, "\xC3\xA9t\xC3\xA9"));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::AreEqual<size_t> (1u, nodes.size());
        Assert::AreEqual<size_t> (1u, nodes[0].children.size());
        Assert::AreEqual (std::wstring (L"Br\x00F8" L"derbund \x03A9\x65E5"), nodes[0].children[0].label);
        Assert::AreEqual (std::wstring (L"\x00E9t\x00E9"),                   nodes[0].children[0].lockReason);
    }


    TEST_METHOD (BuildNodes_HidesEmptyGroup)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>   nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice, "kbd", CapabilityFlag::Required, true));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::AreEqual<size_t> (1u, nodes.size(),
            L"Empty 'Slots' group must not render when no slots exist.");
        Assert::AreEqual (std::wstring (L"Internal devices"), nodes[0].label);
    }


    TEST_METHOD (BuildNodes_MapsRequiredFlag)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>   nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice, "kbd", CapabilityFlag::Required, true));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::IsTrue (nodes[0].children[0].capabilityFlag == DxuiTreeCapabilityFlag::Required,
            L"Required CapabilityFlag must map to DxuiTreeCapabilityFlag::Required.");
    }


    TEST_METHOD (BuildNodes_MapsOptionalFlag)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>   nodes;

        entries.push_back (MakeEntry (HardwareEntryKind::Slot, "Slot 6: disk-ii", CapabilityFlag::Optional, true));
        nodes = HardwarePage::BuildNodes (entries);

        Assert::IsTrue (nodes[0].children[0].capabilityFlag == DxuiTreeCapabilityFlag::Optional);
    }


    TEST_METHOD (BuildNodes_MapsPlatformLockedFlag_PreservesLockReason)
    {
        std::vector<HardwareEntry>  entries;
        std::vector<DxuiTreeNode>   nodes;

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
        std::vector<DxuiTreeNode>   nodes;

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


    // //c mouse: when the machine has a mouse port, BuildNodes appends a
    // top-level checkable "Mouse" leaf whose checked state mirrors the
    // connected pref. It is Optional (interactive) and has no children.
    TEST_METHOD (BuildNodes_AppendsMouseNodeWhenSupported)
    {
        std::vector<HardwareEntry>  entries;
        entries.push_back (MakeEntry (HardwareEntryKind::InternalDevice, "kbd", CapabilityFlag::Required, true));

        std::vector<DxuiTreeNode>  nodes = HardwarePage::BuildNodes (entries, /*supportsMouse*/ true, /*connected*/ true);

        Assert::AreEqual<size_t> (2u, nodes.size(), L"Internal-devices group + mouse leaf.");
        const DxuiTreeNode & ms = nodes.back();
        Assert::AreEqual (std::wstring (L"Mouse"), ms.label);
        Assert::IsTrue (ms.checked, L"connected pref -> checked node");
        Assert::IsTrue (ms.capabilityFlag == DxuiTreeCapabilityFlag::Optional);
        Assert::IsTrue (ms.children.empty(), L"Mouse is a leaf.");
    }


    TEST_METHOD (BuildNodes_MouseNodeReflectsDisconnected)
    {
        std::vector<DxuiTreeNode>  nodes = HardwarePage::BuildNodes ({}, /*supportsMouse*/ true, /*connected*/ false);

        Assert::AreEqual<size_t> (1u, nodes.size(), L"just the mouse leaf");
        Assert::IsFalse (nodes[0].checked, L"not-connected pref -> unchecked node");
    }


    // The second drive connects live from the Storage menu and its
    // right-click menu, so no machine's tree offers it -- under either of
    // the names it used to have here.
    TEST_METHOD (BuildNodes_NoMachineOffersItsSecondDrive)
    {
        std::vector<HardwareEntry>  entries;



        entries.push_back (MakeEntry (HardwareEntryKind::Slot, "Slot 6: disk-ii", CapabilityFlag::Optional, true));

        for (bool supportsMouse : { false, true })
        {
            for (const DxuiTreeNode & n : HardwarePage::BuildNodes (entries, supportsMouse, true))
            {
                Assert::IsFalse (n.label == L"External drive", L"no External drive node");
                Assert::IsFalse (n.label == L"Drive 2",        L"no Drive 2 node");
            }
        }
    }

    //  The Joyport is turned on from the Controllers page and the picker, not
    //  the Machine tab, so no machine's tree lists it: the ][+ and //e, and the
    //  //c with its mouse.
    TEST_METHOD (BuildNodes_NoMachineListsTheJoyport)
    {
        std::vector<HardwareEntry>                entries;
        std::vector<std::vector<DxuiTreeNode>>    machines;
        size_t                                    checkedRows = 0;



        entries.push_back (MakeEntry (HardwareEntryKind::Slot, "Slot 6: disk-ii", CapabilityFlag::Optional, true));

        machines.push_back (HardwarePage::BuildNodes (entries, false, true));
        machines.push_back (HardwarePage::BuildNodes (entries, true,  true));

        for (const std::vector<DxuiTreeNode> & nodes : machines)
        {
            Assert::IsFalse (nodes.empty(), L"each machine lists its hardware");

            for (const DxuiTreeNode & n : nodes)
            {
                Assert::IsTrue (n.label != L"Game port", L"no Game port group");

                for (const DxuiTreeNode & row : n.children)
                {
                    Assert::IsTrue (row.label.find (L"Joyport") == std::wstring::npos, L"and no Joyport row");
                    checkedRows++;
                }
            }
        }

        Assert::IsTrue (checkedRows > 0, L"the rows were looked at");
    }
};

