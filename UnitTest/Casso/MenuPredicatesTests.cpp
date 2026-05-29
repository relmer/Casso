#include "Pch.h"
#include "UiCommandTypes.h"
#include "Core/MachineConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MenuPredicatesTests
//
//  Headless coverage of ShouldEnableDisk2DebugMenuItem (FR-001a):
//  the View -> Disk II Debug item is enabled iff the active machine
//  config wires at least one Disk II controller.
//
////////////////////////////////////////////////////////////////////////////////

namespace MenuPredicatesTests
{
    TEST_CLASS (MenuPredicatesTests)
    {
    public:

        TEST_METHOD (ShouldEnableDisk2DebugMenuItem_emptyConfig_returnsFalse)
        {
            MachineConfig  config;

            Assert::IsFalse (ShouldEnableDisk2DebugMenuItem (config));
        }



        TEST_METHOD (ShouldEnableDisk2DebugMenuItem_DISK2InSlot6_returnsTrue)
        {
            MachineConfig  config;
            SlotConfig     slot6;

            slot6.slot   = 6;
            slot6.device = "disk-ii";
            config.slots.push_back (slot6);

            Assert::IsTrue (ShouldEnableDisk2DebugMenuItem (config));
        }



        TEST_METHOD (ShouldEnableDisk2DebugMenuItem_cassetteOnlyAppleII_returnsFalse)
        {
            MachineConfig  config;
            SlotConfig     slot1;

            slot1.slot   = 1;
            slot1.device = "printer-card";
            config.slots.push_back (slot1);

            Assert::IsFalse (ShouldEnableDisk2DebugMenuItem (config));
        }



        TEST_METHOD (ShouldEnableDisk2DebugMenuItem_multipleDisk2Controllers_returnsTrue)
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

            Assert::IsTrue (ShouldEnableDisk2DebugMenuItem (config));
        }
    };
}
