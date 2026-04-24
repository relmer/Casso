#include "Pch.h"

#include "TestHelpers.h"
#include "OpcodeTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace OpcodeTableTests
{
    TEST_CLASS (OpcodeTableBasicTests)
    {
    private:
        static OpcodeTable BuildTable ()
        {
            TestCpu cpu;
            cpu.InitForTest ();
            return OpcodeTable (cpu.GetInstructionSet ());
        }

    public:

        TEST_METHOD (Lookup_LDA_Immediate_Returns_A9)
        {
            OpcodeTable table = BuildTable ();
            OpcodeEntry entry = {};

            bool found = table.Lookup ("LDA", GlobalAddressingMode::Immediate, entry);

            Assert::IsTrue (found);
            Assert::AreEqual ((Byte) 0xA9, entry.opcode);
            Assert::AreEqual ((Byte) 1,    entry.operandSize);
        }

        TEST_METHOD (Lookup_STA_ZeroPage_Returns_85)
        {
            OpcodeTable table = BuildTable ();
            OpcodeEntry entry = {};

            bool found = table.Lookup ("STA", GlobalAddressingMode::ZeroPage, entry);

            Assert::IsTrue (found);
            Assert::AreEqual ((Byte) 0x85, entry.opcode);
        }

        TEST_METHOD (IsMnemonic_LDA_ReturnsTrue)
        {
            OpcodeTable table = BuildTable ();

            Assert::IsTrue (table.IsMnemonic ("LDA"));
        }

        TEST_METHOD (IsMnemonic_XYZ_ReturnsFalse)
        {
            OpcodeTable table = BuildTable ();

            Assert::IsFalse (table.IsMnemonic ("XYZ"));
        }

        TEST_METHOD (HasMode_LDA_Immediate_ReturnsTrue)
        {
            OpcodeTable table = BuildTable ();

            Assert::IsTrue (table.HasMode ("LDA", GlobalAddressingMode::Immediate));
        }

        TEST_METHOD (HasMode_LDA_SingleByte_ReturnsFalse)
        {
            OpcodeTable table = BuildTable ();

            Assert::IsFalse (table.HasMode ("LDA", GlobalAddressingMode::SingleByteNoOperand));
        }

        TEST_METHOD (AllStandardMnemonics_HaveAtLeastOneEntry)
        {
            OpcodeTable table = BuildTable ();

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
                    (std::wstring (L"Missing mnemonic: ") + std::wstring (mnemonic, mnemonic + strlen (mnemonic))).c_str ());
            }
        }
    };
}
