#include "Pch.h"
#include "MenuSystem.h"
#include "Core/MachineConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MenuSystemTests
//
//  Spec-006 T101b. Headless coverage of ShouldEnableDiskIIDebugMenuItem
//  (FR-001a) -- the View -> Disk II Debug... menu item is enabled iff
//  the active machine config wires at least one Disk II controller.
//
////////////////////////////////////////////////////////////////////////////////

namespace MenuSystemTests
{
    TEST_CLASS (MenuSystemTests)
    {
    public:

        TEST_METHOD (ShouldEnableDiskIIDebugMenuItem_emptyConfig_returnsFalse)
        {
            MachineConfig  config;

            Assert::IsFalse (ShouldEnableDiskIIDebugMenuItem (config));
        }



        TEST_METHOD (ShouldEnableDiskIIDebugMenuItem_diskIIInSlot6_returnsTrue)
        {
            MachineConfig  config;
            SlotConfig     slot6;

            slot6.slot   = 6;
            slot6.device = "disk-ii";
            config.slots.push_back (slot6);

            Assert::IsTrue (ShouldEnableDiskIIDebugMenuItem (config));
        }



        TEST_METHOD (ShouldEnableDiskIIDebugMenuItem_cassetteOnlyAppleII_returnsFalse)
        {
            MachineConfig  config;
            SlotConfig     slot1;

            slot1.slot   = 1;
            slot1.device = "printer-card";
            config.slots.push_back (slot1);

            Assert::IsFalse (ShouldEnableDiskIIDebugMenuItem (config));
        }



        TEST_METHOD (ShouldEnableDiskIIDebugMenuItem_multipleDiskIIControllers_returnsTrue)
        {
            MachineConfig  config;
            SlotConfig     slot5;
            SlotConfig     slot6;

            slot5.slot   = 5;
            slot5.device = "disk-ii";
            slot6.slot   = 6;
            slot6.device = "disk-ii";
            config.slots.push_back (slot5);
            config.slots.push_back (slot6);

            Assert::IsTrue (ShouldEnableDiskIIDebugMenuItem (config));
        }
    };
}
