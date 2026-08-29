#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinSaveObjectTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Fixture
    //
    //  Assembling Merlin source and reaching the outputs it produced.
    //
    //  THE EXPECTED VALUES IN THIS FILE WERE MEASURED, not chosen. Each source
    //  below was run through Merlin Pro 2.23 under Casso and its objects read
    //  back off the disk; the addresses and lengths asserted here are what the
    //  period assembler produced. Where a rule was reasoned out instead, the
    //  test says so.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Fixture
    {
    public:

        static AssemblyResult Assemble (const std::string & source)
        {
            TestCpu           cpu;
            TestCpu65C02      cmos;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = DialectId::Merlin;

            Assembler  assembler (cpu.GetInstructionSet(), cmos.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SaveObjectTests
    //
    //  What the save directive cuts, and where each piece says it loads.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SaveObjectTests)
    {
    public:

        //  Measured: SPAN1A $0300 L$0003, SPAN1B $6000 L$0003.
        //
        //  BOTH CLAUSES DISCRIMINATE. Three bytes rather than six is what
        //  separates this from a cumulative implementation, which would put the
        //  first file's bytes inside the second. And $6000 rather than $0303 is
        //  what separates a stated origin governing from addresses running on
        //  from the previous save.
        TEST_METHOD (TwoSavesWithAnOriginBetweenThem)
        {
            AssemblyResult     result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n SAV SPAN1A\n"
                                                           " ORG $6000\n LDA #$22\n RTS\n SAV SPAN1B\n");
            std::vector<Byte>  first  = { 0xA9, 0x11, 0x60 };
            std::vector<Byte>  second = { 0xA9, 0x22, 0x60 };

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.savePoints.size(), L"two saves, two outputs");

            Assert::AreEqual (std::string ("SPAN1A"), result.savePoints[0].name, L"first is named by its save");
            Assert::AreEqual ((int) 0x0300, (int) result.savePoints[0].loadAddress, L"first at its origin");
            Assert::IsTrue (first == result.savePoints[0].bytes, L"first holds its own bytes");

            Assert::AreEqual (std::string ("SPAN1B"), result.savePoints[1].name, L"second is named by its save");
            Assert::AreEqual ((int) 0x6000, (int) result.savePoints[1].loadAddress,
                              L"second takes the stated origin, not the previous end plus one");
            Assert::IsTrue (second == result.savePoints[1].bytes,
                            L"second holds ONLY the bytes after the first save");
        }



        //  Measured: SPAN2A $0300 L$0003, SPAN2B $0303 L$0003 -- two files from
        //  two output-file directives with no save anywhere.
        TEST_METHOD (TwoObjectFileDirectivesCutTwoOutputsWithNoSave)
        {
            AssemblyResult     result = Fixture::Assemble (" DSK SPAN2A\n ORG $300\n LDA #$11\n RTS\n"
                                                           " DSK SPAN2B\n LDA #$22\n RTS\n");
            std::vector<Byte>  first  = { 0xA9, 0x11, 0x60 };
            std::vector<Byte>  second = { 0xA9, 0x22, 0x60 };

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.savePoints.size(),
                              L"a second output-file directive closes the first and begins another");

            Assert::AreEqual (std::string ("SPAN2A"), result.savePoints[0].name, L"first name");
            Assert::AreEqual ((int) 0x0300, (int) result.savePoints[0].loadAddress, L"first at its origin");
            Assert::IsTrue (first == result.savePoints[0].bytes, L"first holds its own bytes");

            Assert::AreEqual (std::string ("SPAN2B"), result.savePoints[1].name, L"second name");
            Assert::AreEqual ((int) 0x0303, (int) result.savePoints[1].loadAddress,
                              L"nothing moved the counter, so the second runs on from the first");
            Assert::IsTrue (second == result.savePoints[1].bytes, L"second holds its own bytes");
        }



        //  Measured: only SPAN3A reaches the disk. The trailing instructions are
        //  assembled and counted and written nowhere.
        TEST_METHOD (BytesAfterTheLastSaveAreDroppedAndWarnedAbout)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n SAV SPAN3A\n"
                                                        " LDA #$22\n RTS\n");

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.savePoints.size(),
                              L"nothing names the trailing bytes, so no second output");
            Assert::AreEqual (std::string ("SPAN3A"), result.savePoints[0].name, L"the one output");

            Assert::AreEqual ((size_t) 1, result.warnings.size(),
                              L"dropping assembled bytes is said out loud, which is the half that is ours");
        }



        //  The output-file directive stays in effect past a save, so a span
        //  after one still belongs to it. Reasoned from the manual's "already in
        //  effect" rather than measured; no vendor source mixes the two.
        TEST_METHOD (AnObjectFileDirectiveStaysInEffectPastASave)
        {
            AssemblyResult  result = Fixture::Assemble (" DSK OUTER\n ORG $300\n LDA #$11\n RTS\n SAV INNER\n"
                                                        " LDA #$22\n RTS\n");

            Assert::IsTrue (result.success, L"assembly should succeed");
            Assert::AreEqual ((size_t) 2, result.savePoints.size(), L"two outputs");

            Assert::AreEqual (std::string ("INNER"), result.savePoints[0].name,
                              L"the save names the span it ends");
            Assert::AreEqual (std::string ("OUTER"), result.savePoints[1].name,
                              L"and the directive still governs the next one");
            Assert::AreEqual ((size_t) 0, result.warnings.size(),
                              L"nothing was dropped, so nothing is warned about");
        }



        //  An ordinary assembly names nothing and still produces its object.
        TEST_METHOD (SourceWithNoDirectivesProducesOneUnnamedOutput)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n");

            Assert::AreEqual ((size_t) 1, result.savePoints.size(), L"one output");
            Assert::IsTrue (result.savePoints[0].name.empty(),
                            L"nothing named it, so the caller's name stands");
            Assert::AreEqual ((size_t) 0, result.warnings.size(), L"and nothing was dropped");
        }



        //  A save with no name is an error at its own line rather than a
        //  fallback. The fallback is what would let several saves resolve to one
        //  file, each writing over the last.
        TEST_METHOD (ASaveWithNoNameIsAnError)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n SAV\n");

            Assert::IsFalse (result.success, L"a save that names nothing cannot be honored");
            Assert::AreEqual ((size_t) 1, result.errors.size(), L"one error");
            Assert::AreEqual (4, result.errors[0].lineNumber, L"at the line that is missing the name");
        }



        //  Two outputs under one name would write one over the other, and the
        //  assembly would report success having produced one file where the
        //  source named two. Deliberately NOT the same case as a name already
        //  on the target from an earlier run, which is replaced.
        TEST_METHOD (TwoOutputsUnderOneNameIsRefusedNamingTheFile)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n SAV SAME\n"
                                                        " LDA #$22\n SAV SAME\n");

            Assert::IsFalse (result.success, L"one output would be written over the other");
            Assert::AreEqual ((size_t) 1, result.errors.size(), L"one error");
            Assert::IsTrue (result.errors[0].message.find ("SAME") != std::string::npos,
                            L"and it names the file that collided");
        }



        //  Three saves, to show the cutting is not a two-case special.
        TEST_METHOD (ThreeSavesProduceThreeOutputsEachHoldingItsOwnSpan)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n SAV ONE\n"
                                                        " LDA #$22\n SAV TWO\n"
                                                        " LDA #$33\n SAV THREE\n");

            Assert::AreEqual ((size_t) 3, result.savePoints.size(), L"three outputs");

            for (const SavePoint & span : result.savePoints)
            {
                Assert::AreEqual ((size_t) 2, span.bytes.size(), L"each holds only its own two bytes");
            }
        }
    };
}
