#include "Pch.h"

#include "BreakpointRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace BreakpointRigs;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchpointCommandTests
//
//  BPM, BPMR, BPMW, BPIO and BPA, and their GSSquared and WinDbg forms: each
//  watchpoint stopping a real run after the access, or before it in BEFORE
//  mode, with the value, the byte replaced and the instruction that made the
//  access; the page each puts on the real bus's watch mask; IF with ACCESS
//  and VALUE; watch hits ending steps; the accesses a real CPU makes that a
//  watchpoint sees; and every error.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (WatchpointCommandTests)
    {
    public:

        //  0300 NOP / 0301 LDA $0400 / 0304 INX / 0305 JMP $0305
        //  A read of $0400 that is not the run's first instruction.
        static void LoadRead (IDebugTarget & target)
        {
            Load (target, 0x0300, { 0xEA, 0xAD, 0x00, 0x04, 0xE8, 0x4C, 0x05, 0x03 }, 0x0300);
            Assert::IsTrue (target.TryPoke (0x0400, 0xA0));
        }

        //  0300 LDA #$41 / 0302 LDX #$10 / 0304 STA $0400,X / 0307 JMP $0307
        static void LoadIndexedStore (IDebugTarget & target)
        {
            Load (target, 0x0300, { 0xA9, 0x41, 0xA2, 0x10, 0x9D, 0x00, 0x04, 0x4C, 0x07, 0x03 }, 0x0300);
            Assert::IsTrue (target.TryPoke (0x0410, 0xA0));
        }

        //  0300 NOP / 0301 LDA $C000 / 0304 JMP $0304
        static void LoadIoRead (IDebugTarget & target)
        {
            Load (target, 0x0300, { 0xEA, 0xAD, 0x00, 0xC0, 0x4C, 0x04, 0x03 }, 0x0300);
        }

        struct Form
        {
            CommandMode   mode;
            const char  * line;
            const char  * reply;
            const char  * go;
            bool          isRead;
        };

        static constexpr const char * kGss       = "Breakpoint id=0 set";
        static constexpr const char * kWriteStop = "Watchpoint #0: Write $41 to $0400 by $0302 (was $A0)";
        static constexpr const char * kReadStop  = "Watchpoint #0: Read $A0 from $0400 by $0301";



        ////////////////////////////////////////////////////////////////////////
        //
        //  Forms, and what setting one does
        //
        ////////////////////////////////////////////////////////////////////////

        //  Every form, in every dialect, sets a watchpoint that stops a run
        //  after the access: a store's stop gives the byte written and the one
        //  it replaced, a load's the byte read, and both the instruction that
        //  made the access, with PC past it.
        TEST_METHOD (EveryForm_InEveryDialect_SetsAWatchpointThatStopsARun)
        {
            static constexpr Form  kForms[] =
            {
                { CommandMode::AppleWin,  "BPMW 400",        "Breakpoint #0 set on write of $0400",         "G", false },
                { CommandMode::AppleWin,  "bpmw $400",       "Breakpoint #0 set on write of $0400",         "G", false },
                { CommandMode::AppleWin,  "BPMW 400 AFTER",  "Breakpoint #0 set on write of $0400",         "G", false },
                { CommandMode::AppleWin,  "BPMW 400 after",  "Breakpoint #0 set on write of $0400",         "G", false },
                { CommandMode::AppleWin,  "BPMR 400",        "Breakpoint #0 set on read of $0400",          "G", true  },
                { CommandMode::AppleWin,  "BPM 400",         "Breakpoint #0 set on read or write of $0400", "G", false },
                { CommandMode::AppleWin,  "BPM 400",         "Breakpoint #0 set on read or write of $0400", "G", true  },
                { CommandMode::AppleWin,  "BPIO 400",        "Breakpoint #0 set on read or write of $0400", "G", false },
                { CommandMode::Casso,     "BPMW 400",        "Breakpoint #0 set on write of $0400",         "G", false },
                { CommandMode::Casso,     "BPMR 400",        "Breakpoint #0 set on read of $0400",          "G", true  },
                { CommandMode::Monitor,   "/BPMW 400",       "Breakpoint #0 set on write of $0400",         "G", false },
                { CommandMode::Monitor,   "/BPMR 400",       "Breakpoint #0 set on read of $0400",          "G", true  },
                { CommandMode::GSSquared, "bpd 400 w",       kGss,                                          "g", false },
                { CommandMode::GSSquared, "bpd 400 r",       kGss,                                          "g", true  },
                { CommandMode::GSSquared, "bpd 400 rw",      kGss,                                          "g", false },
                { CommandMode::GSSquared, "BPD 00/400 W",    kGss,                                          "g", false },
                { CommandMode::GSSquared, "BPMW 400",        kGss,                                          "g", false },
                { CommandMode::WinDbg,    "ba w1 400",       "Breakpoint #0 set on write of $0400",         "g", false },
                { CommandMode::WinDbg,    "ba r1 400",       "Breakpoint #0 set on read of $0400",          "g", true  },
                { CommandMode::WinDbg,    "ba w1 0x400",     "Breakpoint #0 set on write of $0400",         "g", false },
                { CommandMode::WinDbg,    "!BPMW 400",       "Breakpoint #0 set on write of $0400",         "g", false },
                { CommandMode::WinDbg,    "!BPM 400",        "Breakpoint #0 set on read or write of $0400", "g", true  },
            };



            for (const Form & form : kForms)
            {
                BatchPathRig  rig;
                std::string   where = std::string (CommandModeNames::GetName (form.mode)) + ": " + form.line;



                if (form.isRead)
                {
                    LoadRead (rig.target);
                }
                else
                {
                    Programs::LoadLine (rig.target);
                }

                rig.EnterMode (form.mode);
                AssertLines ({ form.reply }, rig.RunOk (form.line), where);

                if (form.isRead)
                {
                    AssertLines (GetStopLines (form.mode, kReadStop, { 0x0304, 0xA0, 0x00, 0x00, 0xFF, 0xB4 }), rig.RunOk (form.go), where);
                    Assert::AreEqual ((Word) 0x0304, rig.GetRegisters().pc, Widen (where + ": past the load").c_str());
                    Assert::AreEqual ((Byte) 0xA0,   rig.GetRegisters().a,  Widen (where + ": the load ran").c_str());
                }
                else
                {
                    AssertLines (GetStopLines (form.mode, kWriteStop, { 0x0305, 0x41, 0x00, 0x00, 0xFF, 0x34 }), rig.RunOk (form.go), where);
                    Assert::AreEqual ((Word) 0x0305, rig.GetRegisters().pc, Widen (where + ": past the store").c_str());
                    Assert::AreEqual ((Byte) 0x41,   rig.Peek (0x0400),     Widen (where + ": the store ran").c_str());
                }

                Assert::IsTrue (rig.List().at (0).ends_with ("of $0400, hits 1"), Widen (where + ": one hit").c_str());
            }
        }



        //  The command puts the watched pages on the real bus, and only those:
        //  a BEFORE watchpoint adds none, disabling or clearing takes a page
        //  off unless something else still needs it, and a value breakpoint
        //  keeps its page. Call recording keeps the stack page, as batch runs.
        TEST_METHOD (Set_PutsTheRangesPagesOnTheBus_AndOnlyThose)
        {
            BatchPathRig  rig;
            MemoryBus   & bus = rig.machine.GetMemoryBus();



            Assert::IsFalse (bus.IsPageWatched (0x03));
            Assert::IsFalse (bus.IsPageWatched (0x04));
            Assert::IsTrue  (bus.IsPageWatched (0x01), L"batch runs record calls, which watches the stack page");

            rig.Expect ("BPM 3F0:410", { "Breakpoint #0 set on read or write of $03F0-$0410" });
            Assert::IsTrue  (bus.IsPageWatched (0x03));
            Assert::IsTrue  (bus.IsPageWatched (0x04));
            Assert::IsFalse (bus.IsPageWatched (0x05));
            Assert::IsTrue  (rig.machine.GetDebugHook()->GetFilter().everyInstruction, L"a watch hit gives the address of the instruction that made it");

            rig.Expect ("BPMW 500 BEFORE", { "Breakpoint #1 set on write of $0500, before the access" });
            Assert::IsFalse (bus.IsPageWatched (0x05), L"a before-mode watchpoint is predicted, not watched");

            rig.RunOk ("BPD 0");
            Assert::IsFalse (bus.IsPageWatched (0x03));
            Assert::IsFalse (bus.IsPageWatched (0x04));

            rig.RunOk ("BPE 0");
            Assert::IsTrue  (bus.IsPageWatched (0x04));

            rig.RunOk ("BPMV 410 41");
            rig.RunOk ("BPC 0");
            Assert::IsFalse (bus.IsPageWatched (0x03));
            Assert::IsTrue  (bus.IsPageWatched (0x04), L"the value breakpoint keeps its page");

            rig.RunOk ("BPC *");
            Assert::IsFalse (bus.IsPageWatched (0x04));
            Assert::IsTrue  (bus.IsPageWatched (0x01), L"the stack page is the call record's");
            Assert::IsNull  (rig.machine.GetDebugHook(), L"nothing armed, no hook");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Stops on every path
        //
        ////////////////////////////////////////////////////////////////////////

        //  A read watch's stop on each path, as text and as the channel's
        //  record: the address, the value read, the access, the instruction,
        //  the mode, and no replaced byte.
        TEST_METHOD (ReadWatch_StopsAfterTheRead_OnEveryPath)
        {
            BatchPathRig              batch;
            WindowPathRig             window;
            std::vector<std::string>  lines;
            JsonValue                 stop;



            LoadRead (batch.target);
            batch.RunOk ("BPMR 400");
            batch.sink.SetJson (true);
            lines = batch.RunOk ("GG");

            Assert::AreEqual ((size_t) 1, lines.size());
            stop = ParseJson (lines[0]);
            Assert::AreEqual (std::string ("watchpoint"), GetString (stop, "reason"));
            Assert::AreEqual (0x0304,                     GetInt    (stop, "pc"));
            Assert::AreEqual (0,                          GetInt    (GetObject (stop, "watch"), "id"));
            Assert::AreEqual (0x0400,                     GetInt    (GetObject (stop, "watch"), "address"));
            Assert::AreEqual (0xA0,                       GetInt    (GetObject (stop, "watch"), "value"));
            Assert::AreEqual (std::string ("read"),       GetString (GetObject (stop, "watch"), "access"));
            Assert::AreEqual (0x0301,                     GetInt    (GetObject (stop, "watch"), "accessPc"));
            Assert::AreEqual (std::string ("after"),      GetString (GetObject (stop, "watch"), "mode"));
            Assert::IsFalse  (HasMember (GetObject (stop, "watch"), "previous"), L"a read replaces nothing");
            Assert::IsFalse  (HasMember (stop, "breakpointId"));

            LoadRead (window.GetTarget());
            window.RunOk ("BPMR 400");
            window.Go ("G");
            stop = window.GetOnlyStop (0, "watchpoint", 0x0304);
            Assert::AreEqual (0xA0,   GetInt (GetObject (stop, "watch"), "value"));
            Assert::AreEqual (0x0301, GetInt (GetObject (stop, "watch"), "accessPc"));
            Assert::AreEqual (0xA0,   GetInt (GetObject (stop, "registers"), "a"));
            Assert::IsTrue   (window.rig.cpuManager.IsPaused());
            AssertLines ({ "#0 enabled  on read of $0400, hits 1" }, window.List(), "window");
        }



        //  A write watch stops after the store with the byte written and the
        //  one it replaced, on every path; a read of the address does not
        //  stop it, and does not count.
        TEST_METHOD (WriteWatch_StopsAfterTheStore_AndNotOnARead)
        {
            BatchPathRig   store;
            BatchPathRig   load;
            WindowPathRig  window;
            JsonValue      stop;



            Programs::LoadLine (store.target);
            store.RunOk  ("BPMW 400");
            store.Expect ("G", { kWriteStop });
            Assert::AreEqual ((Byte) 0x41, store.Peek (0x0400), L"the byte is written by the time of the stop");

            LoadRead (load.target);
            load.RunOk  ("BPMW 400");
            load.RunOk  ("BP 305");
            load.Expect ("G", { "Breakpoint #1 at $0305" });
            AssertLines ({ "#0 enabled  on write of $0400, hits 0", "#1 enabled  at $0305, hits 1" }, load.List(), "a read is not a write");

            Programs::LoadLine (window.GetTarget());
            window.RunOk ("BPMW 400");
            window.Go ("G");
            stop = window.GetOnlyStop (0, "watchpoint", 0x0305);
            Assert::AreEqual (std::string ("write"), GetString (GetObject (stop, "watch"), "access"));
            Assert::AreEqual (0x41,                  GetInt    (GetObject (stop, "watch"), "value"));
            Assert::AreEqual (0xA0,                  GetInt    (GetObject (stop, "watch"), "previous"));
            Assert::AreEqual (0x0302,                GetInt    (GetObject (stop, "watch"), "accessPc"));
        }



        //  A read-or-write watch stops on either, and on an I/O address the
        //  stop gives the byte the device returned, which is what the load
        //  put in A.
        TEST_METHOD (ReadOrWriteWatch_StopsOnEither_IncludingAnIoRead)
        {
            static constexpr Form  kIoForms[] =
            {
                { CommandMode::AppleWin,  "BPIO C000",   "Breakpoint #0 set on read or write of $C000", "G", true },
                { CommandMode::AppleWin,  "BPMR C000",   "Breakpoint #0 set on read of $C000",          "G", true },
                { CommandMode::GSSquared, "bpi C000 r",  kGss,                                          "g", true },
                { CommandMode::GSSquared, "bpi C000 rw", kGss,                                          "g", true },
            };



            for (const Form & form : kIoForms)
            {
                BatchPathRig  rig;
                std::string   where = form.line;



                LoadIoRead (rig.target);
                rig.EnterMode (form.mode);
                AssertLines ({ form.reply }, rig.RunOk (form.line), where);
                rig.RunOk (form.go);

                Assert::AreEqual ((Word) 0x0304, rig.GetRegisters().pc, Widen (where).c_str());
                rig.EnterMode (CommandMode::AppleWin);
                Assert::IsTrue (rig.sink.GetLastStopReason() == StopReason::Watchpoint, Widen (where).c_str());
            }

            {
                BatchPathRig              rig;
                std::vector<std::string>  lines;



                LoadIoRead (rig.target);
                rig.RunOk ("BPIO C000");
                lines = rig.RunOk ("G");

                AssertLines ({ std::format ("Watchpoint #0: Read ${:02X} from $C000 by $0301", rig.GetRegisters().a) }, lines,
                             "the stop gives what the keyboard returned, which is what the load put in A");
            }
        }



        //  A watch hit ends a step part way through, on each step and each
        //  path: T 5 after the store, P inside the call that stores, RTS
        //  before the routine returns. The step after goes on as usual.
        TEST_METHOD (AWatchHit_EndsAStepPartWay)
        {
            BatchPathRig   count;
            BatchPathRig   over;
            BatchPathRig   out;
            WindowPathRig  window;



            Programs::LoadLine (count.target);
            count.RunOk  ("BPMW 400");
            count.Expect ("T 5", { kWriteStop });
            count.Expect ("T",   { "Step at $0306" });

            Programs::LoadCall (over.target);
            over.RunOk  ("BPMW 400");
            over.Expect ("P", { "Watchpoint #0: Write $41 to $0400 by $0312 (was $A0)" });
            Assert::AreEqual ((Word) 0x0315, over.GetRegisters().pc, L"inside the call, past the store");

            Programs::LoadCall (out.target);
            out.Expect ("T",   { "Step at $0310" });
            out.RunOk  ("BPMR 400");
            out.RunOk  ("BPMW 400");
            out.Expect ("RTS", { "Watchpoint #1: Write $41 to $0400 by $0312 (was $A0)" });
            Assert::AreEqual ((Word) 0x0315, out.GetRegisters().pc, L"before the return");

            Programs::LoadCall (window.GetTarget());
            window.EnterMode (CommandMode::GSSquared);
            window.RunOk ("bpd 400 w");
            window.Go ("o");
            Assert::AreEqual (0x0312, GetInt (GetObject (window.GetOnlyStop (0, "watchpoint", 0x0315), "watch"), "accessPc"));

            window.EnterMode (CommandMode::WinDbg);
            window.Go ("t");
            window.GetOnlyStop (1, "step", 0x0316);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The accesses a real CPU makes
        //
        ////////////////////////////////////////////////////////////////////////

        //  An indexed store reads its target before writing it. Within the
        //  one instruction the write replaces the read, so a read-or-write
        //  watch reports the store and the byte it replaced; a read watch
        //  sees the read. After the access, every access is seen, so a read
        //  watch over code stops on the CPU's own fetch of it.
        TEST_METHOD (TheAccessesARealCpuMakes_AreTheOnesReported)
        {
            BatchPathRig  both;
            BatchPathRig  read;
            BatchPathRig  code;



            LoadIndexedStore (both.target);
            both.RunOk  ("BPM 410");
            both.Expect ("G", { "Watchpoint #0: Write $41 to $0410 by $0304 (was $A0)" });
            AssertLines ({ "#0 enabled  on read or write of $0410, hits 2" }, both.List(), "the read and the write each counted");

            LoadIndexedStore (read.target);
            read.RunOk  ("BPMR 410");
            read.Expect ("G", { "Watchpoint #0: Read $A0 from $0410 by $0304" });
            Assert::AreEqual ((Byte) 0x41, read.Peek (0x0410), L"the store completed");

            Programs::LoadLine (code.target);
            code.RunOk  ("BPMR 300:30F");
            code.Expect ("G", { "Watchpoint #0: Read $A9 from $0300 by $0300" });
            Assert::AreEqual ((Word) 0x0302, code.GetRegisters().pc, L"the first fetch was the first hit");
        }



        //  A watch on the stack page sees a JSR push its return address,
        //  though the call record watches the same page.
        TEST_METHOD (AStackWatch_SeesThePushes_BesideTheCallRecord)
        {
            BatchPathRig   batch;
            WindowPathRig  window;
            JsonValue      stop;



            Programs::LoadCall (batch.target);
            Assert::IsTrue (batch.target.TryPoke (0x01FF, 0x55));
            batch.RunOk  ("BPMW 1FF");
            batch.Expect ("G", { "Watchpoint #0: Write $03 to $01FF by $0300 (was $55)" });
            Assert::AreEqual ((Word) 0x0310, batch.GetRegisters().pc);

            Programs::LoadCall (window.GetTarget());
            Assert::IsTrue (window.GetTarget().TryPoke (0x01FE, 0x55));
            window.RunOk ("BPMW 1FE");
            window.Go ("G");
            stop = window.GetOnlyStop (0, "watchpoint", 0x0310);
            Assert::AreEqual (0x02, GetInt (GetObject (stop, "watch"), "value"),    L"the low byte of the return address less one");
            Assert::AreEqual (0x55, GetInt (GetObject (stop, "watch"), "previous"));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  BEFORE
        //
        ////////////////////////////////////////////////////////////////////////

        //  BEFORE stops ahead of the instruction whose operand would touch the
        //  range, memory untouched. Resuming runs it, and an after-mode watch
        //  on the same range does not stop it again.
        TEST_METHOD (Before_StopsAheadOfTheAccess_AndTheResumeIsOneStop)
        {
            struct BeforeForm
            {
                CommandMode   mode;
                const char  * line;
                const char  * reply;
                const char  * go;
                bool          isRead;
                const char  * stop;
            };

            static constexpr BeforeForm  kForms[] =
            {
                { CommandMode::AppleWin, "BPMW 400 BEFORE",     "Breakpoint #0 set on write of $0400, before the access",                 "G", false, "Watchpoint #0: write of $0400 by $0302, before the access" },
                { CommandMode::AppleWin, "BPMW 400 before",     "Breakpoint #0 set on write of $0400, before the access",                 "G", false, "Watchpoint #0: write of $0400 by $0302, before the access" },
                { CommandMode::AppleWin, "BPMR 400 BEFORE",     "Breakpoint #0 set on read of $0400, before the access",                  "G", true,  "Watchpoint #0: read of $0400 by $0301, before the access"  },
                { CommandMode::AppleWin, "BPM 400:4FF BEFORE",  "Breakpoint #0 set on read or write of $0400-$04FF, before the access",   "G", false, "Watchpoint #0: write of $0400 by $0302, before the access" },
                { CommandMode::AppleWin, "BPM 400,10 BEFORE",   "Breakpoint #0 set on read or write of $0400-$040F, before the access",   "G", true,  "Watchpoint #0: read of $0400 by $0301, before the access"  },
                { CommandMode::WinDbg,   "ba w1 400 before",    "Breakpoint #0 set on write of $0400, before the access",                 "g", false, "Watchpoint #0: write of $0400 by $0302, before the access" },
            };



            for (const BeforeForm & form : kForms)
            {
                BatchPathRig              rig;
                std::string               where = form.line;
                std::vector<std::string>  lines;
                Word                      pc    = form.isRead ? 0x0301 : 0x0302;



                if (form.isRead)
                {
                    LoadRead (rig.target);
                }
                else
                {
                    Programs::LoadLine (rig.target);
                }

                rig.EnterMode (form.mode);
                AssertLines ({ form.reply }, rig.RunOk (form.line), where);
                Assert::IsFalse (rig.machine.GetMemoryBus().IsPageWatched (0x04), Widen (where + ": predicted, not watched").c_str());

                lines = rig.RunOk (form.go);
                Assert::AreEqual (std::string (form.stop), lines.at (0), Widen (where).c_str());
                Assert::AreEqual (pc,          rig.GetRegisters().pc, Widen (where + ": stopped ahead of the access").c_str());
                Assert::AreEqual ((Byte) 0xA0, rig.Peek (0x0400),     Widen (where + ": memory untouched").c_str());
            }
        }



        TEST_METHOD (Before_ResumingRunsTheInstruction_AndAnAfterWatchOnItDoesNotStopIt)
        {
            BatchPathRig   batch;
            WindowPathRig  window;
            JsonValue      stop;



            Programs::LoadLine (batch.target);
            batch.RunOk  ("BPMW 400 BEFORE");
            batch.RunOk  ("BPMW 400");
            batch.RunOk  ("BP 307");
            batch.Expect ("G", { "Watchpoint #0: write of $0400 by $0302, before the access" });
            batch.Expect ("G", { "Breakpoint #2 at $0307" });
            Assert::AreEqual ((Byte) 0x41, batch.Peek (0x0400), L"the store ran on the resume");
            AssertLines ({ "#0 enabled  on write of $0400, before the access, hits 1",
                           "#1 enabled  on write of $0400, hits 0",
                           "#2 enabled  at $0307, hits 1" }, batch.List(), "one instruction, one stop");

            Programs::LoadLine (window.GetTarget());
            window.RunOk ("BPMW 400 BEFORE");
            window.Go ("G");
            stop = window.GetOnlyStop (0, "watchpoint", 0x0302);
            Assert::AreEqual (std::string ("before"), GetString (GetObject (stop, "watch"), "mode"));
            Assert::AreEqual (std::string ("write"),  GetString (GetObject (stop, "watch"), "access"));
            Assert::AreEqual (0x0400,                 GetInt    (GetObject (stop, "watch"), "address"));
            Assert::AreEqual (0x0302,                 GetInt    (GetObject (stop, "watch"), "accessPc"));
            Assert::AreEqual ((Byte) 0xA0,            window.Peek (0x0400));
        }



        //  The first instruction of a run is never asked about, so a BEFORE
        //  watch on the store the run starts at does not stop it.
        TEST_METHOD (Before_OnTheRunsFirstInstruction_DoesNotStop)
        {
            BatchPathRig  rig;



            Programs::LoadLine (rig.target);
            rig.RunOk  ("R PC=302");
            rig.RunOk  ("BPMW 400 BEFORE");
            rig.RunOk  ("BP 307");
            rig.Expect ("G", { "Breakpoint #1 at $0307" });
            Assert::AreEqual ((Byte) 0x00, rig.Peek (0x0400), L"the store ran, unreported");
            AssertLines ({ "#0 enabled  on write of $0400, before the access, hits 0", "#1 enabled  at $0307, hits 1" }, rig.List(), "no hit");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  IF, ACCESS and VALUE
        //
        ////////////////////////////////////////////////////////////////////////

        //  A watchpoint's IF sees ACCESS, and VALUE after the access; a false
        //  one neither stops nor counts, a true one's stop gives its value.
        TEST_METHOD (If_SeesAccessAndValue)
        {
            struct IfForm
            {
                CommandMode   mode;
                const char  * line;
                bool          isRead;
                bool          stops;
                const char  * stop;
            };

            static constexpr IfForm  kForms[] =
            {
                { CommandMode::AppleWin,  "BPMW 400 IF VALUE = 41",          false, true,  "Watchpoint #0: Write $41 to $0400 by $0302 (was $A0), IF VALUE = 41 is $1" },
                { CommandMode::AppleWin,  "BPMW 400 IF VALUE = 42",          false, false, nullptr },
                { CommandMode::AppleWin,  "BPMW 400 if value == 41",         false, true,  "Watchpoint #0: Write $41 to $0400 by $0302 (was $A0), IF value == 41 is $1" },
                { CommandMode::AppleWin,  "BPMR 400:40F IF ACCESS = 400",    true,  true,  "Watchpoint #0: Read $A0 from $0400 by $0301, IF ACCESS = 400 is $1" },
                { CommandMode::AppleWin,  "BPMR 400:40F IF ACCESS = 405",    true,  false, nullptr },
                { CommandMode::AppleWin,  "BPM 400 IF Y = 0 & VALUE > 80",   true,  true,  "Watchpoint #0: Read $A0 from $0400 by $0301, IF Y = 0 & VALUE > 80 is $1" },
                { CommandMode::AppleWin,  "BPM 400 IF Y = 1 & VALUE > 80",   true,  false, nullptr },
                { CommandMode::AppleWin,  "BPMW 400 IF 80/VALUE = 1",        false, true,  "Watchpoint #0: Write $41 to $0400 by $0302 (was $A0), IF 80/VALUE = 1 is $1" },
                { CommandMode::AppleWin,  "BPMW 400 BEFORE IF ACCESS = 400", false, true,  "Watchpoint #0: write of $0400 by $0302, before the access, IF ACCESS = 400 is $1" },
                { CommandMode::GSSquared, "bpd 400 w if VALUE = 41",         false, true,  "Watchpoint #0: Write $41 to $0400 by $0302 (was $A0), IF VALUE = 41 is $1" },
                { CommandMode::WinDbg,    "ba w1 400 IF VALUE = 41",         false, true,  "Watchpoint #0: Write $41 to $0400 by $0302 (was $A0), IF VALUE = 41 is $1" },
            };



            for (const IfForm & form : kForms)
            {
                BatchPathRig              rig;
                std::string               where = form.line;
                std::vector<std::string>  lines;



                if (form.isRead)
                {
                    LoadRead (rig.target);
                }
                else
                {
                    Programs::LoadLine (rig.target);
                }

                rig.EnterMode (form.mode);
                rig.RunOk (form.line);
                rig.RunOk ("bp 305");
                lines = rig.RunOk (form.mode == CommandMode::AppleWin ? "G" : "g");

                if (form.stops)
                {
                    Assert::AreEqual (std::string (form.stop), lines.at (0), Widen (where).c_str());
                }
                else
                {
                    Assert::AreEqual (std::string ("Breakpoint #1 at $0305"), lines.at (0), Widen (where + ": ran past to the next breakpoint").c_str());
                }

                rig.EnterMode (CommandMode::AppleWin);
                Assert::IsTrue (rig.List().at (0).ends_with (form.stops ? "hits 1" : "hits 0"), Widen (where + ": a false IF does not count").c_str());
            }
        }



        //  ACCESS and VALUE are the access's own, even with symbols of the
        //  same names loaded; a BEFORE watch has no VALUE yet.
        TEST_METHOD (If_AccessAndValue_ComeBeforeSymbols_AndBeforeHasNoValue)
        {
            BatchPathRig  rig;



            Programs::LoadLine (rig.target);
            rig.RunOk ("SYM VALUE = 1234");
            rig.RunOk ("SYM ACCESS = 5678");
            rig.RunOk ("BPMW 400 IF VALUE = 41 & ACCESS = 400");
            rig.Expect ("G", { "Watchpoint #0: Write $41 to $0400 by $0302 (was $A0), IF VALUE = 41 & ACCESS = 400 is $1" });

            rig.RunOk    ("SYM ! VALUE");
            rig.RunFails ("BPMW 400 BEFORE IF VALUE = 1", "invalid condition", "VALUE is not a symbol, register, or hex number.");
            rig.RunFails ("BPMW 400 IF NOSUCH = 1",       "invalid condition", "NOSUCH is not a symbol, register, or hex number.");
            rig.RunFails ("BPMR 400 IF *C010 = 0",        "invalid condition", "The condition reads $C010, an I/O address. Reading it would change the machine.");
            rig.RunFails ("BPM 400 IF",                   "invalid arguments", "IF needs an expression.");
            rig.RunFails ("BPM 400 IF A 1",               "invalid arguments", "An operator was expected at column 3.");
            Assert::AreEqual ((size_t) 1, rig.List().size(), L"none of those was set");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Overlapping watchpoints
        //
        ////////////////////////////////////////////////////////////////////////

        //  A watchpoint that only counts does not hide a later one on the same
        //  address: both count the store, and the later one stops.
        TEST_METHOD (ACountsOnlyWatchpoint_DoesNotHideALaterOne)
        {
            BatchPathRig  rig;



            Programs::LoadLine (rig.target);
            rig.RunOk  ("BPM 400");
            rig.RunOk  ("BPCHANGE 0 s");
            rig.RunOk  ("BPMW 400");
            rig.Expect ("G", { "Watchpoint #1: Write $41 to $0400 by $0302 (was $A0)" });
            AssertLines ({ "#0 enabled  on read or write of $0400, counts only, hits 1",
                           "#1 enabled  on write of $0400, hits 1" }, rig.List(), "both counted");
        }



        //  Two watchpoints that stop on the same access: the first in id order
        //  is the stop, and both count.
        TEST_METHOD (TwoStoppingWatchpoints_TheFirstIsTheStop_AndBothCount)
        {
            BatchPathRig  rig;



            Programs::LoadLine (rig.target);
            rig.RunOk  ("BPMW 400");
            rig.RunOk  ("BPM 3FF:401");
            rig.Expect ("G", { kWriteStop });
            AssertLines ({ "#0 enabled  on write of $0400, hits 1",
                           "#1 enabled  on read or write of $03FF-$0401, hits 1" }, rig.List(), "both counted");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Errors
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Errors_GiveTheWholeMessage_AndSetNothing)
        {
            BatchPathRig  rig;



            rig.RunFails ("BPM",              "invalid arguments", "BPM needs an address.");
            rig.RunFails ("BPMR",             "invalid arguments", "BPMR needs an address.");
            rig.RunFails ("bpmw",             "invalid arguments", "BPMW needs an address.");
            rig.RunFails ("BPIO",             "invalid arguments", "BPIO needs an address.");
            rig.RunFails ("BPM 400 SIDEWAYS", "invalid arguments", "A watchpoint's mode is BEFORE or AFTER.");
            rig.RunFails ("BPM 400,0",        "invalid arguments", "A range length must be at least 1.");
            rig.RunFails ("BPMW 400:300",     "invalid arguments", "$0400-$0300 ends before it starts.");
            rig.RunFails ("BPM FFFF,2",       "invalid arguments", "$FFFF-$0000 ends before it starts.");
            rig.RunFails ("BPMR NOSUCH",      "invalid arguments", "NOSUCH is not a symbol, register, or hex number.");
            rig.RunFails ("BPMW 10000",       "invalid arguments", "10000 is outside $0000-$FFFF.");

            rig.EnterMode (CommandMode::GSSquared);
            rig.RunFails ("bpd 400",          "invalid arguments", "BPD takes an address or a range, then r, w, or rw.");
            rig.RunFails ("bpd 400 w junk",   "invalid arguments", "BPD takes an address or a range, then r, w, or rw.");
            rig.RunFails ("bpd 400 x",        "invalid arguments", "x is not an access. The accesses are r, w, and rw.");
            rig.RunFails ("bpd 40F.400 r",    "invalid arguments", "40F.400 ends before it starts.");
            rig.RunFails ("bpd zz r",         "invalid arguments", "zz is not an address. An address is one to four hex digits.");
            rig.RunFails ("bpi C000",         "invalid arguments", "BPI takes an address or a range, then r, w, or rw.");
            rig.RunFails ("bpi 300 rw",       "invalid arguments", "BPI takes an address in the I/O page, $C000-$C0FF.");
            rig.RunFails ("bpi C0F0.C100 r",  "invalid arguments", "BPI takes an address in the I/O page, $C000-$C0FF.");

            rig.EnterMode (CommandMode::WinDbg);
            rig.RunFails ("ba x1 300",        "invalid arguments", "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.");
            rig.RunFails ("ba w0 400",        "invalid arguments", "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.");
            rig.RunFails ("ba w1",            "invalid arguments", "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.");
            rig.RunFails ("ba i1 c000",       "invalid arguments", "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.");
            rig.RunFails ("ba",               "invalid arguments", "ba takes an access and a size, then an address: ba r1 C000, ba w1 400, ba e1 300.");

            rig.EnterMode (CommandMode::AppleWin);
            AssertLines ({ "No breakpoints." }, rig.List(), "nothing was set");
            Assert::IsFalse (rig.machine.GetMemoryBus().IsPageWatched (0x04));
        }



        //  WinDbg's ba sizes become a range, and its r is a read watch, as the
        //  dialect's table documents: a store does not stop ba r1.
        TEST_METHOD (WinDbgBa_SizesAndAccesses)
        {
            BatchPathRig              rig;
            std::vector<std::string>  lines;



            Programs::LoadLine (rig.target);
            rig.EnterMode (CommandMode::WinDbg);
            rig.Expect ("ba w4 3FE", { "Breakpoint #0 set on write of $03FE-$0401" });
            rig.Expect ("ba r1 400", { "Breakpoint #1 set on read of $0400" });
            rig.Expect ("bl",        { "0 e 03fe-0401 w4 0001 (0001)", "1 e 0400 r1 0001 (0001)" });

            lines = rig.RunOk ("g");
            Assert::AreEqual (std::string ("Watchpoint #0: Write $41 to $0400 by $0302 (was $A0)"), lines.at (0));
            AssertLines ({ "#0 enabled  on write of $03FE-$0401, hits 1", "#1 enabled  on read of $0400, hits 0" }, rig.List(), "ba r1 is not a write watch");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  BPA
        //
        ////////////////////////////////////////////////////////////////////////

        //  BPA sets a breakpoint and a read-or-write watchpoint on the same
        //  range, with consecutive ids, and lists them in each format.
        TEST_METHOD (BPA_SetsABreakpointAndAWatchpoint_ListedTogether)
        {
            BatchPathRig  rig;
            BatchPathRig  gss;
            BatchPathRig  windbg;



            rig.Expect ("BPA 302", { "#0 enabled  at $0302, hits 0", "#1 enabled  on read or write of $0302, hits 0" });
            rig.Expect ("BPA 310,10", { "#2 enabled  at $0310-$031F, hits 0", "#3 enabled  on read or write of $0310-$031F, hits 0" });
            rig.Expect ("BPA 320:32F", { "#4 enabled  at $0320-$032F, hits 0", "#5 enabled  on read or write of $0320-$032F, hits 0" });
            Assert::IsTrue (rig.machine.GetMemoryBus().IsPageWatched (0x03));

            rig.EnterMode (CommandMode::Monitor);
            rig.Expect ("/BPA 400", { "#6 enabled  at $0400, hits 0", "#7 enabled  on read or write of $0400, hits 0" });
            rig.EnterMode (CommandMode::Casso);
            rig.Expect ("BPA 500", { "#8 enabled  at $0500, hits 0", "#9 enabled  on read or write of $0500, hits 0" });

            gss.EnterMode (CommandMode::GSSquared);
            gss.Expect ("BPA 302", { "Current breakpoints:", "[0] exec 00/0302", "[1] data 00/0302 rw" });

            windbg.EnterMode (CommandMode::WinDbg);
            windbg.Expect ("!BPA 302", { "0 e 0302 0001 (0001)", "1 e 0302 rw1 0001 (0001)" });

            rig.EnterMode (CommandMode::AppleWin);
            rig.RunFails ("BPA",         "invalid arguments", "BPA needs an address.");
            rig.RunFails ("BPA 400:300", "invalid arguments", "$0400-$0300 ends before it starts.");
            Assert::AreEqual ((size_t) 10, rig.List().size(), L"the errors set nothing");
        }



        //  At run time the pair stops before the instruction at the address,
        //  and again after it on the resume, because the CPU's own fetch of it
        //  is a read of the watched range. A data access to the range stops
        //  after the instruction that made it.
        TEST_METHOD (BPA_StopsBeforeTheInstruction_AndOnItsFetch_AndOnData)
        {
            BatchPathRig  code;
            BatchPathRig  data;



            Programs::LoadLine (code.target);
            code.RunOk  ("BPA 302");
            code.Expect ("G", { "Breakpoint #0 at $0302" });
            code.Expect ("G", { "Watchpoint #1: Read $8D from $0302 by $0302" });
            Assert::AreEqual ((Word) 0x0305, code.GetRegisters().pc);

            Programs::LoadLine (data.target);
            data.RunOk  ("BPA 400");
            data.Expect ("G", { "Watchpoint #1: Write $41 to $0400 by $0302 (was $A0)" });
            AssertLines ({ "#0 enabled  at $0400, hits 0", "#1 enabled  on read or write of $0400, hits 1" }, data.List(), "data");
        }
    };
}
