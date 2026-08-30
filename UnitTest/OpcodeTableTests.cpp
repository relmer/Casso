#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"
#include "Microcode.h"
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
        //  TheTwoQuestionsAboutAWordDisagreeOnPurpose
        //
        //  An OPCODE FIELD naming an instruction and a WORD being an instruction
        //  name are different questions, and `lda` is where they part company.
        //
        //  As an opcode it is LDA -- as65 always took either case, and Merlin
        //  source written in a modern editor arrives lower-case. As a LABEL it is
        //  legal and occasionally deliberate, which Parser::ValidateLabel allows
        //  by asking the exact-case question. One function answering both would
        //  either reject lower-case opcodes or turn a legal label into an error.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (TheTwoQuestionsAboutAWordDisagreeOnPurpose)
        {
            OpcodeTable  table = BuildTable();

            Assert::IsTrue  (table.NamesAnInstruction ("lda"), L"a lower-case opcode field names LDA");
            Assert::IsTrue  (table.NamesAnInstruction ("LDA"), L"and so does the upper-case one");
            Assert::IsTrue  (table.NamesAnInstruction ("Lda"), L"and any mixture of the two");
            Assert::IsFalse (table.IsMnemonic ("lda"),
                             L"but `lda` as a WRITTEN WORD is not an instruction name, which is what keeps it a legal label");
            Assert::IsFalse (table.NamesAnInstruction ("xyz"), L"and a word that is no instruction stays none in any case");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  EncodingAndModeLookupsIgnoreCaseToo
        //
        //  The predicate above would be cosmetic if the lookups that actually
        //  emit bytes still missed. Both are asked here, because an addressing
        //  mode resolved from one table and an opcode encoded from another is
        //  how a lower-case source assembles to the wrong instruction rather
        //  than to none.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EncodingAndModeLookupsIgnoreCaseToo)
        {
            OpcodeTable  table = BuildTable();
            OpcodeEntry  upper = {};
            OpcodeEntry  lower = {};

            Assert::IsTrue (table.TryLookup ("LDA", GlobalAddressingMode::AddressingMode::Immediate, upper),
                            L"the upper-case form must encode, or this test proves nothing");
            Assert::IsTrue (table.TryLookup ("lda", GlobalAddressingMode::AddressingMode::Immediate, lower),
                            L"and the lower-case form must encode as well");
            Assert::AreEqual (static_cast<int> (upper.opcode), static_cast<int> (lower.opcode),
                              L"to the very same opcode");

            Assert::IsTrue (table.HasMode ("lda", GlobalAddressingMode::AddressingMode::Immediate),
                            L"and the mode question must agree with the encoding one");
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
            OpcodeEntry  entry = {};
            bool         found = table.TryLookup (mnemonic, mode, entry);

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





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OpcodeTableCycleCountTests
    //
    //  The number `-c` prints beside an instruction, for BOTH CPUs.
    //
    //  These counts used to be read out of a 256-entry array keyed by opcode and
    //  written for the NMOS part. Every 65C02 instruction that lives in a slot the
    //  NMOS map leaves illegal scored zero there, and a zero prints as nothing at
    //  all, so a 65C02 listing had silent holes wherever the interesting
    //  instructions were. Where the two CPUs share a slot but not its timing, the
    //  NMOS number was printed as though it were the CMOS one.
    //
    //  The SWEEP is the point of this class; the named cases only say what the
    //  numbers are. A hand-picked sample is exactly what missed the gap for as
    //  long as it existed -- nobody had walked the instruction set asking each
    //  entry whether it had an answer. Both sweeps pin an exact instruction count
    //  before asserting over the entries, because a loop over a truncated table
    //  passes while checking almost nothing and reads identically to a full one.
    //
    //  The expectations are TRANSCRIBED from published references rather than
    //  derived from the tables under test: masswerk.at's 6502 instruction set for
    //  the NMOS counts, and Bruce Clark's "65C02 Opcodes" at 6502.org for the CMOS
    //  additions and for the one shared slot the two parts time differently. A
    //  test that computes its expectation the way the code does agrees with the
    //  code's mistakes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OpcodeTableCycleCountTests)
    {
    private:
        //  The 151 documented NMOS opcodes, and 46 slots that are not
        //  instructions. Sweeping skips the latter, so their zeros are never
        //  compared against anything.
        static constexpr int  kNmosInstructionCount = 151;

        //  Those 151 again, plus the 27 CMOS additions and the 32 Rockwell bit
        //  operations. The remaining 46 slots are reserved or undefined NOP fill,
        //  which executes but is hidden from the assembler.
        static constexpr int  kCmosInstructionCount = 210;

        static constexpr size_t  kCmosAdditionCount = 59;

        //  Base cycle counts for the documented NMOS set, with no page-crossing
        //  or branch-taken penalty. Zero marks a slot that is not a documented
        //  instruction.
        static constexpr Byte  kNmosCycles[256] =
        {
            7,6,0,0,0,3,5,0,3,2,2,0,0,4,6,0,  // $00-$0F
            2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $10-$1F
            6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0,  // $20-$2F
            2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $30-$3F
            6,6,0,0,0,3,5,0,3,2,2,0,3,4,6,0,  // $40-$4F
            2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $50-$5F
            6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0,  // $60-$6F
            2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $70-$7F
            0,6,0,0,3,3,3,0,2,0,2,0,4,4,4,0,  // $80-$8F
            2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0,  // $90-$9F
            2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0,  // $A0-$AF
            2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0,  // $B0-$BF
            2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,  // $C0-$CF
            2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $D0-$DF
            2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,  // $E0-$EF
            2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $F0-$FF
        };

        struct CmosTiming
        {
            const char                           * mnemonic;
            GlobalAddressingMode::AddressingMode   mode;
            Byte                                   opcode;
            Byte                                   cycles;
        };

        //  Everything the 65C02 adds to the NMOS map, keyed the way the assembler
        //  is asked for it. BRA is listed at the three cycles an always-taken
        //  branch costs, not at the two it is stored as for the run-time
        //  taken-branch penalty to build on.
        static constexpr CmosTiming  kCmosAdditions[] =
        {
            { "TSB",  GlobalAddressingMode::ZeroPage,         0x04, 5 },
            { "TSB",  GlobalAddressingMode::Absolute,         0x0C, 6 },
            { "TRB",  GlobalAddressingMode::ZeroPage,         0x14, 5 },
            { "TRB",  GlobalAddressingMode::Absolute,         0x1C, 6 },

            { "ORA",  GlobalAddressingMode::ZeroPageIndirect, 0x12, 5 },
            { "AND",  GlobalAddressingMode::ZeroPageIndirect, 0x32, 5 },
            { "EOR",  GlobalAddressingMode::ZeroPageIndirect, 0x52, 5 },
            { "ADC",  GlobalAddressingMode::ZeroPageIndirect, 0x72, 5 },
            { "STA",  GlobalAddressingMode::ZeroPageIndirect, 0x92, 5 },
            { "LDA",  GlobalAddressingMode::ZeroPageIndirect, 0xB2, 5 },
            { "CMP",  GlobalAddressingMode::ZeroPageIndirect, 0xD2, 5 },
            { "SBC",  GlobalAddressingMode::ZeroPageIndirect, 0xF2, 5 },

            { "INC",  GlobalAddressingMode::Accumulator,      0x1A, 2 },
            { "DEC",  GlobalAddressingMode::Accumulator,      0x3A, 2 },

            { "BIT",  GlobalAddressingMode::ZeroPageX,        0x34, 4 },
            { "BIT",  GlobalAddressingMode::AbsoluteX,        0x3C, 4 },
            { "BIT",  GlobalAddressingMode::Immediate,        0x89, 2 },

            { "STZ",  GlobalAddressingMode::ZeroPage,         0x64, 3 },
            { "STZ",  GlobalAddressingMode::ZeroPageX,        0x74, 4 },
            { "STZ",  GlobalAddressingMode::Absolute,         0x9C, 4 },
            { "STZ",  GlobalAddressingMode::AbsoluteX,        0x9E, 5 },

            { "PHY",  GlobalAddressingMode::SingleByteNoOperand, 0x5A, 3 },
            { "PLY",  GlobalAddressingMode::SingleByteNoOperand, 0x7A, 4 },
            { "PHX",  GlobalAddressingMode::SingleByteNoOperand, 0xDA, 3 },
            { "PLX",  GlobalAddressingMode::SingleByteNoOperand, 0xFA, 4 },

            { "BRA",  GlobalAddressingMode::Relative,          0x80, 3 },

            { "JMP",  GlobalAddressingMode::AbsoluteXIndirect, 0x7C, 6 },

            { "RMB0", GlobalAddressingMode::ZeroPage,         0x07, 5 },
            { "RMB1", GlobalAddressingMode::ZeroPage,         0x17, 5 },
            { "RMB2", GlobalAddressingMode::ZeroPage,         0x27, 5 },
            { "RMB3", GlobalAddressingMode::ZeroPage,         0x37, 5 },
            { "RMB4", GlobalAddressingMode::ZeroPage,         0x47, 5 },
            { "RMB5", GlobalAddressingMode::ZeroPage,         0x57, 5 },
            { "RMB6", GlobalAddressingMode::ZeroPage,         0x67, 5 },
            { "RMB7", GlobalAddressingMode::ZeroPage,         0x77, 5 },

            { "SMB0", GlobalAddressingMode::ZeroPage,         0x87, 5 },
            { "SMB1", GlobalAddressingMode::ZeroPage,         0x97, 5 },
            { "SMB2", GlobalAddressingMode::ZeroPage,         0xA7, 5 },
            { "SMB3", GlobalAddressingMode::ZeroPage,         0xB7, 5 },
            { "SMB4", GlobalAddressingMode::ZeroPage,         0xC7, 5 },
            { "SMB5", GlobalAddressingMode::ZeroPage,         0xD7, 5 },
            { "SMB6", GlobalAddressingMode::ZeroPage,         0xE7, 5 },
            { "SMB7", GlobalAddressingMode::ZeroPage,         0xF7, 5 },

            { "BBR0", GlobalAddressingMode::ZeroPageRelative, 0x0F, 5 },
            { "BBR1", GlobalAddressingMode::ZeroPageRelative, 0x1F, 5 },
            { "BBR2", GlobalAddressingMode::ZeroPageRelative, 0x2F, 5 },
            { "BBR3", GlobalAddressingMode::ZeroPageRelative, 0x3F, 5 },
            { "BBR4", GlobalAddressingMode::ZeroPageRelative, 0x4F, 5 },
            { "BBR5", GlobalAddressingMode::ZeroPageRelative, 0x5F, 5 },
            { "BBR6", GlobalAddressingMode::ZeroPageRelative, 0x6F, 5 },
            { "BBR7", GlobalAddressingMode::ZeroPageRelative, 0x7F, 5 },

            { "BBS0", GlobalAddressingMode::ZeroPageRelative, 0x8F, 5 },
            { "BBS1", GlobalAddressingMode::ZeroPageRelative, 0x9F, 5 },
            { "BBS2", GlobalAddressingMode::ZeroPageRelative, 0xAF, 5 },
            { "BBS3", GlobalAddressingMode::ZeroPageRelative, 0xBF, 5 },
            { "BBS4", GlobalAddressingMode::ZeroPageRelative, 0xCF, 5 },
            { "BBS5", GlobalAddressingMode::ZeroPageRelative, 0xDF, 5 },
            { "BBS6", GlobalAddressingMode::ZeroPageRelative, 0xEF, 5 },
            { "BBS7", GlobalAddressingMode::ZeroPageRelative, 0xFF, 5 },
        };



        //  Names one entry of one instruction set, so an assertion failure says
        //  which opcode of which CPU rather than only that a number was wrong.
        static std::wstring Describe (const wchar_t * setName, int opcode, const char * mnemonic)
        {
            wchar_t  text[128] = {};

            swprintf_s (text, L"%s $%02X %hs", setName, opcode, mnemonic);

            return text;
        }



        //  Walks every legal, assembler-visible instruction in a set and asks the
        //  table what it costs. `expectedCycles` may be null, for the sweep that
        //  only cares that SOME count came back.
        //
        //  Returns how many entries it visited. The caller pins that number,
        //  which is the only thing separating a sweep that found no problems
        //  from a sweep that looked at nothing.
        static int SweepCycleCounts (const Microcode * instructionSet, const wchar_t * setName, const Byte * expectedCycles)
        {
            OpcodeTable  table   (instructionSet);
            int          visited = 0;
            int          i       = 0;



            for (i = 0; i < 256; i++)
            {
                const Microcode & mc      = instructionSet[i];
                OpcodeEntry       entry   = {};
                bool              found   = false;
                std::wstring      subject;

                if (!mc.isLegal || mc.assemblerHidden)
                {
                    continue;
                }

                subject = Describe (setName, i, mc.instructionName);
                found   = table.TryLookup (mc.instructionName, mc.globalAddressingMode, entry);

                Assert::IsTrue (found,
                    (subject + L" names no encoding at all, so the sweep proves nothing about it").c_str());
                Assert::AreEqual (i, (int) entry.opcode,
                    (subject + L" resolved to a different opcode than the one being swept").c_str());
                Assert::AreNotEqual (0, (int) entry.cycleCounts,
                    (subject + L" reports no cycle count, so a -c listing prints nothing beside it").c_str());

                if (expectedCycles != nullptr)
                {
                    Assert::AreEqual ((int) expectedCycles[i], (int) entry.cycleCounts,
                        (subject + L" does not cost what the published table says").c_str());
                }

                visited++;
            }

            return visited;
        }

    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  EveryCmosInstruction_ReportsACycleCount
        //
        //  The bug this class exists for, stated over the whole instruction set
        //  rather than over the handful of opcodes anyone thought to name.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EveryCmosInstruction_ReportsACycleCount)
        {
            TestCpu65C02  cpu;
            int           visited = 0;



            cpu.InitForTest();

            visited = SweepCycleCounts (cpu.GetInstructionSet(), L"65C02", nullptr);

            Assert::AreEqual (kCmosInstructionCount, visited,
                L"the CMOS set is 151 NMOS instructions, 27 additions and 32 Rockwell bit operations");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  EveryNmosInstruction_StillCostsWhatItDid
        //
        //  The NMOS set is where the old opcode-keyed table was right, so this is
        //  the regression half: reading the count off the instruction instead
        //  must not move a single documented 6502 number.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EveryNmosInstruction_StillCostsWhatItDid)
        {
            TestCpu  cpu;
            int      visited = 0;



            cpu.InitForTest();

            visited = SweepCycleCounts (cpu.GetInstructionSet(), L"6502", kNmosCycles);

            Assert::AreEqual (kNmosInstructionCount, visited,
                L"the documented NMOS set is 151 instructions");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CmosAdditions_CostWhatThePublishedTableSays
        //
        //  The sweep above only proves a number came back. This one says which
        //  number, for every instruction the 65C02 adds.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CmosAdditions_CostWhatThePublishedTableSays)
        {
            TestCpu65C02  cpu;



            cpu.InitForTest();

            Assert::AreEqual (kCmosAdditionCount, std::size (kCmosAdditions),
                L"the transcribed table must be whole before anything is asserted over it");

            {
                OpcodeTable  table (cpu.GetInstructionSet());

                for (const CmosTiming & timing : kCmosAdditions)
                {
                    OpcodeEntry   entry   = {};
                    bool          found   = table.TryLookup (timing.mnemonic, timing.mode, entry);
                    std::wstring  subject = Describe (L"65C02", timing.opcode, timing.mnemonic);

                    Assert::IsTrue (found, (subject + L" is missing from the assembler's table").c_str());
                    Assert::AreEqual ((int) timing.opcode, (int) entry.opcode,
                        (subject + L" assembles to the wrong byte").c_str());
                    Assert::AreEqual ((int) timing.cycles, (int) entry.cycleCounts,
                        (subject + L" does not cost what 6502.org says").c_str());
                }
            }
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JumpIndirect_IsTimedDifferentlyByEachCpu
        //
        //  $6C is a slot both parts use and time differently: the 65C02 spends a
        //  sixth cycle fixing the NMOS page-wrap bug. Nothing keyed by opcode
        //  alone can answer for both, which is the structural reason the count
        //  now comes off the instruction. The abs,X shifts below are the other
        //  case, and the one that never looked broken.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JumpIndirect_IsTimedDifferentlyByEachCpu)
        {
            TestCpu       nmos;
            TestCpu65C02  cmos;
            OpcodeEntry   nmosEntry = {};
            OpcodeEntry   cmosEntry = {};



            nmos.InitForTest();
            cmos.InitForTest();

            {
                OpcodeTable  nmosTable (nmos.GetInstructionSet());
                OpcodeTable  cmosTable (cmos.GetInstructionSet());

                Assert::IsTrue (nmosTable.TryLookup ("JMP", GlobalAddressingMode::JumpIndirect, nmosEntry),
                    L"the NMOS part must encode JMP (abs)");
                Assert::IsTrue (cmosTable.TryLookup ("JMP", GlobalAddressingMode::JumpIndirectCmos, cmosEntry),
                    L"and so must the CMOS part");
            }

            Assert::AreEqual ((int) 0x6C, (int) nmosEntry.opcode, L"both are $6C");
            Assert::AreEqual ((int) 0x6C, (int) cmosEntry.opcode, L"both are $6C");

            Assert::AreEqual ((int) 5, (int) nmosEntry.cycleCounts, L"JMP (abs) is five cycles on the NMOS 6502");
            Assert::AreEqual ((int) 6, (int) cmosEntry.cycleCounts, L"and six on the 65C02, which fixed the page-wrap bug");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  IndexedShifts_AreTimedDifferentlyByEachCpu
        //
        //  The other shared-slot divergence, and the one that never looked
        //  broken. $1E/$3E/$5E/$7E are legal on the NMOS part, so the old
        //  opcode-keyed table always had a number for them -- it was just the
        //  NMOS number, printed with equal confidence beside 65C02 source.
        //
        //  Six is the base the listing states, matching how every other
        //  page-cross-sensitive instruction is listed: `LDA $1234,X` reads four
        //  here and can cost five. `INC` and `DEC` in abs,X are seven on both
        //  parts and must stay there.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (IndexedShifts_AreTimedDifferentlyByEachCpu)
        {
            static constexpr const char *  kShifts[4] = { "ASL", "ROL", "LSR", "ROR" };
            TestCpu                        nmos;
            TestCpu65C02                   cmos;



            nmos.InitForTest();
            cmos.InitForTest();

            {
                OpcodeTable  nmosTable (nmos.GetInstructionSet());
                OpcodeTable  cmosTable (cmos.GetInstructionSet());

                for (const char * mnemonic : kShifts)
                {
                    OpcodeEntry   nmosEntry = {};
                    OpcodeEntry   cmosEntry = {};
                    std::wstring  subject   = std::wstring (mnemonic, mnemonic + strlen (mnemonic)) + L" abs,X";

                    Assert::IsTrue (nmosTable.TryLookup (mnemonic, GlobalAddressingMode::AbsoluteX, nmosEntry),
                        (subject + L" must encode on the NMOS part").c_str());
                    Assert::IsTrue (cmosTable.TryLookup (mnemonic, GlobalAddressingMode::AbsoluteX, cmosEntry),
                        (subject + L" must encode on the CMOS part").c_str());

                    Assert::AreEqual (7, (int) nmosEntry.cycleCounts,
                        (subject + L" is seven on the NMOS 6502, crossing or not").c_str());
                    Assert::AreEqual (6, (int) cmosEntry.cycleCounts,
                        (subject + L" is six on the 65C02, plus one only when the page crosses").c_str());
                }

                {
                    OpcodeEntry  incEntry = {};
                    OpcodeEntry  decEntry = {};

                    Assert::IsTrue (cmosTable.TryLookup ("INC", GlobalAddressingMode::AbsoluteX, incEntry));
                    Assert::IsTrue (cmosTable.TryLookup ("DEC", GlobalAddressingMode::AbsoluteX, decEntry));

                    Assert::AreEqual (7, (int) incEntry.cycleCounts, L"INC abs,X stayed seven on the 65C02");
                    Assert::AreEqual (7, (int) decEntry.cycleCounts, L"DEC abs,X stayed seven on the 65C02");
                }
            }
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CmosListing_PrintsACountBesideEveryInstruction
        //
        //  End to end through the assembler, because the sweep above tests the
        //  table and the complaint was about the listing. The source is the one
        //  from the bug report, widened to cover each family of addition.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CmosListing_PrintsACountBesideEveryInstruction)
        {
            TestCpu65C02      cpu;
            AssemblerOptions  options      = {};
            int               instructions = 0;



            options.generateListing = true;
            options.cycleCounts     = true;

            cpu.InitForTest();

            {
                Assembler       assembler (cpu.GetInstructionSet(), options);
                AssemblyResult  result = assembler.Assemble (
                    "        org $8000\n"
                    "        nop\n"
                    "        ora ($10)\n"
                    "        inc a\n"
                    "        stz $20\n"
                    "        tsb $30\n"
                    "        trb $30\n"
                    "        phx\n"
                    "        ply\n"
                    "        bit #$40\n"
                    "        rmb0 $50\n"
                    "        bbr0 $50,*\n"
                    "        jmp ($1234,x)\n"
                    "        jmp ($1234)\n"
                    "        bra *\n"
                    "        rts\n");

                Assert::IsTrue (result.success, L"the source must assemble");

                for (const AssemblyLine & line : result.listing)
                {
                    bool  emittedBytes = !line.bytes.empty();

                    if (!emittedBytes)
                    {
                        continue;
                    }

                    Assert::AreNotEqual (0, (int) line.cycleCounts,
                        (L"line " + std::to_wstring (line.lineNumber) + L" of the listing carries bytes but no cycle count").c_str());

                    instructions++;
                }
            }

            Assert::AreEqual (15, instructions, L"every instruction in the source must have reached the listing");
        }
    };
}
