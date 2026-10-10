#include "Pch.h"

#include "../EhmTestHelper.h"
#include "resource.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "EmuTests/OwnerThreadStandIn.h"
#include "EmuTests/TestMachine.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Shell/CpuManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace UnitTestHelpers;


static constexpr int                   s_kOwnedSlot      = 6;
static constexpr int                   s_kOwnedDrive     = 0;
static constexpr Word                  s_kDriveIoAddress = 0xC0E0;
static constexpr uint64_t              s_kPowerCycleSeed = 1;
static constexpr size_t                s_kNibPattern     = 29;
static constexpr Byte                  s_kNibHighBit     = 0x80;
static constexpr std::chrono::seconds  s_kCommandWait    { 5 };

//  Release compiles EHM assertions out, so a row's count only moves in Debug.
#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
static constexpr bool  s_kAreAssertsCounted = true;
#else
static constexpr bool  s_kAreAssertsCounted = false;
#endif





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOwnershipTests
//
//  The disk store and every Disk II controller wired to it belong to one
//  thread at a time. While another thread holds the store, every guarded entry
//  point asserts when the test thread calls it, the exempt ones and the bus and
//  tick paths do not, and a built machine's controller checks the store's
//  token rather than its own. The token follows the CPU thread from its start
//  to its stop.
//
//  The sweeps call each entry point on a store whose every seam is faked,
//  because a guarded body still runs after its assertion is counted.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskOwnershipTests)
{
public:

    struct EntryPoint
    {
        const wchar_t          * name = nullptr;
        std::function<void()>    call;
    };


    TEST_METHOD (EveryStoreEntryPointAssertsOffItsOwner)
    {
        auto                     store   = std::make_unique<DiskImageStore>();
        FakeDiskFileIo           fileIo;
        OwnerThreadStandIn       standIn;
        std::vector<EntryPoint>  rows;
        std::wstring             missing;
        HRESULT                  hr      = S_OK;



        PrepareStore (*store, fileIo);

        rows = MakeStoreRows (*store);

        hr = standIn.Take (store->GetThreadOwnership());
        AssertSucceeded (hr, L"Take");

        missing = SweepOffTheOwner (rows);

        hr = standIn.GiveBack();
        AssertSucceeded (hr, L"GiveBack");

        Assert::IsTrue (missing.empty(), (L"store entry points that did not assert off the owner:" + missing).c_str());
    }


    TEST_METHOD (ExemptStoreEntryPointsDoNotAssert)
    {
        auto                     store   = std::make_unique<DiskImageStore>();
        FakeDiskFileIo           fileIo;
        OwnerThreadStandIn       standIn;
        const DiskImageStore   & view    = *store;
        DiskFormat               format  = DiskFormat::Nib;
        HRESULT                  hr      = S_OK;



        PrepareStore (*store, fileIo);

        hr = standIn.Take (store->GetThreadOwnership());
        AssertSucceeded (hr, L"Take");

        store->NoteExternalChange ("owned.nib", ExternalChangeIntent::Unstated);
        (void) store->GetThreadOwnership();
        (void) view.GetThreadOwnership();
        (void) DiskImageStore::GetSourceFormatByExtension ("owned.nib", format);
        (void) DiskImageStore::IsMountableImageExtension (std::string ("owned.nib"));
        (void) DiskImageStore::MakeRecoveryPath ("owned.nib", 0);

        hr = standIn.GiveBack();
        AssertSucceeded (hr, L"GiveBack");
    }


    TEST_METHOD (EveryControllerEntryPointAssertsOffItsOwner)
    {
        auto                     store      = std::make_unique<DiskImageStore>();
        auto                     controller = std::make_unique<Disk2Controller> (s_kOwnedSlot);
        FakeDiskFileIo           fileIo;
        OwnerThreadStandIn       standIn;
        std::vector<EntryPoint>  rows;
        std::wstring             missing;
        HRESULT                  hr         = S_OK;



        PrepareStore (*store, fileIo);
        controller->SetThreadOwnership (store->GetThreadOwnership());

        rows = MakeControllerRows (*controller, store->GetThreadOwnership());

        hr = standIn.Take (store->GetThreadOwnership());
        AssertSucceeded (hr, L"Take");

        missing = SweepOffTheOwner (rows);

        hr = standIn.GiveBack();
        AssertSucceeded (hr, L"GiveBack");

        Assert::IsTrue (missing.empty(), (L"controller entry points that did not assert off the owner:" + missing).c_str());
    }


    TEST_METHOD (BusAndTickPathsAreNotChecked)
    {
        auto                store      = std::make_unique<DiskImageStore>();
        auto                controller = std::make_unique<Disk2Controller> (s_kOwnedSlot);
        FakeDiskFileIo      fileIo;
        OwnerThreadStandIn  standIn;
        HRESULT             hr         = S_OK;



        PrepareStore (*store, fileIo);
        controller->SetThreadOwnership (store->GetThreadOwnership());

        hr = standIn.Take (store->GetThreadOwnership());
        AssertSucceeded (hr, L"Take");

        (void) controller->Read (s_kDriveIoAddress);
        controller->Write (s_kDriveIoAddress, 0);
        controller->Tick (1);

        hr = standIn.GiveBack();
        AssertSucceeded (hr, L"GiveBack");
    }


    TEST_METHOD (ABuiltControllerUsesItsStoresToken)
    {
        TestMachine         machine ("Apple2e");
        OwnerThreadStandIn  standIn;
        HRESULT             hr      = S_OK;



        hr = standIn.Take (machine.GetDiskStore().GetThreadOwnership());
        AssertSucceeded (hr, L"Take");

        {
            ExpectedEhmAssert  expect;

            (void) machine.GetRefs().diskController->GetDisk (s_kOwnedDrive);
            expect.RequireCount (1);
        }

        hr = standIn.GiveBack();
        AssertSucceeded (hr, L"GiveBack");
    }


    TEST_METHOD (ARebuildOnTheOwnerWiresTheNewController)
    {
        TestMachine         machine ("Apple2e");
        OwnerThreadStandIn  standIn;
        HRESULT             hr      = S_OK;
        HRESULT             hrBuild = E_FAIL;



        hr = standIn.Take (machine.GetDiskStore().GetThreadOwnership());
        AssertSucceeded (hr, L"Take");

        hr = standIn.RunOnOwner ([&] { hrBuild = RebuildDevices (machine); });
        AssertSucceeded (hr, L"RunOnOwner");
        AssertSucceeded (hrBuild, L"Build on the owner");

        hr = standIn.GiveBack();
        AssertSucceeded (hr, L"GiveBack");

        Assert::IsNotNull (machine.GetRefs().diskController, L"the rebuilt machine has a controller");

        // Built on the stand-in, the new controller would check a token that
        // thread made; wired to the store, it accepts the thread that holds it.
        (void) machine.GetRefs().diskController->GetDisk (s_kOwnedDrive);
    }


    TEST_METHOD (DiskOwnershipFollowsTheCpuThreadFromStartToStop)
    {
        auto                     store     = std::make_unique<DiskImageStore>();
        FakeDiskFileIo           fileIo;
        CpuManager               cpu;
        ThreadOwnership        & ownership = store->GetThreadOwnership();
        std::promise<bool>       seen;
        std::future<bool>        reply     = seen.get_future();
        std::future_status       status    = std::future_status::timeout;
        bool                     isHeld    = false;
        HRESULT                  hr        = S_OK;



        PrepareStore (*store, fileIo);

        ownership.Release();

        hr = cpu.Start ([&] { ownership.Claim(); },
                        [&] (const EmulatorCommand & cmd)
                        {
                            bool  isHeldHere = ownership.IsHeldByCurrentThread();

                            if (cmd.id == IDM_DISK_EJECT1)
                            {
                                if (isHeldHere)
                                {
                                    store->Eject (s_kOwnedSlot, s_kOwnedDrive);
                                }

                                seen.set_value (isHeldHere);
                            }
                        },
                        [] {},
                        [&] { ownership.Release(); });
        AssertSucceeded (hr, L"Start");

        cpu.SetPaused (true);
        cpu.PostCommand (IDM_DISK_EJECT1);

        status = reply.wait_for (s_kCommandWait);
        isHeld = status == std::future_status::ready && reply.get();

        cpu.Stop();
        ownership.Claim();

        Assert::IsTrue (status == std::future_status::ready, L"the CPU thread ran the command");
        Assert::IsTrue (isHeld, L"and held the store while it ran it");
        Assert::IsTrue (ownership.IsHeldByCurrentThread(), L"after the stop the test thread holds it again");
        Assert::IsFalse (store->IsMounted (s_kOwnedSlot, s_kOwnedDrive), L"the eject ran on the CPU thread");
    }


private:

    //  What a machine switch does to the devices, on the calling thread: every
    //  device is torn down and the machine is built again from its config.
    static HRESULT RebuildDevices (TestMachine & machine)
    {
        machine.GetInterruptController().ResetSources();
        machine.SetCpu (nullptr);
        machine.SetApple2cRomBank (nullptr);
        machine.GetOwnedDevices().clear();
        machine.GetVideoModes().clear();
        machine.GetMemoryBus() = MemoryBus();
        machine.GetRefs()      = {};
        machine.SetMmu (nullptr);

        return machine.GetBuilder().Build (machine.GetConfig());
    }


    //  A nibble image the loader accepts: every byte has its high bit set.
    static std::vector<Byte> MakeNib()
    {
        std::vector<Byte>  bytes (NibbleImageCodec::kNibImageSize, 0);
        size_t             i     = 0;



        for (i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> ((i * s_kNibPattern) | s_kNibHighBit);
        }

        return bytes;
    }


    //  A store whose every seam is faked, with a nibble image in the owned bay.
    static void PrepareStore (DiskImageStore & store, FakeDiskFileIo & fileIo)
    {
        std::vector<Byte>  bytes = MakeNib();
        HRESULT            hr    = S_OK;



        store.SetImageReader    ([] (const std::string &, std::vector<Byte> & out) { out = MakeNib(); return S_OK; });
        store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
        store.SetFlushSink      ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        store.SetFileIo         (&fileIo);
        store.SetAskSink        ([] (int, int, const ChangePrompt &) { return false; });
        store.SetRescueSink     ([] (const std::string &, std::string &) { return false; });

        hr = store.MountFromBytes (s_kOwnedSlot, s_kOwnedDrive, "owned.nib", DiskFormat::Nib, bytes);
        AssertSucceeded (hr, L"MountFromBytes");
    }


    //  One row per guarded store signature. The destructive ones come late, so
    //  every earlier row finds a disk in the owned bay, and the setters that
    //  clear a seam come last, after every row that calls one.
    static std::vector<EntryPoint> MakeStoreRows (DiskImageStore & store)
    {
        const DiskImageStore  & view = store;
        std::vector<Byte>       nib  = MakeNib();



        return
        {
            { L"GetImage",                  [&store] { (void) store.GetImage (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"IsMounted",                 [&store] { (void) store.IsMounted (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"GetSourcePath",             [&store] { (void) store.GetSourcePath (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"GetMountedSourcePaths",     [&store] { (void) store.GetMountedSourcePaths(); } },
            { L"GetSharedState",            [&store] { (void) store.GetSharedState (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"GetSharedState const",      [&view]  { (void) view.GetSharedState (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"GetMediaId",                [&store] { (void) store.GetMediaId (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"IsFlushHeld",               [&store] { (void) store.IsFlushHeld(); } },
            { L"SetFlushHold",              [&store] { store.SetFlushHold (false); } },
            { L"HasUnsavedWrites",          [&store] { (void) store.HasUnsavedWrites(); } },
            { L"CountUnsavedDisks",         [&store] { (void) store.CountUnsavedDisks(); } },
            { L"IsReplaying",               [&store] { (void) store.IsReplaying(); } },
            { L"SetReplaying",              [&store] { store.SetReplaying (false); } },
            { L"IsRetainingMedia",          [&store] { (void) store.IsRetainingMedia(); } },
            { L"SetMediaRetention",         [&store] { store.SetMediaRetention (false); } },
            { L"GetRetainedMediaCount",     [&store] { (void) store.GetRetainedMediaCount(); } },
            { L"CanSeatMedia",              [&store] { (void) store.CanSeatMedia (s_kOwnedSlot, s_kOwnedDrive, 0); } },
            { L"ReportSeatedMedia",         [&store] { store.ReportSeatedMedia (s_kOwnedSlot, s_kOwnedDrive, 0); } },
            { L"PruneRetainedMedia",        [&store] { store.PruneRetainedMedia (0); } },
            { L"IsSalvageOffered",          [&store] { (void) store.IsSalvageOffered (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"AssessSalvage",             [&store] { SalvageAssessment a; (void) store.AssessSalvage (s_kOwnedSlot, s_kOwnedDrive, a); } },
            { L"SetImageWriteProtect",      [&store] { (void) store.SetImageWriteProtect (s_kOwnedSlot, s_kOwnedDrive, false); } },
            { L"SetFlushSink",              [&store] { store.SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; }); } },
            { L"SetImageReader",            [&store] { store.SetImageReader ([] (const std::string &, std::vector<Byte> & out) { out = MakeNib(); return S_OK; }); } },
            { L"SetIdentityReader",         [&store] { store.SetIdentityReader ([] (const std::string &) { return ImageIdentity(); }); } },
            { L"SetAskSink",                [&store] { store.SetAskSink ([] (int, int, const ChangePrompt &) { return false; }); } },
            { L"SetRescueSink",             [&store] { store.SetRescueSink ([] (const std::string &, std::string &) { return false; }); } },
            { L"ApplyPendingReload",        [&store] { store.ApplyPendingReload(); } },
            { L"ResolvePendingChange",      [&store] { store.ResolvePendingChange (s_kOwnedSlot, s_kOwnedDrive, ChangeAction::Ignore); } },
            { L"Flush",                     [&store] { (void) store.Flush (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"FlushAll",                  [&store] { (void) store.FlushAll(); } },
            { L"FlushAllUnlessHeld",        [&store] { (void) store.FlushAllUnlessHeld(); } },
            { L"FlushAllForShutdown",       [&store] { (void) store.FlushAllForShutdown(); } },
            { L"CommitHeldWrites",          [&store] { (void) store.CommitHeldWrites(); } },
            { L"DiscardHeldWrites",         [&store] { (void) store.DiscardHeldWrites(); } },
            { L"SoftReset",                 [&store] { store.SoftReset(); } },
            { L"SalvageToFile",             [&store] { DenibblizeReport r; (void) store.SalvageToFile (s_kOwnedSlot, s_kOwnedDrive, "salvaged.woz", r); } },
            { L"SeatMedia",                 [&store] { bool changed = false; (void) store.SeatMedia (s_kOwnedSlot, s_kOwnedDrive, 0, changed); } },
            { L"SetFileIo",                 [&store] { store.SetFileIo (nullptr); } },
            { L"Mount",                     [&store] { (void) store.Mount (s_kOwnedSlot, s_kOwnedDrive, "second.nib"); } },
            { L"Mount with diagnosis",      [&store] { MountDiagnosis d; (void) store.Mount (s_kOwnedSlot, s_kOwnedDrive, "third.nib", d); } },
            { L"MountFromBytes",            [&store, nib] { (void) store.MountFromBytes (s_kOwnedSlot, s_kOwnedDrive, "fourth.nib", DiskFormat::Nib, nib); } },
            { L"MountFromBytes diagnosis",  [&store, nib] { MountDiagnosis d; (void) store.MountFromBytes (s_kOwnedSlot, s_kOwnedDrive, "fifth.nib", DiskFormat::Nib, nib, d); } },
            { L"MountRestored",             [&store, nib] { (void) store.MountRestored (s_kOwnedSlot, s_kOwnedDrive, "sixth.nib", DiskFormat::Nib, nib); } },
            { L"Eject",                     [&store] { store.Eject (s_kOwnedSlot, s_kOwnedDrive); } },
            { L"PowerCycle",                [&store] { store.PowerCycle(); } },
            { L"SetImageWatcher",           [&store] { store.SetImageWatcher (nullptr); } },
            { L"SetMachineName",            [&store] { store.SetMachineName (""); } },
            { L"SetMachineRestartCallback", [&store] { store.SetMachineRestartCallback (nullptr); } },
            { L"SetChangeReportSink",       [&store] { store.SetChangeReportSink (nullptr); } },
            { L"SetBayChangeSink",          [&store] { store.SetBayChangeSink (nullptr); } },
            { L"SetDecisionSink",           [&store] { store.SetDecisionSink (nullptr); } },
            { L"SetClock",                  [&store] { store.SetClock (nullptr); } },
            { L"SetTimestampSource",        [&store] { store.SetTimestampSource (nullptr); } },
            { L"SetMediaChangeListener",    [&store] { store.SetMediaChangeListener (nullptr); } },
            { L"SetPositionSource",         [&store] { store.SetPositionSource (nullptr); } },
        };
    }


    //  One row per guarded controller signature. SetThreadOwnership passes the
    //  store's own token, so its repoint changes nothing for the rows after it.
    static std::vector<EntryPoint> MakeControllerRows (Disk2Controller & controller, const ThreadOwnership & storeOwnership)
    {
        StateWriter        writer;
        std::vector<Byte>  state;
        HRESULT            hr     = S_OK;



        hr = controller.SaveState (writer);
        AssertSucceeded (hr, L"SaveState before the sweep");

        state = writer.GetBytes();

        return
        {
            { L"GetDisk",                   [&controller] { (void) controller.GetDisk (s_kOwnedDrive); } },
            { L"HasExternalDisk",           [&controller] { (void) controller.HasExternalDisk (s_kOwnedDrive); } },
            { L"SetExternalDisk",           [&controller] { controller.SetExternalDisk (s_kOwnedDrive, nullptr); } },
            { L"NotifyDiskInserted",        [&controller] { controller.NotifyDiskInserted (s_kOwnedDrive); } },
            { L"NotifyDiskEjected",         [&controller] { controller.NotifyDiskEjected (s_kOwnedDrive); } },
            { L"MountDisk",                 [&controller] { (void) controller.MountDisk (s_kOwnedDrive, "missing.nib"); } },
            { L"EjectDisk",                 [&controller] { controller.EjectDisk (s_kOwnedDrive); } },
            { L"SetAudioSink",              [&controller] { controller.SetAudioSink (nullptr); } },
            { L"GetAudioSink",              [&controller] { (void) controller.GetAudioSink(); } },
            { L"SetEventSink",              [&controller] { controller.SetEventSink (nullptr); } },
            { L"SetIwmMode",                [&controller] { controller.SetIwmMode (false); } },
            { L"SetMotorOffFlushCallback",  [&controller] { controller.SetMotorOffFlushCallback (nullptr); } },
            { L"SetIdleCallback",           [&controller] { controller.SetIdleCallback (nullptr); } },
            { L"SetCpuCycleSource",         [&controller] { controller.SetCpuCycleSource (nullptr); } },
            { L"GetActiveDrive",            [&controller] { (void) controller.GetActiveDrive(); } },
            { L"IsMotorOn",                 [&controller] { (void) controller.IsMotorOn(); } },
            { L"IsMotorAtSpeed",            [&controller] { (void) controller.IsMotorAtSpeed(); } },
            { L"GetMotorSpinupRemaining",   [&controller] { (void) controller.GetMotorSpinupRemaining(); } },
            { L"GetQuarterTrack",           [&controller] { (void) controller.GetQuarterTrack(); } },
            { L"GetCurrentTrack",           [&controller] { (void) controller.GetCurrentTrack(); } },
            { L"IsQ6",                      [&controller] { (void) controller.IsQ6(); } },
            { L"IsQ7",                      [&controller] { (void) controller.IsQ7(); } },
            { L"GetPhases",                 [&controller] { (void) controller.GetPhases(); } },
            { L"GetEngine",                 [&controller] { (void) controller.GetEngine (s_kOwnedDrive); } },
            { L"GetDiagnostics",            [&controller] { DiagnosticsSnapshot s; controller.GetDiagnostics (s); } },
            { L"SaveState",                 [&controller] { StateWriter w; (void) controller.SaveState (w); } },
            { L"SetThreadOwnership",        [&controller, &storeOwnership] { controller.SetThreadOwnership (storeOwnership); } },
            { L"Reset",                     [&controller] { controller.Reset(); } },
            { L"SoftReset",                 [&controller] { controller.SoftReset(); } },
            { L"LoadState",                 [&controller, state] { StateReader r (state); (void) controller.LoadState (r); } },
            { L"PowerCycle",                [&controller] { Prng p (s_kPowerCycleSeed); controller.PowerCycle (p); } },
        };
    }


    //  Calls each row inside one expected-assertion scope and gives the names
    //  of the rows whose call did not move the count.
    static std::wstring SweepOffTheOwner (const std::vector<EntryPoint> & rows)
    {
        std::wstring  missing;
        int           before  = 0;



        {
            ExpectedEhmAssert  expect;

            for (const EntryPoint & row : rows)
            {
                before = expect.Count();

                row.call();

                if (s_kAreAssertsCounted && expect.Count() == before)
                {
                    missing += L" ";
                    missing += row.name;
                }
            }
        }

        return missing;
    }
};
