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



        //  A trailing byte after the closing delimiter is REFUSED rather than
        //  dropped. The vendor sources contain the form, but nothing reachable
        //  today pins what the trailing digits mean, and emitting the string
        //  without them would be plausible and wrong.
        TEST_METHOD (TrailingDataAfterAStringIsRefusedRatherThanDropped)
        {
            AssemblyResult  result = MerlinAssemblyFixture::AssembleMerlin (" ASC \"AB\"0D\n");

            Assert::IsFalse (result.errors.empty(), L"silently dropping the trailing byte would emit short data");
            Assert::IsTrue (MerlinAssemblyFixture::AnyErrorMentions (result, "Trailing data"),
                            L"and the diagnostic must say what was refused");
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
}
