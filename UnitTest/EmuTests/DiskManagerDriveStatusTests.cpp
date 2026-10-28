#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "EmuTests/DiskManagerRig.h"
#include "EmuTests/DeferredDiskIoQueue.h"
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
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

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
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

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
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

            manager.ApplyExternalWriteProtect (s_kShownDrive);
        }

        Assert::AreNotEqual (before, store.GetStatusGeneration(), L"the generation moved");
        Assert::IsTrue (store.GetBayStatus (6, s_kShownDrive).writeProtect.userSetting, L"and the bay has the user's setting");
    }


    //  A write-protect flag set on the image past the store does not move the
    //  generation, so the widget has the published bay until a publish after
    //  the store marks the change.
    TEST_METHOD (DriveWidgetsShowThePublishedBayNotTheLiveImage)
    {
        TestMachine       machine ("Apple2e");
        auto              rig     = std::make_unique<DiskManagerRig>();
        DiskImageStore  & store   = machine.GetDiskStore();
        bool              isRaw   = false;
        bool              isShown = false;
        HRESULT           hr      = S_OK;



        hr = store.MountFromBytes (6, s_kShownDrive, "shown.nib", DiskFormat::Nib, MakeImage());
        AssertSucceeded (hr, L"MountFromBytes");

        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

            (void) manager.PublishDriveStatus (true);
            manager.UpdateDriveWidgets();

            store.GetImage (6, s_kShownDrive)->SetUserWriteProtected (true);
            manager.UpdateDriveWidgets();
            isRaw = rig->widgetState[s_kShownDrive].writeProtect.userSetting;

            store.MarkStatusChanged();
            (void) manager.PublishDriveStatus (true);
            manager.UpdateDriveWidgets();
            isShown = rig->widgetState[s_kShownDrive].writeProtect.userSetting;
        }

        Assert::IsFalse (isRaw,  L"a write past the store does not reach the widget");
        Assert::IsTrue  (isShown, L"the published change does");
    }


    TEST_METHOD (DriveWidgetsShowThePublishedActivityNotTheLiveEngine)
    {
        TestMachine       machine  ("Apple2e");
        auto              rig      = std::make_unique<DiskManagerRig>();
        DiskImageStore  & store    = machine.GetDiskStore();
        bool              isLive   = false;
        bool              isShown  = false;



        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

            (void) manager.PublishDriveStatus (true);

            (void) machine.GetRefs().diskController->Read (s_kShownMotorOn);
            manager.UpdateDriveWidgets();
            isLive = rig->widgetState[s_kShownDrive].motorOn.load();

            (void) manager.PublishDriveStatus (true);
            manager.UpdateDriveWidgets();
            isShown = rig->widgetState[s_kShownDrive].motorOn.load();
        }

        Assert::IsFalse (isLive,  L"the motor the engine has is not shown before a publish");
        Assert::IsTrue  (isShown, L"and is shown after one");
    }


    TEST_METHOD (AUserWriteProtectThroughTheStoreReachesTheWidget)
    {
        TestMachine       machine ("Apple2e");
        auto              rig     = std::make_unique<DiskManagerRig>();
        DiskImageStore  & store   = machine.GetDiskStore();
        bool              isShown = false;
        HRESULT           hr      = S_OK;



        hr = store.MountFromBytes (6, s_kShownDrive, "protect.nib", DiskFormat::Nib, MakeImage());
        AssertSucceeded (hr, L"MountFromBytes");

        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

            store.SetUserWriteProtectFlag (6, s_kShownDrive, true);
            (void) manager.PublishDriveStatus (true);
            manager.UpdateDriveWidgets();
            isShown = rig->widgetState[s_kShownDrive].writeProtect.userSetting;
        }

        Assert::IsTrue (isShown, L"the padlock has the user's setting");
    }


    //  The widget has a mount made while paused once the service pass publishes it.
    TEST_METHOD (AMountWhilePausedShowsAfterTheServicePublish)
    {
        TestMachine       machine ("Apple2e");
        auto              rig     = std::make_unique<DiskManagerRig>();
        DiskImageStore  & store   = machine.GetDiskStore();
        std::wstring      shown;
        HRESULT           hr      = S_OK;



        PrepareStore (store);

        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

            hr = manager.MountDiskInSlot6 (s_kShownDrive, "paused.nib");
            (void) manager.PublishDriveStatus (true);
            manager.UpdateDriveWidgets();
            shown = rig->widgetState[s_kShownDrive].mountedImagePath;
        }

        AssertSucceeded (hr, L"MountDiskInSlot6");
        Assert::AreEqual (std::wstring (L"paused.nib"), shown, L"the door closes on the disk");
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


    //  A write-protect toggle's file work -- the attribute's read and set, the
    //  save and the probe after -- is posted to the store's I/O queue, and the
    //  change is reported once the last completion has run.
    TEST_METHOD (AWriteProtectToggleRunsItsFileWorkOnTheIoQueue)
    {
        TestMachine          machine    ("Apple2e");
        auto                 rig        = std::make_unique<DiskManagerRig>();
        DiskImageStore     & store      = machine.GetDiskStore();
        DeferredDiskIoQueue  queue;
        int                  reports    = 0;
        bool                 isReadOnly = false;
        HRESULT              hr         = S_OK;



        hr = store.MountFromBytes (6, s_kShownDrive, "toggle.nib", DiskFormat::Nib, MakeImage());
        AssertSucceeded (hr, L"MountFromBytes");

        AssertSucceeded (rig->fs.WriteAllText (L"toggle.nib", std::string()), L"the file exists");
        store.SetIoQueue (&queue);

        {
            DiskManager  manager (machine, store, rig->audioSources, rig->audio, rig->widgets,
                                  rig->widgetState, rig->chrome, rig->cpuManager, rig->machineName,
                                  rig->config, rig->fs, rig->writeProtect, rig->workingDirectory);

            manager.SetWriteProtectChangedCallback ([&reports] (const std::wstring &) { reports++; });

            hr = manager.ToggleImageWriteProtect (s_kShownDrive);
            AssertSucceeded (hr, L"ToggleImageWriteProtect");

            Assert::AreEqual ((size_t) 1, queue.GetPostedCount(), L"the attribute's read is posted");
            Assert::AreEqual (0, reports, L"and nothing is reported before it runs");

            queue.RunAll();
        }

        AssertSucceeded (rig->fs.GetReadOnlyAttribute (L"toggle.nib", isReadOnly), L"GetReadOnlyAttribute");
        Assert::IsTrue   (isReadOnly, L"the file is read-only");
        Assert::AreEqual (1, reports, L"and the change is reported after the last completion");

        store.SetIoQueue (nullptr);
    }
};
