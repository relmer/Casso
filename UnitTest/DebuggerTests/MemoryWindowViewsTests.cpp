#include "Pch.h"

#include "Ui/Debugger/Panes/BreakpointsPane.h"

#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/Source/SourcePathList.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "FakeDiagnosticsProvider.h"
#include "ControllerRig.h"
#include "DebuggerViewStateTestRig.h"
#include "HandlerTestRig.h"
#include "InMemoryPipeTransport.h"
#include "TestHelpers.h"
#include "MockDebugTarget.h"
#include "MockExpressionContext.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "Ui/Debugger/InstructionTouches.h"
#include "Ui/Debugger/InstructionEffect.h"
#include "Ui/Debugger/StopChanges.h"
#include "Core/UnicodeSymbols.h"
#include "UiTests/InMemoryFileSystem.h"
#include "UnreadableStackTarget.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/Panes/SoundBoardView.h"
#include "Core/TextEncoding.h"






using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerViewStateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryGoToTests
    //
    //  A Go to address from any expression or operand form.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MemoryGoToTests)
    {
    public:
        TEST_METHOD (GoToResolvesRegistersAndEveryAddressingForm)
        {
            MockExpressionContext         context;
            std::map<Word, Byte>          memory = { { 0x0010, 0x34 }, { 0x0011, 0x12 },
                                                     { 0x0014, 0x78 }, { 0x0015, 0x56 },
                                                     { 0x0300, 0xCD }, { 0x0301, 0xAB } };
            DebuggerViewState::GoToPeek   peek   = [&memory] (Word address) -> std::optional<Byte>
            {
                if (address >= 0xC000 && address < 0xC100) { return std::nullopt; }
                return memory.contains (address) ? memory[address] : (Byte) 0;
            };
            auto                          go     = [&] (const char * text) -> std::optional<Word>
            {
                Word         address = 0;
                std::string  error;

                return DebuggerViewState::TryResolveGoTo (text, context, peek, address, error) ? std::optional<Word> (address) : std::nullopt;
            };



            context.registers = { { "PC", 0x0812 }, { "A", 0x42 }, { "X", 0x04 }, { "Y", 0x02 }, { "S", 0xF6 }, { "P", 0x30 } };

            Assert::IsTrue (go ("0400")      == std::optional<Word> (0x0400));
            Assert::IsTrue (go ("$fff0")     == std::optional<Word> (0xFFF0));
            Assert::IsTrue (go ("pc")        == std::optional<Word> (0x0812));
            Assert::IsTrue (go ("A")         == std::optional<Word> (0x0042), L"A: its value on the zero page");
            Assert::IsTrue (go ("S")         == std::optional<Word> (0x01F6));
            Assert::IsTrue (go ("FE,X")      == std::optional<Word> (0x0002), L"zp,X wraps in the zero page");
            Assert::IsTrue (go ("10,Y")      == std::optional<Word> (0x0012));
            Assert::IsTrue (go ("0300,X")    == std::optional<Word> (0x0304));
            Assert::IsTrue (go ("0300,Y")    == std::optional<Word> (0x0302));
            Assert::IsTrue (go ("(10,X)")    == std::optional<Word> (0x5678), L"(zp,X): the pointer at zp+X");
            Assert::IsTrue (go ("($10),Y")   == std::optional<Word> (0x1236), L"(zp),Y: the pointer at zp, plus Y");
            Assert::IsTrue (go ("(0300)")    == std::optional<Word> (0xABCD));
            Assert::IsFalse (go ("(C000)").has_value(),  L"a pointer in I/O is not read");
            Assert::IsFalse (go ("(0300),Y").has_value(), L"(abs),Y is not a 6502 form");
            Assert::IsFalse (go ("KBD").has_value());
        }


        //  The memory address box reads its address with the console's own
        //  evaluator, so a symbol, decimal or arithmetic is an address there
        //  too, inside the 6502 addressing forms as much as on its own.
        TEST_METHOD (GoToTakesEveryExpressionTheConsoleTakes)
        {
            MockExpressionContext         context;
            std::map<Word, Byte>          memory = { { 0x0028, 0x00 }, { 0x0029, 0x04 },
                                                     { 0x002A, 0x80 }, { 0x002B, 0x07 } };
            DebuggerViewState::GoToPeek   peek   = [&memory] (Word address) -> std::optional<Byte>
            {
                return memory.contains (address) ? memory[address] : (Byte) 0;
            };
            std::string                   error;
            auto                          go     = [&] (const char * text) -> std::optional<Word>
            {
                Word  address = 0;

                return DebuggerViewState::TryResolveGoTo (text, context, peek, address, error) ? std::optional<Word> (address) : std::nullopt;
            };



            context.symbols["BASL"]   = 0x0028;
            context.symbols["BAS2L"]  = 0x002A;
            context.registers["X"]    = 0x02;
            context.registers["Y"]    = 0x05;

            Assert::IsTrue (go ("(basl),y")      == std::optional<Word> (0x0405), L"a symbol in (zp),Y, in lowercase");
            Assert::IsTrue (go ("(BASL),Y")      == std::optional<Word> (0x0405));
            Assert::IsTrue (go ("( BASL ) , Y")  == std::optional<Word> (0x0405), L"spaces anywhere");
            Assert::IsTrue (go ("(BASL,X)")      == std::optional<Word> (0x0780), L"(zp,X) over a symbol");
            Assert::IsTrue (go ("(BASL+2)")      == std::optional<Word> (0x0780), L"arithmetic inside the parentheses");
            Assert::IsTrue (go ("basl")          == std::optional<Word> (0x0028), L"a bare symbol");
            Assert::IsTrue (go ("BASL,X")        == std::optional<Word> (0x002A), L"a symbol, indexed");
            Assert::IsTrue (go ("home+3")        == std::optional<Word> (0xFC5B), L"symbol arithmetic");
            Assert::IsTrue (go ("PC+2")          == std::optional<Word> (0x0302), L"register arithmetic");
            Assert::IsTrue (go ("0n1024")        == std::optional<Word> (0x0400), L"decimal");
            Assert::IsTrue (go ("(0n40),Y")      == std::optional<Word> (0x0405), L"a decimal pointer");

            Assert::IsFalse (go ("(NOSUCH),Y").has_value());
            Assert::IsTrue  (error.find ("NOSUCH") != std::string::npos, L"the evaluator's own reason is kept");
            Assert::IsFalse (go ("(HOME),Y").has_value(), L"(zp),Y takes a zero-page pointer");
            Assert::IsFalse (go ("").has_value());
        }
    };





    TEST_CLASS (MemoryWindowTests)
    {
    public:

        static const DebuggerViewSnapshot::MemoryWindow & Window (const DebuggerViewSnapshot & snapshot, int id)
        {
            for (const DebuggerViewSnapshot::MemoryWindow & window : snapshot.memoryWindows)
            {
                if (window.id == id)
                {
                    return window;
                }
            }

            Assert::Fail (L"no such window in the snapshot");
            return snapshot.memoryWindows.front();
        }



        TEST_METHOD (TheFirstWindowIsAlwaysThere)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());



            Assert::AreEqual ((size_t) 1, snapshot.memoryWindows.size());
            Assert::AreEqual (1,          snapshot.memoryWindows[0].id);
            Assert::AreEqual ((size_t) DebuggerViewState::kMemoryWindowBytes, snapshot.memoryWindows[0].bytes.size());
            Assert::AreEqual (snapshot.memoryWindows[0].bytes.size(),         snapshot.memoryWindows[0].regions.size());
        }


        TEST_METHOD (MoreWindowsOpenAndClose)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.OpenMemoryWindow (2, 0x0300);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 2,     snapshot.memoryWindows.size());
            Assert::AreEqual ((Word) 0x0300,  Window (snapshot, 2).first);
            Assert::AreEqual ((Byte) 0xA9,    *Window (snapshot, 2).bytes[0]);
            Assert::AreEqual ((Byte) 0x8D,    *Window (snapshot, 2).bytes[2]);

            rig.view.CloseMemoryWindow (2);
            Assert::AreEqual ((size_t) 1, rig.view.Build (rig.controller.GetSession()).memoryWindows.size());
        }


        TEST_METHOD (OnlyWindowsTwoToFourOpenAndClose)
        {
            MachineRig  rig;



            rig.view.OpenMemoryWindow  (5, 0x0300);
            rig.view.CloseMemoryWindow (1);

            Assert::IsFalse (rig.view.GetMemoryWindowAddress (5).has_value());
            Assert::IsTrue  (rig.view.GetMemoryWindowAddress (1).has_value(), L"the first window cannot be closed");
        }


        TEST_METHOD (AWindowStartsOnARowBoundary)
        {
            MachineRig  rig;



            rig.view.OpenMemoryWindow (2, 0x0305);

            Assert::AreEqual ((Word) 0x0300, Window (rig.view.Build (rig.controller.GetSession()), 2).first);
        }


        //  An I/O byte is its device's side-effect-free peek: the keyboard's
        //  key is in the window, and its strobe is not cleared.
        TEST_METHOD (IoBytesAreReadWithoutTheirEffects)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            IDebugTarget        & target = rig.controller.GetSession().GetTarget();



            target.InjectKey (0xC1);
            rig.view.OpenMemoryWindow (2, 0xC000);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue  (Window (snapshot, 2).bytes[0] == std::optional<Byte> (0xC1), L"the key, with its strobe bit");
            Assert::IsTrue  (Window (snapshot, 2).regions[0] == MemoryRegion::Io);
            Assert::IsTrue  (target.IsKeyPending(), L"the strobe is not cleared");
        }


        TEST_METHOD (AnEditShowsInEveryWindowAtThatAddress)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.SetMemoryAddress (0x0300);
            rig.view.OpenMemoryWindow (2, 0x0300);
            rig.Run ("PATCH 0300 5A");
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((Byte) 0x5A, *Window (snapshot, 1).bytes[0]);
            Assert::AreEqual ((Byte) 0x5A, *Window (snapshot, 2).bytes[0]);
        }
    };
}
