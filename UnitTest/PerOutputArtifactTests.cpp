#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"
#include "Cli/ArtifactWriter.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace PerOutputArtifactTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Fixture
    //
    //  Assembling with a listing, which is what these tests are about.
    //
    //  THE LISTING IS ASKED FOR HERE and nowhere else in the assembler tests,
    //  because line-to-output attribution is recorded only while a listing is
    //  being built. An assembly that wants no listing does not pay for it.
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
            options.dialect         = DialectId::Merlin;
            options.generateListing = true;

            Assembler  assembler (cpu.GetInstructionSet(), cmos.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        //  Whether a listing shows the given source line, by its number.
        static bool Shows (const AssemblyResult & result, int lineNumber)
        {
            bool  found = false;

            for (const AssemblyLine & line : result.listing)
            {
                found = found || (line.lineNumber == lineNumber);
            }

            return found;
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ArtifactNameTests
    //
    //  What an output's listing and debug file are called.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ArtifactNameTests)
    {
    public:

        TEST_METHOD (TheExtensionIsReplacedRatherThanAppended)
        {
            Assert::AreEqual (std::string ("prog.lst"),
                              ArtifactWriter::ResolveArtifactName ("prog.bin", ".lst"));
        }



        TEST_METHOD (ANameWithNoExtensionGainsOne)
        {
            Assert::AreEqual (std::string ("LOADER.lst"),
                              ArtifactWriter::ResolveArtifactName ("LOADER", ".lst"));
        }



        //  A dot in a directory above the file is not the file's extension, so
        //  cutting at the last dot in the whole path would truncate the
        //  directory and write somewhere else entirely.
        TEST_METHOD (ADotInADirectoryIsNotAnExtension)
        {
            Assert::AreEqual (std::string ("build.out/prog.lst"),
                              ArtifactWriter::ResolveArtifactName ("build.out/prog", ".lst"));
            Assert::AreEqual (std::string ("build.out\\prog.lst"),
                              ArtifactWriter::ResolveArtifactName ("build.out\\prog", ".lst"));
        }



        TEST_METHOD (AnExtensionUnderneathASuchDirectoryIsStillReplaced)
        {
            Assert::AreEqual (std::string ("build.out/prog.dbg"),
                              ArtifactWriter::ResolveArtifactName ("build.out/prog.bin", ".dbg"));
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ForOutputTests
    //
    //  How an assembly's listing and symbols are divided among the outputs it
    //  produced.
    //
    //  THE SOURCE BELOW IS THE ONE CASE THAT MATTERS: an equate above both
    //  outputs, a label inside each, and two saves. Every rule the split has is
    //  visible in it.
    //
    //      1  CONST  EQU $12
    //      2         ORG $300
    //      3  FIRST  LDA #CONST
    //      4         RTS
    //      5         SAV LOADER
    //      6         ORG $6000
    //      7  SECOND LDA #$22
    //      8         RTS
    //      9         SAV MAIN
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ForOutputTests)
    {
    public:

        static std::string TwoOutputSource()
        {
            return "CONST  EQU $12\n"
                   "       ORG $300\n"
                   "FIRST  LDA #CONST\n"
                   "       RTS\n"
                   "       SAV LOADER\n"
                   "       ORG $6000\n"
                   "SECOND LDA #$22\n"
                   "       RTS\n"
                   "       SAV MAIN\n";
        }



        TEST_METHOD (EachOutputCarriesOnlyItsOwnCode)
        {
            AssemblyResult  result = Fixture::Assemble (TwoOutputSource());
            AssemblyResult  first  = ArtifactWriter::ForOutput (result, 0);
            AssemblyResult  second = ArtifactWriter::ForOutput (result, 1);

            Assert::AreEqual ((size_t) 2, result.savePoints.size(), L"two outputs");

            Assert::IsTrue  (Fixture::Shows (first, 3),  L"the first output shows its own code");
            Assert::IsFalse (Fixture::Shows (first, 7),  L"and not the second output's");

            Assert::IsTrue  (Fixture::Shows (second, 7), L"the second output shows its own code");
            Assert::IsFalse (Fixture::Shows (second, 3), L"and not the first output's");
        }



        //  A file missing the definitions its code refers to does not stand
        //  alone, and standing alone is the whole point of splitting.
        TEST_METHOD (WhatSitsAboveTheFirstOutputGoesIntoEveryOne)
        {
            AssemblyResult  result = Fixture::Assemble (TwoOutputSource());

            Assert::IsTrue (Fixture::Shows (ArtifactWriter::ForOutput (result, 0), 1),
                            L"the shared equate is in the first");
            Assert::IsTrue (Fixture::Shows (ArtifactWriter::ForOutput (result, 1), 1),
                            L"and in the second, rather than only in the first");
        }



        //  THE LINE THAT CLOSES AN OUTPUT BELONGS TO IT, not to the one it
        //  opens. The attribution is taken before the line is emitted for
        //  exactly this reason: a save closes its span while it runs, so asking
        //  afterwards files every save one output too late.
        TEST_METHOD (ASaveIsShownInTheOutputItEnds)
        {
            AssemblyResult  result = Fixture::Assemble (TwoOutputSource());

            Assert::IsTrue  (Fixture::Shows (ArtifactWriter::ForOutput (result, 0), 5),
                             L"SAV LOADER is in LOADER's listing");
            Assert::IsFalse (Fixture::Shows (ArtifactWriter::ForOutput (result, 1), 5),
                             L"and not in MAIN's");
        }



        //  Two outputs may begin at the same address, and an index built by
        //  address over all of them collides. Scoping the symbols is what makes
        //  the by-address half of a debug file answerable at all.
        TEST_METHOD (ASymbolBelongsToTheOutputItWasDefinedIn)
        {
            AssemblyResult  result = Fixture::Assemble (TwoOutputSource());
            AssemblyResult  first  = ArtifactWriter::ForOutput (result, 0);
            AssemblyResult  second = ArtifactWriter::ForOutput (result, 1);

            Assert::IsTrue  (first.symbols.count ("FIRST")   != 0, L"FIRST is in the first output");
            Assert::IsTrue  (first.symbols.count ("SECOND")  == 0, L"and SECOND is not");

            Assert::IsTrue  (second.symbols.count ("SECOND") != 0, L"SECOND is in the second output");
            Assert::IsTrue  (second.symbols.count ("FIRST")  == 0, L"and FIRST is not");
        }



        TEST_METHOD (ASymbolDefinedAboveBothOutputsIsInBoth)
        {
            AssemblyResult  result = Fixture::Assemble (TwoOutputSource());

            Assert::IsTrue (ArtifactWriter::ForOutput (result, 0).symbols.count ("CONST") != 0,
                            L"the shared equate is in the first");
            Assert::IsTrue (ArtifactWriter::ForOutput (result, 1).symbols.count ("CONST") != 0,
                            L"and in the second");
        }



        //  Bytes assembled after the last save reach no file and the assembly
        //  says so. The LISTING is a record of what was assembled, so they are
        //  shown rather than dropped.
        TEST_METHOD (LinesBelowTheLastOutputAreShownInTheLastListing)
        {
            AssemblyResult  result = Fixture::Assemble (" ORG $300\n LDA #$11\n SAV ONLY\n LDA #$22\n");

            Assert::AreEqual ((size_t) 1, result.savePoints.size(), L"one output, one trailing line");
            Assert::IsTrue   (Fixture::Shows (ArtifactWriter::ForOutput (result, 0), 4),
                              L"the dropped bytes still appear in the listing");
        }



        //  The bytes come from the save point, so a share is a whole assembly
        //  result a writer can be handed unchanged.
        TEST_METHOD (AShareCarriesItsOwnBytesAndLoadAddress)
        {
            AssemblyResult     result = Fixture::Assemble (TwoOutputSource());
            AssemblyResult     second = ArtifactWriter::ForOutput (result, 1);
            std::vector<Byte>  bytes  = { 0xA9, 0x22, 0x60 };

            Assert::IsTrue    (bytes == second.bytes, L"only the second output's bytes");
            Assert::AreEqual  ((int) 0x6000, (int) second.startAddress, L"at its own origin");
        }
    };


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SingleOutputIsUnchangedTests
    //
    //  The ordinary assembly, which is nearly every assembly, seeing none of this.
    //
    //  THE HAZARD THE SPLIT INTRODUCES RUNS THIS WAY. Every other test here
    //  checks that a source producing several outputs is divided correctly; the
    //  regression to fear is the division reaching a source that produces one,
    //  where there is nothing to divide and the shipped behavior is what callers
    //  already depend on. A listing that lost a line, or gained a second file,
    //  would be a change nobody asked for.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SingleOutputIsUnchangedTests)
    {
    public:

        static constexpr const char *  kOneOutput = "CONST  EQU $12\n"
                                                    "       ORG $300\n"
                                                    "FIRST  LDA #CONST\n"
                                                    "       RTS\n";



        TEST_METHOD (TheWholeListingIsInTheOneOutput)
        {
            AssemblyResult  result = Fixture::Assemble (kOneOutput);
            AssemblyResult  only   = ArtifactWriter::ForOutput (result, 0);

            Assert::AreEqual ((size_t) 1, result.savePoints.size(), L"one output");
            Assert::AreEqual (result.listing.size(), only.listing.size(),
                              L"and it holds every line the assembly listed, none dropped");
        }



        TEST_METHOD (EverySymbolIsInTheOneOutput)
        {
            AssemblyResult  result = Fixture::Assemble (kOneOutput);
            AssemblyResult  only   = ArtifactWriter::ForOutput (result, 0);

            Assert::AreEqual (result.symbols.size(), only.symbols.size(),
                              L"scoping cannot lose a symbol when there is nothing to scope against");
        }



        //  The bytes an existing caller reads are the ones they always read.
        //  `bytes` spans the whole assembly and the single save point spans the
        //  same thing, and a divergence between them would surface as an object
        //  file that changed size for no stated reason.
        TEST_METHOD (TheOnlySavePointHoldsWhatTheAssemblyHolds)
        {
            AssemblyResult  result = Fixture::Assemble (kOneOutput);

            Assert::IsTrue (result.bytes == result.savePoints[0].bytes,
                            L"the one output is the whole object");
            Assert::AreEqual ((int) result.startAddress, (int) result.savePoints[0].loadAddress,
                              L"at the address the assembly reports");
        }
    };
}
