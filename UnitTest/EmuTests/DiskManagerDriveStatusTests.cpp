#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "EmuTests/DiskManagerRig.h"
#include "HResultAssert.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Shell/DiskManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int     s_kShownDrive   = 0;
static constexpr size_t  s_kShownPattern = 43;
static constexpr Byte    s_kShownHighBit = 0x80;
static constexpr Word    s_kShownMotorOn = 0xC0E9;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManagerDriveStatusTests
//
//  The DiskManager's drive status: a mount is published before its
//  completion is posted, and a publish that changes a bay calls the status
//  callback once, while one that changes nothing or only activity calls it
//  not at all.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskManagerDriveStatusTests)
{
public:

    TEST_METHOD (TheMountIsPublishedBeforeItsCompletion)
    {
        TestMachine       machine ("Apple2e");
        auto              rig     = std::make_unique<DiskManagerRig>();
        DiskImageStore  & store   = machine.GetDiskStore();
        std::string       seen;
        HRESULT           hr      = S_OK;



        PrepareStore (store);

        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect);

            manager.SetMountCompletedCallback ([&] (int, const std::string &, HRESULT, const MountDiagnosis &)
            {
                seen = manager.TakeLatestDriveStatus()->GetBay (6, s_kShownDrive).path;
            });

            hr = manager.MountDiskInSlot6 (s_kShownDrive, "shown.nib");
        }

        AssertSucceeded (hr, L"MountDiskInSlot6");
        Assert::AreEqual (std::string ("shown.nib"), seen, L"the completion finds the mount already published");
    }


    TEST_METHOD (APublishWhilePausedGivesAMountAsABayChangeAndOneWake)
    {
        TestMachine        machine ("Apple2e");
        auto               rig     = std::make_unique<DiskManagerRig>();
        DiskImageStore   & store   = machine.GetDiskStore();
        int                wakes   = 0;
        DriveStatusChange  mounted = DriveStatusChange::None;
        DriveStatusChange  idle    = DriveStatusChange::None;
        DriveStatusChange  motor   = DriveStatusChange::None;
        bool               isShown = false;
        HRESULT            hr      = S_OK;



        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect);

            (void) manager.PublishDriveStatus (true);
            manager.SetStatusWake ([&wakes] { wakes++; });

            hr = store.MountFromBytes (6, s_kShownDrive, "paused.nib", DiskFormat::Nib, MakeImage());
            AssertSucceeded (hr, L"MountFromBytes");

            mounted = manager.PublishDriveStatus (true);
            isShown = manager.TakeLatestDriveStatus()->GetBay (6, s_kShownDrive).isMounted;
            idle    = manager.PublishDriveStatus (true);

            (void) machine.GetRefs().diskController->Read (s_kShownMotorOn);
            motor = manager.PublishDriveStatus (true);
        }

        Assert::IsTrue (mounted == DriveStatusChange::Bays,     L"the mount is a bay change");
        Assert::IsTrue (isShown,                                L"and the published bay has the disk");
        Assert::IsTrue (idle == DriveStatusChange::None,        L"a publish with nothing changed changes nothing");
        Assert::IsTrue (motor == DriveStatusChange::Activity,   L"the motor alone is activity");
        Assert::AreEqual (1, wakes,                             L"and only the bay change woke the window");
    }


    //  The write-protect state DiskManager applies to an arriving disk goes
    //  through the store, so it moves the status generation.
    TEST_METHOD (ApplyingExternalWriteProtectMovesTheGeneration)
    {
        TestMachine       machine ("Apple2e");
        auto              rig     = std::make_unique<DiskManagerRig>();
        DiskImageStore  & store   = machine.GetDiskStore();
        uint64_t          before  = 0;
        HRESULT           hr      = S_OK;



        hr = store.MountFromBytes (6, s_kShownDrive, "protect.nib", DiskFormat::Nib, MakeImage());
        AssertSucceeded (hr, L"MountFromBytes");

        rig->writeProtect[s_kShownDrive] = true;
        before = store.GetStatusGeneration();

        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect);

            manager.ApplyExternalWriteProtect (s_kShownDrive, "protect.nib");
        }

        Assert::AreNotEqual (before, store.GetStatusGeneration(), L"the generation moved");
        Assert::IsTrue (store.GetBayStatus (6, s_kShownDrive).writeProtect.userSetting, L"and the bay has the user's setting");
    }


private:

    static void PrepareStore (DiskImageStore & store)
    {
        store.SetImageReader    ([] (const std::string &, std::vector<Byte> & bytes) { bytes = MakeImage(); return S_OK; });
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink      ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
    }


    static std::vector<Byte> MakeImage()
    {
        std::vector<Byte>  bytes (NibbleImageCodec::kNibImageSize, 0);
        size_t             i     = 0;



        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> ((i * s_kShownPattern) | s_kShownHighBit);
        }

        return bytes;
    }
};