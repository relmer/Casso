#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





// Regression tests for bugs found during Harte and Dormann test runs.
// Each test targets a specific bug category to prevent reintroduction.
namespace RegressionTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ZeroPageWrappingTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ZeroPageWrappingTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZeroPageX_WrapsAt256
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZeroPageX_WrapsAt256)
        {
            // Bug: effectiveAddress = location + X didn't wrap with & 0xFF.
            // ZP,X with base=$80, X=$90 should access $10 (wraps), not $110.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x90;
            cpu.Poke (0x10, 0xAB);                          // Wrapped address
            cpu.Poke (0x0110, 0xFF);                         // Unwrapped (wrong) address
            cpu.WriteBytes (0x8000, { 0xB5, 0x80 });         // LDA $80,X



            cpu.Step ();

            Assert::AreEqual ((Byte) 0xAB, cpu.RegA (),
                L"LDA $80,X with X=$90 should read from ZP $10, not $0110");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STA_ZeroPageX_WrapsAt256
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STA_ZeroPageX_WrapsAt256)
        {
            // STA $80,X with X=$90 should store to $10, not $110.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x90;
            cpu.RegA () = 0x42;
            cpu.WriteBytes (0x8000, { 0x95, 0x80 });         // STA $80,X



            cpu.Step ();

            Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x10),
                L"STA $80,X with X=$90 should store to ZP $10");
            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x0110),
                L"Address $0110 should be untouched");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDX_ZeroPageY_WrapsAt256
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDX_ZeroPageY_WrapsAt256)
        {
            // Bug: ZP,Y wrapping. LDX $80,Y with Y=$90 should read from $10.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegY () = 0x90;
            cpu.Poke (0x10, 0xCD);
            cpu.Poke (0x0110, 0xFF);
            cpu.WriteBytes (0x8000, { 0xB6, 0x80 });         // LDX $80,Y



            cpu.Step ();

            Assert::AreEqual ((Byte) 0xCD, cpu.RegX (),
                L"LDX $80,Y with Y=$90 should read from ZP $10, not $0110");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STX_ZeroPageY_WrapsAt256
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STX_ZeroPageY_WrapsAt256)
        {
            // STX $FF,Y with Y=$01 should store to $00, not $100.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegY () = 0x01;
            cpu.RegX () = 0x77;
            cpu.WriteBytes (0x8000, { 0x96, 0xFF });         // STX $FF,Y



            cpu.Step ();

            Assert::AreEqual ((Byte) 0x77, cpu.Peek (0x00),
                L"STX $FF,Y with Y=$01 should store to ZP $00");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IndirectXWrappingTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IndirectXWrappingTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_IndirectX_ZeroPagePointerWraps
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_IndirectX_ZeroPagePointerWraps)
        {
            // Bug: ReadWord(location + X) didn't wrap in ZP.
            // ($F0,X) with X=$10 should read pointer from $00/$01, not $100/$101.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x10;
            cpu.Poke (0x00, 0x34);                           // Low byte of pointer (at $00)
            cpu.Poke (0x01, 0x12);                           // High byte of pointer (at $01)
            cpu.Poke (0x1234, 0xEE);                         // Target value
            cpu.WriteBytes (0x8000, { 0xA1, 0xF0 });         // LDA ($F0,X)



            cpu.Step ();

            Assert::AreEqual ((Byte) 0xEE, cpu.RegA (),
                L"LDA ($F0,X) with X=$10 should read pointer from $00/$01");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_IndirectX_PointerStraddles_FF_00
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_IndirectX_PointerStraddles_FF_00)
        {
            // Pointer read from $FF/$00 (wraps across ZP boundary).
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x00;
            cpu.Poke (0xFF, 0x78);                           // Low byte of pointer at $FF
            cpu.Poke (0x00, 0x56);                           // High byte wraps to $00
            cpu.Poke (0x5678, 0xDD);                         // Target value
            cpu.WriteBytes (0x8000, { 0xA1, 0xFF });         // LDA ($FF,X)



            cpu.Step ();

            Assert::AreEqual ((Byte) 0xDD, cpu.RegA (),
                L"LDA ($FF,X) should read pointer from $FF/$00 (ZP wrap)");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IndirectYWrappingTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IndirectYWrappingTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_IndirectY_PointerAtFF_WrapsHighByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_IndirectY_PointerAtFF_WrapsHighByte)
        {
            // Bug: ReadWord($FF) read high byte from $100 instead of $00.
            // ($FF),Y should read low from $FF, high from $00.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegY () = 0x03;
            cpu.Poke (0xFF, 0x00);                           // Low byte of pointer
            cpu.Poke (0x00, 0x20);                           // High byte wraps to $00
            cpu.Poke (0x2003, 0xBB);                         // Target = base ($2000) + Y ($03)
            cpu.WriteBytes (0x8000, { 0xB1, 0xFF });         // LDA ($FF),Y



            cpu.Step ();

            Assert::AreEqual ((Byte) 0xBB, cpu.RegA (),
                L"LDA ($FF),Y should read pointer from $FF/$00 (ZP wrap)");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  JmpIndirectPageBoundaryTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (JmpIndirectPageBoundaryTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JMP_Indirect_PageBoundary_WrapsWithinPage
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JMP_Indirect_PageBoundary_WrapsWithinPage)
        {
            // Bug: JMP ($10FF) should read low from $10FF, high from $1000
            // (NMOS page wrap), not from $1100.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Poke (0x10FF, 0x76);                         // Low byte of target
            cpu.Poke (0x1000, 0x54);                         // High byte wraps to page start
            cpu.Poke (0x1100, 0xFF);                         // Wrong high byte (no wrap)
            cpu.WriteBytes (0x8000, { 0x6C, 0xFF, 0x10 });   // JMP ($10FF)



            cpu.Step ();

            Assert::AreEqual ((Word) 0x5476, cpu.RegPC (),
                L"JMP ($10FF) should wrap high byte read to $1000, not $1100");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JMP_Indirect_NotOnBoundary_ReadsNormally
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JMP_Indirect_NotOnBoundary_ReadsNormally)
        {
            // Non-boundary case: JMP ($1080) reads from $1080/$1081 normally.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Poke (0x1080, 0x00);
            cpu.Poke (0x1081, 0x30);
            cpu.WriteBytes (0x8000, { 0x6C, 0x80, 0x10 });   // JMP ($1080)



            cpu.Step ();

            Assert::AreEqual ((Word) 0x3000, cpu.RegPC ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  JsrStackOperandOverlapTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (JsrStackOperandOverlapTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JSR_StackOverlapsOperand_MatchesHardware
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JSR_StackOverlapsOperand_MatchesHardware)
        {
            // Bug: JSR reads low byte, pushes return address, then re-reads
            // high byte. If SP points into the operand bytes, the push
            // overwrites the high byte before re-read.
            //
            // Setup: PC=$017B, SP=$7D, bytes at $017B: 20 55 13
            //   T1: read low byte ($55) at $017C, PC=$017D
            //   T3: push PCH ($01) to $017D (overwrites $13)
            //   T4: push PCL ($7D) to $017C
            //   T5: re-read high byte from $017D = $01 (overwritten)
            // Result: PC = $0155 (hardware-accurate), not $1355
            TestCpu cpu;
            cpu.InitForTest (0x017B);
            cpu.RegSP () = 0x7D;
            cpu.WriteBytes (0x017B, { 0x20, 0x55, 0x13 });   // JSR $1355



            cpu.Step ();

            Assert::AreEqual ((Word) 0x0155, cpu.RegPC (),
                L"JSR stack-operand overlap: hardware re-reads high byte after push");
            Assert::AreEqual ((Byte) 0x7B, cpu.RegSP (),
                L"SP should be decremented by 2");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  JSR_NoOverlap_JumpsNormally
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (JSR_NoOverlap_JumpsNormally)
        {
            // Normal case: stack doesn't overlap operand.
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, { 0x20, 0x55, 0x13 });   // JSR $1355



            cpu.Step ();

            Assert::AreEqual ((Word) 0x1355, cpu.RegPC ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AdcBcdNFlagTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AdcBcdNFlagTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_BCD_NFlag_FromIntermediate_NotFinal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_BCD_NFlag_FromIntermediate_NotFinal)
        {
            // Bug: N flag was set from final BCD result instead of intermediate
            // (after low-nibble correction, before high-nibble correction).
            //
            // A=$56 + $56 + C=0 in decimal mode:
            //   Binary sum = $AC
            //   Low nibble: 6+6=12 > 9, add 6: intermediate = $B2 (bit 7 = 1)
            //   High nibble: $B > 9, add $60: final = $12
            // N should be 1 (from intermediate $B2), not 0 (from final $12).
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x18,               // CLC
                0xA9, 0x56,         // LDA #$56
                0x69, 0x56          // ADC #$56
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x12, cpu.RegA (),
                L"BCD result: $56 + $56 = $12");
            Assert::IsTrue ((bool) cpu.Status ().flags.negative,
                L"N flag should be 1 (from intermediate $B2, not final $12)");
            Assert::IsTrue ((bool) cpu.Status ().flags.carry,
                L"Carry should be set (BCD result > 99)");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_BCD_ZFlag_FromBinaryResult
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ADC_BCD_ZFlag_FromBinaryResult)
        {
            // Z flag is from the binary sum, not the BCD-corrected result.
            // A=$50 + $50 + C=0: binary=$A0 (Z=0), BCD=$00 (Z would be 1 if wrong).
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x18,               // CLC
                0xA9, 0x50,         // LDA #$50
                0x69, 0x50          // ADC #$50
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA (),
                L"BCD result: $50 + $50 = $00 (with carry)");
            Assert::IsFalse ((bool) cpu.Status ().flags.zero,
                L"Z flag should be 0 (binary $A0 != 0), not from BCD $00");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SbcBcdFlagTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SbcBcdFlagTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_BCD_NFlag_FromBinarySubtraction
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_BCD_NFlag_FromBinarySubtraction)
        {
            // Bug: N flag was set from BCD result instead of binary subtraction.
            //
            // A=$00, M=$80, C=1 (no borrow):
            //   Binary: $00 - $80 = $80 (N=1)
            //   BCD: 00 - 80 = 20 with borrow, A=$20 (bit 7 = 0)
            // N should be 1 (from binary $80), not 0 (from BCD $20).
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x38,               // SEC
                0xA9, 0x00,         // LDA #$00
                0xE9, 0x80          // SBC #$80
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x20, cpu.RegA (),
                L"BCD result: $00 - $80 = $20 (with borrow)");
            Assert::IsTrue ((bool) cpu.Status ().flags.negative,
                L"N flag should be 1 (from binary $80), not from BCD $20");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_BCD_ZFlag_FromBinarySubtraction
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SBC_BCD_ZFlag_FromBinarySubtraction)
        {
            // A=$00, M=$01, C=1 (no borrow):
            //   Binary: $00 - $01 = $FF (Z=0, N=1)
            //   BCD: 00 - 01 = 99 with borrow
            // Z from binary = 0 (not from BCD $99 which also gives Z=0, so
            // also test the N flag to confirm binary sourcing).
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x38,               // SEC
                0xA9, 0x00,         // LDA #$00
                0xE9, 0x01          // SBC #$01
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x99, cpu.RegA (),
                L"BCD result: $00 - $01 = $99 (with borrow)");
            Assert::IsFalse ((bool) cpu.Status ().flags.zero,
                L"Z flag should be 0 (binary $FF != 0)");
            Assert::IsTrue ((bool) cpu.Status ().flags.negative,
                L"N flag should be 1 (from binary $FF, bit 7 set)");
            Assert::IsFalse ((bool) cpu.Status ().flags.carry,
                L"Carry should be 0 (borrow occurred)");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReadWordWrappingTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReadWordWrappingTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ReadWord_AtFFFF_WrapsHighByteTo0000
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ReadWord_AtFFFF_WrapsHighByteTo0000)
        {
            // Bug: ReadWord($FFFF) read memory[$10000] (OOB) instead of
            // wrapping to memory[$0000].
            //
            // Place LDA abs at $FFFE so the 16-bit operand straddles $FFFF/$0000.
            // Operand low byte at $FFFF, high byte wraps to $0000.
            TestCpu cpu;
            cpu.InitForTest (0xFFFE);
            cpu.Poke (0xFFFE, 0xAD);                         // LDA abs opcode
            cpu.Poke (0xFFFF, 0x10);                         // Low byte of target address
            cpu.Poke (0x0000, 0x20);                         // High byte wraps to $0000
            cpu.Poke (0x2010, 0x42);                         // Value at target address



            cpu.Step ();

            Assert::AreEqual ((Byte) 0x42, cpu.RegA (),
                L"ReadWord at $FFFF should wrap high byte read to $0000");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetVariableTemporalTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SetVariableTemporalTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BuildAssembler
        //
        ////////////////////////////////////////////////////////////////////////////////

        static Assembler BuildAssembler ()
        {
            TestCpu cpu;
            cpu.InitForTest ();
            return Assembler (cpu.GetInstructionSet ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Set_ThreeIncrements_TemporalOrdering
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Set_ThreeIncrements_TemporalOrdering)
        {
            // Bug: Pass 2 re-evaluated Set expressions using final symbol
            // table value, losing temporal ordering. Matches Dormann next_test
            // pattern with 3+ increments.
            Assembler asm6502 = BuildAssembler ();

            auto result = asm6502.Assemble (
                "    .org $1000\n"
                "counter set 0\n"
                "    lda #counter\n"       // should be $00
                "counter set counter + 1\n"
                "    ldx #counter\n"       // should be $01
                "counter set counter + 1\n"
                "    ldy #counter\n"       // should be $02
            );



            Assert::IsTrue (result.success, L"Assembly should succeed");

            // LDA #$00
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0], L"LDA opcode");
            Assert::AreEqual ((Byte) 0x00, result.bytes[1], L"First use: counter=0");

            // LDX #$01
            Assert::AreEqual ((Byte) 0xA2, result.bytes[2], L"LDX opcode");
            Assert::AreEqual ((Byte) 0x01, result.bytes[3], L"Second use: counter=1");

            // LDY #$02
            Assert::AreEqual ((Byte) 0xA0, result.bytes[4], L"LDY opcode");
            Assert::AreEqual ((Byte) 0x02, result.bytes[5], L"Third use: counter=2");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Set_FourIncrements_MatchesDormannPattern
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Set_FourIncrements_MatchesDormannPattern)
        {
            // Extended Dormann pattern: 4 temporal values.
            Assembler asm6502 = BuildAssembler ();

            auto result = asm6502.Assemble (
                "    .org $1000\n"
                "nt set 0\n"
                "    lda #nt\n"
                "nt set nt + 1\n"
                "    lda #nt\n"
                "nt set nt + 1\n"
                "    lda #nt\n"
                "nt set nt + 1\n"
                "    lda #nt\n"
            );



            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x00, result.bytes[1], L"nt=0");
            Assert::AreEqual ((Byte) 0x01, result.bytes[3], L"nt=1");
            Assert::AreEqual ((Byte) 0x02, result.bytes[5], L"nt=2");
            Assert::AreEqual ((Byte) 0x03, result.bytes[7], L"nt=3");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetVariableInMacroTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SetVariableInMacroTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BuildAssembler
        //
        ////////////////////////////////////////////////////////////////////////////////

        static Assembler BuildAssembler ()
        {
            TestCpu cpu;
            cpu.InitForTest ();
            return Assembler (cpu.GetInstructionSet ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Set_IncrementedInMacroBody
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Set_IncrementedInMacroBody)
        {
            // Bug: Set variable incremented inside a macro body — the
            // Dormann-specific pattern where each invocation should see
            // successive values.
            Assembler asm6502 = BuildAssembler ();

            auto result = asm6502.Assemble (
                "    .org $1000\n"
                "counter set 0\n"
                "inc_test macro\n"
                "    lda #counter\n"
                "counter set counter + 1\n"
                "    endm\n"
                "    inc_test\n"           // lda #0
                "    inc_test\n"           // lda #1
                "    inc_test\n"           // lda #2
            );



            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 6, result.bytes.size (),
                L"Three LDA immediate = 6 bytes");

            // Each macro invocation sees the current counter value
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[1], L"First invocation: counter=0");
            Assert::AreEqual ((Byte) 0xA9, result.bytes[2]);
            Assert::AreEqual ((Byte) 0x01, result.bytes[3], L"Second invocation: counter=1");
            Assert::AreEqual ((Byte) 0xA9, result.bytes[4]);
            Assert::AreEqual ((Byte) 0x02, result.bytes[5], L"Third invocation: counter=2");
        }
    };
}
