#include "Pch.h"
#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "CassoExplorer/ExplorerInspectorHost.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Seams/IIntentChannel.h"
#include "Seams/IntentReplyTracker.h"
#include "Seams/Win32IntentChannel.h"
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

    //  The path GuestSession mounts its disk under, and the two builds of
    //  the program the shared-image tests use.
    static constexpr const char    *  kSharedPath     = "gate.dsk";
    static constexpr const wchar_t *  kSharedPathWide = L"gate.dsk";
    static constexpr Byte             kFirstSeed      = 0x21;
    static constexpr Byte             kSecondSeed     = 0x5C;
    static constexpr time_t           kPreservedTime  = 1'000'000'000;



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
    //  the drive, found on the drive's tracks by its DOS-order sector index,
    //  as the inspector's editor finds the field it draws.
    static std::vector<SectorWrite> MakeWrites (const DiskImage & drive, const std::vector<Byte> & current, const std::vector<Byte> & target)
    {
        return MakeWrites (DiskCopy::MakeFromImage (drive, 1, kImagePath, 0, false), current, target);
    }



    //  The same edits, found on a copy of the disk.
    static std::vector<SectorWrite> MakeWrites (const std::shared_ptr<const DiskCopy> & disk, const std::vector<Byte> & current, const std::vector<Byte> & target)
    {
        std::vector<SectorWrite>  writes;
        DiskAnalysis              analysis;
        size_t                    offset   = 0;
        int                       track    = 0;
        int                       logical  = 0;



        DiskAnalyzer::Analyze (disk, DecodeSettings::MakeStandard(), analysis);

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
                if (sector.dosOrderIndex == logical && sector.dataField >= 0)
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



    //  Explorer's intent channel, with the test machine's disk store as the
    //  emulator that receives each intent.
    class StoreIntents : public IIntentChannel
    {
    public:
        explicit StoreIntents (MachineHost & machine) : m_machine (machine) {}

        void StateIntent (const std::string & imagePath, ExternalChangeIntent intent) override
        {
            m_machine.GetDiskStore().NoteExternalChange (imagePath, intent, true);
        }

    private:
        MachineHost & m_machine;
    };



    //  A running Casso booted from a disk, and the folder the disk's file is
    //  in. The store reads and writes the folder, and the action it reports
    //  for a changed file is the input of the reply. While isSaveFailing is set
    //  Casso's saves of the disk fail, so the guest's writes stay unsaved
    //  after the drive stops.
    struct SharedDisk
    {
        TestMachine     machine       { "Apple2e" };
        FakeDiskFileIo  io;
        StoreIntents    intents       { machine };
        int64_t         nowMs         = 0;
        bool            isSaveFailing = false;
        int             decisions     = 0;
        ChangeAction    action        = ChangeAction::Ignore;
        bool            isPreserved   = false;
        std::string     preservedPath;

        explicit SharedDisk (const std::vector<Byte> & bytes)
        {
            DiskImageStore &  store = machine.GetDiskStore();



            io.files[kSharedPath]  = bytes;
            io.stamps[kSharedPath] = FileStamp { bytes.size(), 100 };
            io.files[kHostFile]    = MakePayload (kSecondSeed);
            io.stamps[kHostFile]   = FileStamp { kPayloadBytes, 100 };

            GuestSession::BootToPrompt (machine, bytes);

            store.SetFileIo          (&io);
            store.SetClock           ([this] () { return nowMs; });
            store.SetTimestampSource ([] () { return kPreservedTime; });
            store.SetImageReader     ([this] (const std::string & path, std::vector<Byte> & outBytes) { return io.ReadAllBytes (path, outBytes); });

            store.SetFlushSink ([this] (const std::string & path, const std::vector<Byte> & saved)
            {
                return isSaveFailing ? HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION) : io.WriteAllBytes (path, saved);
            });

            store.SetIdentityReader ([this] (const std::string & path)
            {
                ImageIdentity  identity;
                HRESULT        hr = io.Stat (path, identity.stamp);

                identity.recorded = SUCCEEDED (hr);

                return identity;
            });

            store.SetDecisionSink ([this] (const std::string &, ChangeAction newAction, bool preserved, const std::string & savePath)
            {
                decisions++;
                action        = newAction;
                isPreserved   = preserved;
                preservedPath = savePath;
            });
        }

        //  Casso after a change of the file: the quiet period passes, the
        //  reload runs, and the reply is made from the decision as Casso's
        //  window makes it. False when there is no reply.
        bool Reload (Win32IntentChannel::Reply & outReply)
        {
            isSaveFailing  = false;
            nowMs         += MountedImageState::kQuietPeriodMs;
            machine.GetDiskStore().ApplyPendingReload();

            return decisions > 0 && IntentReplyTracker::TryMakeReloadReply (action, isPreserved, preservedPath, outReply);
        }

        const DiskImage & GetDrive()
        {
            return *machine.GetDiskStore().GetImage (kSlot, kDrive);
        }

        std::vector<Byte> GetDriveBytes()
        {
            std::vector<Byte>  bytes;



            AssertSucceeded (GetDrive().Serialize (bytes));

            return bytes;
        }
    };



    //  Explorer's file commands replacing PROG with the second build: a
    //  delete, then a put. With a channel, each command states its intent
    //  after its write.
    static void ReplaceProgram (FakeDiskFileIo & io, IIntentChannel * intents)
    {
        DiskOperations          ops (io);
        DiskOperations::Result  result;



        ops.SetIntentChannel (intents);

        result = ops.Delete (kSharedPath, kPlacedName);
        Logger::WriteMessage (result.message.c_str());
        Assert::IsTrue (result.Succeeded(), L"the delete succeeds");

        result = ops.Put (kSharedPath, kHostFile, kPlacedName, "B", true, kLoadAddress, DiskOperations::Encoding::Verbatim);
        Logger::WriteMessage (result.message.c_str());
        Assert::IsTrue (result.Succeeded(), L"the put succeeds");
    }



    //  A program the guest saves to the disk in the drive, which Casso has
    //  not written to the file.
    static void SaveGuestProgram (SharedDisk & disk, const std::vector<Byte> & file)
    {
        std::vector<std::string>  rows;



        disk.isSaveFailing = true;

        KeystrokeInjector::InjectLine (disk.machine, "10 PRINT \"SC-020\"", GuestSession::kLineCycles);
        rows = GuestSession::TypeAndCollect (disk.machine, "SAVE NEWPROG");

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"the guest saves the program");
        Assert::IsTrue  (disk.GetDrive().IsDirty(), L"the drive has unsaved guest writes");
        Assert::IsTrue  (disk.io.files[kSharedPath] == file, L"and the file is as it was");
    }



    //  SC-020: an apply from Explorer's inspector on the image a running
    //  Casso has, next to Explorer's file commands writing the same bytes to
    //  a second Casso's image. Both Cassos, their drives and their folders
    //  end the same, and the inspector's note is Casso's reply when the reply
    //  is not a plain reload.
    static void AssertAnApplyEndsAsFileCommandsDo (bool hasGuestWrites)
    {
        std::vector<Byte>          master    = GuestSession::RequireDos33Master();
        std::vector<Byte>          first     = PlaceBuild (master, kFirstSeed);
        std::vector<Byte>          target;
        std::vector<Byte>          guestDisk;
        std::vector<Byte>          replyBytes;
        FakeDiskFileIo             scratch;
        SharedDisk                 edited    (first);
        SharedDisk                 commanded (first);
        ExplorerInspectorHost      host      (kSharedPathWide, edited.io, &edited.intents);
        InspectorRequest           request;
        vector<InspectorReply>     replies;
        Win32IntentChannel::Reply  editedReply;
        Win32IntentChannel::Reply  commandedReply;
        COPYDATASTRUCT             copyData  = {};
        std::wstring               note;



        //  The bytes the file commands write, worked out ahead on a copy.
        scratch.files[kSharedPath]  = first;
        scratch.stamps[kSharedPath] = FileStamp { first.size(), 100 };
        scratch.files[kHostFile]    = MakePayload (kSecondSeed);
        scratch.stamps[kHostFile]   = FileStamp { kPayloadBytes, 100 };
        ReplaceProgram (scratch, nullptr);
        target = scratch.files[kSharedPath];
        Assert::IsFalse (target == first, L"the commands change the file");

        AssertGuestLoads (edited.machine,    kFirstSeed);
        AssertGuestLoads (commanded.machine, kFirstSeed);

        if (hasGuestWrites)
        {
            SaveGuestProgram (edited,    first);
            SaveGuestProgram (commanded, first);
            guestDisk = edited.GetDriveBytes();
            Assert::IsTrue (guestDisk == commanded.GetDriveBytes(), L"both guests wrote the same disk");
        }

        //  Explorer's inspector reads the file and applies the difference.
        (void) host.PostInspectorRequest ({ InspectorRequestKind::CopyDisk, 0, 0, {}, -1 });
        host.TakeInspectorReplies (replies);
        Assert::AreEqual (static_cast<size_t> (1), replies.size());
        Assert::IsNotNull (replies[0].disk.get());

        request.kind      = InspectorRequestKind::Apply;
        request.mediaId   = replies[0].mediaId;
        request.applyKind = ApplyKind::Apply;
        request.writes    = MakeWrites (replies[0].disk, first, target);
        Assert::IsFalse (request.writes.empty(), L"the edit changes some sectors");

        for (const SectorWrite & write : request.writes)
        {
            request.expectedCounts.push_back ({ write.slot, 0 });
        }

        replies.clear();
        (void) host.PostInspectorRequest (request);
        host.TakeInspectorReplies (replies);
        Assert::AreEqual (static_cast<size_t> (1), replies.size());
        Assert::IsTrue (replies[0].applyOutcome == ApplyOutcome::Applied, replies[0].reason.c_str());
        Assert::IsTrue (edited.io.files[kSharedPath] == target, L"the apply writes the bytes the file commands write");

        Assert::IsTrue (edited.Reload (editedReply), L"Casso replies to the apply");
        replyBytes        = Win32IntentChannel::EncodeReply (editedReply);
        copyData.dwData   = Win32IntentChannel::GetReplyMessageId();
        copyData.cbData   = static_cast<DWORD> (replyBytes.size());
        copyData.lpData   = replyBytes.data();
        Assert::IsTrue (host.HandleCopyData (copyData), L"the inspector's host takes the reply");
        note = host.TakeHostNote();

        //  Explorer's file commands, on the second Casso's image.
        ReplaceProgram (commanded.io, &commanded.intents);
        Assert::IsTrue (commanded.io.files[kSharedPath] == target);
        Assert::IsTrue (commanded.Reload (commandedReply), L"Casso replies to the file commands");

        //  Casso.
        Assert::IsTrue   (edited.action == commanded.action, L"the actions match");
        Assert::AreEqual (hasGuestWrites, edited.isPreserved);
        Assert::AreEqual (hasGuestWrites, commanded.isPreserved);
        Assert::AreEqual (commanded.preservedPath, edited.preservedPath);
        Assert::IsTrue   (editedReply.kind == commandedReply.kind, L"and so do the replies");
        Assert::AreEqual (commandedReply.text, editedReply.text);

        //  Its drive.
        Assert::IsTrue  (edited.GetDriveBytes()    == target, L"the drive has the applied disk");
        Assert::IsTrue  (commanded.GetDriveBytes() == target, L"as it has the file commands' disk");
        Assert::IsFalse (edited.GetDrive().IsDirty());
        Assert::IsFalse (commanded.GetDrive().IsDirty());

        //  Its files.
        Assert::IsTrue (edited.io.files == commanded.io.files, L"the two folders have the same files");

        if (hasGuestWrites)
        {
            Assert::IsTrue  (editedReply.kind == Win32IntentChannel::ReplyKind::ReloadConflict);
            Assert::IsTrue  (edited.io.files.at (edited.preservedPath) == guestDisk, L"the guest's disk is in a file beside the image");
            Assert::IsFalse (note.empty(), L"the inspector's note is Casso's reply");
            Assert::IsTrue  (note == ExplorerInspectorHost::DescribeCassoReply (replyBytes.data(), replyBytes.size()), note.c_str());
        }
        else
        {
            Assert::IsTrue (editedReply.kind == Win32IntentChannel::ReplyKind::ReloadDone);
            Assert::IsTrue (note.empty(), note.c_str());
        }

        //  The guest reads the new build on both.
        AssertGuestLoads (edited.machine,    kSecondSeed);
        AssertGuestLoads (commanded.machine, kSecondSeed);
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



    TEST_METHOD (AnExplorerApplyEndsAsExplorerFileCommandsDo)
    {
        AssertAnApplyEndsAsFileCommandsDo (false);
    }



    TEST_METHOD (AnExplorerApplyOverUnsavedGuestWritesEndsAsFileCommandsDo)
    {
        AssertAnApplyEndsAsFileCommandsDo (true);
    }
};
