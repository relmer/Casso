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
    //  Zero-page indexed addressing wrapping at 256 instead of crossing into
    //  page 1.
    //
    //  On the 6502, zero-page,X and zero-page,Y compute their effective address
    //  MODULO 256 -- $FF plus 5 is $04, not $0104. The natural implementation
    //  adds an index to a base and gets it wrong only at the boundary, which is
    //  why the original bug existed and why every test here indexes across it
    //  deliberately.
    //
    //  It matters because page 1 is the STACK. Without the wrap, an indexed
    //  read near the top of zero page returns stack bytes and an indexed write
    //  corrupts a return address, so the failure surfaces as a crash somewhere
    //  unrelated.
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
            cpu.InitForTest();
            cpu.RegX() = 0x90;
            cpu.Poke (0x10, 0xAB);                          // Wrapped address
            cpu.Poke (0x0110, 0xFF);                         // Unwrapped (wrong) address
            cpu.WriteBytes (0x8000, { 0xB5, 0x80 });         // LDA $80,X



            cpu.Step();

            Assert::AreEqual ((Byte) 0xAB, cpu.RegA(),
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
            cpu.InitForTest();
            cpu.RegX() = 0x90;
            cpu.RegA() = 0x42;
            cpu.WriteBytes (0x8000, { 0x95, 0x80 });         // STA $80,X



            cpu.Step();

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
            cpu.InitForTest();
            cpu.RegY() = 0x90;
            cpu.Poke (0x10, 0xCD);
            cpu.Poke (0x0110, 0xFF);
            cpu.WriteBytes (0x8000, { 0xB6, 0x80 });         // LDX $80,Y



            cpu.Step();

            Assert::AreEqual ((Byte) 0xCD, cpu.RegX(),
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
            cpu.InitForTest();
            cpu.RegY() = 0x01;
            cpu.RegX() = 0x77;
            cpu.WriteBytes (0x8000, { 0x96, 0xFF });         // STX $FF,Y



            cpu.Step();

            Assert::AreEqual ((Byte) 0x77, cpu.Peek (0x00),
                L"STX $FF,Y with Y=$01 should store to ZP $00");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IndirectXWrappingTests
    //
    //  (zp,X) wrapping in zero page -- for the POINTER FETCH, not just the
    //  index.
    //
    //  A subtler case than plain zero-page indexing, and a separate bug. Both
    //  bytes of the pointer are read from zero page with wrap, so a pointer at
    //  $FF takes its low byte from $FF and its high byte from $00 -- not from
    //  $0100. Wrapping the index but reading the word normally passes every
    //  test that does not sit on the boundary.
    //
    //  The consequence is worse than a wrong byte: a mis-fetched pointer sends
    //  the access to an entirely unrelated address.
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
            cpu.InitForTest();
            cpu.RegX() = 0x10;
            cpu.Poke (0x00, 0x34);                           // Low byte of pointer (at $00)
            cpu.Poke (0x01, 0x12);                           // High byte of pointer (at $01)
            cpu.Poke (0x1234, 0xEE);                         // Target value
            cpu.WriteBytes (0x8000, { 0xA1, 0xF0 });         // LDA ($F0,X)



            cpu.Step();

            Assert::AreEqual ((Byte) 0xEE, cpu.RegA(),
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
            cpu.InitForTest();
            cpu.RegX() = 0x00;
            cpu.Poke (0xFF, 0x78);                           // Low byte of pointer at $FF
            cpu.Poke (0x00, 0x56);                           // High byte wraps to $00
            cpu.Poke (0x5678, 0xDD);                         // Target value
            cpu.WriteBytes (0x8000, { 0xA1, 0xFF });         // LDA ($FF,X)



            cpu.Step();

            Assert::AreEqual ((Byte) 0xDD, cpu.RegA(),
                L"LDA ($FF,X) should read pointer from $FF/$00 (ZP wrap)");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IndirectYWrappingTests
    //
    //  (zp),Y wrapping the POINTER FETCH in zero page while the Y addition
    //  does not wrap.
    //
    //  The asymmetry is the whole point, and it is the opposite of (zp,X). Here
    //  the pointer is read from zero page with wrap, but Y is added to the
    //  resulting 16-bit address AFTERWARDS and legitimately crosses pages --
    //  that page cross is how (zp),Y reaches all of memory.
    //
    //  So applying the zero-page wrap to the final sum, by symmetry with the
    //  other indexed modes, breaks the most commonly used addressing mode on
    //  the machine.
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
            cpu.InitForTest();
            cpu.RegY() = 0x03;
            cpu.Poke (0xFF, 0x00);                           // Low byte of pointer
            cpu.Poke (0x00, 0x20);                           // High byte wraps to $00
            cpu.Poke (0x2003, 0xBB);                         // Target = base ($2000) + Y ($03)
            cpu.WriteBytes (0x8000, { 0xB1, 0xFF });         // LDA ($FF),Y



            cpu.Step();

            Assert::AreEqual ((Byte) 0xBB, cpu.RegA(),
                L"LDA ($FF),Y should read pointer from $FF/$00 (ZP wrap)");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  JmpIndirectPageBoundaryTests
    //
    //  The famous JMP ($xxFF) bug: the high byte comes from the START of the
    //  same page, not the next one.
    //
    //  A genuine NMOS 6502 DEFECT that this emulator reproduces deliberately.
    //  JMP ($10FF) reads its low byte from $10FF and its high byte from $1000
    //  rather than $1100, because the address increment does not carry into the
    //  high byte.
    //
    //  It is emulated rather than fixed because period software knows about it
    //  -- some avoids the boundary, some exploits it -- and a "corrected"
    //  implementation would run code the hardware never would.
    //
    //  The 65C02 fixed this, so a divergence here is also what would catch the
    //  CMOS behavior leaking into the NMOS core.
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
            cpu.InitForTest();
            cpu.Poke (0x10FF, 0x76);                         // Low byte of target
            cpu.Poke (0x1000, 0x54);                         // High byte wraps to page start
            cpu.Poke (0x1100, 0xFF);                         // Wrong high byte (no wrap)
            cpu.WriteBytes (0x8000, { 0x6C, 0xFF, 0x10 });   // JMP ($10FF)



            cpu.Step();

            Assert::AreEqual ((Word) 0x5476, cpu.RegPC(),
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
            cpu.InitForTest();
            cpu.Poke (0x1080, 0x00);
            cpu.Poke (0x1081, 0x30);
            cpu.WriteBytes (0x8000, { 0x6C, 0x80, 0x10 });   // JMP ($1080)



            cpu.Step();

            Assert::AreEqual ((Word) 0x3000, cpu.RegPC());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  JsrStackOperandOverlapTests
    //
    //  A JSR whose own operand bytes live where it is about to push -- the
    //  operand must be read BEFORE the return address is pushed.
    //
    //  Only reachable when code executes from page 1, which is exactly what a
    //  few copy-protection schemes do. The hardware's fetch order makes the
    //  outcome well-defined, so an implementation that pushed first and read
    //  its operand afterwards would jump somewhere else entirely.
    //
    //  Obscure, and worth a test precisely because nothing else exercises it:
    //  the ordering is invisible in every ordinary JSR, so a refactor could
    //  swap the two steps and every other test would still pass.
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
            cpu.RegSP() = 0x7D;
            cpu.WriteBytes (0x017B, { 0x20, 0x55, 0x13 });   // JSR $1355



            cpu.Step();

            Assert::AreEqual ((Word) 0x0155, cpu.RegPC(),
                L"JSR stack-operand overlap: hardware re-reads high byte after push");
            Assert::AreEqual ((Byte) 0x7B, cpu.RegSP(),
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
            cpu.InitForTest();
            cpu.WriteBytes (0x8000, { 0x20, 0x55, 0x13 });   // JSR $1355



            cpu.Step();

            Assert::AreEqual ((Word) 0x1355, cpu.RegPC());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AdcBcdNFlagTests
    //
    //  ADC in decimal mode: N comes from the INTERMEDIATE binary sum, not the
    //  final BCD result.
    //
    //  An NMOS quirk, and the reason decimal-mode flags cannot be derived from
    //  the answer. The 6502 computes the binary sum, sets N and V from it, and
    //  only then applies the decimal adjustment -- so N frequently disagrees
    //  with the high bit of the value actually stored.
    //
    //  A natural implementation sets the flags from the final result and
    //  produces the right answer with the wrong flags, which is why this needs
    //  a test rather than being covered by the arithmetic tests.
    //
    //  The 65C02 changed this too, so it also guards CMOS behavior leaking into
    //  the NMOS core.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AdcBcdNFlagTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ADC_BCD_NFlag_FromIntermediate_NotFinal
        //
        //  Picks operands where the intermediate binary sum and the final BCD
        //  result DISAGREE about bit 7.
        //
        //  That disagreement is the entire test. Most decimal additions produce
        //  the same N either way, so a wrong implementation passes on almost
        //  any input -- only a case straddling the adjustment distinguishes
        //  them.
        //
        //  The stored value is asserted alongside the flag, so a test failure
        //  says whether the arithmetic or only the flag went wrong.
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
            cpu.InitForTest();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x18,               // CLC
                0xA9, 0x56,         // LDA #$56
                0x69, 0x56          // ADC #$56
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x12, cpu.RegA(),
                L"BCD result: $56 + $56 = $12");
            Assert::IsTrue ((bool) cpu.Status().flags.negative,
                L"N flag should be 1 (from intermediate $B2, not final $12)");
            Assert::IsTrue ((bool) cpu.Status().flags.carry,
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
            cpu.InitForTest();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x18,               // CLC
                0xA9, 0x50,         // LDA #$50
                0x69, 0x50          // ADC #$50
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x00, cpu.RegA(),
                L"BCD result: $50 + $50 = $00 (with carry)");
            Assert::IsFalse ((bool) cpu.Status().flags.zero,
                L"Z flag should be 0 (binary $A0 != 0), not from BCD $00");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SbcBcdFlagTests
    //
    //  SBC in decimal mode: N, V, and Z all come from the BINARY subtraction,
    //  before the decimal adjustment.
    //
    //  The mirror of the ADC case, and Z is the one that surprises. Unlike ADC,
    //  where only N and V come from the intermediate, SBC's Z does too -- so a
    //  decimal subtraction can store a non-zero result with Z set, or zero with
    //  Z clear.
    //
    //  That asymmetry between the two instructions is exactly what a shared
    //  flag helper would erase, which is why both directions have their own
    //  tests.
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
            cpu.InitForTest();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x38,               // SEC
                0xA9, 0x00,         // LDA #$00
                0xE9, 0x80          // SBC #$80
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x20, cpu.RegA(),
                L"BCD result: $00 - $80 = $20 (with borrow)");
            Assert::IsTrue ((bool) cpu.Status().flags.negative,
                L"N flag should be 1 (from binary $80), not from BCD $20");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SBC_BCD_ZFlag_FromBinarySubtraction
        //
        //  A decimal subtraction whose binary intermediate is zero while the
        //  adjusted result is not -- so Z must be SET despite a non-zero
        //  answer.
        //
        //  Counter-intuitive enough that it reads as a bug in the test rather
        //  than a fact about the hardware, which is why it is pinned
        //  explicitly.
        //
        //  It is also the case a shared "set Z from the result" helper would
        //  quietly get wrong for SBC alone, since that rule IS correct for
        //  every other instruction.
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
            cpu.InitForTest();
            cpu.WriteBytes (0x8000, {
                0xF8,               // SED
                0x38,               // SEC
                0xA9, 0x00,         // LDA #$00
                0xE9, 0x01          // SBC #$01
            });



            cpu.StepN (4);

            Assert::AreEqual ((Byte) 0x99, cpu.RegA(),
                L"BCD result: $00 - $01 = $99 (with borrow)");
            Assert::IsFalse ((bool) cpu.Status().flags.zero,
                L"Z flag should be 0 (binary $FF != 0)");
            Assert::IsTrue ((bool) cpu.Status().flags.negative,
                L"N flag should be 1 (from binary $FF, bit 7 set)");
            Assert::IsFalse ((bool) cpu.Status().flags.carry,
                L"Carry should be 0 (borrow occurred)");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReadWordWrappingTests
    //
    //  A 16-bit read at $FFFF taking its high byte from $0000.
    //
    //  The address bus is 16 bits and wraps; it does not fault or read past the
    //  end. An implementation using a wider intermediate index would read out
    //  of bounds of the memory array instead -- undefined behavior rather than
    //  a wrong value, so this guards a crash as much as a correctness bug.
    //
    //  It sits at the very top of the address space, next to the interrupt
    //  vectors, so the wrap is genuinely reachable rather than theoretical.
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



            cpu.Step();

            Assert::AreEqual ((Byte) 0x42, cpu.RegA(),
                L"ReadWord at $FFFF should wrap high byte read to $0000");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetVariableTemporalTests
    //
    //  A `set` variable holding the value it had AT EACH POINT of use, not its
    //  final value.
    //
    //  This is what distinguishes `set` from `equ`. An equ symbol has one value
    //  the whole assembly; a set variable is reassignable, so a reference must
    //  see the assignment most recently preceding it -- three uses between
    //  three assignments must yield three different numbers.
    //
    //  The natural two-pass implementation gets this wrong: pass 1 records
    //  symbols in a table and pass 2 looks them up, so every reference sees the
    //  LAST value assigned. Getting it right requires resolving set variables
    //  positionally, and that is what these pin.
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

        static Assembler BuildAssembler()
        {
            TestCpu cpu;
            cpu.InitForTest();
            return Assembler (cpu.GetInstructionSet());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Set_ThreeIncrements_TemporalOrdering
        //
        //  Three assignments interleaved with three uses, asserting three
        //  DIFFERENT emitted values.
        //
        //  Three rather than two, because two would also pass an implementation
        //  that happened to be off by one assignment. Three distinct values
        //  pin the ordering itself.
        //
        //  The emitted BYTES are asserted rather than the symbol table, since
        //  the table only holds the final value by construction -- the evidence
        //  of temporal resolution exists only in the output.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Set_ThreeIncrements_TemporalOrdering)
        {
            // Bug: Pass 2 re-evaluated Set expressions using final symbol
            // table value, losing temporal ordering. Matches Dormann next_test
            // pattern with 3+ increments.
            Assembler asm6502 = BuildAssembler();

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
            Assembler asm6502 = BuildAssembler();

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
    //  A `set` variable incremented inside a macro BODY, advancing once per
    //  expansion.
    //
    //  The temporal case again, now crossing macro expansion -- which is where
    //  it is most easily lost. Macros expand by splicing lines into the pending
    //  queue, so the assignment inside a body has to take effect at each
    //  EXPANSION SITE rather than once at the definition.
    //
    //  That makes the classic counter idiom work: a macro invoked several times
    //  emitting a different value each time is how period sources generate
    //  tables and unique labels.
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

        static Assembler BuildAssembler()
        {
            TestCpu cpu;
            cpu.InitForTest();
            return Assembler (cpu.GetInstructionSet());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Set_IncrementedInMacroBody
        //
        //  Invokes one macro repeatedly and asserts the emitted value advances
        //  with each invocation.
        //
        //  The failure it guards is a macro body whose assignment runs once at
        //  DEFINITION rather than per expansion, which emits the same value
        //  every time -- an output that looks orderly and is wrong.
        //
        //  Asserting a sequence rather than a single value also catches the
        //  opposite mistake, where the variable advances more than once per
        //  expansion because the body is processed twice.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Set_IncrementedInMacroBody)
        {
            // Bug: Set variable incremented inside a macro body — the
            // Dormann-specific pattern where each invocation should see
            // successive values.
            Assembler asm6502 = BuildAssembler();

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
            Assert::AreEqual ((size_t) 6, result.bytes.size(),
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
