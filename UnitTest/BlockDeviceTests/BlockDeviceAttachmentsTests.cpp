#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/BlockDeviceAttachments.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceAttachmentsTests
//
//  --hd and --smartport images applied to a machine's config for one run.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockDeviceAttachmentsTests)
{
public:

    using Attachment = BlockDeviceAttachments::Attachment;


    static MachineConfig MakeConfig()
    {
        MachineConfig  config;
        SlotConfig     disk;
        SlotConfig     scsi;



        disk.slot   = 6;
        disk.device = kpszDiskIiDevice;
        scsi.slot   = 7;
        scsi.device = kpszAppleScsiCardDevice;
        scsi.ports.resize (7);
        scsi.ports[2].device = kpszHd20scDevice;

        config.slots = { disk, scsi };

        return config;
    }


    static Attachment MakeHd (int slot, int unit, const std::string & image)
    {
        Attachment  attachment;



        attachment.slot  = slot;
        attachment.unit  = unit;
        attachment.image = image;

        return attachment;
    }


    //  WITHOUT A UNIT, THE IMAGE GOES ON THE LOWEST SCSI ID WITH A DRIVE; with
    //  one, on that ID, which gets a drive.
    TEST_METHOD (HdGoesOnTheUnit)
    {
        MachineConfig  config = MakeConfig();
        std::string    error;



        Assert::AreEqual (S_OK, BlockDeviceAttachments::Apply ({ MakeHd (7, -1, "a.hdv"), MakeHd (7, 5, "b.hdv") }, {}, config, error));

        Assert::AreEqual (std::string ("a.hdv"), config.slots[1].ports[2].image);
        Assert::AreEqual (std::string ("b.hdv"), config.slots[1].ports[5].image);
        Assert::AreEqual (std::string (kpszHd20scDevice), config.slots[1].ports[5].device);
    }


    //  A SLOT WITH NO BLOCK DEVICE CARD FAILS, in the fixed error layout, and
    //  the config is unchanged.
    TEST_METHOD (SlotWithoutACardFails)
    {
        MachineConfig  config = MakeConfig();
        std::string    error;
        HRESULT        hr     = BlockDeviceAttachments::Apply ({ MakeHd (7, 1, "a.hdv"), MakeHd (6, -1, "b.hdv") }, {}, config, error);



        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (std::string ("Error: no block device in slot 6"), error.substr (0, error.find ('\n')));
        Assert::IsTrue   (error.find ("Slot 6 has a Disk II.") != std::string::npos);
        Assert::IsTrue   (config.slots[1].ports[1].image.empty(), L"the first attachment is not applied either");
    }


    //  --SMARTPORT GOES ON THE DISK PORT'S CHAIN, adding drives up to the
    //  position, and fails on a machine without one.
    TEST_METHOD (SmartPortGoesOnTheChain)
    {
        MachineConfig  config;
        MachineConfig  slotted = MakeConfig();
        PortConfig     disk;
        Attachment     attachment;
        std::string    error;
        HRESULT        hr      = S_OK;



        disk.name    = "disk";
        config.ports = { disk };

        attachment.chain = 2;
        attachment.image = "u.po";

        Assert::AreEqual (S_OK, BlockDeviceAttachments::Apply ({}, { attachment }, config, error));
        Assert::AreEqual ((size_t) 2, config.ports[0].chain.size());
        Assert::AreEqual (std::string ("u.po"), config.ports[0].chain[1].image);

        hr = BlockDeviceAttachments::Apply ({}, { attachment }, slotted, error);
        Assert::IsTrue (FAILED (hr));
    }
};
