#include "Pch.h"

#include "TestCpu65C02.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace TestCpu65C02Tests
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  TestCpu65C02FlatMemory
    //
    //  Proves the flat-memory 65C02 harness CPU executes CMOS opcodes and
    //  reads/writes the full 64K -- including $C000-$FFFF and the vector page,
    //  which the bus-routed harness cannot reach. This is the capability the
    //  Dormann/Harte 65C02 conformance harnesses depend on.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TestCpu65C02FlatMemory)
    {
    public:
        TEST_METHOD (RunsCmosCodeAcrossHighMemory)
        {
            TestCpu65C02 cpu;

            cpu.InitForTest (0xF000);
            cpu.Poke (0xCF00, 0xFF);

            // $F000: STZ $CF00 ; LDA #$42 ; STA $D000   (CMOS STZ + high-RAM I/O)
            cpu.Poke (0xF000, 0x9C); cpu.Poke (0xF001, 0x00); cpu.Poke (0xF002, 0xCF);
            cpu.Poke (0xF003, 0xA9); cpu.Poke (0xF004, 0x42);
            cpu.Poke (0xF005, 0x8D); cpu.Poke (0xF006, 0x00); cpu.Poke (0xF007, 0xD0);

            cpu.Step ();    // STZ $CF00
            cpu.Step ();    // LDA #$42
            cpu.Step ();    // STA $D000

            Assert::AreEqual<Byte> (0x00, cpu.Peek (0xCF00));   // CMOS STZ into high RAM
            Assert::AreEqual<Byte> (0x42, cpu.RegA ());
            Assert::AreEqual<Byte> (0x42, cpu.Peek (0xD000));   // store into high RAM
            Assert::AreEqual<Word> (0xF008, cpu.RegPC ());
        }


        TEST_METHOD (BrkVectorsThroughFlatVectorPage)
        {
            TestCpu65C02 cpu;

            cpu.InitForTest (0x0200);
            cpu.Status ().flags.decimal = 1;    // 65C02 clears D on BRK entry
            cpu.PokeWord (0xFFFE, 0x0400);      // IRQ/BRK vector -> $0400
            cpu.Poke (0x0200, 0x00);            // BRK

            cpu.Step ();

            Assert::AreEqual<Word> (0x0400, cpu.RegPC ());          // vector read from flat $FFFE
            Assert::IsTrue (cpu.Status ().flags.decimal == 0);      // BreakCmos cleared D
        }
    };
}
