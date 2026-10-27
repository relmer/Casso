#include "Pch.h"

#include "../EhmTestHelper.h"
#include "HResultAssert.h"
#include "Core/TextEncoding.h"
#include "FakeDiskFileIo.h"
#include "FakeImageWatcher.h"
#include "OwnerThreadStandIn.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DriveStatus.h"
#include "Devices/Disk/MountedImageState.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Shell/DriveStatusPublisher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr int     s_kStressSlot         = 6;
static constexpr int     s_kChangingDrive      = 0;
static constexpr int     s_kFixedDrive         = 1;
static constexpr int     s_kCycleCount         = 1000;
static constexpr int     s_kOperationsPerCycle = 10;
static constexpr int     s_kCyclesPerHandshake = 50;
static constexpr int     s_kMinimumReads       = 1000;
static constexpr int     s_kWozTrackCount      = 2;
static constexpr Byte    s_kNibbleHighBit      = 0x80;

static constexpr std::chrono::seconds  s_kHandshakeLimit { 2 };

//  Longer than the small-string buffer, so a torn copy of the path is a
//  heap copy the sanitizer build can catch.
static const std::string  s_kPathA      = "C:\\work\\drive-status-stress-test-images\\first-image-of-the-run.nib";
static const std::string  s_kPathAPrime = "C:\\work\\drive-status-stress-test-images\\second-image-of-the-run.nib";
static const std::string  s_kPathB      = "C:\\work\\drive-status-stress-test-images\\damaged-image-of-the-run.woz";
static const std::string  s_kPathC      = "C:\\work\\drive-status-stress-test-images\\image-in-the-other-drive.nib";





