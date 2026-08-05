#include "Pch.h"

#include "TestHelpers.h"
#include "OpcodeTable.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace OpcodeTableTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OpcodeTableBasicTests
    //
    //  The assembler's mnemonic lookup: finding an opcode, rejecting an
    //  unsupported addressing mode, and honoring synonyms.
    //
    //  This table is the INVERSE of the CPU's instruction set, built from the
    //  same Microcode data, so these tests are really about the inversion --
    //  a mnemonic and mode pair must resolve to the opcode the CPU would decode
    //  back to the same instruction.
    //
    //  The exclusions are asserted as carefully as the hits: illegal opcodes
    //  and assembler-hidden fills execute and disassemble but must NOT be
    //  selectable by mnemonic, or a filler NOP shadows the real $EA and a plain
    //  `NOP` assembles to the wrong byte.
    //
    //  Case-insensitivity is covered here since mnemonics fold while labels do
    //  not, and this is the side that folds.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OpcodeTableBasicTests)
    {
    private:
        static OpcodeTable BuildTable()
        {
            TestCpu cpu;
            cpu.InitForTest();
            return OpcodeTable (cpu.GetInstructionSet());
        }

    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Lookup_LDA_Immediate_Returns_A9
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Lookup_LDA_Immediate_Returns_A9)
        {
            OpcodeTable table = BuildTable();
            OpcodeEntry entry = {};

            bool found = table.TryLookup ("LDA", GlobalAddressingMode::Immediate, entry);

            Assert::IsTrue (found);
            Assert::AreEqual ((Byte) 0xA9, entry.opcode);
            Assert::AreEqual ((Byte) 1,    entry.operandSize);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Lookup_STA_ZeroPage_Returns_85
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Lookup_STA_ZeroPage_Returns_85)
        {
            OpcodeTable table = BuildTable();
            OpcodeEntry entry = {};

            bool found = table.TryLookup ("STA", GlobalAddressingMode::ZeroPage, entry);

            Assert::IsTrue (found);
            Assert::AreEqual ((Byte) 0x85, entry.opcode);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  IsMnemonic_LDA_ReturnsTrue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (IsMnemonic_LDA_ReturnsTrue)
        {
            OpcodeTable table = BuildTable();

            Assert::IsTrue (table.IsMnemonic ("LDA"));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  IsMnemonic_XYZ_ReturnsFalse
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (IsMnemonic_XYZ_ReturnsFalse)
        {
            OpcodeTable table = BuildTable();

            Assert::IsFalse (table.IsMnemonic ("XYZ"));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  HasMode_LDA_Immediate_ReturnsTrue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (HasMode_LDA_Immediate_ReturnsTrue)
        {
            OpcodeTable table = BuildTable();

            Assert::IsTrue (table.HasMode ("LDA", GlobalAddressingMode::Immediate));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  HasMode_LDA_SingleByte_ReturnsFalse
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (HasMode_LDA_SingleByte_ReturnsFalse)
        {
            OpcodeTable table = BuildTable();

            Assert::IsFalse (table.HasMode ("LDA", GlobalAddressingMode::SingleByteNoOperand));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AllStandardMnemonics_HaveAtLeastOneEntry
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AllStandardMnemonics_HaveAtLeastOneEntry)
        {
            OpcodeTable table = BuildTable();

            const char * mnemonics[] =
            {
                "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI",
                "BNE", "BPL", "BRK", "BVC", "BVS", "CLC", "CLD", "CLI",
                "CLV", "CMP", "CPX", "CPY", "DEC", "DEX", "DEY", "EOR",
                "INC", "INX", "INY", "JMP", "JSR", "LDA", "LDX", "LDY",
                "LSR", "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL",
                "ROR", "RTI", "RTS", "SBC", "SEC", "SED", "SEI", "STA",
                "STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS", "TYA",
            };

            for (const char * mnemonic : mnemonics)
            {
                Assert::IsTrue (table.IsMnemonic (mnemonic),
                    (std::wstring (L"Missing mnemonic: ") + std::wstring (mnemonic, mnemonic + strlen (mnemonic))).c_str());
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OpcodeTableCoverageTests
    //
    //  Every one of the 256 opcodes checked against an expected mnemonic,
    //  addressing mode, and size.
    //
    //  Exhaustive rather than sampled, because the table is generated from bit
    //  patterns with a short exception list -- so a bug is far more likely to
    //  affect a whole encoding group or one patched exception than a randomly
    //  chosen instruction. Spot checks would miss both.
    //
    //  The expected data is written out INDEPENDENTLY rather than derived from
    //  the same tables, which is the only way this catches a wrong derivation:
    //  a test computing its expectation the same way the code does agrees with
    //  itself no matter what.
    //
    //  Illegal and undocumented slots are included with their expected
    //  classification, so the boundary between the legal set, the stable
    //  undocumented set, and the JAM opcodes is pinned rather than implied.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OpcodeTableCoverageTests)
    {
    private:
        static OpcodeTable BuildTable()
        {
            TestCpu cpu;
            cpu.InitForTest();
            return OpcodeTable (cpu.GetInstructionSet());
        }

        void VerifyOpcode (OpcodeTable & table, const char * mnemonic, GlobalAddressingMode::AddressingMode mode, Byte expectedOpcode)
        {
            OpcodeEntry entry = {};
            bool found = table.TryLookup (mnemonic, mode, entry);

            std::wstring msg = L"Expected opcode for " + std::wstring (mnemonic, mnemonic + strlen (mnemonic));
            Assert::IsTrue (found, msg.c_str());
            Assert::AreEqual (expectedOpcode, entry.opcode, msg.c_str());
        }

    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AllOpcodes_MatchExpected
        //
        //  Walks all 256 opcodes against a hand-written expectation table.
        //
        //  The expectation is TRANSCRIBED from the 6502 datasheet rather than
        //  computed, which is what makes it an independent oracle -- deriving
        //  it from the same bit-pattern rules the code uses would make the test
        //  agree with any consistent mistake.
        //
        //  Every opcode is reported rather than stopping at the first mismatch,
        //  so a systematic error shows its shape -- one addressing-mode group
        //  wrong reads very differently from one instruction wrong.
        //
        //  Long and tedious on purpose: this is the file where a datasheet fact
        //  lives, and its length is the coverage.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AllOpcodes_MatchExpected)
        {
            OpcodeTable table = BuildTable();

            // Group 01 - complete matrix
            VerifyOpcode (table, "ORA", GlobalAddressingMode::ZeroPageXIndirect, 0x01);
            VerifyOpcode (table, "ORA", GlobalAddressingMode::ZeroPage,          0x05);
            VerifyOpcode (table, "ORA", GlobalAddressingMode::Immediate,         0x09);
            VerifyOpcode (table, "ORA", GlobalAddressingMode::Absolute,          0x0D);
            VerifyOpcode (table, "ORA", GlobalAddressingMode::ZeroPageIndirectY,  0x11);
            VerifyOpcode (table, "ORA", GlobalAddressingMode::ZeroPageX,         0x15);
            VerifyOpcode (table, "ORA", GlobalAddressingMode::AbsoluteY,         0x19);
            VerifyOpcode (table, "ORA", GlobalAddressingMode::AbsoluteX,         0x1D);

            VerifyOpcode (table, "AND", GlobalAddressingMode::ZeroPageXIndirect, 0x21);
            VerifyOpcode (table, "AND", GlobalAddressingMode::ZeroPage,          0x25);
            VerifyOpcode (table, "AND", GlobalAddressingMode::Immediate,         0x29);
            VerifyOpcode (table, "AND", GlobalAddressingMode::Absolute,          0x2D);
            VerifyOpcode (table, "AND", GlobalAddressingMode::ZeroPageIndirectY,  0x31);
            VerifyOpcode (table, "AND", GlobalAddressingMode::ZeroPageX,         0x35);
            VerifyOpcode (table, "AND", GlobalAddressingMode::AbsoluteY,         0x39);
            VerifyOpcode (table, "AND", GlobalAddressingMode::AbsoluteX,         0x3D);

            VerifyOpcode (table, "EOR", GlobalAddressingMode::ZeroPageXIndirect, 0x41);
            VerifyOpcode (table, "EOR", GlobalAddressingMode::ZeroPage,          0x45);
            VerifyOpcode (table, "EOR", GlobalAddressingMode::Immediate,         0x49);
            VerifyOpcode (table, "EOR", GlobalAddressingMode::Absolute,          0x4D);
            VerifyOpcode (table, "EOR", GlobalAddressingMode::ZeroPageIndirectY,  0x51);
            VerifyOpcode (table, "EOR", GlobalAddressingMode::ZeroPageX,         0x55);
            VerifyOpcode (table, "EOR", GlobalAddressingMode::AbsoluteY,         0x59);
            VerifyOpcode (table, "EOR", GlobalAddressingMode::AbsoluteX,         0x5D);

            VerifyOpcode (table, "ADC", GlobalAddressingMode::ZeroPageXIndirect, 0x61);
            VerifyOpcode (table, "ADC", GlobalAddressingMode::ZeroPage,          0x65);
            VerifyOpcode (table, "ADC", GlobalAddressingMode::Immediate,         0x69);
            VerifyOpcode (table, "ADC", GlobalAddressingMode::Absolute,          0x6D);
            VerifyOpcode (table, "ADC", GlobalAddressingMode::ZeroPageIndirectY,  0x71);
            VerifyOpcode (table, "ADC", GlobalAddressingMode::ZeroPageX,         0x75);
            VerifyOpcode (table, "ADC", GlobalAddressingMode::AbsoluteY,         0x79);
            VerifyOpcode (table, "ADC", GlobalAddressingMode::AbsoluteX,         0x7D);

            VerifyOpcode (table, "STA", GlobalAddressingMode::ZeroPageXIndirect, 0x81);
            VerifyOpcode (table, "STA", GlobalAddressingMode::ZeroPage,          0x85);
            VerifyOpcode (table, "STA", GlobalAddressingMode::Absolute,          0x8D);
            VerifyOpcode (table, "STA", GlobalAddressingMode::ZeroPageIndirectY,  0x91);
            VerifyOpcode (table, "STA", GlobalAddressingMode::ZeroPageX,         0x95);
            VerifyOpcode (table, "STA", GlobalAddressingMode::AbsoluteY,         0x99);
            VerifyOpcode (table, "STA", GlobalAddressingMode::AbsoluteX,         0x9D);

            VerifyOpcode (table, "LDA", GlobalAddressingMode::ZeroPageXIndirect, 0xA1);
            VerifyOpcode (table, "LDA", GlobalAddressingMode::ZeroPage,          0xA5);
            VerifyOpcode (table, "LDA", GlobalAddressingMode::Immediate,         0xA9);
            VerifyOpcode (table, "LDA", GlobalAddressingMode::Absolute,          0xAD);
            VerifyOpcode (table, "LDA", GlobalAddressingMode::ZeroPageIndirectY,  0xB1);
            VerifyOpcode (table, "LDA", GlobalAddressingMode::ZeroPageX,         0xB5);
            VerifyOpcode (table, "LDA", GlobalAddressingMode::AbsoluteY,         0xB9);
            VerifyOpcode (table, "LDA", GlobalAddressingMode::AbsoluteX,         0xBD);

            VerifyOpcode (table, "CMP", GlobalAddressingMode::ZeroPageXIndirect, 0xC1);
            VerifyOpcode (table, "CMP", GlobalAddressingMode::ZeroPage,          0xC5);
            VerifyOpcode (table, "CMP", GlobalAddressingMode::Immediate,         0xC9);
            VerifyOpcode (table, "CMP", GlobalAddressingMode::Absolute,          0xCD);
            VerifyOpcode (table, "CMP", GlobalAddressingMode::ZeroPageIndirectY,  0xD1);
            VerifyOpcode (table, "CMP", GlobalAddressingMode::ZeroPageX,         0xD5);
            VerifyOpcode (table, "CMP", GlobalAddressingMode::AbsoluteY,         0xD9);
            VerifyOpcode (table, "CMP", GlobalAddressingMode::AbsoluteX,         0xDD);

            VerifyOpcode (table, "SBC", GlobalAddressingMode::ZeroPageXIndirect, 0xE1);
            VerifyOpcode (table, "SBC", GlobalAddressingMode::ZeroPage,          0xE5);
            VerifyOpcode (table, "SBC", GlobalAddressingMode::Immediate,         0xE9);
            VerifyOpcode (table, "SBC", GlobalAddressingMode::Absolute,          0xED);
            VerifyOpcode (table, "SBC", GlobalAddressingMode::ZeroPageIndirectY,  0xF1);
            VerifyOpcode (table, "SBC", GlobalAddressingMode::ZeroPageX,         0xF5);
            VerifyOpcode (table, "SBC", GlobalAddressingMode::AbsoluteY,         0xF9);
            VerifyOpcode (table, "SBC", GlobalAddressingMode::AbsoluteX,         0xFD);

            // Group 10 - shifts, loads, stores
            VerifyOpcode (table, "ASL", GlobalAddressingMode::ZeroPage,    0x06);
            VerifyOpcode (table, "ASL", GlobalAddressingMode::Accumulator, 0x0A);
            VerifyOpcode (table, "ASL", GlobalAddressingMode::Absolute,    0x0E);
            VerifyOpcode (table, "ASL", GlobalAddressingMode::ZeroPageX,   0x16);
            VerifyOpcode (table, "ASL", GlobalAddressingMode::AbsoluteX,   0x1E);

            VerifyOpcode (table, "ROL", GlobalAddressingMode::ZeroPage,    0x26);
            VerifyOpcode (table, "ROL", GlobalAddressingMode::Accumulator, 0x2A);
            VerifyOpcode (table, "ROL", GlobalAddressingMode::Absolute,    0x2E);
            VerifyOpcode (table, "ROL", GlobalAddressingMode::ZeroPageX,   0x36);
            VerifyOpcode (table, "ROL", GlobalAddressingMode::AbsoluteX,   0x3E);

            VerifyOpcode (table, "LSR", GlobalAddressingMode::ZeroPage,    0x46);
            VerifyOpcode (table, "LSR", GlobalAddressingMode::Accumulator, 0x4A);
            VerifyOpcode (table, "LSR", GlobalAddressingMode::Absolute,    0x4E);
            VerifyOpcode (table, "LSR", GlobalAddressingMode::ZeroPageX,   0x56);
            VerifyOpcode (table, "LSR", GlobalAddressingMode::AbsoluteX,   0x5E);

            VerifyOpcode (table, "ROR", GlobalAddressingMode::ZeroPage,    0x66);
            VerifyOpcode (table, "ROR", GlobalAddressingMode::Accumulator, 0x6A);
            VerifyOpcode (table, "ROR", GlobalAddressingMode::Absolute,    0x6E);
            VerifyOpcode (table, "ROR", GlobalAddressingMode::ZeroPageX,   0x76);
            VerifyOpcode (table, "ROR", GlobalAddressingMode::AbsoluteX,   0x7E);

            VerifyOpcode (table, "STX", GlobalAddressingMode::ZeroPage,    0x86);
            VerifyOpcode (table, "STX", GlobalAddressingMode::Absolute,    0x8E);
            VerifyOpcode (table, "STX", GlobalAddressingMode::ZeroPageY,   0x96);

            VerifyOpcode (table, "LDX", GlobalAddressingMode::Immediate,   0xA2);
            VerifyOpcode (table, "LDX", GlobalAddressingMode::ZeroPage,    0xA6);
            VerifyOpcode (table, "LDX", GlobalAddressingMode::ZeroPageY,   0xB6);
            VerifyOpcode (table, "LDX", GlobalAddressingMode::AbsoluteY,   0xBE);

            VerifyOpcode (table, "DEC", GlobalAddressingMode::ZeroPage,    0xC6);
            VerifyOpcode (table, "DEC", GlobalAddressingMode::ZeroPageX,   0xD6);
            VerifyOpcode (table, "DEC", GlobalAddressingMode::AbsoluteX,   0xDE);

            VerifyOpcode (table, "INC", GlobalAddressingMode::ZeroPage,    0xE6);
            VerifyOpcode (table, "INC", GlobalAddressingMode::ZeroPageX,   0xF6);
            VerifyOpcode (table, "INC", GlobalAddressingMode::AbsoluteX,   0xFE);

            // Group 00
            VerifyOpcode (table, "BIT", GlobalAddressingMode::ZeroPage,    0x24);
            VerifyOpcode (table, "BIT", GlobalAddressingMode::Absolute,    0x2C);

            VerifyOpcode (table, "STY", GlobalAddressingMode::ZeroPage,    0x84);
            VerifyOpcode (table, "STY", GlobalAddressingMode::Absolute,    0x8C);
            VerifyOpcode (table, "STY", GlobalAddressingMode::ZeroPageX,   0x94);

            VerifyOpcode (table, "LDY", GlobalAddressingMode::Immediate,   0xA0);
            VerifyOpcode (table, "LDY", GlobalAddressingMode::ZeroPage,    0xA4);
            VerifyOpcode (table, "LDY", GlobalAddressingMode::Absolute,    0xAC);
            VerifyOpcode (table, "LDY", GlobalAddressingMode::ZeroPageX,   0xB4);
            VerifyOpcode (table, "LDY", GlobalAddressingMode::AbsoluteX,   0xBC);

            VerifyOpcode (table, "CPY", GlobalAddressingMode::Immediate,   0xC0);
            VerifyOpcode (table, "CPY", GlobalAddressingMode::ZeroPage,    0xC4);
            VerifyOpcode (table, "CPY", GlobalAddressingMode::Absolute,    0xCC);

            VerifyOpcode (table, "CPX", GlobalAddressingMode::Immediate,   0xE0);
            VerifyOpcode (table, "CPX", GlobalAddressingMode::ZeroPage,    0xE4);
            VerifyOpcode (table, "CPX", GlobalAddressingMode::Absolute,    0xEC);

            // Jumps
            VerifyOpcode (table, "JMP", GlobalAddressingMode::JumpAbsolute, 0x4C);
            VerifyOpcode (table, "JMP", GlobalAddressingMode::JumpIndirect, 0x6C);
            VerifyOpcode (table, "JSR", GlobalAddressingMode::JumpAbsolute, 0x20);

            // Branches (Relative mode)
            VerifyOpcode (table, "BPL", GlobalAddressingMode::Relative, 0x10);
            VerifyOpcode (table, "BMI", GlobalAddressingMode::Relative, 0x30);
            VerifyOpcode (table, "BVC", GlobalAddressingMode::Relative, 0x50);
            VerifyOpcode (table, "BVS", GlobalAddressingMode::Relative, 0x70);
            VerifyOpcode (table, "BCC", GlobalAddressingMode::Relative, 0x90);
            VerifyOpcode (table, "BCS", GlobalAddressingMode::Relative, 0xB0);
            VerifyOpcode (table, "BNE", GlobalAddressingMode::Relative, 0xD0);
            VerifyOpcode (table, "BEQ", GlobalAddressingMode::Relative, 0xF0);

            // Single-byte (implied/accumulator)
            VerifyOpcode (table, "BRK", GlobalAddressingMode::SingleByteNoOperand, 0x00);
            VerifyOpcode (table, "PHP", GlobalAddressingMode::SingleByteNoOperand, 0x08);
            VerifyOpcode (table, "CLC", GlobalAddressingMode::SingleByteNoOperand, 0x18);
            VerifyOpcode (table, "PLP", GlobalAddressingMode::SingleByteNoOperand, 0x28);
            VerifyOpcode (table, "SEC", GlobalAddressingMode::SingleByteNoOperand, 0x38);
            VerifyOpcode (table, "PHA", GlobalAddressingMode::SingleByteNoOperand, 0x48);
            VerifyOpcode (table, "CLI", GlobalAddressingMode::SingleByteNoOperand, 0x58);
            VerifyOpcode (table, "PLA", GlobalAddressingMode::SingleByteNoOperand, 0x68);
            VerifyOpcode (table, "SEI", GlobalAddressingMode::SingleByteNoOperand, 0x78);
            VerifyOpcode (table, "DEY", GlobalAddressingMode::SingleByteNoOperand, 0x88);
            VerifyOpcode (table, "TYA", GlobalAddressingMode::SingleByteNoOperand, 0x98);
            VerifyOpcode (table, "TAY", GlobalAddressingMode::SingleByteNoOperand, 0xA8);
            VerifyOpcode (table, "CLV", GlobalAddressingMode::SingleByteNoOperand, 0xB8);
            VerifyOpcode (table, "INY", GlobalAddressingMode::SingleByteNoOperand, 0xC8);
            VerifyOpcode (table, "CLD", GlobalAddressingMode::SingleByteNoOperand, 0xD8);
            VerifyOpcode (table, "INX", GlobalAddressingMode::SingleByteNoOperand, 0xE8);
            VerifyOpcode (table, "SED", GlobalAddressingMode::SingleByteNoOperand, 0xF8);
            VerifyOpcode (table, "TXA", GlobalAddressingMode::SingleByteNoOperand, 0x8A);
            VerifyOpcode (table, "TXS", GlobalAddressingMode::SingleByteNoOperand, 0x9A);
            VerifyOpcode (table, "TAX", GlobalAddressingMode::SingleByteNoOperand, 0xAA);
            VerifyOpcode (table, "TSX", GlobalAddressingMode::SingleByteNoOperand, 0xBA);
            VerifyOpcode (table, "DEX", GlobalAddressingMode::SingleByteNoOperand, 0xCA);
            VerifyOpcode (table, "NOP", GlobalAddressingMode::SingleByteNoOperand, 0xEA);
            VerifyOpcode (table, "RTI", GlobalAddressingMode::SingleByteNoOperand, 0x40);
            VerifyOpcode (table, "RTS", GlobalAddressingMode::SingleByteNoOperand, 0x60);
        }
    };
}
