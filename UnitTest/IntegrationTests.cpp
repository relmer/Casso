#include "Pch.h"

#include "TestHelpers.h"
#include "Assembler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace IntegrationTests
{
    // =========================================================================
    // T022: TestCpu::Assemble() Tests
    // =========================================================================
    TEST_CLASS (AssembleTests)
    {
    public:

        TEST_METHOD (Assemble_DefaultAddress_WritesToMemory)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            auto result = cpu.Assemble ("LDA #$42\nSTA $10");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Byte) 0xA9, cpu.Peek (0x8000));
            Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x8001));
            Assert::AreEqual ((Byte) 0x85, cpu.Peek (0x8002));
            Assert::AreEqual ((Byte) 0x10, cpu.Peek (0x8003));
            Assert::AreEqual ((Word) 0x8000, cpu.RegPC ());
        }

        TEST_METHOD (Assemble_ExplicitAddress_WritesToCorrectLocation)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            auto result = cpu.Assemble ("NOP", 0xC000);

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Byte) 0xEA, cpu.Peek (0xC000));
            Assert::AreEqual ((Word) 0xC000, cpu.RegPC ());
        }

        TEST_METHOD (Assemble_WithErrors_MemoryUnchanged)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            // Write sentinel bytes to verify memory is unchanged
            cpu.Poke (0x8000, 0xAA);
            cpu.Poke (0x8001, 0xBB);

            auto result = cpu.Assemble ("BEQ nowhere");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((Byte) 0xAA, cpu.Peek (0x8000));
            Assert::AreEqual ((Byte) 0xBB, cpu.Peek (0x8001));
        }
    };



    // =========================================================================
    // T023: TestCpu::RunUntil() Tests
    // =========================================================================
    TEST_CLASS (RunUntilTests)
    {
    public:

        TEST_METHOD (RunUntil_ExecutesAndStopsAtTarget)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            auto result = cpu.Assemble ("LDA #$42\nSTA $10\ndone: BRK");
            Assert::IsTrue (result.success);

            Word doneAddr = cpu.LabelAddress (result, "done");
            auto stop     = cpu.RunUntil (doneAddr);

            Assert::AreEqual ((int) TestCpu::StopReason::ReachedTarget, (int) stop);
            Assert::AreEqual ((Byte) 0x42, cpu.RegA ());
            Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x10));
        }

        TEST_METHOD (RunUntil_CycleLimit_Timeout)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            auto result = cpu.Assemble ("LDA #$42\nSTA $10\ndone: BRK");
            Assert::IsTrue (result.success);

            Word doneAddr = cpu.LabelAddress (result, "done");
            auto stop     = cpu.RunUntil (doneAddr, 1);

            Assert::AreEqual ((int) TestCpu::StopReason::CycleLimit, (int) stop);
        }

        TEST_METHOD (RunUntil_IllegalOpcode_Stops)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            // JMP to uninitialized memory ($FF = illegal opcode)
            auto result = cpu.Assemble ("JMP $0200");
            Assert::IsTrue (result.success);

            // Memory at $0200 is 0x00 (initialized by InitForTest)
            // 0x00 = BRK — actually a legal opcode. Let's write an illegal one.
            cpu.Poke (0x0200, 0x02); // 0x02 is an illegal/undocumented opcode

            auto stop = cpu.RunUntil (0xFFFF, 100);

            Assert::AreEqual ((int) TestCpu::StopReason::IllegalOpcode, (int) stop);
        }
    };



    // =========================================================================
    // T024: TestCpu::LabelAddress() Tests
    // =========================================================================
    TEST_CLASS (LabelAddressTests)
    {
    public:

        TEST_METHOD (LabelAddress_ReturnsCorrectAddresses)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            auto result = cpu.Assemble ("start: NOP\nend: BRK");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x8000, cpu.LabelAddress (result, "start"));
            Assert::AreEqual ((Word) 0x8001, cpu.LabelAddress (result, "end"));
        }
    };



    // =========================================================================
    // T024a: BRK Software Interrupt Test
    // =========================================================================
    TEST_CLASS (BrkInterruptTests)
    {
    public:

        TEST_METHOD (BRK_PushesStatusAndPC_LoadsIRQVector)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            // Set up IRQ vector at $FFFE/$FFFF pointing to handler at $C000
            cpu.PokeWord (0xFFFE, 0xC000);

            // Put a NOP at handler so RunUntil has something to stop at
            cpu.Poke (0xC000, 0xEA); // NOP

            // Assemble and run BRK
            auto result = cpu.Assemble ("LDA #$42\nBRK");
            Assert::IsTrue (result.success);

            // Execute LDA + BRK
            cpu.StepN (2);

            // PC should be loaded from IRQ vector
            Assert::AreEqual ((Word) 0xC000, cpu.RegPC ());

            // Stack should have pushed: PChi, PClo, status (with B flag set)
            // SP started at 0xFF, pushed 3 bytes → SP = 0xFC
            Assert::AreEqual ((Byte) 0xFC, cpu.RegSP ());

            // Status byte on stack should have B flag (bit 4) set
            Byte pushedStatus = cpu.Peek (0x01FD);
            Assert::IsTrue ((pushedStatus & 0x10) != 0, L"B flag should be set in pushed status");
        }
    };



    // =========================================================================
    // T025: Stack Page Boundary Tests
    // =========================================================================
    TEST_CLASS (StackPageTests)
    {
    public:

        TEST_METHOD (PushWord_AtMaxSP_StaysWithinStackPage)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            // SP starts at 0xFF; push should write hi byte to 0x01FF, lo byte to 0x01FE
            cpu.DoPushWord (0xABCD);

            Assert::AreEqual ((Byte) 0xAB, cpu.Peek (0x01FF), L"High byte at 0x01FF");
            Assert::AreEqual ((Byte) 0xCD, cpu.Peek (0x01FE), L"Low byte at 0x01FE");
            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x0200), L"No write outside stack page");
            Assert::AreEqual ((Byte) 0xFD, cpu.RegSP ());
        }

        TEST_METHOD (PopWord_AfterPushWord_ReturnsOriginalValue)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            cpu.DoPushWord (0xABCD);
            Word value = cpu.DoPopWord ();

            Assert::AreEqual ((Word) 0xABCD, value);
            Assert::AreEqual ((Byte) 0xFF, cpu.RegSP ());
        }

        TEST_METHOD (BRK_AtMaxSP_DoesNotWriteOutsideStackPage)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            // Set up IRQ vector
            cpu.PokeWord (0xFFFE, 0xC000);
            cpu.Poke     (0xC000, 0xEA);  // NOP at handler

            // BRK with SP=0xFF; PushWord(PC+1) must not touch 0x0200
            cpu.Assemble ("BRK");
            cpu.StepN (1);

            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x0200), L"No write to 0x0200");
        }
    };




    // =========================================================================
    // T055: Quickstart Validation
    // =========================================================================
    TEST_CLASS (QuickstartValidationTests)
    {
    public:

        TEST_METHOD (QuickstartExample_AssemblesAndRuns)
        {
            TestCpu cpu;
            cpu.InitForTest ();

            auto result = cpu.Assemble (
                "; Multiply A by 2 using shifts\n"
                "    .org $8000\n"
                "\n"
                "    LDA #$15\n"
                "    ASL A\n"
                "    STA $10\n"
                "done:\n"
                "    BRK\n"
            );

            Assert::IsTrue (result.success, L"Quickstart example should assemble successfully");

            Word doneAddr = cpu.LabelAddress (result, "done");
            auto stop     = cpu.RunUntil (doneAddr);

            Assert::AreEqual ((int) TestCpu::StopReason::ReachedTarget, (int) stop);

            // LDA #$15 (21), ASL A (shift left = 42 = 0x2A), STA $10
            Assert::AreEqual ((Byte) 0x2A, cpu.RegA ());
            Assert::AreEqual ((Byte) 0x2A, cpu.Peek (0x10));
        }
    };



    // =========================================================================
    // T057: WriteBytes Equivalence Test
    // =========================================================================
    TEST_CLASS (WriteBytesEquivalenceTests)
    {
    public:

        TEST_METHOD (AssembledAndWriteBytes_ProduceIdenticalResults)
        {
            // Assemble a program
            TestCpu cpuAsm;
            cpuAsm.InitForTest ();

            auto result = cpuAsm.Assemble (
                "    LDA #$42\n"
                "    STA $10\n"
                "done: BRK\n"
            );

            Assert::IsTrue (result.success);

            Word doneAddr = cpuAsm.LabelAddress (result, "done");
            cpuAsm.RunUntil (doneAddr);

            // Write the same raw bytes manually
            TestCpu cpuRaw;
            cpuRaw.InitForTest ();

            cpuRaw.WriteBytes (0x8000, {
                0xA9, 0x42,       // LDA #$42
                0x85, 0x10,       // STA $10
                0x00,             // BRK
            });

            cpuRaw.RunUntil (0x8004); // done = 0x8004

            // Both CPUs should have identical state
            Assert::AreEqual (cpuAsm.RegA (),    cpuRaw.RegA ());
            Assert::AreEqual (cpuAsm.Peek (0x10), cpuRaw.Peek (0x10));
            Assert::AreEqual (cpuAsm.RegX (),    cpuRaw.RegX ());
            Assert::AreEqual (cpuAsm.RegY (),    cpuRaw.RegY ());
        }
    };
}
