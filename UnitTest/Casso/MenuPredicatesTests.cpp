#include "Pch.h"
#include "UiCommandTypes.h"
#include "Core/MachineConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MenuPredicatesTests
//
//  Headless coverage of the pure menu-enable predicates:
//
//    ShouldEnableDisk2DebugMenuItem -- the View -> Disk II Debug item is
//    enabled iff the active machine config wires a Disk II controller.
//
//    ShouldEnableWriteProtectMenuItem -- the Disk -> Write-protect item is
//    enabled iff something is mounted and it is not a damaged image.
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



        TEST_METHOD (ShouldEnableDisk2DebugMenuItem_Disk2InSlot6_returnsTrue)
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



        TEST_METHOD (ShouldEnableWriteProtectMenuItem_emptyDrive_returnsFalse)
        {
            WriteProtectInfo  wp;

            Assert::IsFalse (ShouldEnableWriteProtectMenuItem (false, wp),
                L"nothing mounted, nothing to protect");
        }



        TEST_METHOD (ShouldEnableWriteProtectMenuItem_ordinaryImage_returnsTrue)
        {
            WriteProtectInfo  wp;

            Assert::IsTrue (ShouldEnableWriteProtectMenuItem (true, wp),
                L"a mounted, undamaged image can be toggled either way");
        }



        TEST_METHOD (ShouldEnableWriteProtectMenuItem_alreadyProtected_returnsTrue)
        {
            WriteProtectInfo  wp;

            wp.imageFlag = true;

            Assert::IsTrue (ShouldEnableWriteProtectMenuItem (true, wp),
                L"an already-protected image still offers the toggle -- clearing "
                L"the flag is what a user does before writing to a disk");

            wp.imageFlag   = false;
            wp.userSetting = true;

            Assert::IsTrue (ShouldEnableWriteProtectMenuItem (true, wp),
                L"and the drive preference is a separate control, not a reason to "
                L"disable this one");
        }



        TEST_METHOD (ShouldEnableWriteProtectMenuItem_damagedImage_returnsFalse)
        {
            WriteProtectInfo  wp;

            wp.checksumMismatch = true;

            Assert::IsFalse (ShouldEnableWriteProtectMenuItem (true, wp),
                L"a damaged image must not be offered a toggle that refuses it -- "
                L"patching the flag recomputes the header checksum, and that "
                L"checksum failing to match is the evidence of damage");
        }



        TEST_METHOD (ShouldEnableWriteProtectMenuItem_damageOutranksTheOtherCauses)
        {
            // Damage is not one cause among several here: it is the only one
            // that makes the ACTION impossible rather than merely already-done.
            WriteProtectInfo  wp;

            wp.imageFlag        = true;
            wp.readOnlyFile     = true;
            wp.userSetting      = true;
            wp.checksumMismatch = true;

            Assert::IsFalse (ShouldEnableWriteProtectMenuItem (true, wp),
                L"damage disables the item whatever else is set");
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
