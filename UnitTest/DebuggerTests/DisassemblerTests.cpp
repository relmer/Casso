#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Debugger/Disassembler.h"
#include "OpcodeTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblerTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DisassemblerTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////
        //
        //  SweepTable
        //
        //  Every opcode decodes to the table's own length, mnemonic and
        //  documented flag. Returns the number of opcodes checked.
        //
        ////////////////////////////////////////////////////////////////////////

        static size_t SweepTable (const Microcode * table)
        {
            Disassembler            disassembler (table);
            DisassembledInstruction instruction;

            Byte                    bytes[Disassembler::kMaxInstructionBytes] = { 0, 0x34, 0x12 };

            size_t                  checked = 0;
            HRESULT                 hr      = S_OK;



            for (int opcode = 0; opcode <= 0xFF; ++opcode)
            {
                const Microcode & microcode = table[opcode];
                size_t            expected  = 1;
                std::string       name      = microcode.isLegal ? microcode.instructionName : "???";



                if (microcode.isLegal)
                {
                    expected += OpcodeTable::GetOperandSize (microcode.globalAddressingMode);
                }

                bytes[0] = (Byte) opcode;
                hr       = disassembler.DisassembleOne (0x0300, bytes, instruction);

                Assert::AreEqual (S_OK,     hr);
                Assert::AreEqual (expected, instruction.bytes.size());
                Assert::AreEqual (name,     instruction.mnemonic);
                Assert::AreEqual (microcode.isLegal && !microcode.assemblerHidden, instruction.documented);
                ++checked;
            }

            return checked;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Decode
        //
        ////////////////////////////////////////////////////////////////////////

        static DisassembledInstruction Decode (const Microcode * table, Word address, std::initializer_list<Byte> bytes)
        {
            Disassembler            disassembler (table);
            DisassembledInstruction instruction;
            std::vector<Byte>       buffer (bytes);
            HRESULT                 hr = S_OK;



            buffer.resize (Disassembler::kMaxInstructionBytes, 0);
            hr = disassembler.DisassembleOne (address, buffer, instruction);
            Assert::AreEqual (S_OK, hr);

            return instruction;
        }



        TEST_METHOD (Sweep_6502_AllOpcodes)
        {
            TestCpu cpu;



            cpu.InitForTest();
            Assert::AreEqual ((size_t) 256, SweepTable (cpu.GetInstructionSet()));
        }



        TEST_METHOD (Sweep_65C02_AllOpcodes)
        {
            TestCpu65C02 cpu;



            cpu.InitForTest();
            Assert::AreEqual ((size_t) 256, SweepTable (cpu.GetInstructionSet()));
        }



        TEST_METHOD (Operands_MonitorForm)
        {
            TestCpu cpu;



            cpu.InitForTest();
            Assert::AreEqual (std::string ("#$A0"),    Decode (cpu.GetInstructionSet(), 0x0300, { 0xA9, 0xA0 }).operand);
            Assert::AreEqual (std::string ("($3E),Y"), Decode (cpu.GetInstructionSet(), 0x0300, { 0xB1, 0x3E }).operand);
            Assert::AreEqual (std::string ("($3E,X)"), Decode (cpu.GetInstructionSet(), 0x0300, { 0xA1, 0x3E }).operand);
            Assert::AreEqual (std::string ("$1234,X"), Decode (cpu.GetInstructionSet(), 0x0300, { 0xBD, 0x34, 0x12 }).operand);
            Assert::AreEqual (std::string ("$1234,Y"), Decode (cpu.GetInstructionSet(), 0x0300, { 0xBE, 0x34, 0x12 }).operand);
            Assert::AreEqual (std::string ("$3E,Y"),   Decode (cpu.GetInstructionSet(), 0x0300, { 0xB6, 0x3E }).operand);
            Assert::AreEqual (std::string ("($0036)"), Decode (cpu.GetInstructionSet(), 0x0300, { 0x6C, 0x36, 0x00 }).operand);
            Assert::AreEqual (std::string (""),        Decode (cpu.GetInstructionSet(), 0x0300, { 0x60 }).operand);
            Assert::AreEqual (std::string ("A"),       Decode (cpu.GetInstructionSet(), 0x0300, { 0x4A }).operand);
            Assert::AreEqual (std::string ("LDA"),     Decode (cpu.GetInstructionSet(), 0x0300, { 0xA9, 0xA0 }).mnemonic);
        }



        TEST_METHOD (Targets_BranchesAndJumps)
        {
            TestCpu                 cpu;
            DisassembledInstruction forward;
            DisassembledInstruction backward;
            DisassembledInstruction jump;



            cpu.InitForTest();
            forward  = Decode (cpu.GetInstructionSet(), 0xF808, { 0x90, 0x02 });
            backward = Decode (cpu.GetInstructionSet(), 0x0300, { 0xD0, 0xFE });
            jump     = Decode (cpu.GetInstructionSet(), 0x0300, { 0x20, 0xED, 0xFD });

            Assert::IsTrue    (forward.hasTarget);
            Assert::AreEqual  ((Word) 0xF80C,           forward.target);
            Assert::AreEqual  (std::string ("$F80C"),   forward.operand);
            Assert::AreEqual  ((Word) 0x0300,           backward.target);
            Assert::IsTrue    (jump.hasTarget);
            Assert::AreEqual  ((Word) 0xFDED,           jump.target);
            Assert::IsFalse   (Decode (cpu.GetInstructionSet(), 0x0300, { 0xAD, 0x00, 0xC0 }).hasTarget);
        }



        TEST_METHOD (Undocumented_6502)
        {
            TestCpu                 cpu;
            DisassembledInstruction lax;
            DisassembledInstruction jam;



            cpu.InitForTest();
            lax = Decode (cpu.GetInstructionSet(), 0x0300, { 0xA7, 0x3E });
            jam = Decode (cpu.GetInstructionSet(), 0x0300, { 0x02 });

            Assert::AreEqual (std::string ("LAX"), lax.mnemonic);
            Assert::IsFalse  (lax.documented);
            Assert::AreEqual (std::string ("???"), jam.mnemonic);
            Assert::AreEqual ((size_t) 1,          jam.bytes.size());
            Assert::IsFalse  (jam.documented);
        }



        TEST_METHOD (Cmos_AddressingModes)
        {
            TestCpu65C02            cpu;
            DisassembledInstruction bbr;



            cpu.InitForTest();
            bbr = Decode (cpu.GetInstructionSet(), 0x0300, { 0x0F, 0x3E, 0x10 });

            Assert::AreEqual (std::string ("BBR0"),        bbr.mnemonic);
            Assert::AreEqual (std::string ("$3E,$0313"),   bbr.operand);
            Assert::AreEqual ((Word) 0x0313,               bbr.target);
            Assert::AreEqual (std::string ("($1234,X)"),   Decode (cpu.GetInstructionSet(), 0x0300, { 0x7C, 0x34, 0x12 }).operand);
            Assert::AreEqual (std::string ("($3E)"),       Decode (cpu.GetInstructionSet(), 0x0300, { 0xB2, 0x3E }).operand);
            Assert::IsTrue   (Decode (cpu.GetInstructionSet(), 0x0300, { 0xDA }).documented);
        }



        TEST_METHOD (ShortBuffer_Fails)
        {
            TestCpu                 cpu;
            DisassembledInstruction instruction;
            const Byte              bytes[] = { 0xAD, 0x00 };
            HRESULT                 hr      = S_OK;



            cpu.InitForTest();
            hr = Disassembler (cpu.GetInstructionSet()).DisassembleOne (0x0300, bytes, instruction);
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
        }
    };
}
