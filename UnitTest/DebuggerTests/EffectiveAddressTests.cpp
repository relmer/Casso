#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Debugger/EffectiveAddress.h"
#include "MockExpressionContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EffectiveAddressTests
//
//  One hand-computed case per addressing mode, on the table that has it, plus
//  a sweep proving every legal opcode on both CPUs predicts without error.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (EffectiveAddressTests)
    {
    public:

        struct Case
        {
            const char *                          name;
            GlobalAddressingMode::AddressingMode  mode;
            bool                                  isCmos;
            std::vector<Byte>                     bytes;
            std::vector<PredictedTouch>           expected;
        };

        static constexpr Word  kPc = 0x0300;

        // Registers: X=$10, Y=$20. Zero page $FE/$FF holds pointer $2010,
        // $30/$31 holds $12F0, and $10FF/$1000 hold the NMOS-wrapped JMP
        // pointer $ABCD ($10FF=$CD, $1000=$AB) while $1100 holds $EF.
        static MockExpressionContext MakeContext()
        {
            MockExpressionContext  context;



            context.memory[0x00FE] = 0x10;
            context.memory[0x00FF] = 0x20;
            context.memory[0x0030] = 0xF0;
            context.memory[0x0031] = 0x12;
            context.memory[0x00FF] = 0x20;
            context.memory[0x10FF] = 0xCD;
            context.memory[0x1000] = 0xAB;
            context.memory[0x1100] = 0xEF;
            context.memory[0x1240] = 0x34;
            context.memory[0x1241] = 0x12;
            return context;
        }

        static Cpu6502Registers MakeRegisters()
        {
            Cpu6502Registers  registers = {};



            registers.pc = kPc;
            registers.x  = 0x10;
            registers.y  = 0x20;
            return registers;
        }

        static std::wstring Describe (const AccessPrediction & prediction)
        {
            std::wstring  text;



            for (const PredictedTouch & touch : prediction.touches)
            {
                text += std::format (L"{:04X}:{} ", touch.address, (int) touch.access);
            }

            return text;
        }



        TEST_METHOD (EveryMode_HandComputed)
        {
            // X=$10, Y=$20 throughout.
            const std::vector<Case> cases =
            {
                { "LDA #$41",        GlobalAddressingMode::Immediate,           false, { 0xA9, 0x41 },       {} },
                { "LDA $80",         GlobalAddressingMode::ZeroPage,            false, { 0xA5, 0x80 },       { { 0x0080, PredictedAccess::Read } } },
                { "STA $F8,X wraps", GlobalAddressingMode::ZeroPageX,           false, { 0x95, 0xF8 },       { { 0x0008, PredictedAccess::Write } } },
                { "LDX $F0,Y wraps", GlobalAddressingMode::ZeroPageY,           false, { 0xB6, 0xF0 },       { { 0x0010, PredictedAccess::Read } } },
                { "INC $1234",       GlobalAddressingMode::Absolute,            false, { 0xEE, 0x34, 0x12 }, { { 0x1234, PredictedAccess::ReadWrite } } },
                { "STA $12F8,X",     GlobalAddressingMode::AbsoluteX,           false, { 0x9D, 0xF8, 0x12 }, { { 0x1308, PredictedAccess::Write } } },
                { "LDA $FFF0,Y",     GlobalAddressingMode::AbsoluteY,           false, { 0xB9, 0xF0, 0xFF }, { { 0x0010, PredictedAccess::Read } } },
                { "LDA ($EE,X)",     GlobalAddressingMode::ZeroPageXIndirect,   false, { 0xA1, 0xEE },       { { 0x00FE, PredictedAccess::Read }, { 0x00FF, PredictedAccess::Read }, { 0x2010, PredictedAccess::Read } } },
                { "STA ($FF),Y",     GlobalAddressingMode::ZeroPageIndirectY,   false, { 0x91, 0xFF },       { { 0x00FF, PredictedAccess::Read }, { 0x0000, PredictedAccess::Read }, { 0x0040, PredictedAccess::Write } } },
                { "ASL A",           GlobalAddressingMode::Accumulator,         false, { 0x0A },             {} },
                { "JMP $1234",       GlobalAddressingMode::JumpAbsolute,        false, { 0x4C, 0x34, 0x12 }, {} },
                { "JMP ($10FF) nmos",GlobalAddressingMode::JumpIndirect,        false, { 0x6C, 0xFF, 0x10 }, { { 0x10FF, PredictedAccess::Read }, { 0x1000, PredictedAccess::Read } } },
                { "BNE",             GlobalAddressingMode::Relative,            false, { 0xD0, 0x10 },       {} },
                { "RTS",             GlobalAddressingMode::SingleByteNoOperand, false, { 0x60 },             {} },
                { "LDA ($30)",       GlobalAddressingMode::ZeroPageIndirect,    true,  { 0xB2, 0x30 },       { { 0x0030, PredictedAccess::Read }, { 0x0031, PredictedAccess::Read }, { 0x12F0, PredictedAccess::Read } } },
                { "JMP ($1230,X)",   GlobalAddressingMode::AbsoluteXIndirect,   true,  { 0x7C, 0x30, 0x12 }, { { 0x1240, PredictedAccess::Read }, { 0x1241, PredictedAccess::Read } } },
                { "BBR0 $80,rel",    GlobalAddressingMode::ZeroPageRelative,    true,  { 0x0F, 0x80, 0x05 }, { { 0x0080, PredictedAccess::Read } } },
                { "JMP ($10FF) cmos",GlobalAddressingMode::JumpIndirectCmos,    true,  { 0x6C, 0xFF, 0x10 }, { { 0x10FF, PredictedAccess::Read }, { 0x1100, PredictedAccess::Read } } },
            };

            TestCpu               nmos;
            TestCpu65C02          cmos;
            std::set<int>         modesCovered;
            size_t                checked = 0;



            nmos.InitForTest();
            cmos.InitForTest();

            for (const Case & c : cases)
            {
                MockExpressionContext  context   = MakeContext();
                AccessPrediction       prediction;
                const Microcode      * table     = c.isCmos ? cmos.GetInstructionSet() : nmos.GetInstructionSet();
                std::string            narrow (c.name);
                std::wstring           where (narrow.begin(), narrow.end());
                HRESULT                hr        = S_OK;



                for (size_t i = 0; i < c.bytes.size(); ++i)
                {
                    context.memory[kPc + i] = c.bytes[i];
                }

                Assert::AreEqual ((int) c.mode, (int) table[c.bytes[0]].globalAddressingMode, (where + L": case targets the wrong mode").c_str());

                hr = EffectiveAddress::Predict (table, kPc, MakeRegisters(), context, prediction);
                Assert::AreEqual (S_OK, hr, where.c_str());
                Assert::AreEqual (c.expected.size(), prediction.touches.size(), (where + L": " + Describe (prediction)).c_str());

                for (size_t i = 0; i < c.expected.size(); ++i)
                {
                    Assert::AreEqual (c.expected[i].address,      prediction.touches[i].address,      (where + L": " + Describe (prediction)).c_str());
                    Assert::AreEqual ((int) c.expected[i].access, (int) prediction.touches[i].access, where.c_str());
                }

                modesCovered.insert ((int) c.mode);
                ++checked;
            }

            Assert::AreEqual ((size_t) GlobalAddressingMode::__Count, modesCovered.size(), L"every addressing mode has a case");
            Assert::AreEqual (cases.size(), checked);
        }



        TEST_METHOD (Sweep_EveryLegalOpcode_Predicts)
        {
            TestCpu       nmos;
            TestCpu65C02  cmos;
            size_t        checked = 0;



            nmos.InitForTest();
            cmos.InitForTest();

            for (const Microcode * table : { nmos.GetInstructionSet(), cmos.GetInstructionSet() })
            {
                for (int opcode = 0; opcode <= 0xFF; ++opcode)
                {
                    MockExpressionContext  context = MakeContext();
                    AccessPrediction       prediction;
                    HRESULT                hr      = S_OK;



                    context.memory[kPc]     = (Byte) opcode;
                    context.memory[kPc + 1] = 0x34;
                    context.memory[kPc + 2] = 0x12;

                    hr = EffectiveAddress::Predict (table, kPc, MakeRegisters(), context, prediction);

                    if (!table[opcode].isLegal)
                    {
                        Assert::IsTrue (FAILED (hr));
                        continue;
                    }

                    Assert::AreEqual (S_OK, hr, std::format (L"opcode {:02X}", opcode).c_str());
                    ++checked;
                }
            }

            Assert::IsTrue (checked > 256);
        }



        TEST_METHOD (UnreadableOperand_Fails)
        {
            TestCpu                nmos;
            MockExpressionContext  context;
            AccessPrediction       prediction;
            HRESULT                hr = S_OK;



            nmos.InitForTest();
            context.memory[0xBFFF] = 0xAD;               // LDA abs, operand in $C000
            hr = EffectiveAddress::Predict (nmos.GetInstructionSet(), 0xBFFF, MakeRegisters(), context, prediction);
            Assert::IsTrue (FAILED (hr));
            Assert::IsTrue (prediction.touches.empty());
        }
    };
}
