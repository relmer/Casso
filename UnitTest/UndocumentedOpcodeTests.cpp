#include "Pch.h"

#include "TestHelpers.h"
#include "OpcodeTable.h"




using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace CpuOperationTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndocumentedSemanticsTests
    //
    //  Representative per-operation checks. The Tom Harte SingleStepTests
    //  (HarteTestRunner) exhaustively validate every vector, but those vectors
    //  are generated on demand and not committed; these committed tests document
    //  intent and guard the result + flag behavior of each combined opcode.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UndocumentedSemanticsTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Sax_StoresAAndX_NoFlags
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Sax_StoresAAndX_NoFlags)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0xF0;
            cpu.RegX () = 0x3C;
            cpu.WriteBytes (0x8000, { 0x87, 0x10 });    // SAX $10
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x30, cpu.Peek (0x10));    // 0xF0 & 0x3C
            Assert::IsFalse (cpu.Status ().flags.zero);
            Assert::IsFalse (cpu.Status ().flags.negative);
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Lax_LoadsBothAAndX_SetsNZ
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Lax_LoadsBothAAndX_SetsNZ)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, { 0xA7, 0x10 });    // LAX $10
            cpu.WriteBytes (0x10,   { 0x80 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x80, cpu.RegA ());
            Assert::AreEqual ((Byte) 0x80, cpu.RegX ());
            Assert::IsTrue  (cpu.Status ().flags.negative);
            Assert::IsFalse (cpu.Status ().flags.zero);
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Slo_ShiftsMemoryThenOrsIntoA
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Slo_ShiftsMemoryThenOrsIntoA)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0x01;
            cpu.WriteBytes (0x8000, { 0x07, 0x10 });    // SLO $10
            cpu.WriteBytes (0x10,   { 0x40 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x80, cpu.Peek (0x10));    // ASL $40
            Assert::AreEqual ((Byte) 0x81, cpu.RegA ());        // 0x01 | 0x80
            Assert::IsFalse (cpu.Status ().flags.carry);        // bit 7 of $40 was 0
            Assert::IsTrue  (cpu.Status ().flags.negative);
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Slo_ShiftCarryOutSurvivesOra
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Slo_ShiftCarryOutSurvivesOra)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0x01;
            cpu.WriteBytes (0x8000, { 0x07, 0x10 });
            cpu.WriteBytes (0x10,   { 0x80 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x10));    // ASL $80
            Assert::AreEqual ((Byte) 0x01, cpu.RegA ());        // 0x01 | 0x00
            Assert::IsTrue (cpu.Status ().flags.carry);         // bit 7 of $80 was 1
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Rla_RotatesMemoryThroughCarryThenAnds
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Rla_RotatesMemoryThroughCarryThenAnds)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0xFF;
            cpu.Status ().flags.carry = 1;
            cpu.WriteBytes (0x8000, { 0x27, 0x10 });    // RLA $10
            cpu.WriteBytes (0x10,   { 0x80 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x01, cpu.Peek (0x10));    // ROL $80 with carry in
            Assert::AreEqual ((Byte) 0x01, cpu.RegA ());        // 0xFF & 0x01
            Assert::IsTrue (cpu.Status ().flags.carry);         // bit 7 of $80 was 1
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Sre_ShiftsMemoryRightThenXors
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Sre_ShiftsMemoryRightThenXors)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0xFF;
            cpu.WriteBytes (0x8000, { 0x47, 0x10 });    // SRE $10
            cpu.WriteBytes (0x10,   { 0x03 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x01, cpu.Peek (0x10));    // LSR $03
            Assert::AreEqual ((Byte) 0xFE, cpu.RegA ());        // 0xFF ^ 0x01
            Assert::IsTrue (cpu.Status ().flags.carry);         // bit 0 of $03 was 1
            Assert::IsTrue (cpu.Status ().flags.negative);
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Rra_RotatesMemoryRightThenAdds
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Rra_RotatesMemoryRightThenAdds)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0x10;
            cpu.Status ().flags.carry = 0;
            cpu.WriteBytes (0x8000, { 0x67, 0x10 });    // RRA $10
            cpu.WriteBytes (0x10,   { 0x02 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x01, cpu.Peek (0x10));    // ROR $02
            Assert::AreEqual ((Byte) 0x11, cpu.RegA ());        // 0x10 + 0x01 + carry 0
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Rra_HonorsDecimalModeInAdcStep
        //
        //  The ROR carry-out feeds the ADC, and the ADC respects the D flag --
        //  proving RRA composes the real decimal-mode adder rather than a bare
        //  binary add.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Rra_HonorsDecimalModeInAdcStep)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.Status ().flags.decimal = 1;
            cpu.RegA () = 0x09;
            cpu.Status ().flags.carry = 0;
            cpu.WriteBytes (0x8000, { 0x67, 0x10 });
            cpu.WriteBytes (0x10,   { 0x04 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x02, cpu.Peek (0x10));    // ROR $04
            Assert::AreEqual ((Byte) 0x11, cpu.RegA ());        // BCD 0x09 + 0x02
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Isc_IncrementsMemoryThenSubtracts
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Isc_IncrementsMemoryThenSubtracts)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0x20;
            cpu.Status ().flags.carry = 1;              // no borrow in
            cpu.WriteBytes (0x8000, { 0xE7, 0x10 });    // ISC $10
            cpu.WriteBytes (0x10,   { 0x0F });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x10, cpu.Peek (0x10));    // INC $0F
            Assert::AreEqual ((Byte) 0x10, cpu.RegA ());        // 0x20 - 0x10
            Assert::IsTrue (cpu.Status ().flags.carry);         // no borrow out
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dcp_DecrementsMemoryThenCompares
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dcp_DecrementsMemoryThenCompares)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0x10;
            cpu.WriteBytes (0x8000, { 0xC7, 0x10 });    // DCP $10
            cpu.WriteBytes (0x10,   { 0x10 });
            cpu.Step ();

            Assert::AreEqual ((Byte) 0x0F, cpu.Peek (0x10));    // DEC $10
            Assert::IsTrue  (cpu.Status ().flags.carry);        // A >= decremented value
            Assert::IsFalse (cpu.Status ().flags.zero);
            Assert::IsFalse (cpu.Status ().flags.negative);
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NopImplied_AdvancesOneByte_LeavesRegisters
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NopImplied_AdvancesOneByte_LeavesRegisters)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.RegA () = 0x42;
            cpu.WriteBytes (0x8000, { 0x1A });          // NOP (implied)
            cpu.Step ();

            Assert::AreEqual ((Word) 0x8001, cpu.RegPC ());
            Assert::AreEqual ((Byte) 0x42,   cpu.RegA ());
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NopDouble_AdvancesTwoBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NopDouble_AdvancesTwoBytes)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, { 0x04, 0x10 });    // DOP $10
            cpu.Step ();

            Assert::AreEqual ((Word) 0x8002, cpu.RegPC ());
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NopTriple_AdvancesThreeBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NopTriple_AdvancesThreeBytes)
        {
            TestCpu cpu;

            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, { 0x0C, 0x34, 0x12 });  // TOP $1234
            cpu.Step ();

            Assert::AreEqual ((Word) 0x8003, cpu.RegPC ());
        }
    };


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UndocumentedRegistrationTests
    //
    //  Table-level properties: base cycle counts (which the Harte state vectors
    //  do NOT check), the legal/assembler-hidden flags, and the guarantee that
    //  the shared opcode table never lets the assembler emit an undocumented
    //  opcode.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UndocumentedRegistrationTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BaseCycles_MatchStandard6502Timing
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BaseCycles_MatchStandard6502Timing)
        {
            TestCpu cpu;

            Assert::AreEqual ((Byte) 5, cpu.GetMicrocode (0x07).baseCycles);    // SLO zp
            Assert::AreEqual ((Byte) 6, cpu.GetMicrocode (0x17).baseCycles);    // SLO zp,X
            Assert::AreEqual ((Byte) 6, cpu.GetMicrocode (0x0F).baseCycles);    // SLO abs
            Assert::AreEqual ((Byte) 7, cpu.GetMicrocode (0x1F).baseCycles);    // SLO abs,X
            Assert::AreEqual ((Byte) 8, cpu.GetMicrocode (0x03).baseCycles);    // SLO (zp,X)
            Assert::AreEqual ((Byte) 8, cpu.GetMicrocode (0x13).baseCycles);    // SLO (zp),Y

            Assert::AreEqual ((Byte) 3, cpu.GetMicrocode (0x87).baseCycles);    // SAX zp
            Assert::AreEqual ((Byte) 4, cpu.GetMicrocode (0x8F).baseCycles);    // SAX abs
            Assert::AreEqual ((Byte) 6, cpu.GetMicrocode (0x83).baseCycles);    // SAX (zp,X)

            Assert::AreEqual ((Byte) 3, cpu.GetMicrocode (0xA7).baseCycles);    // LAX zp
            Assert::AreEqual ((Byte) 5, cpu.GetMicrocode (0xB3).baseCycles);    // LAX (zp),Y
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NopFamily_BaseCyclesAndByteCounts
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NopFamily_BaseCyclesAndByteCounts)
        {
            TestCpu cpu;

            Assert::AreEqual ((Byte) 2, cpu.GetMicrocode (0x1A).baseCycles);    // NOP implied
            Assert::AreEqual ((Byte) 3, cpu.GetMicrocode (0x04).baseCycles);    // DOP zp
            Assert::AreEqual ((Byte) 4, cpu.GetMicrocode (0x14).baseCycles);    // DOP zp,X
            Assert::AreEqual ((Byte) 2, cpu.GetMicrocode (0x80).baseCycles);    // DOP #imm
            Assert::AreEqual ((Byte) 4, cpu.GetMicrocode (0x0C).baseCycles);    // TOP abs
            Assert::AreEqual ((Byte) 4, cpu.GetMicrocode (0x1C).baseCycles);    // TOP abs,X
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Undocumented_AreLegalButAssemblerHidden
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Undocumented_AreLegalButAssemblerHidden)
        {
            TestCpu cpu;

            const Microcode & slo = cpu.GetMicrocode (0x07);

            Assert::IsTrue (slo.isLegal);
            Assert::IsTrue (slo.assemblerHidden);
            Assert::AreEqual ((int) Microcode::ShiftLeftAndOr, (int) slo.operation);
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Assembler_CannotEmitUndocumented_ButNopStaysCanonical
        //
        //  Every undocumented mnemonic is hidden from OpcodeTable::Lookup, so the
        //  assembler cannot emit one, and the only NOP it resolves is the
        //  canonical $EA -- never one of the undocumented NOP fillers.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Assembler_CannotEmitUndocumented_ButNopStaysCanonical)
        {
            TestCpu     cpu;
            OpcodeTable table (cpu.GetInstructionSet ());
            OpcodeEntry entry = {};

            Assert::IsFalse (table.Lookup ("SLO", GlobalAddressingMode::ZeroPage, entry));
            Assert::IsFalse (table.Lookup ("LAX", GlobalAddressingMode::ZeroPage, entry));

            Assert::IsTrue (table.Lookup ("NOP", GlobalAddressingMode::SingleByteNoOperand, entry));
            Assert::AreEqual ((Byte) 0xEA, entry.opcode);
        }
    };
}
