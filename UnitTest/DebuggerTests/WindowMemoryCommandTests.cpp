#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Widgets/DxuiHexView.h"
#include "WindowConsoleRig.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace WindowMemoryCommandTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryWindowRig
    //
    //  The console and the window's first memory pane: a real MemoryPane over
    //  a real hex view, laid out with no HWND.
    //
    //  THE WINDOW'S PART IS PLAYED HERE, and that is a limit worth stating.
    //  DebuggerWindow cannot be driven without an HWND, so Frame does what one
    //  of its frames does with a memory window: DebuggerWindow::
    //  ApplyMemoryWindows acts on a Go to once and hands the pane its bytes,
    //  RenderFrame calls FollowScroll, and the pane's move goes to
    //  EmulatorShell::SetDebugView, which moves window 1 with SetMemoryAddress.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MemoryWindowRig : public WindowConsoleRig
    {
    public:
        DxuiHexView  hexView;
        MemoryPane   pane;
        uint32_t     goToSerial = 0;



        MemoryWindowRig() :
            pane (1, &hexView, [this] (int id, Word first) { Move (id, first); },
                  [] (const std::string &) {}, [] (const std::string &) {})
        {
            static constexpr int  kDpi = 96;
            DxuiDpiScaler         scaler;



            scaler.SetDpi (kDpi);
            pane.Configure (nullptr);
            hexView.Layout (RECT { 0, 0, 640, 240 }, scaler);
        }



        //  What the shell does with a pane's move.
        void Move (int id, Word first)
        {
            if (id == 1)
            {
                view.SetMemoryAddress (first);
            }
            else
            {
                view.OpenMemoryWindow (id, first);
            }
        }



        //  One frame of the window, for memory window 1.
        DebuggerViewSnapshot Frame()
        {
            DebuggerViewSnapshot  snapshot = Build();



            if (snapshot.goTo.has_value() && snapshot.goTo->serial != goToSerial)
            {
                goToSerial = snapshot.goTo->serial;

                if (snapshot.goTo->address.has_value() && snapshot.goTo->window == pane.GetId())
                {
                    pane.GoTo (*snapshot.goTo->address);
                }
            }

            for (const DebuggerViewSnapshot::MemoryWindow & window : snapshot.memoryWindows)
            {
                if (window.id == pane.GetId())
                {
                    pane.Apply (window);
                }
            }

            pane.FollowScroll();
            return snapshot;
        }



        //  Frames until the pane and the view state stop moving each other.
        DebuggerViewSnapshot Settle()
        {
            static constexpr int  kFrames = 4;
            DebuggerViewSnapshot  snapshot;



            for (int frame = 0; frame < kFrames; frame++)
            {
                snapshot = Frame();
            }

            return snapshot;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MiniMemoryCommandTests
    //
    //  MD, MA, MT and M, in both forms, move memory window 1 to a hex address,
    //  as the help for each says and docs/Debugger.md describes: "MD1 addr (or
    //  MA1, MT1, and the 2 forms) moves the memory pane".
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MiniMemoryCommandTests)
    {
    public:

        static void AssertMoved (const Reply & reply, const std::string & line, Word address)
        {
            WindowConsoleRig::AssertReply (reply, CommandStatus::Ok, "", "", { std::format ("The memory pane is at ${:04X}.", address) }, line);
            Assert::AreEqual (line, reply.command, WindowConsoleRig::Widen (line).c_str());
            Assert::IsTrue   (reply.verb == DebugVerb::None, WindowConsoleRig::Widen (line).c_str());
        }



        static void AssertRefused (const Reply & reply, const std::string & line, const std::string & name, bool isMonitor = false)
        {
            std::string               detail = name + " needs a hex address.";
            std::vector<std::string>  text   = { "Error: invalid arguments", "       " + detail };



            if (isMonitor)
            {
                text.insert (text.begin(), "ERR");
            }

            WindowConsoleRig::AssertReply (reply, CommandStatus::Error, "invalid arguments", detail, text, line);
        }



        //  All eight names, each moving window 1 and none opening window 2. The
        //  window is told as a Go to, one new one per command.
        TEST_METHOD (EveryNameMovesTheFirstMemoryWindow)
        {
            static const std::pair<const char *, Word>  kLines[] =
            {
                { "MD1 1000", 0x1000 }, { "MD2 2000", 0x2000 }, { "MA1 3000", 0x3000 }, { "MA2 4000", 0x4000 },
                { "MT1 5000", 0x5000 }, { "MT2 6000", 0x6000 }, { "M1 7000",  0x7000 }, { "M2 8000",  0x8000 },
            };
            WindowConsoleRig      rig;
            DebuggerViewSnapshot  snapshot;
            uint32_t              serial = 0;



            for (const auto & [line, address] : kLines)
            {
                AssertMoved (rig.Type (line), line, address);

                snapshot = rig.Build();
                Assert::AreEqual ((size_t) 1,  snapshot.memoryWindows.size(), L"window 2 stays closed");
                Assert::AreEqual (1,           snapshot.memoryWindows[0].id);
                Assert::AreEqual (address,     snapshot.memoryWindows[0].first);
                Assert::AreEqual ((size_t) DebuggerViewState::kMemoryWindowBytes, snapshot.memoryWindows[0].bytes.size());
                Assert::AreEqual (address,     rig.view.GetMemoryAddress());

                Assert::IsTrue   (snapshot.goTo.has_value());
                Assert::AreEqual (1,           snapshot.goTo->window);
                Assert::IsTrue   (snapshot.goTo->address == std::optional<Word> (address));
                Assert::AreEqual (serial + 1,  snapshot.goTo->serial, L"a new Go to for each");

                serial = snapshot.goTo->serial;
            }
        }



        //  The bytes window 1 holds are the machine's, from the row the address
        //  is on.
        TEST_METHOD (TheWindowHoldsTheMachinesBytes)
        {
            WindowConsoleRig      rig;
            DebuggerViewSnapshot  snapshot;



            rig.Poke (0x2345, { 0x11, 0x22, 0x33, 0x44 });
            AssertMoved (rig.Type ("MD1 2345"), "MD1 2345", 0x2345);

            snapshot = rig.Build();
            Assert::AreEqual ((Word) 0x2340, snapshot.memoryWindows[0].first);

            for (size_t i = 0; i < 4; i++)
            {
                Assert::IsTrue (snapshot.memoryWindows[0].bytes[5 + i] == std::optional<Byte> (rig.Peek ((Word) (0x2345 + i))));
            }
        }



        //  Any case, a `$`, leading zeros, a trailing token ignored, Monitor's
        //  `/` and Casso mode.
        TEST_METHOD (EveryFormOfTheAddress)
        {
            static const std::tuple<const char *, const char *, Word>  kLines[] =
            {
                { "APPLEWIN", "md1 1000",      0x1000 },
                { "APPLEWIN", "MD1 $1000",     0x1000 },
                { "APPLEWIN", "MD1 000001000", 0x1000 },
                { "APPLEWIN", "md2 ffff",      0xFFFF },
                { "APPLEWIN", "MD1 0",         0x0000 },
                { "APPLEWIN", "MD1 1000 2000", 0x1000 },
                { "MONITOR",  "/MT2 $2000",    0x2000 },
                { "MONITOR",  "  / m1 2000",   0x2000 },
                { "CASSO",    "MA1 2000",      0x2000 },
            };



            for (const auto & [mode, line, address] : kLines)
            {
                WindowConsoleRig  rig;



                rig.Type (std::string ("MODE ") + mode);
                rig.Type ("MD1 4000");

                AssertMoved (rig.Type (line), line, address);
                Assert::AreEqual ((Word) (address & ~0x000F), rig.Build().memoryWindows[0].first, WindowConsoleRig::Widen (line).c_str());
            }
        }



        //  Anything but one to four hex digits -- a symbol, a register or an
        //  expression included, since the help calls for a hex address -- is
        //  an error that gives the name as the table has it, and nothing moves.
        TEST_METHOD (AnythingButAHexAddressIsAnError)
        {
            static const std::pair<const char *, const char *>  kLines[] =
            {
                { "MA1",           "MA1" },
                { "MD1 $",         "MD1" },
                { "MD1 XYZ",       "MD1" },
                { "MD1 10000",     "MD1" },
                { "MD1 12G",       "MD1" },
                { "MD1 0x10",      "MD1" },
                { "md2 -1",        "MD2" },
                { "MD1 COUT",      "MD1" },
                { "MD1 PC",        "MD1" },
                { "MD1 300+10",    "MD1" },
            };
            WindowConsoleRig      rig;
            DebuggerViewSnapshot  before;
            DebuggerViewSnapshot  after;



            rig.Type ("MD1 4000");
            before = rig.Build();

            for (const auto & [line, name] : kLines)
            {
                AssertRefused (rig.Type (line), line, name);

                after = rig.Build();
                Assert::AreEqual ((Word) 0x4000,        after.memoryWindows[0].first, WindowConsoleRig::Widen (line).c_str());
                Assert::AreEqual (before.goTo->serial,  after.goTo->serial,           L"and no Go to");
            }

            rig.Type ("MODE MONITOR");
            AssertRefused (rig.Type ("/MD1 XYZ"), "/MD1 XYZ", "MD1", true);
        }



        //  Near $FFFF the window holds what is left of memory; at $0000 a full
        //  window.
        TEST_METHOD (TheWindowStopsAtTheTopOfMemory)
        {
            static const std::tuple<const char *, Word, size_t>  kLines[] =
            {
                { "MD1 FFFF", 0xFFF0, 16  },
                { "MD1 FFF5", 0xFFF0, 16  },
                { "MD1 FE11", 0xFE10, 496 },
                { "MD1 FE01", 0xFE00, 512 },
                { "MD1 0",    0x0000, 512 },
            };
            WindowConsoleRig  rig;



            for (const auto & [line, first, count] : kLines)
            {
                DebuggerViewSnapshot  snapshot;



                rig.Type (line);
                snapshot = rig.Build();

                Assert::AreEqual (first, snapshot.memoryWindows[0].first,        WindowConsoleRig::Widen (line).c_str());
                Assert::AreEqual (count, snapshot.memoryWindows[0].bytes.size(), WindowConsoleRig::Widen (line).c_str());
            }
        }



        //  THE PANE ON SCREEN MOVES, and stays. Once placed, a memory pane keeps
        //  its rows wherever a snapshot's bytes start and asks for a read back
        //  where its rows are, so a command that only moved the read was undone
        //  a frame later with its reply still saying it had moved.
        TEST_METHOD (ThePaneOnScreenMovesAndStays)
        {
            MemoryWindowRig       rig;
            DebuggerViewSnapshot  snapshot;



            rig.Settle();
            Assert::AreEqual ((Word) 0x0000, rig.pane.GetTopAddress(), L"placed at $0000");

            AssertMoved (rig.Type ("MD1 2000"), "MD1 2000", 0x2000);

            snapshot = rig.Settle();
            Assert::AreEqual ((Word) 0x2000, rig.pane.GetTopAddress(),     L"the rows on screen start at $2000");
            Assert::AreEqual ((Word) 0x2000, rig.view.GetMemoryAddress(),  L"and nothing moved the window back");
            Assert::AreEqual ((Word) 0x2000, snapshot.memoryWindows[0].first);

            rig.Type ("MT2 8123");
            rig.Settle();
            Assert::AreEqual ((Word) 0x8123, rig.pane.GetTopAddress(), L"a second move, off a row boundary");
        }



        //  The rows start at the address itself, as the Go to box starts them,
        //  while the window reads from the row boundary below it.
        TEST_METHOD (AnAddressOffARowIsWhereTheRowsStart)
        {
            MemoryWindowRig       typed;
            MemoryWindowRig       box;
            DebuggerViewSnapshot  snapshot;



            typed.Settle();
            box.Settle();

            typed.Type ("MD1 1003");
            box.view.RequestGoTo (box.Session(), 1, "1003");

            snapshot = typed.Settle();
            box.Settle();

            Assert::AreEqual ((Word) 0x1003,        typed.pane.GetTopAddress());
            Assert::AreEqual (box.pane.GetTopAddress(), typed.pane.GetTopAddress(), L"as the Go to box does");
            Assert::AreEqual ((Word) 0x1000,        snapshot.memoryWindows[0].first);
        }



        //  The address is the view state's, not the machine's: a run and a stop
        //  leave it. The window's preferences do not keep window 1, so a new
        //  window starts it at $0000.
        TEST_METHOD (TheAddressHoldsAcrossARunButNotARestart)
        {
            WindowConsoleRig      rig;
            DebuggerViewSnapshot  snapshot;
            DebuggerViewState     restarted;



            rig.Fill (0x0300, 0x0500, 0xEA);
            rig.Poke (0x0500, { 0x4C, 0x00, 0x03 });
            rig.SetPc (0x0300);
            rig.Type ("MD1 2000");

            rig.Type ("G");
            rig.RunFrames (2);
            rig.controller.RequestPause();
            rig.RunFrames (1);
            Assert::IsFalse (rig.controller.IsRunInProgress());

            snapshot = rig.Build();
            Assert::AreEqual ((Word) 0x2000, snapshot.memoryWindows[0].first);
            Assert::AreEqual (std::string(), DebuggerViewState::FormatOpenViews (snapshot), L"window 1 is not kept");
            Assert::AreEqual ((Word) 0x0000, restarted.GetMemoryAddress());
        }



        //  Typed while the machine runs, the move lands in the next frame's
        //  build and the run goes on.
        TEST_METHOD (AMoveDuringARunLandsAtTheNextFrame)
        {
            MemoryWindowRig       rig;
            DebuggerViewSnapshot  snapshot;



            rig.Fill (0x0300, 0x0500, 0xEA);
            rig.Poke (0x0500, { 0x4C, 0x00, 0x03 });
            rig.SetPc (0x0300);
            rig.Settle();

            rig.Type ("G");
            rig.RunFrames (1);
            AssertMoved (rig.Type ("MD1 2000"), "MD1 2000", 0x2000);

            snapshot = rig.Settle();
            Assert::IsTrue   (rig.controller.IsRunInProgress(), L"MD1 does not stop the run");
            Assert::AreEqual ((Word) 0x2000, snapshot.memoryWindows[0].first);
            Assert::AreEqual ((Word) 0x2000, rig.pane.GetTopAddress());

            rig.controller.RequestPause();
            rig.RunFrames (1);
        }
    };
}
