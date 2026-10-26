#include "Pch.h"
#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Shell/InspectorRequestQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleInspectorEditTests
//
//  A sector edit from the disk inspector, as read by a running 6502 (SC-018).
//  The edit goes through the request queue the inspector window uses, with
//  the test machine's disk store behind it, and the test checks the bytes the
//  guest's BLOAD of the changed file puts in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleInspectorEditTests)
{
public:

    static constexpr int       kSlot            = 6;
    static constexpr int       kDrive           = 0;
    static constexpr size_t    kSectorBytes     = 256;
    static constexpr size_t    kPayloadBytes    = 256;
    static constexpr Word      kLoadAddress     = 0x6000;
    static constexpr int       kSectorsPerTrack = 16;
    static constexpr int       kMaxPasses       = 400;
    static constexpr uint64_t  kPassCycles      = 100'000ULL;

    static constexpr const char *  kImagePath  = "edit.dsk";
    static constexpr const char *  kHostFile   = "C:\\build\\prog.bin";
    static constexpr const char *  kPlacedName = "PROG";



    //  The test machine's slot 6 drive, accessed as Casso's inspector host
    //  accesses it, with each save captured rather than written to a file.
    class StoreSource : public IInspectorDiskSource
    {
    public:
        explicit StoreSource (MachineHost & machine) : m_machine (machine) {}

        const DiskImage * GetImage (int drive, InspectorDiskIdentity & outIdentity) override
        {
            outIdentity.mediaId  = m_machine.GetDiskStore().GetMediaId (kSlot, drive);
            outIdentity.fileName = kImagePath;

            return m_machine.GetDiskStore().GetImage (kSlot, drive);
        }

        DiskImage * GetImageForEdit (int drive) override
        {
            return m_machine.GetDiskStore().GetImage (kSlot, drive);
        }

        InspectorDriveMotion GetMotion (int drive) override
        {
            Disk2Controller *     controller = m_machine.GetRefs().diskController;
            InspectorDriveMotion  motion;

            if (controller != nullptr)
            {
                motion.isTurning    = controller->IsMotorOn() && controller->GetActiveDrive() == drive;
                motion.isWriting    = motion.isTurning && controller->IsQ7();
                motion.slot         = controller->GetEngine (drive).GetSlot();
                motion.angle        = controller->GetEngine (drive).GetAngle();
                motion.writeNibbles = controller->GetEngine (drive).GetWriteNibbles();
            }

            return motion;
        }

        InspectorSaveResult SaveDrive (int drive) override
        {
            InspectorSaveResult  result;

            m_machine.GetDiskStore().SetFlushSink ([this] (const std::string &, const std::vector<Byte> & bytes) { saved = bytes; return S_OK; });
            result.hr   = m_machine.GetDiskStore().Flush (kSlot, drive);
            result.path = kImagePath;

            return result;
        }

        std::vector<Byte>  saved;

    private:
        MachineHost & m_machine;
    };



    static std::vector<Byte> MakePayload (Byte seed)
    {
        std::vector<Byte>  bytes (kPayloadBytes, 0);



        for (size_t i = 0; i < kPayloadBytes; i++)
        {
            bytes[i] = static_cast<Byte> ((i * 7 + seed) & 0xFF);
        }

        return bytes;
    }



    //  The master with one build of the program put on it, as a build step
    //  would.
    static std::vector<Byte> PlaceBuild (const std::vector<Byte> & master, Byte seed)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  options;
        DiskCommandResult   result;



        io.files[kImagePath]  = master;
        io.stamps[kImagePath] = FileStamp { master.size(), 100 };
        io.files[kHostFile]   = MakePayload (seed);
        io.stamps[kHostFile]  = FileStamp { kPayloadBytes, 100 };

        options.subcommand          = CommandLineOptions::Subcommand::Disk;
        options.disk.command        = CommandLineOptions::DiskOptions::Command::Put;
        options.disk.imagePath      = kImagePath;
        options.disk.hostFile       = kHostFile;
        options.disk.path           = kPlacedName;
        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        {
            DiskCommandRunner  runner (io);

            result = runner.Run (options);
        }

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        return io.files[kImagePath];
    }



    static void AssertGuestLoads (MachineHost & machine, Byte seed)
    {
        std::vector<std::string>  rows   = GuestSession::TypeAndCollect (machine, "BLOAD PROG");
        std::vector<Byte>         loaded = GuestSession::GuestBytesAt (machine, kLoadAddress, kPayloadBytes);



        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"the guest loads the file");
        Assert::IsTrue  (loaded == MakePayload (seed), L"with this build's bytes, all of them");
    }



    //  An edit for every sector in which `target` differs from the disk in
    //  the drive, found on the drive's tracks by its DOS 3.3 sector number,
    //  as the inspector's editor finds the field it draws.
    static std::vector<SectorWrite> MakeWrites (const DiskImage & drive, const std::vector<Byte> & current, const std::vector<Byte> & target)
    {
        std::vector<SectorWrite>  writes;
        DiskAnalysis              analysis;
        size_t                    offset   = 0;
        int                       track    = 0;
        int                       logical  = 0;



        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (drive, 1, kImagePath, 0, false), DecodeSettings::MakeStandard(), analysis);

        for (offset = 0; offset + kSectorBytes <= target.size(); offset += kSectorBytes)
        {
            if (std::equal (target.begin() + offset, target.begin() + offset + kSectorBytes, current.begin() + offset))
            {
                continue;
            }

            track   = static_cast<int> (offset / kSectorBytes) / kSectorsPerTrack;
            logical = static_cast<int> (offset / kSectorBytes) % kSectorsPerTrack;

            const TrackAnalysis &  analyzed = *analysis.tracks[analysis.entries[track * DiskImage::kQuarterTracksPerWholeTrack].slot];

            for (const AnalyzedSector & sector : analyzed.sectors)
            {
                if (sector.dos33Logical == logical && sector.dataField >= 0)
                {
                    SectorWrite  write;

                    write.slot          = analyzed.slot;
                    write.dataFieldCell = static_cast<int> (analyzed.fields[sector.dataField].startCell);
                    write.sector        = sector.sector;
                    std::copy (target.begin() + offset, target.begin() + offset + kSectorBytes, write.bytes.begin());
                    writes.push_back (write);
                }
            }
        }

        return writes;
    }



    TEST_METHOD (AGuestLoadsTheBytesAnAppliedEditPutInAFile)
    {
        TestMachine             machine ("Apple2e");
        std::vector<Byte>       master  = GuestSession::RequireDos33Master();
        std::vector<Byte>       first   = PlaceBuild (master, 0x21);
        std::vector<Byte>       second  = PlaceBuild (master, 0x5C);
        StoreSource             source (machine);
        InspectorRequestQueue   queue;
        InspectorRequest        request;
        vector<InspectorReply>  replies;
        ApplyOutcome            outcome = ApplyOutcome::Waiting;
        int                     pass    = 0;



        GuestSession::BootToPrompt (machine, first);
        AssertGuestLoads (machine, 0x21);

        request.kind      = InspectorRequestKind::Apply;
        request.drive     = kDrive;
        request.mediaId   = machine.GetDiskStore().GetMediaId (kSlot, kDrive);
        request.applyKind = ApplyKind::Apply;
        request.applyMode = ApplyMode::WaitForStop;
        request.writes    = MakeWrites (*machine.GetDiskStore().GetImage (kSlot, kDrive), first, second);
        Assert::IsFalse (request.writes.empty(), L"the two builds differ in some sectors");

        queue.Post (request);

        //  The guest runs on while the apply waits for the drive to stop.
        for (pass = 0; pass < kMaxPasses && outcome == ApplyOutcome::Waiting; pass++)
        {
            (void) machine.RunCycles (kPassCycles);
            queue.Drain (source);
            queue.TakeReplies (replies);
            outcome = replies.empty() ? outcome : replies.back().applyOutcome;
        }

        Assert::IsTrue (outcome == ApplyOutcome::Applied, replies.empty() ? L"no reply" : replies.back().reason.c_str());
        Assert::IsTrue (source.saved == second, L"the saved file is the second build, byte for byte");

        //  SC-018: the guest reads what the edit wrote.
        AssertGuestLoads (machine, 0x5C);
    }


    //  SC-018: a guest that writes the file in a loop ends the wait, since its
    //  writes go to the very tracks the apply changes; nothing is applied.
    TEST_METHOD (AGuestWritingInALoopEndsAnApplyWait)
    {
        TestMachine             machine ("Apple2e");
        std::vector<Byte>       master  = GuestSession::RequireDos33Master();
        std::vector<Byte>       first   = PlaceBuild (master, 0x21);
        std::vector<Byte>       second  = PlaceBuild (master, 0x5C);
        StoreSource             source (machine);
        InspectorRequestQueue   queue;
        InspectorRequest        request;
        vector<InspectorReply>  replies;
        ApplyOutcome            outcome = ApplyOutcome::Waiting;
        int                     pass    = 0;



        GuestSession::BootToPrompt (machine, first);
        AssertGuestLoads (machine, 0x21);

        request.kind      = InspectorRequestKind::Apply;
        request.drive     = kDrive;
        request.mediaId   = machine.GetDiskStore().GetMediaId (kSlot, kDrive);
        request.applyKind = ApplyKind::Apply;
        request.applyMode = ApplyMode::WaitForStop;
        request.writes    = MakeWrites (*machine.GetDiskStore().GetImage (kSlot, kDrive), first, second);

        for (const SectorWrite & write : request.writes)
        {
            request.expectedCounts.push_back ({ write.slot, machine.GetDiskStore().GetImage (kSlot, kDrive)->GetGuestWriteCount (write.slot) });
        }

        KeystrokeInjector::InjectLine (machine, "10 PRINT CHR$(4)\"BSAVE PROG,A$6000,L$100\": GOTO 10", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "RUN", GuestSession::kLineCycles);

        queue.Post (request);

        for (pass = 0; pass < kMaxPasses && outcome == ApplyOutcome::Waiting; pass++)
        {
            (void) machine.RunCycles (kPassCycles);
            queue.Drain (source);
            queue.TakeReplies (replies);
            outcome = replies.empty() ? outcome : replies.back().applyOutcome;
        }

        Assert::IsTrue (outcome == ApplyOutcome::WaitEnded, replies.empty() ? L"no reply" : replies.back().reason.c_str());
        Assert::IsTrue (source.saved.empty(), L"nothing was saved");
    }
};