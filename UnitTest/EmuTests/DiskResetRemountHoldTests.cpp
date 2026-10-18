#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Config/UserConfigStore.h"
#include "Debugger/Reverse/DivergenceGate.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "Shell/Components/ShellDisks.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DriveWidgetState.h"
#include "EmuTests/OwnerThreadStandIn.h"
#include "EmuTests/DiskManagerRig.h"
#include "UiTests/InMemoryFileSystem.h"
#include "WasapiAudio.h"

#include "resource.h"

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

    using Shell = DiskManagerRig;



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


    //  Inserting a salvaged copy from the window only queues the insert for
    //  the CPU thread, which holds the disks: the drive keeps the disk it is
    //  running and its unsaved writes, and the queued insert is journaled and
    //  gated like any other.
    TEST_METHOD (SalvagedCopyIsQueuedForTheCpuThreadAndLeavesTheBayAlone)
    {
        using Posted = std::pair<WORD, std::string>;

        TestMachine             machine  ("Apple2e");
        auto                    shell    = std::make_unique<Shell>();
        size_t                  writes   = 0;
        size_t                  changes  = 0;
        DiskImageStore        & store    = machine.GetDiskStore();
        DiskImage             * image    = nullptr;
        std::vector<Posted>     posted;
        OwnerThreadStandIn      standIn;
        InputRecord             input;
        bool                    isInput  = false;
        HRESULT                 hr       = S_OK;
        HRESULT                 hrInsert = E_FAIL;



        store.SetImageReader         ([] (const std::string &, std::vector<Byte> & bytes) { bytes = MakeImage(); return S_OK; });
        store.SetIdentityReader      ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink           ([&writes] (const std::string &, const std::vector<Byte> &) { writes++; return S_OK; });

        hr = store.Mount (6, s_kRemountDrive, "damaged.nib");
        AssertSucceeded (hr, L"Mount");

        image = store.GetImage (6, s_kRemountDrive);
        Assert::IsNotNull (image, L"a disk is mounted");

        image->WriteBit (0, s_kRemountBitIndex, image->ReadBit (0, s_kRemountBitIndex) ^ 1);
        store.SetMediaChangeListener ([&changes] { changes++; });

        shell->cpuManager.SetCommandGate ([&posted] (WORD id, const std::string & payload) { posted.emplace_back (id, payload); return true; });

        {
            DiskManager  manager (machine, store, shell->audioSources, shell->audio, shell->widgets,
                                  shell->widgetState, shell->chrome, shell->cpuManager, shell->machineName,
                                  shell->config, shell->fs, shell->writeProtect);

            hr = standIn.Take (store.GetThreadOwnership());
            AssertSucceeded (hr, L"Take");

            hrInsert = ShellDisks::InsertSalvagedCopy (manager, s_kRemountDrive, "damaged (salvaged).nib");

            hr = standIn.GiveBack();
            AssertSucceeded (hr, L"GiveBack");
        }

        AssertSucceeded (hrInsert, L"InsertSalvagedCopy");
        Assert::AreEqual<size_t> (1, posted.size(), L"one command was posted");
        Assert::AreEqual<int> (IDM_DISK_INSERT1, posted[0].first, L"an insert into drive 1");
        Assert::AreEqual (std::string ("damaged (salvaged).nib"), posted[0].second, L"of the salvaged copy");
        Assert::IsTrue (shell->cpuManager.HasPendingCommands(), L"waiting for the CPU thread");
        Assert::IsTrue (store.GetImage (6, s_kRemountDrive) == image, L"the drive still holds the disk it was running");
        Assert::AreEqual<size_t> (0, writes, L"nothing was written from the window");
        Assert::IsTrue (image->IsDirty(), L"the guest's write is still the drive's");
        Assert::AreEqual<size_t> (0, changes, L"reverse execution was told nothing from the window");

        isInput = CpuCommandDispatcher::TryGetJournalInput (EmulatorCommand { posted[0].first, posted[0].second }, input);

        Assert::IsTrue (isInput, L"the insert is journaled");
        Assert::IsTrue (input.kind == InputKind::DiskMount, L"as a mount");
        Assert::IsTrue (DivergenceGate::IsStateChangingCommand (posted[0].first), L"and gated");
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
