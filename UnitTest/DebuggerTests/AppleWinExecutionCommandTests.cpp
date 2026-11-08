#include "Pch.h"

#include "AppleWinRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinExecutionCommandTests
//
//  NOP and ZAP, KEY, LBR, BPV, VIDEOINFO, CYCLES and RCC, the benchmark
//  names, TF and PROFILE, on a real machine: in a batch script, and in the
//  window with the machine run by its frame loop or left running by the user.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleWinExecutionCommandTests)
    {
    public:

        static std::string GetString (const JsonValue & value, const std::string & key)
        {
            std::string  text;



            Assert::IsTrue (value.HasString (key, text), AppleWinBatchRig::Widen (key).c_str());
            return text;
        }

        static uint64_t ParseCycles (const std::string & line)
        {
            Assert::IsTrue (line.starts_with ("Cycles: "), AppleWinBatchRig::Widen (line).c_str());
            return std::stoull (line.substr (8));
        }



        //  Every byte of the instruction at PC becomes $EA, and nothing past
        //  it: a three-byte, a two-byte and a one-byte instruction.
        TEST_METHOD (NOP_And_ZAP_OverwriteExactlyTheInstructionAtPc)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ "MEB 300 AD 00 04 60", "MEB 310 A9 41 60",
                                            "R PC 300", "NOP", "R PC 310", "ZAP", "R PC 312", "zap", "NOP 1234" }));

            rig.ExpectBlock (3, { "0300: EA       NOP", "0301: EA       NOP", "0302: EA       NOP" });
            rig.ExpectBlock (5, { "0310: EA       NOP", "0311: EA       NOP" });
            rig.ExpectBlock (7, { "0312: EA       NOP" });

            for (Word address : { 0x0300, 0x0301, 0x0302, 0x0310, 0x0311, 0x0312 })
            {
                Assert::AreEqual ((Byte) 0xEA, rig.PeekRam (address), std::format (L"${:04X}", address).c_str());
            }

            Assert::AreEqual ((Byte) 0x60, rig.PeekRam (0x0303), L"the byte after the instruction is kept");

            //  NOP takes no argument, and like AppleWin ignores one: it
            //  overwrites the instruction at PC, not at $1234.
            rig.ExpectBlock (8, { "0312: EA       NOP" });
            Assert::AreNotEqual ((Byte) 0xEA, rig.PeekRam (0x1234));
        }



        //  The ROM cannot take a NOP, and nothing is written.
        TEST_METHOD (NOP_OnRom_IsAnError)
        {
            AppleWinBatchRig  rig;
            Byte              before = 0;



            Assert::AreEqual (1, rig.Run ({ "R PC F800", "NOP" }));
            rig.ExpectBlock (1, { "Error: memory not writable", "       $F800 cannot be written." });

            before = rig.PeekRam (0xF800);
            Assert::AreNotEqual ((Byte) 0xEA, before);
        }



        //  The first key reaches the keyboard at once; the rest wait and are
        //  fed as the program clears the strobe during the run.
        TEST_METHOD (KEY_FeedsEveryKey_AsTheProgramReadsThem)
        {
            AppleWinBatchRig  rig;



            //  $0300 LDA $C000 / BPL $0300 / STA $C010 / STA $1000,X / INX /
            //  CPX #3 / BNE $0300; $0310 JMP $0310.
            Assert::AreEqual (1, rig.Run ({ "MEB 300 AD 00 C0 10 FB 8D 10 C0 9D 00 10 E8 E0 03 D0 F0 4C 10 03",
                                            "R PC 300", "R X 0", "BP 310",
                                            "KEY 41 42 43", "IN C000", "G",
                                            "KEY", "KEY 100" }));

            rig.ExpectBlock (4, { "Queued 3 keys. Keys waiting: 2." });
            rig.ExpectBlock (5, { "C000: C1                       A" });
            rig.ExpectBlock (6, { "Breakpoint #0 at $0310" });
            rig.ExpectBlock (7, { "Error: invalid arguments", "       KEY takes one or more key codes." });
            rig.ExpectBlock (8, { "Error: invalid arguments", "       100 is not a byte." });

            Assert::AreEqual ((Byte) 0xC1, rig.PeekRam (0x1000));
            Assert::AreEqual ((Byte) 0xC2, rig.PeekRam (0x1001));
            Assert::AreEqual ((Byte) 0xC3, rig.PeekRam (0x1002));
        }



        //  The queue is fed only by a debugger run, as its comment says: on a
        //  machine the user left running the first key arrives and the rest
        //  wait for the next G or T.
        TEST_METHOD (KEY_OnAFreeRunningMachine_DeliversOnlyTheFirstKey)
        {
            AppleWinConsoleRig  console;



            console.TypeOk ("MEB 300 AD 00 C0 10 FB 8D 10 C0 9D 00 10 E8 E0 03 D0 F0 4C 10 03");
            console.TypeOk ("F 1000:1002 0");
            console.TypeOk ("R X 0");
            console.UserResume();

            Assert::AreEqual (std::string ("Queued 3 keys. Keys waiting: 2."), console.TypeOk ("KEY 41 42 43").text.at (0));
            console.RunSlices (40);
            console.UserPause();

            Assert::AreEqual ((Byte) 0xC1, console.Peek (0x1000));
            Assert::AreEqual ((Byte) 0x00, console.Peek (0x1001));

            console.TypeOk ("BP 310");
            console.TypeOk ("G");
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual ((Byte) 0xC2, console.Peek (0x1001), L"the next run fed the rest");
            Assert::AreEqual ((Byte) 0xC3, console.Peek (0x1002));
        }



        //  LBR reads the CPU's own record, so a batch run with no stop
        //  condition, and a machine the user simply ran, both leave it.
        TEST_METHOD (LBR_IsTheLastTransfer_AfterAnyRun)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (3, rig.Run ({ "MEB 300 E8 4C 00 03", "R PC 300", "BUDGET 50", "G", "LBR" }));
            rig.ExpectBlock (4, { "Last branch at $0301" });
        }



        TEST_METHOD (LBR_AfterTheUserRanTheMachine)
        {
            AppleWinConsoleRig  console;
            Reply               reply;



            Assert::AreEqual (std::string ("No branch recorded."), console.TypeOk ("LBR").text.at (0), L"nothing has run yet");

            console.TypeOk ("MEB 300 E8 4C 00 03");
            console.UserResume();
            console.RunSlices (5);
            console.UserPause();

            reply = console.TypeOk ("LBR");
            Assert::AreEqual (std::string ("Last branch at $0301"), reply.text.at (0));
        }



        TEST_METHOD (LBR_AsJson_IsTheAddress_OrNull)
        {
            AppleWinConsoleRig        console;
            std::vector<std::string>  records;
            JsonValue                 reply;
            const JsonValue         * data    = nullptr;
            int                       value   = 0;



            records = console.Send (R"({"type":"command","id":1,"line":"LBR"})");
            reply   = AppleWinConsoleRig::Parse (records.at (0));
            Assert::IsTrue   (reply.HasObject ("data", data));
            Assert::AreEqual (std::string ("branchRecord"), GetString (*data, "kind"));
            Assert::IsTrue   (AppleWinJson::IsNull (*data, "address"), L"present and null when there is no branch");

            console.TypeOk ("MEB 300 E8 4C 00 03");
            console.TypeOk ("T 2");
            console.RunSlices();

            records = console.Send (R"({"type":"command","id":2,"line":"LBR"})");
            reply   = AppleWinConsoleRig::Parse (records.at (0));
            Assert::IsTrue   (reply.HasObject ("data", data));
            Assert::IsTrue   (data->HasInt ("address", value));
            Assert::AreEqual (0x0301, value);
        }



        //  BPV arms one stop on the scanlines, in hex; BPL does not list it,
        //  and it stops once.
        TEST_METHOD (BPV_StopsOnceWhenTheScanlineEntersTheRange)
        {
            AppleWinBatchRig  rig;
            std::string       info;



            Assert::AreEqual (3, rig.Run ({ "MEB 300 E8 4C 00 03", "R PC 300", "BPV A0", "BPL", "G", "VIDEOINFO", "BUDGET 20000", "G" }));

            rig.ExpectBlock (2, { "Breakpoint set on video scanlines 160-160. It clears after it fires." });
            rig.ExpectBlock (3, { "No breakpoints." });
            Assert::AreEqual ((size_t) 1, rig.GetBlock (4).size());
            Assert::IsTrue   (rig.GetBlock (4)[0].starts_with ("Breakpoint at $030"), AppleWinBatchRig::Widen (rig.GetBlock (4)[0]).c_str());

            info = rig.GetBlock (5).at (0);
            Assert::IsTrue (info.starts_with ("Scanline 160, cycle "), AppleWinBatchRig::Widen (info).c_str());

            //  A frame later the beam passes line 160 again, and nothing stops.
            Assert::IsTrue (rig.GetBlock (7).at (0).starts_with ("Budget at $030"), AppleWinBatchRig::Widen (rig.GetBlock (7)[0]).c_str());
        }



        TEST_METHOD (BPV_Forms_AndErrors)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (1, rig.Run ({ "BPV A0,10", "bpv 10:1F", "BPV", "BPV A0 IF A=1", "BPV A0 B0" }));

            rig.ExpectBlock (0, { "Breakpoint set on video scanlines 160-175. It clears after it fires." });
            rig.ExpectBlock (1, { "Breakpoint set on video scanlines 16-31. It clears after it fires." });
            rig.ExpectBlock (2, { "Error: invalid arguments", "       BPV needs an address." });
            rig.ExpectBlock (3, { "Error: invalid arguments", "       BPV takes one address or range, and nothing after it." });
            rig.ExpectBlock (4, { "Error: invalid arguments", "       BPV takes one address or range, and nothing after it." });
        }



        //  VIDEOINFO reads the machine's video timing; the JSON carries the
        //  same two numbers.
        TEST_METHOD (VIDEOINFO_IsTheBeamPosition)
        {
            AppleWinBatchRig         rig;
            std::vector<JsonValue>   replies;
            const JsonValue        * data     = nullptr;
            int                      scanline = 0;
            int                      cycle    = 0;



            rig.options.json = true;
            Assert::AreEqual (0, rig.Run ({ "MEB 300 E8 4C 00 03", "R PC 300", "BPV 20", "G", "VIDEOINFO" }));

            replies = rig.GetReplies();
            Assert::IsTrue   (replies[4].HasObject ("data", data));
            Assert::AreEqual (std::string ("videoInfo"), GetString (*data, "kind"));
            Assert::IsTrue   (data->HasInt ("scanline", scanline));
            Assert::IsTrue   (data->HasInt ("cycleInLine", cycle));
            Assert::AreEqual (0x20, scanline, L"where BPV stopped the run");
            Assert::IsTrue   (cycle >= 0 && cycle < 65);
            Assert::AreEqual (std::format ("Scanline {}, cycle {}", scanline, cycle), AppleWinJson::GetArrayMember (replies[4], "text").GetArrayElement (0).GetString());
        }



        //  CYCLES and CYCLES ABS are the machine's count, REL the last run's,
        //  PART the count since RCC.
        TEST_METHOD (CYCLES_AndRCC_OnARealMachine)
        {
            AppleWinBatchRig  rig;
            uint64_t          abs     = 0;
            uint64_t          later   = 0;



            Assert::AreEqual (1, rig.Run ({ "F 300:3FF E8", "R PC 300", "RCC", "T", "CYCLES PART", "CYCLES REL", "T 2",
                                            "cycles part", "CYCLES REL", "CYCLES", "CYCLES ABS", "CYCLES FROB" }));

            rig.ExpectBlock (2,  { "Cycle counter reset. CYCLES PART now counts from here." });
            rig.ExpectBlock (4,  { "Cycles: 2" });
            rig.ExpectBlock (5,  { "Cycles: 2" });
            rig.ExpectBlock (7,  { "Cycles: 6" });
            rig.ExpectBlock (8,  { "Cycles: 4" });
            rig.ExpectBlock (11, { "Error: invalid arguments", "       CYCLES takes ABS, REL or PART." });

            abs   = ParseCycles (rig.GetBlock (9).at (0));
            later = ParseCycles (rig.GetBlock (10).at (0));
            Assert::AreEqual (abs, later, L"CYCLES is CYCLES ABS");
            Assert::IsTrue   (abs >= 6);
        }



        //  REL is the last run's count however the run was made: here by the
        //  window's frame loop.
        TEST_METHOD (CYCLES_REL_AfterAnEmulatorRun)
        {
            AppleWinConsoleRig        console;
            std::vector<std::string>  records;
            int                       cycles = 0;



            console.TypeOk ("MEB 300 E8 4C 00 03");
            console.TypeOk ("BUDGET 3000");
            console.TakeRecords();
            console.TypeOk ("G");
            Assert::IsTrue (console.RunSlices());

            for (const std::string & record : console.TakeRecords())
            {
                JsonValue  root = AppleWinConsoleRig::Parse (record);



                root.HasInt ("cycles", cycles);
            }

            Assert::IsTrue   (cycles >= 3000);
            Assert::AreEqual (std::format ("Cycles: {}", cycles), console.TypeOk ("CYCLES REL").text.at (0));
        }



        TEST_METHOD (Benchmark_NamesAreNotAvailable)
        {
            AppleWinBatchRig  rig;



            Assert::AreEqual (0, rig.Run ({ "BENCHMARK", "BENCH", "bench", "EXITBENCH" }));

            rig.ExpectBlock (0, { "Error: command not available", "       BENCHMARK is not available in this session." });
            rig.ExpectBlock (1, { "Error: command not available", "       BENCH is not available in this session." });
            rig.ExpectBlock (2, { "Error: command not available", "       bench is not available in this session." });
            rig.ExpectBlock (3, { "Error: command not available", "       EXITBENCH is not available in this session." });
        }



        //  TF writes one line per instruction of a debugger run: the cycle
        //  count before it, the registers, the flags and the disassembly.
        TEST_METHOD (TF_WritesOneLinePerInstruction_OfEachRun)
        {
            AppleWinBatchRig          rig;
            std::string               written;
            uint64_t                  start = 0;
            std::vector<std::string>  lines;



            Assert::AreEqual (0, rig.Run ({ "F 300:3FF E8", "R PC 300", "R X 0", "CYCLES", "TF", "T 3", "TF" }));

            start = ParseCycles (rig.GetBlock (3).at (0));
            rig.ExpectBlock (4, { "Trace on: Trace.txt" });
            rig.ExpectBlock (6, { "Trace off: Trace.txt" });

            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"C:\\Work\\Trace.txt", written));
            Assert::AreEqual (std::format ("{:08} A=00 X=00 Y=00 SP=FD ..R..I..  0300: INX \n"
                                           "{:08} A=00 X=01 Y=00 SP=FD ..R..I..  0301: INX \n"
                                           "{:08} A=00 X=02 Y=00 SP=FD ..R..I..  0302: INX \n",
                                           start, start + 2, start + 4),
                              written);
        }



        //  With V the beam position replaces the cycle count; a quoted name
        //  keeps its quotes in the reply and none in the file's name.
        TEST_METHOD (TF_VideoForm_AndAQuotedName)
        {
            AppleWinBatchRig  rig;
            std::string       written;



            Assert::AreEqual (0, rig.Run ({ "F 300:3FF E8", "R PC 300", "TF \"t.txt\" v", "T", "TF" }));

            rig.ExpectBlock (2, { "Trace on: \"t.txt\"" });
            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"C:\\Work\\t.txt", written));
            Assert::IsTrue   (written.size() > 10 && written[0] == 'V' && written[4] == ' ' && written[5] == 'H' && written[8] == ' ',
                              AppleWinBatchRig::Widen (written).c_str());
            Assert::IsTrue   (written.ends_with (" 0300: INX \n"), AppleWinBatchRig::Widen (written).c_str());
        }



        //  PROFILE counts what debugger runs execute, lists by mnemonic and
        //  address, saves both, resets, and stops counting when off.
        TEST_METHOD (PROFILE_EveryForm)
        {
            AppleWinBatchRig  rig;
            std::string       saved;



            Assert::AreEqual (1, rig.Run ({ "F 300:3FF E8", "R PC 300",
                                            "PROFILE ON", "T 3", "PROFILE", "PROFILE LIST", "PROFILE LIST ADDR",
                                            "PROFILE SAVE", "PROFILE SAVE p.txt", "PROFILE OFF", "T 3", "PROFILE LIST",
                                            "PROFILE RESET", "PROFILE", "PROFILE FROB", "PROFILE ON NOW", "PROFILE LIST ADDR X" }));

            rig.ExpectBlock (2, { "Profiling on." });
            Assert::AreEqual (std::string ("Instructions: 3, cycles: 6"), rig.GetBlock (4).at (0));
            Assert::AreEqual (std::string ("Opcode Mode                      Count     Cycles Percent"), rig.GetBlock (4).at (1));
            Assert::IsTrue   (rig.GetBlock (4).at (2).starts_with ("INX    "));
            Assert::IsTrue   (rig.GetBlock (4).at (2).ends_with ("         3          6  100.0%"), AppleWinBatchRig::Widen (rig.GetBlock (4).at (2)).c_str());
            Assert::IsTrue   (rig.GetBlock (4) == rig.GetBlock (5), L"PROFILE is PROFILE LIST");

            Assert::AreEqual (std::string ("Address Symbol                   Cycles Percent"), rig.GetBlock (6).at (1));
            Assert::AreEqual ((size_t) 5, rig.GetBlock (6).size(), L"the summary, the heading and three addresses");
            Assert::IsTrue   (rig.GetBlock (6).at (2).starts_with ("$0300"));

            rig.ExpectBlock (7, { "Saved the profile to Profile.txt." });
            rig.ExpectBlock (8, { "Saved the profile to p.txt." });
            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"C:\\Work\\Profile.txt", saved));
            Assert::IsTrue   (saved.starts_with ("Instructions: 3, cycles: 6\n"));
            Assert::IsTrue   (rig.files.Exists (L"C:\\Work\\p.txt"));

            rig.ExpectBlock (9, { "Profiling off." });
            Assert::AreEqual (std::string ("Instructions: 3, cycles: 6"), rig.GetBlock (11).at (0), L"off counts nothing");

            rig.ExpectBlock (12, { "Profile reset." });
            rig.ExpectBlock (13, { "Instructions: 0, cycles: 0" });

            for (size_t i : { 14, 15, 16 })
            {
                rig.ExpectBlock (i, { "Error: invalid arguments", "       PROFILE takes ON, OFF, RESET, LIST [ADDR] or SAVE [file]." });
            }
        }



        //  As documented, only debugger-driven runs are counted: a machine the
        //  user runs adds nothing, and a run the window starts does.
        TEST_METHOD (PROFILE_CountsDebuggerRuns_NotAMachineLeftRunning)
        {
            AppleWinConsoleRig  console;



            console.TypeOk ("MEB 300 E8 4C 00 03");
            console.TypeOk ("PROFILE ON");
            console.UserResume();
            console.RunSlices (5);
            console.UserPause();
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), console.TypeOk ("PROFILE").text.at (0));

            console.TypeOk ("R PC 300");
            console.TypeOk ("T 4");
            Assert::IsTrue   (console.RunSlices());
            Assert::AreEqual (std::string ("Instructions: 4, cycles: 10"), console.TypeOk ("PROFILE").text.at (0));
        }



        //  With no file system, the file commands say so. No production
        //  session lacks one, so this is a bare session on the mock target.
        TEST_METHOD (FileCommands_WithNoFileSystem)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;



            handlers.Attach (session);

            for (const char * line : { "TF", "PROFILE SAVE", "BSAVE out.bin 300", "BLOAD in.bin 300", "TSAVE s.txt", "BPSAVE b.txt", "WSAVE w.txt", "SAVE all.txt", "RUN s.txt" })
            {
                Reply  reply = session.ExecuteLine (line);



                Assert::AreEqual (std::string ("no file access"), reply.error.label, AppleWinBatchRig::Widen (line).c_str());
                Assert::AreEqual (std::string ("This session cannot read or write host files."), reply.error.detail, AppleWinBatchRig::Widen (line).c_str());
            }
        }
    };
}
