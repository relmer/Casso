#include "Pch.h"

#include "MerlinCorpus/MerlinFixture.h"
#include "EmuTests/FixtureProvider.h"
#include "EhmTestHelper.h"
#include "TestHelpers.h"
#include "Assembler.h"
#include "DialectRegistry.h"
#include "DialectProfile.h"
#include "MerlinSubsetBoundary.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinSubsetBoundaryTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Fixture
    //
    //  Assembling a string of Merlin source, and the small amount of counting
    //  every test below does over the diagnostics that come back.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Fixture
    {
    public:

        static AssemblyResult Assemble (const std::string & source,
                                        DialectId           dialect = DialectId::Merlin)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = dialect;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        //  One vendor source through the real assembler. The answer is supplied
        //  for the keyboard-input line both linker-demo files open with, so the
        //  object-file directive inside its conditional stays unassembled and
        //  the only boundary constructs reached are the ones under test.
        static AssemblyResult AssembleFixture (const char * path)
        {
            FixtureProvider   provider;
            TestCpu           cpu;
            std::string       source;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect           = DialectId::Merlin;
            options.predefinedSymbols = { { "SAVOBJ", 0 } };

            AssertSucceeded (MerlinFixture::LoadSource (provider, path, source));

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        static std::vector<AssemblyError> Refusals (const AssemblyResult & result)
        {
            std::vector<AssemblyError>  refusals;

            for (const AssemblyError & error : result.errors)
            {
                if (error.kind == DiagnosticKind::SubsetBoundary)
                {
                    refusals.push_back (error);
                }
            }

            return refusals;
        }



        //  How many refusals quote a given sentence. The whole sentence rather
        //  than a word of it: "no workaround" and "remove REL" both appear in
        //  prose that means the opposite of the other, so a short probe would
        //  match either message.
        static size_t CountQuoting (const std::vector<AssemblyError> & refusals, const char * sentence)
        {
            size_t  count = 0;

            for (const AssemblyError & refusal : refusals)
            {
                if (refusal.message.find (sentence) != std::string::npos)
                {
                    count++;
                }
            }

            return count;
        }



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        //  The source that reaches one row's refusal, built from the row rather
        //  than written out per construct -- a hand-written list would cover the
        //  rows somebody remembered rather than the rows that exist.
        static std::string SourceFor (const SubsetBoundaryRow & row)
        {
            std::string  source = std::string (" ") + row.spelling + "\n";

            if (row.trigger == SubsetBoundaryTrigger::SecondOccurrence)
            {
                source += std::string (" ") + row.spelling + "\n";
            }

            return source;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BoundaryTableTests
    //
    //  The table itself, before anything reads it. A sweep over an empty span
    //  passes while checking nothing, and two rows sharing a token would make
    //  the lookup answer with whichever came first -- both are failures the
    //  tests below cannot see, because they would be sweeping the wrong thing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BoundaryTableTests)
    {
    public:

        TEST_METHOD (TheTableIsNotEmpty)
        {
            Assert::IsFalse (MerlinSubsetBoundary::GetAll().empty(),
                             L"an empty boundary makes every sweep below pass without checking anything");
        }



        TEST_METHOD (EveryRowIsFullyPopulated)
        {
            for (const SubsetBoundaryRow & row : MerlinSubsetBoundary::GetAll())
            {
                Assert::IsTrue (row.token != Directive::None,           L"a row with no token can never be reached");
                Assert::IsTrue (row.spelling != nullptr,                L"spelling");
                Assert::IsTrue (row.construct != nullptr,               L"construct");
                Assert::IsTrue (row.explanation != nullptr,             L"explanation");
                Assert::IsTrue (row.widensWith != nullptr,              L"what would widen the boundary");

                Assert::IsTrue (std::strlen (row.spelling) > 0,         L"spelling");
                Assert::IsTrue (std::strlen (row.construct) > 0,        L"construct");
                Assert::IsTrue (std::strlen (row.explanation) > 0,      L"explanation");
                Assert::IsTrue (std::strlen (row.widensWith) > 0,       L"what would widen the boundary");
            }
        }



        TEST_METHOD (NoTwoRowsShareAToken)
        {
            std::span<const SubsetBoundaryRow>  rows = MerlinSubsetBoundary::GetAll();

            for (size_t i = 0; i < rows.size(); i++)
            {
                for (size_t j = i + 1; j < rows.size(); j++)
                {
                    Assert::IsTrue (rows[i].token != rows[j].token,
                                    L"two rows for one token make the lookup answer with whichever comes first");
                }
            }
        }



        //  The inverse direction. The sweep below visits rows that exist and so
        //  structurally cannot notice a construct that was meant to be refused
        //  and has no row -- which is the shape that fails silently, since a
        //  token with no row and no handler is simply ignored.
        TEST_METHOD (EveryConstructRecognizedOnlyToBeRefusedHasARow)
        {
            const Directive  kRefusedByName[] = { Directive::Relocatable,
                                                  Directive::EntrySymbol,
                                                  Directive::ExternalSymbol,
                                                  Directive::FileType,
                                                  Directive::SaveObject };

            for (Directive token : kRefusedByName)
            {
                Assert::IsTrue (SubsetBoundary::Find (MerlinSubsetBoundary::GetAll(), token) != nullptr,
                                L"a construct recognized only so it can be refused, with nothing to refuse it");
            }
        }



        //  The mechanism is not built for exactly one dialect that refuses
        //  things. A profile that states no boundary must reach the refusal path
        //  never, and AS65 is the one in the tree that states none.
        TEST_METHOD (As65StatesNoBoundaryAndMerlinDoes)
        {
            Assert::IsTrue (DialectRegistry::Get (DialectId::As65).GetSubsetBoundary().empty(),
                            L"AS65 refuses nothing by this route");

            Assert::IsFalse (DialectRegistry::Get (DialectId::Merlin).GetSubsetBoundary().empty(),
                             L"Merlin's profile must hand its own table through, or the comparison above is vacuous");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BoundaryRefusalSweepTests
    //
    //  Every row, driven through the real assembler. Derived from the accessor
    //  rather than from a list here, so a row added to the table is exercised
    //  without anyone editing this file -- and a row added without a refusal
    //  behind it fails the build instead of shipping.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BoundaryRefusalSweepTests)
    {
    public:

        TEST_METHOD (EveryRowProducesExactlyOneRefusalNamingItself)
        {
            std::span<const SubsetBoundaryRow>  rows = MerlinSubsetBoundary::GetAll();

            Assert::IsFalse (rows.empty(), L"nothing to sweep");

            for (const SubsetBoundaryRow & row : rows)
            {
                AssemblyResult              result   = Fixture::Assemble (Fixture::SourceFor (row));
                std::vector<AssemblyError>  refusals = Fixture::Refusals (result);
                std::wstring                what     = Fixture::Widen (row.spelling);

                Assert::AreEqual ((size_t) 1, refusals.size(), what.c_str());

                Assert::IsTrue (refusals[0].message.starts_with (std::string (row.spelling) + ":"),
                                what.c_str());
                Assert::IsTrue (refusals[0].message.find (row.construct)   != std::string::npos, what.c_str());
                Assert::IsTrue (refusals[0].message.find (row.explanation) != std::string::npos, what.c_str());
                Assert::IsTrue (refusals[0].message.find (row.widensWith)  != std::string::npos, what.c_str());

                //  The refused occurrence, which for a cumulative construct is
                //  the second line rather than the first.
                Assert::AreEqual ((row.trigger == SubsetBoundaryTrigger::SecondOccurrence) ? 2 : 1,
                                  refusals[0].lineNumber, what.c_str());

                Assert::IsFalse (result.success, what.c_str());
            }
        }



        //  The same sweep under AS65, which knows none of these spellings. It is
        //  the check that the refusal comes from the ACTIVE profile's table and
        //  not from the token being present in the shared vocabulary.
        TEST_METHOD (NoRowIsRefusedUnderADialectThatDoesNotStateTheBoundary)
        {
            for (const SubsetBoundaryRow & row : MerlinSubsetBoundary::GetAll())
            {
                AssemblyResult              result   = Fixture::Assemble (Fixture::SourceFor (row), DialectId::As65);
                std::vector<AssemblyError>  refusals = Fixture::Refusals (result);

                Assert::AreEqual ((size_t) 0, refusals.size(), Fixture::Widen (row.spelling).c_str());
            }
        }



        //  Every offender, not the first. A developer meeting this boundary is
        //  deciding whether the file can be ported at all, and one refusal per
        //  run turns that into as many runs as there are constructs.
        TEST_METHOD (EveryOffenderInTheSourceIsReported)
        {
            AssemblyResult              result   = Fixture::Assemble (" REL\n ENT\n ENT\n TYP\n SAV\n");
            std::vector<AssemblyError>  refusals = Fixture::Refusals (result);

            Assert::AreEqual ((size_t) 5, refusals.size(), L"one refusal per offending line");

            for (size_t i = 0; i < refusals.size(); i++)
            {
                Assert::AreEqual ((int) i + 1, refusals[i].lineNumber, L"each refusal at its own line");
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BoundaryDistinctionTests
    //
    //  A refusal against a complaint about the source, and what an assembly
    //  stops doing once the boundary has been crossed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BoundaryDistinctionTests)
    {
    public:

        TEST_METHOD (ARefusalAndASyntaxErrorAreDifferentKinds)
        {
            AssemblyResult  refused = Fixture::Assemble (" REL\n");
            AssemblyResult  wrong   = Fixture::Assemble (" LDA #$12,,\n");

            Assert::AreEqual ((size_t) 1, refused.errors.size(), L"the refusal");
            Assert::IsTrue (refused.errors[0].kind == DiagnosticKind::SubsetBoundary,
                            L"a refused construct must not read as a broken one");

            Assert::IsFalse (wrong.errors.empty(), L"the malformed operand has to fail, or this proves nothing");

            for (const AssemblyError & error : wrong.errors)
            {
                Assert::IsTrue (error.kind == DiagnosticKind::SourceError,
                                L"a genuine source error must not claim to be a subset boundary");
            }
        }



        //  Nothing after the boundary runs. The refusals ARE the answer, and the
        //  cascade an unsupported construct produces -- entry symbols a linker
        //  would have resolved are simply undefined -- would bury them.
        TEST_METHOD (CrossingTheBoundaryStopsTheAssemblyBeforeItComplainsAboutTheRest)
        {
            AssemblyResult  result = Fixture::Assemble (" REL\n LDA NEVERDEFINED\n");

            Assert::AreEqual ((size_t) 1, result.errors.size(),
                              L"the undefined symbol pass 2 would report is noise beside the refusal");

            Assert::IsTrue (result.errors[0].kind == DiagnosticKind::SubsetBoundary, L"the one error is the refusal");
            Assert::IsTrue (result.bytes.empty(), L"a refused assembly produces no object");
        }



        //  An entry symbol is written as a label on the refused line, and the
        //  same name recurs across a module. The refusal is the whole answer
        //  for that line: binding the label as well would add a duplicate-label
        //  complaint about a construct that was already declined.
        TEST_METHOD (ALabelOnARefusedLineAddsNoSecondComplaint)
        {
            AssemblyResult  result = Fixture::Assemble ("GO ENT\nGO ENT\n");

            Assert::AreEqual ((size_t) 2, result.errors.size(), L"one refusal per line and nothing else");

            for (const AssemblyError & error : result.errors)
            {
                Assert::IsTrue (error.kind == DiagnosticKind::SubsetBoundary, L"nothing but the refusals");
            }
        }



        //  And a repeated label OUTSIDE the boundary still is a complaint, or
        //  the test above passes because nothing was ever going to object.
        TEST_METHOD (ARepeatedLabelOnAnOrdinaryLineStillObjects)
        {
            AssemblyResult  result = Fixture::Assemble ("GO NOP\nGO NOP\n");

            Assert::IsFalse (result.errors.empty(), L"a duplicate label has to be reported where it is not refused");
        }



        //  And the same source without the boundary construct DOES report it,
        //  or the test above passes because nothing was ever going to complain.
        TEST_METHOD (TheSameSourceWithoutTheRefusedConstructReportsItsUndefinedSymbol)
        {
            AssemblyResult  result = Fixture::Assemble (" LDA NEVERDEFINED\n");

            Assert::IsFalse (result.errors.empty(), L"pass 2 has to reach the undefined symbol here");

            for (const AssemblyError & error : result.errors)
            {
                Assert::IsTrue (error.kind == DiagnosticKind::SourceError, L"nothing here is a boundary refusal");
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CpuSelectionBoundaryTests
    //
    //  The one construct that is inside the subset once and outside it twice.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CpuSelectionBoundaryTests)
    {
    public:

        TEST_METHOD (OneCpuSelectionIsNotRefused)
        {
            AssemblyResult  result = Fixture::Assemble (" XC\n");

            Assert::AreEqual ((size_t) 0, Fixture::Refusals (result).size(),
                              L"the first selection reaches the 65C02, which Casso emulates");
        }



        TEST_METHOD (TheSecondCpuSelectionIsRefusedAsAnUnemulatedProcessor)
        {
            AssemblyResult              result   = Fixture::Assemble (" XC\n XC\n");
            std::vector<AssemblyError>  refusals = Fixture::Refusals (result);

            Assert::AreEqual ((size_t) 1, refusals.size(), L"the second selection, and only the second");
            Assert::AreEqual (2, refusals[0].lineNumber, L"reported at the second occurrence, not the first");

            Assert::IsTrue (refusals[0].message.find ("65802/65816") != std::string::npos,
                            L"the refusal has to name the processor it would have selected");
        }



        TEST_METHOD (AThirdCpuSelectionIsRefusedToo)
        {
            AssemblyResult  result = Fixture::Assemble (" XC\n XC\n XC\n");

            Assert::AreEqual ((size_t) 2, Fixture::Refusals (result).size(),
                              L"every occurrence past the first is outside the subset");
        }



        //  An occurrence inside a false conditional was never assembled, so it
        //  crossed no boundary and must not count toward the next one either.
        TEST_METHOD (ASkippedOccurrenceIsNeitherRefusedNorCounted)
        {
            AssemblyResult  result = Fixture::Assemble ("GATE  EQU 0\n DO GATE\n XC\n XC\n FIN\n XC\n");

            Assert::AreEqual ((size_t) 0, Fixture::Refusals (result).size(),
                              L"only the one live occurrence was assembled");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DeferredCapabilityRefusalTests
    //
    //  The two constructs refused for reasons that are not the linker, and whose
    //  messages must not be confused with each other. One is waiting on a
    //  capability Casso is building; the other is waiting on a decision nobody
    //  has taken, and saying it waits on the first would be false.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DeferredCapabilityRefusalTests)
    {
    public:

        TEST_METHOD (TheFileTypeDirectiveIsRefusedAsBelongingToDiskFileAccess)
        {
            std::vector<AssemblyError>  refusals = Fixture::Refusals (Fixture::Assemble (" TYP $06\n"));

            Assert::AreEqual ((size_t) 1, refusals.size(), L"the file-type directive");

            Assert::IsTrue (refusals[0].message.find ("filesystem file type") != std::string::npos,
                            L"the refusal has to say what the directive sets");
            Assert::IsTrue (refusals[0].message.find ("disk file-access") != std::string::npos,
                            L"and where the capability it needs is being built");
        }



        TEST_METHOD (TheSaveObjectDirectiveIsRefusedAsAnUndecidedQuestionRatherThanAWait)
        {
            std::vector<AssemblyError>  refusals = Fixture::Refusals (Fixture::Assemble (" SAV OUT\n"));

            Assert::AreEqual ((size_t) 1, refusals.size(), L"the save-object directive");

            Assert::IsTrue (refusals[0].message.find ("several outputs") != std::string::npos,
                            L"the refusal has to say what makes this its own question");
            Assert::IsTrue (refusals[0].message.find ("disk file access will not settle") != std::string::npos,
                            L"and deny the reading that it is merely waiting on file access");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RelocatableWorkaroundTests
    //
    //  The two relocatable refusals, and the vendor sources that produce one
    //  each. The distinction is not decoration: a module that publishes symbols
    //  and imports none assembles once the relocatable constructs are removed
    //  and an origin supplied, and a module that imports one cannot be made to,
    //  because the definition lives in a file this one never sees.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RelocatableWorkaroundTests)
    {
    public:

        static constexpr const char *  kFixLine    = "remove REL";
        static constexpr const char *  kNoFixLine  = "there is no workaround";



        TEST_METHOD (AModuleThatOnlyExportsIsToldHowToAssembleItAbsolutely)
        {
            std::vector<AssemblyError>  refusals = Fixture::Refusals (Fixture::Assemble (" REL\nGO ENT\n"));

            Assert::AreEqual ((size_t) 2, refusals.size(), L"the relocatable directive and the entry symbol");
            Assert::AreEqual ((size_t) 2, Fixture::CountQuoting (refusals, kFixLine),   L"both carry the fix");
            Assert::AreEqual ((size_t) 0, Fixture::CountQuoting (refusals, kNoFixLine), L"and neither denies one");
        }



        TEST_METHOD (OneExternalSymbolRemovesTheFixFromEveryRefusalInTheModule)
        {
            std::vector<AssemblyError>  refusals = Fixture::Refusals (Fixture::Assemble (" REL\nGO ENT\nOTHER EXT\n"));

            Assert::AreEqual ((size_t) 3, refusals.size(), L"three offending lines");
            Assert::AreEqual ((size_t) 0, Fixture::CountQuoting (refusals, kFixLine),
                              L"a fix that cannot work must not be offered");
            Assert::AreEqual ((size_t) 2, Fixture::CountQuoting (refusals, kNoFixLine),
                              L"the two refusals that would have carried a fix say why there is none");
        }



        //  The external declaration appears BELOW the refusals it changes, so a
        //  message composed where the construct was met would have offered the
        //  fix to the two lines above it and then contradicted itself.
        TEST_METHOD (AnExternalSymbolBelowTheOthersStillRemovesTheFix)
        {
            std::vector<AssemblyError>  refusals = Fixture::Refusals (Fixture::Assemble (" REL\nGO ENT\nOTHER EXT\n"));
            std::vector<AssemblyError>  above    = Fixture::Refusals (Fixture::Assemble ("OTHER EXT\n REL\nGO ENT\n"));

            Assert::AreEqual (Fixture::CountQuoting (refusals, kNoFixLine),
                              Fixture::CountQuoting (above, kNoFixLine),
                              L"where the external declaration sits cannot change what the others say");
        }



        TEST_METHOD (TheVendorExportOnlyModuleGetsTheActionableRefusal)
        {
            std::vector<AssemblyError>  refusals = Fixture::Refusals (Fixture::AssembleFixture ("Merlin/PI.ADD.S"));

            Assert::AreEqual ((size_t) 7, refusals.size(),
                              L"one relocatable directive and six entry symbols");
            Assert::AreEqual ((size_t) 7, Fixture::CountQuoting (refusals, kFixLine),
                              L"nothing in this module imports, so every refusal names the way forward");
            Assert::AreEqual ((size_t) 0, Fixture::CountQuoting (refusals, kNoFixLine),
                              L"and none of them denies one");
        }



        TEST_METHOD (TheVendorImportingModuleGetsTheRefusalWithNoWayForward)
        {
            std::vector<AssemblyError>  refusals = Fixture::Refusals (Fixture::AssembleFixture ("Merlin/PI.START.S"));

            Assert::AreEqual ((size_t) 5, refusals.size(),
                              L"one relocatable directive, three external symbols and one entry symbol");
            Assert::AreEqual ((size_t) 0, Fixture::CountQuoting (refusals, kFixLine),
                              L"this module imports, so the fix cannot work and must not be offered");
            Assert::AreEqual ((size_t) 2, Fixture::CountQuoting (refusals, kNoFixLine),
                              L"the relocatable directive and the entry symbol say why there is none");
        }



        //  The two vendor files are the same project and must not produce the
        //  same message, which is the whole reason both are committed.
        TEST_METHOD (TheTwoVendorModulesDoNotGetTheSameRelocatableRefusal)
        {
            std::vector<AssemblyError>  exporting = Fixture::Refusals (Fixture::AssembleFixture ("Merlin/PI.ADD.S"));
            std::vector<AssemblyError>  importing = Fixture::Refusals (Fixture::AssembleFixture ("Merlin/PI.START.S"));

            Assert::IsFalse (exporting.empty(), L"PI.ADD.S has to reach the boundary");
            Assert::IsFalse (importing.empty(), L"PI.START.S has to reach the boundary");

            Assert::AreNotEqual (exporting[0].message, importing[0].message,
                                 L"one project, two shapes, two messages");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BoundaryHelpTextTests
    //
    //  The help text against the table it is generated from. A row added without
    //  help coverage fails here rather than shipping, which is the property the
    //  generation exists for -- help and implementation cannot disagree by
    //  construction rather than by anyone noticing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BoundaryHelpTextTests)
    {
    public:

        //  The line of the help text that mentions a row's construct, or empty
        //  when no line does. Located by the construct rather than by the
        //  spelling, which is three characters and matches inside words.
        static std::string LineNaming (const std::string & help, const char * construct)
        {
            std::string  line;
            size_t       at    = help.find (construct);
            size_t       start = 0;
            size_t       end   = 0;

            if (at != std::string::npos)
            {
                start = help.rfind ('\n', at);
                start = (start == std::string::npos) ? 0 : start + 1;
                end   = help.find ('\n', at);
                end   = (end == std::string::npos) ? help.size() : end;
                line  = help.substr (start, end - start);
            }

            return line;
        }



        TEST_METHOD (EveryRowReachesTheHelpTextWholeAndOnOneLine)
        {
            std::string                         help = MerlinSubsetBoundary::GetHelpText();
            std::span<const SubsetBoundaryRow>  rows = MerlinSubsetBoundary::GetAll();

            Assert::IsFalse (rows.empty(), L"nothing to check the help text against");

            for (const SubsetBoundaryRow & row : rows)
            {
                std::string   line = LineNaming (help, row.construct);
                std::wstring  what = std::wstring (row.spelling, row.spelling + std::strlen (row.spelling));

                Assert::IsFalse (line.empty(), what.c_str());

                //  All four on the SAME line, so a help text that listed every
                //  spelling and every reason in two unrelated columns could not
                //  pass by holding both somewhere.
                Assert::IsTrue (line.find (row.spelling)    != std::string::npos, what.c_str());
                Assert::IsTrue (line.find (row.explanation) != std::string::npos, what.c_str());
                Assert::IsTrue (line.find (row.widensWith)  != std::string::npos, what.c_str());
            }
        }



        TEST_METHOD (TheHelpTextHasOneLinePerRowAndNoOthers)
        {
            std::string  help  = MerlinSubsetBoundary::GetHelpText();
            size_t       lines = 0;
            size_t       at    = help.find ("  ");

            while (at != std::string::npos)
            {
                lines++;
                at = help.find ("\n  ", at + 1);
            }

            Assert::AreEqual (MerlinSubsetBoundary::GetAll().size(), lines,
                              L"a listed construct with no row, or a row with no listing");
        }



        TEST_METHOD (TheHelpTextNamesTheReasonClassOfEveryRow)
        {
            std::string  help = MerlinSubsetBoundary::GetHelpText();

            Assert::IsTrue (help.find ("needs a linker")                     != std::string::npos, L"linker");
            Assert::IsTrue (help.find ("needs a CPU Casso does not emulate") != std::string::npos, L"cpu");
            Assert::IsTrue (help.find ("owned by another part of Casso")     != std::string::npos, L"another feature");
            Assert::IsTrue (help.find ("undecided")                          != std::string::npos, L"undecided");
        }
    };
}
