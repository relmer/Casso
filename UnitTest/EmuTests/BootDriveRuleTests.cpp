#include "Pch.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Shell/BootDriveRule.h"
#include "Ui/Settings/SettingsPanelState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootDriveRuleTests
//
//  The drive the startup form opens for, in the order the Apple II looks for
//  a boot disk, and where a pick for a drive other than slot 6's Disk II is
//  put.
//
////////////////////////////////////////////////////////////////////////////////

namespace BootDriveRuleTests
{
    using M = DeskDriveModel;


    static SlotConfig  MakeSlot (int slot, const char * device, std::vector<PortConfig> ports = {}, bool enabled = true)
    {
        SlotConfig  config;



        config.slot    = slot;
        config.device  = device;
        config.ports   = std::move (ports);
        config.enabled = enabled;

        return config;
    }


    static PortConfig  MakePort (const char * name, const char * device, const char * image = "")
    {
        PortConfig  port;



        port.name   = name;
        port.device = device;
        port.image  = image;

        return port;
    }


    static MachineConfig  MakeDiskIiMachine()
    {
        MachineConfig  config;



        config.slots = { MakeSlot (1, "parallel-printer"), MakeSlot (6, kpszDiskIiDevice) };

        return config;
    }





    TEST_CLASS (BootDriveRuleTests)
    {
    public:

        //  The //c boots from its built-in drive, drive 1 of slot 6.
        TEST_METHOD (BuiltInDrive_IsAskedUntilDrive1HasASavedDisk)
        {
            MachineConfig  config;
            BootDrive      drive;



            config.ports = { MakePort ("disk", "") };

            Assert::IsTrue  (BootDriveRule::TryFindDriveToFill (config, true, false, drive));
            Assert::IsTrue  (drive.drive.model == M::Disk2c);
            Assert::IsTrue  (drive.drive.builtIn);
            Assert::IsTrue  (drive.IsDiskIi());

            Assert::IsFalse (BootDriveRule::TryFindDriveToFill (config, true, true, drive));
        }


        //  The //c Plus boots from its internal 3.5-inch drive, whose image is
        //  on its internal port.
        TEST_METHOD (InternalApple35_IsAskedUntilItsPortHasAnImage)
        {
            MachineConfig  config;
            BootDrive      drive;



            config.ports = { MakePort (BootDriveRule::kpszInternalPort, kpszApple35DriveDevice), MakePort ("disk", "") };

            Assert::IsTrue  (BootDriveRule::TryFindDriveToFill (config, false, false, drive));
            Assert::IsTrue  (drive.drive.model == M::Apple35Plus);
            Assert::AreEqual (std::string (BootDriveRule::kpszInternalPort), drive.portName);
            Assert::IsFalse (drive.IsDiskIi());

            config.ports[0].image = "C:\\Disks\\system.po";
            Assert::IsFalse (BootDriveRule::TryFindDriveToFill (config, false, true, drive), L"a saved 5.25-inch disk is not the boot drive's");
        }


        TEST_METHOD (DiskIiInSlot6_IsAskedUntilDrive1HasASavedDisk)
        {
            MachineConfig  config = MakeDiskIiMachine();
            BootDrive      drive;



            Assert::IsTrue  (BootDriveRule::TryFindDriveToFill (config, false, false, drive));
            Assert::IsTrue  (drive.drive.model == M::DiskII);
            Assert::AreEqual (6, drive.drive.slot);
            Assert::AreEqual (0, drive.drive.drive);

            Assert::IsFalse (BootDriveRule::TryFindDriveToFill (config, false, true, drive));
        }


        //  With slot 6's Disk II turned off, a 3.5-inch card's first drive is
        //  the boot drive, and its image is on the card's port.
        TEST_METHOD (DiskIiOff_Apple35Card_IsAsked)
        {
            MachineConfig  config;
            BootDrive      drive;



            config.slots = { MakeSlot (5, kpszApple35CardDevice, { MakePort ("", kpszApple35DriveDevice), MakePort ("", kpszApple35DriveDevice) }),
                             MakeSlot (6, kpszDiskIiDevice, {}, false) };

            Assert::IsTrue  (BootDriveRule::TryFindDriveToFill (config, false, true, drive), L"drive 1's saved disk is in a drive the machine doesn't have");
            Assert::IsTrue  (drive.drive.model == M::Apple35Drive);
            Assert::AreEqual (5, drive.drive.slot);
            Assert::AreEqual (0, drive.drive.drive);
            Assert::IsTrue  (drive.portName.empty());

            config.slots[0].ports[0].image = "C:\\Disks\\system.po";
            Assert::IsFalse (BootDriveRule::TryFindDriveToFill (config, false, false, drive));
        }


        TEST_METHOD (UniDiskCard_IsAskedForItsUniDisk)
        {
            MachineConfig  config;
            BootDrive      drive;



            config.slots = { MakeSlot (5, kpszLironCardDevice, { MakePort ("", kpszUniDisk35Device) }) };

            Assert::IsTrue (BootDriveRule::TryFindDriveToFill (config, false, false, drive));
            Assert::IsTrue (drive.drive.model == M::UniDisk35);
            Assert::AreEqual (5, drive.drive.slot);
        }


        //  Slot 7 comes before slot 6. A hard disk with an image boots; one
        //  with none, or with the stock image, is passed over.
        TEST_METHOD (HardDiskInSlot7_BootsWithAnImage_AndIsPassedOverWithout)
        {
            MachineConfig  config = MakeDiskIiMachine();
            BootDrive      drive;



            config.slots.push_back (MakeSlot (7, kpszAppleScsiCardDevice, { MakePort ("", ""), MakePort ("", kpszHd20scDevice) }));

            Assert::IsTrue  (BootDriveRule::TryFindDriveToFill (config, false, false, drive));
            Assert::IsTrue  (drive.drive.model == M::DiskII);

            config.slots.back().ports[1].image = kpszStockImage;
            Assert::IsTrue  (BootDriveRule::TryFindDriveToFill (config, false, false, drive), L"the stock image is an empty volume");
            Assert::IsTrue  (drive.drive.model == M::DiskII);

            config.slots.back().ports[1].image = "C:\\Disks\\hard.hdv";
            Assert::IsFalse (BootDriveRule::TryFindDriveToFill (config, false, false, drive));
        }


        TEST_METHOD (NoDiskDrive_NothingIsAsked)
        {
            MachineConfig  config;
            BootDrive      drive;



            config.slots = { MakeSlot (1, "parallel-printer"), MakeSlot (6, kpszDiskIiDevice, {}, false) };

            Assert::IsFalse (BootDriveRule::TryFindDriveToFill (config, false, false, drive));
        }


        TEST_METHOD (SetImage_PutsThePickOnTheDrivesPort)
        {
            MachineConfig  config;
            BootDrive      internal;
            BootDrive      card;



            config.ports = { MakePort (BootDriveRule::kpszInternalPort, kpszApple35DriveDevice) };
            config.slots = { MakeSlot (5, kpszApple35CardDevice, { MakePort ("", kpszApple35DriveDevice), MakePort ("", kpszApple35DriveDevice) }) };

            internal.drive    = DeskDrive { M::Apple35Plus, 0, 0 };
            internal.portName = BootDriveRule::kpszInternalPort;
            card.drive        = DeskDrive { M::Apple35Drive, 5, 1 };

            BootDriveRule::SetImage (config, internal, "C:\\Disks\\a.po");
            BootDriveRule::SetImage (config, card, "C:\\Disks\\b.po");

            Assert::AreEqual (std::string ("C:\\Disks\\a.po"), config.ports[0].image);
            Assert::IsTrue   (config.slots[0].ports[0].image.empty());
            Assert::AreEqual (std::string ("C:\\Disks\\b.po"), config.slots[0].ports[1].image);
        }


        //  The saved settings take the image on a machine port, as the
        //  machine's document has it.
        TEST_METHOD (SetMachinePortImage_IsWrittenOnThePort)
        {
            static constexpr const char *  s_kpszShipped = R"JSON({
                "name": "Apple //c Plus",
                "ports": [
                    { "name": "internal", "device": "apple35-drive" },
                    { "name": "disk",     "device": "" }
                ]
            })JSON";

            JsonValue           shipped;
            JsonParseError      error;
            SettingsPanelState  state;
            std::string         saved;



            AssertSucceeded (JsonParser::Parse (s_kpszShipped, shipped, error));
            AssertSucceeded (state.LoadFromMachine ("Apple2cPlus", shipped, shipped));

            AssertSucceeded (state.SetMachinePortImage ("internal", "C:\\Disks\\system.po"));
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), state.SetMachinePortImage ("serial9", "C:\\Disks\\system.po"));

            saved = JsonWriter::Write (state.BuildCurrentJson());
            Assert::AreNotEqual (std::string::npos, saved.find ("\"image\""), TextEncoding::Utf8ToWide (saved).c_str());
            Assert::AreNotEqual (std::string::npos, saved.find ("system.po"));
        }
    };
}
