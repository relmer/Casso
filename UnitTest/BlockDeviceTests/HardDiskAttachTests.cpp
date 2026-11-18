#include "Pch.h"

#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Shell/EmulatorShell.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HardDiskAttachTests
//
//  A hard disk's image changed while the machine runs, through
//  MachineStorage::AttachHardDisk: an image dropped on the drive or chosen from
//  its file dialog, once the confirmation that the machine restarts is
//  accepted. The image takes the machine build's checks, and when it fails
//  them the drive has its image from before.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HardDiskAttachTests)
{
public:

    //  THE NEW IMAGE REPLACES THE OLD ONE, and the machine's configuration
    //  has it, so the next build attaches it too.
    TEST_METHOD (Attach_ReplacesTheImage)
    {
        FakeDiskFileIo  io;
        std::wstring    error;
        HRESULT         hr = S_OK;



        AddFile (io, kOldPath, kHardDiskBlocks);
        AddFile (io, kNewPath, kHardDiskBlocks);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { ScsiCard (kOldPath) }, io);

        hr = machine.GetStorage().AttachHardDisk (kScsiSlot, 0, kNewPath, error);

        AssertSucceeded (hr);
        Assert::AreEqual (std::string (kNewPath), Target (machine).GetMedia().GetPath());
        Assert::AreEqual (std::string (kNewPath), FindPort (machine, kScsiSlot, 0).image, L"the configuration has the new image");

        machine.PowerCycle();
        Assert::AreEqual (std::string (kNewPath), Target (machine).GetMedia().GetPath(), L"after the power cycle, the drive has the new image");
    }


    //  A FLOPPY IMAGE isn't attached to a hard disk, and the drive has its
    //  image from before.
    TEST_METHOD (Attach_FloppyImage_PutsTheOldImageBack)
    {
        FakeDiskFileIo  io;
        std::wstring    error;
        HRESULT         hr = S_OK;



        AddFile (io, kOldPath,    kHardDiskBlocks);
        AddFile (io, kFloppyPath, kFloppyBlocks);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { ScsiCard (kOldPath) }, io);

        hr = machine.GetStorage().AttachHardDisk (kScsiSlot, 0, kFloppyPath, error);

        Assert::IsTrue  (FAILED (hr));
        Assert::IsTrue  (error.find (L"floppy") != std::wstring::npos, error.c_str());
        Assert::IsTrue  (Target (machine).GetMedia().IsAttached());
        Assert::AreEqual (std::string (kOldPath), Target (machine).GetMedia().GetPath());
        Assert::AreEqual (std::string (kOldPath), FindPort (machine, kScsiSlot, 0).image);
    }


    //  WITH A MISSING FILE the drive has its image from before.
    TEST_METHOD (Attach_MissingFile_PutsTheOldImageBack)
    {
        FakeDiskFileIo  io;
        std::wstring    error;
        HRESULT         hr = S_OK;



        AddFile (io, kOldPath, kHardDiskBlocks);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { ScsiCard (kOldPath) }, io);

        hr = machine.GetStorage().AttachHardDisk (kScsiSlot, 0, "gone.po", error);

        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (error.empty(), L"an error for the notice");
        Assert::AreEqual (std::string (kOldPath), Target (machine).GetMedia().GetPath());
    }


    //  A PROFILE'S IMAGE is changed the same way.
    TEST_METHOD (Attach_ProFile)
    {
        FakeDiskFileIo          io;
        std::wstring            error;
        HRESULT                 hr    = S_OK;
        TestMachine::BlockCard  card;



        AddFile (io, kOldPath, kHardDiskBlocks);
        AddFile (io, kNewPath, kHardDiskBlocks);

        card.slot   = kProFileSlot;
        card.device = kpszProFileCardDevice;
        card.ports.assign (1, PortConfig {});
        card.ports[0].device = kpszProFile5Device;
        card.ports[0].image  = kOldPath;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { card }, io);

        hr = machine.GetStorage().AttachHardDisk (kProFileSlot, 0, kNewPath, error);

        AssertSucceeded (hr);
        Assert::AreEqual (std::string (kNewPath), machine.GetRefs().proFileCards[0]->GetDrive().GetMedia().GetPath());
    }


    //  THE CONFIRMATION'S TEXT has the restart, the drive and the file.
    //  Restart is the default, and Cancel is the window's close.
    TEST_METHOD (ChangeDialog_SaysTheMachineRestarts)
    {
        DialogDefinition  def = EmulatorShell::MakeHardDiskChangeDialog (L"HD20SC, SCSI ID 6", L"big disk.hdv");
        std::wstring      body;



        for (const DialogTextRun & run : def.body)
        {
            body += run.text;
        }

        Assert::IsTrue (body.find (L"restarts the machine") != std::wstring::npos, body.c_str());
        Assert::IsTrue (body.find (L"HD20SC, SCSI ID 6")    != std::wstring::npos);
        Assert::IsTrue (body.find (L"big disk.hdv")         != std::wstring::npos);

        Assert::AreEqual ((size_t) 2, def.buttons.size());
        Assert::AreEqual (std::wstring (L"Restart"), def.buttons[0].label);
        Assert::AreEqual (EmulatorShell::kHardDiskChangeRestart, def.buttons[0].resultCode);
        Assert::IsTrue   (def.buttons[0].isDefault);
        Assert::AreEqual (std::wstring (L"Cancel"), def.buttons[1].label);
        Assert::IsTrue   (def.buttons[1].isCancel);
        Assert::AreNotEqual (EmulatorShell::kHardDiskChangeRestart, def.buttons[1].resultCode);
    }


    //  THE NEW IMAGE IS SAVED with the machine, set by its card's slot and
    //  the drive's port as the Machine tab sets it. A slot without a card
    //  fails.
    TEST_METHOD (SaveImage_ReplacesTheImageInTheMachineSettings)
    {
        SettingsPanelState   st;
        JsonValue            machine;
        JsonParseError       parseError;
        std::string          text;
        JsonWriter::Options  opts;
        const char         * json = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "TestMachine",
            "internalDevices": [ { "type": "keyboard" } ],
            "slots": [ { "slot": 7, "device": "apple-scsi-card",
                         "ports": [ "", { "device": "hd20sc", "image": "C:/a.hdv" } ] } ]
        })JSON";



        AssertSucceeded (JsonParser::Parse (json, machine, parseError));
        AssertSucceeded (st.LoadFromMachine ("X", machine, machine));

        AssertSucceeded (st.SetSlotPortImage (7, 1, "C:/b.hdv"));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), st.SetSlotPortImage (5, 1, "C:/b.hdv"));

        opts.fPretty = false;
        AssertSucceeded (JsonWriter::Write (st.BuildCurrentJson(), opts, text));

        Assert::IsTrue (text.find ("\"image\":\"C:/b.hdv\"") != std::string::npos, L"the new image is saved");
        Assert::IsTrue (text.find ("C:/a.hdv") == std::string::npos, L"in place of the old one");
    }


    //  A SLOT WITHOUT A HARD DISK fails.
    TEST_METHOD (Attach_NoHardDisk_Fails)
    {
        FakeDiskFileIo  io;
        std::wstring    error;
        HRESULT         hr = S_OK;



        AddFile (io, kNewPath, kHardDiskBlocks);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { ScsiCard (kOldPath) }, io);

        hr = machine.GetStorage().AttachHardDisk (kProFileSlot, 0, kNewPath, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), hr);
    }


