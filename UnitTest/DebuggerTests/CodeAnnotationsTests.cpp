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
    //  CodeAnnotationTests
    //
    //  What a code line is annotated with, and what running it changes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CodeAnnotationTests)
    {
    public:
        TEST_METHOD (AnInstructionIsAnnotatedWithWhatItActsOn)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            Cpu6502Registers      r        = rig.controller.GetSession().GetTarget().GetRegisters();



            rig.machine.GetMemoryBus().WriteByte (0x0400, 0x5A);
            rig.machine.GetMemoryBus().WriteByte (0x0306, 0xF0);    // BEQ $0306
            rig.machine.GetMemoryBus().WriteByte (0x0307, 0xFE);
            rig.machine.GetMemoryBus().WriteByte (0x0308, 0x9D);    // STA $0400,X
            rig.machine.GetMemoryBus().WriteByte (0x0309, 0x00);
            rig.machine.GetMemoryBus().WriteByte (0x030A, 0x04);
            rig.machine.GetMemoryBus().WriteByte (0x0402, 0x77);
            rig.machine.GetMemoryBus().WriteByte (0x0305, 0xEA);    // NOP, in place of the RTS

            r.x = 0x02;
            r.p = (Byte) (r.p | 0x02);
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (std::string (""),          LineAt (snapshot, 0x0300).annotation, L"an immediate operand touches no memory");
            Assert::AreEqual (std::string ("A=00 $0400=5A"), LineAt (snapshot, 0x0302).annotation, L"a store reads A, whatever A holds");
            Assert::AreEqual (std::string ("Z=1"),       LineAt (snapshot, 0x0306).annotation, L"a branch shows the flag it tests");
            Assert::AreEqual (std::string ("A=00 X=02 $0402=77"), LineAt (snapshot, 0x0308).annotation, L"a store reads A, and the index that formed the address");
            Assert::IsTrue   (LineAt (snapshot, 0x0306).target == std::optional<Word> (0x0306));
        }


        TEST_METHOD (ASoftSwitchIsAnnotatedWithWhatItDoesRatherThanAValue)
        {
            MachineRig            rig;
            IDebugTarget        & target = rig.controller.GetSession().GetTarget();
            DebuggerViewSnapshot  snapshot;
            Cpu6502Registers      r      = rig.controller.GetSession().GetTarget().GetRegisters();
            auto                  lineAt = [] (const DebuggerViewSnapshot & shot, Word address) -> DebuggerViewSnapshot::CodeLine
            {
                for (const DebuggerViewSnapshot::CodeLine & line : shot.code)
                {
                    if (line.address == address)
                    {
                        return line;
                    }
                }

                return {};
            };



            //  LDA KBD, then STA SPKR: a read of a switch and a write to one.
            (void) target.TryPoke (0x0300, 0xAD);
            (void) target.TryPoke (0x0301, 0x00);
            (void) target.TryPoke (0x0302, 0xC0);
            (void) target.TryPoke (0x0303, 0x8D);
            (void) target.TryPoke (0x0304, 0x30);
            (void) target.TryPoke (0x0305, 0xC0);
            (void) target.TryPoke (0x0306, 0xE6);
            (void) target.TryPoke (0x0307, 0x4E);
            (void) target.TryPoke (0x004E, 0x96);

            r.pc = 0x0303;
            target.SetRegisters (r);

            rig.view.GetCodeViews().SetCodeLines (20);
            rig.view.GetCodeViews().CenterCodeOn (0x0300);
            snapshot = rig.view.Build (rig.controller.GetSession());

            //  What the switch does, never a byte: the byte does not exist
            //  until a read happens, and the read operates the machine.
            Assert::AreEqual (std::string ("keyboard data; bit 7 set when a key is waiting"),
                              lineAt (snapshot, 0x0300).annotation, L"a read of KBD says what KBD is");
            Assert::AreEqual (std::string ("A=00 Toggle the speaker (each access is a click)"),
                              lineAt (snapshot, 0x0303).annotation, L"a write to SPKR says what it does, and STA reads A");
            Assert::AreEqual (std::string ("$004E=96; random seed, low byte (counts while waiting for a key)"),
                              lineAt (snapshot, 0x0306).annotation, L"a named location in memory says what it is after its byte");

            //  And the result of running it is the action, not a store, then
            //  the cycles STA absolute takes.
            Assert::AreEqual (std::string ("Toggle the speaker (each access is a click); 4 cycles"),
                              lineAt (snapshot, 0x0303).effect, L"the PC's line: a write to a switch stores nothing");
        }


        //  LDA PADDL0,X with X=1 reads paddle 1, so the annotation is paddle
        //  1's description, not the description of the switch at the symbol.
        TEST_METHOD (AnIndexedSoftSwitchIsDescribedByTheSwitchItReaches)
        {
            MachineRig            rig;
            IDebugTarget        & target = rig.controller.GetSession().GetTarget();
            DebuggerViewSnapshot  snapshot;
            Cpu6502Registers      r      = target.GetRegisters();
            std::string           annotation;



            (void) target.TryPoke (0x0300, 0xBD);
            (void) target.TryPoke (0x0301, 0x64);
            (void) target.TryPoke (0x0302, 0xC0);

            r.pc = 0x0300;
            r.x  = 0x01;
            target.SetRegisters (r);

            rig.view.GetCodeViews().SetCodeLines (20);
            rig.view.GetCodeViews().CenterCodeOn (0x0300);
            snapshot = rig.view.Build (rig.controller.GetSession());

            for (const DebuggerViewSnapshot::CodeLine & line : snapshot.code)
            {
                if (line.address == 0x0300)
                {
                    annotation = line.annotation;
                }
            }

            Assert::AreEqual (std::string ("X=01 paddle 1 timer in bit 7"), annotation);
        }


        //  A switch whose read operates nothing is peeked for the PC line's
        //  result, and the peek changes nothing about the switch: the paddle
        //  timer of an axis at 0 has run out, and a key's strobe stays set.
        TEST_METHOD (APeekableSwitchReadHasItsValueInTheResult)
        {
            MachineRig            rig;
            IDebugTarget        & target = rig.controller.GetSession().GetTarget();
            Cpu6502Registers      r      = target.GetRegisters();
            std::string           effect;



            Assert::IsNotNull (rig.machine.GetRefs().iieSoftSwitches);
            Assert::IsNotNull (rig.machine.GetRefs().iieKeyboard);

            rig.machine.GetRefs().iieSoftSwitches->SetPaddle (1, 0);
            rig.machine.GetRefs().iieKeyboard->PressKey ('A');

            //  LDA PADDL0,X with X=1, then LDA KBD.
            (void) target.TryPoke (0x0300, 0xBD);
            (void) target.TryPoke (0x0301, 0x64);
            (void) target.TryPoke (0x0302, 0xC0);
            (void) target.TryPoke (0x0303, 0xAD);
            (void) target.TryPoke (0x0304, 0x00);
            (void) target.TryPoke (0x0305, 0xC0);

            r.pc = 0x0300;
            r.x  = 0x01;
            r.a  = 0x41;
            r.p  = 0x20;
            target.SetRegisters (r);

            rig.view.GetCodeViews().SetCodeLines (20);
            rig.view.GetCodeViews().CenterCodeOn (0x0300);
            effect = LineAt (rig.view.Build (rig.controller.GetSession()), 0x0300).effect;

            Assert::AreEqual (std::string ("A=00 Z=1; 4 cycles"), effect, L"paddle 1's timer has run out");

            r.pc = 0x0303;
            target.SetRegisters (r);
            effect = LineAt (rig.view.Build (rig.controller.GetSession()), 0x0303).effect;

            Assert::AreEqual (std::string ("A=C1 N=1; 4 cycles"), effect, L"the key with its strobe");
            Assert::IsFalse  (rig.machine.GetRefs().iieKeyboard->IsStrobeClear(), L"the peek leaves the strobe set");
        }


        //  PTRIG arms the paddle timers when read; its peek returns the byte
        //  the read returns, 0, and arms nothing.
        TEST_METHOD (APtrigReadHasItsValueAndArmsNothing)
        {
            MachineRig            rig;
            IDebugTarget        & target  = rig.controller.GetSession().GetTarget();
            Cpu6502Registers      r       = target.GetRegisters();
            uint64_t              trigger = 0;
            std::string           effect;



            Assert::IsNotNull (rig.machine.GetRefs().iieSoftSwitches);

            trigger = rig.machine.GetRefs().iieSoftSwitches->GetPaddleTriggerCycle();

            //  LDA PTRIG.
            (void) target.TryPoke (0x0300, 0xAD);
            (void) target.TryPoke (0x0301, 0x70);
            (void) target.TryPoke (0x0302, 0xC0);

            r.pc = 0x0300;
            r.a  = 0x41;
            r.p  = 0x20;
            target.SetRegisters (r);

            rig.view.GetCodeViews().SetCodeLines (20);
            rig.view.GetCodeViews().CenterCodeOn (0x0300);
            effect = LineAt (rig.view.Build (rig.controller.GetSession()), 0x0300).effect;

            Assert::AreEqual (std::string ("A=00 Z=1; 4 cycles"), effect);
            Assert::AreEqual (trigger, rig.machine.GetRefs().iieSoftSwitches->GetPaddleTriggerCycle(), L"the trial run arms no timer");
        }


        //  A byte nothing can peek feeds unknowns: each run of the instruction
        //  with another fill value changes A, N and Z, so the result writes
        //  them as "?", with the cycles and the next PC as they are.
        TEST_METHOD (AnUnpeekableReadFeedsUnknowns)
        {
            MachineRig                   rig;
            MockExpressionContext        memory;
            Cpu6502Registers             registers = {};
            InstructionTouches::Result   touches;
            const Microcode            * set       = rig.controller.GetSession().GetTarget().GetInstructionSet();



            memory.memory[0x0300] = 0xAD;     // LDA $C000, which the mock cannot read
            memory.memory[0x0301] = 0x00;
            memory.memory[0x0302] = 0xC0;

            registers.pc = 0x0300;
            registers.a  = 0x41;
            registers.p  = 0x20;

            touches = InstructionTouches::Find (memory, set, registers, 0x0300, 3);

            Assert::IsTrue   (touches.isKnown);
            Assert::IsTrue   (touches.unknowns.a);
            Assert::AreEqual (std::string ("A=? Z=? N=?"), InstructionEffect::Format (registers, touches.after, touches.writes, 0x0303, nullptr, touches.unknowns));
            Assert::AreEqual (4u, touches.cycles);
        }

        //  On a 65C02 the instruction runs as the 65C02 runs it: STZ stores a
        //  zero where the 6502 reads past a NOP, and BRK clears D.
        TEST_METHOD (A65C02InstructionRunsAsThe65C02RunsIt)
        {
            TestMachine                  machine   ("Apple2eEnhanced", TestMachine::Slots::Empty);
            MockExpressionContext        memory;
            Cpu6502Registers             registers = {};
            InstructionTouches::Result   touches;
            const Microcode            * set       = machine.GetCpu()->GetCpu6502()->GetInstructionSet();



            memory.memory[0x0300] = 0x64;     // STZ $10
            memory.memory[0x0301] = 0x10;
            memory.memory[0x0010] = 0x55;

            registers.pc = 0x0300;
            registers.sp = 0xFF;

            touches = InstructionTouches::Find (memory, set, registers, 0x0300, 2);

            Assert::AreEqual ((size_t) 1, touches.writes.size(), L"one write");
            Assert::AreEqual ((Word) 0x0010, touches.writes[0].address);
            Assert::AreEqual ((Byte) 0x00,   touches.writes[0].value);

            memory.memory[0x0300] = 0x00;     // BRK
            registers.p           = 0x08;

            touches = InstructionTouches::Find (memory, set, registers, 0x0300, 1);
            Assert::AreEqual (0, touches.after.p & 0x08, L"BRK clears D");
        }

        //  The PC's line ends with the cycles its instruction will take with
        //  the registers as they are: a branch's count is for the way it goes.
        TEST_METHOD (ThePcLineEndsWithItsCycles)
        {
            constexpr Byte        kZero  = 0x02;
            MachineRig            rig;
            IDebugTarget        & target = rig.controller.GetSession().GetTarget();
            Cpu6502Registers      r      = target.GetRegisters();
            DebuggerViewSnapshot  snapshot;
            std::string           effect;
            auto                  pcLine = [] (const DebuggerViewSnapshot & shot) -> std::string
            {
                for (const DebuggerViewSnapshot::CodeLine & line : shot.code)
                {
                    if (line.address == shot.pc)
                    {
                        return line.effect;
                    }
                }

                return {};
            };



            //  BEQ  at , a branch within the page.
            (void) target.TryPoke (0x0300, 0xF0);
            (void) target.TryPoke (0x0301, 0x0E);

            r.pc = 0x0300;
            r.p  = (Byte) (r.p | kZero);
            target.SetRegisters (r);
            rig.view.GetCodeViews().SetCodeLines (20);
            rig.view.GetCodeViews().CenterCodeOn (0x0300);
            snapshot = rig.view.Build (rig.controller.GetSession());
            effect   = pcLine (snapshot);
            Assert::IsTrue (effect.ends_with ("3 cycles"), TextEncoding::NarrowToWide (effect).c_str());

            r.p = (Byte) (r.p & ~kZero);
            target.SetRegisters (r);
            snapshot = rig.view.Build (rig.controller.GetSession());
            effect   = pcLine (snapshot);
            Assert::IsTrue (effect.ends_with ("2 cycles"), TextEncoding::NarrowToWide (effect).c_str());
        }


        TEST_METHOD (ASoftSwitchOperandTakesTheSymbolForItsDirection)
        {
            //  Each address here is two switches; which one the line shows
            //  turns on whether the instruction reads it or writes it.
            const std::tuple<const wchar_t *, std::vector<Byte>, const char *>  cases[] =
            {
                { L"LDA $C000 reads the keyboard",   { 0xAD, 0x00, 0xC0 }, "KBD"        },
                { L"STA $C000 turns 80STORE off",    { 0x8D, 0x00, 0xC0 }, "80STOREOFF" },
                { L"INC $C000 both reads and writes",    { 0xEE, 0x00, 0xC0 }, "80STOREOFF" },
                { L"LDA $FDED has one symbol either way", { 0xAD, 0xED, 0xFD }, "COUT"       },
                { L"LDA $C07E reads the IOUDIS bit",     { 0xAD, 0x7E, 0xC0 }, "RDIOUDIS"   },
                { L"STA $C07E turns IOU access off",     { 0x8D, 0x7E, 0xC0 }, "SETIOUDIS"  },
                { L"LDA $C0E9 starts the drive motor",   { 0xAD, 0xE9, 0xC0 }, "MOTORON"    },

                //  $C05E is ONE switch under two names -- annunciator 3 and,
                //  on a //e, double hi-res -- not a read and a write. Neither
                //  description claims a direction, so the first stands
                //  whichever way the instruction touches it.
                { L"LDA $C05E",                          { 0xAD, 0x5E, 0xC0 }, "CLRAN3"     },
                { L"STA $C05E",                          { 0x8D, 0x5E, 0xC0 }, "CLRAN3"     },
            };



            for (const auto & [what, bytes, expected] : cases)
            {
                MachineRig               rig;
                IDebugTarget           & target = rig.controller.GetSession().GetTarget();
                Reply                    code;
                const DisassemblyData  * data   = nullptr;



                for (size_t i = 0; i < bytes.size(); i++)
                {
                    (void) target.TryPoke ((Word) (0x0300 + i), bytes[i]);
                }

                code = rig.controller.GetSession().ExecuteLine ("U 0300", CommandMode::AppleWin);
                data = std::get_if<DisassemblyData> (&code.data);

                Assert::IsNotNull (data, what);
                Assert::IsTrue    (data != nullptr && !data->lines.empty(), what);
                Assert::AreEqual  (std::string (expected), data->lines[0].operandSymbol, what);
            }
        }


        TEST_METHOD (AnnotationsAreBuiltOnlyWhileTheMachineIsPaused)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  paused;
            DebuggerViewSnapshot  running;
            auto                  anyAnnotation = [] (const DebuggerViewSnapshot & snapshot)
            {
                return std::any_of (snapshot.code.begin(), snapshot.code.end(),
                                    [] (const DebuggerViewSnapshot::CodeLine & line) { return !line.annotation.empty() || !line.effect.empty(); });
            };



            rig.view.GetCodeViews().SetCodeLines (20);

            paused  = rig.view.Build (rig.controller.GetSession(), true);
            running = rig.view.Build (rig.controller.GetSession(), false);

            Assert::IsTrue  (paused.isPaused);
            Assert::IsFalse (running.isPaused);
            Assert::IsTrue  (anyAnnotation (paused),   L"a paused machine annotates what the instructions act on");
            Assert::IsFalse (anyAnnotation (running),  L"a running machine annotates nothing: the values would be from mid-flight");
            Assert::AreEqual (paused.code.size(), running.code.size(), L"the lines themselves are built either way");
        }


        TEST_METHOD (TheInstructionTouchesAreWhatTheInstructionTouches)
        {
            using Kind = InstructionTouches::Kind;

            MachineRig                               rig;
            IDebugTarget                           & target = rig.controller.GetSession().GetTarget();
            Cpu6502Registers                         r      = rig.controller.GetSession().GetTarget().GetRegisters();
            std::vector<InstructionTouches::Item>    items;
            auto                                 has    = [&items] (Kind kind, const char * name, Word address, bool isRead, bool isWrite)
            {
                return std::any_of (items.begin(), items.end(), [&] (const InstructionTouches::Item & item)
                {
                    return item.kind == kind && item.name == name && item.address == address &&
                           item.isRead == isRead && item.isWrite == isWrite;
                });
            };



            r.pc = 0x0300;
            r.a  = 0x10;
            r.x  = 0x02;
            r.y  = 0x00;
            r.p  = 0x01;                              // carry set
            r.sp = 0xF8;
            target.SetRegisters (r);

            //  ADC $0400,X: reads A, X, carry and the byte at $0402; writes A
            //  and the arithmetic flags. Nothing says so in a table -- each
            //  one is found by running the instruction again with that input
            //  changed.
            (void) target.TryPoke (0x0300, 0x7D);
            (void) target.TryPoke (0x0301, 0x00);
            (void) target.TryPoke (0x0302, 0x04);
            (void) target.TryPoke (0x0402, 0x7F);

            items = InstructionTouches::Find (rig.controller.GetSession(), target.GetInstructionSet(), r, 0x0300, 3).items;

            Assert::IsTrue  (has (Kind::Address,  "",  0x0402, true, false), L"the byte it adds");
            Assert::IsTrue  (has (Kind::Register, "A", 0,      true, true),  L"A is read and written");
            Assert::IsTrue  (has (Kind::Register, "X", 0,      true, false), L"X indexes the address");
            Assert::IsTrue  (has (Kind::Flag,     "C", 0,      true, true),  L"carry goes in and comes out");
            Assert::IsTrue  (has (Kind::Flag,     "V", 0,      false, true), L"overflow is written, never read");
            Assert::IsFalse (has (Kind::Register, "Y", 0,      true, false), L"Y has nothing to do with it");
            Assert::IsFalse (has (Kind::Flag,     "I", 0,      true, false), L"nor does the interrupt flag");

            //  A branch reads the flag it tests and writes nothing at all.
            items.clear();
            (void) target.TryPoke (0x0300, 0xF0);      // BEQ +$10
            (void) target.TryPoke (0x0301, 0x10);

            items = InstructionTouches::Find (rig.controller.GetSession(), target.GetInstructionSet(), r, 0x0300, 2).items;

            Assert::IsTrue  (has (Kind::Flag, "Z", 0, true, false), L"BEQ reads Z");
            Assert::IsFalse (has (Kind::Flag, "C", 0, true, false), L"and no other flag");
            Assert::IsTrue  (std::none_of (items.begin(), items.end(),
                                           [] (const InstructionTouches::Item & item) { return item.isWrite; }),
                             L"a branch writes nothing");

            //  A store writes memory and reads the register it stores.
            items.clear();
            (void) target.TryPoke (0x0300, 0x8D);      // STA $0400
            (void) target.TryPoke (0x0301, 0x00);
            (void) target.TryPoke (0x0302, 0x04);

            items = InstructionTouches::Find (rig.controller.GetSession(), target.GetInstructionSet(), r, 0x0300, 3).items;

            Assert::IsTrue (has (Kind::Address,  "",  0x0400, false, true), L"the address it stores to");
            Assert::IsTrue (has (Kind::Register, "A", 0,      true,  false), L"A is read, not changed");
        }


        TEST_METHOD (ThePredictedEffectMatchesWhatTheCoreActuallyDoes)
        {
            //  Every case: the bytes at $0300, and the registers going in.
            const std::tuple<const wchar_t *, std::vector<Byte>, Byte, Byte, Byte, Byte>  cases[] =
            {
                //  what                       bytes                 A     X     Y     P
                { L"LDA #$00",                 { 0xA9, 0x00 },       0x55, 0x00, 0x00, 0x00 },
                { L"CMP #$8D",                 { 0xC9, 0x8D },       0xA0, 0x00, 0x00, 0x00 },
                { L"ADC #$8D binary",          { 0x69, 0x8D },       0xA0, 0x00, 0x00, 0x00 },
                { L"ADC #$8D with carry in",   { 0x69, 0x8D },       0xA0, 0x00, 0x00, 0x01 },
                { L"ADC #$19 DECIMAL",         { 0x69, 0x19 },       0x28, 0x00, 0x00, 0x08 },
                { L"SBC #$19 DECIMAL",         { 0xE9, 0x19 },       0x28, 0x00, 0x00, 0x09 },
                { L"STA $0402,X",              { 0x9D, 0x00, 0x04 }, 0x3B, 0x02, 0x00, 0x00 },
                { L"LDA ($10),Y",              { 0xB1, 0x10 },       0x00, 0x00, 0x04, 0x00 },
                { L"INC $0400",                { 0xEE, 0x00, 0x04 }, 0x00, 0x00, 0x00, 0x00 },
                { L"ASL A",                    { 0x0A },             0xC1, 0x00, 0x00, 0x00 },
                { L"ROR A with carry in",      { 0x6A },             0x01, 0x00, 0x00, 0x01 },
                { L"BEQ taken",                { 0xF0, 0x10 },       0x00, 0x00, 0x00, 0x02 },
                { L"BEQ not taken",            { 0xF0, 0x10 },       0x00, 0x00, 0x00, 0x00 },
                { L"JSR $0400",                { 0x20, 0x00, 0x04 }, 0x00, 0x00, 0x00, 0x00 },
                { L"PHA",                      { 0x48 },             0x7E, 0x00, 0x00, 0x00 },
                { L"SEC",                      { 0x38 },             0x00, 0x00, 0x00, 0x00 },
                { L"NOP",                      { 0xEA },             0x00, 0x00, 0x00, 0x00 },
                { L"undocumented LAX $0400",   { 0xAF, 0x00, 0x04 }, 0x00, 0x00, 0x00, 0x00 },
            };



            for (const auto & [what, bytes, a, x, y, p] : cases)
            {
                MachineRig                    rig;
                IDebugTarget                & target    = rig.controller.GetSession().GetTarget();
                Cpu6502Registers              before    = target.GetRegisters();
                InstructionTouches::Result    touches;
                std::string                   predicted;
                std::string                   actual;



                for (size_t i = 0; i < bytes.size(); i++)
                {
                    (void) target.TryPoke ((Word) (0x0300 + i), bytes[i]);
                }

                //  A pointer at $10 for the indirect case, and something to
                //  read and increment at $0400.
                (void) target.TryPoke (0x0010, 0x00);
                (void) target.TryPoke (0x0011, 0x04);
                (void) target.TryPoke (0x0400, 0x7F);
                (void) target.TryPoke (0x0402, 0x00);
                (void) target.TryPoke (0x0404, 0x99);

                before.pc = 0x0300;
                before.sp = 0xF8;
                before.a  = a;
                before.x  = x;
                before.y  = y;
                before.p  = p;
                target.SetRegisters (before);

                touches   = InstructionTouches::Find (rig.controller.GetSession(), target.GetInstructionSet(),
                                                      before, 0x0300, (Word) bytes.size());
                predicted = InstructionEffect::Format (before, touches.after, touches.writes,
                                                       (Word) (0x0300 + bytes.size()));

                //  THE MACHINE ITSELF, one instruction, and what it left.
                actual = StepAndDescribe (rig, before);

                Assert::AreEqual (actual, predicted, what);
            }
        }


        //  Steps the real machine one instruction and writes what changed in
        //  the same words the prediction uses. Restating the format here is
        //  the point: the two are arrived at independently, so a prediction
        //  that drifts from the core fails this.
        static std::string StepAndDescribe (MachineRig & rig, const Cpu6502Registers & before)
        {
            static constexpr char    kFlagLetters[] = { 'C', 'Z', 'I', 'D', 'B', '-', 'V', 'N' };
            IDebugTarget           & target         = rig.controller.GetSession().GetTarget();
            std::array<Byte, 16>     pageBefore     = {};
            std::array<Byte, 16>     stackBefore    = {};
            Cpu6502Registers         after          = {};
            std::string              text;
            Byte                     value          = 0;



            for (Word i = 0; i < 16; i++)
            {
                (void) target.TryPeek ((Word) (0x0400 + i), pageBefore[i]);
                (void) target.TryPeek ((Word) (0x01F0 + i), stackBefore[i]);
            }

            rig.machine.StepOne();
            after = target.GetRegisters();

            if (after.a  != before.a)  { text += std::format (" A={:02X}", after.a);  }
            if (after.x  != before.x)  { text += std::format (" X={:02X}", after.x);  }
            if (after.y  != before.y)  { text += std::format (" Y={:02X}", after.y);  }
            if (after.sp != before.sp) { text += std::format (" S={:02X}", after.sp); }

            //  A push reads as what was pushed, the higher address first --
            //  the order the CPU wrote them. JSR's two bytes are the return
            //  address less one.
            if (before.sp - after.sp == 1)
            {
                (void) target.TryPeek ((Word) (0x0100 + before.sp), value);
                text += std::format (" pushed ${:02X}", value);
            }
            else if (before.sp - after.sp == 2)
            {
                Byte  high = 0;
                Byte  low  = 0;



                (void) target.TryPeek ((Word) (0x0100 + before.sp),     high);
                (void) target.TryPeek ((Word) (0x0100 + before.sp - 1), low);
                text += std::format (" pushed ${:02X}{:02X} (returns to ${:04X})", high, low, (Word) (((high << 8) | low) + 1));
            }

            for (Word i = 16; i > 0; i--)
            {
                Word  address = (Word) (0x01F0 + i - 1);

                if (address > 0x0100 + after.sp && address <= 0x0100 + before.sp)
                {
                    continue;
                }

                if (target.TryPeek (address, value) && value != stackBefore[i - 1])
                {
                    text += std::format (" ${:04X}={:02X}", address, value);
                }
            }

            for (Word i = 0; i < 16; i++)
            {
                Word  address = (Word) (0x0400 + i);

                if (target.TryPeek (address, value) && value != pageBefore[i])
                {
                    text += std::format (" ${:04X}={:02X}", address, value);
                }
            }

            for (int bit = 0; bit < 8; bit++)
            {
                Byte  mask = (Byte) (1u << bit);

                if (kFlagLetters[bit] != '-' && ((before.p ^ after.p) & mask) != 0)
                {
                    text += std::format (" {}={}", kFlagLetters[bit], (after.p & mask) ? 1 : 0);
                }
            }

            if (after.pc != (Word) (before.pc + GetInstructionLength (rig)))
            {
                text += std::format (" PC=${:04X}", after.pc);
            }

            return text.empty() ? text : text.substr (1);
        }


        //  The length of the instruction at $0300, from the disassembler.
        static size_t GetInstructionLength (MachineRig & rig)
        {
            Reply  code = rig.controller.GetSession().ExecuteLine ("U 0300", CommandMode::AppleWin);

            if (const DisassemblyData * data = std::get_if<DisassemblyData> (&code.data))
            {
                if (!data->lines.empty())
                {
                    return data->lines[0].instruction.bytes.size();
                }
            }

            return 1;
        }


        //  A push is the value pushed, not the stack bytes it took: JSR's
        //  return address, and the byte from PHA, PHP, BRK or an interrupt.
        TEST_METHOD (APushShowsAsTheValuePushed)
        {
            Cpu6502Registers  before = {};
            Cpu6502Registers  after  = {};



            before.pc = 0x0300;
            before.sp = 0xCE;
            after     = before;
            after.sp  = 0xCC;
            after.pc  = 0x0400;

            Assert::AreEqual (std::string ("S=CC pushed $DD97 (returns to $DD98) PC=$0400"),
                              InstructionEffect::Format (before, after, { { 0x01CE, 0xDD }, { 0x01CD, 0x97 } }, 0x0303));

            after.sp = 0xCD;
            after.pc = 0x0301;
            Assert::AreEqual (std::string ("S=CD pushed $41"),
                              InstructionEffect::Format (before, after, { { 0x01CE, 0x41 } }, 0x0301));

            after.sp = 0xCB;
            after.pc = 0xFA62;
            Assert::AreEqual (std::string ("S=CB pushed $0302 and $30 PC=$FA62"),
                              InstructionEffect::Format (before, after, { { 0x01CE, 0x03 }, { 0x01CD, 0x02 }, { 0x01CC, 0x30 } }, 0x0302));
        }

        //  A data line is marked as data, and has no operand or result, since
        //  it isn't an instruction.
        TEST_METHOD (ADataLineHasNoAnnotation)
        {
            std::unique_ptr<MachineRig>   held     = std::make_unique<MachineRig>();
            MachineRig                  & rig      = *held;
            DebuggerViewSnapshot          snapshot;
            Cpu6502Registers              r        = rig.controller.GetSession().GetTarget().GetRegisters();



            r.pc = 0x0300;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            (void) rig.Run ("DB 0302:0304");
            rig.view.GetCodeViews().SetCodeLines (20);
            snapshot = rig.view.Build (rig.controller.GetSession());

            for (const DebuggerViewSnapshot::CodeLine & line : snapshot.code)
            {
                if (line.address == 0x0302)
                {
                    Assert::AreEqual ((int) DataOrigin::Marked, (int) line.dataOrigin);
                    Assert::IsTrue   (line.annotation.empty() && line.effect.empty());
                    return;
                }
            }

            Assert::Fail (L"no line at $0302");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AutoWatchTests
    //
    //  The automatic watches of a stopped machine.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AutoWatchTests)
    {
    public:
        TEST_METHOD (TheAutomaticWatchesAreTheCurrentAndTheJustExecutedInstructions)
        {
            MachineRig            rig;
            IDebugTarget        & target = rig.controller.GetSession().GetTarget();
            Cpu6502Registers      r      = target.GetRegisters();
            DebuggerViewSnapshot  snapshot;
            auto                  find   = [&snapshot] (const char * key) -> const DebuggerViewSnapshot::AutoWatchLine *
            {
                for (const DebuggerViewSnapshot::AutoWatchLine & line : snapshot.autoWatches)
                {
                    if (line.key == key)
                    {
                        return &line;
                    }
                }

                return nullptr;
            };



            //  LDX #$05, then STA $0400,X.
            (void) target.TryPoke (0x0300, 0xA2);
            (void) target.TryPoke (0x0301, 0x05);
            (void) target.TryPoke (0x0302, 0x9D);
            (void) target.TryPoke (0x0303, 0x00);
            (void) target.TryPoke (0x0304, 0x04);

            r.pc = 0x0300;
            r.a  = 0x42;
            target.SetRegisters (r);

            snapshot = rig.view.Build (rig.controller.GetSession(), true);

            Assert::IsNotNull (find ("R:X"), L"LDX writes X");
            Assert::IsNotNull (find ("F:Z"), L"and Z");
            Assert::IsNull    (find ("R:A"), L"and has nothing to do with A");

            //  One step: the previous instruction is known, and what it
            //  touched shows below what the current one touches.
            rig.machine.StepOne();
            snapshot = rig.view.Build (rig.controller.GetSession(), true);

            Assert::IsNotNull (find ("R:A"),    L"STA reads A");
            Assert::IsNotNull (find ("M:0405"), L"and writes $0400 + X");
            Assert::IsNotNull (find ("R:X"),    L"X indexes it");
            Assert::IsFalse   (find ("R:X")->isPrevious, L"X is listed once, for the current instruction");
            Assert::IsTrue    (find ("F:Z") != nullptr && find ("F:Z")->isPrevious, L"Z only the previous one wrote");

            //  The same stop rebuilt, as every click does, keeps the previous.
            snapshot = rig.view.Build (rig.controller.GetSession(), true);
            Assert::IsTrue (find ("F:Z") != nullptr, L"a rebuild at the same stop keeps the previous instruction");

            //  After anything that is not a step, the previous instruction is
            //  unknown and is left out rather than shown for the wrong one.
            r    = target.GetRegisters();
            r.pc = 0x0300;
            target.SetRegisters (r);
            snapshot = rig.view.Build (rig.controller.GetSession(), true);

            Assert::IsTrue (std::none_of (snapshot.autoWatches.begin(), snapshot.autoWatches.end(),
                                          [] (const DebuggerViewSnapshot::AutoWatchLine & line) { return line.isPrevious; }),
                            L"not a step: no previous instruction");
        }


        TEST_METHOD (TheStatusRegisterWatchShowsPRatherThanS)
        {
            MachineRig            rig;
            IDebugTarget        & target = rig.controller.GetSession().GetTarget();
            Cpu6502Registers      r      = target.GetRegisters();
            DebuggerViewSnapshot  snapshot;
            std::string           shown;



            //  PHP reads the whole status register.
            (void) target.TryPoke (0x0300, 0x08);

            r.pc = 0x0300;
            r.p  = 0xE5;
            r.sp = 0xF0;
            target.SetRegisters (r);

            snapshot = rig.view.Build (rig.controller.GetSession(), true);

            for (const DebuggerViewSnapshot::AutoWatchLine & line : snapshot.autoWatches)
            {
                shown = (line.key == "R:P") ? line.value : shown;
            }

            Assert::AreEqual (std::string ("E5"), shown, L"the P row holds P");
            Assert::IsTrue   (LineAt (snapshot, 0x0300).annotation.find ("P=E5") != std::string::npos, L"the annotation shows P");
        }
    };
}
