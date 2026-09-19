#include "Pch.h"

#include "Cli/DebugBatchRunner.h"
#include "Devices/Disk/DiskImageStore.h"
#include "EmuTests/FixtureProvider.h"
#include "EmuTests/FixtureRomSource.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugModeTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BatchRig
    //
    //  A batch run over the fixture ROMs and an in-memory file system. The
    //  scripts and the output they are expected to produce are fixtures
    //  under Debugger/Scripts, so a change in any command's text shows up
    //  here as a diff against a file a reader can open.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BatchRig
    {
    public:
        FixtureRomSource                  roms;
        InMemoryFileSystem                files;
        CommandLineOptions::DebugOptions  options;
        DebugBatchResult                  result;

        BatchRig()
        {
            options.machine = "Apple2e";
        }

        static std::string Fixture (const std::string & relativePath)
        {
            FixtureProvider       provider;
            std::vector<uint8_t>  bytes;
            HRESULT               hr = provider.OpenFixture (relativePath, bytes);



            Assert::AreEqual (S_OK, hr, Widen ("fixture missing: " + relativePath).c_str());
            return std::string (bytes.begin(), bytes.end());
        }

        static std::string Script   (const char * name) { return Fixture (std::string ("Debugger/Scripts/") + name); }
        static std::string Expected (const char * name) { return Fixture (std::string ("Debugger/Scripts/expected/") + name); }

        int Run (const std::string & script)
        {
            DebugBatchRunner  runner (roms, files);
            HRESULT           hr = runner.Run (options, script, result);



            IGNORE_RETURN_VALUE (hr, S_OK);
            return result.exitStatus;
        }

        //  The run's output against a golden file, reported by the first line
        //  that differs so the diff is readable in a test log.
        void AssertOutputIs (const char * expectedName)
        {
            std::string  expected = Expected (expectedName);



            if (expected != result.output)
            {
                std::istringstream  wanted (expected);
                std::istringstream  got    (result.output);
                std::string         wantedLine;
                std::string         gotLine;
                int                 line = 0;

                while (true)
                {
                    bool  hasWanted = (bool) std::getline (wanted, wantedLine);
                    bool  hasGot    = (bool) std::getline (got, gotLine);

                    line++;

                    if (!hasWanted && !hasGot)
                    {
                        break;
                    }

                    if (!hasWanted || !hasGot || wantedLine != gotLine)
                    {
                        Assert::Fail (Widen (std::string (expectedName) + " line " + std::to_string (line)
                                             + "\n  expected: " + (hasWanted ? wantedLine : "<end>")
                                             + "\n  actual:   " + (hasGot ? gotLine : "<end>")
                                             + "\n--- whole output ---\n" + result.output).c_str());
                    }
                }

                Assert::Fail (Widen (std::string (expectedName) + ": the outputs differ only in bytes no line shows").c_str());
            }
        }

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugModeTests)
    {
    public:
        //  The quickstart's first scenario: a loop at $0300 reading a soft
        //  switch, a read watchpoint on it, the stop, the registers, three
        //  steps.
        TEST_METHOD (StopScript_ProducesTheExpectedText_AndExitsZero)
        {
            BatchRig  rig;



            Assert::AreEqual (0, rig.Run (rig.Script ("stop.txt")));
            Assert::IsTrue   (rig.result.diagnostics.empty(), rig.Widen (rig.result.diagnostics).c_str());
            rig.AssertOutputIs ("stop.txt");
        }

        TEST_METHOD (StopScript_ProducesTheExpectedJsonLines)
        {
            BatchRig  rig;



            rig.options.json = true;

            Assert::AreEqual (0, rig.Run (rig.Script ("stop.txt")));
            rig.AssertOutputIs ("stop.jsonl");
        }

        //  SC-004: two runs of one script are the same bytes, in both forms.
        TEST_METHOD (TwoRuns_ProduceTheSameBytes)
        {
            BatchRig     text;
            BatchRig     json;
            std::string  script = BatchRig::Script ("stop.txt");
            std::string  first;



            text.Run (script);
            first = text.result.output;
            text.Run (script);
            Assert::IsTrue (first == text.result.output, L"text");

            json.options.json = true;
            json.Run (script);
            first = json.result.output;
            json.Run (script);
            Assert::IsTrue (first == json.result.output, L"JSON");
        }

        //  A dump of never-written DRAM is the power-on pattern, which is what
        //  the seed pins: the same seed twice is the same bytes, and another
        //  seed is not.
        TEST_METHOD (DramPattern_FollowsTheSeed)
        {
            BatchRig     same;
            BatchRig     other;
            std::string  first;



            same.options.commands = { "D 2000" };
            same.Run ("");
            first = same.result.output;
            same.Run ("");
            Assert::IsTrue (first == same.result.output, L"the same seed twice");

            other.options.commands = { "D 2000" };
            other.options.seed     = 0x1234;
            other.Run ("");
            Assert::IsFalse (first == other.result.output, L"another seed");
        }

        //  A run that never reaches its breakpoint ends on the budget every
        //  run carries, and the status says so.
        TEST_METHOD (BudgetStop_ExitsThree)
        {
            BatchRig  rig;



            rig.options.maxCycles = 20000;

            Assert::AreEqual (3, rig.Run (rig.Script ("budget.txt")));
            rig.AssertOutputIs ("budget.txt");
        }

        //  An unknown command and a command that fails both set the status,
        //  and neither stops the script.
        TEST_METHOD (ErrorAndUnknown_ExitOne_AndTheScriptGoesOn)
        {
            BatchRig  rig;



            Assert::AreEqual (1, rig.Run (rig.Script ("errors.txt")));
            rig.AssertOutputIs ("errors.txt");
        }

        TEST_METHOD (ErrorBeatsBudget)
        {
            BatchRig  rig;



            rig.options.maxCycles = 20000;
            rig.options.commands  = { "FROB", "G" };

            Assert::AreEqual (1, rig.Run (""));
            Assert::IsTrue (rig.result.output.find ("Budget at") != std::string::npos, L"the budget stop still printed");
        }

        //  A command that is not available in this session is not a failure.
        TEST_METHOD (NotAvailable_ExitsZero)
        {
            BatchRig  rig;



            rig.options.commands = { "DISK EJECT" };

            Assert::AreEqual (0, rig.Run (""));
        }

        //  MODE switches the parser mid-script and the prompt follows it; `/`
        //  reaches AppleWin mode from Monitor mode.
        TEST_METHOD (ModeSwitch_MidScript)
        {
            BatchRig  rig;



            Assert::AreEqual (0, rig.Run (rig.Script ("modes.txt")));
            rig.AssertOutputIs ("modes.txt");
        }

        //  Quickstart Story 14: WinDbg mode's commands in its own layouts, the
        //  excluded families, `!` reaching the engine, and the three ways of
        //  writing one address.
        TEST_METHOD (WinDbgScript_ProducesTheExpectedText)
        {
            BatchRig  rig;



            rig.Run (rig.Script ("windbg.txt"));
            rig.AssertOutputIs ("windbg.txt");
        }

        //  --mode windbg starts the script in WinDbg mode, at its prompt.
        TEST_METHOD (ModeOption_StartsInWinDbgMode)
        {
            BatchRig  rig;



            rig.options.mode     = "windbg";
            rig.options.commands = { "r", "!mode" };

            Assert::AreEqual (0, rig.Run (""));
            Assert::IsTrue   (rig.result.output.starts_with ("0:000> r\na="), rig.Widen (rig.result.output).c_str());
            Assert::IsTrue   (rig.result.output.find ("Mode: WINDBG") != std::string::npos, rig.Widen (rig.result.output).c_str());
        }

        //  As JSON the mode change is a record of its own, since a tool
        //  reading the stream keys its parser on it.
        TEST_METHOD (ModeSwitch_MidScript_AsJsonLines)
        {
            BatchRig  rig;



            rig.options.json = true;

            Assert::AreEqual (0, rig.Run (rig.Script ("modes.txt")));
            rig.AssertOutputIs ("modes.jsonl");
        }

        //  Quickstart phase 1 steps 5 and 6, driven from a script: deposit
        //  and examine, search, step, the registers and the colon that sets
        //  them, a host file written and read back, and an AppleWin command
        //  reached with the slash prefix.
        TEST_METHOD (MonitorScript_ProducesTheExpectedText)
        {
            BatchRig  rig;



            Assert::AreEqual (0, rig.Run (rig.Script ("monitor.txt")));
            Assert::IsTrue   (rig.result.diagnostics.empty(), rig.Widen (rig.result.diagnostics).c_str());
            rig.AssertOutputIs ("monitor.txt");

            //  The file the script wrote went through the injected file
            //  system and nowhere near a real disk.
            Assert::IsTrue (rig.files.Exists (L"out.bin"), L"the script's host file");
        }



        TEST_METHOD (MonitorScript_ProducesTheExpectedJsonLines)
        {
            BatchRig  rig;



            rig.options.json = true;

            Assert::AreEqual (0, rig.Run (rig.Script ("monitor.txt")));
            rig.AssertOutputIs ("monitor.jsonl");
        }



        //  Quickstart Story 12 steps 1-5: GSSquared's commands in its own
        //  layouts, the steps, a breakpoint set here listed by the Monitor's
        //  /bpl, the output format changed alone and reset by MODE, BPR with
        //  its operator unspaced, the bank rules, a IIgs command, and an
        //  engine command by its bare name.
        TEST_METHOD (GSSquaredScript_ProducesTheExpectedText)
        {
            BatchRig  rig;



            Assert::AreEqual (0, rig.Run (rig.Script ("gssquared.txt")));
            Assert::IsTrue   (rig.result.diagnostics.empty(), rig.Widen (rig.result.diagnostics).c_str());
            rig.AssertOutputIs ("gssquared.txt");
        }



        TEST_METHOD (GSSquaredScript_ProducesTheExpectedJsonLines)
        {
            BatchRig  rig;



            rig.options.json = true;

            Assert::AreEqual (0, rig.Run (rig.Script ("gssquared.txt")));
            rig.AssertOutputIs ("gssquared.jsonl");
        }



        //  --mode and --output from the command line: the mode's format by
        //  default, and the output format apart from it when given.
        TEST_METHOD (ModeAndOutputOptions_SetTheStartingModeAndFormat)
        {
            BatchRig  gssquared;
            BatchRig  appleWin;



            gssquared.options.mode     = "gssquared";
            gssquared.options.commands = { "300: A9 41", "300.301" };
            Assert::AreEqual (0, gssquared.Run (""));
            Assert::IsTrue   (gssquared.result.output.find (">300.301\n00/0300: A9 41") != std::string::npos,
                              gssquared.Widen (gssquared.result.output).c_str());

            appleWin.options.output   = "gssquared";
            appleWin.options.commands = { "MEB 300 A9 41", "D 300:301" };
            Assert::AreEqual (0, appleWin.Run (""));
            Assert::IsTrue   (appleWin.result.output.find (">D 300:301\n00/0300: A9 41") != std::string::npos,
                              appleWin.Widen (appleWin.result.output).c_str());
        }



        //  Quickstart Story 10: an IF breakpoint on a loop that counts A up
        //  stops once, with A at $41; a value breakpoint stops after the
        //  write that makes $06 hold 7; an IF expression that reads an I/O
        //  address is an error, which sets the exit status, and creates
        //  nothing.
        TEST_METHOD (ConditionsScript_ProducesTheExpectedText)
        {
            BatchRig  rig;



            Assert::AreEqual (1, rig.Run (rig.Script ("conditions.txt")));
            rig.AssertOutputIs ("conditions.txt");
        }



        TEST_METHOD (ConditionsScript_ProducesTheExpectedJsonLines)
        {
            BatchRig  rig;



            rig.options.json = true;

            Assert::AreEqual (1, rig.Run (rig.Script ("conditions.txt")));
            rig.AssertOutputIs ("conditions.jsonl");
        }



        TEST_METHOD (MonitorOption_StartsInMonitorMode)
        {
            BatchRig  rig;



            rig.options.mode     = "monitor";
            rig.options.commands = { "/R" };

            Assert::AreEqual (0, rig.Run (""));
            Assert::IsTrue (rig.result.output.starts_with ("*/R\n"), rig.Widen (rig.result.output).c_str());
        }

        TEST_METHOD (Comments_BlankLines_AndWhitespace_AreSkipped)
        {
            BatchRig  rig;



            Assert::AreEqual (0, rig.Run (rig.Script ("comments.txt")));
            rig.AssertOutputIs ("comments.txt");
        }

        //  The one blank line that reaches the session: the end of a line
        //  assembly.
        TEST_METHOD (BlankLine_EndsAnAssembly)
        {
            BatchRig  rig;



            Assert::AreEqual (0, rig.Run (rig.Script ("assemble.txt")));
            rig.AssertOutputIs ("assemble.txt");
        }

        //  Quickstart Story 11: a loop of LDA $10FF,X with X crossing the page
        //  three times in four, profiled. The crossing cycles and the taken
        //  branches are billed apart from the base cycles, the hottest address
        //  carries its symbol, and SAVE writes the same rows to a file.
        TEST_METHOD (ProfileScript_SeparatesPenalties)
        {
            BatchRig     rig;
            std::string  saved;



            Assert::AreEqual (0, rig.Run (rig.Script ("profile.txt")));
            Assert::IsTrue   (rig.result.diagnostics.empty(), rig.Widen (rig.result.diagnostics).c_str());
            rig.AssertOutputIs ("profile.txt");

            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"profile.txt", saved));
            Assert::IsTrue   (rig.result.output.find (saved.substr (0, saved.find ('\n'))) != std::string::npos, L"the saved file starts with the listed summary");
            Assert::IsTrue   (saved.find ("$0302   LOOP") != std::string::npos, L"and holds the per-address rows");
        }



        TEST_METHOD (ProfileScript_AsJsonLines)
        {
            BatchRig  rig;



            rig.options.json = true;

            Assert::AreEqual (0, rig.Run (rig.Script ("profile.txt")));
            rig.AssertOutputIs ("profile.jsonl");
        }

        //  BSAVE, BLOAD and TF reach the injected file system and nothing else.
        TEST_METHOD (HostFiles_GoThroughTheFileSystemSeam)
        {
            BatchRig     rig;
            std::string  saved;



            Assert::AreEqual (0, rig.Run (rig.Script ("files.txt")));
            rig.AssertOutputIs ("files.txt");

            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"out.bin", saved));
            Assert::IsTrue (saved == std::string ("\x01\x02\x03\x04", 4), L"the four bytes BSAVE wrote");
            Assert::IsTrue (rig.files.Exists (L"trace.txt"), L"the trace file");
        }

        //  Command lines run after the script's lines.
        TEST_METHOD (CommandLines_FollowTheScript)
        {
            BatchRig  rig;



            rig.options.commands = { "R" };

            Assert::AreEqual (0, rig.Run ("MEB 300 EA\n"));
            Assert::IsTrue (rig.result.output.starts_with (">MEB 300 EA\n"), rig.Widen (rig.result.output).c_str());
            Assert::IsTrue (rig.result.output.find ("\n>R\n") != std::string::npos, rig.Widen (rig.result.output).c_str());
        }

        //  A disk image is read through the seam and mounted in drive 1.
        TEST_METHOD (Disk_IsMountedFromTheFileSystem)
        {
            BatchRig  rig;



            rig.files.WriteAllText (L"C:\\Work\\merlin.dsk", rig.Fixture ("Disks/Merlin-proDos2.23.dsk"));
            rig.options.disk1    = "C:\\Work\\merlin.dsk";
            rig.options.commands = { "DISK" };

            Assert::AreEqual (0, rig.Run (""));
            Assert::IsTrue (rig.result.output.find ("Slot 6, drive 1: C:\\Work\\merlin.dsk") != std::string::npos, rig.Widen (rig.result.output).c_str());
            Assert::IsTrue (rig.result.output.find ("Slot 6, drive 2: empty") != std::string::npos, rig.Widen (rig.result.output).c_str());
        }

        //  What the guest writes stays in the overlay unless --write-disks.
        //
        //  A WOZ, because its flush writes the bit stream as it is, so one
        //  flipped bit is one changed byte in the file. A .dsk is rebuilt
        //  from its decoded sectors, and a bit flipped in a gap decodes to
        //  the same sectors and the same file.
        TEST_METHOD (DiskWrites_StayInTheOverlay_UnlessWriteDisks)
        {
            std::string        dsk = BatchRig::Fixture ("Disks/Merlin-proDos2.23.dsk");
            std::vector<Byte>  sectors (dsk.begin(), dsk.end());
            std::vector<Byte>  woz;
            DiskImage          nibblized;



            Assert::AreEqual (S_OK, NibblizationLayer::NibblizeDsk (sectors, nibblized));
            Assert::AreEqual (S_OK, WozLoader::Serialize (nibblized, woz));

            for (bool writeDisks : { false, true })
            {
                BatchRig          rig;
                DebugBatchRunner  runner (rig.roms, rig.files);
                std::string       original (woz.begin(), woz.end());
                std::string       after;
                DiskImage       * image = nullptr;



                rig.files.WriteAllText (L"C:\\Work\\merlin.woz", original);
                rig.options.disk1      = "C:\\Work\\merlin.woz";
                rig.options.writeDisks = writeDisks;
                rig.options.commands   = { "DISK" };

                Assert::AreEqual (S_OK, runner.Prepare (rig.options, rig.result));

                image = runner.GetHost()->GetDiskStore().GetImage (6, 0);
                Assert::IsNotNull (image);
                image->WriteBit (0, 0, image->ReadBit (0, 0) ^ 1);

                runner.Execute (rig.options, "", rig.result);

                Assert::AreEqual (0, rig.result.exitStatus);
                Assert::AreEqual (S_OK, rig.files.ReadAllText (L"C:\\Work\\merlin.woz", after));
                Assert::AreEqual (!writeDisks, after == original, writeDisks ? L"--write-disks wrote the file" : L"the overlay left the file alone");
            }
        }

        TEST_METHOD (MissingDisk_ExitsTwo_AndRunsNothing)
        {
            BatchRig  rig;



            rig.options.disk1    = "C:\\Work\\none.dsk";
            rig.options.commands = { "R" };

            Assert::AreEqual (2, rig.Run (""));
            Assert::IsTrue (rig.result.output.empty());
            Assert::IsTrue (rig.result.diagnostics.find ("none.dsk") != std::string::npos, rig.Widen (rig.result.diagnostics).c_str());
        }

        TEST_METHOD (UnknownMachine_ExitsTwo)
        {
            BatchRig  rig;



            rig.options.machine  = "Apple9";
            rig.options.commands = { "R" };

            Assert::AreEqual (2, rig.Run (""));
            Assert::IsTrue (rig.result.output.empty());
            Assert::IsFalse (rig.result.diagnostics.empty());
        }
    };
}
