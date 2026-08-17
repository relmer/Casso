#include "Pch.h"

#include "TestHelpers.h"
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



        //  A label sharing a line with an origin binds to the OUTPUT position.
        //  `MAKE DUMP`'s loader copies its interface section to page 3 and needs
        //  to know where that section sits in the file it was loaded from, so
        //  binding the label to the relocated address would be silently wrong --
        //  and it would still assemble.
        TEST_METHOD (ALabelOnAnOriginLineBindsToTheOutputPosition)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         " ORG $9000\n DFB $11\n"
                                         "HERE ORG $300\n DFB $22\n");

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x9001, (int) result.symbols.at ("HERE"),
                              L"HERE is where the relocated section sits in the file, not where it runs");
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
        //  than a Merlin one. The VALUE differs only because the dialects differ
        //  about what the origin did to the output cursor -- one rule, two
        //  answers, no branch.
        TEST_METHOD (As65BindsALabelOnAnOriginLineToTheAddressItSeekedTo)
        {
            AssemblyResult  result = MerlinAssemblyFixture::Assemble (
                                         " .org $9000\n .byte $11\n"
                                         "HERE: .org $300\n .byte $22\n", DialectId::As65);

            Assert::IsTrue (result.errors.empty(), MerlinAssemblyFixture::FirstDiagnostic (result).c_str());
            Assert::AreEqual (0x0300, (int) result.symbols.at ("HERE"),
                              L"where the origin seeks, the output cursor IS the new address");
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



        //  Merlin also lets a variable stand where a label stands, binding to the
        //  program counter. Casso does NOT, and refuses loudly rather than
        //  quietly binding one: pass 2 resolves every reference against a single
        //  symbol table, so a program-counter symbol assigned more than once would
        //  point every branch at the last copy. A wrong branch target that
        //  assembles is worse than a refusal.
        TEST_METHOD (AVariableStandingAsAProgramCounterLabelIsRefused)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (
                                         "]LOOP LDA #0\n"
                                         " BNE ]LOOP\n");

            Assert::IsFalse (result.errors.empty(), L"the unsupported form must not assemble silently");
        }
    };
}
