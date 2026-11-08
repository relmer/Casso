#include "Pch.h"

#include "Debugger/Handlers/ExecutionHandlers.h"
#include "HandlerTestRig.h"
#include "RunPathRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileCommandTests
//
//  PROFILE through every way a user reaches it, on a real //e. The loop is
//  the quickstart's: LDA $10FF,X with X crossing the page three times in
//  four, stopped by a breakpoint set with BP. Counting depends on the
//  instruction observer DebugHandlerSet::Attach installs, which the window,
//  the channel and batch mode all go through; nothing here installs it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ProfileCommandTests)
    {
    public:

        static constexpr Word  kStart = 0x0300;

        static const std::vector<std::string> & ExpectedList()
        {
            static const std::vector<std::string>  s_lines =
            {
                "Instructions: 17, cycles: 48",
                "Opcode Mode                      Count     Cycles Percent",
                "LDA    Absolute, X                   4         16   33.3%",
                "BNE    Relative                      4          8   16.7%",
                "CPX    #Immediate                    4          8   16.7%",
                "INX    [No Operand]                  4          8   16.7%",
                "LDX    #Immediate                    1          2    4.2%",
                "Penalty                                    Cycles Percent",
                "Page crossing                                   3    6.2%",
                "Taken branches                                  3    6.2%",
                "Branches crossing a page                        0    0.0%",
            };

            return s_lines;
        }

        static const std::vector<std::string> & ExpectedAddresses()
        {
            static const std::vector<std::string>  s_lines =
            {
                "Instructions: 17, cycles: 48",
                "Address Symbol                   Cycles Percent",
                "$0302   LOOP                         19   39.6%",
                "$0308                                11   22.9%",
                "$0305                                 8   16.7%",
                "$0306                                 8   16.7%",
                "$0300                                 2    4.2%",
            };

            return s_lines;
        }

        static std::string Joined (const std::vector<std::string> & first, const std::vector<std::string> & second)
        {
            std::string  text;



            for (const std::string & line : first)  { text += line + "\n"; }
            for (const std::string & line : second) { text += line + "\n"; }

            return text;
        }

        static std::vector<std::string> LoopScript()
        {
            return { "MEB 300 A2 00 BD FF 10 E8 E0 04 D0 F8 EA", "R PC 300", "SYM LOOP = 302", "BP 30A" };
        }

        //  $0300: LDX #$00 / LOOP: LDA $10FF,X / INX / CPX #$04 / BNE LOOP / NOP,
        //  with a breakpoint on the NOP.
        static void LoadLoop (EmulatorRig & rig)
        {
            rig.Load     (kStart, { 0xA2, 0x00, 0xBD, 0xFF, 0x10, 0xE8, 0xE0, 0x04, 0xD0, 0xF8, 0xEA }, kStart);
            rig.WindowOk ("SYM LOOP = 302");
            rig.WindowOk ("BP 30A");
        }

        static std::vector<std::string> Error (const std::string & label, const std::string & detail)
        {
            return { "Error: " + label, "       " + detail };
        }

        static std::string Summary (EmulatorRig & rig)
        {
            return rig.WindowOk ("PROFILE").text.at (0);
        }



        //  The run paths.

        TEST_METHOD (Window_G_ProfilesTheRun)
        {
            EmulatorRig  rig;



            LoadLoop (rig);
            EmulatorRig::AssertLines ({ "Profiling on." }, rig.WindowOk ("PROFILE ON").text, "PROFILE ON");
            rig.WindowRun ("G");

            Assert::AreEqual (0x030A, (int) rig.Session().GetTarget().GetRegisters().pc);
            EmulatorRig::AssertLines (ExpectedList(),      rig.WindowOk ("PROFILE LIST").text,      "PROFILE LIST");
            EmulatorRig::AssertLines (ExpectedList(),      rig.WindowOk ("PROFILE").text,           "a bare PROFILE lists");
            EmulatorRig::AssertLines (ExpectedList(),      rig.WindowOk ("profile list").text,      "any case");
            EmulatorRig::AssertLines (ExpectedAddresses(), rig.WindowOk ("PROFILE LIST ADDR").text, "PROFILE LIST ADDR");
            EmulatorRig::AssertLines (ExpectedAddresses(), rig.WindowOk ("PROFILE list addr").text, "ADDR in any case");
        }



        TEST_METHOD (Channel_G_ProfilesTheRun)
        {
            EmulatorRig  rig;



            LoadLoop (rig);
            EmulatorRig::AssertLines ({ "Profiling on." }, rig.PipeText ("PROFILE ON"), "PROFILE ON from a client");
            rig.Pipe ("G");
            Assert::IsTrue (rig.Slices(), L"the client's run reached the breakpoint");

            EmulatorRig::AssertLines (ExpectedList(),      rig.PipeText ("PROFILE LIST"),      "PROFILE LIST from a client");
            EmulatorRig::AssertLines (ExpectedAddresses(), rig.PipeText ("PROFILE LIST ADDR"), "PROFILE LIST ADDR from a client");
        }



        TEST_METHOD (Batch_G_ProfilesTheRun)
        {
            BatchScript               batch;
            std::vector<std::string>  lines = LoopScript();



            lines.insert (lines.end(), { "PROFILE ON", "G", "PROFILE LIST", "PROFILE LIST ADDR", "PROFILE" });

            Assert::AreEqual (0, batch.Run (lines));
            BatchScript::AssertLines (ExpectedList(),      batch.GetReplyText (6), "PROFILE LIST in batch");
            BatchScript::AssertLines (ExpectedAddresses(), batch.GetReplyText (7), "PROFILE LIST ADDR in batch");
            BatchScript::AssertLines (ExpectedList(),      batch.GetReplyText (8), "PROFILE in batch");
        }



        //  Steps profile each instruction they run: T, P over a call, RTS out
        //  of one, the JSR command, and GG.
        TEST_METHOD (Steps_ProfileEachInstructionTheyRun)
        {
            EmulatorRig  rig;



            //  $0300: JSR $0310 / NOP    $0310: INX / INX / RTS
            rig.Load (kStart, { 0x20, 0x10, 0x03, 0xEA }, kStart);
            rig.Poke (0x0310, { 0xE8, 0xE8, 0x60 });
            rig.WindowOk ("PROFILE ON");

            rig.WindowRun ("T");
            Assert::AreEqual (std::string ("Instructions: 1, cycles: 6"), Summary (rig), L"T: the JSR");

            rig.WindowRun ("T 2");
            Assert::AreEqual (std::string ("Instructions: 3, cycles: 10"), Summary (rig), L"T 2: two INX");

            rig.WindowRun ("RTS");
            Assert::AreEqual (0x0303, (int) rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual (std::string ("Instructions: 4, cycles: 16"), Summary (rig), L"RTS: the routine's RTS");

            rig.WindowOk  ("PROFILE RESET");
            rig.SetPc     (kStart);
            rig.WindowRun ("P");
            Assert::AreEqual (0x0303, (int) rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual (std::string ("Instructions: 4, cycles: 16"), Summary (rig), L"P: the call and all it ran");

            rig.WindowOk  ("PROFILE RESET");
            rig.WindowRun ("JSR 310");
            Assert::AreEqual (0x0303, (int) rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual (std::string ("Instructions: 3, cycles: 10"), Summary (rig), L"JSR 310: the routine");

            rig.WindowOk  ("PROFILE RESET");
            rig.SetPc     (kStart);
            rig.WindowRun ("GG 303");
            Assert::AreEqual (std::string ("Instructions: 4, cycles: 16"), Summary (rig), L"GG: a full-speed run");
        }



        //  Only a debugger run with profiling on is counted. A free run is
        //  not, even with a watchpoint armed so that every instruction is
        //  reported to the session.
        TEST_METHOD (NothingIsCountedWhileOff_OrOutsideADebuggerRun)
        {
            EmulatorRig  rig;
            uint64_t     before = 0;



            LoadLoop (rig);
            rig.WindowRun ("G");
            EmulatorRig::AssertLines ({ "Instructions: 0, cycles: 0" }, rig.WindowOk ("PROFILE").text, "a run while off");

            rig.WindowOk ("PROFILE ON");
            rig.WindowOk ("BPC *");
            rig.Load     (kStart, { 0xE8, 0x4C, 0x00, 0x03 }, kStart);
            before = rig.machine.GetCpu()->GetTotalCycles();
            rig.FreeRun  (5000);
            rig.UserPause();
            Assert::IsTrue   (rig.machine.GetCpu()->GetTotalCycles() - before >= 5000, L"the machine ran");
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), Summary (rig), L"a free run");

            rig.WindowOk ("BPMW 2000");
            rig.FreeRun  (5000);
            rig.UserPause();
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), Summary (rig), L"a free run with every instruction reported");

            rig.WindowOk  ("BPC *");
            rig.WindowOk  ("BUDGET 10");
            rig.WindowRun ("G");
            Assert::AreNotEqual (std::string ("Instructions: 0, cycles: 0"), Summary (rig), L"and a debugger run still counts");
        }



        //  ON leaves the counts alone, and a second ON changes nothing. Counts
        //  add up across runs until RESET.
        TEST_METHOD (OnKeepsTheCounts_AndRunsAccumulate)
        {
            EmulatorRig  rig;



            LoadLoop (rig);
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");
            EmulatorRig::AssertLines ({ "Profiling on." }, rig.WindowOk ("PROFILE ON").text, "ON again");
            EmulatorRig::AssertLines (ExpectedList(), rig.WindowOk ("PROFILE LIST").text, "ON does not reset");

            rig.SetPc     (kStart);
            rig.WindowRun ("G");
            Assert::AreEqual (std::string ("Instructions: 34, cycles: 96"), Summary (rig), L"the second run adds to the first");
        }



        //  OFF stops counting and keeps what was counted.
        TEST_METHOD (OffKeepsTheCounts)
        {
            EmulatorRig        rig;
            JsonValue          reply;
            const JsonValue  * data  = nullptr;
            bool               isOn  = true;



            LoadLoop (rig);
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");
            EmulatorRig::AssertLines ({ "Profiling off." }, rig.WindowOk ("PROFILE OFF").text, "PROFILE OFF");
            EmulatorRig::AssertLines (ExpectedList(), rig.WindowOk ("PROFILE LIST").text, "the counts stay");

            reply = rig.Pipe ("PROFILE LIST");
            Assert::IsTrue   (reply.HasObject ("data", data));
            Assert::IsTrue   (data->HasBool ("on", isOn));
            Assert::IsFalse  (isOn, L"the JSON says it is off");

            rig.SetPc     (kStart);
            rig.WindowRun ("G");
            EmulatorRig::AssertLines (ExpectedList(), rig.WindowOk ("PROFILE LIST").text, "nothing is added while off");
        }



        //  Between the slices of an emulator run, OFF takes effect at once,
        //  and the instruction under way is not billed. NOP / JMP $0300
        //  runs 410 instructions in a 1023-cycle slice: 205 NOPs and 205
        //  JMPs, of which the last JMP is still under way.
        TEST_METHOD (OffBetweenSlices_DropsTheInstructionUnderWay)
        {
            EmulatorRig  rig;



            rig.Load     (kStart, { 0xEA, 0x4C, 0x00, 0x03 }, kStart);
            rig.WindowOk ("PROFILE ON");
            rig.WindowOk ("G");
            Assert::IsFalse (rig.Slices (1), L"the loop runs on");

            rig.WindowOk ("PROFILE OFF");
            Assert::IsFalse (rig.Slices (2), L"the loop runs on");
            rig.UserPause();

            EmulatorRig::AssertLines ({ "Instructions: 409, cycles: 1022",
                                        "Opcode Mode                      Count     Cycles Percent",
                                        "JMP    Absolute                    204        612   59.9%",
                                        "NOP    [No Operand]                205        410   40.1%",
                                        "Penalty                                    Cycles Percent",
                                        "Page crossing                                   0    0.0%",
                                        "Taken branches                                  0    0.0%",
                                        "Branches crossing a page                        0    0.0%" },
                                      rig.WindowOk ("PROFILE LIST").text, "what ran before OFF, less the JMP under way");
        }



        //  RESET between the slices of an emulator run clears what the run
        //  counted so far; the rest of the run counts from there, and the
        //  stop bills the last instruction.
        TEST_METHOD (ResetBetweenSlices_CountsTheRestOfTheRun)
        {
            EmulatorRig  rig;



            rig.Load     (kStart, { 0xEA, 0x4C, 0x00, 0x03 }, kStart);
            rig.WindowOk ("PROFILE ON");
            rig.WindowOk ("G");
            rig.Slices   (1);

            EmulatorRig::AssertLines ({ "Profile reset." }, rig.WindowOk ("PROFILE RESET").text, "PROFILE RESET");
            rig.Slices (1);
            Assert::AreEqual (std::string ("Instructions: 409, cycles: 1022"), Summary (rig), L"the second slice, less its last JMP");

            rig.UserPause();
            Assert::AreEqual (std::string ("Instructions: 410, cycles: 1025"), Summary (rig), L"the stop billed the last JMP");
        }



        //  RESET clears everything and keeps profiling on or off as it was.
        TEST_METHOD (ResetClears_AndKeepsOnOrOff)
        {
            EmulatorRig  rig;



            LoadLoop (rig);
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");
            EmulatorRig::AssertLines ({ "Profile reset." }, rig.WindowOk ("PROFILE RESET").text, "PROFILE RESET");
            EmulatorRig::AssertLines ({ "Instructions: 0, cycles: 0" }, rig.WindowOk ("PROFILE").text,           "nothing is left");
            EmulatorRig::AssertLines ({ "Instructions: 0, cycles: 0" }, rig.WindowOk ("PROFILE LIST ADDR").text, "no address either");

            rig.SetPc     (kStart);
            rig.WindowRun ("G");
            EmulatorRig::AssertLines (ExpectedList(), rig.WindowOk ("PROFILE LIST").text, "still on: the next run counts");

            rig.WindowOk  ("PROFILE OFF");
            rig.WindowOk  ("PROFILE RESET");
            rig.SetPc     (kStart);
            rig.WindowRun ("G");
            EmulatorRig::AssertLines ({ "Instructions: 0, cycles: 0" }, rig.WindowOk ("PROFILE").text, "still off");
        }



        //  Opcodes that share a mnemonic and mode are one row; one the 6502
        //  does not have is ???. The documented NOP and the undocumented $1A
        //  are both NOP with no operand.
        TEST_METHOD (Rows_MergeByMnemonicAndMode_AndUnknownOpcodesAreQuestionMarks)
        {
            EmulatorRig  rig;



            //  $0300: NOP / NOP ($1A) / ANE / NOP / NOP
            rig.Load      (kStart, { 0xEA, 0x1A, 0x8B, 0xEA, 0xEA }, kStart);
            rig.WindowOk  ("BP 304");
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");

            EmulatorRig::AssertLines ({ "Instructions: 4, cycles: 8",
                                        "Opcode Mode                      Count     Cycles Percent",
                                        "NOP    [No Operand]                  3          6   75.0%",
                                        "???    ???                           1          2   25.0%",
                                        "Penalty                                    Cycles Percent",
                                        "Page crossing                                   0    0.0%",
                                        "Taken branches                                  0    0.0%",
                                        "Branches crossing a page                        0    0.0%" },
                                      rig.WindowOk ("PROFILE LIST").text, "merged rows");
        }



        //  The address view is the twenty hottest; equal cycles go in
        //  address order.
        TEST_METHOD (Addresses_TheHottestTwenty_TiesInAddressOrder)
        {
            EmulatorRig               rig;
            std::vector<std::string>  lines;



            for (Word at = kStart; at < kStart + 25; ++at)
            {
                rig.Poke (at, { 0xEA });
            }

            rig.SetPc     (kStart);
            rig.WindowOk  ("BP 319");
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");

            lines = rig.WindowOk ("PROFILE LIST ADDR").text;
            Assert::AreEqual (std::string ("Instructions: 25, cycles: 50"), lines.at (0));
            Assert::AreEqual ((size_t) 22, lines.size(), L"the summary, the heading and twenty rows");

            for (int row = 0; row < 20; ++row)
            {
                Assert::AreEqual (std::format ("${:04X}{}2    4.0%", kStart + row, std::string (33, ' ')), lines[(size_t) row + 2]);
            }
        }



        //  SAVE writes the LIST rows and then the LIST ADDR rows, in
        //  AppleWin's text whatever the output format, to the file resolved
        //  from the current directory, over any file already there.
        TEST_METHOD (Save_WritesBothTables)
        {
            EmulatorRig  rig;
            std::string  expected = Joined (ExpectedList(), ExpectedAddresses());



            LoadLoop (rig);
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");

            EmulatorRig::AssertLines ({ "Saved the profile to Profile.txt." }, rig.WindowOk ("PROFILE SAVE").text, "the default file");
            Assert::AreEqual (expected, rig.files.PeekContent (L"Profile.txt"));

            rig.files.WriteAllText (L"hot.txt", "something else\n");
            EmulatorRig::AssertLines ({ "Saved the profile to hot.txt." }, rig.WindowOk ("PROFILE SAVE hot.txt").text, "a named file");
            Assert::AreEqual (expected, rig.files.PeekContent (L"hot.txt"), L"overwritten");

            EmulatorRig::AssertLines ({ "Saved the profile to C:\\Out\\p.txt." }, rig.WindowOk ("PROFILE SAVE C:\\Out\\p.txt").text, "a rooted path");
            Assert::AreEqual (expected, rig.files.PeekContent (L"C:\\Out\\p.txt"));

            rig.WindowOk ("CD C:\\Work");
            rig.WindowOk ("PROFILE SAVE rel.txt");
            Assert::AreEqual (expected, rig.files.PeekContent (L"C:\\Work\\rel.txt"), L"relative to the current directory");

            EmulatorRig::AssertLines ({ "Saved the profile to \"quoted.txt\"." }, rig.WindowOk ("PROFILE SAVE \"quoted.txt\"").text, "a quoted name");
            Assert::AreEqual (expected, rig.files.PeekContent (L"C:\\Work\\quoted.txt"));

            EmulatorRig::AssertLines ({ "Saved the profile to \"my profile.txt\"." }, rig.WindowOk ("PROFILE SAVE \"my profile.txt\"").text, "a quoted name with a space");
            Assert::AreEqual (expected, rig.files.PeekContent (L"C:\\Work\\my profile.txt"));

            rig.WindowOk ("MODE WINDBG");
            rig.WindowOk ("!profile save windbg.txt");
            Assert::AreEqual (expected, rig.files.PeekContent (L"C:\\Work\\windbg.txt"), L"AppleWin text in WinDbg's output");

            rig.WindowOk ("!output gssquared");
            rig.WindowOk ("!profile save gssquared.txt");
            Assert::AreEqual (expected, rig.files.PeekContent (L"C:\\Work\\gssquared.txt"), L"AppleWin text in GSSquared's output");
        }



        TEST_METHOD (Save_Failures)
        {
            EmulatorRig                         rig;
            MachineHandlerRig<ExecutionHandlers>  noFiles;
            Reply                               reply;



            rig.files.WriteAllText        (L"locked.txt", "x");
            rig.files.SetReadOnlyAttribute (L"locked.txt", true);
            reply = rig.Window ("PROFILE SAVE locked.txt");
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            EmulatorRig::AssertLines (Error ("file not written", "locked.txt could not be written."), reply.text, "a write that fails");

            reply = rig.Window ("PROFILE SAVE a.txt b.txt");
            EmulatorRig::AssertLines (Error ("invalid arguments", "PROFILE takes ON, OFF, RESET, LIST [ADDR] or SAVE [file]."), reply.text, "two names");

            //  No session a user reaches is without host files; this one is
            //  built without them to reach the message.
            noFiles.session.SetFileSystem (nullptr);
            reply = noFiles.Run ("PROFILE SAVE");
            EmulatorRig::AssertLines (Error ("no file access", "This session cannot read or write host files."), reply.text, "no file system");
        }



        //  Every other form is the usage error.
        TEST_METHOD (BadForms_AreTheUsageError)
        {
            EmulatorRig  rig;



            for (const char * line : { "PROFILE SIDEWAYS", "PROFILE LIST SIDEWAYS", "PROFILE ON X", "PROFILE OFF X", "PROFILE RESET X",
                                       "PROFILE SAVE a b", "PROFILE ADDR", "PROFILE LIST ADDR X" })
            {
                Reply  reply = rig.Window (line);



                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, Widen (line).c_str());
                EmulatorRig::AssertLines (Error ("invalid arguments", "PROFILE takes ON, OFF, RESET, LIST [ADDR] or SAVE [file]."), reply.text, line);
            }

            rig.WindowOk ("MODE MONITOR");
            EmulatorRig::AssertLines ({ "ERR", "Error: invalid arguments", "       PROFILE takes ON, OFF, RESET, LIST [ADDR] or SAVE [file]." },
                                      rig.Window ("/PROFILE SIDEWAYS").text, "in the Monitor's output");
        }



        //  Page crossings, taken branches and branches that cross a page
        //  are billed apart from the base cycles. The BNE at $03FF goes back
        //  to $03FC, across a page, three times.
        TEST_METHOD (Penalties_AreBilledApart_IncludingABranchAcrossAPage)
        {
            EmulatorRig  rig;



            //  $03FA: LDX #$00 / INX / CPX #$04 / BNE $03FC / NOP
            rig.Load      (0x03FA, { 0xA2, 0x00, 0xE8, 0xE0, 0x04, 0xD0, 0xFB, 0xEA }, 0x03FA);
            rig.WindowOk  ("BP 401");
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");

            EmulatorRig::AssertLines ({ "Instructions: 13, cycles: 32",
                                        "Opcode Mode                      Count     Cycles Percent",
                                        "BNE    Relative                      4          8   25.0%",
                                        "CPX    #Immediate                    4          8   25.0%",
                                        "INX    [No Operand]                  4          8   25.0%",
                                        "LDX    #Immediate                    1          2    6.2%",
                                        "Penalty                                    Cycles Percent",
                                        "Page crossing                                   0    0.0%",
                                        "Taken branches                                  3    9.4%",
                                        "Branches crossing a page                        3    9.4%" },
                                      rig.WindowOk ("PROFILE LIST").text, "a branch across a page");
        }



        //  The same program profiles differently on the two CPUs. The NMOS
        //  ASL abs,X pays its extra cycle in its base; the 65C02 pays it only
        //  on a crossing, and bills it as one. An indexed store never bills a
        //  crossing. BRA's cycle is a taken branch's.
        TEST_METHOD (Penalties_FollowTheCpu)
        {
            for (const auto & [machine, expected] : std::vector<std::pair<std::string, std::vector<std::string>>>
                 {
                     { "Apple2e",         { "Instructions: 3, cycles: 14",
                                            "Opcode Mode                      Count     Cycles Percent",
                                            "ASL    Absolute, X                   1          7   50.0%",
                                            "STA    Absolute, X                   1          5   35.7%",
                                            "LDX    #Immediate                    1          2   14.3%",
                                            "Penalty                                    Cycles Percent",
                                            "Page crossing                                   0    0.0%",
                                            "Taken branches                                  0    0.0%",
                                            "Branches crossing a page                        0    0.0%" } },
                     { "Apple2eEnhanced", { "Instructions: 3, cycles: 14",
                                            "Opcode Mode                      Count     Cycles Percent",
                                            "ASL    Absolute, X                   1          6   42.9%",
                                            "STA    Absolute, X                   1          5   35.7%",
                                            "LDX    #Immediate                    1          2   14.3%",
                                            "Penalty                                    Cycles Percent",
                                            "Page crossing                                   1    7.1%",
                                            "Taken branches                                  0    0.0%",
                                            "Branches crossing a page                        0    0.0%" } },
                 })
            {
                BatchScript  batch (machine);



                Assert::AreEqual (0, batch.Run ({ "MEB 300 A2 01 1E FF 10 9D FF 10 EA", "R PC 300", "BP 308", "PROFILE ON", "G", "PROFILE LIST" }));
                BatchScript::AssertLines (expected, batch.GetReplyText (5), machine);
            }

            {
                BatchScript  batch ("Apple2eEnhanced");



                //  $0300: LDX #$00 / INX / CPX #$03 / BEQ $0309 / BRA $0302 / NOP
                Assert::AreEqual (0, batch.Run ({ "MEB 300 A2 00 E8 E0 03 F0 02 80 F9 EA", "R PC 300", "BP 309", "PROFILE ON", "G", "PROFILE LIST" }));
                BatchScript::AssertLines ({ "Instructions: 12, cycles: 30",
                                            "Opcode Mode                      Count     Cycles Percent",
                                            "BEQ    Relative                      3          6   20.0%",
                                            "CPX    #Immediate                    3          6   20.0%",
                                            "INX    [No Operand]                  3          6   20.0%",
                                            "BRA    Relative                      2          4   13.3%",
                                            "LDX    #Immediate                    1          2    6.7%",
                                            "Penalty                                    Cycles Percent",
                                            "Page crossing                                   0    0.0%",
                                            "Taken branches                                  3   10.0%",
                                            "Branches crossing a page                        0    0.0%" },
                                          batch.GetReplyText (5), "BRA's cycle is a taken branch's");
            }
        }



        //  An interrupt dispatched in place of an instruction is not that
        //  instruction: it is billed once, when it runs after the handler
        //  returns, and the dispatch's cycles are billed to nothing. The
        //  machine has no device raising the line, so the test asserts it.
        TEST_METHOD (AnInterrupt_IsNotBilledToTheInstructionItReplaced)
        {
            EmulatorRig               rig;
            uint64_t                  start  = 0;
            std::vector<std::string>  lines;



            //  $0300: NOP / NOP / JMP $0301, with interrupts enabled.
            rig.Load     (kStart, { 0xEA, 0xEA, 0x4C, 0x01, 0x03 }, kStart, 0x30);
            rig.Poke     (0x0400, { 0xEA, 0x40 });
            rig.Poke     (0x03FE, { 0x00, 0x04 });
            rig.WindowOk ("PROFILE ON");
            start = rig.machine.GetCpu()->GetTotalCycles();

            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, true);
            rig.WindowRun ("T");
            Assert::AreNotEqual (0x0301, (int) rig.Session().GetTarget().GetRegisters().pc, L"the interrupt was taken in place of the NOP");
            Assert::AreEqual (std::string ("Instructions: 0, cycles: 0"), Summary (rig), L"no instruction ran");

            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, false);
            rig.WindowRun ("RTS");
            Assert::AreEqual (0x0300, (int) rig.Session().GetTarget().GetRegisters().pc, L"back at the NOP the interrupt replaced");
            rig.WindowRun ("T");

            lines = rig.WindowOk ("PROFILE LIST").text;
            Assert::AreEqual (std::format ("Instructions: {}, cycles: {}", CountInstructions (lines), rig.machine.GetCpu()->GetTotalCycles() - start - 7),
                              lines.at (0), L"every cycle but the dispatch's");
            Assert::IsTrue   (GetRow (lines, "NOP    [No Operand]") == std::pair<uint64_t, uint64_t> (2, 4),
                              L"the replaced NOP counted once, beside the handler's NOP");
        }



        //  A power cycle between slices zeroes the machine's count under the
        //  instruction under way; its cost is unknown, and it is not billed.
        //  Every NOP row then holds two cycles a NOP, and every JMP three.
        TEST_METHOD (APowerCycleMidRun_BillsNothingForTheInstructionUnderWay)
        {
            EmulatorRig               rig;
            std::vector<std::string>  lines;



            rig.Load     (kStart, { 0xEA, 0x4C, 0x00, 0x03 }, kStart);
            rig.WindowOk ("PROFILE ON");
            rig.WindowOk ("G");
            rig.Slices   (1);
            rig.PowerCycle();
            rig.Slices   (1);
            rig.UserPause();

            lines = rig.WindowOk ("PROFILE LIST").text;
            AssertRowCostsPerInstruction (lines, "NOP    [No Operand]", 2);
            AssertRowCostsPerInstruction (lines, "JMP    Absolute",     3);
        }



        //  The counts belong to the session: the window and every client share
        //  them, and they outlive a machine switch.
        TEST_METHOD (TheCountsAreTheSessions)
        {
            EmulatorRig  rig;



            LoadLoop (rig);
            rig.Pipe      ("PROFILE ON");
            rig.WindowRun ("G");
            EmulatorRig::AssertLines (ExpectedList(), rig.PipeText ("PROFILE LIST"), "the client sees the window's run");

            rig.Session().OnMachineChanged ("Apple //e");
            EmulatorRig::AssertLines (ExpectedList(), rig.WindowOk ("PROFILE LIST").text, "the counts survive a machine switch");
        }



        //  Each dialect's form, each in its output: AppleWin text for all.
        TEST_METHOD (EveryDialect)
        {
            EmulatorRig  rig;



            LoadLoop (rig);

            rig.WindowOk ("MODE MONITOR");
            EmulatorRig::AssertLines ({ "Profiling on." }, rig.WindowOk ("/PROFILE ON").text, "/PROFILE ON");
            rig.WindowRun ("/G");
            EmulatorRig::AssertLines (ExpectedList(),      rig.WindowOk ("/PROFILE LIST").text,      "/PROFILE LIST");
            EmulatorRig::AssertLines (ExpectedAddresses(), rig.WindowOk ("/profile list addr").text, "/profile list addr");

            rig.WindowOk ("/MODE GSSQUARED");
            EmulatorRig::AssertLines (ExpectedList(), rig.WindowOk ("profile").text, "profile");
            EmulatorRig::AssertLines ({ "Saved the profile to hot.txt." }, rig.WindowOk ("profile save hot.txt").text, "profile save");
            Assert::AreEqual (Joined (ExpectedList(), ExpectedAddresses()), rig.files.PeekContent (L"hot.txt"));
            EmulatorRig::AssertLines ({ "Profile reset." }, rig.WindowOk ("profile reset").text, "profile reset");

            rig.WindowOk ("mode windbg");
            rig.SetPc    (kStart);
            rig.WindowRun ("g");
            EmulatorRig::AssertLines (ExpectedList(), rig.WindowOk ("!profile list").text, "!profile list");
            EmulatorRig::AssertLines ({ "Profiling off." }, rig.WindowOk ("!profile off").text, "!profile off");

            rig.WindowOk ("!mode casso");
            EmulatorRig::AssertLines (ExpectedAddresses(), rig.WindowOk ("PROFILE LIST ADDR").text, "Casso mode");
            EmulatorRig::AssertLines (ExpectedList(), rig.PipeText ("!profile", "windbg"), "a client asking in WinDbg mode");
        }



        //  As JSON, over the channel.
        TEST_METHOD (Json_CarriesTheRowsPenaltiesAndAddresses)
        {
            EmulatorRig        rig;
            JsonValue          reply;
            const JsonValue  * data      = nullptr;
            const JsonValue  * penalties = nullptr;
            const JsonValue  * addresses = nullptr;
            const JsonValue  * opcodes   = nullptr;
            const JsonValue  * symbol    = nullptr;
            bool               isOn      = false;
            int                value     = 0;



            LoadLoop (rig);
            rig.WindowOk  ("PROFILE ON");
            rig.WindowRun ("G");
            reply = rig.Pipe ("PROFILE LIST ADDR");

            Assert::IsTrue   (reply.HasObject ("data", data));
            Assert::AreEqual (std::string ("profile"), EmulatorRig::GetString (*data, "kind"));
            Assert::IsTrue   (data->HasBool ("on", isOn) && isOn);
            Assert::IsTrue   (data->HasInt ("instructions", value) && value == 17);
            Assert::IsTrue   (data->HasInt ("cycles", value) && value == 48);
            Assert::IsTrue   (data->HasObject ("penalties", penalties));
            Assert::IsTrue   (penalties->HasInt ("pageCross", value) && value == 3);
            Assert::IsTrue   (penalties->HasInt ("branchTaken", value) && value == 3);
            Assert::IsTrue   (penalties->HasInt ("branchCross", value) && value == 0);
            Assert::IsTrue   (data->HasArray ("opcodes", opcodes));
            Assert::AreEqual ((size_t) 5, opcodes->GetArraySize());
            Assert::AreEqual (std::string ("LDA"), EmulatorRig::GetString (opcodes->GetArrayElement (0), "mnemonic"));
            Assert::IsTrue   (data->HasArray ("addresses", addresses));
            Assert::AreEqual ((size_t) 5, addresses->GetArraySize());
            Assert::AreEqual (std::string ("LOOP"), EmulatorRig::GetString (addresses->GetArrayElement (0), "symbol"));
            symbol = EmulatorRig::FindMember (addresses->GetArrayElement (1), "symbol");
            Assert::IsNotNull (symbol);
            Assert::IsTrue    (symbol->GetType() == JsonType::Null, L"an address with no symbol");
        }



        //  Profiling rides the debugger's own runs: switching it installs no
        //  hook on the machine.
        TEST_METHOD (OnAndOff_InstallNoHook)
        {
            EmulatorRig  rig;



            rig.WindowOk ("PROFILE ON");
            Assert::IsNull (rig.machine.GetDebugHook());
            rig.WindowOk ("PROFILE OFF");
            Assert::IsNull (rig.machine.GetDebugHook());
        }



        //  One instruction is billed at most 255 cycles. No instruction costs
        //  that much, so the mock target stands in for a clock that jumps.
        TEST_METHOD (OneInstruction_IsBilledAtMost255Cycles)
        {
            HandlerRig<ExecutionHandlers>  rig;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.RunOk ("PROFILE ON");
            rig.RunOk ("G");
            rig.session.OnInstruction (0x0300);
            rig.target.cycleCount = 1000;
            rig.session.OnInstruction (0x0301);
            rig.target.Stop (StopEvent { StopReason::Pause, 0x0301 });

            Assert::AreEqual (std::string ("Instructions: 2, cycles: 255"), rig.RunOk ("PROFILE").text.at (0),
                              L"the first billed 255, and the second nothing: the clock stood still");
        }

    private:

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }

        //  An opcode row's count and cycles, by its mnemonic and mode.
        static constexpr size_t  kCountColumn  = 28;
        static constexpr size_t  kCyclesColumn = 39;
        static constexpr size_t  kNumberWidth  = 10;

        static std::pair<uint64_t, uint64_t> GetRow (const std::vector<std::string> & lines, const std::string & row)
        {
            for (const std::string & line : lines)
            {
                if (line.starts_with (row))
                {
                    return { std::stoull (line.substr (kCountColumn, kNumberWidth)), std::stoull (line.substr (kCyclesColumn, kNumberWidth)) };
                }
            }

            Assert::Fail (Widen ("no row " + row).c_str());
            return {};
        }

        static uint64_t CountInstructions (const std::vector<std::string> & lines)
        {
            uint64_t  count = 0;



            for (size_t i = 2; i < lines.size() && !lines[i].starts_with ("Penalty"); ++i)
            {
                count += std::stoull (lines[i].substr (kCountColumn, kNumberWidth));
            }

            return count;
        }

        static void AssertRowCostsPerInstruction (const std::vector<std::string> & lines, const std::string & row, uint64_t each)
        {
            std::pair<uint64_t, uint64_t>  counts = GetRow (lines, row);



            Assert::AreEqual (counts.first * each, counts.second, Widen (row).c_str());
        }
    };
}
