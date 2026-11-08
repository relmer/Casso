#include "Pch.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/DebugHandlerSet.h"
#include "FakeDiagnosticsProvider.h"
#include "HandlerTestRig.h"
#include "MockDebugTarget.h"
#include "TestHelpers.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "WindowConsoleRig.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace WindowPaneCommandTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneRig
    //
    //  The window's console over a real //e, with NOPs from $0200 to $0500 and
    //  the PC at $0300, so a view centered on the PC has it on its middle row.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PaneRig : public WindowConsoleRig
    {
    public:
        static constexpr Byte  kNop    = 0xEA;
        static constexpr int   kMiddle = DebuggerViewState::kCodeLines / 2;



        PaneRig()
        {
            Fill (0x0200, 0x0500, kNop);
            Poke (0x0500, { 0x4C, 0x00, 0x03 });    // JMP $0300
            SetPc (0x0300);
        }



        //  A reply that brought a pane forward: Ok, nothing to say, and the
        //  line it answers.
        static void AssertShown (const Reply & reply, const std::string & line)
        {
            AssertReply (reply, CommandStatus::Ok, "", "", {}, line);
            Assert::AreEqual (line, reply.command, Widen (line).c_str());
            Assert::IsTrue   (reply.verb == DebugVerb::None, Widen (line).c_str());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CodeCommandTests
    //
    //  CODE and CODE1 bring the first disassembly forward and move nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CodeCommandTests)
    {
    public:

        TEST_METHOD (CodeBringsTheFirstDisassemblyForward)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "CODE" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "CODE1" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "code1" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/CODE" },
                                               std::pair<const char *, const char *> { "MONITOR",  " /code1" },
                                               std::pair<const char *, const char *> { "CASSO",    "Code" } })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  before;
                DebuggerViewSnapshot  after;



                rig.Type (std::string ("MODE ") + mode);
                rig.Type ("CONSOLE");
                before = rig.Build();

                PaneRig::AssertShown (rig.Type (line), line);

                after = rig.Build();
                Assert::AreEqual (DebuggerLayout::GetCodePaneId (0), after.showPane,       PaneRig::Widen (line).c_str());
                Assert::AreEqual (before.showPaneSerial + 1,        after.showPaneSerial, PaneRig::Widen (line).c_str());

                after.showPane       = before.showPane;
                after.showPaneSerial = before.showPaneSerial;
                PaneRig::AssertPanesUnchanged (before, after, line);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CodeTwoCommandTests
    //
    //  CODE2 brings the second disassembly forward, opening it on the PC when
    //  it is closed and leaving it where it is when it is open.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CodeTwoCommandTests)
    {
    public:

        TEST_METHOD (CodeTwoOpensTheSecondViewOnThePc)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "CODE2" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "code2" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/CODE2" },
                                               std::pair<const char *, const char *> { "CASSO",    "CODE2" } })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  before;
                DebuggerViewSnapshot  after;



                rig.Type (std::string ("MODE ") + mode);
                before = rig.Build();
                Assert::IsFalse (before.codeOpen[1]);

                PaneRig::AssertShown (rig.Type (line), line);

                after = rig.Build();
                Assert::IsTrue   (after.codeOpen[1],                                             PaneRig::Widen (line).c_str());
                Assert::AreEqual (PaneRig::kMiddle, PaneRig::GetCurrentRow (after.codeViews[1]), L"the PC on its middle row");
                Assert::AreEqual (DebuggerLayout::GetCodePaneId (1), after.showPane);
                Assert::AreEqual (before.showPaneSerial + 1,        after.showPaneSerial);
                Assert::AreEqual (0,                                after.followView,     L"it does not take the PC");
                Assert::IsTrue   (PaneRig::GetAddresses (before.codeViews[0]) == PaneRig::GetAddresses (after.codeViews[0]));
            }
        }



        //  Open already, the second view is not moved to the PC again.
        TEST_METHOD (AnOpenSecondViewStaysWhereItIs)
        {
            PaneRig               rig;
            DebuggerViewSnapshot  opened;
            DebuggerViewSnapshot  again;



            rig.Type ("CODE2");
            opened = rig.Build();

            rig.Type ("PAGEUP4K");
            for (int i = 0; i < 3; i++)
            {
                rig.Step();
            }

            rig.Build();
            PaneRig::AssertShown (rig.Type ("CODE2"), "CODE2");

            again = rig.Build();
            Assert::IsTrue   (PaneRig::GetAddresses (opened.codeViews[1]) == PaneRig::GetAddresses (again.codeViews[1]));
            Assert::AreEqual (opened.showPaneSerial + 1, again.showPaneSerial, L"brought forward again");
        }



        //  During a run the second view opens on the PC of that moment and
        //  stays there, while the first follows the PC.
        TEST_METHOD (OpenedDuringARunItStaysWhereItOpened)
        {
            PaneRig               rig;
            DebuggerViewSnapshot  opened;
            DebuggerViewSnapshot  later;



            rig.Type ("G");
            rig.RunFrames (1);
            rig.Type ("CODE2");

            opened = rig.Build (false);
            Assert::AreEqual (PaneRig::kMiddle, PaneRig::GetRowOf (opened.codeViews[1], opened.pc));

            rig.RunFrames (1);
            later = rig.Build (false);

            Assert::AreNotEqual (opened.pc, later.pc);
            Assert::IsTrue (PaneRig::GetAddresses (opened.codeViews[1]) == PaneRig::GetAddresses (later.codeViews[1]), L"the second view held");
            Assert::IsTrue (PaneRig::GetCurrentRow (later.codeViews[0]) >= 0, L"the first follows the PC");
            Assert::IsTrue (rig.controller.IsRunInProgress());

            rig.controller.RequestPause();
            rig.RunFrames (1);
        }



        //  With a debug file loaded, the rows of every view carry their source
        //  line, since those are the rows the window paints and selects from;
        //  the first view's copy in the snapshot is not.
        TEST_METHOD (EveryViewsRowsCarryTheirSourceLines)
        {
            PaneRig               rig;
            DebugFile             file;
            DebuggerViewSnapshot  snapshot;



            rig.Poke (0x0300, { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0x60 });

            file.major    = 2;
            file.files    = { { 0, "main.a65", 45, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", 0x0300, 6 });
            file.spans    = { { 0, 0, 0, 2 }, { 1, 0, 2, 3 }, { 2, 0, 5, 1 } };
            file.lines    = { { 0, 0, 2, DebugLineType::Asm, 0, { 0 } },
                              { 1, 0, 3, DebugLineType::Asm, 0, { 1 } },
                              { 2, 0, 4, DebugLineType::Asm, 0, { 2 } } };
            rig.Session().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");

            rig.Type ("CODE2");
            snapshot = rig.Build();

            for (int view = 0; view < 2; view++)
            {
                const std::vector<DebuggerViewSnapshot::CodeLine> & rows = snapshot.codeViews[(size_t) view];



                for (const auto & [address, line] : { std::pair<Word, int> { 0x0300, 2 }, std::pair<Word, int> { 0x0302, 3 }, std::pair<Word, int> { 0x0305, 4 } })
                {
                    int           row   = PaneRig::GetRowOf (rows, address);
                    std::wstring  where = std::format (L"view {} ${:04X}", view + 1, address);



                    Assert::IsTrue   (row >= 0, where.c_str());
                    Assert::AreEqual (0,    rows[(size_t) row].sourceFileId, where.c_str());
                    Assert::AreEqual (line, rows[(size_t) row].sourceLine,   where.c_str());
                }
            }

            Assert::AreEqual (-1, snapshot.codeViews[0][0].sourceFileId, L"a NOP no line produced");
        }



        //  The view CODE2 opened is one the window keeps, top line and all,
        //  and reopens at start as the host reopens it.
        TEST_METHOD (TheSecondViewSurvivesARestart)
        {
            PaneRig                        rig;
            DebuggerViewSnapshot           snapshot;
            DebuggerViewState              restarted;
            DebuggerViewState::OpenViews   views;
            std::string                    text;



            rig.Type ("CODE2");
            snapshot = rig.Build();
            text     = DebuggerViewState::FormatOpenViews (snapshot);

            Assert::AreEqual (std::format ("code2={:04X}", snapshot.codeViews[1][0].address), text);

            views = DebuggerViewState::ParseOpenViews (text);
            Assert::IsTrue (views.code[1].has_value());
            restarted.OpenCodeViewAt (1, *views.code[1]);

            Assert::IsTrue (PaneRig::GetAddresses (snapshot.codeViews[1]) == PaneRig::GetAddresses (restarted.Build (rig.Session()).codeViews[1]));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MoreViewNamesTests
    //
    //  AppleWin has two code windows and two data windows; this window has
    //  four of each, but the names stop at 2. The rest open from the menus.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MoreViewNamesTests)
    {
    public:

        TEST_METHOD (ThereIsNoCodeThreeOrDataThree)
        {
            for (const char * name : { "CODE3", "CODE4", "DATA3", "DATA4", "CODE0" })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  before = rig.Build();
                std::string           detail = std::string (name) + " is not a command.";



                PaneRig::AssertReply (rig.Type (name), CommandStatus::Unknown, "unknown command", detail,
                                      { "Error: unknown command", "       " + detail }, name);
                PaneRig::AssertPanesUnchanged (before, rig.Build(), name);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DataCommandTests
    //
    //  DATA and DATA1 bring the first memory window forward and move nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DataCommandTests)
    {
    public:

        TEST_METHOD (DataBringsTheFirstMemoryWindowForward)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "DATA" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "DATA1" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "data" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/DATA1" },
                                               std::pair<const char *, const char *> { "CASSO",    "DATA" } })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  before;
                DebuggerViewSnapshot  after;



                rig.Type (std::string ("MODE ") + mode);
                rig.Type (std::string (mode) == "MONITOR" ? "/MD1 2000" : "MD1 2000");
                before = rig.Build();

                PaneRig::AssertShown (rig.Type (line), line);

                after = rig.Build();
                Assert::AreEqual (DebuggerLayout::GetMemoryPaneId (1), after.showPane, PaneRig::Widen (line).c_str());
                Assert::AreEqual (before.showPaneSerial + 1,          after.showPaneSerial);

                after.showPane       = before.showPane;
                after.showPaneSerial = before.showPaneSerial;
                PaneRig::AssertPanesUnchanged (before, after, line);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DataTwoCommandTests
    //
    //  DATA2 brings the second memory window forward, opening it where the
    //  first one is when it is closed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DataTwoCommandTests)
    {
    public:

        TEST_METHOD (DataTwoOpensTheSecondWindowAtTheFirstsAddress)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "DATA2" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "data2" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/DATA2" },
                                               std::pair<const char *, const char *> { "CASSO",    "DATA2" } })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  before;
                DebuggerViewSnapshot  after;



                rig.Type (std::string ("MODE ") + mode);
                rig.Type (std::string (mode) == "MONITOR" ? "/MD1 2000" : "MD1 2000");
                before = rig.Build();
                Assert::AreEqual ((size_t) 1, before.memoryWindows.size());

                PaneRig::AssertShown (rig.Type (line), line);

                after = rig.Build();
                Assert::AreEqual ((size_t) 2,                          after.memoryWindows.size(), PaneRig::Widen (line).c_str());
                Assert::AreEqual (2,                                   after.memoryWindows[1].id);
                Assert::AreEqual ((Word) 0x2000,                       after.memoryWindows[1].first);
                Assert::AreEqual (DebuggerLayout::GetMemoryPaneId (2), after.showPane);
                Assert::AreEqual (before.showPaneSerial + 1,           after.showPaneSerial);
                Assert::AreEqual ((Word) 0x2000,                       after.memoryWindows[0].first, L"the first stays");
            }
        }



        TEST_METHOD (AnOpenSecondWindowStaysWhereItIs)
        {
            PaneRig               rig;
            DebuggerViewSnapshot  snapshot;



            rig.Type ("MD1 2000");
            rig.Type ("DATA2");
            rig.Type ("MD1 3000");
            PaneRig::AssertShown (rig.Type ("DATA2"), "DATA2");

            snapshot = rig.Build();
            Assert::AreEqual ((Word) 0x2000, snapshot.memoryWindows[1].first, L"window 2 did not follow window 1");
            Assert::AreEqual ((Word) 0x3000, snapshot.memoryWindows[0].first);
        }



        TEST_METHOD (TheSecondWindowSurvivesARestart)
        {
            PaneRig                        rig;
            DebuggerViewState              restarted;
            DebuggerViewState::OpenViews   views;
            std::string                    text;



            rig.Type ("MD1 2468");
            rig.Type ("DATA2");
            text = DebuggerViewState::FormatOpenViews (rig.Build());

            Assert::AreEqual (std::string ("memory2=2460"), text);

            views = DebuggerViewState::ParseOpenViews (text);
            Assert::IsTrue (views.memory[1].has_value());
            restarted.OpenMemoryWindow (2, *views.memory[1]);

            Assert::AreEqual ((Word) 0x2460, restarted.Build (rig.Session()).memoryWindows[1].first);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConsoleCommandTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConsoleCommandTests)
    {
    public:

        TEST_METHOD (ConsoleBringsTheConsoleForward)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "CONSOLE" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "console" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/CONSOLE" },
                                               std::pair<const char *, const char *> { "CASSO",    "CONSOLE" } })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  before;
                DebuggerViewSnapshot  after;



                rig.Type (std::string ("MODE ") + mode);
                before = rig.Build();

                PaneRig::AssertShown (rig.Type (line), line);

                after = rig.Build();
                Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), after.showPane, PaneRig::Widen (line).c_str());
                Assert::AreEqual (before.showPaneSerial + 1,              after.showPaneSerial);

                after.showPane       = before.showPane;
                after.showPaneSerial = before.showPaneSerial;
                PaneRig::AssertPanesUnchanged (before, after, line);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShowPaneSerialTests
    //
    //  The window brings a pane forward only when the serial changes, so each
    //  name must change it, even asking again for the pane it just showed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ShowPaneSerialTests)
    {
    public:

        TEST_METHOD (EveryPaneNameChangesTheSerialEveryTime)
        {
            static const std::pair<const char *, std::wstring>  kNames[] =
            {
                { "CODE",    DebuggerLayout::GetCodePaneId (0)   },
                { "CODE1",   DebuggerLayout::GetCodePaneId (0)   },
                { "CODE2",   DebuggerLayout::GetCodePaneId (1)   },
                { "DATA",    DebuggerLayout::GetMemoryPaneId (1) },
                { "DATA1",   DebuggerLayout::GetMemoryPaneId (1) },
                { "DATA2",   DebuggerLayout::GetMemoryPaneId (2) },
                { "CONSOLE", DebuggerLayout::kConsole            },
            };
            PaneRig    rig;
            uint32_t   serial = rig.Build().showPaneSerial;



            for (const auto & [name, pane] : kNames)
            {
                for (int again = 0; again < 2; again++)
                {
                    DebuggerViewSnapshot  snapshot;



                    rig.Type (name);
                    snapshot = rig.Build();

                    Assert::AreEqual (serial + 1, snapshot.showPaneSerial, PaneRig::Widen (name).c_str());
                    Assert::AreEqual (pane,       snapshot.showPane,       PaneRig::Widen (name).c_str());

                    serial = snapshot.showPaneSerial;
                }
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LayoutCommandTests
    //
    //  AppleWin's WIN, WINDOW and \ pick a layout. This window shows every
    //  pane at once, so they say so and change nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LayoutCommandTests)
    {
    public:

        TEST_METHOD (TheLayoutNamesSayEveryPaneIsShown)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "WIN" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "WINDOW" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "\\" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "win" },
                                               std::pair<const char *, const char *> { "APPLEWIN", " Window" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/WIN" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/\\" },
                                               std::pair<const char *, const char *> { "CASSO",    "WINDOW" } })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  before;
                Reply                 reply;



                rig.Type (std::string ("MODE ") + mode);
                rig.Type ("CODE2");
                before = rig.Build();

                reply = rig.Type (line);
                PaneRig::AssertReply (reply, CommandStatus::Ok, "", "", { "This window shows every pane at once." }, line);
                Assert::AreEqual (std::string (line), reply.command);

                PaneRig::AssertPanesUnchanged (before, rig.Build(), line);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceCommandTests
    //
    //  AppleWin's SOURCE1 and SOURCE2 show an assembler listing it is linked
    //  to, which Casso has no link to.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceCommandTests)
    {
    public:

        TEST_METHOD (TheSourceNamesAreNotAvailable)
        {
            static const char * const  kDetail = "SOURCE needs a link to an assembler listing.";



            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN", "SOURCE1" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "SOURCE2" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "source2" },
                                               std::pair<const char *, const char *> { "APPLEWIN", "SOURCE" },
                                               std::pair<const char *, const char *> { "MONITOR",  "/SOURCE1" },
                                               std::pair<const char *, const char *> { "CASSO",    "SOURCE2" } })
            {
                PaneRig                   rig;
                DebuggerViewSnapshot      before;
                std::vector<std::string>  text = { "Error: command not available", std::string ("       ") + kDetail };



                rig.Type (std::string ("MODE ") + mode);
                before = rig.Build();

                if (std::string (mode) == "MONITOR")
                {
                    text.insert (text.begin(), "ERR");
                }

                PaneRig::AssertReply (rig.Type (line), CommandStatus::NotAvailable, "command not available", kDetail, text, line);
                PaneRig::AssertPanesUnchanged (before, rig.Build(), line);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ScreenViewCommandTests
    //
    //  AppleWin's screen views show the machine's screen in the debugger. Here
    //  the emulator window shows it, so every one is not available.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static const char * const  s_kScreenViews[] =
    {
        "TEXT", "TEXT1", "TEXT2", "TEXT80", "TEXT81", "TEXT82", "TEXT40", "TEXT41", "TEXT42",
        "GR", "GR1", "GR2", "DGR", "DGR1", "DGR2",
        "HGR", "HGR0", "HGR1", "HGR2", "HGR3", "HGR4", "HGR5", "HGR6", "HGR7", "HGR8",
        "DHGR", "DHGR1", "DHGR2",
    };

    static const char * const  s_kAppearance[] = { "BW", "COLOR", "FONT", "HCOLOR", "MONO" };



    TEST_CLASS (ScreenViewCommandTests)
    {
    public:

        static std::string GetDetail (const std::string & name)
        {
            return name + " is not available here: the emulator window shows the screen.";
        }



        TEST_METHOD (EveryScreenViewIsNotAvailable)
        {
            PaneRig               rig;
            DebuggerViewSnapshot  before = rig.Build();



            Assert::AreEqual ((size_t) 28, std::size (s_kScreenViews));

            for (const char * name : s_kScreenViews)
            {
                PaneRig::AssertReply (rig.Type (name), CommandStatus::NotAvailable, "command not available", GetDetail (name),
                                      { "Error: command not available", "       " + GetDetail (name) }, name);
            }

            PaneRig::AssertPanesUnchanged (before, rig.Build(), "the screen views");
        }



        //  The name in the reply is the table's, whatever was typed.
        TEST_METHOD (EveryFormOfAScreenView)
        {
            for (const auto & [mode, line, name] : { std::tuple<const char *, const char *, const char *> { "APPLEWIN", "hgr",       "HGR"   },
                                                     std::tuple<const char *, const char *, const char *> { "APPLEWIN", "Text80 1",  "TEXT80" },
                                                     std::tuple<const char *, const char *, const char *> { "MONITOR",  "/HGR",      "HGR"   },
                                                     std::tuple<const char *, const char *, const char *> { "CASSO",    "dhgr2",     "DHGR2" } })
            {
                PaneRig                   rig;
                std::vector<std::string>  text = { "Error: command not available", "       " + GetDetail (name) };



                rig.Type (std::string ("MODE ") + mode);

                if (std::string (mode) == "MONITOR")
                {
                    text.insert (text.begin(), "ERR");
                }

                PaneRig::AssertReply (rig.Type (line), CommandStatus::NotAvailable, "command not available", GetDetail (name), text, line);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AppearanceCommandTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AppearanceCommandTests)
    {
    public:

        TEST_METHOD (EveryAppearanceNameIsNotAvailable)
        {
            PaneRig               rig;
            DebuggerViewSnapshot  before = rig.Build();



            for (const char * name : s_kAppearance)
            {
                std::string  detail = std::string (name) + " is not available here: the Casso theme sets the window's colors and font.";



                PaneRig::AssertReply (rig.Type (name), CommandStatus::NotAvailable, "command not available", detail,
                                      { "Error: command not available", "       " + detail }, name);
            }

            PaneRig::AssertReply (rig.Type ("mono"), CommandStatus::NotAvailable, "command not available",
                                  "MONO is not available here: the Casso theme sets the window's colors and font.",
                                  { "Error: command not available", "       MONO is not available here: the Casso theme sets the window's colors and font." },
                                  "mono");
            PaneRig::AssertPanesUnchanged (before, rig.Build(), "the appearance names");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WindowOnlyTableTests
    //
    //  Every name the table marks as needing the window, swept against this
    //  test's own list of them by family, both ways: a name the table gains,
    //  loses or moves to another family changes its reply, and is caught here.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WindowOnlyTableTests)
    {
    public:

        static std::map<std::string, AppleWinCommandFamily> GetExpectedFamilies()
        {
            std::map<std::string, AppleWinCommandFamily>  families;



            for (const char * name : { ".", "RET", "^", "V", "->", "PAGEUP", "PAGEUP256", "PAGEUP4K", "PAGEDN", "PAGEDOWN256", "PAGEDOWN4K" })
            {
                families[name] = AppleWinCommandFamily::Cursor;
            }

            for (const char * name : { "WIN", "WINDOW", "CODE", "CODE1", "CODE2", "CONSOLE", "DATA", "DATA1", "DATA2", "SOURCE1", "SOURCE2", "\\" })
            {
                families[name] = AppleWinCommandFamily::Window;
            }

            for (const char * name : { "MD1", "MD2", "MA1", "MA2", "MT1", "MT2", "M1", "M2" })
            {
                families[name] = AppleWinCommandFamily::MiniMemory;
            }

            for (const char * name : s_kScreenViews)
            {
                families[name] = AppleWinCommandFamily::Views;
            }

            for (const char * name : s_kAppearance)
            {
                families[name] = AppleWinCommandFamily::Appearance;
            }

            return families;
        }



        TEST_METHOD (TheTableHoldsExactlyTheseSixtyFourNames)
        {
            std::map<std::string, AppleWinCommandFamily>  expected = GetExpectedFamilies();
            size_t                                        found    = 0;



            Assert::AreEqual ((size_t) 64, expected.size());

            for (const AppleWinCommand & entry : AppleWinCommandTable::GetAll())
            {
                if (entry.availability != CommandAvailability::WindowOnly)
                {
                    continue;
                }

                found++;
                Assert::IsTrue (expected.contains (entry.name), PaneRig::Widen (std::string (entry.name) + " is not in this test's list").c_str());
                Assert::IsTrue (expected[entry.name] == entry.family, PaneRig::Widen (std::string (entry.name) + " moved family").c_str());
            }

            Assert::AreEqual (expected.size(), found, L"every name in this test's list is in the table");
        }



        //  Each name typed into the window replies as its family replies. The
        //  exact reply of each is in its own test; this pins the family.
        TEST_METHOD (EveryNameRepliesAsItsFamily)
        {
            static const char * const                     kSource     = "SOURCE needs a link to an assembler listing.";
            static const char * const                     kViews      = " is not available here: the emulator window shows the screen.";
            static const char * const                     kAppearance = " is not available here: the Casso theme sets the window's colors and font.";
            std::map<std::string, AppleWinCommandFamily>  expected    = GetExpectedFamilies();



            for (const auto & [name, family] : expected)
            {
                PaneRig       rig;
                std::string   line   = (family == AppleWinCommandFamily::MiniMemory) ? name + " 1000" : name;
                Reply         reply;
                std::wstring  where  = PaneRig::Widen (line);



                rig.SetPc (0x0500);                         // JMP $0300, so -> has an address
                reply = rig.Type (line);

                Assert::AreEqual (line, reply.command,             where.c_str());
                Assert::IsTrue   (reply.verb == DebugVerb::None,   where.c_str());

                switch (family)
                {
                case AppleWinCommandFamily::Cursor:
                    Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
                    Assert::AreEqual ((size_t) 1,              reply.text.size(),  where.c_str());
                    Assert::IsTrue   (reply.text[0].starts_with ("The code pane "), where.c_str());
                    break;

                case AppleWinCommandFamily::MiniMemory:
                    PaneRig::AssertReply (reply, CommandStatus::Ok, "", "", { "The memory pane is at $1000." }, line);
                    break;

                case AppleWinCommandFamily::Window:
                    if (name.starts_with ("SOURCE"))
                    {
                        PaneRig::AssertReply (reply, CommandStatus::NotAvailable, "command not available", kSource,
                                              { "Error: command not available", std::string ("       ") + kSource }, line);
                    }
                    else if (name == "WIN" || name == "WINDOW" || name == "\\")
                    {
                        PaneRig::AssertReply (reply, CommandStatus::Ok, "", "", { "This window shows every pane at once." }, line);
                    }
                    else
                    {
                        PaneRig::AssertReply (reply, CommandStatus::Ok, "", "", {}, line);
                    }

                    break;

                case AppleWinCommandFamily::Views:
                    PaneRig::AssertReply (reply, CommandStatus::NotAvailable, "command not available", name + kViews,
                                          { "Error: command not available", "       " + name + kViews }, line);
                    break;

                default:
                    PaneRig::AssertReply (reply, CommandStatus::NotAvailable, "command not available", name + kAppearance,
                                          { "Error: command not available", "       " + name + kAppearance }, line);
                    break;
                }
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PanelRig
    //
    //  A session over the mock target with a provider that stands for no real
    //  device. A real machine's panels are fixed by its devices; this one has
    //  an id and a title that differ in more than case, so a match by title is
    //  a match by title, and the tests can add or remove devices at will.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PanelRig
    {
    public:
        TestCpu                    cpu;
        MockDebugTarget            target;
        RecordingNotificationSink  sink;
        DebugSession               session { target, sink, RunState::Paused };
        DebugHandlerSet            handlers;
        FakeDiagnosticsProvider    provider;
        DebuggerViewState          view;



        PanelRig()
        {
            cpu.InitForTest();
            target.instructionSet = cpu.GetInstructionSet();
            handlers.Attach (session);
            provider.title              = "Widget";
            target.diagnosticsProviders = { &provider };
        }



        Reply Type (const std::string & line)
        {
            return view.ExecuteWindowLine (session, line, session.GetMode());
        }



        //  The dialect set as the window's Dialect menu sets it: MODE, in the
        //  words of the mode in force.
        void SetMode (CommandMode mode, const std::string & name)
        {
            Reply  reply = Type (DebuggerViewState::GetModeLine ("MODE " + name, session.GetMode()));



            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, PaneRig::Widen (name).c_str());
            Assert::IsTrue   (session.GetMode() == mode, PaneRig::Widen (name).c_str());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PanelCommandTests
    //
    //  PANEL opens, closes and lists the machine's device panels. It is the
    //  window's own, in every dialect, since the Panels menu sends it in the
    //  dialect's words.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PanelCommandTests)
    {
    public:

        //  An id and a title to a line, the id in fourteen columns, and the
        //  open ones marked.
        TEST_METHOD (TheListGivesEachPanelsIdAndTitle)
        {
            PanelRig                 rig;
            FakeDiagnosticsProvider  second;



            second.id    = "clockwork";
            second.title = "Clockwork";
            rig.target.diagnosticsProviders.push_back (&second);

            PaneRig::AssertReply (rig.Type ("PANEL LIST"), CommandStatus::Ok, "", "",
                                  { "fake          Widget", "clockwork     Clockwork" }, "PANEL LIST");

            rig.Type ("PANEL clockwork");

            PaneRig::AssertReply (rig.Type ("PANEL"), CommandStatus::Ok, "", "",
                                  { "fake          Widget", "clockwork     Clockwork (open)" }, "PANEL alone lists");
            PaneRig::AssertReply (rig.Type ("panel list"), CommandStatus::Ok, "", "",
                                  { "fake          Widget", "clockwork     Clockwork (open)" }, "in any case");
        }



        TEST_METHOD (AMachineWithNoPanelsSaysSo)
        {
            PanelRig  rig;



            rig.target.diagnosticsProviders.clear();
            PaneRig::AssertReply (rig.Type ("PANEL LIST"), CommandStatus::Ok, "", "", { "This machine has no device panels." }, "PANEL LIST");
        }



        //  By id or by title, either case; the reply gives the title, and the
        //  next build carries the open panel's rows.
        TEST_METHOD (APanelOpensAndClosesByIdOrTitle)
        {
            PanelRig              rig;
            DebuggerViewSnapshot  snapshot;



            PaneRig::AssertReply (rig.Type ("panel FAKE"), CommandStatus::Ok, "", "", { "The Widget panel is open." }, "by id");
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"));

            snapshot = rig.view.Build (rig.session);
            Assert::IsTrue   (snapshot.panels[0].open);
            Assert::AreEqual ((size_t) 1, snapshot.diagnostics.size());
            Assert::AreEqual (std::string ("fake"), snapshot.diagnostics[0].id);

            PaneRig::AssertReply (rig.Type ("PANEL CLOSE widget"), CommandStatus::Ok, "", "", { "The Widget panel is closed." }, "by title");
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"));
            Assert::IsTrue  (rig.view.Build (rig.session).diagnostics.empty());

            PaneRig::AssertReply (rig.Type ("PANEL WIDGET"), CommandStatus::Ok, "", "", { "The Widget panel is open." }, "open by title");
            Assert::IsTrue (rig.view.IsPanelOpen ("fake"));
        }



        TEST_METHOD (APanelTheMachineLacksIsAnError)
        {
            PanelRig  rig;



            PaneRig::AssertReply (rig.Type ("PANEL mmu"), CommandStatus::Error, "no such panel",
                                  "This machine has no mmu panel. Use PANEL LIST to list the ones it has.",
                                  { "Error: no such panel", "       This machine has no mmu panel. Use PANEL LIST to list the ones it has." }, "PANEL mmu");
            PaneRig::AssertReply (rig.Type ("PANEL CLOSE"), CommandStatus::Error, "invalid arguments",
                                  "Use PANEL LIST to list the device panels, PANEL name to open one, or PANEL CLOSE name to close one.",
                                  { "Error: invalid arguments",
                                    "       Use PANEL LIST to list the device panels, PANEL name to open one, or PANEL CLOSE name to close one." },
                                  "PANEL CLOSE");
            Assert::IsFalse (rig.view.IsPanelOpen ("mmu"));
            Assert::IsFalse (rig.view.IsPanelOpen ("fake"));
        }



        //  The Panels menu's lines, in the words of each dialect, open and close
        //  the panel. GSSquared and WinDbg lines went to the session, which
        //  answered that PANEL needs the debugger window -- from inside it.
        TEST_METHOD (ThePanelsMenuWorksInEveryDialect)
        {
            static const std::pair<CommandMode, const char *>  kModes[] =
            {
                { CommandMode::AppleWin,  "APPLEWIN"  },
                { CommandMode::Casso,     "CASSO"     },
                { CommandMode::Monitor,   "MONITOR"   },
                { CommandMode::GSSquared, "GSSQUARED" },
                { CommandMode::WinDbg,    "WINDBG"    },
            };



            for (const auto & [mode, name] : kModes)
            {
                PanelRig     rig;
                std::string  open;
                std::string  close;



                rig.SetMode (mode, name);
                open  = DebuggerViewState::GetModeLine (DebuggerViewState::GetPanelLine ("fake", true),  mode);
                close = DebuggerViewState::GetModeLine (DebuggerViewState::GetPanelLine ("fake", false), mode);

                PaneRig::AssertReply (rig.Type (open), CommandStatus::Ok, "", "", { "The Widget panel is open." }, open);
                Assert::IsTrue (rig.view.IsPanelOpen ("fake"), PaneRig::Widen (open).c_str());
                Assert::IsTrue (rig.view.Build (rig.session).panels[0].open, PaneRig::Widen (open).c_str());

                PaneRig::AssertReply (rig.Type (close), CommandStatus::Ok, "", "", { "The Widget panel is closed." }, close);
                Assert::IsFalse (rig.view.IsPanelOpen ("fake"), PaneRig::Widen (close).c_str());
            }
        }



        //  Typed rather than sent by the menu: WinDbg's marker with a blank
        //  after it, GSSquared's lowercase, and Monitor's `/` after blanks.
        TEST_METHOD (EveryTypedFormOfPanel)
        {
            static const std::tuple<CommandMode, const char *, const char *>  kLines[] =
            {
                { CommandMode::WinDbg,    "WINDBG",    "! panel Widget" },
                { CommandMode::WinDbg,    "WINDBG",    "!PANEL fake"    },
                { CommandMode::GSSquared, "GSSQUARED", "panel widget"   },
                { CommandMode::Monitor,   "MONITOR",   "  /PANEL fake"  },
            };



            for (const auto & [mode, name, line] : kLines)
            {
                PanelRig  rig;



                rig.SetMode (mode, name);
                PaneRig::AssertReply (rig.Type (line), CommandStatus::Ok, "", "", { "The Widget panel is open." }, line);
                Assert::IsTrue (rig.view.IsPanelOpen ("fake"), PaneRig::Widen (line).c_str());
            }
        }



        //  A real //e's MMU panel, opened from each dialect, arrives with its
        //  map in the next build.
        TEST_METHOD (TheMmuPanelOfARealMachineInEveryDialect)
        {
            for (const auto & [mode, line] : { std::pair<const char *, const char *> { "APPLEWIN",  "PANEL mmu" },
                                               std::pair<const char *, const char *> { "GSSQUARED", "PANEL mmu" },
                                               std::pair<const char *, const char *> { "WINDBG",    "!PANEL mmu" } })
            {
                PaneRig               rig;
                DebuggerViewSnapshot  snapshot;



                rig.Type (DebuggerViewState::GetModeLine (std::string ("MODE ") + mode, rig.Session().GetMode()));
                PaneRig::AssertReply (rig.Type (line), CommandStatus::Ok, "", "", { "The MMU panel is open." }, line);

                snapshot = rig.Build();
                Assert::AreEqual ((size_t) 1,           snapshot.diagnostics.size(), PaneRig::Widen (line).c_str());
                Assert::AreEqual (std::string ("mmu"),  snapshot.diagnostics[0].id);
                Assert::IsTrue   (std::holds_alternative<DiagnosticsMemoryMap> (snapshot.diagnostics[0].visual));
            }
        }
    };
}
