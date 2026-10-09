#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Config/UserConfigStore.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DriveWidgetState.h"
#include "UiTests/InMemoryFileSystem.h"
#include "WasapiAudio.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int     s_kRemountDrive    = 0;
static constexpr size_t  s_kRemountPattern  = 37;
static constexpr Byte    s_kRemountHighBit  = 0x80;
static constexpr size_t  s_kRemountBitIndex = 200;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskResetRemountHoldTests
//
//  The Reset and Power Cycle commands remount the slot 6 disks, which flushes
//  each one and reads its file back. While reverse execution holds the disks
//  that remount must not happen: it would write the file and throw away the
//  disk the machine has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskResetRemountHoldTests)
{
public:

    struct Shell
    {
        std::vector<std::unique_ptr<Disk2AudioSource>>  audioSources;
        WasapiAudio                                     audio;
        DriveWidgetController                           widgets;
        std::array<DriveWidgetState, 2>                 widgetState;
        std::array<DriveWidget, 2>                      chrome;
        CpuManager                                      cpuManager;
        std::wstring                                    machineName;
        UserConfigStore                                 config { L"C:\\Casso\\User" };
        InMemoryFileSystem                              fs;
        std::array<bool, 2>                             writeProtect {};
    };


    TEST_METHOD (RemountUnderHoldWritesNothingAndKeepsTheDisk)
    {
        TestMachine             machine ("Apple2e");
        auto                    shell   = std::make_unique<Shell>();
        size_t                  writes  = 0;
        DiskImageStore        & store   = machine.GetDiskStore();
        DiskImage             * image   = nullptr;



        store.SetImageReader    ([] (const std::string &, std::vector<Byte> & bytes) { bytes = MakeImage(); return S_OK; });
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink      ([&writes] (const std::string &, const std::vector<Byte> &) { writes++; return S_OK; });

        AssertSucceeded (store.Mount (6, s_kRemountDrive, "remount.nib"), L"Mount");

        image = store.GetImage (6, s_kRemountDrive);
        Assert::IsNotNull (image, L"a disk is mounted");

        image->WriteBit (0, s_kRemountBitIndex, image->ReadBit (0, s_kRemountBitIndex) ^ 1);
        store.SetFlushHold (true);

        {
            DiskManager  manager (machine, store, shell->audioSources, shell->audio, shell->widgets,
                                  shell->widgetState, shell->chrome, shell->cpuManager, shell->machineName,
                                  shell->config, shell->fs, shell->writeProtect);

            manager.RemountSlot6Disks();
        }

        Assert::AreEqual<size_t> (0, writes, L"the remount wrote nothing to the file");
        Assert::IsTrue (store.GetImage (6, s_kRemountDrive) == image, L"the same disk is still in the drive");
        Assert::IsTrue (image->IsDirty(), L"with the guest's write");
    }


    //  The Reset command resets the machine after the remount, and under the
    //  hold that writes nothing either, through the store or around it. The
    //  disk has no file behind it, so a write around the store has nowhere to
    //  go, yet it still clears the dirty bit without reaching the counting
    //  sink: the disk staying dirty is what shows nothing wrote it.
    TEST_METHOD (ResetUnderHoldWritesNothingAndKeepsTheWrites)
    {
        TestMachine             machine ("Apple2e");
        auto                    shell   = std::make_unique<Shell>();
        size_t                  writes  = 0;
        DiskImageStore        & store   = machine.GetDiskStore();
        DiskImage             * image   = nullptr;



        store.SetImageReader    ([] (const std::string &, std::vector<Byte> & bytes) { bytes = MakeImage(); return S_OK; });
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink      ([&writes] (const std::string &, const std::vector<Byte> &) { writes++; return S_OK; });

        AssertSucceeded (store.MountFromBytes (6, s_kRemountDrive, {}, DiskFormat::Nib, MakeImage()), L"MountFromBytes");

        image = store.GetImage (6, s_kRemountDrive);
        Assert::IsNotNull (image, L"a disk is mounted");

        machine.GetRefs().diskController->SetExternalDisk (s_kRemountDrive, image);

        image->WriteBit (0, s_kRemountBitIndex, image->ReadBit (0, s_kRemountBitIndex) ^ 1);
        store.SetFlushHold (true);

        {
            DiskManager  manager (machine, store, shell->audioSources, shell->audio, shell->widgets,
                                  shell->widgetState, shell->chrome, shell->cpuManager, shell->machineName,
                                  shell->config, shell->fs, shell->writeProtect);

            manager.RemountSlot6Disks();
        }

        machine.SoftReset();

        Assert::AreEqual<size_t> (0, writes, L"the reset wrote nothing through the store");
        Assert::IsTrue (store.GetImage (6, s_kRemountDrive) == image, L"the same disk is still in the drive");
        Assert::IsTrue (image->IsDirty(), L"nor around it: the guest's write is still unsaved");
    }


private:

    static std::vector<Byte> MakeImage()
    {
        std::vector<Byte>  bytes (NibbleImageCodec::kNibImageSize, 0);
        size_t             i     = 0;



        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> ((i * s_kRemountPattern) | s_kRemountHighBit);
        }

        return bytes;
    }
};
