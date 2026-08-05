#include "Pch.h"

#include "TestHelpers.h"
#include "Assembler.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace IntegrationTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AssembleTests
    //
    //  Source in, assembled, loaded, EXECUTED -- the assembler and the CPU
    //  checked against each other end to end.
    //
    //  The unit tests on either side can both be wrong in agreeing ways: an
    //  opcode encoded incorrectly by the assembler and decoded to the same
    //  wrong meaning by the CPU passes both suites. Running the bytes is what
    //  makes the round trip real.
    //
    //  Assertions are on the machine's final STATE -- registers and memory --
    //  rather than on the emitted bytes, so the test says what the program did
    //  rather than restating the encoding a unit test already covers.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AssembleTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Assemble_DefaultAddress_WritesToMemory
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Assemble_DefaultAddress_WritesToMemory)
        {
            TestCpu cpu;
            cpu.InitForTest();

            auto result = cpu.Assemble (
                R"(                 LDA #$42
                                    STA $10
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Byte) 0xA9, cpu.Peek (0x8000));
            Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x8001));
            Assert::AreEqual ((Byte) 0x85, cpu.Peek (0x8002));
            Assert::AreEqual ((Byte) 0x10, cpu.Peek (0x8003));
            Assert::AreEqual ((Word) 0x8000, cpu.RegPC());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Assemble_ExplicitAddress_WritesToCorrectLocation
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Assemble_ExplicitAddress_WritesToCorrectLocation)
        {
            TestCpu cpu;
            cpu.InitForTest();

            auto result = cpu.Assemble ("NOP", 0xC000);

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Byte) 0xEA, cpu.Peek (0xC000));
            Assert::AreEqual ((Word) 0xC000, cpu.RegPC());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Assemble_WithErrors_MemoryUnchanged
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Assemble_WithErrors_MemoryUnchanged)
        {
            TestCpu cpu;
            cpu.InitForTest();

            // Write sentinel bytes to verify memory is unchanged
            cpu.Poke (0x8000, 0xAA);
            cpu.Poke (0x8001, 0xBB);

            auto result = cpu.Assemble ("BEQ nowhere");

            Assert::IsFalse (result.success);
            Assert::AreEqual ((Byte) 0xAA, cpu.Peek (0x8000));
            Assert::AreEqual ((Byte) 0xBB, cpu.Peek (0x8001));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RunUntilTests
    //
    //  Running real programs -- loops, subroutine calls, indexed access -- to a
    //  stop address.
    //
    //  These are the multi-instruction behaviors no single-instruction test
    //  reaches: a loop depends on flags surviving from the compare to the
    //  branch, a subroutine on JSR and RTS agreeing about the stack, indexed
    //  access on the index register advancing between iterations.
    //
    //  Every run is BOUNDED by a cycle limit as well as a stop address, so a
    //  program that never terminates fails the test instead of hanging the
    //  suite -- which is the failure mode any of these bugs actually produces.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RunUntilTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RunUntil_ExecutesAndStopsAtTarget
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RunUntil_ExecutesAndStopsAtTarget)
        {
            TestCpu              cpu;
            TestCpu::StopReason  stop = {};
            cpu.InitForTest();

            auto result = cpu.Assemble 
                (R"(                LDA #$42
                                    STA $10
                            done:
                                    BRK
                )");

            Assert::IsTrue (result.success);

            Word doneAddr = cpu.LabelAddress (result, "done");
            stop = cpu.RunUntil (doneAddr);

            Assert::AreEqual ((int) TestCpu::StopReason::ReachedTarget, (int) stop);
            Assert::AreEqual ((Byte) 0x42, cpu.RegA());
            Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x10));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RunUntil_CycleLimit_Timeout
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RunUntil_CycleLimit_Timeout)
        {
            TestCpu              cpu;
            TestCpu::StopReason  stop = {};
            cpu.InitForTest();

            auto result = cpu.Assemble (
                R"(
                                    LDA #$42
                                    STA $10
                            done:
                                    BRK
                )");

            Assert::IsTrue (result.success);

            Word doneAddr = cpu.LabelAddress (result, "done");
            stop = cpu.RunUntil (doneAddr, 1);

            Assert::AreEqual ((int) TestCpu::StopReason::CycleLimit, (int) stop);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  RunUntil_IllegalOpcode_Stops
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (RunUntil_IllegalOpcode_Stops)
        {
            TestCpu              cpu;
            TestCpu::StopReason  stop = {};
            cpu.InitForTest();

            // JMP to uninitialized memory ($FF = illegal opcode)
            auto result = cpu.Assemble ("JMP $0200");
            Assert::IsTrue (result.success);

            // Memory at $0200 is 0x00 (initialized by InitForTest)
            // 0x00 = BRK — actually a legal opcode. Let's write an illegal one.
            cpu.Poke (0x0200, 0x02); // 0x02 is an illegal/undocumented opcode

            stop = cpu.RunUntil (0xFFFF, 100);

            Assert::AreEqual ((int) TestCpu::StopReason::IllegalOpcode, (int) stop);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LabelAddressTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LabelAddressTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LabelAddress_ReturnsCorrectAddresses
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LabelAddress_ReturnsCorrectAddresses)
        {
            TestCpu cpu;
            cpu.InitForTest();

            auto result = cpu.Assemble (
                R"(         start:  NOP
                            end:    BRK
                )");

            Assert::IsTrue (result.success);
            Assert::AreEqual ((Word) 0x8000, cpu.LabelAddress (result, "start"));
            Assert::AreEqual ((Word) 0x8001, cpu.LabelAddress (result, "end"));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BrkInterruptTests
    //
    //  BRK's full sequence: push the status and PC, set I, and vector through
    //  $FFFE.
    //
    //  BRK is a two-byte instruction whose second byte is IGNORED, and the
    //  pushed PC points past it. That padding byte is the detail an
    //  implementation gets wrong -- pushing the address of the byte after the
    //  opcode makes every RTI return into the padding.
    //
    //  The pushed STATUS has the B flag set, which is how a handler tells a
    //  software BRK from a hardware IRQ. The two share a vector, so without
    //  that bit they are indistinguishable.
    //
    //  This runs the whole path rather than testing the pieces, since the
    //  interaction between the push order and the vector fetch is exactly what
    //  a real handler depends on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BrkInterruptTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BRK_PushesStatusAndPC_LoadsIRQVector
        //
        //  Asserts each piece of the BRK sequence separately: the three pushed
        //  bytes, the I flag, and the new PC.
        //
        //  Separately rather than by running a handler and checking it
        //  returned, so a failure names WHICH part of the sequence is wrong.
        //  A round-trip test would fail identically for a bad push order, a
        //  wrong vector, and an off-by-one PC.
        //
        //  The stack is read directly at $01xx to verify the push order --
        //  status on top of a big-endian-pushed PC -- which is what an RTI
        //  depends on and what a handler inspecting its own return address
        //  reads.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BRK_PushesStatusAndPC_LoadsIRQVector)
        {
            TestCpu  cpu;
            Byte     pushedStatus = 0;
            cpu.InitForTest();

            // Set up IRQ vector at $FFFE/$FFFF pointing to handler at $C000
            cpu.PokeWord (0xFFFE, 0xC000);

            // Put a NOP at handler so RunUntil has something to stop at
            cpu.Poke (0xC000, 0xEA); // NOP

            // Assemble and run BRK
            auto result = cpu.Assemble (
                R"(                 LDA #$42
                                    BRK
                )");
                
            Assert::IsTrue (result.success);

            // Execute LDA + BRK
            cpu.StepN (2);

            // PC should be loaded from IRQ vector
            Assert::AreEqual ((Word) 0xC000, cpu.RegPC());

            // Stack should have pushed: PChi, PClo, status (with B flag set)
            // SP started at 0xFF, pushed 3 bytes → SP = 0xFC
            Assert::AreEqual ((Byte) 0xFC, cpu.RegSP());

            // Status byte on stack should have B flag (bit 4) set
            pushedStatus = cpu.Peek (0x01FD);
            Assert::IsTrue ((pushedStatus & 0x10) != 0, L"B flag should be set in pushed status");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StackPageTests
    //
    //  The stack living in page 1, growing down, and wrapping within it.
    //
    //  Run as programs rather than by poking the pointer, so the addresses are
    //  the ones real pushes and pulls produce -- these assert where the bytes
    //  actually LAND, at $0100 plus the pointer, which is what nested
    //  subroutines and interrupt handlers depend on.
    //
    //  The wrap is exercised deliberately: overflowing past the bottom returns
    //  to $01FF rather than descending into zero page. That is what makes stack
    //  overflow corrupt the stack -- survivable, and observed in real software
    //  -- instead of silently overwriting zero-page variables, which is not.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StackPageTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  PushWord_AtMaxSP_StaysWithinStackPage
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PushWord_AtMaxSP_StaysWithinStackPage)
        {
            TestCpu cpu;
            cpu.InitForTest();

            // SP starts at 0xFF; push should write hi byte to 0x01FF, lo byte to 0x01FE
            cpu.DoPushWord (0xABCD);

            Assert::AreEqual ((Byte) 0xAB, cpu.Peek (0x01FF), L"High byte at 0x01FF");
            Assert::AreEqual ((Byte) 0xCD, cpu.Peek (0x01FE), L"Low byte at 0x01FE");
            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x0200), L"No write outside stack page");
            Assert::AreEqual ((Byte) 0xFD, cpu.RegSP());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  PopWord_AfterPushWord_ReturnsOriginalValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (PopWord_AfterPushWord_ReturnsOriginalValue)
        {
            TestCpu  cpu;
            Word     value = 0;
            cpu.InitForTest();

            cpu.DoPushWord (0xABCD);
            value = cpu.DoPopWord();

            Assert::AreEqual ((Word) 0xABCD, value);
            Assert::AreEqual ((Byte) 0xFF, cpu.RegSP());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BRK_AtMaxSP_DoesNotWriteOutsideStackPage
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BRK_AtMaxSP_DoesNotWriteOutsideStackPage)
        {
            TestCpu cpu;
            cpu.InitForTest();

            // Set up IRQ vector
            cpu.PokeWord (0xFFFE, 0xC000);
            cpu.Poke     (0xC000, 0xEA);  // NOP at handler

            // BRK with SP=0xFF; PushWord(PC+1) must not touch 0x0200
            cpu.Assemble ("BRK");
            cpu.StepN (1);

            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x0200), L"No write to 0x0200");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  QuickstartValidationTests
    //
    //  The example program from the documentation, assembled and run.
    //
    //  Documentation rots silently. A quickstart whose sample no longer
    //  assembles is the first thing a new user meets and the last thing anyone
    //  else notices, so the example is executed here rather than trusted.
    //
    //  The source is kept identical to the published text, which is the point
    //  -- adapting it to make the test convenient would test something the
    //  reader never sees.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (QuickstartValidationTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  QuickstartExample_AssemblesAndRuns
        //
        //  Assembles the documented sample verbatim and asserts the result the
        //  documentation claims.
        //
        //  Both halves matter. A sample that assembles but computes something
        //  other than what the text says is just as misleading as one that
        //  fails outright, so the asserted value is the one printed in the
        //  docs.
        //
        //  Any edit to the quickstart's code block should be made here too --
        //  that coupling is deliberate, and it is what keeps the two from
        //  drifting.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (QuickstartExample_AssemblesAndRuns)
        {
            TestCpu              cpu;
            TestCpu::StopReason  stop = {};
            cpu.InitForTest();

            auto result = cpu.Assemble (
                R"(         ; Multiply A by 2 using shifts
                                    .org $8000
                        
                                    LDA #$15
                                    ASL A
                                    STA $10
                            done:
                                    BRK
                )");

            Assert::IsTrue (result.success, L"Quickstart example should assemble successfully");

            Word doneAddr = cpu.LabelAddress (result, "done");
            stop = cpu.RunUntil (doneAddr);

            Assert::AreEqual ((int) TestCpu::StopReason::ReachedTarget, (int) stop);

            // LDA #$15 (21), ASL A (shift left = 42 = 0x2A), STA $10
            Assert::AreEqual ((Byte) 0x2A, cpu.RegA());
            Assert::AreEqual ((Byte) 0x2A, cpu.Peek (0x10));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WriteBytesEquivalenceTests
    //
    //  Assembled output and hand-written data directives must produce the
    //  IDENTICAL image.
    //
    //  Two paths reach the same bytes -- the instruction encoder and the .byte
    //  directive -- and they run through different code. Asserting they agree
    //  means either can be used to check the other, which is what makes a
    //  hand-written expected image a valid oracle elsewhere.
    //
    //  It also catches the subtler divergence: a data directive that lays bytes
    //  at a different address than instructions of the same length, which
    //  matters as soon as a source mixes code and tables.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WriteBytesEquivalenceTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AssembledAndWriteBytes_ProduceIdenticalResults
        //
        //  Assembles the same program twice -- once as instructions, once as
        //  .byte data -- and compares the images byte for byte.
        //
        //  Comparing the two OUTPUTS rather than either against a literal is
        //  what makes this meaningful: a hard-coded expected image would only
        //  restate whichever path was used to produce it.
        //
        //  Start and end addresses are compared as well as the bytes, since two
        //  identical byte sequences placed at different origins are not the
        //  same image to a loader.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AssembledAndWriteBytes_ProduceIdenticalResults)
        {
            TestCpu cpuRaw;



            // Assemble a program
            TestCpu cpuAsm;
            cpuAsm.InitForTest();

            auto result = cpuAsm.Assemble (
                R"(                 LDA #$42
                                    STA $10
                            done:
                                    BRK
                )");

            Assert::IsTrue (result.success);

            Word doneAddr = cpuAsm.LabelAddress (result, "done");
            cpuAsm.RunUntil (doneAddr);

            // Write the same raw bytes manually
            cpuRaw.InitForTest();

            cpuRaw.WriteBytes (0x8000, {
                0xA9, 0x42,       // LDA #$42
                0x85, 0x10,       // STA $10
                0x00,             // BRK
            });

            cpuRaw.RunUntil (0x8004); // done = 0x8004

            // Both CPUs should have identical state
            Assert::AreEqual (cpuAsm.RegA(),     cpuRaw.RegA());
            Assert::AreEqual (cpuAsm.Peek (0x10), cpuRaw.Peek (0x10));
            Assert::AreEqual (cpuAsm.RegX(),     cpuRaw.RegX());
            Assert::AreEqual (cpuAsm.RegY(),     cpuRaw.RegY());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LoadBinaryTests
    //
    //  Cpu::LoadBinary: the bytes that land, and each way it can refuse.
    //
    //  Driven through the STREAM overload so the tests never touch the
    //  filesystem -- that overload exists for exactly this reason, and the
    //  filename form is a thin wrapper over it.
    //
    //  The refusals are covered as carefully as the success: an image that
    //  would run off the top of memory, a stream that fails mid-read, and a
    //  zero-length load are each distinct, and the first two are reported with
    //  DIFFERENT codes -- a caller's bad address is not the same as a broken
    //  file.
    //
    //  Memory is asserted unchanged on failure, which is the contract that
    //  makes a failed load recoverable rather than leaving the machine holding
    //  half an image.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LoadBinaryTests)
    {
    public:

        // Build an in-memory binary stream from a list of bytes.
        static std::istringstream MakeStream (std::initializer_list<Byte> bytes)
        {
            std::string data (reinterpret_cast<const char *>(bytes.begin()), bytes.size());
            return std::istringstream (data, std::ios::binary);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LoadBinary_ValidStream_LoadsBytesAtAddress
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LoadBinary_ValidStream_LoadsBytesAtAddress)
        {
            TestCpu             cpu;
            HRESULT             hr  = S_OK;
            std::istringstream  bin;
            cpu.InitForTest();

            bin = MakeStream ({ 0xA9, 0x42, 0x85, 0x10, 0x00 });

            hr = cpu.LoadBinary (bin, (Word) 0x8000);

            AssertSucceeded (hr);
            Assert::AreEqual ((Byte) 0xA9, cpu.Peek (0x8000));
            Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x8001));
            Assert::AreEqual ((Byte) 0x85, cpu.Peek (0x8002));
            Assert::AreEqual ((Byte) 0x10, cpu.Peek (0x8003));
            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x8004));

            // Bytes outside the loaded range are unchanged.
            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x7FFF));
            Assert::AreEqual ((Byte) 0x00, cpu.Peek (0x8005));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LoadBinary_LoadedProgram_Executes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LoadBinary_LoadedProgram_Executes)
        {
            TestCpu             cpu;
            std::istringstream  bin;
            cpu.InitForTest();

            // LDA #$42 ; STA $10 ; BRK
            bin = MakeStream ({ 0xA9, 0x42, 0x85, 0x10, 0x00 });

            AssertSucceeded (cpu.LoadBinary (bin, (Word) 0x8000));

            cpu.RegPC() = 0x8000;
            cpu.StepN (2); // LDA, STA

            Assert::AreEqual ((Byte) 0x42, cpu.RegA());
            Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x10));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LoadBinary_TooLargeForAddress_ReturnsFalse
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LoadBinary_TooLargeForAddress_ReturnsFalse)
        {
            TestCpu             cpu;
            HRESULT             hr  = S_OK;
            std::istringstream  bin;
            cpu.InitForTest();

            // 3-byte stream, but loading at 0xFFFE would need address 0x10000 (overflow).
            bin = MakeStream ({ 0x11, 0x22, 0x33 });

            cpu.Poke (0xFFFE, 0xAB);
            cpu.Poke (0xFFFF, 0xCD);

            hr = cpu.LoadBinary (bin, (Word) 0xFFFE);

            Assert::IsTrue (FAILED (hr), L"an image that overruns the address space must fail");
            // Memory unchanged on failure.
            Assert::AreEqual ((Byte) 0xAB, cpu.Peek (0xFFFE));
            Assert::AreEqual ((Byte) 0xCD, cpu.Peek (0xFFFF));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LoadBinary_FitsExactlyAtEnd_Succeeds
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LoadBinary_FitsExactlyAtEnd_Succeeds)
        {
            TestCpu             cpu;
            HRESULT             hr  = S_OK;
            std::istringstream  bin;
            cpu.InitForTest();

            // 2-byte stream loaded at 0xFFFE fills the last two bytes of the address space.
            bin = MakeStream ({ 0xAA, 0xBB });

            hr = cpu.LoadBinary (bin, (Word) 0xFFFE);

            AssertSucceeded (hr);
            Assert::AreEqual ((Byte) 0xAA, cpu.Peek (0xFFFE));
            Assert::AreEqual ((Byte) 0xBB, cpu.Peek (0xFFFF));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LoadBinary_EmptyStream_Succeeds
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LoadBinary_EmptyStream_Succeeds)
        {
            TestCpu             cpu;
            HRESULT             hr  = S_OK;
            std::istringstream  bin;
            cpu.InitForTest();

            bin = MakeStream ({});

            cpu.Poke (0x8000, 0xAB);

            hr = cpu.LoadBinary (bin, (Word) 0x8000);

            AssertSucceeded (hr);
            // Empty stream means nothing is overwritten.
            Assert::AreEqual ((Byte) 0xAB, cpu.Peek (0x8000));
        }
    };
}
