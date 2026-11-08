#include "Pch.h"

#include "AppleWinRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinRunCommandTests
//
//  G, GG, T, TL, TRACE, P, RTS, JSR, addrG, PAUSE and BUDGET, each run the
//  two ways a user runs it: in a batch script, where SynchronousRunDriver
//  carries the run out before the reply, and in the window or over the
//  channel, where CpuManagerRunDriver starts it and the CPU thread's slices
//  finish it. Every test reads back where the run stopped and what it did to
//  the registers and memory.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleWinRunCommandTests)
    {
    public:

        //  INX / JMP $0300, forever.
        static constexpr const char * kLoop = "MEB 300 E8 4C 00 03";

        //  $0300 JSR $0310 / INX / JMP $0304 (a spin); $0310 INY / JSR $0320 /
        //  RTS; $0320 INY / RTS.
        static constexpr const char * kCall1 = "MEB 300 20 10 03 E8 4C 04 03";
        static constexpr const char * kCall2 = "MEB 310 C8 20 20 03 60";
        static constexpr const char * kCall3 = "MEB 320 C8 60";

        //  The first record of a type among the records a client was sent.
        static std::optional<JsonValue> FindRecord (const std::vector<std::string> & records, const std::string & wanted)
        {
            std::string  type;



            for (const std::string & record : records)
            {
                JsonValue  root = AppleWinConsoleRig::Parse (record);



                if (root.HasString ("type", type) && type == wanted)
                {
                    return root;
                }
            }

            return std::nullopt;
        }

        static std::string GetString (const JsonValue & value, const std::string & key)
        {
            std::string  text;



            Assert::IsTrue (value.HasString (key, text), AppleWinBatchRig::Widen (key).c_str());
            return text;
        }

        static int GetInt (const JsonValue & value, const std::string & key)
        {
            int  number = 0;



            Assert::IsTrue (value.HasInt (key, number), AppleWinBatchRig::Widen (key).c_str());
            return number;
        }



        //  T counts in hex; no count, and a count of 0, are one instruction.
        //  TL and TRACE are T.
        TEST_METHOD (T_TL_TRACE_StepTheCountInHex)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ "F 300:3FF E8", "R PC 300", "R X 0",
                                            "T", "T 2", "T 10", "T 0", "TL 3", "TRACE", "TRACE 2", "tl", "trace" }));

            rig.ExpectBlock (3,  { "Step at $0301" });
            rig.ExpectBlock (4,  { "Step at $0303" });
            rig.ExpectBlock (5,  { "Step at $0313" });
            rig.ExpectBlock (6,  { "Step at $0314" });
            rig.ExpectBlock (7,  { "Step at $0317" });
            rig.ExpectBlock (8,  { "Step at $0318" });
            rig.ExpectBlock (9,  { "Step at $031A" });
            rig.ExpectBlock (10, { "Step at $031B" });
            rig.ExpectBlock (11, { "Step at $031C" });

            Assert::AreEqual ((Byte) 0x1C,   rig.GetRegisters().x, L"one INX per instruction stepped");
            Assert::AreEqual ((Word) 0x031C, rig.GetRegisters().pc);
        }



        //  The count is a 16-bit expression: $FFFF instructions run, and
        //  $10000 is refused.
        TEST_METHOD (T_TakesUpToFFFFInstructions)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ kLoop, "R PC 300", "R X 5", "T FFFF", "CYCLES REL", "R PC 300", "T #65535", "T 10000", "T FROB" }));

            //  65535 instructions from the INX: 32768 INX and 32767 JMP.
            rig.ExpectBlock (3, { "Step at $0301" });
            rig.ExpectBlock (4, { "Cycles: 163837" });
            rig.ExpectBlock (6, { "Step at $0301" });
            rig.ExpectBlock (7, { "Error: invalid arguments", "       10000 is outside $0000-$FFFF." });
            rig.ExpectBlock (8, { "Error: invalid arguments", "       FROB is not a symbol, register, or hex number." });

            Assert::AreEqual ((Byte) 0x05, rig.GetRegisters().x, L"two runs of 32768 INX each");
        }



        //  P runs a JSR to its return; any other instruction is one step.
        TEST_METHOD (P_StepsOverACall_AndSingleStepsTheRest)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kCall1, kCall2, kCall3, "R PC 300", "R Y 0", "R X 0", "P", "P", "P" }));

            rig.ExpectBlock (6, { "Step at $0303" });
            rig.ExpectBlock (7, { "Step at $0304" });
            rig.ExpectBlock (8, { "Step at $0304" });

            Assert::AreEqual ((Byte) 0x02, rig.GetRegisters().y, L"both INYs of the calls it stepped over");
            Assert::AreEqual ((Byte) 0x01, rig.GetRegisters().x);
            Assert::AreEqual ((Byte) 0xFD, rig.GetRegisters().sp, L"the call returned");
        }



        //  RTS leaves the innermost call, found by the call record a batch run
        //  keeps from its first instruction.
        TEST_METHOD (RTS_StepsOutOfTheInnermostCall)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kCall1, kCall2, kCall3, "R PC 300", "R Y 0", "T", "T", "T", "RTS", "RTS", "R" }));

            rig.ExpectBlock (5,  { "Step at $0310" });
            rig.ExpectBlock (6,  { "Step at $0311" });
            rig.ExpectBlock (7,  { "Step at $0320" });
            rig.ExpectBlock (8,  { "Step at $0314" });
            rig.ExpectBlock (9,  { "Step at $0303" });
            rig.ExpectBlock (10, { "A:00 X:00 Y:02 P:24 S:FD PC:0303  ..R..I.." });
        }



        //  G runs until a stop condition. Its reply has no text; the stop is
        //  the line after it. A breakpoint's hits add up across runs.
        TEST_METHOD (G_RunsToABreakpoint_AndHitsAccumulateAcrossRuns)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kLoop, "R PC 300", "R X 0", "BP 301", "G", "G", "BPL" }));

            rig.ExpectBlock (4, { "Breakpoint #0 at $0301" });
            rig.ExpectBlock (5, { "Breakpoint #0 at $0301" });
            rig.ExpectBlock (6, { "#0 enabled  at $0301, hits 2" });
            Assert::AreEqual ((Byte) 0x02, rig.GetRegisters().x, L"the loop went round once more");
        }



        TEST_METHOD (G_AsJson_RepliesWithNoText_ThenTheStoppedRecord)
        {
            AppleWinBatchRig         rig;
            std::vector<JsonValue>   records;
            const JsonValue        * data   = nullptr;
            const JsonValue        * text   = nullptr;
            const JsonValue        * regs   = nullptr;



            rig.options.json = true;
            Assert::AreEqual (0, rig.Run ({ kLoop, "R PC 300", "BP 301", "G" }));

            records = rig.GetRecords();
            Assert::AreEqual ((size_t) 5, records.size());
            Assert::AreEqual (std::string ("reply"), GetString (records[3], "type"));
            Assert::AreEqual (std::string ("G"),     GetString (records[3], "command"));
            Assert::AreEqual (std::string ("ok"),    GetString (records[3], "status"));
            Assert::IsTrue   (records[3].HasObject ("data", data));
            Assert::AreEqual (std::string ("message"), GetString (*data, "kind"));
            Assert::IsTrue   (records[3].HasArray ("text", text));
            Assert::AreEqual ((size_t) 0, text->GetArraySize());

            Assert::AreEqual (std::string ("stopped"),    GetString (records[4], "type"));
            Assert::AreEqual (std::string ("breakpoint"), GetString (records[4], "reason"));
            Assert::AreEqual (0x0301,                     GetInt (records[4], "pc"));
            Assert::AreEqual (0,                          GetInt (records[4], "breakpointId"));
            Assert::AreEqual (2,                          GetInt (records[4], "cycles"), L"the INX alone");
            Assert::IsTrue   (records[4].HasObject ("registers", regs));
            Assert::AreEqual (1,                          GetInt (*regs, "x"));
        }



        //  G addr and GG addr run until PC reaches the address, and the first
        //  instruction is never checked, so a run to where PC already is goes
        //  round the loop.
        TEST_METHOD (G_And_GG_RunToAnAddress)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kLoop, "R PC 300", "R X 0", "G 300", "GG 301", "g 300" }));

            rig.ExpectBlock (3, { "Run to at $0300" });
            rig.ExpectBlock (4, { "Run to at $0301" });
            rig.ExpectBlock (5, { "Run to at $0300" });
            Assert::AreEqual ((Byte) 0x02, rig.GetRegisters().x);
        }



        TEST_METHOD (G_RunTo_AsJson_IsReasonRunTo)
        {
            AppleWinBatchRig         rig;
            std::vector<JsonValue>   records;



            rig.options.json = true;
            Assert::AreEqual (0, rig.Run ({ kLoop, "R PC 300", "G 301" }));

            records = rig.GetRecords();
            Assert::AreEqual (std::string ("stopped"), GetString (records.back(), "type"));
            Assert::AreEqual (std::string ("runTo"),   GetString (records.back(), "reason"));
            Assert::AreEqual (0x0301,                  GetInt (records.back(), "pc"));
        }



        //  A second argument is a range the run stays inside: the run ends
        //  when PC leaves it, in either the addr,len or the addr:last form.
        TEST_METHOD (G_SkipRange_EndsTheRunWhenPcLeavesIt)
        {
            AppleWinBatchRig  rig;



            //  $0300 INX / CPX #5 / BNE $0300 / JMP $0310; $0310 JMP $0310.
            Assert::AreEqual (0, rig.Run ({ "MEB 300 E8 E0 05 D0 FB 4C 10 03", "MEB 310 4C 10 03",
                                            "R PC 300", "R X 0", "G FFFF 300,10",
                                            "R PC 300", "R X 0", "G FFFF 300:30F",
                                            "R PC 300", "R X 0", "GG FFFF 300,10" }));

            rig.ExpectBlock (4,  { "Run to at $0310" });
            rig.ExpectBlock (7,  { "Run to at $0310" });
            rig.ExpectBlock (10, { "Run to at $0310" });
            Assert::AreEqual ((Byte) 0x05, rig.GetRegisters().x, L"the loop ran out inside the range");
        }



        //  A single address is a range of one, as a range is everywhere else:
        //  a JMP to itself never leaves it, so the run goes to its budget.
        TEST_METHOD (G_SkipOfOneAddress_IsARangeOfOne)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (3, rig.Run ({ "MEB 310 4C 10 03", "R PC 310", "BUDGET 100", "G FFFF 310" }));
            rig.ExpectBlock (3, { "Budget at $0310" });
        }



        //  JSR pushes the address before PC, as the instruction does, and runs
        //  until the routine returns to PC.
        TEST_METHOD (JSR_CallsTheRoutine_AndReturnsToPc)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ kCall1, kCall2, kCall3, "R PC 300", "R S FD", "R Y 0", "JSR 310", "R", "JSR" }));

            rig.ExpectBlock (6, { "Run to at $0300" });
            rig.ExpectBlock (7, { "A:00 X:00 Y:02 P:24 S:FD PC:0300  ..R..I.." });
            rig.ExpectBlock (8, { "Error: invalid arguments", "       JSR takes the subroutine's address." });

            Assert::AreEqual ((Byte) 0x02, rig.PeekRam (0x01FD), L"the high byte of $02FF");
            Assert::AreEqual ((Byte) 0xFF, rig.PeekRam (0x01FC), L"the low byte");
        }



        //  The run ends at PC's old value wherever it is reached, even inside
        //  the routine, so the stack is left two bytes down.
        TEST_METHOD (JSR_EndsWhereverTheOldPcIsReached)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kCall1, "R PC 304", "R S FD", "R X 0", "JSR 303" }));

            rig.ExpectBlock (4, { "Run to at $0304" });
            Assert::AreEqual ((Byte) 0xFB, rig.GetRegisters().sp);
            Assert::AreEqual ((Byte) 0x01, rig.GetRegisters().x);
            Assert::AreEqual ((Byte) 0x03, rig.PeekRam (0x01FD));
            Assert::AreEqual ((Byte) 0x03, rig.PeekRam (0x01FC));
        }



        //  addrG sets PC and runs, with no return address pushed in AppleWin
        //  mode; the address can be $hex or a symbol.
        TEST_METHOD (AddrG_SetsPcAndRuns_PushingNothing)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kLoop, "MEB 1FC AA BB", "R S FD", "R PC 1234", "BP 301",
                                            "300G", "R PC 1234", "$300G", "SYM LOOPTOP = 300", "R PC 1234", "looptopg", "BPL" }));

            rig.ExpectBlock (5,  { "Breakpoint #0 at $0301" });
            rig.ExpectBlock (7,  { "Breakpoint #0 at $0301" });
            rig.ExpectBlock (10, { "Breakpoint #0 at $0301" });
            rig.ExpectBlock (11, { "#0 enabled  at $0301, hits 3" });

            Assert::AreEqual ((Byte) 0xFD, rig.GetRegisters().sp, L"nothing was pushed");
            Assert::AreEqual ((Byte) 0xAA, rig.PeekRam (0x01FC));
            Assert::AreEqual ((Byte) 0xBB, rig.PeekRam (0x01FD));
        }



        //  A token outside the table that ends in G runs from its head when the
        //  head is hex: BG runs from $000B.
        TEST_METHOD (AddrG_AHexLookingTokenRuns)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (3, rig.Run ({ "MEB B E8 4C 0B 00", "R X 0", "BUDGET 100", "BG" }));

            rig.ExpectBlock (3, { "Budget at $000B" });
            Assert::AreEqual ((Byte) 0x14, rig.GetRegisters().x, L"twenty trips round the loop in 100 cycles");
        }



        //  BUDGET limits later runs, persists across them, and 0 removes it:
        //  a run that needs more than --max-cycles then reaches its breakpoint.
        TEST_METHOD (Budget_LimitsRuns_Persists_AndZeroRemovesIt)
        {
            AppleWinBatchRig  rig;



            rig.options.maxCycles = 1000;

            //  $0310 INX / BNE $0310 / NOP: 256 trips, about 1280 cycles.
            Assert::AreEqual (0, rig.Run ({ kLoop, "R PC 300", "G", "BUDGET 500", "G", "G",
                                            "MEB 310 E8 D0 FD EA", "R PC 310", "R X 0", "BP 313", "BUDGET 0", "G" }));

            rig.ExpectBlock (2,  { "Budget at $0300" });
            rig.ExpectBlock (3,  {});
            rig.ExpectBlock (4,  { "Budget at $0300" });
            rig.ExpectBlock (5,  { "Budget at $0300" });
            rig.ExpectBlock (10, {});
            rig.ExpectBlock (11, { "Breakpoint #0 at $0313" });
            Assert::AreEqual ((Byte) 0x00, rig.GetRegisters().x, L"X went all the way round");
        }



        TEST_METHOD (Budget_ArgumentErrors)
        {
            AppleWinBatchRig  rig;
            std::string       usage = "       BUDGET takes a number of cycles in decimal. Use BUDGET 0 to remove the budget.";



            Assert::AreEqual (1, rig.Run ({ "BUDGET C000", "BUDGET", "BUDGET -1", "BUDGET 4294967296", "BUDGET 99999999999999999999", "BUDGET 4294967295" }));

            rig.ExpectBlock (0, { "Error: invalid arguments", usage });
            rig.ExpectBlock (1, { "Error: invalid arguments", usage });
            rig.ExpectBlock (2, { "Error: invalid arguments", usage });
            rig.ExpectBlock (3, { "Error: invalid arguments", "       A budget is at most 4294967295 cycles. Use BUDGET 0 to remove the budget." });
            rig.ExpectBlock (4, { "Error: invalid arguments", "       A budget is at most 4294967295 cycles. Use BUDGET 0 to remove the budget." });
            rig.ExpectBlock (5, {});
        }



        //  PAUSE in batch has no run to end, since each run is over before the
        //  next line: it replies with nothing and the next run is unaffected.
        TEST_METHOD (Pause_InBatch_RepliesWithNothing)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kLoop, "R PC 300", "BP 301", "PAUSE", "G" }));

            rig.ExpectBlock (3, {});
            rig.ExpectBlock (4, { "Breakpoint #0 at $0301" });
        }



        //  Stepping onto a breakpoint is a step: the stop says so, and the
        //  breakpoint neither counts a hit nor is removed.
        TEST_METHOD (T_OntoABreakpoint_IsAStep)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ kLoop, "R PC 300", "BP 301", "BPCHANGE 0 T", "T", "BPL" }));

            rig.ExpectBlock (4, { "Step at $0301" });
            rig.ExpectBlock (5, { "#0 enabled  at $0301, temporary, hits 0" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The emulator's run path
        //
        ////////////////////////////////////////////////////////////////////////

        //  G typed in the window starts a run on the CPU thread and returns;
        //  the slices find the breakpoint, and the stop goes to every client.
        TEST_METHOD (Console_G_RunsOnTheCpuThread_UntilTheBreakpoint)
        {
            AppleWinConsoleRig        console;
            Reply                     reply;
            std::optional<JsonValue>  stopped;



            console.TypeOk (kLoop);
            console.TypeOk ("R X 0");
            console.TypeOk ("BP 301");
            console.TakeRecords();

            reply = console.TypeOk ("G");
            Assert::IsTrue   (reply.text.empty());
            Assert::IsTrue   (console.GetSession().GetRunState() == RunState::DebugRun);
            Assert::IsFalse  (console.rig.cpuManager.IsPaused(), L"the run un-pauses the machine");
            Assert::IsTrue   (console.rig.controller.IsRunInProgress());

            Assert::IsTrue   (console.RunSlices(), L"the run ended");
            Assert::AreEqual ((Word) 0x0301, console.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x01,   console.GetRegisters().x);
            Assert::IsTrue   (console.GetSession().GetRunState() == RunState::Paused);
            Assert::IsTrue   (console.rig.cpuManager.IsPaused());

            stopped = FindRecord (console.TakeRecords(), "stopped");
            Assert::IsTrue   (stopped.has_value(), L"the stop reached the client");
            Assert::AreEqual (std::string ("breakpoint"), GetString (*stopped, "reason"));
            Assert::AreEqual (0x0301,                     GetInt (*stopped, "pc"));
            Assert::AreEqual (0,                          GetInt (*stopped, "breakpointId"));
        }



        //  T, P and RTS in the window, each finished by the slices.
        TEST_METHOD (Console_Steps_FinishOnTheCpuThread)
        {
            AppleWinConsoleRig        console;
            std::optional<JsonValue>  stopped;



            for (const char * line : { kCall1, kCall2, kCall3, "R PC 300", "R S FD", "R Y 0" })
            {
                console.TypeOk (line);
            }

            console.TakeRecords();

            console.TypeOk ("T");
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual ((Word) 0x0310, console.GetRegisters().pc);
            stopped = FindRecord (console.TakeRecords(), "stopped");
            Assert::IsTrue   (stopped.has_value());
            Assert::AreEqual (std::string ("step"), GetString (*stopped, "reason"));

            console.TypeOk ("RTS");
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual ((Word) 0x0303, console.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x02,   console.GetRegisters().y);

            console.TypeOk ("R PC 300");
            console.TypeOk ("P");
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual ((Word) 0x0303, console.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x04,   console.GetRegisters().y);

            console.TypeOk ("TL 2");
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual ((Word) 0x0304, console.GetRegisters().pc, L"INX, then JMP $0304");
        }



        //  With no recorded frame -- the routine was entered before the
        //  debugger opened -- RTS ends once the stack pointer rises above
        //  where it was and a return has run.
        TEST_METHOD (Console_RTS_WithNoRecordedFrame_FollowsTheStackPointer)
        {
            AppleWinConsoleRig  console;



            for (const char * line : { kCall1, kCall2, kCall3, "R PC 310", "R S FB", "MEB 1FC 02 03", "R Y 0" })
            {
                console.TypeOk (line);
            }

            console.TypeOk ("RTS");
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual ((Word) 0x0303, console.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0xFD,   console.GetRegisters().sp);
            Assert::AreEqual ((Byte) 0x02,   console.GetRegisters().y);
        }



        //  A channel client's G is answered at once, marked running, and the
        //  stop names the request that caused it.
        TEST_METHOD (Channel_G_RepliesRunning_AndTheStopCarriesItsId)
        {
            AppleWinConsoleRig        console;
            std::vector<std::string>  records;
            std::optional<JsonValue>  reply;
            std::optional<JsonValue>  stopped;
            bool                      isRunning = false;



            console.TypeOk (kLoop);
            console.TypeOk ("BP 301");
            console.TakeRecords();

            records = console.Send (R"({"type":"command","id":7,"line":"G"})");
            reply   = FindRecord (records, "reply");
            Assert::IsTrue   (reply.has_value());
            Assert::AreEqual (7, GetInt (*reply, "id"));
            Assert::IsTrue   (reply->HasBool ("running", isRunning));
            Assert::IsTrue   (isRunning);
            Assert::IsFalse  (FindRecord (records, "stopped").has_value(), L"the run has not ended yet");

            Assert::IsTrue   (console.RunSlices());
            stopped = FindRecord (console.TakeRecords(), "stopped");
            Assert::IsTrue   (stopped.has_value());
            Assert::AreEqual (7, GetInt (*stopped, "causeId"));
            Assert::AreEqual (0x0301, GetInt (*stopped, "pc"));
        }



        //  GG runs the emulator at full speed and puts the speed back at the
        //  stop.
        TEST_METHOD (Console_GG_RunsAtFullSpeed_AndRestoresTheSpeed)
        {
            AppleWinConsoleRig  console;



            console.TypeOk (kLoop);
            console.TypeOk ("BP 301");
            Assert::IsTrue (console.rig.cpuManager.GetSpeedMode() == SpeedMode::Authentic);

            console.TypeOk ("GG");
            Assert::IsTrue (console.rig.cpuManager.GetSpeedMode() == SpeedMode::Maximum, L"full speed for the run");

            Assert::IsTrue (console.RunSlices());
            Assert::IsTrue (console.rig.cpuManager.GetSpeedMode() == SpeedMode::Authentic, L"and the speed the user had afterward");
            Assert::AreEqual ((Word) 0x0301, console.GetRegisters().pc);
        }



        //  The emulator counts a budget across slices, so the run ends at the
        //  first slice boundary past it.
        TEST_METHOD (Console_Budget_EndsTheRunAtASliceBoundary)
        {
            AppleWinConsoleRig        console;
            std::optional<JsonValue>  stopped;
            int                       cycles = 0;



            console.TypeOk (kLoop);
            console.TypeOk ("BUDGET 5000");
            console.TakeRecords();
            console.TypeOk ("G");

            Assert::IsTrue (console.RunSlices());
            stopped = FindRecord (console.TakeRecords(), "stopped");
            Assert::IsTrue   (stopped.has_value());
            Assert::AreEqual (std::string ("budget"), GetString (*stopped, "reason"));

            cycles = GetInt (*stopped, "cycles");
            Assert::IsTrue (cycles >= 5000 && cycles < 5000 + (int) AppleWinConsoleRig::kSliceCycles + 7,
                            std::format (L"cycles {}", cycles).c_str());
        }



        //  PAUSE ends a run in progress at the next slice, with reason pause.
        TEST_METHOD (Console_Pause_EndsTheRunInProgress)
        {
            AppleWinConsoleRig        console;
            std::optional<JsonValue>  stopped;
            Reply                     reply;



            console.TypeOk (kLoop);
            console.TakeRecords();
            console.TypeOk ("G");
            Assert::IsFalse (console.RunSlices (5), L"nothing stops the loop");

            reply = console.TypeOk ("PAUSE");
            Assert::IsTrue  (reply.text.empty());
            Assert::IsTrue  (console.RunSlices(), L"the next slice ends the run");
            Assert::IsTrue  (console.GetSession().GetRunState() == RunState::Paused);

            stopped = FindRecord (console.TakeRecords(), "stopped");
            Assert::IsTrue   (stopped.has_value());
            Assert::AreEqual (std::string ("pause"), GetString (*stopped, "reason"));
        }



        //  A run command while a run is in progress starts nothing. Reads still
        //  answer, and writes wait for a paused machine.
        TEST_METHOD (Console_CommandsDuringARun)
        {
            AppleWinConsoleRig  console;
            Reply               reply;
            Byte                a      = 0;



            console.TypeOk (kLoop);
            console.TypeOk ("G");
            console.RunSlices (3);
            a = console.GetRegisters().a;

            for (const char * line : { "G", "GG", "T", "TL", "TRACE", "P", "RTS", "300G", "G 300", "T 2" })
            {
                reply = console.Type (line);
                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, AppleWinBatchRig::Widen (line).c_str());
                Assert::AreEqual (std::string ("already running"), reply.error.label);
                Assert::AreEqual (std::string ("A run is in progress. Use PAUSE to stop it."), reply.error.detail);
                Assert::IsTrue   (console.GetSession().GetRunState() == RunState::DebugRun);
            }

            for (const char * line : { "R", "D 300", "BPL", "STACK", "U 300", "LBR" })
            {
                console.TypeOk (line);
            }

            reply = console.Type ("R A 41");
            Assert::AreEqual (std::string ("machine running"), reply.error.label);
            Assert::AreEqual (std::string ("R changes registers or memory. Pause the machine first."), reply.error.detail);
            Assert::AreEqual ((size_t) 2, reply.text.size());
            Assert::AreEqual (std::string ("Error: machine running"), reply.text[0]);
            Assert::AreEqual (a, console.GetRegisters().a, L"A was not changed");

            console.TypeOk ("PAUSE");
            Assert::IsTrue (console.RunSlices());
        }



        //  A run command on a machine the user left running takes the run over
        //  rather than refusing.
        TEST_METHOD (Console_G_OnAFreeRunningMachine_TakesOverTheRun)
        {
            AppleWinConsoleRig  console;



            console.TypeOk (kLoop);
            console.UserResume();
            Assert::IsTrue  (console.GetSession().GetRunState() == RunState::FreeRunning);
            Assert::IsFalse (console.RunSlices (3));

            console.TypeOk ("G 301");
            Assert::IsTrue   (console.GetSession().GetRunState() == RunState::DebugRun);
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual ((Word) 0x0301, console.GetRegisters().pc);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AppleWinRunFailureTests
    //
    //  A target whose driver cannot start a run. A real machine cannot be made
    //  to fail StartRun without tripping an assertion, so this is the mock
    //  target with a failing StartRun.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AppleWinRunFailureTests)
    {
    public:

        TEST_METHOD (ARunThatCannotStart_IsAnError_AndTheMachineStaysPaused)
        {
            RefusingDebugTarget        target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;
            Reply                      reply;



            handlers.Attach (session);

            reply = session.ExecuteLine ("G");
            session.FormatReply (reply);

            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual ((size_t) 2, reply.text.size());
            Assert::AreEqual (std::string ("Error: run failed"),                              reply.text[0]);
            Assert::AreEqual (std::string ("       The machine could not start the run."), reply.text[1]);
            Assert::IsTrue   (session.GetRunState() == RunState::Paused);
            Assert::IsTrue   (sink.stops.empty());
        }
    };
}
