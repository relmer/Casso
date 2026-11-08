#include "Pch.h"

#include "EntryPointRig.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SaveConfigTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SaveConfigTests
    //
    //  SAVE writes the breakpoints, watches, zero-page pointers and bookmarks
    //  as one file of AppleWin commands, and LOAD of that file puts them back.
    //  Every table is filled the way a user fills it, by typing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SaveConfigTests)
    {
    public:

        static void TypeAll (ConsoleRig & rig, std::initializer_list<const char *> lines)
        {
            for (const char * line : lines)
            {
                Reply  reply = rig.Type (line);



                Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (reply.status),
                                  ReplyCheck::Widen (std::string (line) + ": " + reply.error.detail).c_str());
            }
        }


        static std::string ListAll (ConsoleRig & rig)
        {
            return ReplyCheck::Join (rig.Type ("BPL").text) + "\n" + ReplyCheck::Join (rig.Type ("WL").text) + "\n" +
                   ReplyCheck::Join (rig.Type ("ZPL").text) + "\n" + ReplyCheck::Join (rig.Type ("BML").text);
        }


        static void FillEveryTable (ConsoleRig & rig)
        {
            TypeAll (rig, { "BP 300", "BP 310:31F IF A == 1", "BPMR 400 BEFORE", "BPMW 410:41F", "BPM 420", "BPMV 430 7F",
                            "BPR A = 41", "BRKOP EA", "BRK ON", "BRKINT ON", "BPD 2", "BPCHANGE 3 T", "BPCHANGE 4 s",
                            "WA 36", "WA 38", "WD 1",
                            "ZP 3C", "ZP3 40", "ZPD 3",
                            "BMA 400", "BMA 500" });
        }



        TEST_METHOD (WritesEveryTableAsOneFile)
        {
            ConsoleRig  rig;



            FillEveryTable (rig);

            ReplyCheck::IsOk (rig.Type ("SAVE all.txt"), "Saved the breakpoints, watches, zero-page pointers and bookmarks to all.txt.", "SAVE");
            Assert::AreEqual (std::string ("BPC *\n"
                                           "BP 0300\n"
                                           "BP 0310:031F IF A == 1\n"
                                           "BPMR 0400 BEFORE\n"
                                           "BPMW 0410:041F\n"
                                           "BPM 0420\n"
                                           "BPMV 0430 7F\n"
                                           "BPR A =41\n"
                                           "BRKOP EA\n"
                                           "BRK ON\n"
                                           "BRKINT ON\n"
                                           "BPD 2\n"
                                           "BPCHANGE 3 T\n"
                                           "BPCHANGE 4 s\n"
                                           "WC *\n"
                                           "WA 0036\n"
                                           "WA 0038\n"
                                           "WD 1\n"
                                           "ZPC *\n"
                                           "ZP0 003C\n"
                                           "ZP3 0040\n"
                                           "ZPD 3\n"
                                           "BMC *\n"
                                           "BMA 0400\n"
                                           "BMA 0500\n"), rig.files.PeekContent (L"all.txt"));
        }


        //  Cleared and loaded back, every table lists as it did, the disabled
        //  and flagged entries included.
        TEST_METHOD (LoadPutsEveryTableBack)
        {
            ConsoleRig   rig;
            std::string  before;



            FillEveryTable (rig);
            before = ListAll (rig);

            TypeAll (rig, { "SAVE all.txt", "BPC *", "WC *", "ZPC *", "BMC *" });
            Assert::AreEqual (std::string ("No breakpoints.\nNo watches.\nNo zero-page pointers.\nNo bookmarks."), ListAll (rig));

            TypeAll (rig, { "LOAD all.txt" });
            Assert::AreEqual (before, ListAll (rig));
        }


        //  A replayed file numbers its entries from 0, whatever ids they had.
        TEST_METHOD (LoadNumbersTheEntriesFromZero)
        {
            ConsoleRig  rig;



            TypeAll (rig, { "BP 300", "BP 310", "BP 320", "BPC 0", "BPD 2", "SAVE ids.txt", "BPC *", "LOAD ids.txt" });
            ReplyCheck::IsOk (rig.Type ("BPL"), "#0 enabled  at $0310, hits 0\n#1 disabled at $0320, hits 0", "BPL after LOAD");
        }


        //  What LOAD put back stops a real run: the enabled breakpoint does, and
        //  the one saved disabled does not.
        TEST_METHOD (TheBreakpointsLoadPutBack_StopARun)
        {
            ConsoleRig  rig;



            TypeAll (rig, { "BP 302", "BPD 0", "BP 305", "SAVE run.txt", "BPC *", "LOAD run.txt", "G" });
            rig.FinishRun();

            Assert::AreEqual ((Word) 0x0305, rig.GetPc(), L"the disabled $0302 was passed and the enabled $0305 stopped the run");
            Assert::AreEqual ((Byte) 0x41,   rig.Peek (0x0400), L"the STA between them ran");
        }


        TEST_METHOD (EmptyTables_WriteOnlyTheClears_AndLoadingThatClearsEverything)
        {
            ConsoleRig  rig;



            ReplyCheck::IsOk (rig.Type ("SAVE empty.txt"), "Saved the breakpoints, watches, zero-page pointers and bookmarks to empty.txt.", "SAVE");
            Assert::AreEqual (std::string ("BPC *\nWC *\nZPC *\nBMC *\n"), rig.files.PeekContent (L"empty.txt"));

            FillEveryTable (rig);
            TypeAll (rig, { "LOAD empty.txt" });
            Assert::AreEqual (std::string ("No breakpoints.\nNo watches.\nNo zero-page pointers.\nNo bookmarks."), ListAll (rig));
        }


        TEST_METHOD (NoFileName_IsAnError_InEveryDialect)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::AppleWin, "SAVE"   },
                { CommandMode::AppleWin, "save  " },
                { CommandMode::Casso,    "SAVE"   },
                { CommandMode::WinDbg,   "!save"  },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                ConsoleRig  rig;



                rig.SwitchTo (mode);
                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "invalid arguments", "SAVE takes a file name.", line);
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);

            {
                ConsoleRig  monitor;



                monitor.SwitchTo (CommandMode::Monitor);
                ReplyCheck::IsMonitorError (monitor.Type ("/SAVE"), CommandStatus::Error, "invalid arguments", "SAVE takes a file name.", "/SAVE");
                Assert::AreEqual ((size_t) 0, monitor.files.FileCount(), L"nothing was written");
            }
        }


        TEST_METHOD (AFileThatCannotBeWritten_IsAnError_AndStaysAsItWas)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"ro.txt", "kept\n");
            Assert::AreEqual (S_OK, rig.files.SetReadOnlyAttribute (L"ro.txt", true));
            TypeAll (rig, { "BP 300" });

            ReplyCheck::IsError (rig.Type ("SAVE ro.txt"), CommandStatus::Error, "file not written", "ro.txt could not be written.", "SAVE ro.txt");
            Assert::AreEqual (std::string ("kept\n"), rig.files.PeekContent (L"ro.txt"));
        }


        //  The name as typed: quotes dropped for the file, kept in the reply; a
        //  relative name under the current directory, a rooted one where it
        //  says.
        TEST_METHOD (TheNameIsResolvedAsEveryFileNameIs)
        {
            ConsoleRig  rig;



            (void) rig.Type ("CD C:\\cfg");

            ReplyCheck::IsOk (rig.Type ("SAVE \"my file.txt\""), "Saved the breakpoints, watches, zero-page pointers and bookmarks to \"my file.txt\".", "quoted");
            ReplyCheck::IsOk (rig.Type ("SAVE D:\\elsewhere\\f.txt"), "Saved the breakpoints, watches, zero-page pointers and bookmarks to D:\\elsewhere\\f.txt.", "rooted");

            Assert::IsTrue (rig.files.Exists (L"C:\\cfg\\my file.txt"));
            Assert::IsTrue (rig.files.Exists (L"D:\\elsewhere\\f.txt"));
        }


        TEST_METHOD (DialectForms_WriteTheSameFile)
        {
            ConsoleRig  rig;



            TypeAll (rig, { "BP 300", "WA 36" });

            rig.SwitchTo (CommandMode::Monitor);
            (void) rig.Type ("/SAVE mon.txt");

            rig.SwitchTo (CommandMode::WinDbg);
            (void) rig.Type ("!save dbg.txt");

            rig.SwitchTo (CommandMode::Casso);
            (void) rig.Type ("SAVE casso.txt");

            Assert::AreEqual (std::string ("BPC *\nBP 0300\nWC *\nWA 0036\nZPC *\nBMC *\n"), rig.files.PeekContent (L"mon.txt"));
            Assert::AreEqual (rig.files.PeekContent (L"mon.txt"), rig.files.PeekContent (L"dbg.txt"));
            Assert::AreEqual (rig.files.PeekContent (L"mon.txt"), rig.files.PeekContent (L"casso.txt"));
        }


        //  SAVE keeps the four tables and nothing else: none of these settings
        //  reaches the file.
        TEST_METHOD (OtherSettingsAreNotSaved)
        {
            ConsoleRig  rig;



            TypeAll (rig, { "DISASM BRANCH 0", "SYM HERE = 300", "LOG ERROR", "DB 300,4", "SKIP HOME", "BUDGET 1000",
                            "SRC ON", "CALLS MODE WALK", "BPV 100", "OUTPUT MONITOR", "CD C:\\cfg" });

            (void) rig.Type ("SAVE f.txt");
            Assert::AreEqual (std::string ("BPC *\nWC *\nZPC *\nBMC *\n"), rig.files.PeekContent (L"C:\\cfg\\f.txt"));
        }


        //  GSSquared's `save` is its BSAVE, so SAVE is out of its reach; its help
        //  describes its own.
        TEST_METHOD (InGSSquared_SaveIsBsave)
        {
            ConsoleRig   rig;
            std::string  expected;



            TypeAll (rig, { "BP 300" });
            rig.SwitchTo (CommandMode::GSSquared);

            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("save \"mem.bin\" 300.30F").status), L"save");

            for (Word address = 0x300; address <= 0x30F; ++address)
            {
                expected += (char) rig.Peek (address);
            }

            Assert::AreEqual (expected, rig.files.PeekContent (L"mem.bin"), L"the sixteen bytes, not the breakpoints");
            ReplyCheck::IsOk (rig.Type ("? save"), "save \"file\" first.last: Write first..last to a file", "? save");
        }
    };
}