////////////////////////////////////////////////////////////////////////////////
//
//  DriveStatusStressTests
//
//  SC-002: a thread with the disk store's token mounts, ejects, swaps and
//  reloads disks while the test thread reads the published drive status. Every
//  status read is one the owner published, never a mixture of two, and the
//  published status equals the live bay after every operation.
//
//  A third thread notes external changes to one image throughout, as the
//  folder watcher does from another thread.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveStatusStressTests)
{
public:

    //  The states the changing drive may have. Drive (6,1) always has C.
    enum class BayState
    {
        Empty,
        A,
        APrime,
        B,
        Count,
    };

    using StateCounts = std::array<int64_t, static_cast<size_t> (BayState::Count)>;


    //  A heap store whose reads, identities and watches all stay in this
    //  object. Only the owner thread touches the maps; the clock is atomic
    //  because NoteExternalChange reads it on the watcher thread.
    struct Rig
    {
        std::unique_ptr<DiskImageStore>                 store = std::make_unique<DiskImageStore>();
        FakeDiskFileIo                                  fileIo;
        FakeImageWatcher                                watcher;
        DriveStatusPublisher                            publisher;
        std::map<std::string, std::vector<Byte>>        files;
        std::map<std::string, int64_t>                  modified;
        std::atomic<int64_t>                            nowMs { 0 };

        Rig()
        {
            store->SetFileIo          (&fileIo);
            store->SetImageWatcher    (&watcher);
            store->SetImageReader     ([this] (const std::string & path, std::vector<Byte> & bytes) { bytes = files[path]; return S_OK; });
            store->SetIdentityReader  ([this] (const std::string & path) { return Identity (path); });
            store->SetClock           ([this] () { return nowMs.load(); });
            store->SetChangeReportSink ([] (int, int, const ChangePrompt &) {});
            store->SetAskSink         ([] (int, int, const ChangePrompt &) { return true; });
        }

        void  Write (const std::string & path, const std::vector<Byte> & bytes)
        {
            files[path]        = bytes;
            fileIo.files[path] = bytes;
            modified[path]++;
        }

        ImageIdentity  Identity (const std::string & path)
        {
            ImageIdentity  identity;

            identity.recorded           = files.count (path) != 0;
            identity.stamp.sizeBytes    = files[path].size();
            identity.stamp.modifiedUnix = modified[path];
            return identity;
        }
    };


    TEST_METHOD (TenThousandDiskChangesPublishOnlyWholeStatuses)
    {
        Rig                                    rig;
        OwnerThreadStandIn                     standIn;
        std::atomic<bool>                      isDriverDone  = false;
        std::atomic<bool>                      isWatcherDone = false;
        std::atomic<int64_t>                   reads         = 0;
        std::atomic<int64_t>                   notes         = 0;
        std::atomic<int>                       operations    = 0;
        std::exception_ptr                     driverFailure;
        std::wstring                           ownerFailure;
        std::wstring                           readFailure;
        StateCounts                            seen          {};
        std::thread                            driver;
        std::thread                            watcher;
        std::shared_ptr<const DriveStatus>     final;
        std::chrono::steady_clock::time_point  start         = std::chrono::steady_clock::now();
        HRESULT                                hr            = S_OK;



        rig.Write (s_kPathA,      MakeNib (0x11));
        rig.Write (s_kPathAPrime, MakeNib (0x22));
        rig.Write (s_kPathB,      MakeDamagedWoz());
        rig.Write (s_kPathC,      MakeNib (0x33));

        hr = rig.store->Mount (s_kStressSlot, s_kFixedDrive, s_kPathC);
        AssertSucceeded (hr, L"Mount C");

        hr = standIn.Take (rig.store->GetThreadOwnership());
        AssertSucceeded (hr, L"Take");

        hr = standIn.RunOnOwner ([&] { (void) rig.publisher.Publish (*rig.store, nullptr, false); });
        AssertSucceeded (hr, L"the first publish");

        watcher = std::thread ([&]
        {
            while (!isWatcherDone.load())
            {
                rig.store->NoteExternalChange (s_kPathA, ExternalChangeIntent::Unstated);
                notes++;
                std::this_thread::yield();
            }
        });

        driver = std::thread ([&]
        {
            try
            {
                RunCycles (rig, standIn, reads, operations, ownerFailure);
            }
            catch (...)
            {
                driverFailure = std::current_exception();
            }

            isDriverDone = true;
        });

        while (!isDriverDone.load())
        {
            ReadOnce (rig.publisher, seen, readFailure);
            reads++;
            std::this_thread::yield();
        }

        driver.join();
        isWatcherDone = true;
        watcher.join();

        Logger::WriteMessage (std::format (L"{} operations, {} reads, {} notes in {} ms",
                                           operations.load(), reads.load(), notes.load(),
                                           std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now() - start).count()).c_str());

        if (driverFailure)
        {
            std::rethrow_exception (driverFailure);
        }

        Assert::IsTrue  (ownerFailure.empty(), ownerFailure.c_str());
        Assert::IsTrue  (readFailure.empty(), readFailure.c_str());
        Assert::AreEqual (s_kCycleCount * s_kOperationsPerCycle, operations.load(), L"every operation ran");
        Assert::IsTrue  (reads.load() >= s_kMinimumReads, L"the status was read throughout");
        Assert::IsTrue  (seen[static_cast<size_t> (BayState::Empty)]  > 0, L"an empty drive was read");
        Assert::IsTrue  (seen[static_cast<size_t> (BayState::A)]      > 0, L"A was read");
        Assert::IsTrue  (seen[static_cast<size_t> (BayState::APrime)] > 0, L"A' was read");
        Assert::IsTrue  (seen[static_cast<size_t> (BayState::B)]      > 0, L"the damaged B was read");
        Assert::IsTrue  (notes.load() > 0, L"the watcher thread noted changes");

        hr = standIn.RunOnOwner ([&]
        {
            (void) rig.publisher.Publish (*rig.store, nullptr, false);
            final = rig.publisher.Take();

            rig.store->Eject (s_kStressSlot, s_kFixedDrive);
        });
        AssertSucceeded (hr, L"the final publish and eject");

        Assert::IsFalse  (final->GetBay (s_kStressSlot, s_kChangingDrive).isMounted, L"the changing drive ends empty");
        Assert::AreEqual (s_kPathC, final->GetBay (s_kStressSlot, s_kFixedDrive).path, L"and the other drive ends with C");
        Assert::IsTrue   (rig.watcher.watched.empty(), L"no watch is left once every disk is out");

        hr = standIn.GiveBack();
        AssertSucceeded (hr, L"GiveBack");
    }


