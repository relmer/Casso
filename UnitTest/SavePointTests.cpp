#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SavePointTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Fixture
    //
    //  Assembling a string of source and reaching the outputs it produced.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Fixture
    {
    public:

        static AssemblyResult Assemble (const std::string & source,
                                        DialectId           dialect = DialectId::Merlin)
        {
            TestCpu           cpu;
            TestCpu65C02      cmos;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = dialect;

            Assembler  assembler (cpu.GetInstructionSet(), cmos.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        //  Every byte the outputs hold, in order, so a test can assert that the
        //  outputs together account for the object without caring how they were
        //  divided.
        static std::vector<Byte> AllBytes (const AssemblyResult & result)
        {
            std::vector<Byte>  bytes;

            for (const SavePoint & span : result.savePoints)
            {
                bytes.insert (bytes.end(), span.bytes.begin(), span.bytes.end());
            }

            return bytes;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SavePointBasicsTests
    //
    //  What an assembly reports about its outputs before any directive cuts
    //  them apart. One save point is the ordinary case, not a special one, and
    //  these fix that so the several-output work cannot quietly change it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SavePointBasicsTests)
    {
    public:

        TEST_METHOD (EmittingSource_ProducesExactlyOneSavePoint)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n");

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.savePoints.size(),
                              L"a source with no save directive produces one output");
        }



        TEST_METHOD (SingleSavePoint_HoldsTheWholeObject)
        {
            AssemblyResult     result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n");
            std::vector<Byte>  expect = { 0xA9, 0x11, 0x60 };

            Assert::AreEqual ((size_t) 1, result.savePoints.size(), L"expected one output");
            Assert::IsTrue (expect == result.savePoints[0].bytes, L"output holds the assembled bytes");
            Assert::IsTrue (expect == result.bytes, L"and agrees with the whole-assembly bytes");
        }



        TEST_METHOD (SingleSavePoint_RecordsTheOrigin)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n");

            Assert::AreEqual ((size_t) 1, result.savePoints.size(), L"expected one output");
            Assert::IsTrue (result.savePoints[0].hasLoadAddress, L"the output records a load address");
            Assert::AreEqual ((int) 0x0300, (int) result.savePoints[0].loadAddress,
                              L"and it is the origin the source stated");
        }



        //  A source of only equates and comments emits nothing, so there is no
        //  output to report. Zero rather than one empty file, which is what a
        //  caller would otherwise have to write to a disk.
        TEST_METHOD (SourceEmittingNothing_ProducesNoSavePoint)
        {
            AssemblyResult  result = Fixture::Assemble ("* just a comment\nFOO = $10\n");

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 0, result.savePoints.size(),
                              L"nothing emitted means nothing to save");
        }



        //  The whole point of reporting outputs separately: they must account
        //  for the object exactly, with nothing added and nothing dropped.
        TEST_METHOD (SavePoints_AccountForEveryEmittedByte)
        {
            AssemblyResult     result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n NOP\n");
            std::vector<Byte>  all;

            Assert::IsTrue (result.savePoints.size() > 0, L"expected at least one output");

            all = Fixture::AllBytes (result);

            Assert::AreEqual (result.bytes.size(), all.size(),
                              L"the outputs together hold as many bytes as the assembly emitted");
            Assert::IsTrue (result.bytes == all, L"and exactly those bytes, in order");
        }



        //  as65 reaches the same machinery through a dialect that has no
        //  directive able to cut a span, so it must always report exactly one.
        TEST_METHOD (As65Source_ProducesOneSavePoint)
        {
            AssemblyResult  result = Fixture::Assemble ("        .org $300\n        lda #$11\n        rts\n",
                                                        DialectId::As65);

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.savePoints.size(),
                              L"as65 has no directive that cuts an output");
            Assert::AreEqual ((int) 0x0300, (int) result.savePoints[0].loadAddress,
                              L"and it loads where the source said");
        }



        //  A relocating origin moves the program counter without moving the
        //  output cursor, so the address a save point records is NOT where its
        //  bytes sit. Taking both from one cursor would file this at $0303.
        TEST_METHOD (RelocatingOrigin_RecordsTheStatedAddressNotTheOutputPosition)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n ORG $6000\n LDA #$22\n RTS\n");

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.savePoints.size(),
                              L"nothing cut the span, so it is still one output");
            Assert::AreEqual ((int) 0x0300, (int) result.savePoints[0].loadAddress,
                              L"the output begins where the first origin put it");
            Assert::AreEqual ((size_t) 6, result.savePoints[0].bytes.size(),
                              L"and holds both halves, contiguous in the output");
        }
    };
}
