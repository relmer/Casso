#include "Pch.h"

#include "Debugger/CallStackHistory.h"
#include "Debugger/Handlers/CallStackHandlers.h"
#include "Debugger/Reverse/ReverseController.h"
#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "HandlerTestRig.h"
#include "HResultAssert.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Shell/ScratchCallReplayer.h"
#include "Ui/Debugger/Panes/CallStackPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistoryTests
//
//  The call record rebuilt from history when it starts mid-run: the record
//  a rebuild hands over, continued to where the machine has run meanwhile,
//  is the record a recorder that watched from power-on would hold, resets,
//  power cycles and recorded inputs included, and the live record goes on
//  from it as that one would; history that does not reach power-on, or is
//  cut by a gap or a change of disks, gives a record whose bottom marks the
//  calls before it as unknown, and the rebuild replays nothing before that
//  point; a rebuild under way is shown in the pane, with its progress; a
//  rebuild that stopped anywhere but where the machine stands is not taken;
//  and closing the debugger, a reset or a move through history drops it
//  cleanly, while a reverse command that leaves the machine where it was
//  does not. Copies of the record kept at keyframes, as they are taken and
//  by every rebuild, let a move replay one keyframe span at most and give
//  the same record; they go with their keyframes and fit their share of the
//  budget. A record no rebuild can replace marks only that the calls before
//  it are not available.
//
//  The program runs on a //e with a Mockingboard: three nested calls, a TXS
//  inside the innermost, and a loop calling a leaf routine and pushing and
//  pulling, while the Mockingboard's timer interrupts every 4096 cycles. The
//  warm-start vector points at it, so a Ctrl+Reset starts it again.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CallStackHistoryTests)
    {
    public:

        using MachineRig = MachineHandlerRig<CallStackHandlers>;

        static constexpr Word      kStart        = 0x0800;
        static constexpr Word      kMiddle       = 0x0850;
        static constexpr Word      kLoopCall     = 0x0866;   // the low byte of the loop's JSR leaf
        static constexpr Word      kLeaf         = 0x0870;
        static constexpr Word      kHandler      = 0x0880;
        static constexpr Word      kLeafCaller   = 0x0890;
        static constexpr uint64_t  kEdit         = 10000;
        static constexpr uint64_t  kSwap         = 12000;
        static constexpr uint64_t  kSwapBack     = 20000;
        static constexpr int       kDiskSlot     = 6;
        static constexpr int       kDiskDrive    = 0;
        static constexpr int       kSecondDrive  = 1;
        static constexpr uint64_t  kAttachAt     = 30000;    // well into the loop, all three calls made
        static constexpr uint64_t  kHistoryStart = 20000;    // the same, for history that starts late
        static constexpr uint64_t  kGapStart     = 10000;
        static constexpr uint64_t  kGapEnd       = 15000;
        static constexpr uint64_t  kResetAt      = 15000;
        static constexpr uint64_t  kAfterReset   = 115000;   // past the beep a reset sounds, the program well into its loop again
        static constexpr uint64_t  kFarAhead     = 400000;   // more than a catch-up's worth past the attach, and several keyframes in
        static constexpr uint64_t  kRunOn        = 25000;
        static constexpr int       kDrainLimit   = 16;
        static constexpr Word      kSoftEntry    = 0x03F2;   // the warm-start vector, and the power-up byte after it
        static constexpr Byte      kPowerUpEor   = 0xA5;     // the power-up byte is the vector's high byte with these bits flipped

        static constexpr float     kProgressTolerance = 0.001f;



        //  Loads the program with the PC at its start, interrupts masked until
        //  it clears them itself, and points the warm-start vector at it.
        static void LoadProgram (MachineRig & rig)
        {
            static constexpr Byte  kMain[] =
            {
                0xA2, 0xFF,             // 0800  LDX #$FF
                0x9A,                   // 0802  TXS
                0xA9, 0x80,             // 0803  LDA #<handler
                0x8D, 0xFE, 0x03,       // 0805  STA $03FE
                0xA9, 0x08,             // 0808  LDA #>handler
                0x8D, 0xFF, 0x03,       // 080A  STA $03FF
                0x20, 0x20, 0x08,       // 080D  JSR setup
                0x20, 0x40, 0x08,       // 0810  JSR outer
                0x4C, 0x13, 0x08,       // 0813  JMP $0813
            };
            static constexpr Byte  kSetup[] =
            {
                0xA9, 0x40,             // 0820  LDA #$40     T1 free-running
                0x8D, 0x0B, 0xC4,       // 0822  STA $C40B    ACR
                0xA9, 0xC0,             // 0825  LDA #$C0     enable T1's interrupt
                0x8D, 0x0E, 0xC4,       // 0827  STA $C40E    IER
                0xA9, 0x00,             // 082A  LDA #$00
                0x8D, 0x04, 0xC4,       // 082C  STA $C404    T1 latch low
                0xA9, 0x10,             // 082F  LDA #$10
                0x8D, 0x05, 0xC4,       // 0831  STA $C405    T1 high: every 4096 cycles
                0x58,                   // 0834  CLI
                0x60,                   // 0835  RTS
            };
            static constexpr Byte  kCalls[] =
            {
                0x20, 0x50, 0x08,       // 0840  outer   JSR middle
                0x60,                   // 0843          RTS
                0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
                0x20, 0x60, 0x08,       // 0850  middle  JSR inner
                0x60,                   // 0853          RTS
                0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
                0xBA,                   // 0860  inner   TSX
                0x9A,                   // 0861          TXS
                0xEE, 0x00, 0x03,       // 0862  loop    INC $0300
                0x20, 0x70, 0x08,       // 0865          JSR leaf
                0x48,                   // 0868          PHA
                0x68,                   // 0869          PLA
                0x4C, 0x62, 0x08,       // 086A          JMP loop
                0xEA, 0xEA, 0xEA,
                0xEA,                   // 0870  leaf    NOP
                0x60,                   // 0871          RTS
                0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
                0xAD, 0x04, 0xC4,       // 0880  handler LDA $C404    clears T1's flag
                0xA5, 0x45,             // 0883          LDA $45      the ROM kept A there
                0x40,                   // 0885          RTI
                0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
                0x20, 0x70, 0x08,       // 0890  caller  JSR leaf     reached only once EditLoop points the loop here
                0x60,                   // 0893          RTS
            };
            Word  at = kStart;



            for (Byte b : kMain)
            {
                rig.target.TryPoke (at++, b);
            }

            at = 0x0820;

            for (Byte b : kSetup)
            {
                rig.target.TryPoke (at++, b);
            }

            at = 0x0840;

            for (Byte b : kCalls)
            {
                rig.target.TryPoke (at++, b);
            }

            rig.target.TryPoke (kSoftEntry,     (Byte) (kStart & 0xFF));
            rig.target.TryPoke (kSoftEntry + 1, (Byte) (kStart >> 8));
            rig.target.TryPoke (kSoftEntry + 2, (Byte) ((kStart >> 8) ^ kPowerUpEor));

            rig.Load (kStart, {}, kStart);
        }



        //  Points the loop's call at a routine that calls the leaf itself, so
        //  from here on each call the loop makes is two deep.
        static void EditLoop (MachineRig & rig)
        {
            rig.target.TryPoke (kLoopCall, (Byte) (kLeafCaller & 0xFF));
        }



        //  A disk image of its own in a slot 6 drive, journaled first as the
        //  CPU thread journals a mount, and mounted as a restored disk is, so
        //  history records it; the program never reads it.
        static void MountDisk (TestMachine & machine, const std::string & name, int drive = kDiskDrive)
        {
            std::vector<Byte>  bytes (NibblizationLayer::kImageByteSize, 0);
            HRESULT            hr    = S_OK;



            machine.RecordInput (InputKind::DiskMount, (Byte) drive, 0, name);

            hr = machine.GetDiskStore().MountRestored (kDiskSlot, drive, name, DiskFormat::Dsk, bytes);
            AssertSucceeded (hr, L"MountRestored");
        }



        //  The disk in a slot 6 drive taken out, journaled first as a mount is.
        static void EjectDisk (TestMachine & machine, int drive)
        {
            machine.RecordInput (InputKind::DiskEject, (Byte) drive, 0, {});
            machine.GetDiskStore().Eject (kDiskSlot, drive);
        }



        //  A Ctrl+Reset or a power cycle as the CPU thread makes one:
        //  journaled, made, then reported to the debugger.
        static void Reset (MachineRig & rig, bool isPowerCycle)
        {
            rig.machine.RecordInput (isPowerCycle ? InputKind::PowerCycle : InputKind::Reset, 0, 0, {});

            if (isPowerCycle)
            {
                rig.machine.PowerCycle();
            }
            else
            {
                rig.machine.SoftReset();
            }

            rig.session.OnReset (isPowerCycle);
        }



        //  A machine with the program loaded, recording history from where
        //  it stands once it has run to historyFrom, and rebuilding its call
        //  record on a scratch machine whose jobs wait on a queue the test
        //  runs.
        struct HistoryRig
        {
            MachineRig           base       { "Apple2e", TestMachine::Slots::AsShipped };
            ReverseController    controller { base.machine };
            InlineWorkQueue      queue;
            ScratchCallReplayer  replayer;
            CallStackHistory     history    { base.machine, base.session };

            explicit HistoryRig (uint64_t historyFrom = 0, bool hasDisk = false, const ReverseSettings & settings = ReverseSettings())
            {
                HRESULT  hr = S_OK;



                LoadProgram (base);
                RunTo (base.machine, historyFrom);

                if (hasDisk)
                {
                    base.machine.GetDiskStore().SetFlushSink      ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
                    base.machine.GetDiskStore().SetIdentityReader ([] (const std::string &) { return ImageIdentity(); });
                    MountDisk (base.machine, "first.dsk");
                }

                hr = controller.Start (settings);
                AssertSucceeded (hr, L"Start");

                replayer.SetWorkQueue (&queue);
                replayer.SetMachine   (base.machine.GetConfig(), base.machine.GetCurrentMachineName());
                history.Attach        (&controller, &replayer);
            }

            ~HistoryRig()
            {
                history.Attach (nullptr, nullptr);
            }

            //  The debugger attaches here: recording starts, and the history
            //  requests its rebuild.
            void Attach()
            {
                base.session.SetCallRecording (true);
                history.Service();
            }

            //  Every rebuild waiting runs and is taken in, continued and
            //  run again, until it is installed or dropped.
            void Drain()
            {
                int  rounds = 0;



                do
                {
                    queue.WaitAll();
                    history.Service();
                }
                while (history.IsRebuilding() && ++rounds < kDrainLimit);

                Assert::IsFalse (history.IsRebuilding(), L"the rebuild finished");
            }
        };



        //  The reference: the same program on a machine of its own, recorded
        //  from where it stands once it has run to recordFrom.
        struct RecordRig
        {
            MachineRig  base { "Apple2e", TestMachine::Slots::AsShipped };

            explicit RecordRig (uint64_t recordFrom = 0)
            {
                LoadProgram (base);
                RunTo (base.machine, recordFrom);
                base.session.SetCallRecording (true);
            }
        };



        static void RunTo (TestMachine & machine, uint64_t position)
        {
            while (machine.GetPosition() < position)
            {
                machine.StepOne();
            }
        }



        //  The first position after `after` with the PC at pc, the loop edited
        //  at editAt when that is given.
        static uint64_t FindPcAfter (uint64_t after, Word pc, uint64_t editAt = 0)
        {
            constexpr uint64_t  kSearchLimit = 1'000'000;
            RecordRig           rig;
            uint64_t            limit        = after + kSearchLimit;



            if (editAt != 0)
            {
                RunTo    (rig.base.machine, editAt);
                EditLoop (rig.base);
            }

            RunTo (rig.base.machine, after + 1);

            while (rig.base.machine.GetCpu()->GetPC() != pc && rig.base.machine.GetPosition() < limit)
            {
                rig.base.machine.StepOne();
            }

            Assert::AreEqual (pc, rig.base.machine.GetCpu()->GetPC(), L"the program reaches the address");
            return rig.base.machine.GetPosition();
        }



        static CallRecord GetRecord (DebugSession & session)
        {
            DebugSessionView  view;



            session.TakeView (view);
            return view.callRecord;
        }



        static void AssertSameFrame (const CallStackFrame & expected, const CallStackFrame & actual, const std::wstring & where)
        {
            Assert::AreEqual (expected.callSite,         actual.callSite,         (where + L": call site").c_str());
            Assert::AreEqual (expected.target,           actual.target,           (where + L": target").c_str());
            Assert::AreEqual ((int) expected.kind,       (int) actual.kind,       (where + L": kind").c_str());
            Assert::AreEqual ((int) expected.provenance, (int) actual.provenance, (where + L": provenance").c_str());
            Assert::AreEqual (expected.stackLevel,       actual.stackLevel,       (where + L": stack level").c_str());
            Assert::AreEqual (expected.cycle,            actual.cycle,            (where + L": cycle").c_str());
            Assert::AreEqual (expected.isVerified,       actual.isVerified,       (where + L": verified").c_str());
            Assert::AreEqual (expected.isRewritten,      actual.isRewritten,      (where + L": rewritten").c_str());
            Assert::AreEqual (expected.hasRisen,         actual.hasRisen,         (where + L": risen").c_str());
            Assert::AreEqual (expected.note,             actual.note,             (where + L": note").c_str());
        }



        //  Every frame, break and the last return the same; the bottom's kind
        //  too, unless isBottomKindSkipped.
        static void AssertSameRecord (const CallRecord & expected, const CallRecord & actual, bool isBottomKindSkipped = false)
        {
            size_t  i = 0;



            Assert::IsTrue   (actual.isActive, L"the record is on");
            Assert::AreEqual (expected.frames.size(), actual.frames.size(), L"as many frames");
            Assert::AreEqual (expected.breaks.size(), actual.breaks.size(), L"as many breaks");
            Assert::AreEqual (expected.startCycle,    actual.startCycle,    L"dated from the same cycle");

            for (i = 0; i < expected.frames.size(); i++)
            {
                AssertSameFrame (expected.frames[i], actual.frames[i], std::format (L"frame {}", i));
            }

            for (i = 0; i < expected.breaks.size(); i++)
            {
                if (!(isBottomKindSkipped && i == 0))
                {
                    Assert::AreEqual ((int) expected.breaks[i].info.kind, (int) actual.breaks[i].info.kind, std::format (L"break {}: kind", i).c_str());
                }

                Assert::AreEqual (expected.breaks[i].info.pc,     actual.breaks[i].info.pc,     std::format (L"break {}: pc", i).c_str());
                Assert::AreEqual (expected.breaks[i].info.opcode, actual.breaks[i].info.opcode, std::format (L"break {}: opcode", i).c_str());
                Assert::AreEqual (expected.breaks[i].depth,       actual.breaks[i].depth,       std::format (L"break {}: depth", i).c_str());
            }

            Assert::AreEqual (expected.lastReturn.has_value(), actual.lastReturn.has_value(), L"a last return in both or neither");

            if (expected.lastReturn.has_value())
            {
                AssertSameFrame (*expected.lastReturn, *actual.lastReturn, L"last return");
            }
        }



        static bool HasTarget (const CallRecord & record, Word target)
        {
            return std::ranges::any_of (record.frames, [target] (const CallStackFrame & frame) { return frame.target == target; });
        }



        static bool HasKind (const CallRecord & record, CallFrameKind kind)
        {
            return std::ranges::any_of (record.frames, [kind] (const CallStackFrame & frame) { return frame.kind == kind; });
        }



        static bool HasBreak (const CallRecord & record, CallBreakKind kind, size_t depth)
        {
            return std::ranges::any_of (record.breaks, [kind, depth] (const CallStackRecorder::Break & each) { return each.info.kind == kind && each.depth == depth; });
        }



        //  Whether history holds a keyframe at position, taken where recording
        //  resumed after a gap.
        static bool IsAfterGap (HistoryRig & rig, uint64_t position)
        {
            const KeyframeStore  & keyframes = rig.controller.GetKeyframes();
            size_t                 index     = 0;
            bool                   isFound   = keyframes.TryFindByPosition (position, index);



            return isFound && keyframes.GetInfo (index).position == position && keyframes.GetInfo (index).hasGapBefore;
        }



        //  The position of the newest keyframe at or before position.
        static uint64_t GetKeyframeAtOrBefore (HistoryRig & rig, uint64_t position)
        {
            const KeyframeStore  & keyframes = rig.controller.GetKeyframes();
            size_t                 index     = 0;
            bool                   isFound   = keyframes.TryFindByPosition (position, index);



            Assert::IsTrue (isFound, L"history holds a keyframe at or before the position");
            return keyframes.GetInfo (index).position;
        }



        //  Whether every copy of the record kept is at a keyframe history holds.
        static bool AreCopiesAtKeyframes (HistoryRig & rig)
        {
            const CallRecordCopies  & copies    = rig.history.GetCopies();
            const KeyframeStore     & keyframes = rig.controller.GetKeyframes();
            size_t                    index     = 0;
            size_t                    i         = 0;
            bool                      isFound   = false;



            for (i = 0; i < copies.GetCount(); i++)
            {
                isFound = keyframes.TryFindByPosition (copies.GetPosition (i), index);

                if (!isFound || keyframes.GetInfo (index).position != copies.GetPosition (i))
                {
                    return false;
                }
            }

            return true;
        }



        //  A rebuilder that replays nothing: it keeps the jobs it is handed,
        //  and hands back the results a test gives it.
        class HandFedRebuilder : public ICallStackRebuilder
        {
        public:
            HRESULT Submit (std::shared_ptr<const CallStackRebuildJob> job) override
            {
                jobs.push_back (job);
                return S_OK;
            }

            bool TryTakeResult (CallStackRebuildResult & outResult) override
            {
                bool  hasOne = !results.empty();



                if (hasOne)
                {
                    outResult = results.front();
                    results.pop_front();
                }

                return hasOne;
            }

            void Cancel() override
            {
                results.clear();
            }

            HRESULT Rebuild (const CallStackRebuildJob & job, CallStackRebuildResult & outResult) override
            {
                UNREFERENCED_PARAMETER (job);

                outResult = CallStackRebuildResult();
                return E_NOTIMPL;
            }

            float GetProgress() const override
            {
                return 0.0f;
            }

            std::vector<std::shared_ptr<const CallStackRebuildJob>>  jobs;
            std::deque<CallStackRebuildResult>                       results;
        };



        //  What a rebuild that stopped where the machine stands hands back,
        //  for the job the rebuilder last took: a record from power-on that
        //  holds no frame, which the live record is not.
        static CallStackRebuildResult MakeHandFedResult (HistoryRig & rig, const HandFedRebuilder & rebuilder)
        {
            CallStackRebuildResult    result;
            CallStackRecorder::Break  bottom;
            bool                      hasJob = !rebuilder.jobs.empty();



            Assert::IsTrue (hasJob, L"the rebuilder took a job");

            result.generation = rebuilder.jobs.back()->generation;
            result.position   = rig.base.machine.GetPosition();
            result.cycle      = rig.base.machine.GetCpu()->GetTotalCycles();
            result.registers  = rig.base.machine.GetCpu()->GetCpu6502()->GetRegisters();

            bottom.info            = CallStackBreak { CallBreakKind::PowerOn, result.registers.pc, 0 };
            result.record.isActive = true;
            result.record.breaks.push_back (bottom);

            return result;
        }



        //  One slice of the reverse rig's session on both machines: the
        //  host's inputs for it, then a frame of machine time.
        static void FeedSlice (TestMachine & machine, TestMachine & reference, size_t slice)
        {
            ReverseSessionRig::Feed (machine,   slice);
            ReverseSessionRig::Feed (reference, slice);

            machine.RunCycles   (KeyframeSettings::kFrameCycles);
            reference.RunCycles (KeyframeSettings::kFrameCycles);
        }



        //  The record a rebuild hands over is the one kept from power-on, at
        //  a stop inside the interrupt handler with three calls below it and
        //  a TXS between them, after the machine ran on far enough that the
        //  rebuild was continued by its worker. Recording then goes on from
        //  it as the one from power-on goes on.
        TEST_METHOD (TheRebuiltRecordIsTheOneKeptFromPowerOn)
        {
            uint64_t    stop      = FindPcAfter (kFarAhead, kHandler);
            HistoryRig  rig;
            RecordRig   reference;
            CallRecord  expected;
            CallRecord  actual;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            Assert::IsTrue   (rig.history.IsRebuilding(),            L"attaching starts a rebuild");
            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(), L"on the rebuilder's worker");

            RunTo (rig.base.machine, stop);
            rig.queue.WaitAll();
            rig.history.Service();

            Assert::IsTrue   (rig.history.IsRebuilding(),            L"the machine ran on far: the rebuild goes on");
            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(), L"on the worker again");

            rig.Drain();

            RunTo (reference.base.machine, stop);
            expected = GetRecord (reference.base.session);
            actual   = GetRecord (rig.base.session);

            Assert::IsTrue (HasKind  (expected, CallFrameKind::Irq),        L"the stop is inside the interrupt");
            Assert::IsTrue (expected.frames.size() >= 4,                     L"below it, the three calls");
            Assert::IsTrue (HasBreak (expected, CallBreakKind::Txs, 3),      L"and the TXS inside the third");
            Assert::IsTrue (HasBreak (expected, CallBreakKind::PowerOn, 0),  L"from power-on");

            AssertSameRecord (expected, actual);

            RunTo (reference.base.machine, stop + kRunOn);
            RunTo (rig.base.machine,       stop + kRunOn);

            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  A machine that runs on while the worker continues the rebuild, as
        //  far as a pass at double speed takes it, is caught up here once
        //  the worker has had a round, rather than sent back to the worker.
        TEST_METHOD (ARebuildChasingARunningMachineFinishesAfterOneRound)
        {
            static constexpr uint64_t  kPassFrames = 3;
            HistoryRig                 rig;
            RecordRig                  reference;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            RunTo (rig.base.machine, kFarAhead);
            rig.queue.WaitAll();
            rig.history.Service();

            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(), L"far behind: continued on the worker");

            rig.base.machine.RunCycles (kPassFrames * KeyframeSettings::kFrameCycles);
            rig.queue.WaitAll();
            rig.history.Service();

            Assert::IsFalse  (rig.history.IsRebuilding(),            L"finished here after one round");
            Assert::AreEqual ((size_t) 0, rig.queue.GetPendingCount(), L"nothing left on the worker");

            RunTo (reference.base.machine, rig.base.machine.GetPosition());
            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  The machine ran on only a little while the rebuild ran: it is
        //  finished at once, with nothing more on the worker.
        TEST_METHOD (ARebuildCloseBehindTheMachineFinishesAtOnce)
        {
            uint64_t    stop      = kAttachAt + 1000;
            HistoryRig  rig;
            RecordRig   reference;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            RunTo (rig.base.machine, stop);
            rig.queue.WaitAll();
            rig.history.Service();

            Assert::IsFalse  (rig.history.IsRebuilding(),            L"caught up and installed in one pass");
            Assert::AreEqual ((size_t) 0, rig.queue.GetPendingCount(), L"nothing left on the worker");

            RunTo (reference.base.machine, stop);
            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  History that starts after power-on gives the record a recorder
        //  started where history starts would keep, with its bottom saying
        //  that the calls before it are unknown, and the pane saying so.
        TEST_METHOD (HistoryThatStartsLateSaysEarlierCallsAreUnknown)
        {
            uint64_t                         stop      = FindPcAfter (kAttachAt, kHandler);
            HistoryRig                       rig       (kHistoryStart);
            RecordRig                        reference (kHistoryStart);
            CallRecord                       actual;
            std::optional<CallStackBreak>    bottom;
            std::vector<CallStackPane::Row>  rows;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();
            RunTo (rig.base.machine, stop);
            rig.Drain();

            RunTo (reference.base.machine, stop);
            actual = GetRecord (rig.base.session);
            bottom = rig.base.session.GetCallRecordBottom();

            AssertSameRecord (GetRecord (reference.base.session), actual, true);

            Assert::IsTrue  (bottom.has_value() && bottom->kind == CallBreakKind::HistoryBegan, L"the record begins where history does");
            Assert::IsFalse (HasTarget (actual, kMiddle), L"the calls made before it are not in the record");

            rows = CallStackPane::GetRows (rig.base.session.GetCallStack());

            Assert::IsTrue (std::ranges::any_of (rows, [&bottom] (const CallStackPane::Row & row)
            {
                return row.isNote && row.routine == CallStackPane::GetHistoryStartNote (bottom->pc);
            }), L"the pane's note: the earlier calls are unknown");
        }


        //  While the record is rebuilt, the call stack shows that in place of
        //  the note on where recording began, and goes back to it once done.
        TEST_METHOD (ARebuildUnderWayIsShownInThePane)
        {
            HistoryRig                       rig;
            CallStackData                    data;
            std::vector<CallStackPane::Row>  rows;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            data = rig.base.session.GetCallStack();
            rows = CallStackPane::GetRows (data);

            Assert::IsTrue (data.rebuildProgress.has_value(), L"the call stack holds the rebuild's progress");
            Assert::IsTrue (std::ranges::any_of (rows, [] (const CallStackPane::Row & row) { return row.isNote && row.routine == CallStackPane::GetRebuildingNote (0.0f); }),
                            L"the pane's note: the earlier calls are being rebuilt");

            rig.Drain();

            data = rig.base.session.GetCallStack();
            rows = CallStackPane::GetRows (data);

            Assert::IsFalse (data.rebuildProgress.has_value(), L"done");
            Assert::IsFalse (std::ranges::any_of (rows, [] (const CallStackPane::Row & row) { return row.isNote; }), L"a record from power-on has no note");
        }


        //  Closing the debugger drops the rebuild under way: its result is
        //  never taken in, and opening it again starts a fresh one.
        TEST_METHOD (ClosingTheDebuggerDropsTheRebuild)
        {
            HistoryRig  rig;
            RecordRig   reference;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            rig.base.session.SetCallRecording (false);
            rig.history.Service();

            Assert::IsFalse (rig.history.IsRebuilding(), L"closing dropped it");
            Assert::IsFalse (rig.base.session.GetCallStack().rebuildProgress.has_value(), L"and no progress is shown");

            rig.queue.WaitAll();
            rig.history.Service();

            Assert::IsFalse (rig.base.session.IsCallRecording(), L"no record was brought back by it");

            rig.Attach();
            Assert::IsTrue (rig.history.IsRebuilding(), L"opening again starts a rebuild of its own");

            rig.Drain();

            RunTo (reference.base.machine, rig.base.machine.GetPosition());
            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  A reset while the record is rebuilt makes the old calls moot: the
        //  rebuild is dropped and the record is the one since the reset.
        TEST_METHOD (AResetDropsTheRebuild)
        {
            HistoryRig                     rig;
            CallRecord                     before;
            std::optional<CallStackBreak>  bottom;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            rig.base.machine.SoftReset();
            rig.base.session.OnReset (false);
            rig.history.Service();

            Assert::IsFalse (rig.history.IsRebuilding(), L"the reset dropped it");

            before = GetRecord (rig.base.session);

            rig.queue.WaitAll();
            rig.history.Service();

            bottom = rig.base.session.GetCallRecordBottom();

            Assert::IsTrue  (bottom.has_value() && bottom->kind == CallBreakKind::Reset, L"the record begins at the reset");
            Assert::IsFalse (rig.history.IsRebuilding(), L"and no rebuild came back");
            AssertSameRecord (before, GetRecord (rig.base.session));
        }


        //  A move through history while the record is rebuilt starts it again
        //  where the machine landed, and the record rebuilt there is the one
        //  kept from power-on to there.
        TEST_METHOD (AMoveDuringARebuildRebuildsWhereTheMachineLands)
        {
            uint64_t       landing = FindPcAfter (kAttachAt / 2, kHandler);
            HistoryRig     rig;
            RecordRig      reference;
            ReverseResult  result;
            HRESULT        hr      = S_OK;



            RunTo (rig.base.machine, kFarAhead);
            rig.Attach();

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (landing, result);
            AssertSucceeded (hr, L"SeekToPosition");
            Assert::AreEqual (landing, rig.base.machine.GetPosition(), L"the seek lands at the position given");

            rig.history.OnMoved (false);
            rig.history.Service();

            Assert::IsTrue (rig.history.IsRebuilding(), L"a rebuild for where it landed");

            rig.Drain();

            RunTo (reference.base.machine, landing);
            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  A drag of the history timeline starts the record again at every
        //  move, and rebuilds it only once the drag is let go, even when the
        //  last move left the machine where it was. The rebuild starts from
        //  nothing, not from where the last one, which was finished, ended.
        TEST_METHOD (ADragRebuildsOnlyOnceLetGo)
        {
            HistoryRig             rig;
            RecordRig              reference;
            ReverseResult          result;
            HRESULT                hr        = S_OK;
            uint64_t               landing   = 0;
            std::optional<float>   progress;



            RunTo (rig.base.machine, kFarAhead);
            rig.Attach();
            rig.Drain();

            Assert::AreEqual (1.0f, rig.replayer.GetProgress(), L"the last rebuild finished");

            for (uint64_t at : { kAttachAt, kAttachAt * 2, kAttachAt * 3 })
            {
                rig.history.OnMoving();

                hr = rig.controller.SeekToPosition (at, result);
                AssertSucceeded (hr, L"SeekToPosition");

                rig.history.OnMoved (true);
                rig.history.Service();

                Assert::AreEqual ((size_t) 0, rig.queue.GetPendingCount(), L"no rebuild while the drag goes on");
                Assert::IsTrue   (rig.base.session.GetCallStack().rebuildProgress.has_value(), L"though one is due");
            }

            landing = rig.base.machine.GetPosition();

            rig.history.OnMoving();
            rig.history.OnMoved (false);
            rig.history.Service();

            progress = rig.base.session.GetCallStack().rebuildProgress;

            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(), L"one rebuild, once let go");
            Assert::IsTrue   (progress.has_value() && *progress == 0.0f, L"shown from nothing");

            rig.Drain();

            RunTo (reference.base.machine, landing);
            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  A debugger edit in history is a keyframe a replay loads rather
        //  than reaches. The rebuild loads it on its way and the record goes
        //  on across it, as the one kept from power-on went on across the
        //  edit itself.
        TEST_METHOD (AnEditInHistoryIsLoadedOnTheWay)
        {
            uint64_t             stop    = FindPcAfter (kAttachAt, kLeaf, kEdit);
            HistoryRig           rig;
            RecordRig            reference;
            CallStackRebuildJob  job;
            CallRecord           expected;
            bool                 hasJob  = false;
            HRESULT              hr      = S_OK;



            RunTo    (rig.base.machine, kEdit);
            EditLoop (rig.base);
            RunTo    (rig.base.machine, kAttachAt);

            hr = rig.history.MakeJob (false, 0, 0, kAttachAt, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");

            Assert::IsTrue   (hasJob, L"history holds the run");
            Assert::AreEqual ((size_t) 2, job.parts.size(), L"the edit starts a part of its own");
            Assert::AreEqual (kEdit, job.parts[1].startPosition, L"where it was made");

            rig.Attach();
            RunTo (rig.base.machine, stop);
            rig.Drain();

            RunTo    (reference.base.machine, kEdit);
            EditLoop (reference.base);
            RunTo    (reference.base.machine, stop);

            expected = GetRecord (reference.base.session);

            Assert::IsTrue (HasTarget (expected, kLeafCaller), L"the stop is in the leaf the edited loop reaches through its caller");

            AssertSameRecord (expected, GetRecord (rig.base.session));
        }


        //  A keyframe saved with disks other than the ones in the bays now
        //  cannot be loaded over them, so the record begins at the change of
        //  disk, and marks the calls before it as unknown.
        TEST_METHOD (ADiskChangedInHistoryStartsTheRecordAfterIt)
        {
            uint64_t                       stop      = FindPcAfter (kAttachAt, kHandler);
            HistoryRig                     rig       (0, true);
            RecordRig                      reference (kSwap);
            CallStackRebuildJob            job;
            std::optional<CallStackBreak>  bottom;
            bool                           hasJob    = false;
            HRESULT                        hr        = S_OK;



            RunTo (rig.base.machine, kSwap);

            rig.base.machine.RecordInput (InputKind::DiskEject, 0, 0, {});
            rig.base.machine.GetDiskStore().Eject (kDiskSlot, kDiskDrive);
            MountDisk (rig.base.machine, "second.dsk");

            RunTo (rig.base.machine, kAttachAt);

            hr = rig.history.MakeJob (false, 0, 0, kAttachAt, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");

            Assert::IsTrue   (hasJob, L"history holds the run");
            Assert::AreEqual ((size_t) 2, job.parts.size(), L"the change of disk starts a part of its own");
            Assert::AreEqual (kSwap, job.parts[1].startPosition, L"where it was made");

            rig.Attach();
            RunTo (rig.base.machine, stop);
            rig.Drain();

            RunTo (reference.base.machine, stop);
            bottom = rig.base.session.GetCallRecordBottom();

            Assert::IsTrue (bottom.has_value() && bottom->kind == CallBreakKind::HistoryBegan, L"the record begins at the change of disk");

            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session), true);
        }


        //  Where history starts, as the bottom of a rebuilt record, and where
        //  recording began again after a move no rebuild replaced: each one's
        //  name in the protocol, and the one sentence that both the line CALLS
        //  prints and the pane's note across the row give; and the pane's
        //  note while a rebuild runs.
        TEST_METHOD (WhereHistoryStartsIsDescribedInEveryForm)
        {
            CallStackBreak                   began     { CallBreakKind::HistoryBegan, 0x0803, 0 };
            CallStackBreak                   restarted { CallBreakKind::TrackingRestarted, 0x0905, 0 };
            CallStackData                    data;
            std::vector<CallStackPane::Row>  rows;



            data.rows.push_back ({ std::nullopt, began });
            data.rows.push_back ({ std::nullopt, restarted });
            rows = CallStackPane::GetRows (data);

            Assert::AreEqual (std::string ("historyBegan"),      std::string (CallStack::GetBreakKindName (CallBreakKind::HistoryBegan)));
            Assert::AreEqual (std::string ("trackingRestarted"), std::string (CallStack::GetBreakKindName (CallBreakKind::TrackingRestarted)));

            Assert::AreEqual (std::string ("Calls before history starts at $0803 are not available."), CallStack::DescribeBreak (began));
            Assert::AreEqual (std::string ("Calls before $0905 are not available."),                   CallStack::DescribeBreak (restarted));

            Assert::AreEqual ((size_t) 2, rows.size());
            Assert::IsTrue   (rows[0].isNote && rows[1].isNote, L"each a note across the row");
            Assert::AreEqual (std::wstring (L"Calls before history starts at $0803 are not available."), rows[0].routine);
            Assert::AreEqual (std::wstring (L"Calls before $0905 are not available."),                   rows[1].routine);

            Assert::AreEqual (std::wstring (L"Rebuilding earlier calls from history (25%)"), CallStackPane::GetRebuildingNote (0.25f));
        }


        //  Hybrid walks the stack below where history starts, as it does
        //  below where recording began: the calls made before it may still
        //  be on the stack.
        TEST_METHOD (TheWalkGoesOnBelowWhereHistoryStarts)
        {
            std::vector<Byte>          memory (0x10000, 0);
            CallRecord                 record;
            CallStackRecorder::Break   began;
            CallStackData              data;
            auto                       peek   = [&memory] (Word address) { return memory[address]; };



            memory[0x0900] = 0x20;              //  JSR $0A00 at $0900, its return address on the stack
            memory[0x0901] = 0x00;
            memory[0x0902] = 0x0A;
            memory[0x01FE] = 0x02;
            memory[0x01FF] = 0x09;

            began.info     = CallStackBreak { CallBreakKind::HistoryBegan, 0x0A00, 0 };
            record.isActive = true;
            record.breaks.push_back (began);

            data = CallStack::Build (CallStackMechanism::Hybrid, record, 0xFD, peek, nullptr);

            Assert::AreEqual ((size_t) 2, data.rows.size(), L"where history starts, then what the walk found");
            Assert::IsTrue   (data.rows[0].chainBreak.has_value() && data.rows[0].chainBreak->kind == CallBreakKind::HistoryBegan);
            Assert::IsTrue   (data.rows[1].frame.has_value() && data.rows[1].frame->provenance == CallProvenance::Guessed, L"found on the stack");
            Assert::IsFalse  (data.rows[1].frame->isVerified, L"and unverified, below the break");
        }


        //  A Ctrl+Reset in history before the debugger opened ends every
        //  call made before it, as it does live: the rebuilt record is the
        //  one kept from power-on through the reset, its bottom the reset,
        //  with the calls the program made again, from the warm-start
        //  vector, above it.
        TEST_METHOD (AResetInHistoryEndsTheCallsBeforeIt)
        {
            HistoryRig  rig;
            RecordRig   reference;
            CallRecord  expected;



            RunTo (rig.base.machine,       kResetAt);
            RunTo (reference.base.machine, kResetAt);

            Reset (rig.base,       false);
            Reset (reference.base, false);

            RunTo (rig.base.machine, kAfterReset);
            rig.Attach();
            rig.Drain();

            RunTo (reference.base.machine, kAfterReset);
            expected = GetRecord (reference.base.session);

            Assert::IsTrue (HasBreak  (expected, CallBreakKind::Reset, 0), L"the record begins at the reset");
            Assert::IsTrue (HasTarget (expected, kMiddle),                L"with the calls the program made again after it");

            AssertSameRecord (expected, GetRecord (rig.base.session));
        }


        //  A power cycle in history leaves nothing of the record before it,
        //  so the rebuild starts at the last keyframe before it rather than
        //  at the oldest, and the record it gives dates from the power
        //  cycle, as the one kept from power-on does.
        TEST_METHOD (APowerCycleInHistoryStartsTheRecordAgain)
        {
            uint64_t             attachAt = kFarAhead + kRunOn;
            HistoryRig           rig;
            RecordRig            reference;
            CallStackRebuildJob  job;
            CallRecord           expected;
            bool                 hasJob   = false;
            HRESULT              hr       = S_OK;



            RunTo (rig.base.machine,       kFarAhead);
            RunTo (reference.base.machine, kFarAhead);

            Reset (rig.base,       true);
            Reset (reference.base, true);

            RunTo (rig.base.machine, attachAt);

            hr = rig.history.MakeJob (false, 0, 0, attachAt, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");

            Assert::IsTrue (hasJob, L"history holds the run");
            Assert::IsTrue (job.parts.front().startPosition > rig.controller.GetOldestPosition(), L"not from the oldest keyframe");
            Assert::IsTrue (job.parts.front().startPosition <= kFarAhead,                      L"but from one before the power cycle");

            rig.Attach();
            rig.Drain();

            RunTo (reference.base.machine, attachAt);
            expected = GetRecord (reference.base.session);

            Assert::IsTrue (HasBreak (expected, CallBreakKind::PowerOn, 0), L"the record begins at the power cycle");

            AssertSameRecord (expected, GetRecord (rig.base.session));
        }


        //  A reverse command that left the machine where it stood, and
        //  replayed nothing on it -- a step back out with no caller, live --
        //  fed the record nothing: it goes on, with no rebuild.
        TEST_METHOD (AReverseCommandThatStaysPutKeepsTheRecord)
        {
            HistoryRig     rig;
            ReverseResult  result;
            CallRecord     before;
            uint64_t       generation = 0;
            HRESULT        hr         = S_OK;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();
            rig.Drain();

            rig.controller.SetCallerProbe ([] (uint64_t) { return true; });

            generation = rig.base.session.GetCallRecordGeneration();
            before     = GetRecord (rig.base.session);

            rig.history.OnMoving();

            hr = rig.controller.StepBackOut (result);
            AssertSucceeded (hr, L"StepBackOut");

            rig.history.OnMoved (false);
            rig.history.Service();

            Assert::IsTrue   (result.outcome == ReverseOutcome::NoCaller,                  L"no caller: the machine stays");
            Assert::AreEqual (generation, rig.base.session.GetCallRecordGeneration(),     L"the record did not start again");
            Assert::IsFalse  (rig.history.IsRebuilding(),                                  L"and nothing is rebuilt");
            Assert::AreEqual ((size_t) 0, rig.queue.GetPendingCount(),                     L"nothing on the worker");

            AssertSameRecord (before, GetRecord (rig.base.session));
        }


        //  A command that replays on the machine and puts it back where it
        //  stood -- the trace's instructions gathered behind live -- fed the
        //  record that replay, so it starts again although the machine did
        //  not move.
        TEST_METHOD (AReverseCommandThatReplaysInPlaceStartsTheRecordAgain)
        {
            HistoryRig     rig;
            ReverseResult  result;
            uint64_t       landing    = 0;
            uint64_t       generation = 0;
            uint64_t       replayed   = 0;
            HRESULT        hr         = S_OK;



            RunTo (rig.base.machine, kFarAhead);
            rig.Attach();
            rig.Drain();

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (kAttachAt, result);
            AssertSucceeded (hr, L"SeekToPosition");

            rig.history.OnMoved (false);
            rig.Drain();

            landing    = rig.base.machine.GetPosition();
            generation = rig.base.session.GetCallRecordGeneration();
            replayed   = rig.controller.GetReplayer().GetReplayedCount();

            rig.history.OnMoving();

            hr = rig.controller.PrepareRecentSteps (result);
            AssertSucceeded (hr, L"PrepareRecentSteps");

            rig.history.OnMoved (false);

            Assert::AreEqual (landing, rig.base.machine.GetPosition(),                  L"the machine is put back");
            Assert::IsTrue   (rig.controller.GetReplayer().GetReplayedCount() > replayed, L"after a replay");
            Assert::AreNotEqual (generation, rig.base.session.GetCallRecordGeneration(), L"so the record starts again");
        }


        //  A stretch run at Maximum speed is a gap in history, and the record
        //  starts again after it. The rebuild replays nothing before the gap:
        //  its one part starts where recording resumed.
        TEST_METHOD (AGapInHistoryStartsTheRecordAfterIt)
        {
            HistoryRig                     rig;
            RecordRig                      reference (kGapEnd);
            CallStackRebuildJob            job;
            std::optional<CallStackBreak>  bottom;
            bool                           hasJob    = false;
            HRESULT                        hr        = S_OK;



            RunTo (rig.base.machine, kGapStart);

            hr = rig.controller.SetUserMaximumSpeed (true);
            AssertSucceeded (hr, L"SetUserMaximumSpeed on");

            RunTo (rig.base.machine, kGapEnd);

            hr = rig.controller.SetUserMaximumSpeed (false);
            AssertSucceeded (hr, L"SetUserMaximumSpeed off");

            RunTo (rig.base.machine, kAttachAt);

            hr = rig.history.MakeJob (false, 0, 0, kAttachAt, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");

            Assert::IsTrue   (hasJob, L"history holds the run");
            Assert::AreEqual ((size_t) 1, job.parts.size(), L"nothing before the gap is replayed");
            Assert::AreEqual (kGapEnd, job.parts[0].startPosition, L"from where recording resumed");
            Assert::IsTrue   (IsAfterGap (rig, job.parts[0].startPosition), L"the one part follows the gap");

            rig.Attach();
            rig.Drain();

            RunTo (reference.base.machine, kAttachAt);
            bottom = rig.base.session.GetCallRecordBottom();

            Assert::IsTrue (bottom.has_value() && bottom->kind == CallBreakKind::HistoryBegan, L"the record begins after the gap");

            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session), true);
        }


        //  A disk put in drive 2 and taken out again leaves the bays as they
        //  were, but the stretch between was saved with other disks, so the
        //  record starts again where the disk came out, and the rebuild
        //  replays nothing before that.
        TEST_METHOD (ARebuildStartsWhereTheBaysWereLastAsTheyAreNow)
        {
            HistoryRig              rig       (0, true);
            RecordRig               reference (kSwapBack);
            CallStackRebuildJob     job;
            CallStackRebuildResult  result;
            bool                    hasJob    = false;
            HRESULT                 hr        = S_OK;



            RunTo     (rig.base.machine, kSwap);
            MountDisk (rig.base.machine, "second.dsk", kSecondDrive);
            RunTo     (rig.base.machine, kSwapBack);
            EjectDisk (rig.base.machine, kSecondDrive);
            RunTo     (rig.base.machine, kAttachAt);

            hr = rig.history.MakeJob (false, 0, 0, kAttachAt, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");

            Assert::IsTrue   (hasJob, L"history holds the run");
            Assert::AreEqual ((size_t) 3, job.parts.size(), L"the oldest keyframe, then a part for each change of disk");

            job.generation = 1;

            hr = rig.replayer.Rebuild (job, result);
            AssertSucceeded (hr, L"Rebuild");

            Assert::AreEqual (kSwapBack, result.recordFrom, L"the record starts where the disk came out");
            Assert::IsTrue   (result.instructions <= kAttachAt - kSwapBack, L"and nothing before that was replayed");

            RunTo (reference.base.machine, kAttachAt);
            AssertSameRecord (GetRecord (reference.base.session), result.record, true);
        }


        //  A rebuild continued on the worker shows the share of the whole
        //  rebuild done, the first round counted: neither all of it, while
        //  the rest is replayed, nor only the round in flight.
        TEST_METHOD (AContinuedRebuildShowsTheShareDone)
        {
            HistoryRig             rig;
            std::optional<float>   progress;
            float                  share    = (float) kAttachAt / (float) kFarAhead;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            RunTo (rig.base.machine, kFarAhead);
            rig.queue.WaitAll();
            rig.history.Service();

            progress = rig.base.session.GetCallStack().rebuildProgress;

            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(), L"continued on the worker");
            Assert::IsTrue   (progress.has_value(), L"its progress is shown");
            Assert::AreEqual (share, *progress, kProgressTolerance, L"the first round's share of the whole");

            rig.Drain();
        }


        //  A rebuilt record is taken only for the machine as it stands: a
        //  second machine that stopped on any other cycle count or register
        //  replayed some other run, and the live record stays. Begun where
        //  the debugger opened, its bottom still marks that; begun again after
        //  a move, its bottom no longer claims that history starts there.
        TEST_METHOD (ARebuildThatStoppedOnAnotherStateIsNotTaken)
        {
            static const std::vector<std::pair<const wchar_t *, std::function<void (CallStackRebuildResult &)>>>  kChanges =
            {
                { L"cycle", [] (CallStackRebuildResult & r) { r.cycle++; } },
                { L"PC",    [] (CallStackRebuildResult & r) { r.registers.pc++; } },
                { L"SP",    [] (CallStackRebuildResult & r) { r.registers.sp--; } },
                { L"A",     [] (CallStackRebuildResult & r) { r.registers.a ^= 1; } },
                { L"X",     [] (CallStackRebuildResult & r) { r.registers.x ^= 1; } },
                { L"Y",     [] (CallStackRebuildResult & r) { r.registers.y ^= 1; } },
                { L"P",     [] (CallStackRebuildResult & r) { r.registers.p ^= 1; } },
            };

            HandFedRebuilder               rebuilder;
            HistoryRig                     rig;
            CallStackRebuildResult         result;
            std::optional<CallStackBreak>  bottom;
            CallBreakKind                  left = CallBreakKind::TrackingBegan;



            RunTo (rig.base.machine, kAttachAt);

            rig.history.Attach (&rig.controller, &rebuilder);
            rig.Attach();

            for (const auto & [name, change] : kChanges)
            {
                result = MakeHandFedResult (rig, rebuilder);
                change (result);

                rebuilder.results.push_back (result);
                rig.history.Service();

                bottom = rig.base.session.GetCallRecordBottom();

                Assert::IsFalse (rig.history.IsRebuilding(), std::format (L"{}: the result was taken in", name).c_str());
                Assert::IsTrue  (bottom.has_value() && bottom->kind == left, std::format (L"{}: and its record left, its bottom marking what it holds", name).c_str());

                rig.base.session.RestartCallRecording();
                rig.history.Service();

                left = CallBreakKind::TrackingRestarted;
            }

            rebuilder.results.push_back (MakeHandFedResult (rig, rebuilder));
            rig.history.Service();

            bottom = rig.base.session.GetCallRecordBottom();

            Assert::IsTrue (bottom.has_value() && bottom->kind == CallBreakKind::PowerOn, L"the same state's record is taken");
        }


        //  A move to where history starts leaves nothing to rebuild: the
        //  record starts again there, and its bottom marks that history starts
        //  there, not that the debugger opened there.
        TEST_METHOD (AMoveToTheStartOfHistoryMarksWhereHistoryStarts)
        {
            HistoryRig                       rig       (kHistoryStart);
            RecordRig                        reference (kHistoryStart);
            ReverseResult                    result;
            std::optional<CallStackBreak>    bottom;
            std::vector<CallStackPane::Row>  rows;
            HRESULT                          hr        = S_OK;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();
            rig.Drain();

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (rig.controller.GetOldestPosition(), result);
            AssertSucceeded (hr, L"SeekToPosition");

            rig.history.OnMoved (false);
            rig.history.Service();

            bottom = rig.base.session.GetCallRecordBottom();
            rows   = CallStackPane::GetRows (rig.base.session.GetCallStack());

            Assert::AreEqual (kHistoryStart, rig.base.machine.GetPosition(), L"at the start of history");
            Assert::IsFalse  (rig.history.IsRebuilding(), L"nothing to rebuild");
            Assert::IsTrue   (bottom.has_value() && bottom->kind == CallBreakKind::HistoryBegan, L"the record begins where history does");

            Assert::IsTrue (std::ranges::any_of (rows, [&bottom] (const CallStackPane::Row & row)
            {
                return row.isNote && row.routine == CallStackPane::GetHistoryStartNote (bottom->pc);
            }), L"the pane's note: the earlier calls are unknown");

            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session), true);
        }


        //  Keys and paddle moves recorded in history reach the second machine
        //  at the instructions that read them, across a rebuild continued on
        //  the worker. The reverse rig's program times the paddle, so a move
        //  applied anywhere else changes when it makes its next call, and the
        //  cycle the record dates that call by.
        TEST_METHOD (RecordedInputsReachTheRebuiltRecord)
        {
            static constexpr size_t  kAttachSlices = 20;
            static constexpr size_t  kRunOnSlices  = 10;    // more than a catch-up's worth
            static constexpr size_t  kLastSlices   = 3;     // within one, after a round on the worker

            MachineRig           rig        ("Apple2e", TestMachine::Slots::AsShipped);
            MachineRig           reference  ("Apple2e", TestMachine::Slots::AsShipped);
            ReverseController    controller (rig.machine);
            InlineWorkQueue      queue;
            ScratchCallReplayer  replayer;
            CallStackHistory     history    (rig.machine, rig.session);
            CallRecord           expected;
            size_t               slice      = 0;
            int                  rounds     = 0;
            HRESULT              hr         = S_OK;



            ReverseSessionRig::Prepare (rig.machine);
            ReverseSessionRig::Prepare (reference.machine);
            reference.session.SetCallRecording (true);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            replayer.SetWorkQueue (&queue);
            replayer.SetMachine   (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
            history.Attach        (&controller, &replayer);

            for (; slice < kAttachSlices; slice++)
            {
                FeedSlice (rig.machine, reference.machine, slice);
            }

            rig.session.SetCallRecording (true);
            history.Service();

            for (; slice < kAttachSlices + kRunOnSlices; slice++)
            {
                FeedSlice (rig.machine, reference.machine, slice);
            }

            queue.WaitAll();
            history.Service();

            Assert::AreEqual ((size_t) 1, queue.GetPendingCount(), L"continued on the worker");

            for (; slice < kAttachSlices + kRunOnSlices + kLastSlices; slice++)
            {
                FeedSlice (rig.machine, reference.machine, slice);
            }

            //  Inside the routine the counter calls, its call on the record.
            while (rig.machine.GetCpu()->GetPC() != ReverseSessionRig::kRoutinePla)
            {
                rig.machine.StepOne();
            }

            RunTo (reference.machine, rig.machine.GetPosition());

            do
            {
                queue.WaitAll();
                history.Service();
            }
            while (history.IsRebuilding() && ++rounds < kDrainLimit);

            expected = GetRecord (reference.session);

            Assert::IsFalse (history.IsRebuilding(), L"the rebuild finished");
            Assert::IsTrue  (HasTarget (expected, ReverseSessionRig::kWriteRoutine), L"the stop is inside the call the paddle timed");

            AssertSameRecord (expected, GetRecord (rig.session));

            history.Attach (nullptr, nullptr);
        }


        //  A step back after a long run of history replays from the newest
        //  keyframe at or before where it lands, with the copy of the record
        //  kept there as that keyframe was taken, and no further: one
        //  keyframe span at most. The record it gives is the one a rebuild
        //  of the whole history gives, and the one kept from power-on.
        TEST_METHOD (AStepBackAfterALongHistoryReplaysOneKeyframeSpanAtMost)
        {
            static constexpr uint64_t  kLongRun = kFarAhead * 2;

            HistoryRig              rig;
            HistoryRig              whole;
            RecordRig               reference;
            ReverseResult           result;
            CallStackRebuildJob     job;
            CallStackRebuildResult  rebuilt;
            uint64_t                landing  = 0;
            uint64_t                keyframe = 0;
            bool                    hasJob   = false;
            HRESULT                 hr       = S_OK;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();
            rig.Drain();

            RunTo (rig.base.machine, kLongRun);

            Assert::IsTrue (rig.history.GetCopies().GetCount() > 0, L"copies of the record are kept as keyframes are taken");
            Assert::IsTrue (AreCopiesAtKeyframes (rig),               L"each at a keyframe history holds");

            rig.history.OnMoving();

            hr = rig.controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");

            rig.history.OnMoved (false);

            landing  = rig.base.machine.GetPosition();
            keyframe = GetKeyframeAtOrBefore (rig, landing);

            Assert::AreEqual (kLongRun - 1, landing, L"one instruction back");
            Assert::IsTrue   (keyframe > kAttachAt, L"long after the record began");

            hr = rig.history.MakeJob (false, 0, 0, landing, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");

            Assert::IsTrue   (hasJob, L"history holds the run");
            Assert::AreEqual (keyframe, job.parts.front().startPosition, L"the rebuild starts at the newest keyframe at or before the landing");
            Assert::IsFalse  (job.parts.front().seed.empty(),            L"from the copy of the record kept there");

            job.generation = 1;

            hr = rig.replayer.Rebuild (job, rebuilt);
            AssertSucceeded (hr, L"Rebuild");

            Assert::AreEqual (landing - keyframe, rebuilt.instructions, L"it replays that one stretch and no more");

            rig.Drain();

            RunTo (whole.base.machine, landing);
            whole.Attach();
            whole.Drain();

            RunTo (reference.base.machine, landing);

            AssertSameRecord (GetRecord (whole.base.session),     rebuilt.record);
            AssertSameRecord (GetRecord (whole.base.session),     GetRecord (rig.base.session));
            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  The rebuild made when the debugger opened kept a copy of the
        //  record at each keyframe it replayed past, so a move back into that
        //  stretch starts from the newest of them at or before where it
        //  lands, and gives the record kept from power-on.
        TEST_METHOD (AMoveBackStartsFromACopyTheFirstRebuildMade)
        {
            uint64_t             landing  = FindPcAfter (kFarAhead / 2, kHandler);
            HistoryRig           rig;
            RecordRig            reference;
            ReverseResult        result;
            CallStackRebuildJob  job;
            uint64_t             keyframe = 0;
            bool                 hasJob   = false;
            HRESULT              hr       = S_OK;



            RunTo (rig.base.machine, kFarAhead);
            rig.Attach();
            rig.Drain();

            Assert::IsTrue (rig.history.GetCopies().GetCount() > 0, L"the rebuild kept copies of the record");
            Assert::IsTrue (AreCopiesAtKeyframes (rig),               L"each at a keyframe history holds");

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (landing, result);
            AssertSucceeded (hr, L"SeekToPosition");

            rig.history.OnMoved (false);

            keyframe = GetKeyframeAtOrBefore (rig, landing);

            hr = rig.history.MakeJob (false, 0, 0, landing, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");

            Assert::IsTrue   (hasJob, L"history holds the run");
            Assert::IsTrue   (keyframe > 0, L"a keyframe after the oldest, or the test proves nothing");
            Assert::AreEqual (keyframe, job.parts.front().startPosition, L"the rebuild starts at the newest keyframe at or before the landing");
            Assert::IsFalse  (job.parts.front().seed.empty(),            L"from the copy of the record kept there");

            rig.Drain();

            RunTo (reference.base.machine, landing);

            Assert::IsTrue (HasKind (GetRecord (reference.base.session), CallFrameKind::Irq), L"the landing is inside the interrupt");

            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session));
        }


        //  Keyframes every frame, two to a group, in a budget a few seconds of
        //  machine time fill.
        static ReverseSettings MakeSmallSettings()
        {
            static constexpr uint32_t  kGroupKeyframes = 2;
            static constexpr size_t    kBudgetBytes    = 512 * 1024;

            ReverseSettings  settings;



            settings.keyframes.intervalCycles = KeyframeSettings::kFrameCycles;
            settings.keyframes.wholeEvery     = kGroupKeyframes;
            settings.keyframes.longestGroup   = kGroupKeyframes;
            settings.keyframes.budgetBytes    = kBudgetBytes;

            return settings;
        }


        //  A copy of the record goes when history drops its keyframe: the
        //  oldest, as history keeps to its budget; those after a change made
        //  in the past; and every one when history stops.
        TEST_METHOD (CopiesGoWithTheirKeyframes)
        {
            static constexpr uint64_t  kRunLimit = kFarAhead * 10;

            HistoryRig     rig      (0, false, MakeSmallSettings());
            ReverseResult  result;
            uint64_t       middle   = 0;
            uint64_t       newest   = 0;
            bool           isFound  = false;
            HRESULT        hr       = S_OK;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();
            rig.Drain();

            while (rig.controller.GetOldestPosition() == 0 && rig.base.machine.GetPosition() < kRunLimit)
            {
                rig.base.machine.RunCycles (KeyframeSettings::kFrameCycles);
            }

            Assert::IsTrue (rig.controller.GetOldestPosition() > 0,  L"history let its oldest keyframes go");
            Assert::IsTrue (rig.history.GetCopies().GetCount() > 0,  L"copies of the record are kept");
            Assert::IsTrue (AreCopiesAtKeyframes (rig),               L"none where history no longer holds a keyframe");
            Assert::IsTrue (rig.history.GetCopies().GetCount() <= rig.controller.GetKeyframes().GetCount(), L"at most one a keyframe");

            middle = (rig.controller.GetOldestPosition() + rig.base.machine.GetPosition()) / 2;

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (middle, result);
            AssertSucceeded (hr, L"SeekToPosition");

            rig.history.OnMoved (false);
            rig.Drain();

            hr = rig.controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            isFound = rig.history.GetCopies().TryFindAtOrBefore (UINT64_MAX, newest);

            Assert::IsTrue (isFound && newest <= middle, L"a change in the past drops the copies after it");
            Assert::IsTrue (AreCopiesAtKeyframes (rig),   L"with their keyframes");

            rig.controller.Stop();

            Assert::AreEqual ((size_t) 0, rig.history.GetCopies().GetCount(), L"history stopped, no copy is left");
        }


        //  A rebuild's copies wait for its record to be taken, and by then
        //  history may have let go of keyframes they were made at: those are
        //  not kept, and every copy kept is at a keyframe history holds.
        TEST_METHOD (CopiesOfKeyframesDroppedDuringTheRebuildAreNotKept)
        {
            static constexpr uint64_t  kRunLimit = kFarAhead * 10;

            HistoryRig  rig (0, false, MakeSmallSettings());



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();

            Assert::IsTrue (rig.history.IsRebuilding(), L"the rebuild waits on the worker");

            while (rig.controller.GetOldestPosition() == 0 && rig.base.machine.GetPosition() < kRunLimit)
            {
                rig.base.machine.RunCycles (KeyframeSettings::kFrameCycles);
            }

            Assert::IsTrue (rig.controller.GetOldestPosition() > 0, L"history let keyframes the rebuild replays past go");

            rig.Drain();

            Assert::IsTrue (rig.history.GetCopies().GetCount() > 0, L"the rebuild's copies were kept");
            Assert::IsTrue (AreCopiesAtKeyframes (rig),               L"but none at a keyframe history let go");
        }


        //  What the copies of the record cost: the bytes each uses, for the
        //  program's record of three to five calls and a TXS, and those for
        //  every keyframe the default budget's table holds, which must fit
        //  the share of the budget the copies may hold.
        TEST_METHOD (TheCopiesFitTheirShareOfTheDefaultBudget)
        {
            static constexpr uint64_t  kLongRun = kFarAhead * 4;

            HistoryRig         rig;
            std::vector<Byte>  packed;
            size_t             count     = 0;
            size_t             perCopy   = 0;
            size_t             keyframes = 0;
            size_t             total     = 0;
            size_t             share     = KeyframeSettings::kDefaultBudgetBytes / CallStackHistory::kCopyBudgetParts;



            RunTo (rig.base.machine, kAttachAt);
            rig.Attach();
            rig.Drain();

            RunTo (rig.base.machine, kLongRun);

            count     = rig.history.GetCopies().GetCount();
            keyframes = rig.controller.GetKeyframes().GetCapacity();

            Assert::IsTrue (count > 0, L"copies of the record are kept");

            perCopy = rig.history.GetCopies().GetUsedByteCount() / count;
            total   = perCopy * keyframes;

            CallRecordCopies::Pack (GetRecord (rig.base.session), packed);

            Logger::WriteMessage (std::format ("{} copies of the record, {} bytes each in use, the record as it stands {} bytes packed; "
                                               "{} bytes for the {} keyframes the default budget's table holds, of a share of {} bytes\n",
                                               count, perCopy, packed.size(), total, keyframes, share).c_str());

            Assert::IsTrue (total <= share, L"the copies for a whole default history fit their share of the budget");
        }


        //  The machine ran live through a stretch at Maximum speed while the
        //  record begun again after a move was being rebuilt. What ran in
        //  the gap is not in history, so the rebuild cannot be continued past
        //  it and no job is made; the live record, which ran through the gap,
        //  stays, and its bottom marks only that the calls before it are not
        //  available, not that history starts there.
        TEST_METHOD (AGapWhileTheRebuildRunsLeavesTheLiveRecord)
        {
            static constexpr uint64_t  kGapLength = 5000;

            HistoryRig                       rig;
            ReverseResult                    result;
            std::optional<CallStackBreak>    bottom;
            std::vector<CallStackPane::Row>  rows;
            Word                             pc       = 0;
            uint64_t                         gapEnd   = 0;
            HRESULT                          hr       = S_OK;



            RunTo (rig.base.machine, kFarAhead);
            rig.Attach();
            rig.Drain();

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (kAttachAt, result);
            AssertSucceeded (hr, L"SeekToPosition back");

            rig.history.OnMoved (false);
            rig.Drain();

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (rig.controller.GetLiveEndPosition(), result);
            AssertSucceeded (hr, L"SeekToPosition to the end");

            rig.history.OnMoved (false);
            rig.history.Service();

            pc = rig.base.machine.GetCpu()->GetPC();

            Assert::IsFalse (rig.controller.IsInHistory(), L"live again");
            Assert::IsTrue  (rig.history.IsRebuilding(),   L"its record being rebuilt");

            hr = rig.controller.SetUserMaximumSpeed (true);
            AssertSucceeded (hr, L"SetUserMaximumSpeed on");

            RunTo (rig.base.machine, kFarAhead + kGapLength);

            hr = rig.controller.SetUserMaximumSpeed (false);
            AssertSucceeded (hr, L"SetUserMaximumSpeed off");

            gapEnd = rig.base.machine.GetPosition();

            RunTo (rig.base.machine, gapEnd + kRunOn);

            Assert::IsTrue (IsAfterGap (rig, gapEnd), L"history holds the gap");

            rig.queue.WaitAll();
            rig.history.Service();

            bottom = rig.base.session.GetCallRecordBottom();
            rows   = CallStackPane::GetRows (rig.base.session.GetCallStack());

            Assert::IsFalse (rig.history.IsRebuilding(), L"the rebuild is given up");
            Assert::IsTrue  (bottom.has_value() && bottom->kind == CallBreakKind::TrackingRestarted && bottom->pc == pc,
                             L"the live record stays, from where it began");

            Assert::IsTrue (std::ranges::any_of (rows, [pc] (const CallStackPane::Row & row)
            {
                return row.isNote && row.routine == CallStackPane::GetUnavailableNote (pc);
            }), L"the pane's note: the calls before it are not available");
        }


        //  Running on from the past across a gap loads the keyframe after
        //  it under the running machine, and the record, fed across the load,
        //  starts again and is rebuilt: it begins after the gap, as one kept
        //  from there does.
        TEST_METHOD (RunningOnFromThePastAcrossAGapStartsTheRecordAgain)
        {
            uint64_t                       stop       = kGapEnd + kRunOn;
            HistoryRig                     rig;
            RecordRig                      reference  (kGapEnd);
            ReverseResult                  result;
            std::optional<CallStackBreak>  bottom;
            uint64_t                       generation = 0;
            HRESULT                        hr         = S_OK;



            RunTo (rig.base.machine, kGapStart);

            hr = rig.controller.SetUserMaximumSpeed (true);
            AssertSucceeded (hr, L"SetUserMaximumSpeed on");

            RunTo (rig.base.machine, kGapEnd);

            hr = rig.controller.SetUserMaximumSpeed (false);
            AssertSucceeded (hr, L"SetUserMaximumSpeed off");

            RunTo (rig.base.machine, kFarAhead);
            rig.Attach();
            rig.Drain();

            rig.history.OnMoving();

            hr = rig.controller.SeekToPosition (kGapStart / 2, result);
            AssertSucceeded (hr, L"SeekToPosition");

            rig.history.OnMoved (false);
            rig.Drain();

            generation = rig.base.session.GetCallRecordGeneration();

            RunTo (rig.base.machine, stop);

            Assert::IsTrue (rig.controller.IsInHistory(), L"still behind live");

            rig.Drain();

            bottom = rig.base.session.GetCallRecordBottom();

            Assert::AreNotEqual (generation, rig.base.session.GetCallRecordGeneration(), L"the record started again");
            Assert::IsTrue      (bottom.has_value() && bottom->kind == CallBreakKind::HistoryBegan, L"and begins after the gap");

            RunTo (reference.base.machine, stop);
            AssertSameRecord (GetRecord (reference.base.session), GetRecord (rig.base.session), true);
        }


        //  The progress a job published shows only while that job is the
        //  newest: once it is cancelled, or a newer one is submitted, it shows
        //  none, whatever the job publishes after.
        TEST_METHOD (ACancelledJobShowsNoProgress)
        {
            HistoryRig           rig;
            CallStackRebuildJob  job;
            bool                 hasJob = false;
            HRESULT              hr     = S_OK;



            RunTo (rig.base.machine, kAttachAt);

            hr = rig.history.MakeJob (false, 0, 0, kAttachAt, job, hasJob);
            AssertSucceeded (hr, L"MakeJob");
            Assert::IsTrue  (hasJob, L"history holds the run");

            job.generation = 1;

            hr = rig.replayer.Submit (std::make_shared<CallStackRebuildJob> (job));
            AssertSucceeded (hr, L"Submit");

            Assert::AreEqual (0.0f, rig.replayer.GetProgress(), L"a job waiting shows none done");

            rig.queue.WaitAll();

            Assert::AreEqual (1.0f, rig.replayer.GetProgress(), L"finished, it shows all of it done");

            rig.replayer.Cancel();

            Assert::AreEqual (0.0f, rig.replayer.GetProgress(), L"cancelled, it shows none");
        }
    };
}