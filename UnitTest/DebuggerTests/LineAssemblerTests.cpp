#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "OpcodeTable.h"
#include "Debugger/Disassembler.h"
#include "Debugger/LineAssembler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LineAssemblerTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (LineAssemblerTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////
        //
        //  Assemble
        //
        //  Asserts the status and, on success, the bytes.
        //
        ////////////////////////////////////////////////////////////////////////

        static void Assemble (const Microcode              * table,
                              Word                           address,
                              const std::string            & line,
                              LineAssemblyStatus             expectedStatus,
                              std::initializer_list<Byte>    expectedBytes = {})
        {
            OpcodeTable        opcodes (table);
            std::vector<Byte>  bytes;
            std::string        error;
            LineAssemblyStatus status = LineAssemblyStatus::Ok;



            status = LineAssembler (opcodes).TryAssemble (address, line, bytes, error);

            Assert::AreEqual ((int) expectedStatus, (int) status, std::wstring (line.begin(), line.end()).c_str());
            Assert::IsTrue   (std::vector<Byte> (expectedBytes) == bytes, std::wstring (line.begin(), line.end()).c_str());
            Assert::AreEqual (status == LineAssemblyStatus::Ok, error.empty());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  RoundTripDocumented
        //
        //  Every documented opcode, disassembled and assembled again, gives
        //  back its own bytes. Returns the number of opcodes checked.
        //
        ////////////////////////////////////////////////////////////////////////

        static size_t RoundTripDocumented (const Microcode * table)
        {
            static constexpr Word  kAddress = 0x0300;

            OpcodeTable             opcodes (table);
            LineAssembler           assembler (opcodes);
            Disassembler            disassembler (table);
            DisassembledInstruction instruction;
            std::vector<Byte>       bytes;
            std::string             error;
            size_t                  checked = 0;

            Byte                    source[Disassembler::kMaxInstructionBytes] = { 0, 0x34, 0x12 };



            for (int opcode = 0; opcode <= 0xFF; ++opcode)
            {
                std::string        line;
                std::wstring       where;
                LineAssemblyStatus status = LineAssemblyStatus::Ok;
                HRESULT            hr     = S_OK;



                source[0] = (Byte) opcode;
                hr        = disassembler.DisassembleOne (kAddress, source, instruction);
                Assert::AreEqual (S_OK, hr);

                if (!instruction.documented)
                {
                    continue;
                }

                line   = instruction.mnemonic + " " + instruction.operand;
                where  = std::wstring (line.begin(), line.end());
                status = assembler.TryAssemble (kAddress, line, bytes, error);

                Assert::AreEqual ((int) LineAssemblyStatus::Ok, (int) status, where.c_str());
                Assert::IsTrue   (instruction.bytes == bytes, where.c_str());
                ++checked;
            }

            return checked;
        }



        TEST_METHOD (Forms_6502)
        {
            TestCpu cpu;



            cpu.InitForTest();
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA #$41",      LineAssemblyStatus::Ok, { 0xA9, 0x41 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "lda #41",       LineAssemblyStatus::Ok, { 0xA9, 0x41 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "JMP ($0036)",   LineAssemblyStatus::Ok, { 0x6C, 0x36, 0x00 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA 36",        LineAssemblyStatus::Ok, { 0xA5, 0x36 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA 0036",      LineAssemblyStatus::Ok, { 0xAD, 0x36, 0x00 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA $1234,X",   LineAssemblyStatus::Ok, { 0xBD, 0x34, 0x12 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA ($3E),Y",   LineAssemblyStatus::Ok, { 0xB1, 0x3E });
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA ($3E,X)",   LineAssemblyStatus::Ok, { 0xA1, 0x3E });
            Assemble (cpu.GetInstructionSet(), 0x0300, "STX 3E,Y",      LineAssemblyStatus::Ok, { 0x96, 0x3E });
            Assemble (cpu.GetInstructionSet(), 0x0300, "ASL",           LineAssemblyStatus::Ok, { 0x0A });
            Assemble (cpu.GetInstructionSet(), 0x0300, "ASL A",         LineAssemblyStatus::Ok, { 0x0A });
            Assemble (cpu.GetInstructionSet(), 0x0300, "JSR FDED",      LineAssemblyStatus::Ok, { 0x20, 0xED, 0xFD });
            Assemble (cpu.GetInstructionSet(), 0x0300, "RTS",           LineAssemblyStatus::Ok, { 0x60 });
        }



        TEST_METHOD (Branches_ToAbsoluteTargets)
        {
            TestCpu cpu;



            cpu.InitForTest();
            Assemble (cpu.GetInstructionSet(), 0x0310, "BNE $0300",     LineAssemblyStatus::Ok,               { 0xD0, 0xEE });
            Assemble (cpu.GetInstructionSet(), 0x0300, "BCC $0381",     LineAssemblyStatus::Ok,               { 0x90, 0x7F });
            Assemble (cpu.GetInstructionSet(), 0x0300, "BCC $0382",     LineAssemblyStatus::BranchOutOfRange);
            Assemble (cpu.GetInstructionSet(), 0x0300, "BEQ $0282",     LineAssemblyStatus::Ok,               { 0xF0, 0x80 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "BEQ $0281",     LineAssemblyStatus::BranchOutOfRange);
        }



        TEST_METHOD (Errors)
        {
            TestCpu cpu;



            cpu.InitForTest();
            Assemble (cpu.GetInstructionSet(), 0x0300, "XYZ",           LineAssemblyStatus::UnknownMnemonic);
            Assemble (cpu.GetInstructionSet(), 0x0300, "STZ $3E",       LineAssemblyStatus::UnknownMnemonic);
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA #$1234",    LineAssemblyStatus::InvalidOperand);
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA $12345",    LineAssemblyStatus::InvalidOperand);
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA ($1234),Y", LineAssemblyStatus::InvalidOperand);
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA #ZZ",       LineAssemblyStatus::InvalidOperand);
            Assemble (cpu.GetInstructionSet(), 0x0300, "STA #$41",      LineAssemblyStatus::ModeNotAvailable);
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA ($3E)",     LineAssemblyStatus::ModeNotAvailable);
        }



        TEST_METHOD (Forms_65C02)
        {
            TestCpu65C02 cpu;



            cpu.InitForTest();
            Assemble (cpu.GetInstructionSet(), 0x0300, "BRA $0310",       LineAssemblyStatus::Ok, { 0x80, 0x0E });
            Assemble (cpu.GetInstructionSet(), 0x0300, "LDA ($3E)",       LineAssemblyStatus::Ok, { 0xB2, 0x3E });
            Assemble (cpu.GetInstructionSet(), 0x0300, "BBR0 $3E,$0313",  LineAssemblyStatus::Ok, { 0x0F, 0x3E, 0x10 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "STZ $3E",         LineAssemblyStatus::Ok, { 0x64, 0x3E });
            Assemble (cpu.GetInstructionSet(), 0x0300, "JMP ($1234,X)",   LineAssemblyStatus::Ok, { 0x7C, 0x34, 0x12 });
            Assemble (cpu.GetInstructionSet(), 0x0300, "JMP ($0036)",     LineAssemblyStatus::Ok, { 0x6C, 0x36, 0x00 });
        }



        TEST_METHOD (RoundTrip_6502)
        {
            TestCpu cpu;



            cpu.InitForTest();
            Assert::IsTrue (RoundTripDocumented (cpu.GetInstructionSet()) > 0);
        }



        TEST_METHOD (RoundTrip_65C02)
        {
            TestCpu65C02 cpu;



            cpu.InitForTest();
            Assert::IsTrue (RoundTripDocumented (cpu.GetInstructionSet()) > 0);
        }
    };
}
