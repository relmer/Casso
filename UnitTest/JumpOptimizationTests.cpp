#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"
#include "Directive.h"
#include "MerlinDialect.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace JumpOptimizationTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  JumpOptimizationFixture
    //
    //  One assembly, over whichever instruction set the case needs.
    //
    //  The NMOS entry point is not a formality: three of AS65's four conditions
    //  are about the source, and the fourth is that there is a BRA to emit at
    //  all. A suite that only ever assembled 65C02 could not tell the
    //  substitution from an unconditional rewrite.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class JumpOptimizationFixture
    {
    public:
        static AssemblyResult Assemble65C02 (const std::string & source, const AssemblerOptions & options)
        {
            TestCpu65C02  cpu;

            cpu.InitForTest();

            {
                Assembler  assembler (cpu.GetInstructionSet(), options);

                return assembler.Assemble (source);
            }
        }



        static AssemblyResult Assemble65C02 (const std::string & source)
        {
            AssemblerOptions  options = {};

            return Assemble65C02 (source, options);
        }



        static AssemblyResult Assemble6502 (const std::string & source)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();

            {
                Assembler  assembler (cpu.GetInstructionSet(), options);

                return assembler.Assemble (source);
            }
        }



        // The bytes as a hex string, so a mismatch reports what was emitted
        // rather than only that a comparison failed.
        static std::wstring Describe (const AssemblyResult & result)
        {
            std::wstring  text;

            for (Byte b : result.bytes)
            {
                wchar_t  pair[4] = {};

                swprintf_s (pair, L"%02X ", b);
                text += pair;
            }

            return text;
        }



        // Every diagnostic, so a source that failed to assemble says why rather
        // than only that it failed.
        static std::wstring Diagnostics (const AssemblyResult & result)
        {
            std::wstring  text;

            for (const AssemblyError & error : result.errors)
            {
                std::string  line = "[" + std::to_string (error.lineNumber) + "] " + error.message + "; ";

                text += std::wstring (line.begin(), line.end());
            }

            return text;
        }



        // The listing row for a source line, so a cycle-count assertion names
        // the line it is about.
        static AssemblyLine LineNumbered (const AssemblyResult & result, int lineNumber)
        {
            AssemblyLine  found = {};

            for (const AssemblyLine & line : result.listing)
            {
                if (line.lineNumber == lineNumber)
                {
                    found = line;
                    break;
                }
            }

            return found;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MeasuredAs65CasesTests
    //
    //  The four shapes measured against a real as65 1.42, one test each. They are
    //  the specification; anything else in this file exists to explain them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MeasuredAs65CasesTests)
    {
    public:

        //  Backward and within reach: two bytes, not three.
        TEST_METHOD (BackwardInRangeJump_BecomesBranch)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        jmp target\n");

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) 3, result.bytes.size(),
                (L"NOP plus a two-byte branch, not a three-byte jump: "
                 + JumpOptimizationFixture::Describe (result)).c_str());
            Assert::AreEqual ((int) 0x80, (int) result.bytes[1], L"BRA is $80");
            Assert::AreEqual ((int) 0xFD, (int) result.bytes[2], L"and reaches back three bytes");
        }



        //  Backward but too far: the displacement does not fit in a signed byte,
        //  so the jump stands.
        TEST_METHOD (BackwardOutOfRangeJump_StaysAJump)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        ds $200\n"
                "        jmp target\n");

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) (1 + 0x200 + 3), result.bytes.size(),
                L"NOP, the reserved span, then a three-byte jump");
            Assert::AreEqual ((int) 0x4C, (int) result.bytes[1 + 0x200], L"JMP absolute is $4C");
            Assert::AreEqual ((int) 0x00, (int) result.bytes[1 + 0x200 + 1], L"low byte of $8000");
            Assert::AreEqual ((int) 0x80, (int) result.bytes[1 + 0x200 + 2], L"high byte of $8000");
        }



        //  A forward reference is never optimized. The target is in reach, and
        //  that is deliberately not enough: as65 decides while sizing, and while
        //  sizing it does not yet know where the label lands.
        TEST_METHOD (ForwardJump_StaysAJump)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "        jmp target\n"
                "        nop\n"
                "target: rts\n");

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) 5, result.bytes.size(),
                (L"a three-byte jump, a NOP and an RTS: "
                 + JumpOptimizationFixture::Describe (result)).c_str());
            Assert::AreEqual ((int) 0x4C, (int) result.bytes[0], L"JMP absolute is $4C");
            Assert::AreEqual ((int) 0x04, (int) result.bytes[1], L"low byte of $8004");
            Assert::AreEqual ((int) 0x80, (int) result.bytes[2], L"high byte of $8004");
        }



        //  NOOPT before the line turns the substitution off.
        TEST_METHOD (NoOptDirective_KeepsTheJump)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        noopt\n"
                "        jmp target\n");

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) 4, result.bytes.size(),
                (L"NOOPT restores the three-byte jump: "
                 + JumpOptimizationFixture::Describe (result)).c_str());
            Assert::AreEqual ((int) 0x4C, (int) result.bytes[1], L"JMP absolute is $4C");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OptimizationControlTests
    //
    //  Who wins when the source and the command line disagree, and the fourth
    //  condition -- that there is a BRA at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OptimizationControlTests)
    {
    public:

        //  Optimization is ON with nothing said, which is what makes the default
        //  output match as65's rather than merely being reachable from it.
        TEST_METHOD (OptIsOnByDefault)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        jmp target\n");

            Assert::AreEqual ((int) 0x80, (int) result.bytes[1],
                L"no OPT in the source and no flag on the command line still means optimized");
        }



        //  NOOPT then OPT: the second directive turns it back on.
        TEST_METHOD (OptAfterNoOpt_RestoresTheBranch)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        noopt\n"
                "        opt\n"
                "        jmp target\n");

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) 3, result.bytes.size(),
                (L"OPT undoes NOOPT: " + JumpOptimizationFixture::Describe (result)).c_str());
            Assert::AreEqual ((int) 0x80, (int) result.bytes[1], L"BRA is $80");
        }



        //  -n outranks an OPT in the source. as65's manual is explicit that the
        //  switch wins "even when the OPT pseudo-instruction is used", so an OPT
        //  after it must NOT turn optimization back on.
        TEST_METHOD (CommandLineDisable_OutranksOptInSource)
        {
            AssemblerOptions  options = {};

            options.disableOpt = true;

            {
                AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                    "        org $8000\n"
                    "target: nop\n"
                    "        opt\n"
                    "        jmp target\n",
                    options);

                Assert::IsTrue (result.success, L"the source must assemble");
                Assert::AreEqual ((size_t) 4, result.bytes.size(),
                    (L"-n is permanent, so the jump stands: "
                     + JumpOptimizationFixture::Describe (result)).c_str());
                Assert::AreEqual ((int) 0x4C, (int) result.bytes[1], L"JMP absolute is $4C");
            }
        }



        //  -n with no directive at all in the source. Seeding and the OPT handler
        //  are separate paths, and only this reaches the seeding one.
        TEST_METHOD (CommandLineDisable_WithNoDirective_KeepsTheJump)
        {
            AssemblerOptions  options = {};

            options.disableOpt = true;

            {
                AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                    "        org $8000\n"
                    "target: nop\n"
                    "        jmp target\n",
                    options);

                Assert::AreEqual ((size_t) 4, result.bytes.size(),
                    (L"-n alone must be enough: " + JumpOptimizationFixture::Describe (result)).c_str());
            }
        }



        //  Without the extended set there is no BRA to emit, so the same source
        //  keeps its jump however the switches are set.
        TEST_METHOD (WithoutExtendedSet_JumpIsNotRewritten)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble6502 (
                "        org $8000\n"
                "target: nop\n"
                "        jmp target\n");

            Assert::IsTrue (result.success, L"the source must assemble on the NMOS set");
            Assert::AreEqual ((size_t) 4, result.bytes.size(),
                (L"the 6502 has no BRA: " + JumpOptimizationFixture::Describe (result)).c_str());
            Assert::AreEqual ((int) 0x4C, (int) result.bytes[1], L"JMP absolute is $4C");
        }



        //  JSR carries the same addressing mode as JMP and must not be touched:
        //  no branch pushes a return address.
        TEST_METHOD (BackwardSubroutineCall_IsNotRewritten)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        jsr target\n");

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) 4, result.bytes.size(),
                (L"JSR is three bytes whatever the target: "
                 + JumpOptimizationFixture::Describe (result)).c_str());
            Assert::AreEqual ((int) 0x20, (int) result.bytes[1], L"JSR is $20");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SubstitutedTimingTests
    //
    //  What `-c` reports for a line whose bytes are not the instruction the
    //  source wrote.
    //
    //  A jump and an always-taken branch are BOTH three cycles, so the number
    //  alone cannot tell a correct lookup from a lucky one. What discriminates is
    //  that the wrong lookup does not resolve at all: JMP has no relative
    //  encoding, so a listing that asked for the written mnemonic against the
    //  substituted mode reports nothing rather than reporting three.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SubstitutedTimingTests)
    {
    public:

        static AssemblerOptions ListingOptions()
        {
            AssemblerOptions  options = {};

            options.generateListing = true;
            options.cycleCounts     = true;

            return options;
        }



        //  The substituted line reports the branch's three cycles. Zero here is
        //  the failure this test exists for: it is what a lookup of the written
        //  mnemonic against the relative mode produces.
        TEST_METHOD (SubstitutedLine_ReportsBranchTiming)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        jmp target\n",
                ListingOptions());

            AssemblyLine    jumpLine = JumpOptimizationFixture::LineNumbered (result, 3);

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) 2, jumpLine.bytes.size(), L"the line emitted a branch");
            Assert::AreEqual ((int) 0x80, (int) jumpLine.bytes[0], L"BRA is $80");
            Assert::AreEqual ((int) 3, (int) jumpLine.cycleCounts,
                L"an always-taken branch is three cycles; 0 means the listing looked up JMP");
        }



        //  A branch the source wrote itself reports the same three. The opcode
        //  slot BRA occupies is illegal on NMOS, so scoring it by opcode alone
        //  gave it no count at all.
        TEST_METHOD (WrittenBranch_ReportsThreeCycles)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "target: nop\n"
                "        bra target\n",
                ListingOptions());

            AssemblyLine    branchLine = JumpOptimizationFixture::LineNumbered (result, 3);

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((int) 3, (int) branchLine.cycleCounts,
                L"BRA is three cycles taken, four across a page boundary");
        }



        //  An unsubstituted jump still reports its own three, so the change did
        //  not simply move every jump's count.
        TEST_METHOD (UnsubstitutedJump_ReportsThreeCycles)
        {
            AssemblyResult  result = JumpOptimizationFixture::Assemble65C02 (
                "        org $8000\n"
                "        jmp target\n"
                "        nop\n"
                "target: rts\n",
                ListingOptions());

            AssemblyLine    jumpLine = JumpOptimizationFixture::LineNumbered (result, 2);

            Assert::IsTrue (result.success, L"the source must assemble");
            Assert::AreEqual ((size_t) 3, jumpLine.bytes.size(), L"the line emitted a jump");
            Assert::AreEqual ((int) 3, (int) jumpLine.cycleCounts, L"JMP absolute is always three");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinListingOptionTests
    //
    //  Merlin's TR, EXP and AST share the token that used to spell as65's OPT and
    //  NOOPT. They steer the listing and change no byte, and splitting the
    //  optimization switches out of that token must have left them exactly as
    //  they were.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinListingOptionTests)
    {
    public:

        static AssemblyResult AssembleMerlin (const std::string & source)
        {
            TestCpu           cpu;
            TestCpu65C02      cmos;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = DialectId::Merlin;

            {
                Assembler  assembler (cpu.GetInstructionSet(), cmos.GetInstructionSet(), options);

                return assembler.Assemble (source);
            }
        }



        //  Present or absent, the object is the same.
        TEST_METHOD (TrExpAst_ChangeNoByte)
        {
            AssemblyResult  plain = AssembleMerlin (
                "         ORG $8000\n"
                "         LDA #$01\n"
                "         RTS\n");

            AssemblyResult  optioned = AssembleMerlin (
                "         ORG $8000\n"
                "         TR\n"
                "         EXP ON\n"
                "         AST 20\n"
                "         LDA #$01\n"
                "         RTS\n");

            Assert::IsTrue (plain.success,    L"the plain source must assemble");
            Assert::IsTrue (optioned.success, L"and so must the one carrying the listing options");
            Assert::IsTrue (plain.bytes == optioned.bytes,
                L"TR, EXP and AST steer the listing and must emit nothing");
        }



        //  And they still resolve to the no-op token rather than to either of
        //  the switches split out of it.
        TEST_METHOD (TrExpAst_StillResolveToTheNoOpToken)
        {
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("TR")  == Directive::OptNoop,
                L"TR must still be the directive that does nothing");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("EXP") == Directive::OptNoop,
                L"EXP must still be the directive that does nothing");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("AST") == Directive::OptNoop,
                L"AST must still be the directive that does nothing");
        }



        //  Merlin's own assembler does not rewrite jumps, so sharing the engine
        //  with as65 must not have given it the behavior. XC selects the 65C02,
        //  which is the only way a BRA could be reached at all.
        TEST_METHOD (Merlin_DoesNotRewriteJumps)
        {
            AssemblyResult  result = AssembleMerlin (
                "         ORG $8000\n"
                "         XC\n"
                "TARGET   NOP\n"
                "         JMP TARGET\n");

            Assert::IsTrue (result.success,
                (L"the source must assemble: " + JumpOptimizationFixture::Diagnostics (result)).c_str());
            Assert::AreEqual ((size_t) 4, result.bytes.size(),
                L"Merlin writes the three-byte jump the source asked for");
            Assert::AreEqual ((int) 0x4C, (int) result.bytes[1], L"JMP absolute is $4C");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OptimizationSpellingTests
    //
    //  The vocabulary split itself: OPT and NOOPT must be distinguishable, which
    //  they were not while both resolved to one token.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OptimizationSpellingTests)
    {
    public:

        TEST_METHOD (OptAndNoOpt_AreDifferentTokens)
        {
            Assert::IsTrue (DirectiveTable::FromSpelling ("OPT")   == Directive::Optimize,
                L"OPT turns optimization on");
            Assert::IsTrue (DirectiveTable::FromSpelling ("NOOPT") == Directive::NoOptimize,
                L"NOOPT turns it off");
            Assert::IsTrue (Directive::Optimize != Directive::NoOptimize,
                L"and they cannot be one token, or neither can steer anything");
        }



        TEST_METHOD (DottedSpellings_ResolveToTheSameTokens)
        {
            Assert::IsTrue (DirectiveTable::FromSpelling (".OPT")   == Directive::Optimize,
                L"the dotted canonical form of OPT");
            Assert::IsTrue (DirectiveTable::FromSpelling (".NOOPT") == Directive::NoOptimize,
                L"the dotted canonical form of NOOPT");
        }
    };
}
