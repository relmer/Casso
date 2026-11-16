#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Shell/DiskMenuRule.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMenuRuleTests
//
//  The drive each of the Storage menu's drive 1 and drive 2 commands acts
//  on: slot 6's Disk II drives, or on a //c Plus its built-in 3.5-inch drive
//  and the first 5.25-inch drive on its disk port.
//
////////////////////////////////////////////////////////////////////////////////

namespace DiskMenuRuleTests
{
    static MachineConfig  MakeMachine (const char * internalDevice)
    {
        MachineConfig  config;
        PortConfig     port;



        port.name   = "internal";
        port.device = internalDevice;

        config.ports.push_back (port);

        return config;
    }



    TEST_CLASS (DiskMenuRuleTests)
    {
    public:

        TEST_METHOD (HasBuiltIn35Drive_InternalApple35Drive)
        {
            Assert::IsTrue  (DiskMenuRule::HasBuiltIn35Drive (MakeMachine (kpszApple35DriveDevice)));
            Assert::IsFalse (DiskMenuRule::HasBuiltIn35Drive (MakeMachine ("disk-iic-drive")), L"a //c's disk port");
            Assert::IsFalse (DiskMenuRule::HasBuiltIn35Drive (MachineConfig()),               L"no ports");
        }


        TEST_METHOD (Resolve_NoBuiltIn35_DiskIiDrives)
        {
            DiskMenuDrive  drive1 = DiskMenuRule::Resolve (0, false, 0);
            DiskMenuDrive  drive2 = DiskMenuRule::Resolve (1, false, 0);



            Assert::IsFalse (drive1.isBuiltIn35);
            Assert::AreEqual (0, drive1.diskIiDrive);
            Assert::IsFalse (drive2.isBuiltIn35);
            Assert::AreEqual (1, drive2.diskIiDrive);
        }


        TEST_METHOD (Resolve_BuiltIn35_Drive1IsBuiltIn)
        {
            DiskMenuDrive  drive1 = DiskMenuRule::Resolve (0, true, 2);



            Assert::IsTrue  (drive1.isBuiltIn35);
            Assert::IsFalse (drive1.IsDiskIi());
        }


        TEST_METHOD (Resolve_BuiltIn35_Drive2IsFirstPortDrive)
        {
            DiskMenuDrive  attached = DiskMenuRule::Resolve (1, true, 1);
            DiskMenuDrive  none     = DiskMenuRule::Resolve (1, true, 0);



            Assert::IsFalse  (attached.isBuiltIn35);
            Assert::AreEqual (0, attached.diskIiDrive);
            Assert::IsFalse  (none.isBuiltIn35);
            Assert::IsFalse  (none.IsDiskIi(), L"no 5.25-inch drive on the port");
        }
    };
}
