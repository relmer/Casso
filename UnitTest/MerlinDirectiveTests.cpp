#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "MockFileReader.h"
#include "MerlinCorpus/MerlinFixture.h"
#include "EmuTests/FixtureProvider.h"
#include "EhmTestHelper.h"
#include "Assembler.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinDirectiveTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinAssemblyFixture
    //
    //  One assembly of a Merlin source, so every test below reads as source in
    //  and bytes out.
    //
    //  The AS65 counterpart is here for the same reason the corpus carries a
    //  discriminates flag: labels, expressions and the evaluator are shared, so a
    //  test that passes under both dialects has not shown the Merlin profile was
    //  consulted at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MerlinAssemblyFixture
    {
    public:
        static AssemblyResult Assemble (const std::string & source, DialectId dialect)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = dialect;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        static AssemblyResult AssembleMerlin (const std::string & source)
        {
            return Assemble (source, DialectId::Merlin);
        }



        //  A Merlin assembly with answers supplied for the symbols its
        //  keyboard-input lines name. Merlin asks the operator; a batch assembly
        //  is told, through the same predefined symbols every other externally
        //  supplied value arrives on.
        static AssemblyResult AssembleMerlinWithAnswers (
            const std::string & source,
            const std::unordered_map<std::string, int32_t> & answers)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect           = DialectId::Merlin;
            options.predefinedSymbols = answers;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        //  A Merlin assembly with a second instruction table to switch to, for
        //  the in-source CPU selection. Two tables is the whole point: with one,
        //  the provider honestly answers the base table for both and the
        //  directive has nothing to select.
        static AssemblyResult AssembleMerlinWithExtendedSet (const std::string & source)
        {
            TestCpu           cpu;
            TestCpu65C02      cmos;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = DialectId::Merlin;

            Assembler  assembler (cpu.GetInstructionSet(), cmos.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        //  A Merlin assembly reading its included files from memory, so an
        //  inclusion test states which NAME was asked for rather than arranging
        //  files on a disk.
        static AssemblyResult AssembleMerlinWithReader (const std::string & source,
                                                        MockFileReader    & reader,
                                                        DialectId           dialect = DialectId::Merlin)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect    = dialect;
            options.fileReader = &reader;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        //  A Merlin assembly whose caller has already named the output, which is
        //  the half of the precedence rule no source can express.
        static AssemblyResult AssembleMerlinWithOutputName (const std::string & source,
                                                            const std::string & outputFileName)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect        = DialectId::Merlin;
            options.outputFileName = outputFileName;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        //  One committed vendor macro library as source text, decoded from the
        //  bytes the distribution disk holds. Both libraries are stored as type-T
        //  files, which DOS 3.3 gives no header at all -- so the type-B path would
        //  eat four characters of real text, and the entry point is chosen rather
        //  than sniffed for that reason.
        static std::string LoadFixtureTextSource (const char * path)
        {
            FixtureProvider  provider;
            std::string      source;

            AssertSucceeded (MerlinFixture::LoadTextSource (provider, path, source));

            return source;
        }



        //  The first diagnostic, so a failing assertion names what the assembler
        //  objected to rather than only that it objected.
        static std::wstring FirstDiagnostic (const AssemblyResult & result)
        {
            std::string   text;
            std::wstring  wide;

            if (!result.errors.empty())
            {
                text = "line " + std::to_string (result.errors[0].lineNumber) + ": " + result.errors[0].message;
            }

            wide.assign (text.begin(), text.end());

            return wide;
        }



        static bool AnyErrorMentions (const AssemblyResult & result, const std::string & fragment)
        {
            bool  found = false;

            for (const AssemblyError & error : result.errors)
            {
                if (error.message.find (fragment) != std::string::npos)
                {
                    found = true;
                    break;
                }
            }

            return found;
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinStringDirectiveTests
    //
    //  The string family through the whole assembler, not through the encoder
    //  alone. What is new here is that pass 1 must SIZE the line and pass 2 must
    //  emit it, and a disagreement between the two moves every label after it --
    //  a failure the encoder on its own cannot have.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinStringDirectiveTests)
    {
    public:

        TEST_METHOD (AscEmitsHighAsciiForAQuotedString)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" ASC \"AB\"\n");
            std::vector<Byte>  expected = { 0xC1, 0xC2 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"a quote delimiter selects high ASCII");
        }



        TEST_METHOD (DciInvertsTheLastCharactersHighBit)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DCI \"AB\"\n");
            std::vector<Byte>  expected = { 0xC1, 0x42 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"DCI marks its end by flipping the last byte's high bit");
        }



        TEST_METHOD (RevEmitsItsTextBackToFront)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" REV \"AB\"\n");
            std::vector<Byte>  expected = { 0xC2, 0xC1 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"REV reverses the text before encoding it");
        }



        //  The delimiter is ANY character, and the vendor sources depend on it
        //  where a fixed quote set would fail. `!` produces the same high ASCII
        //  as `"`, which is what rules out any rule ordering delimiters by value.
        TEST_METHOD (AnyCharacterMayDelimitTheText)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" ASC !A\"B!\n");
            std::vector<Byte>  expected = { 0xC1, 0xA2, 0xC2 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the bang delimits, and the quote inside it is payload");
        }



        //  Pass 1 sizes the line by encoding it, so a label after a string binds
        //  to the address the string actually ends at. A character count would
        //  agree here and disagree for a length-prefixed string, which is why the
        //  size comes from the encoder rather than from strlen.
        TEST_METHOD (ALabelAfterAStringBindsPastItsBytes)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin ("START ASC \"ABCD\"\nAFTER BRK\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x8000, (int) result.symbols.at ("START"), L"assembly starts at Merlin's default origin");
            Assert::AreEqual (0x8004, (int) result.symbols.at ("AFTER"), L"and four characters moved the PC by four");
        }



        //  A trailing byte after the closing delimiter used to be REFUSED,
        //  because nothing reachable then pinned what the digits meant. `MAKE
        //  DUMP`'s object settled it -- hexadecimal, emitted verbatim -- and the
        //  encodings are pinned in MerlinRawDataTests. What survives here is the
        //  SIZING half: pass 1 must count the trailing bytes too, or the next
        //  label binds two bytes early.
        TEST_METHOD (ALabelAfterAStringCountsItsTrailingBytesToo)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin ("START ASC \"AB\"0D\nAFTER BRK\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x8003, (int) result.symbols.at ("AFTER"),
                              L"two characters plus one trailing byte is three");
        }



        TEST_METHOD (AnUnterminatedStringIsAnError)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" ASC \"AB\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "Unterminated string"),
                            L"a string with no closing delimiter must be reported");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinErrorDirectiveTests
    //
    //  ERR, Merlin's assembly-time assertion: a NON-ZERO expression fails the
    //  assembly, and a zero one is silent.
    //
    //  Both directions are tested because only the pair distinguishes a working
    //  implementation from one that never fires -- and the vendor corpus contains
    //  only the silent case, since a source shipping an object necessarily
    //  assembled clean.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinErrorDirectiveTests)
    {
    public:

        TEST_METHOD (ANonZeroExpressionFailsTheAssembly)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" ERR 1\n");

            Assert::IsFalse (result.success, L"a non-zero ERR expression must fail the assembly");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "assertion failed"),
                            L"and must read as an assertion rather than as a syntax error");
        }



        TEST_METHOD (AZeroExpressionIsSilent)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" ERR 0\n BRK\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual ((size_t) 1, result.bytes.size(), L"ERR itself emits nothing");
        }



        //  The expression is evaluated in pass 2, so it may name a label defined
        //  after it. That is the whole reason ERR is worth having: the assertions
        //  people write bound a table by the distance between its own two ends.
        TEST_METHOD (TheExpressionMayNameAForwardLabel)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         "TOP ASC \"AB\"\nEND BRK\n ERR END-TOP-4\n");

            Assert::IsFalse (result.success, L"END-TOP is 2, so END-TOP-4 is non-zero and must fire");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "assertion failed"),
                            L"a forward reference must resolve rather than report as unresolvable");
        }



        //  The second form. A leading backslash makes the operand a CEILING
        //  rather than a condition: the assertion fires when the assembly has
        //  grown past it. `MAKE DUMP.S` bounds all three of its sections that way.
        TEST_METHOD (TheAddressCheckFormFiresWhenTheAssemblyOverruns)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " ORG $300\n HEX 0102030405\n ERR \\$303\n");

            Assert::IsFalse (result.success, L"five bytes from $300 reach $305, which is past $303");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "assertion failed"),
                            L"and it is the same assertion, not a syntax error about the backslash");
        }



        TEST_METHOD (TheAddressCheckFormIsSilentWhenItFits)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " ORG $300\n HEX 0102030405\n ERR \\$400\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual ((size_t) 5, result.bytes.size(), L"and the assertion itself emits nothing");
        }



        //  The ceiling is the WHOLE operand. Merlin folds left to right, so
        //  without grouping this reads as "did it overrun $300, plus five" -- a
        //  comparison result with something added to it, which is non-zero and
        //  fires on a file that fits exactly.
        TEST_METHOD (TheAddressCheckCeilingIsTheWholeExpression)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " ORG $300\n HEX 0102030405\n ERR \\$300+5\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual ((size_t) 5, result.bytes.size(), L"$305 is not past $305");
        }



        //  The address it checks is the PROGRAM COUNTER, which a relocating origin
        //  has moved away from where the bytes are landing. A ceiling measured
        //  against the output position would pass this and fail the vendor source.
        TEST_METHOD (TheAddressCheckMeasuresTheRelocatedProgramCounter)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " ORG $9000\n DFB $11\n ORG $300\n HEX 0102030405\n ERR \\$303\n");

            Assert::IsFalse (result.success, L"the section runs at $300 even though its bytes sit at $9001");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinLocalLabelTests
    //
    //  A leading colon scopes a label to the global label above it.
    //
    //  This is not a convenience. The vendor sources reuse `:OK`, `:RET` and
    //  `:EXIT` throughout -- CLOCK.S defines `:OK` twice in one file -- so without
    //  scoping every one of them is a duplicate-label error, and with scoping but
    //  no reference rewriting every USE of one is an unknown symbol.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinLocalLabelTests)
    {
    public:

        //  THE test. The same local name under two globals must be two symbols,
        //  and each reference must reach the nearer one.
        TEST_METHOD (TheSameLocalNameUnderTwoGlobalsIsTwoSymbols)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         "FIRST BRK\n"
                                         ":OK LDA :OK\n"
                                         "SECOND BRK\n"
                                         ":OK LDA :OK\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());

            //  FIRST at $8000, its :OK at $8001 (3 bytes), SECOND at $8004,
            //  its :OK at $8005.
            Assert::AreEqual (0x8001, (int) result.symbols.at ("FIRST.OK"),  L"the first local belongs to FIRST");
            Assert::AreEqual (0x8005, (int) result.symbols.at ("SECOND.OK"), L"and the second to SECOND");
        }



        //  The reference half. An operand naming a local must assemble to the
        //  address of the local in ITS OWN scope, not the other one.
        TEST_METHOD (AReferenceResolvesWithinItsOwnScope)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "FIRST BRK\n"
                                              ":OK LDA :OK\n"
                                              "SECOND BRK\n"
                                              ":OK LDA :OK\n");
            std::vector<Byte>  expected = { 0x00, 0xAD, 0x01, 0x80,
                                            0x00, 0xAD, 0x05, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"each LDA must reach the local in its own scope");
        }



        //  A local reference inside an expression, which is the shape the vendor
        //  sources actually use (`LDA :TABLE+5,X`). A rewrite that only handled a
        //  bare operand would leave this unresolvable.
        TEST_METHOD (ALocalMayBeReferencedInsideAnExpression)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "TOP BRK\n"
                                              ":TABLE BRK\n"
                                              " LDA :TABLE+5\n");
            std::vector<Byte>  expected = { 0x00, 0x00, 0xAD, 0x06, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the local must resolve as an operand of the expression");
        }



        //  A local with nothing to belong to is an error rather than a symbol in
        //  an unnamed scope, which would let two files' stray locals collide.
        TEST_METHOD (ALocalBeforeAnyGlobalIsRefused)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (":ORPHAN BRK\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "before any global label"),
                            L"a local label with no enclosing global must say so");
        }



        //  A colon inside string payload is data. `ASC ":::6::6:6:"` is on the
        //  vendor disk, and rewriting inside it would change emitted bytes rather
        //  than resolve a symbol.
        TEST_METHOD (AColonInsideStringPayloadIsNotALocalReference)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin ("TOP ASC \":AB\"\n");
            std::vector<Byte>  expected = { 0xBA, 0xC1, 0xC2 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the colon is payload, not the start of a local label");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinOperatorBindingTests
    //
    //  Merlin folds an expression strictly left to right. Parentheses are the
    //  only grouping.
    //
    //  The evidence is LABELS.S rather than the manual: it ends with
    //  `ERR END-LABTBL-1/$700`, bounding its own table at seven pages, and under
    //  ordinary precedence that reduces to the table's length and fires on a file
    //  the vendor shipped a working object for.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinOperatorBindingTests)
    {
    public:

        TEST_METHOD (AdditionAndMultiplicationFoldLeftToRight)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DFB 1+2*3\n");
            std::vector<Byte>  expected = { 9 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"(1+2)*3 is 9; precedence would give 7");
        }



        //  The same source under AS65, which DOES have precedence. Without this
        //  the test above cannot tell a working binding rule from an evaluator
        //  that happens to agree.
        TEST_METHOD (TheSameExpressionUnderAs65KeepsPrecedence)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::Assemble (" .byte 1+2*3\n", DialectId::As65);
            std::vector<Byte>  expected = { 7 };

            Assert::IsTrue (result.bytes == expected, L"AS65 must be unaffected: 1+(2*3) is 7");
        }



        TEST_METHOD (ParenthesesStillGroup)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DFB 1+(2*3)\n");
            std::vector<Byte>  expected = { 7 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"parentheses are the only grouping left to right allows");
        }



        //  LABELS.S's own assertion, in miniature: a difference divided by a page
        //  count. Under precedence the division binds first and the whole
        //  expression collapses to the difference, which is the failure this rule
        //  was discovered by.
        TEST_METHOD (TheVendorAssertionShapeFoldsToZero)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "TOP BRK\n"
                                              "END BRK\n"
                                              " DFB END-TOP-1/$700\n");
            std::vector<Byte>  expected = { 0x00, 0x00, 0x00 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"(END-TOP-1)/$700 is 0; precedence would give END-TOP");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinDefaultOriginTests
    //
    //  Where an assembly starts when the source names no origin. Half of
    //  "byte-identical" is the address the bytes load at, and a wrong default
    //  produces perfect bytes in the wrong place.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinDefaultOriginTests)
    {
    public:

        TEST_METHOD (MerlinStartsAtItsOwnDefaultWithNoOrigin)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" BRK\n");

            Assert::AreEqual (0x8000, (int) result.startAddress, L"Merlin assembles to $8000 with no ORG");
        }



        TEST_METHOD (As65StillStartsAtZero)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (" brk\n", DialectId::As65);

            Assert::AreEqual (0x0000, (int) result.startAddress, L"AS65's default must be unchanged");
        }



        TEST_METHOD (AnExplicitOriginOverridesTheDefault)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" ORG $300\n BRK\n");

            Assert::AreEqual (0x0300, (int) result.startAddress, L"a source naming an origin decides its own");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinOriginSemanticTests
    //
    //  What an origin directive moves. Merlin's relocates the program counter and
    //  leaves the output cursor alone, so a source assembling several sections at
    //  several addresses still ships as one contiguous stream. AS65's seeks, and
    //  the gap between two sections becomes fill.
    //
    //  Every test here has an AS65 counterpart, because the two dialects share
    //  the directive and differ only in what it does -- so a Merlin-only test
    //  would pass just as happily against an engine that never asked.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinOriginSemanticTests)
    {
    public:

        //  Three bytes written at three different addresses, arriving as three
        //  consecutive bytes. The label values are the other half: HERE is where
        //  its section SITS in the file, AFTER is where the resync put the
        //  program counter back to, and the byte between them ran at $0300.
        TEST_METHOD (AnOriginRelocatesWithoutMovingTheOutput)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $9000\n DFB $11\n"
                                              "HERE ORG $300\n DFB $22\n"
                                              " ORG\nAFTER DFB $33\n");
            std::vector<Byte>  expected = { 0x11, 0x22, 0x33 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the output stays contiguous across both origins");
            Assert::AreEqual (0x9000, (int) result.startAddress, L"and loads where the first origin put it");
        }



        //  The same source under AS65, whose origin seeks. It cannot produce
        //  three bytes: the second section lands 36 KB below the first and the
        //  image spans the gap.
        TEST_METHOD (As65SeeksAndLeavesTheGapBehind)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         " .org $9000\n .byte $11\n"
                                         " .org $300\n .byte $22\n", DialectId::As65);

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes.size() > 3,
                            L"a seeking origin writes into an address-indexed image, so the gap is in the output");
            Assert::AreEqual (0x0300, (int) result.startAddress, L"and the image begins at the lowest address written");
        }



        //  The first origin places the image, and a directive that reserved
        //  NOTHING must not be what stops it. Otherwise a zero-length
        //  reservation above the origin silently moves the whole object to the
        //  dialect's default address -- perfect bytes, wrong place.
        TEST_METHOD (ADirectiveReservingNothingDoesNotCountAsOutput)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DS 0\n ORG $9000\n DFB $11\n");
            std::vector<Byte>  expected = { 0x11 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"nothing was reserved, so nothing was emitted before the origin");
            Assert::AreEqual (0x9000, (int) result.startAddress, L"and the origin still decides where the image begins");
        }



        //  A bare origin resyncs the program counter to where output has actually
        //  reached. `MAKE DUMP.S` writes it twice, commenting both as recalling
        //  the real load address, and it is meaningless unless the two can drift.
        TEST_METHOD (ABareOriginResyncsTheProgramCounterToTheOutput)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " ORG $9000\n DFB $11\n"
                                         " ORG $300\n DFB $22\n"
                                         " ORG\nAFTER DFB $33\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x9002, (int) result.symbols.at ("AFTER"),
                              L"the resync must name the file position, not the address the last section ran at");
        }



        //  Nothing to resync to where the two cursors are the same thing, so AS65
        //  keeps reporting a missing operand rather than quietly accepting a
        //  no-op that reads as though it did something.
        TEST_METHOD (As65StillRefusesAnOriginWithNoOperand)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (" .org\n .byte $11\n", DialectId::As65);

            Assert::IsFalse (result.success, L"an operandless .org must stay an error under AS65");
        }



        //  A label sharing a line with an origin binds where the line was
        //  REACHED -- the program counter before the directive acted, which is
        //  exactly where a label on any other line binds. `MAKE DUMP`'s loader
        //  copies its interface section to page 3 and needs to know where that
        //  section sits in the file it was loaded from, so binding the label to
        //  the relocated address would be silently wrong and still assemble.
        TEST_METHOD (ALabelOnAnOriginLineBindsWhereTheLineWasReached)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " ORG $9000\n DFB $11\n"
                                         "HERE ORG $300\n DFB $22\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x9001, (int) result.symbols.at ("HERE"),
                              L"HERE is where the relocated section sits in the file, not where it runs");
        }



        //  THE case that tells the two candidate rules apart, and the corpus
        //  settles it. A label on a BARE origin sits where the program counter
        //  had reached -- the end of the relocated section -- not where output
        //  had reached, and those are different numbers only here.
        //
        //  CLOCK.S is the oracle: `IRQEND ORG` closes a section relocated to
        //  $BFC8, and `LDY #IRQEND-IRQHAND-1` two dozen lines earlier assembles
        //  to $12 in the shipped object. Under the output-cursor reading the
        //  difference spans the relocation and the byte comes out $30.
        TEST_METHOD (ALabelOnABareOriginBindsToTheProgramCounterNotTheOutput)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $9000\n DFB $11\n"
                                              " ORG $300\n DFB $22\n"
                                              "HERE ORG\n DFB $33\n DA HERE\n");
            std::vector<Byte>  expected = { 0x11, 0x22, 0x33, 0x01, 0x03 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x0301, (int) result.symbols.at ("HERE"),
                              L"the resync had not happened yet when the label bound");
            Assert::IsTrue (result.bytes == expected, L"and the value must reach the operand that uses it");
        }



        //  It has to BIND, which is the part that was missing: the origin claimed
        //  the line before the label stage ran, so a label there was dropped
        //  without a word and every use of it reported as undefined.
        TEST_METHOD (ALabelOnAnOriginLineIsUsableAsASymbol)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $9000\n DFB $11\n"
                                              "HERE ORG $300\n DFB $22\n"
                                              " ORG\n DA HERE\n");
            std::vector<Byte>  expected = { 0x11, 0x22, 0x01, 0x90 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the label must resolve where it is used, not only be recorded");
        }



        //  AS65 dropped such a label too, so this is a dialect-neutral fix rather
        //  than a Merlin one -- and the RULE is dialect-neutral as well. A label
        //  binds where its line was reached in either dialect; the origin acts
        //  afterwards, so what it does to the cursors cannot move the label.
        TEST_METHOD (As65BindsALabelOnAnOriginLineWhereTheLineWasReached)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         " .org $9000\n .byte $11\n"
                                         "HERE: .org $300\n .byte $22\n", DialectId::As65);

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x9001, (int) result.symbols.at ("HERE"),
                              L"the label marks where the line sat, not where the origin then went");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinOperandlessInstructionTests
    //
    //  Merlin writes accumulator mode by leaving the operand off entirely. AS65
    //  requires the register named, and treats a bare shift as a missing operand.
    //
    //  Which mnemonics have an accumulator encoding is the opcode table's answer,
    //  so a mnemonic that HAS an implied form must keep resolving to it -- that is
    //  the guard against a rule that swallows every operandless line.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinOperandlessInstructionTests)
    {
    public:

        TEST_METHOD (ABareShiftIsAccumulatorMode)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" LSR\n ASL\n ROL\n ROR\n");
            std::vector<Byte>  expected = { 0x4A, 0x0A, 0x2A, 0x6A };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"all four shifts take their accumulator opcode");
        }



        TEST_METHOD (AMnemonicWithAnImpliedFormStillTakesIt)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" NOP\n TAX\n INX\n");
            std::vector<Byte>  expected = { 0xEA, 0xAA, 0xE8 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the accumulator fallback must not displace an implied encoding");
        }



        TEST_METHOD (TheAccumulatorMayStillBeNamed)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" LSR A\n");
            std::vector<Byte>  expected = { 0x4A };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the explicit spelling is not withdrawn by accepting the bare one");
        }



        TEST_METHOD (As65StillRequiresTheAccumulatorNamed)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (" lsr\n", DialectId::As65);

            Assert::IsFalse (result.success, L"a bare shift must stay a missing operand under AS65");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "Missing operand"),
                            L"and must say so rather than failing some other way");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinCharacterConstantTests
    //
    //  Merlin spells a character constant two ways and they mean different bytes:
    //  the apostrophe form is the plain character and the double-quoted form is
    //  the same character in high ASCII, matching the convention its string
    //  directives take from their delimiter.
    //
    //  `MAKE DUMP.S` compares keystrokes and builds hexadecimal digits with the
    //  quoted form throughout -- `CMP #"N"`, `ORA #"0"` -- so its object is the
    //  oracle for which bit 7 is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinCharacterConstantTests)
    {
    public:

        TEST_METHOD (ADoubleQuotedCharacterIsHighAscii)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" LDA #\"A\"\n");
            std::vector<Byte>  expected = { 0xA9, 0xC1 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the quoted form sets bit 7");
        }



        TEST_METHOD (AnApostropheCharacterStaysLow)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" LDA #'A'\n");
            std::vector<Byte>  expected = { 0xA9, 0x41 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the two spellings must not collapse into one");
        }



        TEST_METHOD (AQuotedCharacterTakesPartInAnExpression)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" LDA #\"9\"+1\n");
            std::vector<Byte>  expected = { 0xA9, 0xBA };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"it is a value, not a whole-operand special case");
        }



        //  The instruction forms above are all settled in PASS 1, so they say
        //  nothing about whether pass 2 lexes the same way. A data directive is
        //  what discriminates: its arguments are evaluated where the bytes are
        //  emitted. Confirmed by mutation -- carrying the spelling into pass 1
        //  alone leaves every other test in this class green.
        //
        //  The `+1` is load-bearing rather than decoration. An argument that is
        //  ENTIRELY a quoted run is taken as string data before the evaluator
        //  ever sees it, so a bare `DA "A"` would exercise the wrong path.
        TEST_METHOD (AQuotedCharacterAlsoResolvesWhereTheBytesAreEmitted)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DA \"A\"+1\n");
            std::vector<Byte>  expected = { 0xC2, 0x00 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"both passes must read the same character spelling");
        }



        TEST_METHOD (As65DoesNotAcceptTheQuotedForm)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (" lda #\"A\"\n", DialectId::As65);

            Assert::IsFalse (result.success, L"admitting one dialect's spelling into another is what strictness forbids");
        }



        TEST_METHOD (As65KeepsTheApostropheForm)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::Assemble (" lda #'A'\n", DialectId::As65);
            std::vector<Byte>  expected = { 0xA9, 0x41 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the spelling AS65 already had must be untouched");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinByteSelectorTests
    //
    //  In Merlin the selector after the immediate sigil picks a byte out of the
    //  WHOLE expression. The shared evaluator's `<` and `>` are prefix operators
    //  binding to the term beside them, which is what AS65 means by them.
    //
    //  Settled from the object. `MAKE DUMP`'s loader stores the end of a section
    //  with `LDA #>HEREMAIN-1` and the shipped bytes hold the high byte of the
    //  subtracted value. Both readings agree on the LOW byte of every such pair,
    //  which is what lets the mistake survive a careless comparison.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinByteSelectorTests)
    {
    public:

        TEST_METHOD (TheHighByteSelectorAppliesToTheWholeExpression)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "TARGET = $90A7\n LDA #>TARGET-1\n");
            std::vector<Byte>  expected = { 0xA9, 0x90 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the byte is taken after the subtraction, not before it");
        }



        //  The low selector needs a DIVISION to be testable at all, and that is
        //  worth stating rather than leaving as an odd-looking source. Addition,
        //  subtraction and multiplication are all congruent modulo 256, so
        //  `<X-1` and `<(X-1)` agree for every X -- a subtraction test here
        //  passes under both readings and proves nothing. Division does not
        //  commute with taking a byte, so it discriminates. Verified by mutation.
        TEST_METHOD (TheLowByteSelectorDoesTheSame)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "TARGET = $9105\n LDA #<TARGET/$100\n");
            std::vector<Byte>  expected = { 0xA9, 0x91 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the whole expression is divided, and the byte taken afterwards");
        }



        TEST_METHOD (As65KeepsTheSelectorBindingToItsOwnTerm)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::Assemble (
                                              "TARGET equ $90A7\n lda #>TARGET-1\n", DialectId::As65);
            std::vector<Byte>  expected = { 0xA9, 0x8F };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"AS65's prefix operator must bind exactly as it always did");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinBranchAliasTests
    //
    //  BLT and BGE, Merlin's names for BCC and BCS.
    //
    //  These are not decoration. Three of the five vendor oracle sources use
    //  them, so four of the six shipped objects are unreachable without them --
    //  and they are the only construct standing between a source and its object
    //  that is a MNEMONIC rather than a directive.
    //
    //  The AS65 counterparts are the discriminating half. Branch encoding is
    //  shared, so a test that only proved $90 comes out would pass whether or not
    //  the dialect was consulted.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinBranchAliasTests)
    {
    public:

        TEST_METHOD (BltAssemblesAsBranchOnCarryClear)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              " BLT AHEAD\n"
                                              "AHEAD RTS\n");
            std::vector<Byte>  expected = { 0x90, 0x00, 0x60 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"BLT is BCC, opcode $90");
        }



        TEST_METHOD (BgeAssemblesAsBranchOnCarrySet)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              " BGE AHEAD\n"
                                              "AHEAD RTS\n");
            std::vector<Byte>  expected = { 0xB0, 0x00, 0x60 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"BGE is BCS, opcode $B0");
        }



        //  A backward branch, so the alias is proved to reach the displacement
        //  arithmetic rather than only the opcode lookup.
        TEST_METHOD (AnAliasedBranchComputesItsDisplacementLikeTheRealMnemonic)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "BACK NOP\n"
                                              " BLT BACK\n");
            std::vector<Byte>  expected = { 0xEA, 0x90, 0xFD };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"a backward alias branch needs the same relative arithmetic");
        }



        //  The real spellings must still work. An alias table consulted in the
        //  wrong order, or one that replaced rather than added, would break these.
        TEST_METHOD (TheUnaliasedSpellingsStillAssemble)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              " BCC AHEAD\n"
                                              " BCS AHEAD\n"
                                              "AHEAD RTS\n");
            std::vector<Byte>  expected = { 0x90, 0x02, 0xB0, 0x00, 0x60 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"BCC and BCS are unaffected by their aliases");
        }



        //  FR-005: one dialect's constructs must not be admitted into another.
        TEST_METHOD (As65RejectsBlt)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         "        .org $8000\n"
                                         "        blt ahead\n"
                                         "ahead:  rts\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"BLT is Merlin's spelling and is not an as65 instruction");
        }



        TEST_METHOD (As65RejectsBge)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         "        .org $8000\n"
                                         "        bge ahead\n"
                                         "ahead:  rts\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"BGE likewise");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinRawDataTests
    //
    //  Raw hexadecimal, in the two places Merlin admits it: the HEX directive and
    //  a run of digits after a string's closing delimiter.
    //
    //  Both were settled against `MAKE DUMP`'s shipped object rather than
    //  reasoned about. Its source carries
    //  `ASC "This destroys current source."8D8D` and
    //  `ASC "Do you really want it (Y/N)? "00`, and the object holds the
    //  high-ASCII text followed by `8D 8D` and then `00`. Two facts fall out: the
    //  digits are hexadecimal, and the bytes are NOT pushed through the
    //  delimiter's high-bit convention -- `00` staying `00` is the proof, since a
    //  high-bit rule would have written `80`.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinRawDataTests)
    {
    public:

        TEST_METHOD (TrailingDigitsAfterAStringAreHexadecimalBytes)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" ASC \"AB\"8D8D\n");
            std::vector<Byte>  expected = { 0xC1, 0xC2, 0x8D, 0x8D };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"two digit pairs are two bytes, appended after the text");
        }



        //  The half the corpus settles that no amount of reasoning would: a
        //  trailing `00` is a zero byte, not a high-bit zero.
        TEST_METHOD (ATrailingZeroByteIsNotForcedToHighAscii)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" ASC \"A\"00\n");
            std::vector<Byte>  expected = { 0xC1, 0x00 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the trailing run is verbatim; only the TEXT takes the high bit");
        }



        //  Half a byte is refused rather than padded. Both plausible repairs
        //  change every byte after it, so neither may be guessed at.
        TEST_METHOD (AnOddDigitRunAfterAStringIsRefused)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" ASC \"A\"8\n");

            Assert::IsFalse (result.errors.empty(), L"an odd digit count is not a whole byte");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "hexadecimal"),
                            L"and the diagnostic must say what was wrong with it");
        }



        TEST_METHOD (HexEmitsItsDigitsAsRawBytes)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" HEX 8D8D\n");
            std::vector<Byte>  expected = { 0x8D, 0x8D };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"HEX writes the bytes the digits spell");
        }



        //  The two passes have to agree about the length, or every label after a
        //  HEX line binds to the wrong address. Checked by making a later label's
        //  value the output rather than by counting bytes.
        TEST_METHOD (HexReservesTheSameLengthInBothPasses)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              " HEX 010203\n"
                                              "AFTER RTS\n"
                                              " DA AFTER\n");
            std::vector<Byte>  expected = { 0x01, 0x02, 0x03, 0x60, 0x03, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"AFTER must land at $8003, which only holds if pass 1 sized HEX");
        }



        TEST_METHOD (HexWithAnOddDigitCountIsRefused)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" HEX 8D8\n");

            Assert::IsFalse (result.errors.empty(), L"five digits are not whole bytes");
        }



        //  Separators are accepted although the disk never uses one. UNVERIFIED
        //  against vendor bytes and deliberately so: refusing a documented form
        //  can only cost a user source that assembles elsewhere.
        TEST_METHOD (HexAcceptsCommaSeparatedBytes)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" HEX 8D,00,FF\n");
            std::vector<Byte>  expected = { 0x8D, 0x00, 0xFF };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"commas separate bytes; they are not data");
        }



        //  FR-005 again. HEX is Merlin's word and must stay unknown to as65.
        TEST_METHOD (As65DoesNotAssembleHex)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble ("        hex 8D8D\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"HEX is not an as65 directive");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinEquateTests
    //
    //  `NAME = expr` puts the sign in the OPCODE field, with the name beside it
    //  in the label field. That makes it a field-model fact rather than an
    //  expression one -- a parser hunting for `=` inside the operand finds it in
    //  the wrong place and leaves the line looking like an instruction called
    //  `=`, which is exactly what it did.
    //
    //  128 of them across the vendor sources, and no other equate spelling.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinEquateTests)
    {
    public:

        TEST_METHOD (AnEquateDefinesTheNameToItsExpression)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "PORT = $12\n"
                                              " LDA PORT\n");
            std::vector<Byte>  expected = { 0xA5, 0x12 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"PORT is $12, and zero page at that");
        }



        //  The name must NOT also bind to the program counter. It did, before the
        //  label field was cleared, and the equate was then reported as a
        //  duplicate of the label the same line had just defined.
        TEST_METHOD (AnEquateDoesNotAlsoBindItsNameToTheProgramCounter)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "MARK = $1234\n"
                                              " DA MARK\n");
            std::vector<Byte>  expected = { 0x34, 0x12 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"MARK is $1234, not the address the line sat at");
        }



        //  The equate line itself emits nothing, and the line after it starts
        //  where the origin said.
        TEST_METHOD (AnEquateOccupiesNoSpace)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "A = 1\n"
                                              "B = 2\n"
                                              "HERE RTS\n"
                                              " DA HERE\n");
            std::vector<Byte>  expected = { 0x60, 0x00, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"HERE is $8000, so neither equate advanced the counter");
        }



        //  A comment after the expression is a comment, not part of it. Every
        //  equate in MAKE DUMP carries one.
        TEST_METHOD (AnEquateIgnoresItsTrailingComment)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "LOADADR = $9000 ;Address of BRUN\n"
                                              " DA LOADADR\n");
            std::vector<Byte>  expected = { 0x00, 0x90 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the comment must not reach the expression");
        }



        //  EQU is in the language although absent from the disk. Recorded as a
        //  deliberate acceptance rather than an oracle-backed one.
        TEST_METHOD (TheEquKeywordIsAcceptedAsWell)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "PORT EQU $12\n"
                                              " LDA PORT\n");
            std::vector<Byte>  expected = { 0xA5, 0x12 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"EQU is the same operation as the sign");
        }



        //  An indented `=` has no name to define, and must say so rather than
        //  quietly defining nothing.
        TEST_METHOD (AnEquateWithNoNameIsNotAnEquate)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" = $12\n");

            Assert::IsFalse (result.errors.empty(), L"with no label there is nothing to equate");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinListingDirectiveTests
    //
    //  Directives that steer the listing and change no object byte. They reuse
    //  the assembler-option token rather than each bringing one whose handler
    //  would do nothing -- a token exists for an operation the assembler cannot
    //  already perform, and "recognized, affects no output" is one it can.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinListingDirectiveTests)
    {
    public:

        TEST_METHOD (ListingDirectivesAreRecognizedAndEmitNothing)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              " TR\n"
                                              " EXP OFF\n"
                                              " AST 50\n"
                                              " RTS\n");
            std::vector<Byte>  expected = { 0x60 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"three listing directives and one instruction is one byte");
        }



        TEST_METHOD (As65DoesNotAcceptMerlinsListingDirectives)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble ("        ast 50\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"AST is Merlin's word, not as65's");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinMacroDefinitionTests
    //
    //  Merlin opens a macro with MAC in the opcode field and the name in the
    //  label, and closes it with `<<<`. Both resolve to tokens the shared engine
    //  already has, so the dialect difference is vocabulary rather than behavior.
    //
    //  The terminator is the interesting one. It resolved to exactly the right
    //  token and was then compared against as65's SPELLING, so it was swallowed
    //  into the body it was meant to close and the rest of the file went with
    //  it -- lines simply stopped producing bytes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinMacroDefinitionTests)
    {
    public:

        TEST_METHOD (AMacroClosedByAngleBracketsExpandsAtItsCallSite)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "SETONE MAC\n"
                                              " LDA #$01\n"
                                              " <<<\n"
                                              " SETONE\n"
                                              " RTS\n");
            std::vector<Byte>  expected = { 0xA9, 0x01, 0x60 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"the body must expand and the line after the terminator must survive");
        }



        //  The definition emits nothing where it stands. A body assembled in
        //  place would put its bytes at the definition's address as well as at
        //  every call site.
        TEST_METHOD (AMacroBodyEmitsNothingWhereItIsDefined)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "NEVER MAC\n"
                                              " LDA #$01\n"
                                              " <<<\n"
                                              " RTS\n");
            std::vector<Byte>  expected = { 0x60 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"a macro that is never called contributes no bytes");
        }



        //  Merlin has no macro-local DECLARATION -- its macro locals are ordinary
        //  local labels -- so a body line beginning with the word another dialect
        //  uses for one is just a labeled instruction. Dropping it took the label
        //  and its two bytes with it, with nothing said.
        TEST_METHOD (ABodyLineBeginningWithAnotherDialectsLocalKeywordSurvives)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "SETUP MAC\n"
                                              "LOCAL LDA #$42\n"
                                              " <<<\n"
                                              " SETUP\n"
                                              " RTS\n");
            std::vector<Byte>  expected = { 0xA9, 0x42, 0x60 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"LOCAL is not a Merlin keyword, so the line is a labeled LDA");
        }



        //  The same word, in the dialect that DOES declare it. as65 must keep
        //  dropping the declaration, or every macro using one emits a line the
        //  assembler cannot read.
        TEST_METHOD (As65StillDropsItsOwnLocalDeclaration)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::Assemble (
                                              "        .org $8000\n"
                                              "twice   macro\n"
                                              "        local skip\n"
                                              "        bne skip\n"
                                              "skip:   nop\n"
                                              "        endm\n"
                                              "        twice\n"
                                              "        twice\n", DialectId::As65);
            std::vector<Byte>  expected = { 0xD0, 0x00, 0xEA, 0xD0, 0x00, 0xEA };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"as65's local declaration is still consumed, and its label still renamed per call");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinMacroExpansionTests
    //
    //  What an invocation actually emits: arguments separated by a semicolon,
    //  positional parameters substituted into the body, and body labels made
    //  unique per expansion.
    //
    //  Every shape here is taken from the vendor macro library rather than
    //  invented, because the shapes that look natural to write are not the ones
    //  Merlin source contains. `LDX #A]1-ADRTBL` splices a parameter into the
    //  middle of a symbol, which no whole-word substitution reaches, and the
    //  semicolon that separates arguments is the same character that introduces
    //  a comment one field later.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinMacroExpansionTests)
    {
    public:

        TEST_METHOD (PositionalParametersTakeTheirArgumentsInOrder)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "MOV MAC\n"
                                              " LDA ]1\n"
                                              " STA ]2\n"
                                              " <<<\n"
                                              " MOV $10;$11\n");
            std::vector<Byte>  expected = { 0xA5, 0x10, 0x85, 0x11 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"]1 and ]2 take the first and second arguments");
        }



        //  Three arguments, the shape the vendor library's own `ADD SUMSTR;DEFLEN;PL`
        //  uses. A parser stripping from the first semicolon passes one argument
        //  and silently drops two, which is why this counts rather than merely
        //  assembling.
        TEST_METHOD (SemicolonsSeparateArgumentsRatherThanStartingAComment)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "THREE MAC\n"
                                              " LDA ]1\n"
                                              " LDA ]2\n"
                                              " LDA ]3\n"
                                              " <<<\n"
                                              " THREE $10;$11;$12\n");
            std::vector<Byte>  expected = { 0xA5, 0x10, 0xA5, 0x11, 0xA5, 0x12 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"all three arguments arrive, not just the first");
        }



        //  A comment still follows the operand, and the semicolon introducing it
        //  is the same character that just separated two arguments. The field
        //  boundary is the whole difference.
        TEST_METHOD (ACommentMayFollowAnArgumentList)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "MOV MAC\n"
                                              " LDA ]1\n"
                                              " STA ]2\n"
                                              " <<<\n"
                                              " MOV $10;$11 ;move a byte\n");
            std::vector<Byte>  expected = { 0xA5, 0x10, 0x85, 0x11 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the trailing field is a comment, the operand's semicolons are not");
        }



        //  `LDX #A]1-ADRTBL`, from MAKE DUMP.S. The parameter is pasted after a
        //  prefix and the result is one symbol, so substitution has to happen
        //  before anything looks a symbol up -- and it must ignore identifier
        //  boundaries, which a named-parameter substitution deliberately does not.
        TEST_METHOD (AParameterPastesIntoTheMiddleOfASymbol)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "ADRTBL = $2000\n"
                                              "AKEYIN = ADRTBL+4\n"
                                              "CALL MAC\n"
                                              " LDX #A]1-ADRTBL\n"
                                              " <<<\n"
                                              " CALL KEYIN\n");
            std::vector<Byte>  expected = { 0xA2, 0x04 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"A + KEYIN names AKEYIN, four past the table");
        }



        //  The other direction, from the same file: `LDX #]1END-]1-1` pastes a
        //  suffix onto the argument. One argument, two different symbols.
        TEST_METHOD (AParameterTakesASuffixToNameASecondSymbol)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "FRST = $9000\n"
                                              "FRSTEND = $9004\n"
                                              "STORE MAC\n"
                                              " LDX #]1END-]1-1\n"
                                              " <<<\n"
                                              " STORE FRST\n");
            std::vector<Byte>  expected = { 0xA2, 0x03 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"]1END names FRSTEND while ]1 names FRST");
        }



        //  A macro body invoking another macro and forwarding its own parameters,
        //  which is how the vendor library builds MOVD out of MOV. The forwarded
        //  arguments are re-separated at the inner call, so the separator has to
        //  survive substitution intact.
        TEST_METHOD (AMacroBodyMayInvokeAnotherMacroWithItsOwnParameters)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "MOV MAC\n"
                                              " LDA ]1\n"
                                              " STA ]2\n"
                                              " <<<\n"
                                              "MOVD MAC\n"
                                              " MOV ]1;]2\n"
                                              " MOV ]1+1;]2+1\n"
                                              " <<<\n"
                                              " MOVD $10;$20\n");
            std::vector<Byte>  expected = { 0xA5, 0x10, 0x85, 0x20, 0xA5, 0x11, 0x85, 0x21 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the inner call receives the outer call's arguments");
        }



        //  The finding the corpus forces. MAKE DUMP.S expands INCD twice and
        //  STORE three times, each redefining a bare label, and the vendor shipped
        //  a working object -- so Merlin renames them per expansion. Without that,
        //  the second expansion is a duplicate-label error, and an assembler that
        //  merely allowed the redefinition would point both branches at the first
        //  copy.
        TEST_METHOD (BodyLabelsAreUniquePerExpansion)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "INCD MAC\n"
                                              " INC ]1\n"
                                              " BNE NI\n"
                                              " INC ]1+1\n"
                                              "NI\n"
                                              " <<<\n"
                                              " INCD $10\n"
                                              " INCD $12\n");
            std::vector<Byte>  expected = { 0xE6, 0x10, 0xD0, 0x02, 0xE6, 0x11,
                                            0xE6, 0x12, 0xD0, 0x02, 0xE6, 0x13 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"each expansion branches to its own copy of NI");
        }



        //  The terminator may carry the body's own branch target. KEYMAC.S ends
        //  its INCD with `NI <<<`, so the line that closes the definition is also
        //  the line that defines the label the body branches to -- and closing
        //  the body is exactly the operation that discards the line. Found by the
        //  vendor source after every synthetic macro test above already passed,
        //  which is the corpus earning its keep.
        TEST_METHOD (ALabelOnTheTerminatorLineStillBinds)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "INCD MAC\n"
                                              " INC ]1\n"
                                              " BNE NI\n"
                                              " INC ]1+1\n"
                                              "NI <<<\n"
                                              " INCD $10\n"
                                              " INCD $12\n");
            std::vector<Byte>  expected = { 0xE6, 0x10, 0xD0, 0x02, 0xE6, 0x11,
                                            0xE6, 0x12, 0xD0, 0x02, 0xE6, 0x13 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the label on the closing line binds at the end of the body");
        }



        //  And the label a macro produced must not become the enclosing global
        //  for the locals that follow the call. MAKE DUMP.S calls macros defining
        //  `LP` and `ND` in the middle of routines whose locals belong to a global
        //  further up, so a macro label opening a scope strands every local after
        //  it -- which is exactly what the file's first assembly attempt reported.
        TEST_METHOD (AMacroLabelDoesNotReScopeTheLocalsAroundIt)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "INCD MAC\n"
                                              " INC ]1\n"
                                              " BNE NI\n"
                                              " INC ]1+1\n"
                                              "NI\n"
                                              " <<<\n"
                                              "MAIN LDA #0\n"
                                              ":BACK NOP\n"
                                              " INCD $10\n"
                                              " JMP :BACK\n");
            std::vector<Byte>  expected = { 0xA9, 0x00, 0xEA,
                                            0xE6, 0x10, 0xD0, 0x02, 0xE6, 0x11,
                                            0x4C, 0x02, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L":BACK still belongs to MAIN after the expansion");
        }



        //  The explicit invocation prefix. UNVERIFIED against the corpus and
        //  unverifiable there: every macro on the vendor disk is invoked by bare
        //  name, so the disk can only say the bare form works. This is the first
        //  instance of the general rule that absence from the disk is not absence
        //  from the language.
        TEST_METHOD (TheExplicitPrefixInvokesTheNamedMacro)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "MOV MAC\n"
                                              " LDA ]1\n"
                                              " STA ]2\n"
                                              " <<<\n"
                                              " >>> MOV;$10;$11\n");
            std::vector<Byte>  expected = { 0xA5, 0x10, 0x85, 0x11 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the macro name is the first item of the operand");
        }



        //  The same, written flush against the name. Both spellings have to work:
        //  the tidy columns in a Merlin listing are the editor's doing, and a file
        //  arriving from anywhere else carries whatever its author typed.
        TEST_METHOD (TheExplicitPrefixMayBeWrittenFlushAgainstTheName)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "MOV MAC\n"
                                              " LDA ]1\n"
                                              " STA ]2\n"
                                              " <<<\n"
                                              " >>>MOV;$10;$11\n");
            std::vector<Byte>  expected = { 0xA5, 0x10, 0x85, 0x11 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the prefix separates from the name it is written against");
        }



        //  An explicit invocation is a macro call whether the macro exists or not,
        //  so an unknown name says so. Letting it fall through would report an
        //  unknown mnemonic named for the prefix, which describes the symptom and
        //  not the mistake.
        TEST_METHOD (AnExplicitInvocationOfAnUnknownMacroNamesIt)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" >>> NOSUCH;$10\n");

            Assert::IsFalse (result.errors.empty(), L"an unknown macro must be reported");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "NOSUCH"),
                            L"the diagnostic must name the macro that was not found");
        }



        //  The discriminating half. Macro expansion, argument splitting and label
        //  renaming are all shared mechanism, so a test that passes under both
        //  dialects is no evidence the Merlin profile was consulted at all.
        TEST_METHOD (TheSameMacroSourceUnderAs65DoesNotProduceThoseBytes)
        {
            AssemblyResult     merlin   = MerlinAssemblyFixture::AssembleMerlin (
                                              "MOV MAC\n"
                                              " LDA ]1\n"
                                              " STA ]2\n"
                                              " <<<\n"
                                              " MOV $10;$11\n");
            AssemblyResult     as65     = MerlinAssemblyFixture::Assemble (
                                              "MOV MAC\n"
                                              " LDA ]1\n"
                                              " STA ]2\n"
                                              " <<<\n"
                                              " MOV $10;$11\n", DialectId::As65);
            std::vector<Byte>  expected = { 0xA5, 0x10, 0x85, 0x11 };

            Assert::IsTrue (merlin.bytes == expected, L"the Merlin profile assembles it");
            Assert::IsFalse (as65.bytes == expected, L"AS65 must not, or the profile is not being consulted");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinVariableSymbolTests
    //
    //  The sigil's other job. `]COUNT` is a symbol that may be assigned again;
    //  `]1` is a macro parameter, and the digit is the whole distinction.
    //
    //  NOTHING HERE HAS AN ORACLE. No variable symbol appears in any of the nine
    //  committed vendor sources -- the sigil occurs there only as `]1` through
    //  `]3` inside macro bodies -- so these tests pin the implementation against
    //  itself and against the documented rule that a variable may be redefined
    //  where a label may not. They are not evidence about real Merlin the way the
    //  byte comparisons are, and should not be quoted as if they were.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinVariableSymbolTests)
    {
    public:

        TEST_METHOD (AVariableMayBeAssignedAgain)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "]V = 1\n"
                                              "]V = 2\n"
                                              " LDA #]V\n");
            std::vector<Byte>  expected = { 0xA9, 0x02 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the second assignment stands");
        }



        //  The other half of the same rule, and the half that gives it meaning: an
        //  ordinary equate is immutable, so the sigil is doing the work rather
        //  than Merlin simply permitting redefinition everywhere.
        TEST_METHOD (AnOrdinaryEquateMayNotBeAssignedAgain)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         "V = 1\n"
                                         "V = 2\n");

            Assert::IsFalse (result.errors.empty(), L"redefining an ordinary equate is an error");
        }



        //  A reference takes the value assigned most recently BEFORE it, which is
        //  the property that makes a reassignable symbol worth having. Getting
        //  this wrong is silent: every reference simply reads the file's last
        //  assignment, and the bytes are plausible.
        TEST_METHOD (AReferenceTakesTheValueAssignedMostRecentlyBeforeIt)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "]V = 1\n"
                                              " LDA #]V\n"
                                              "]V = 2\n"
                                              " LDA #]V\n");
            std::vector<Byte>  expected = { 0xA9, 0x01, 0xA9, 0x02 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"each reference reads the assignment above it");
        }



        //  The same reassignment, read by a data directive instead of by an
        //  instruction -- and this is the case that discriminates. An
        //  instruction's operand is settled in pass 1, which walks the file in
        //  order and sees each assignment in turn, so it reads the right value
        //  whether or not anything replays the assignments later. A data
        //  directive is emitted in pass 2 against the finished symbol table, and
        //  without the replay it reads whatever the file assigned LAST. The two
        //  then disagree inside one file: the instruction above reads 1 and this
        //  reads 2, from the same symbol at the same point.
        TEST_METHOD (DataEmittedInPassTwoAlsoReadsTheAssignmentAboveIt)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "]V = 1\n"
                                              " DFB ]V\n"
                                              "]V = 2\n"
                                              " DFB ]V\n");
            std::vector<Byte>  expected = { 0x01, 0x02 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"pass 2 must see the assignments in order too");
        }



        //  A variable is a namespace of its own: the sigil is part of the name, so
        //  `]COUNT` and `COUNT` are two symbols and defining one says nothing
        //  about the other.
        TEST_METHOD (AVariableAndALabelOfTheSameNameAreDifferentSymbols)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "]COUNT = 5\n"
                                              "COUNT = 7\n"
                                              " LDA #]COUNT\n"
                                              " LDA #COUNT\n");
            std::vector<Byte>  expected = { 0xA9, 0x05, 0xA9, 0x07 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the two names do not collide");
        }



        //  A variable inside a macro body is a variable, not a parameter: the
        //  digit is what makes `]1` positional, so a named one survives expansion
        //  and resolves as an ordinary symbol.
        TEST_METHOD (AMacroBodyMayReferenceAVariableAlongsideItsParameters)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              "]BASE = $30\n"
                                              "GRAB MAC\n"
                                              " LDA ]BASE+]1\n"
                                              " <<<\n"
                                              " GRAB 2\n");
            std::vector<Byte>  expected = { 0xA5, 0x32 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"]BASE stays a symbol while ]1 takes the argument");
        }



        //  A variable may also stand where a label stands, taking the program
        //  counter -- and the point is that it may do so REPEATEDLY. Each branch
        //  means the definition immediately above it.
        //
        //  The two distances differ deliberately. Equal ones would produce the
        //  same displacement whichever definition won, which is a test that
        //  cannot fail. If every reference resolved to the last definition, the
        //  first branch would come out $00 instead of $FC.
        TEST_METHOD (AVariableStandingAsAProgramCounterLabelBindsAtEachDefinition)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "]LOOP NOP\n NOP\n BNE ]LOOP\n"
                                              "]LOOP NOP\n BNE ]LOOP\n");
            std::vector<Byte>  expected = { 0xEA, 0xEA, 0xD0, 0xFC, 0xEA, 0xD0, 0xFD };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"each branch takes the definition above it, not the last one in the file");
        }



        //  The same claim through a DATA directive rather than a branch. Three
        //  times in this feature a data directive has caught what an instruction
        //  operand hid, because sizing settles instruction operands in pass 1
        //  while data values are computed in pass 2 against one finished table.
        TEST_METHOD (DataReadsTheProgramCounterLabelDefinedAboveIt)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "]MARK NOP\n DA ]MARK\n"
                                              "]MARK NOP\n DA ]MARK\n");
            std::vector<Byte>  expected = { 0xEA, 0x00, 0x80, 0xEA, 0x03, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"the second word must be the second definition, and the first the first");
        }



        //  A variable label does NOT open a local-label scope. The same name is
        //  defined over and over, so treating each as a scope would put the
        //  locals after it under whichever definition came last -- and would
        //  strand every local defined before it. CLOCK.S depends on this:
        //  `INCTIME` opens a scope, `]LOOP` follows immediately, and `:OUT`
        //  further down still belongs to `INCTIME`.
        TEST_METHOD (AVariableLabelDoesNotOpenALocalScope)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "GLOBAL NOP\n"
                                              ":LOC NOP\n"
                                              "]LOOP NOP\n"
                                              " DA :LOC\n");
            std::vector<Byte>  expected = { 0xEA, 0xEA, 0xEA, 0x01, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"the local still belongs to GLOBAL after a variable label has intervened");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinKeyboardInputTests
    //
    //  KBD binds the symbol in its label field to an answer supplied from outside
    //  the source. Merlin stops the assembly and prompts; a batch assembler is
    //  told, and refuses when it was not.
    //
    //  The refusal is the half worth testing hardest. Both alternatives are silent
    //  failures of a kind this feature exists to avoid: blocking on a prompt turns
    //  an unattended build into a hang, and defaulting the answer assembles a
    //  different program cleanly.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinKeyboardInputTests)
    {
    public:

        TEST_METHOD (AnAnsweredSymbolTakesTheValueSupplied)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlinWithAnswers (
                                              "SIZE KBD \"How many?\"\n DFB SIZE\n",
                                              { { "SIZE", 7 } });
            std::vector<Byte>  expected = { 0x07 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the answer is the symbol's value");
        }



        //  The bare form -- no prompt, and a trailing comment that begins the very
        //  next field. Both PI sources on the disk write it with nothing after the
        //  directive at all, and PRINTFILER.S writes it with a comment.
        TEST_METHOD (TheBareFormWithATrailingCommentParses)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlinWithAnswers (
                                              "FORMAT KBD ; 1 = format, 0 = pack\n DFB FORMAT\n",
                                              { { "FORMAT", 1 } });
            std::vector<Byte>  expected = { 0x01 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the comment is a comment, not an operand");
        }



        //  The prompt holds spaces, so it must be read as delimited text rather
        //  than as one whitespace-bounded word. Every prompt on the vendor disk is
        //  a sentence.
        TEST_METHOD (AMissingAnswerNamesTheSymbolAndTheWholePrompt)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         "VERSION KBD \"Want 12 or 24 hour version (12/24)?\"\n DFB VERSION\n");

            Assert::IsFalse (result.errors.empty(), L"an unanswered prompt must fail rather than guess");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "VERSION"),
                            L"the diagnostic must name the symbol that needs an answer");
            //  The prompt arrives WITHOUT its delimiters -- the source's choice of
            //  delimiter is Merlin's syntax, not part of what it asked. Asserting
            //  the parenthesis directly against the first word is what makes that
            //  checkable; a bare substring of the prompt matches either way, and
            //  a mutation leaving the delimiters in went uncaught until this.
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "(Want 12 or 24 hour version (12/24)?)"),
                            L"and the whole prompt, which is the only place the source says what the answer means");
        }



        //  A prompt-less line still has to fail by name, and must not report an
        //  empty parenthetical where the prompt would have been.
        TEST_METHOD (AMissingAnswerWithNoPromptStillNamesTheSymbol)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin ("SAVOBJ KBD\n DFB SAVOBJ\n");

            //  Naming the symbol is not enough to assert: the undefined-symbol
            //  complaint from the line below names it too, so a test settling for
            //  that passes against an assembler that says nothing about the
            //  keyboard-input line at all. Verified by mutation.
            Assert::IsFalse (result.errors.empty(), L"the bare form must fail the same way");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "No answer supplied for SAVOBJ"),
                            L"the refusal must come from the directive, not from the undefined symbol below it");
            Assert::IsFalse (MerlinAssemblyFixture::AnyErrorMentions (result, "()"),
                             L"and no empty parenthetical where a prompt would have gone");
        }



        //  The label names a SYMBOL, not an address. Binding it to the program
        //  counter as well would leave the conditional that reads it testing where
        //  the line sat -- which is non-zero for any ordinary origin, so every
        //  gated block would assemble regardless of the answer.
        TEST_METHOD (TheLabelDoesNotAlsoBindToTheProgramCounter)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlinWithAnswers (
                                              " ORG $8000\n"
                                              "SAVOBJ KBD \"Save object code?\"\n"
                                              " DO SAVOBJ\n DFB $11\n FIN\n"
                                              " DFB $22\n",
                                              { { "SAVOBJ", 0 } });
            std::vector<Byte>  expected = { 0x22 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"an answer of 0 must skip the gated block, which a program-counter binding would not");
        }



        //  And the answer really drives the conditional in the other direction, or
        //  the test above would pass against an assembler that skipped everything.
        TEST_METHOD (TheAnswerDrivesConditionalAssembly)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlinWithAnswers (
                                              " ORG $8000\n"
                                              "SAVOBJ KBD \"Save object code?\"\n"
                                              " DO SAVOBJ\n DFB $11\n FIN\n"
                                              " DFB $22\n",
                                              { { "SAVOBJ", 1 } });
            std::vector<Byte>  expected = { 0x11, 0x22 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"an answer of 1 must take the gated block");
        }



        //  KBD is Merlin's alone. AS65 must go on refusing it, or FR-005's
        //  strictness rule is broken by the very construct that needed a token.
        TEST_METHOD (As65DoesNotKnowKbd)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         "SIZE KBD \"How many?\"\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"a Merlin directive must not become an AS65 one");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinExpressionDialectTests
    //
    //  The three ways Merlin's expressions differ from the shared syntax beyond
    //  binding: two renamed operators, unsigned 16-bit arithmetic, and a wider
    //  symbol character set. Each has an AS65 counterpart, because a test passing
    //  under both dialects is no evidence the profile was consulted.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinExpressionDialectTests)
    {
    public:

        //  `!` is exclusive-or. The operands are chosen so inclusive-or and
        //  exclusive-or DISAGREE -- $FF against $0F gives $F0 one way and $FF the
        //  other -- since overlapping bits would make either reading pass.
        TEST_METHOD (TheBangOperatorIsExclusiveOr)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DFB $FF!$0F\n");
            std::vector<Byte>  expected = { 0xF0 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"exclusive-or clears the bits both operands share");
        }



        //  `.` is inclusive-or. The SAME overlapping pair, so the two tests
        //  discriminate each other: disjoint operands like $F0 and $0F give $FF
        //  under either reading, which is a test that cannot fail and was caught
        //  by mutating the table.
        TEST_METHOD (ThePeriodOperatorIsInclusiveOr)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DFB $FF.$0F\n");
            std::vector<Byte>  expected = { 0xFF };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"inclusive-or keeps every bit either operand has");
        }



        //  A renamed character has to STOP meaning what it meant, or the digraph
        //  it leads would still win. Under the shared syntax `!` is logical-not.
        TEST_METHOD (As65StillReadsBangAsLogicalNot)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::Assemble (" .byte !0\n", DialectId::As65);
            std::vector<Byte>  expected = { 0x01 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"AS65 must not acquire Merlin's spelling");
        }



        //  Merlin folds in unsigned 16-bit quantities, which changes division and
        //  nothing else visible. $FFF3 over $FFFF is 0 because the numerator is
        //  smaller; read as signed 32-bit the same operands are -13 over -1 = 13.
        TEST_METHOD (DivisionFoldsInUnsignedSixteenBitQuantities)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DFB 12-25/-1\n");
            std::vector<Byte>  expected = { 0x00 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the numerator is the smaller unsigned quantity");
        }



        //  The other half of the same computation, so the test above is not
        //  passing on a zero it would have produced anyway.
        TEST_METHOD (TheSameDivisionIsOneWhenTheOperandsAreEqual)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (" DFB 24-25/-1\n");
            std::vector<Byte>  expected = { 0x01 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"$FFFF over $FFFF is 1");
        }



        TEST_METHOD (As65DividesInSignedThirtyTwoBitQuantities)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::Assemble (" .byte (12-25)/-1\n", DialectId::As65);
            std::vector<Byte>  expected = { 0x0D };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"AS65 keeps the arithmetic it has always had");
        }



        //  `?` is legal inside a Merlin symbol. Both halves have to agree: the
        //  definition must be accepted AND the reference must lex as one
        //  identifier, or the name binds and then resolves nowhere.
        TEST_METHOD (AQuestionMarkIsLegalInsideASymbol)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " ORG $8000\n"
                                              "CMD? NOP\n"
                                              " DA CMD?\n");
            std::vector<Byte>  expected = { 0xEA, 0x00, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the name must both bind and resolve");
        }



        TEST_METHOD (As65StillRefusesAQuestionMarkInALabel)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         " .org $8000\nCMD?: nop\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"AS65 must not acquire Merlin's character set");
        }



        //  A character constant may hold a space, and the operand scanner has to
        //  know it. CLOCK.S blanks the leading zero of the hour with `LDA #" "`
        //  and follows it with a comment that itself contains quotes.
        TEST_METHOD (ACharacterConstantMayHoldASpace)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " LDA #\" \" ;Blank leading \"0\"\n");
            std::vector<Byte>  expected = { 0xA9, 0xA0 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the space is payload and the comment is still a comment");
        }



        //  A constant holding a space in a MULTI-ARGUMENT operand, with a comment
        //  that itself contains quotes. The scanner has to resume after the
        //  constant rather than treating the delimiter as a toggle -- a toggle
        //  reads the comma and everything past it as still inside a string, and
        //  the second byte disappears.
        //
        //  Shaped like the vendor's own lines: KEYMAC.S writes
        //  `DFB $9F&"N",$88,$88` with the character constant inside an
        //  expression, which is the form that reaches the evaluator at all.
        TEST_METHOD (ACharacterConstantHoldingASpaceSurvivesInAnArgumentList)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DFB $FF&\" \",$41 ;the \"space\" one\n");
            std::vector<Byte>  expected = { 0xA0, 0x41 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"both arguments survive and the comment is still a comment");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinLoopTests
    //
    //  The repeat block, which turns one written line into several assembled ones.
    //
    //  Almost every assertion below ends in a DATA directive naming a label
    //  defined after the block. That is deliberate: an instruction's operand is
    //  settled in pass 1, which walks the file in order, while a data directive is
    //  emitted in pass 2 against the finished symbol table -- so a block that
    //  expanded to the wrong SIZE shows up as a wrong address rather than only as
    //  a wrong byte count, and the two failures are told apart in the message.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinLoopTests)
    {
    public:

        TEST_METHOD (ALoopEmitsItsBodyOncePerIteration)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " LUP 3\n DFB $EA\n --^\n");
            std::vector<Byte>  expected = { 0xEA, 0xEA, 0xEA };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"three iterations of a one-byte body");
        }



        //  A block of no iterations is the count doing its job, not a failure --
        //  but the bytes around it must be untouched, which is the half that
        //  distinguishes "expanded zero times" from "swallowed the rest of the
        //  file".
        TEST_METHOD (ALoopOfNoIterationsEmitsNothingAndTheFileCarriesOn)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DFB $11\n LUP 0\n DFB $EA\n --^\n DFB $22\n");
            std::vector<Byte>  expected = { 0x11, 0x22 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"no iterations, and the lines either side still assemble");
        }



        //  The expansion has to move the program counter, or every label after the
        //  block binds where the block's FIRST iteration ended. Read through a data
        //  directive against the finished symbol table.
        TEST_METHOD (ALoopAdvancesTheAddressesOfWhatFollowsIt)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " LUP 4\n DFB $00\n --^\n"
                                              "HERE DFB $12\n DA HERE\n");
            std::vector<Byte>  expected = { 0x00, 0x00, 0x00, 0x00, 0x12, 0x04, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"HERE binds past all four iterations, at $8004");
        }



        //  A block inside a block is body text to the outer one, counted only so
        //  the right terminator closes it. Every outer copy then expands the inner
        //  block afresh, which is the only reading that gives six of the inner byte.
        TEST_METHOD (ALoopMayHoldAnotherLoop)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " LUP 2\n LUP 3\n DFB $EA\n --^\n DFB $FF\n --^\n");
            std::vector<Byte>  expected = { 0xEA, 0xEA, 0xEA, 0xFF, 0xEA, 0xEA, 0xEA, 0xFF };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the inner block runs three times inside each of two outer ones");
        }



        //  Every copy of a body line has to carry the line number the author wrote
        //  it at. A body kept as bare text has no line number to carry, and the
        //  second iteration's diagnostic would then land wherever the expansion
        //  happened to be spliced.
        TEST_METHOD (ADiagnosticInsideALoopPointsAtTheLineThatWasWritten)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " LUP 2\n LDA NOSUCHLABEL\n --^\n");

            Assert::AreEqual ((size_t) 2, result.errors.size(),
                              L"one diagnostic per iteration, since each is a line that was assembled");
            Assert::AreEqual (2, result.errors[0].lineNumber, L"the line the body was written on");
            Assert::AreEqual (2, result.errors[1].lineNumber, L"and the same line for the second iteration");
        }



        //  The count is settled while pass 1 is still walking, because that is when
        //  the block becomes lines. A label defined below it therefore cannot
        //  answer, and saying so beats expanding zero times in silence.
        TEST_METHOD (ALoopCountThatCannotBeSettledYetIsReported)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " LUP LATER\n DFB $EA\n --^\nLATER EQU 3\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "repeat count must be resolvable"),
                            L"a forward count has no answer at the moment the block is expanded");
        }



        TEST_METHOD (ALoopCountBeyondTheGuardIsReported)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " LUP $FFFF\n DFB $EA\n --^\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "repeat count must be 0 to"),
                            L"a count no source can have meant is refused rather than answered with memory");
        }



        //  A terminator with nothing open used to be dropped without a word, which
        //  is the shape every directive in this dialect exists to avoid.
        TEST_METHOD (ALoopTerminatorWithNothingOpenIsReported)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" DFB $11\n --^\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "has no loop to close"),
                            MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
        }



        TEST_METHOD (AnUnclosedLoopIsReportedAtTheLineItOpenedOn)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " DFB $11\n LUP 2\n DFB $EA\n");

            Assert::AreEqual ((size_t) 1, result.errors.size(), L"one diagnostic, about the block that never closed");
            Assert::AreEqual (2, result.errors[0].lineNumber, L"reported where the block opened, not at the end of file");

            Assert::IsTrue (result.errors[0].message.find ("LUP") != std::string::npos,
                            L"named in the spelling the source actually wrote");
        }



        //  A DEFERRED diagnostic, so the file and the column have to be captured
        //  where the block opened. By the time this is reported the ambient answer
        //  is whatever was processed last -- here the top-level line after the
        //  include -- and using it would give the right line number in the wrong
        //  file, which reads as a correct diagnostic and is the harder mistake to
        //  see.
        TEST_METHOD (AnUnclosedLoopInsideAnIncludedFileNamesThatFileAndColumn)
        {
            MockFileReader  reader;
            AssemblyResult  result;

            reader.files["T.PART"] = " LUP 2\n DFB $EA\n";

            result = MerlinAssemblyFixture::AssembleMerlinWithReader (" PUT PART\n DFB $11\n", reader);

            Assert::AreEqual ((size_t) 1, result.errors.size(),
                              MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (std::string ("T.PART"), result.errors[0].file,
                              L"attributed to the file the block opened in");
            Assert::AreEqual (1, result.errors[0].lineNumber, L"and to the line inside that file");
            Assert::AreEqual (2, result.errors[0].column, L"and to the column the opening directive sits at");
        }



        //  The construct must not have leaked into the other dialect. AS65 knows no
        //  such spelling and has to go on rejecting it.
        TEST_METHOD (AS65DoesNotAcquireTheLoopConstruct)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         " LUP 3\n DFB $EA\n --^\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"AS65 must not gain a Merlin spelling");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinDummySectionTests
    //
    //  The section that assigns addresses and emits nothing.
    //
    //  Every positive assertion here reads its labels back through a data
    //  directive, which is the only way to see BOTH halves at once: that the
    //  addresses inside the section are what the section said, and that not one
    //  byte of it reached the object.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinDummySectionTests)
    {
    public:

        TEST_METHOD (ADummySectionBindsLabelsAndEmitsNothing)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DUM $20\nPTR DS 2\nFLAG DS 1\n DEND\n"
                                              " DA PTR\n DA FLAG\n");
            std::vector<Byte>  expected = { 0x20, 0x00, 0x22, 0x00 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"the layout is described at $20 and only the two words are emitted");
        }



        //  The program counter has to come back to where the section was entered
        //  from, or every address after it belongs to the layout instead of to the
        //  program.
        TEST_METHOD (TheAddressesAfterASectionResumeWhereItStarted)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DFB $01\n DUM $30\nSLOT DS 4\n DEND\n"
                                              "HERE DFB $02\n DA HERE\n");
            std::vector<Byte>  expected = { 0x01, 0x02, 0x01, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"HERE binds at $8001, immediately after the byte before the section");
        }



        //  Instructions inside a section are SIZED like any others, which is what
        //  makes the label after them right. That is all this one claims -- see
        //  the next test for the other half, and the note there for why the two
        //  cannot be claimed by one assertion.
        TEST_METHOD (AnInstructionInsideASectionStillMovesTheAddressesAfterIt)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DUM $300\nGO LDA $1234\nAFTER DS 0\n DEND\n"
                                              " DA GO\n DA AFTER\n DFB $99\n");
            std::vector<Byte>  expected = { 0x00, 0x03, 0x03, 0x03, 0x99 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"GO is $0300 and the three-byte instruction moved AFTER to $0303");
        }



        //  Nothing inside a section reaches the object, and the section has to be
        //  LONGER than what follows it for that to be observable at all.
        //
        //  This was found by mutation rather than by reading, and it is worth the
        //  space: a section whose contents are shorter than the lines after it is
        //  vacuous however carefully it is written, because the output cursor never
        //  advanced across the section -- so every byte the section wrongly emitted
        //  sits exactly where the next real line is about to write, and the real
        //  line overwrites all of it. Three instructions against one byte is what
        //  leaves eight bytes with nothing to cover them.
        TEST_METHOD (NothingInsideASectionReachesTheObject)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DUM $300\n LDA $1234\n LDX $5678\n LDY $9ABC\n DEND\n"
                                              " DFB $99\n");
            std::vector<Byte>  expected = { 0x99 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"nine bytes of layout, and one byte of object");
        }



        //  A section that emits nothing must not be what STARTS the output, and
        //  this is the only place it shows. Closing a section puts the output
        //  cursor back where it was, so a section that wrongly advanced it is
        //  invisible everywhere else -- but the first origin only places the image
        //  while nothing has been output yet, and a section that claimed to have
        //  output something takes that away. The origin then merely relocates, the
        //  byte lands where the section left the cursor, and the image loads at an
        //  address nobody wrote.
        TEST_METHOD (ASectionBeforeTheFirstOriginDoesNotStartTheOutput)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DUM $300\n DS 4\n DEND\n ORG $2000\n DFB $99\n");
            std::vector<Byte>  expected = { 0x99 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual ((int) 0x2000, (int) result.startAddress,
                              L"the origin still places the image, because the section produced nothing");
            Assert::IsTrue (result.bytes == expected, L"and the one byte written is the whole object");
        }



        TEST_METHOD (ASectionTerminatorWithNothingOpenIsReported)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" DFB $11\n DEND\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "has no dummy section to close"),
                            MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
        }



        //  There is one place to return to, so a second entry would overwrite it
        //  and the first terminator would restore the second section's origin --
        //  putting every byte after it somewhere nobody asked for, in silence.
        TEST_METHOD (ASectionInsideASectionIsRefused)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " DUM $20\n DUM $30\n DEND\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "is already open, from line 1"),
                            MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
        }



        TEST_METHOD (AnUnclosedSectionIsReportedAtTheLineItOpenedOn)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " DFB $11\n DUM $20\nPTR DS 2\n");

            Assert::AreEqual ((size_t) 1, result.errors.size(), L"one diagnostic, about the section that never closed");
            Assert::AreEqual (2, result.errors[0].lineNumber, L"reported where the section opened");

            Assert::IsTrue (result.errors[0].message.find ("DUM") != std::string::npos,
                            L"named in the spelling the source actually wrote");
        }



        //  Deferred exactly as the unclosed loop is, and captured for the same
        //  reason: the ambient file and column belong to whatever was processed
        //  last, which by then is a line in another file entirely.
        TEST_METHOD (AnUnclosedSectionInsideAnIncludedFileNamesThatFileAndColumn)
        {
            MockFileReader  reader;
            AssemblyResult  result;

            reader.files["T.PART"] = "* layout\n DUM $20\nPTR DS 2\n";

            result = MerlinAssemblyFixture::AssembleMerlinWithReader (" PUT PART\n DFB $11\n", reader);

            Assert::AreEqual ((size_t) 1, result.errors.size(),
                              MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (std::string ("T.PART"), result.errors[0].file,
                              L"attributed to the file the section opened in");
            Assert::AreEqual (2, result.errors[0].lineNumber, L"and to the line inside that file");
            Assert::AreEqual (2, result.errors[0].column, L"and to the column the opening directive sits at");
        }



        TEST_METHOD (AS65DoesNotAcquireTheDummySection)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         " DUM $20\nPTR DS 2\n DEND\n", DialectId::As65);

            Assert::IsFalse (result.errors.empty(), L"AS65 must not gain a Merlin spelling");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinInclusionTests
    //
    //  File inclusion, whose operand is NOT the filename.
    //
    //  Merlin prepends a fixed prefix on the way from operand to name, and the
    //  distribution disk is what says so: `USE PI.MACS` and `PUT SENDMSG` name
    //  files stored as `T.PI.MACS` and `T.SENDMSG`. Both are committed here, so
    //  the rule is checked against the vendor's own libraries rather than against
    //  a filename invented to suit it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinInclusionTests)
    {
    public:

        TEST_METHOD (AnInclusionAsksForThePrefixedName)
        {
            MockFileReader  reader;
            AssemblyResult  result;

            reader.files["T.MACS"] = " DFB $EA\n";

            result = MerlinAssemblyFixture::AssembleMerlinWithReader (" PUT MACS\n", reader);

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (1, reader.CountRequests ("T.MACS"), L"the name the disk stores the file under");
            Assert::AreEqual (0, reader.CountRequests ("MACS"), L"and never the operand as written");
        }



        //  Two spellings, one operation. A prefix applied to only one of them
        //  would look right everywhere the other is not used.
        TEST_METHOD (TheOtherInclusionSpellingResolvesTheSameWay)
        {
            MockFileReader  reader;
            AssemblyResult  result;

            reader.files["T.MACS"] = " DFB $EA\n";

            result = MerlinAssemblyFixture::AssembleMerlinWithReader (" USE MACS\n", reader);

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (1, reader.CountRequests ("T.MACS"), L"both spellings resolve identically");
        }



        //  The committed macro library, end to end: resolved under its on-disk
        //  name, read, and its macros expanded into bytes. The byte assertion is
        //  what separates "the file was requested" from "the file was assembled" --
        //  a request alone is satisfied by a reader that returns nothing.
        TEST_METHOD (TheVendorMacroLibraryResolvesAndItsMacrosExpand)
        {
            MockFileReader     reader;
            AssemblyResult     result;
            std::vector<Byte>  expected = { 0xA9, 0x00, 0x85, 0x00, 0xA9, 0x0E, 0x85, 0x01 };

            reader.files["T.PI.MACS"] = MerlinAssemblyFixture::LoadFixtureTextSource ("Merlin/T.PI.MACS");

            result = MerlinAssemblyFixture::AssembleMerlinWithReader (" USE PI.MACS\n STADR SUMSTR;PL\n", reader);

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (1, reader.CountRequests ("T.PI.MACS"), L"resolved under the name the disk stores");
            Assert::AreEqual (0, reader.CountRequests ("PI.MACS"), L"and not under the operand as written");

            Assert::IsTrue (result.bytes == expected,
                            L"the library's STADR macro stores $0E00 into the zero-page pointer it defines");
        }



        //  The second committed library, which is stored as a type-T file and is a
        //  FRAGMENT: it expects its caller to bind the two variables it uses, so a
        //  clean assembly is not what is being claimed. Its label binding is, and
        //  that only happens if the file was found, read and walked.
        TEST_METHOD (TheVendorSubroutineLibraryResolvesUnderItsOnDiskName)
        {
            MockFileReader  reader;
            AssemblyResult  result;

            reader.files["T.PI.MACS"]  = MerlinAssemblyFixture::LoadFixtureTextSource ("Merlin/T.PI.MACS");
            reader.files["T.SENDMSG"]  = MerlinAssemblyFixture::LoadFixtureTextSource ("Merlin/T.SENDMSG");

            result = MerlinAssemblyFixture::AssembleMerlinWithReader (" USE PI.MACS\n PUT SENDMSG\n", reader);

            Assert::AreEqual (1, reader.CountRequests ("T.SENDMSG"), L"resolved under the name the disk stores");
            Assert::AreEqual (0, reader.CountRequests ("SENDMSG"), L"and not under the operand as written");

            Assert::AreEqual ((size_t) 1, result.symbols.count ("SENDMSG"),
                              L"the file's own label bound, so its lines really were assembled");
        }



        //  The prefix belongs to ONE dialect. AS65 resolves the name its source
        //  wrote, and a prefix leaking into the shared include path would send
        //  every AS65 build looking for a file that does not exist.
        TEST_METHOD (AS65InclusionKeepsTheNameItsSourceWrote)
        {
            MockFileReader  reader;
            AssemblyResult  result;

            reader.files["shared.a65"] = ".byte $EA\n";

            result = MerlinAssemblyFixture::AssembleMerlinWithReader (".include \"shared.a65\"\n",
                                                                      reader, DialectId::As65);

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (1, reader.CountRequests ("shared.a65"), L"AS65 asks for exactly what it was told");
            Assert::AreEqual (0, reader.CountRequests ("T.shared.a65"), L"and never for a prefixed one");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinCpuSelectionTests
    //
    //  The in-source CPU directive, which is the only way this dialect reaches the
    //  wider instruction set.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinCpuSelectionTests)
    {
    public:

        //  The negative half first, because without it "the wider set is active"
        //  is satisfied by an assembler that had it active all along.
        TEST_METHOD (WithoutTheDirectiveAWiderInstructionIsNotAccepted)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlinWithExtendedSet (" PHX\n");

            Assert::IsFalse (result.errors.empty(),
                             L"the wider set must not be reachable without the directive");
        }



        //  The bytes AND the address after them. A directive that selected the
        //  wider table but also occupied space would satisfy the encoding half on
        //  its own; the trailing data directive is what refuses that reading.
        TEST_METHOD (TheDirectiveEnablesTheWiderSetAndEmitsNothingItself)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlinWithExtendedSet (
                                              " XC\n PHX\nHERE DFB $77\n DA HERE\n");
            std::vector<Byte>  expected = { 0xDA, 0x77, 0x01, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"PHX encodes at $8000 and HERE binds at $8001, so the directive itself took no space");
        }



        //  "For the remainder of the assembly", read through lines that are not
        //  next to the directive.
        TEST_METHOD (TheSelectionLastsForTheRestOfTheAssembly)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlinWithExtendedSet (
                                              " XC\n NOP\n DFB $11\n PHY\n");
            std::vector<Byte>  expected = { 0xEA, 0x11, 0x5A };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"a wider instruction still encodes three lines later");
        }



        //  An assembly handed one instruction table cannot honor the directive, and
        //  a provider with nothing to switch to answers with the table it already
        //  had -- so saying nothing would leave the source told it had reached a
        //  wider processor while the assembler stayed on the narrow one.
        TEST_METHOD (TheDirectiveSaysSoWhenThereIsNothingToSelect)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" XC\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "only one instruction set"),
                            MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
        }



        //  Whether this directive has a form that puts the CPU BACK is an open
        //  question about the language, to be answered by assembling one under the
        //  real assembler. Refusing the operand answers nothing about Merlin; it
        //  says only that Casso implements the plain form. Ignoring the operand
        //  would answer it -- with "no such form exists, and writing one selects
        //  the wider processor anyway", silently.
        TEST_METHOD (AnOperandIsRefusedAndDoesNotSelectAnything)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlinWithExtendedSet (" XC OFF\n PHX\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "takes no operand here"),
                            MerlinAssemblyFixture::FirstDiagnostic (result).c_str());

            Assert::IsFalse (result.bytes.size() == 1,
                             L"a refused line must not have selected the wider set on the way past");
        }



        //  The first occurrence taking effect must not disturb the boundary, which
        //  refuses the second and every later one.
        TEST_METHOD (TheSecondOccurrenceIsStillRefused)
        {
            AssemblyResult  result   = MerlinAssemblyFixture::AssembleMerlinWithExtendedSet (" XC\n XC\n");
            int             refusals = 0;

            for (const AssemblyError & error : result.errors)
            {
                if (error.kind == DiagnosticKind::SubsetBoundary)
                {
                    refusals++;
                }
            }

            Assert::AreEqual (1, refusals, L"the second occurrence, and only the second");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinObjectFileTests
    //
    //  The source naming its own output, and the caller overruling it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinObjectFileTests)
    {
    public:

        TEST_METHOD (TheSourceNamesTheOutput)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" DSK CLOCK.24\n DFB $01\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (std::string ("CLOCK.24"), result.outputFileName,
                              L"the name the source asked for");
        }



        //  The directive names an output; it does not produce one. A line that
        //  emitted anything would shift every byte after it.
        TEST_METHOD (TheDirectiveEmitsNoBytes)
        {
            AssemblyResult     result   = MerlinAssemblyFixture::AssembleMerlin (
                                              " DSK CLOCK.24\nHERE DFB $01\n DA HERE\n");
            std::vector<Byte>  expected = { 0x01, 0x00, 0x80 };

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"HERE binds at $8000, so the directive took no space");
        }



        TEST_METHOD (TheCallersNameBeatsTheSources)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlinWithOutputName (
                                         " DSK CLOCK.24\n DFB $01\n", "build/out.bin");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (std::string ("build/out.bin"), result.outputFileName,
                              L"a build script overrides what the source asks for");
        }



        //  The other half of the precedence, without which "the caller wins" is
        //  satisfied by an assembler that ignores the directive entirely.
        TEST_METHOD (TheCallersNameStandsWhenTheSourceNamesNothing)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlinWithOutputName (" DFB $01\n",
                                                                                          "build/out.bin");

            Assert::AreEqual (std::string ("build/out.bin"), result.outputFileName,
                              L"nothing in the source to override it with");
        }



        TEST_METHOD (NeitherNamingAnOutputLeavesNoName)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" DFB $01\n");

            Assert::IsTrue (result.outputFileName.empty(), L"nobody named an output");
        }



        TEST_METHOD (TheLastDirectiveInTheSourceWins)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " DSK FIRST\n DFB $01\n DSK SECOND\n");

            Assert::AreEqual (std::string ("SECOND"), result.outputFileName,
                              L"the name in effect is the last one stated");
        }



        TEST_METHOD (AnObjectFileDirectiveWithNoNameIsReported)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" DSK\n");

            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "names no output file"),
                            MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinUnterminatedConstructTests
    //
    //  The third construct that can be left open at end of file, beside the loop
    //  and the dummy section each of which is covered above.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinUnterminatedConstructTests)
    {
    public:

        TEST_METHOD (AnUnclosedMacroIsReportedAtTheLineItOpenedOn)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " DFB $11\nSHIFT MAC\n ASL\n");

            Assert::AreEqual ((size_t) 1, result.errors.size(), L"one diagnostic, about the definition that never closed");
            Assert::AreEqual (2, result.errors[0].lineNumber, L"reported where the definition opened");

            Assert::IsTrue (result.errors[0].message.find ("SHIFT") != std::string::npos,
                            L"named by the macro the source was defining");
        }
    };
}
