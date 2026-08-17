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
}
