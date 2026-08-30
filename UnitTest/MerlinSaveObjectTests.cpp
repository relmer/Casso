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



        //  The two output directives are mutually exclusive, measured.
        //
        //  THIS TEST ASSERTED THE OPPOSITE and the opposite was invented. The
        //  code carried a rule for combining them -- the directive staying in
        //  effect past a save, governing the next span -- reasoned out because
        //  no vendor source mixes the two. Running it showed the period
        //  assembler answers `Bad "SAV"` and writes no second file: one streams
        //  the following code to disk and the other writes the buffer held in
        //  memory, and they are different mechanisms rather than two spellings.
        TEST_METHOD (ASaveIsRefusedOnceAnObjectFileDirectiveIsInEffect)
        {
            AssemblyResult  result = Fixture::Assemble (" DSK OUTER\n ORG $300\n LDA #$11\n RTS\n SAV INNER\n"
                                                        " LDA #$22\n RTS\n");

            Assert::IsFalse (result.success, L"the two cannot both be in play");
            Assert::AreEqual ((size_t) 1, result.errors.size(), L"one error");
            Assert::AreEqual (5, result.errors[0].lineNumber, L"at the save that cannot be honored");
            Assert::IsTrue (result.errors[0].message.find ("DSK") != std::string::npos,
                            L"and it names the directive already in effect");
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



        //  Two outputs under one name warn rather than refuse, measured.
        //
        //  THIS TEST ASSERTED A REFUSAL and measurement said otherwise. The
        //  period assembler writes both and lets the second overwrite the
        //  first, reporting no error: the disk ends with one file holding the
        //  second save's bytes. Refusing would leave no files where it leaves
        //  one, which the promise to produce the same files does not allow.
        //  The warning is what keeps the loss from being silent.
        TEST_METHOD (TwoOutputsUnderOneNameWarnAndTheLaterOneSurvives)
        {
            AssemblyResult     result = Fixture::Assemble (" ORG $300\n LDA #$11\n SAV SAME\n"
                                                           " LDA #$22\n SAV SAME\n");
            std::vector<Byte>  second = { 0xA9, 0x22 };

            Assert::IsTrue (result.success, L"the period assembler reports no error here");
            Assert::AreEqual ((size_t) 2, result.savePoints.size(), L"both saves happened");
            Assert::IsTrue (second == result.savePoints[1].bytes,
                            L"and the later one is what a caller writing in order leaves behind");

            Assert::AreEqual ((size_t) 1, result.warnings.size(), L"one warning");
            Assert::IsTrue (result.warnings[0].message.find ("SAME") != std::string::npos,
                            L"naming the file that will not survive");
        }



        //  A type belongs to the output it precedes, not to the assembly.
        //
        //  THIS WAS WRONG AND ONLY AN END-TO-END RUN SHOWED IT. The directive
        //  was handled in the pass that sizes lines, so the last type the
        //  source stated reached back and retyped every earlier output too --
        //  invisible with one output, which is all the tests had.
        TEST_METHOD (AFileTypeAppliesOnlyToTheOutputItPrecedes)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n SAV LOADER\n"
                                                        " ORG $6000\n LDA #$22\n RTS\n TYP $04\n SAV MAIN\n");

            Assert::AreEqual ((size_t) 2, result.savePoints.size(), L"two outputs");

            Assert::IsFalse (result.savePoints[0].hasFileType,
                             L"nothing stated a type before the first save");
            Assert::IsTrue (result.savePoints[1].hasFileType,
                            L"and the second output has the one stated above it");
            Assert::AreEqual ((int) 0x04, (int) result.savePoints[1].fileType, L"which is the type given");
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
