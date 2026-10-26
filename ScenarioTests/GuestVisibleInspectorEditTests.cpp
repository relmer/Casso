#include "Pch.h"
#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
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

            savedMotion = GetMotion (drive);
            m_machine.GetDiskStore().SetFlushSink ([this] (const std::string &, const std::vector<Byte> & bytes) { saved = bytes; return S_OK; });
            result.hr   = m_machine.GetDiskStore().Flush (kSlot, drive);
            result.path = kImagePath;

            return result;
        }

        std::vector<Byte>     saved;
        InspectorDriveMotion  savedMotion;

    private:
        MachineHost & m_machine;
    };



    static std::vector<Byte> MakePayload (Byte seed, size_t length = kPayloadBytes)
    {
        std::vector<Byte>  bytes (length, 0);



        for (size_t i = 0; i < length; i++)
        {
            bytes[i] = static_cast<Byte> ((i * 7 + seed) & 0xFF);
        }

        return bytes;
    }



    //  The master with one build of the program put on it, as a build step
    //  would.
    static std::vector<Byte> PlaceBuild (const std::vector<Byte> & master, Byte seed, size_t length = kPayloadBytes)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  options;
        DiskCommandResult   result;



        io.files[kImagePath]  = master;
        io.stamps[kImagePath] = FileStamp { master.size(), 100 };
        io.files[kHostFile]   = MakePayload (seed, length);
        io.stamps[kHostFile]  = FileStamp { length, 100 };

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


    static bool IsInSpan (const EditedFieldSpan & span, const InspectorDriveMotion & motion)
    {
        double  start = span.startTurn - std::floor (span.startTurn);
        double  end   = start + (span.endTurn - span.startTurn);
        double  angle = motion.angle - std::floor (motion.angle);



        angle += (angle < start) ? 1.0 : 0.0;

        return motion.slot == span.slot && angle >= start && angle <= end;
    }



    //  The turn span of each written data field, as the inspector window
    //  builds it for the safe point test.
    static std::vector<EditedFieldSpan> MakeSpans (const DiskImage & drive, const std::vector<SectorWrite> & writes)
    {
        std::vector<EditedFieldSpan>  spans;
        DiskAnalysis                  analysis;



        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (drive, 1, kImagePath, 0, false), DecodeSettings::MakeStandard(), analysis);

        for (const SectorWrite & write : writes)
        {
            const TrackAnalysis &  track = *analysis.tracks[write.slot];

            for (const LocatedField & field : track.fields)
            {
                if (static_cast<int> (field.startCell) == write.dataFieldCell && track.framed.cellCount > 0)
                {
                    spans.push_back ({ write.slot, static_cast<double> (field.startCell) / track.framed.cellCount, static_cast<double> (field.endCell) / track.framed.cellCount });
                }
            }
        }

        return spans;
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

    //  SC-022: after a guest SAVE, the disk now and the disk as inserted
    //  differ only on tracks the guest wrote, and the saved file is only in A.
    TEST_METHOD (ASavedFileIsTheOnlyDifferenceFromTheDiskAsInserted)
    {
        TestMachine                      machine ("Apple2e");
        std::vector<Byte>                master     = GuestSession::RequireDos33Master();
        std::shared_ptr<const DiskCopy>  now;
        std::shared_ptr<const DiskCopy>  inserted;
        std::wstring                     reason;
        DiskAnalysis                     a;
        DiskAnalysis                     b;
        DiskComparison                   comparison;
        std::vector<std::string>         rows;
        int                              differing  = 0;
        int                              onlyInA    = 0;
        HRESULT                          hr         = S_OK;



        GuestSession::BootToPrompt (machine, master);
        KeystrokeInjector::InjectLine (machine, "10 PRINT \"SC-022\"", GuestSession::kLineCycles);
        rows = GuestSession::TypeAndCollect (machine, "SAVE NEWPROG");
        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"the guest saves the program");

        const DiskImage &  image = *machine.GetDiskStore().GetImage (kSlot, kDrive);

        now = DiskCopy::MakeFromImage (image, 1, kImagePath, 0, false);
        hr = InspectorImageLoader::LoadBytes (image.GetSourceBytes(), kImagePath, false, inserted, reason);
        Assert::IsTrue (SUCCEEDED (hr), reason.c_str());

        DiskAnalyzer::Analyze (now,      DecodeSettings::MakeStandard(), a);
        DiskAnalyzer::Analyze (inserted, DecodeSettings::MakeStandard(), b);
        comparison = DiskComparer::Compare (a, b);

        for (const TrackComparison & track : comparison.tracks)
        {
            if (track.verdict == TrackVerdict::SectorsDiffer || track.verdict == TrackVerdict::NibblesDiffer ||
                track.verdict == TrackVerdict::OnlyInA       || track.verdict == TrackVerdict::OnlyInB)
            {
                differing++;
                Assert::IsTrue (track.slotA >= 0 && image.GetGuestWriteCount (track.slotA) > 0, L"a track that differs is one the guest wrote");
            }
        }

        for (const FilePair & file : comparison.files)
        {
            if (file.outcome == DifferenceKind::FileOnlyInA)
            {
                onlyInA++;
                Assert::IsTrue (file.path.find (L"NEWPROG") != std::wstring::npos, file.path.c_str());
            }
            else
            {
                Assert::IsTrue (file.isSame, file.path.c_str());
            }
        }

        Assert::IsTrue  (differing > 0, L"the save changed some tracks");
        Assert::AreEqual (1, onlyInA, L"the saved file is the one file only in A");
    }

    //  SC-018: a guest that reads the edited sector in a loop while "Pause and
    //  apply" runs reads all the old bytes or all the new ones, never a mix
    //  and never a checksum error. The loop sets $300 on a mixed read and
    //  $301 once it reads the new bytes.
    TEST_METHOD (AGuestReadingInALoopSeesAllOldOrAllNewBytes)
    {
        //  With DOS's four-byte address and length, the file fits one sector.
        static constexpr size_t    kOneSector       = kSectorBytes - 4;
        static constexpr Word      kMixedFlag       = 0x0300;
        static constexpr uint64_t  kStepCycles      = 10'000ULL;
        static constexpr int       kMaxSteps        = 20'000;
        static constexpr uint64_t  kFieldStepCycles = 500ULL;
        static constexpr int       kMaxFieldSteps   = 400'000;

        TestMachine             machine ("Apple2e");
        std::vector<Byte>       master  = GuestSession::RequireDos33Master();
        std::vector<Byte>       first   = PlaceBuild (master, 0x21, kOneSector);
        std::vector<Byte>       second  = PlaceBuild (master, 0x5C, kOneSector);
        StoreSource             source (machine);
        InspectorRequestQueue     queue;
        InspectorRequest          request;
        vector<InspectorReply>    replies;
        std::vector<Byte>         flags;
        std::vector<std::string>  rows;
        ApplyOutcome              outcome = ApplyOutcome::Waiting;
        int                       step    = 0;



        GuestSession::BootToPrompt (machine, first);

        request.kind      = InspectorRequestKind::Apply;
        request.drive     = kDrive;
        request.mediaId   = machine.GetDiskStore().GetMediaId (kSlot, kDrive);
        request.applyKind = ApplyKind::Apply;
        request.applyMode = ApplyMode::PauseAtSafePoint;
        request.writes    = MakeWrites (*machine.GetDiskStore().GetImage (kSlot, kDrive), first, second);
        request.spans     = MakeSpans (*machine.GetDiskStore().GetImage (kSlot, kDrive), request.writes);
        Assert::AreEqual (static_cast<size_t> (1), request.writes.size(), L"the two builds differ in one sector");
        Assert::AreEqual (static_cast<size_t> (1), request.spans.size(),  L"which has one data field");

        KeystrokeInjector::InjectLine (machine, "10 PRINT CHR$(4)\"BLOAD PROG\":O=1:N=1", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "20 FOR I=0 TO 251:B=PEEK(24576+I):V=I*7+33:IF B<>V-INT(V/256)*256 THEN O=0", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "30 V=I*7+92:IF B<>V-INT(V/256)*256 THEN N=0", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "40 NEXT:IF O=0 AND N=0 THEN POKE 768,1", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "50 IF N=1 THEN POKE 769,1", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "60 GOTO 10", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "POKE 768,0:POKE 769,0", GuestSession::kLineCycles);
        KeystrokeInjector::InjectLine (machine, "RUN", 0);

        //  Post the apply while the head is inside the edited data field,
        //  which is not a safe point, so the apply waits for the field to pass.
        for (step = 0; step < kMaxFieldSteps && !(source.GetMotion (kDrive).isTurning && IsInSpan (request.spans[0], source.GetMotion (kDrive))); step++)
        {
            (void) machine.RunCycles (kFieldStepCycles);
        }

        Assert::IsTrue (IsInSpan (request.spans[0], source.GetMotion (kDrive)), L"the head is over the edited field while the guest reads");
        queue.Post (request);
        queue.Drain (source);
        queue.TakeReplies (replies);
        Assert::IsTrue (replies.empty() || replies.back().applyOutcome == ApplyOutcome::Waiting, L"no apply with the head inside the field");

        for (step = 0; step < kMaxSteps && outcome == ApplyOutcome::Waiting; step++)
        {
            (void) machine.RunCycles (kStepCycles);
            queue.Drain (source);
            queue.TakeReplies (replies);
            outcome = replies.empty() ? outcome : replies.back().applyOutcome;
        }

        Assert::IsTrue  (outcome == ApplyOutcome::Applied, replies.empty() ? L"no reply" : replies.back().reason.c_str());
        Assert::IsFalse (IsInSpan (request.spans[0], source.savedMotion), L"the apply lands with the head outside the field");

        //  Run the loop until it reads the new bytes.
        for (step = 0; step < kMaxSteps; step++)
        {
            (void) machine.RunCycles (kPassCycles);
            flags = GuestSession::GuestBytesAt (machine, kMixedFlag, 2);

            if (flags[1] != 0)
            {
                break;
            }
        }

        GuestSession::CollectRows (machine, rows);

        Assert::IsFalse  (GuestSession::AnyRowContains (rows, "ERROR"), L"every read completes with a good checksum");
        Assert::AreEqual (static_cast<int> (0), static_cast<int> (flags[0]), L"no read returned a mix of old and new bytes");
        Assert::AreEqual (static_cast<int> (1), static_cast<int> (flags[1]), L"the loop reads the new bytes");
    }
};
