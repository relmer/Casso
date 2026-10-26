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

            cpu.Step();    // STZ $CF00
            cpu.Step();    // LDA #$42
            cpu.Step();    // STA $D000

            Assert::AreEqual<Byte> (0x00, cpu.Peek (0xCF00));   // CMOS STZ into high RAM
            Assert::AreEqual<Byte> (0x42, cpu.RegA());
            Assert::AreEqual<Byte> (0x42, cpu.Peek (0xD000));   // store into high RAM
            Assert::AreEqual<Word> (0xF008, cpu.RegPC());
        }


        TEST_METHOD (BrkVectorsThroughFlatVectorPage)
        {
            TestCpu65C02 cpu;

            cpu.InitForTest (0x0200);
            cpu.Status().flags.decimal = 1;    // 65C02 clears D on BRK entry
            cpu.PokeWord (0xFFFE, 0x0400);      // IRQ/BRK vector -> $0400
            cpu.Poke (0x0200, 0x00);            // BRK

            cpu.Step();

            Assert::AreEqual<Word> (0x0400, cpu.RegPC());          // vector read from flat $FFFE
            Assert::IsTrue (cpu.Status().flags.decimal == 0);      // BreakCmos cleared D
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  ReadCountingCpu
    //
    //  The flat-memory 65C02 with a count of the reads of one I/O address,
    //  the Uthernet II's data register in slot 3.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ReadCountingCpu : public TestCpu65C02
    {
    public:
        static constexpr Word  kWatched = 0xC0B7;

        Byte  ReadByteSlow (Word address) override
        {
            if (address == kWatched)
            {
                m_reads++;
            }

            return TestCpu65C02::ReadByteSlow (address);
        }

        int  m_reads = 0;
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  StoreTargetReads
    //
    //  A store writes its target and never reads it, in every addressing mode
    //  that can address I/O space. A read of the W5100's data register
    //  advances its address pointer, so each byte a guest stores is written
    //  one address further on.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StoreTargetReads)
    {
    public:

        struct Case
        {
            const wchar_t *     what;
            std::vector<Byte>   code;
            int                 expectedReads;
        };


        //  X = 7 and Y = 7; $10 points at $C0B0 and $20 at $C0B7, so every
        //  form below addresses $C0B7.
        static int RunCase (const Case & c)
        {
            ReadCountingCpu  cpu;
            size_t           i = 0;

            cpu.InitForTest (0x0300);
            cpu.RegA() = 0x5A;
            cpu.RegX() = 0x07;
            cpu.RegY() = 0x07;
            cpu.PokeWord (0x0010, 0xC0B0);
            cpu.PokeWord (0x0020, 0xC0B7);
            cpu.PokeWord (0x0017, 0xC0B7);

            for (i = 0; i < c.code.size(); i++)
            {
                cpu.Poke ((Word) (0x0300 + i), c.code[i]);
            }

            cpu.Step();

            return cpu.m_reads;
        }


        TEST_METHOD (StoresNeverReadTheirTarget)
        {
            const std::vector<Case>  cases =
            {
                { L"STA abs",      { 0x8D, 0xB7, 0xC0 }, 0 },
                { L"STA abs,X",    { 0x9D, 0xB0, 0xC0 }, 0 },
                { L"STA abs,Y",    { 0x99, 0xB0, 0xC0 }, 0 },
                { L"STA (zp,X)",   { 0x81, 0x10 },       0 },
                { L"STA (zp),Y",   { 0x91, 0x10 },       0 },
                { L"STA (zp)",     { 0x92, 0x20 },       0 },
                { L"STX abs",      { 0x8E, 0xB7, 0xC0 }, 0 },
                { L"STY abs",      { 0x8C, 0xB7, 0xC0 }, 0 },
                { L"STZ abs",      { 0x9C, 0xB7, 0xC0 }, 0 },
                { L"STZ abs,X",    { 0x9E, 0xB0, 0xC0 }, 0 },
                { L"LDA (zp),Y",   { 0xB1, 0x10 },       1 },
                { L"LDA abs",      { 0xAD, 0xB7, 0xC0 }, 1 },
            };

            for (const Case & c : cases)
            {
                Assert::AreEqual (c.expectedReads, RunCase (c), c.what);
            }
        }
    };
}
