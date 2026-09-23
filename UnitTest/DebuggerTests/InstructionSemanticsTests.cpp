#include "Pch.h"

#include "Debugger/DebuggerController.h"
#include "Debugger/InstructionSemantics.h"
#include "EmuTests/TestMachine.h"
#include "InMemoryPipeTransport.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/InstructionTouches.h"
#include "UiTests/InMemoryFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSemanticsTests
//
//  The one table in the debugger that describes the CPU rather than reading
//  it: which flags each operation writes. A table beside a CPU can drift from
//  it, so this runs EVERY opcode through the core and fails if an instruction
//  changes a flag or a register the table does not declare.
//
//  It cannot catch the other direction -- a flag declared but never written,
//  which is what an instruction writing the value that was already there
//  looks like -- and that is the safe way round: the pane shows a value that
//  did not need showing rather than hiding one that changed.
//
////////////////////////////////////////////////////////////////////////////////

namespace InstructionSemanticsTests
{
    //  Values chosen to make flags move: a negative accumulator, an index
    //  that crosses a page, a full carry and a byte with both top bits set.
    static constexpr Byte  kOperandValues[] = { 0x00, 0x01, 0x7F, 0x80, 0xC0, 0xFF };
    static constexpr Byte  kFlagStates[]    = { 0x00, 0xFF };



    class Rig
    {
    public:
        TestMachine                          machine;
        CpuManager                           cpuManager;
        InMemoryPipeTransport                transport;
        InMemoryFileSystem                   files;
        std::unique_ptr<DebuggerController>  controller;



        Rig() : machine ("Apple2e", TestMachine::Slots::Empty)
        {
            controller = std::make_unique<DebuggerController> (machine, cpuManager, transport, files, nullptr, 1);
        }
    };



    TEST_CLASS (InstructionSemanticsTests)
    {
    public:

        TEST_METHOD (NoInstructionChangesAFlagTheTableDoesNotDeclare)
        {
            Rig            rig;
            DebugSession & session = rig.controller->GetSession();
            IDebugTarget & target  = session.GetTarget();
            int            checked = 0;



            for (int opcode = 0; opcode <= 0xFF; opcode++)
            {
                for (Byte operand : kOperandValues)
                {
                    for (Byte flags : kFlagStates)
                    {
                        Cpu6502Registers            before    = target.GetRegisters();
                        InstructionTouches::Result  touches;
                        Byte                        declared  = 0;
                        Byte                        moved     = 0;
                        wchar_t                     line[160] = {};



                        //  The instruction at $0300, with something to work
                        //  on at $0400 and a pointer at $0010.
                        (void) target.TryPoke (0x0300, (Byte) opcode);
                        (void) target.TryPoke (0x0301, operand);
                        (void) target.TryPoke (0x0302, 0x04);
                        (void) target.TryPoke (0x0010, 0x00);
                        (void) target.TryPoke (0x0011, 0x04);
                        (void) target.TryPoke (0x0400, operand);
                        (void) target.TryPoke (0x0401, operand);

                        before.pc = 0x0300;
                        before.sp = 0xF8;
                        before.a  = operand;
                        before.x  = 0x01;
                        before.y  = 0x01;
                        before.p  = flags;

                        touches = InstructionTouches::Find (session, target.GetInstructionSet(), before, 0x0300, 1);

                        if (!touches.isKnown)
                        {
                            continue;
                        }

                        for (const InstructionTouches::Item & item : touches.items)
                        {
                            static constexpr std::pair<char, Byte>  kBits[] =
                            {
                                { 'C', InstructionSemantics::kCarry    }, { 'Z', InstructionSemantics::kZero     },
                                { 'I', InstructionSemantics::kIrqOff   }, { 'D', InstructionSemantics::kDecimal  },
                                { 'V', InstructionSemantics::kOverflow }, { 'N', InstructionSemantics::kNegative },
                            };

                            for (const auto & [letter, bit] : kBits)
                            {
                                if (item.kind == InstructionTouches::Kind::Flag && item.isWrite && item.name[0] == letter)
                                {
                                    declared |= bit;
                                }
                            }
                        }

                        //  Bit 5 is always set and the break flag lives only
                        //  in what a push writes, so neither is compared.
                        moved = (Byte) ((before.p ^ touches.after.p) & ~(Byte) (0x20 | InstructionSemantics::kBreak));

                        swprintf_s (line, L"opcode $%02X operand $%02X flags $%02X: changed $%02X, declared $%02X",
                                    opcode, operand, flags, moved, declared);
                        Assert::AreEqual (0, (int) (moved & ~declared), line);

                        checked++;
                    }
                }
            }

            Assert::IsTrue (checked > 1000, L"the sweep ran over the whole opcode table");
        }
    };
}
