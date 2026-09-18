#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Disassembler.h"
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



        ////////////////////////////////////////////////////////////////////////
        //
        //  OperandAddress
        //
        //  The address an operand names, which is what a symbol is looked up
        //  by: the location itself for direct and indexed modes, the pointer
        //  for indirect ones, and the destination for branches and jumps.
        //
        ////////////////////////////////////////////////////////////////////////

        static Word OperandAddress (const Microcode * table, std::initializer_list<Byte> bytes)
        {
            DisassembledInstruction  instruction = Decode (table, 0x0300, bytes);



            Assert::IsTrue (instruction.hasOperandAddress, L"the operand names an address");

            return instruction.operandAddress;
        }



        TEST_METHOD (OperandAddress_EveryMemoryMode)
        {
            TestCpu  cpu;



            cpu.InitForTest();
            Assert::AreEqual ((Word) 0x0006, OperandAddress (cpu.GetInstructionSet(), { 0x85, 0x06 }));         // STA $06
            Assert::AreEqual ((Word) 0x0006, OperandAddress (cpu.GetInstructionSet(), { 0x95, 0x06 }));         // STA $06,X
            Assert::AreEqual ((Word) 0x0006, OperandAddress (cpu.GetInstructionSet(), { 0xB6, 0x06 }));         // LDX $06,Y
            Assert::AreEqual ((Word) 0x0400, OperandAddress (cpu.GetInstructionSet(), { 0x8D, 0x00, 0x04 }));   // STA $0400
            Assert::AreEqual ((Word) 0x0400, OperandAddress (cpu.GetInstructionSet(), { 0x9D, 0x00, 0x04 }));   // STA $0400,X
            Assert::AreEqual ((Word) 0x0400, OperandAddress (cpu.GetInstructionSet(), { 0x99, 0x00, 0x04 }));   // STA $0400,Y
            Assert::AreEqual ((Word) 0x0006, OperandAddress (cpu.GetInstructionSet(), { 0x91, 0x06 }));         // STA ($06),Y
            Assert::AreEqual ((Word) 0x0006, OperandAddress (cpu.GetInstructionSet(), { 0x81, 0x06 }));         // STA ($06,X)
            Assert::AreEqual ((Word) 0x0036, OperandAddress (cpu.GetInstructionSet(), { 0x6C, 0x36, 0x00 }));   // JMP ($0036)
            Assert::AreEqual ((Word) 0xFDED, OperandAddress (cpu.GetInstructionSet(), { 0x20, 0xED, 0xFD }));   // JSR $FDED
            Assert::AreEqual ((Word) 0x0304, OperandAddress (cpu.GetInstructionSet(), { 0xD0, 0x02 }));         // BNE $0304
        }



        TEST_METHOD (OperandAddress_CmosModes)
        {
            TestCpu65C02  cpu;



            cpu.InitForTest();
            Assert::AreEqual ((Word) 0x0006, OperandAddress (cpu.GetInstructionSet(), { 0xB2, 0x06 }));         // LDA ($06)
            Assert::AreEqual ((Word) 0x1234, OperandAddress (cpu.GetInstructionSet(), { 0x7C, 0x34, 0x12 }));   // JMP ($1234,X)
            Assert::AreEqual ((Word) 0x0313, OperandAddress (cpu.GetInstructionSet(), { 0x0F, 0x3E, 0x10 }));   // BBR0 $3E,$0313
        }



        TEST_METHOD (OperandAddress_AbsentWhereNoAddressIsNamed)
        {
            TestCpu  cpu;



            cpu.InitForTest();
            Assert::IsFalse (Decode (cpu.GetInstructionSet(), 0x0300, { 0xA9, 0x06 }).hasOperandAddress, L"immediate");
            Assert::IsFalse (Decode (cpu.GetInstructionSet(), 0x0300, { 0x4A }).hasOperandAddress,       L"accumulator");
            Assert::IsFalse (Decode (cpu.GetInstructionSet(), 0x0300, { 0x60 }).hasOperandAddress,       L"implied");
            Assert::IsFalse (Decode (cpu.GetInstructionSet(), 0x0300, { 0x02 }).hasOperandAddress,       L"undefined");
        }



        TEST_METHOD (SubstituteSymbol_ReplacesTheAddressInEveryForm)
        {
            Assert::AreEqual (std::string ("PTR"),         Disassembler::SubstituteSymbol ("$06",       0x0006, "PTR"));
            Assert::AreEqual (std::string ("PTR,X"),       Disassembler::SubstituteSymbol ("$06,X",     0x0006, "PTR"));
            Assert::AreEqual (std::string ("(PTR),Y"),     Disassembler::SubstituteSymbol ("($06),Y",   0x0006, "PTR"));
            Assert::AreEqual (std::string ("(PTR,X)"),     Disassembler::SubstituteSymbol ("($06,X)",   0x0006, "PTR"));
            Assert::AreEqual (std::string ("SCREEN,X"),    Disassembler::SubstituteSymbol ("$0400,X",   0x0400, "SCREEN"));
            Assert::AreEqual (std::string ("(VECT)"),      Disassembler::SubstituteSymbol ("($0036)",   0x0036, "VECT"));
            Assert::AreEqual (std::string ("COUT"),        Disassembler::SubstituteSymbol ("$FDED",     0xFDED, "COUT"));
        }



        TEST_METHOD (SubstituteSymbol_ABitBranchNamesItsDestination)
        {
            Assert::AreEqual (std::string ("$3E,LOOP"), Disassembler::SubstituteSymbol ("$3E,$0313", 0x0313, "LOOP"));
        }



        TEST_METHOD (SubstituteSymbol_LeavesTextAloneWithoutAMatch)
        {
            Assert::AreEqual (std::string ("#$06"),  Disassembler::SubstituteSymbol ("#$06",  0x0006, "PTR"),  L"an immediate is a value, not an address");
            Assert::AreEqual (std::string ("$0400"), Disassembler::SubstituteSymbol ("$0400", 0x0401, "X1"));
            Assert::AreEqual (std::string ("$0400"), Disassembler::SubstituteSymbol ("$0400", 0x0400, ""),    L"no name, no change");
        }
    };
}