private:

    //  The owner's side: s_kCycleCount cycles of s_kOperationsPerCycle
    //  operations, one cycle per handoff, with a bounded wait every
    //  s_kCyclesPerHandshake cycles for the reader to read again.
    //
    //  Nothing here asserts. A failed Assert raises an SEH exception, which
    //  catch (...) does not catch under /EHsc, so one raised off the test
    //  thread ends the test host. The first failure is written to
    //  outFailure instead, and the test thread asserts on it.
    static void RunCycles (Rig                   & rig,
                           OwnerThreadStandIn    & standIn,
                           std::atomic<int64_t>  & reads,
                           std::atomic<int>      & operations,
                           std::wstring          & outFailure)
    {
        int      cycle = 0;
        int64_t  mark  = 0;
        HRESULT  hr    = S_OK;



        for (cycle = 0; cycle < s_kCycleCount && outFailure.empty(); cycle++)
        {
            hr = standIn.RunOnOwner ([&] { RunCycle (rig, operations, outFailure); });

            if (FAILED (hr) && outFailure.empty())
            {
                outFailure = std::format (L"cycle {} did not run on the owner: 0x{:08X}", cycle, static_cast<uint32_t> (hr));
            }

            if (outFailure.empty() && (cycle + 1) % s_kCyclesPerHandshake == 0)
            {
                auto  deadline = std::chrono::steady_clock::now() + s_kHandshakeLimit;

                mark = reads.load();

                while (reads.load() == mark && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::yield();
                }

                if (reads.load() == mark)
                {
                    outFailure = std::format (L"the reader read nothing for 2 s after cycle {}", cycle);
                }
            }
        }
    }


    static void RunCycle (Rig & rig, std::atomic<int> & operations, std::wstring & outFailure)
    {
        Operate (rig, operations, outFailure, L"mount A",    [&] { return rig.store->Mount (s_kStressSlot, s_kChangingDrive, s_kPathA); });
        Operate (rig, operations, outFailure, L"reload A",   [&] { return Reload (rig, s_kPathA); });
        Operate (rig, operations, outFailure, L"swap to A'", [&] { return rig.store->Mount (s_kStressSlot, s_kChangingDrive, s_kPathAPrime); });
        Operate (rig, operations, outFailure, L"eject",      [&] { rig.store->Eject (s_kStressSlot, s_kChangingDrive); return S_OK; });
        Operate (rig, operations, outFailure, L"mount A",    [&] { return rig.store->Mount (s_kStressSlot, s_kChangingDrive, s_kPathA); });
        Operate (rig, operations, outFailure, L"swap to B",  [&] { return rig.store->Mount (s_kStressSlot, s_kChangingDrive, s_kPathB); });
        Operate (rig, operations, outFailure, L"eject",      [&] { rig.store->Eject (s_kStressSlot, s_kChangingDrive); return S_OK; });
        Operate (rig, operations, outFailure, L"mount A'",   [&] { return rig.store->Mount (s_kStressSlot, s_kChangingDrive, s_kPathAPrime); });
        Operate (rig, operations, outFailure, L"reload A'",  [&] { return Reload (rig, s_kPathAPrime); });
        Operate (rig, operations, outFailure, L"eject",      [&] { rig.store->Eject (s_kStressSlot, s_kChangingDrive); return S_OK; });
    }


    //  One operation, then a publish: the published bays equal the live bays.
    //  Skipped once outFailure is not empty.
    static void Operate (Rig                              & rig,
                         std::atomic<int>                 & operations,
                         std::wstring                     & outFailure,
                         const wchar_t                    * name,
                         const std::function<HRESULT()>   & operation)
    {
        std::shared_ptr<const DriveStatus>  status;
        int                                 drive = 0;
        HRESULT                             hr    = S_OK;



        if (outFailure.empty())
        {
            hr = operation();
            Check (SUCCEEDED (hr), name, L"the operation succeeds", outFailure);

            (void) rig.publisher.Publish (*rig.store, nullptr, false);
            status = rig.publisher.Take();

            for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
            {
                const BayStatus &  bay       = status->GetBay (s_kStressSlot, drive);
                bool               isMounted = rig.store->IsMounted (s_kStressSlot, drive);

                Check (bay == rig.store->GetBayStatus (s_kStressSlot, drive), name, L"the published bay is the store's", outFailure);
                Check (bay.isMounted == isMounted,                            name, L"mounted as the live bay",          outFailure);

                if (isMounted)
                {
                    Check (bay.path == rig.store->GetSourcePath (s_kStressSlot, drive),                         name, L"the live bay's path",                 outFailure);
                    Check (bay.writeProtect == rig.store->GetImage (s_kStressSlot, drive)->GetWriteProtectInfo(), name, L"the live bay's write-protect causes", outFailure);
                }
            }

            operations++;
        }
    }


    //  Sets outFailure to the first failed condition.
    static void Check (bool isMet, const wchar_t * operation, const wchar_t * condition, std::wstring & outFailure)
    {
        if (!isMet && outFailure.empty())
        {
            outFailure = std::format (L"{}: {}", operation, condition);
        }
    }


    //  The file changes on disk, the change is noted and settles, and the
    //  owner applies it.
    static HRESULT Reload (Rig & rig, const std::string & path)
    {
        rig.modified[path]++;
        rig.store->NoteExternalChange (path, ExternalChangeIntent::Unstated);
        rig.nowMs += MountedImageState::kQuietPeriodMs;
        rig.store->ApplyPendingReload();

        return S_OK;
    }


    //  The reader's side: one status, classified. A status that is none of
    //  the published states is a failure, and outFailure is set to the first one.
    static void ReadOnce (const DriveStatusPublisher  & publisher,
                          StateCounts                 & seen,
                          std::wstring                & outFailure)
    {
        std::shared_ptr<const DriveStatus>  status   = publisher.Take();
        const BayStatus                   & changing = status->GetBay (s_kStressSlot, s_kChangingDrive);
        const BayStatus                   & fixed    = status->GetBay (s_kStressSlot, s_kFixedDrive);
        BayState                            state    = BayState::Count;
        bool                                isFixedC = fixed.isMounted && fixed.path == s_kPathC && !fixed.writeProtect.IsDamaged();



        if (!changing.isMounted)
        {
            if (changing.path.empty() && changing.writeProtect == WriteProtectInfo())
            {
                state = BayState::Empty;
            }
        }
        else if (changing.path == s_kPathA && !changing.writeProtect.IsDamaged())
        {
            state = BayState::A;
        }
        else if (changing.path == s_kPathAPrime && !changing.writeProtect.IsDamaged())
        {
            state = BayState::APrime;
        }
        else if (changing.path == s_kPathB && changing.writeProtect.IsDamaged())
        {
            state = BayState::B;
        }

        if (state != BayState::Count && isFixedC)
        {
            seen[static_cast<size_t> (state)]++;
        }
        else if (outFailure.empty())
        {
            outFailure = std::format (L"a status no owner published: mounted {}, path '{}', damaged {}; other drive path '{}'",
                                      changing.isMounted, TextEncoding::NarrowToWide (changing.path),
                                      changing.writeProtect.IsDamaged(), TextEncoding::NarrowToWide (fixed.path));
        }
    }


    static std::vector<Byte> MakeNib (Byte fill)
    {
        std::vector<Byte>  bytes (NibbleImageCodec::kNibImageSize, static_cast<Byte> (fill | s_kNibbleHighBit));



        return bytes;
    }


    //  A WOZ image whose first track record lies past the end of the file, so
    //  it mounts read-only and damaged.
    static std::vector<Byte> MakeDamagedWoz()
    {
        DiskImage                       source;
        std::vector<Byte>               sectors (NibblizationLayer::kImageByteSize, 0);
        std::vector<WozSyntheticTrack>  tracks;
        std::vector<Byte>               woz;
        size_t                          at      = 0;
        int                             t       = 0;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, source));

        for (t = 0; t < s_kWozTrackCount; t++)
        {
            WozSyntheticTrack  track;

            track.isFlux        = false;
            track.quarterTracks = { t * 4 };
            track.bitCount      = source.GetTrackBitCount (t);
            track.data          = source.GetTrackBits (t);
            tracks.push_back (track);
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));

        at = WozLoader::kHeaderSize;

        while (at + 4 <= woz.size() && memcmp (woz.data() + at, "TRKS", 4) != 0)
        {
            at++;
        }

        Assert::IsTrue (at + 4 <= woz.size(), L"the image has a TRKS chunk");

        woz[at + 8]     = 0xFF;
        woz[at + 8 + 1] = 0x7F;

        return woz;
    }
};
