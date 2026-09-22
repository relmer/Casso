#include "Pch.h"

#include "Version.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ConfigHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlersTests
//
//  Directories, scripts, the DISASM and LOG settings, the output commands,
//  help and the disk report.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ConfigHandlersTests)
    {
    public:

        using Rig = HandlerRig<ConfigHandlers>;



        TEST_METHOD (PWD_CD)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("C:\\Work"),        rig.RunOk ("PWD").text.at (0));
            Assert::AreEqual (std::string ("D:\\disks"),       rig.RunOk ("CD D:\\disks").text.at (0));
            Assert::AreEqual (std::string ("D:\\disks\\sub"),  rig.RunOk ("cd sub").text.at (0), L"a relative directory is under the current one");
            Assert::AreEqual (std::wstring (L"D:\\disks\\sub"), rig.session.GetCurrentDirectory());
            rig.RunFails ("CD", "invalid arguments");
        }



        TEST_METHOD (ECHO_PRINT_PRINTF_CALC)
        {
            Rig    rig;
            Reply  reply;



            rig.target.registers.a = 0x41;

            Assert::AreEqual (std::string ("Hello,  World"), rig.RunOk ("ECHO Hello,  World").text.at (0));
            Assert::AreEqual (std::string ("Hello World"),   rig.RunOk ("ECHO \"Hello World\"").text.at (0));
            Assert::AreEqual (std::string ("A=0041"),        rig.RunOk ("PRINT \"A=\",A").text.at (0));
            Assert::AreEqual (std::string ("0302"),          rig.RunOk ("PRINT PC+2").text.at (0));
            Assert::AreEqual (std::string ("A=0041 d=65 z=01000001 c=A %"), rig.RunOk ("PRINTF \"A=%x d=%d z=%z c=%c %%\",A,A,A,A").text.at (0));

            reply = rig.RunOk ("PRINTF \"one\\ntwo %X\",1234");
            Assert::AreEqual ((size_t) 2, reply.text.size());
            Assert::AreEqual (std::string ("one"),      reply.text[0]);
            Assert::AreEqual (std::string ("two 1234"), reply.text[1]);

            Assert::AreEqual (std::string ("$0041  0z01000001     65  'A'"), rig.RunOk ("CALC 41").text.at (0));
            Assert::AreEqual (std::string ("$0043  0z01000011     67  'C'"), rig.RunOk ("CALC A+2").text.at (0));

            rig.RunFails ("PRINTF",             "invalid arguments");
            rig.RunFails ("PRINTF \"%d %d\",1", "invalid arguments");
            rig.RunFails ("PRINT NOSUCH",       "invalid arguments");
            rig.RunFails ("CALC",               "invalid arguments");
        }



        TEST_METHOD (LOG_Levels)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Log: INFO"),  rig.RunOk ("LOG").text.at (0));
            Assert::AreEqual (std::string ("Log: ALL"),   rig.RunOk ("LOG ALL").text.at (0));
            Assert::AreEqual ((int) LogLevel::All,        (int) rig.session.GetLogLevel());
            Assert::AreEqual (std::string ("Log: ERROR"), rig.RunOk ("log error").text.at (0));
            Assert::AreEqual (std::string ("Log: ERROR"), rig.RunOk ("LOG OFF").text.at (0));
            Assert::AreEqual (std::string ("Log: INFO"),  rig.RunOk ("LOG ON").text.at (0));
            rig.RunFails ("LOG loud", "invalid arguments");
        }



        TEST_METHOD (RUN_LOAD_STARTUP_ScriptsThroughTheSession)
        {
            Rig                       rig;
            std::vector<std::string>  lines;



            rig.files.WriteAllText (L"C:\\Work\\script.txt", "ECHO one\r\n; a comment\nCALC 2\nFROB\n");

            lines = rig.RunOk ("RUN script.txt").text;
            Assert::AreEqual ((size_t) 4, lines.size(), L"a comment prints nothing; an unknown command prints its error");
            Assert::AreEqual (std::string ("one"),                          lines[0]);
            Assert::AreEqual (std::string ("$0002  0z00000010      2  ' ' (Ctrl)"), lines[1]);
            Assert::AreEqual (std::string ("Error: unknown command"),       lines[2]);

            Assert::AreEqual ((size_t) 4, rig.RunOk ("LOAD \"script.txt\"").text.size());
            rig.RunFails ("RUN missing.txt", "file not found");
            rig.RunFails ("RUN",             "invalid arguments");
            rig.RunFails ("STARTUP",         "file not found");

            rig.files.WriteAllText (L"C:\\Work\\DebuggerAutoRun.txt", "ECHO started\n");
            Assert::AreEqual (std::string ("started"), rig.RunOk ("STARTUP").text.at (0));
        }



        TEST_METHOD (SAVE_WritesAllFourScripts_LoadReplays)
        {
            Rig                 rig;
            BreakpointHandlers  breakpoints;
            WatchHandlers       watches;
            std::string         script;



            rig.session.AddHandler (&breakpoints);
            rig.session.AddHandler (&watches);
            rig.RunOk ("BP 300");
            rig.RunOk ("WA 36");
            rig.RunOk ("ZP 3C");
            rig.RunOk ("BMA 400");

            rig.RunOk ("SAVE all.txt");
            script = rig.files.PeekContent (L"C:\\Work\\all.txt");
            Assert::AreEqual (std::string ("BPC *\nBP 0300\nWC *\nWA 0036\nZPC *\nZP0 003C\nBMC *\nBMA 0400\n"), script);

            rig.RunOk ("BPC *");
            rig.RunOk ("WC *");
            rig.RunOk ("ZPC *");
            rig.RunOk ("BMC *");
            rig.RunOk ("LOAD all.txt");

            Assert::AreEqual (std::string ("#0 enabled  at $0300, hits 0"), rig.RunOk ("BPL").text.at (0));
            Assert::AreEqual (std::string ("#0 $0036 = $0000"),             rig.RunOk ("WL").text.at (0));
            Assert::AreEqual (std::string ("#0 $003C -> $0000"),            rig.RunOk ("ZPL").text.at (0));
            Assert::AreEqual (std::string ("#0 $0400"),                     rig.RunOk ("BML").text.at (0));
            rig.RunFails ("SAVE", "invalid arguments");
        }



        TEST_METHOD (HELP_VERSION_MOTD)
        {
            Rig                       rig;
            std::vector<std::string>  lines = rig.RunOk ("HELP").text;
            bool                      hasCpu = false;



            for (const std::string & line : lines)
            {
                hasCpu |= line.starts_with ("Cpu: ") && line.find (" GG ") != std::string::npos;
            }

            Assert::IsTrue   (lines.size() > 10, L"one line per family");
            Assert::IsTrue   (hasCpu);
            Assert::AreEqual (std::string ("BPM (Breakpoints)"),             rig.RunOk ("HELP bpm").text.at (0));
            Assert::AreEqual (std::string ("BPIO (Breakpoints): alias of BPM"), rig.RunOk ("? BPIO").text.at (0));
            Assert::AreEqual (std::string ("HGR (Views): needs the debugger window"), rig.RunOk ("HELP HGR").text.at (0));
            Assert::AreEqual ((int) CommandStatus::Unknown, (int) rig.Run ("HELP FROB").status);
            Assert::AreEqual (std::string ("Casso " VERSION_STRING), rig.RunOk ("VERSION").text.at (0));
            Assert::IsFalse  (rig.RunOk ("MOTD").text.at (0).empty());
        }


        TEST_METHOD (HELP_ListsTheModesOwnCommandsFirst)
        {
            const std::tuple<const char *, const char *, const char *, const char *>  modes[] =
            {
                { "WINDBG",    ".help", "WinDbg commands:",    "Casso commands, after !" },
                { "MONITOR",   "/HELP", "Monitor commands:",   "Casso commands, after /" },
                { "GSSQUARED", "help",  "GSSquared commands:", "Casso commands, by name" },
            };



            for (const auto & [mode, help, heading, engine] : modes)
            {
                Rig                       rig;
                std::vector<std::string>  lines;
                bool                      hasEngine = false;



                (void) rig.session.ExecuteLine (std::string ("MODE ") + mode, CommandMode::AppleWin);
                lines = rig.RunOk (help).text;

                for (const std::string & line : lines)
                {
                    hasEngine |= line.starts_with (engine);
                }

                Assert::AreEqual (std::string (heading), lines.at (0));
                Assert::IsTrue   (hasEngine, L"the engine commands follow, with the way the mode reaches them");
            }

            {
                Rig  rig;



                (void) rig.session.ExecuteLine ("MODE WINDBG", CommandMode::AppleWin);
                Assert::AreEqual (std::string ("ba r1|w1|e1 addr: Break on a read, write or execution of addr"), rig.RunOk (".help ba").text.at (0));
                Assert::AreEqual (std::string ("BPM (Breakpoints)"), rig.RunOk (".help bpm").text.at (0), L"an engine command still answers");
            }
        }



        TEST_METHOD (DISASM_Settings)
        {
            Rig  rig;



            Assert::AreEqual ((size_t) 8, rig.RunOk ("DISASM").text.size());
            Assert::AreEqual (std::string ("BRANCH = 0"), rig.RunOk ("DISASM branch 0").text.at (0));
            Assert::AreEqual (std::string ("BRANCH = 0"), rig.RunOk ("DISASM BRANCH").text.at (0), L"the setting sticks");
            Assert::AreEqual (std::string ("BRANCH = 1"), rig.RunOk ("DISASM BRANCH 1").text.at (0));
            rig.RunFails ("DISASM FOO",      "invalid arguments");
            rig.RunFails ("DISASM BRANCH 2", "invalid arguments");
        }



        TEST_METHOD (DISK_InfoAndSlot_ChangesNeedTheEmulator)
        {
            Rig                       rig;
            std::vector<std::string>  lines;



            rig.target.machineInfo.disks = { "C:\\a.dsk", "" };

            lines = rig.RunOk ("DISK").text;
            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::AreEqual (std::string ("Slot 6, drive 1: C:\\a.dsk"), lines[0]);
            Assert::AreEqual (std::string ("Slot 6, drive 2: empty"),     lines[1]);
            Assert::AreEqual (std::string ("Disk slot: 6"), rig.RunOk ("DISK SLOT").text.at (0));
            Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) rig.Run ("DISK EJECT 1").status);
            rig.RunFails ("DISK FROB", "invalid arguments");
        }



        //  Device panels live in the window, which runs PANEL itself; batch and
        //  the pipe report that it needs one.
        TEST_METHOD (PANEL_NeedsTheWindow)
        {
            Rig  rig;



            for (const char * line : { "PANEL", "PANEL LIST", "PANEL disk", "PANEL CLOSE disk" })
            {
                Reply  reply = rig.Run (line);

                Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status, std::wstring (line, line + strlen (line)).c_str());
            }
        }
    };
}