private:

    static constexpr int           kScsiSlot       = 7;
    static constexpr int           kProFileSlot    = 4;
    static constexpr size_t        kHardDiskBlocks = 4096;
    static constexpr size_t        kFloppyBlocks   = 1600;
    static constexpr const char *  kOldPath        = "old.po";
    static constexpr const char *  kNewPath        = "new.po";
    static constexpr const char *  kFloppyPath     = "floppy.po";


    static void AddFile (FakeDiskFileIo & io, const std::string & path, size_t blocks)
    {
        io.files[path]  = std::vector<Byte> (blocks * 512, 0);
        io.stamps[path] = FileStamp { blocks * 512, 1 };
    }


    //  The SCSI card in slot 7 with an HD20SC at SCSI ID 0 on `image`.
    static TestMachine::BlockCard ScsiCard (const std::string & image)
    {
        TestMachine::BlockCard  card;



        card.slot   = kScsiSlot;
        card.device = kpszAppleScsiCardDevice;
        card.ports.assign (1, PortConfig {});
        card.ports[0].device = kpszHd20scDevice;
        card.ports[0].image  = image;

        return card;
    }


    static ScsiHardDisk & Target (TestMachine & machine)
    {
        return machine.GetRefs().scsiCards[0]->GetTarget (0);
    }


    static const PortConfig & FindPort (TestMachine & machine, int slot, size_t port)
    {
        for (const SlotConfig & entry : machine.GetConfig().slots)
        {
            if (entry.slot == slot && port < entry.ports.size())
            {
                return entry.ports[port];
            }
        }

        Assert::Fail (L"the slot has the port");
    }
};
