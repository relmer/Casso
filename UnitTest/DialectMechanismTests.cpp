#include "Pch.h"

#include "Assembler.h"
#include "Dialect.h"
#include "Directive.h"
#include "MerlinDialect.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"
#include "InstructionSetProvider.h"
#include "Parser.h"
#include "TestHelpers.h"
#include "TestCpu65C02.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DialectMechanismTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PipeDialect
    //
    //  A THIRD dialect, invented here, existing only to fail if the mechanism has
    //  quietly been built for exactly the two that ship.
    //
    //  It is deliberately not a copy of either. Its fields are separated by a
    //  literal character rather than by whitespace or by column, so neither
    //  existing line model can read it; its comment introducer is a third
    //  character; its directive spellings share nothing with either vocabulary;
    //  its default origin is a third address; and its local-label sigil is a
    //  character neither dialect uses.
    //
    //  MOST IMPORTANTLY it declares the OPPOSITE origin semantic from AS65. That
    //  is the whole point of adding it now rather than earlier: a synthetic
    //  profile gets written to fit whatever seam exists, so one that happened to
    //  agree with AS65 on every axis would pass while exercising none of them.
    //  The axis the emit-cursor split introduced is the newest and the easiest to
    //  build a two-dialect assumption into, so this profile takes the far side of
    //  it and the tests below assert that it does.
    //
    //  It is NOT in the registry, and a test below checks that it never quietly
    //  becomes so. It reaches the engine through AssemblerOptions::dialectProfile,
    //  which exists precisely because a closed table cannot demonstrate that a
    //  dialect outside it would work.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PipeDialect : public DialectProfile
    {
    public:

        //  Claims no enumerator, because it is not one. The registry sweeps walk
        //  DialectId and the registry table; a profile that belongs to neither
        //  must not answer to a value either sweep will visit, or it would be
        //  found by a test that has no business finding it.
        DialectId           GetId() const override { return DialectId::Count; }
        const char *        GetName() const override { return "pipe"; }

        CpuSelectionSource  GetCpuSelectionSource() const override { return CpuSelectionSource::CommandLine; }
        const char *        GetCpuDirectiveName() const override { return ""; }

        //  A third default origin, so a test asserting this one cannot be
        //  satisfied by either shipped dialect's.
        Word                GetDefaultOrigin() const override { return 0x0400; }

        //  The far side of the axis the emit-cursor split added.
        OriginSemantic      GetOriginSemantic() const override { return OriginSemantic::ProgramCounterOnly; }

        //  ...and the near side of the other two new axes, so this profile is a
        //  genuine third point rather than a second copy of Merlin.
        OperandlessForm     GetOperandlessForm() const override { return OperandlessForm::ImpliedOnly; }
        char                GetHighAsciiCharDelimiter() const override { return 0; }

        char                GetLocalLabelPrefix() const override { return '%'; }

        //  Fields separated by a literal pipe: `label|mnemonic|operand`. Neither
        //  shipped line model can read this, which is the point -- if the engine
        //  could still assemble it by accident, the seam would not be where it
        //  claims to be.
        ParsedLine ParseLine (const std::string & line, int lineNumber) const override
        {
            ParsedLine  result = {};
            size_t      first  = 0;
            size_t      second = 0;

            result.lineNumber      = lineNumber;
            result.isEmpty         = true;
            result.startsAtColumn0 = true;

            if (line.empty() || (line[0] == kCommentIntroducer))
            {
                return result;
            }

            first  = line.find (kFieldSeparator);
            second = (first == std::string::npos) ? std::string::npos
                                                  : line.find (kFieldSeparator, first + 1);

            if (first == std::string::npos)
            {
                return result;
            }

            result.label    = line.substr (0, first);
            result.mnemonic = (second == std::string::npos)
                                  ? Parser::ToUpper (line.substr (first + 1))
                                  : Parser::ToUpper (line.substr (first + 1, second - first - 1));
            result.operand  = (second == std::string::npos) ? "" : line.substr (second + 1);

            result.directiveToken = TokenForSpelling (result.mnemonic);
            result.isDirective    = (result.directiveToken != Directive::None);

            if (result.isDirective)
            {
                result.directive    = result.mnemonic;
                result.directiveArg = result.operand;
            }

            result.isEmpty = result.label.empty() && result.mnemonic.empty();

            return result;
        }

    private:

        static constexpr char  kFieldSeparator    = '|';
        static constexpr char  kCommentIntroducer = '!';

        //  Two directives, spelled like nothing in either shipped vocabulary, so
        //  a table consulted by spelling instead of by token finds neither.
        static Directive TokenForSpelling (const std::string & word)
        {
            Directive  token = Directive::None;

            if (word == "SEEK")
            {
                token = Directive::Org;
            }
            else if (word == "EMIT")
            {
                token = Directive::Byte;
            }

            return token;
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SyntheticDialectTests
    //
    //  SC-009, proved rather than asserted: a dialect the mechanism has never
    //  heard of assembles through the shared engine, the shared evaluator and the
    //  shared opcode tables, with no edit to any of them.
    //
    //  The companion to this class is a git check, not a test. T070 requires that
    //  the commit adding this file's synthetic profile touch none of
    //  AssemblySession.cpp, ExpressionEvaluator.cpp or OpcodeTable.cpp -- which is
    //  a property of the commit and cannot be expressed here.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SyntheticDialectTests)
    {
    public:

        //  The claim that makes every test below worth running. A synthetic
        //  profile agreeing with AS65 on this axis would exercise none of the
        //  emit-cursor machinery while looking exactly as green.
        TEST_METHOD (TheSyntheticProfileDeclaresTheOppositeOriginSemanticFromAs65)
        {
            PipeDialect             pipe;
            const DialectProfile &  as65 = DialectRegistry::Get (DialectId::As65);

            Assert::IsTrue (as65.GetOriginSemantic() == OriginSemantic::MovesOutputCursor,
                            L"AS65 seeks, and this test is meaningless if that ever stops being true");
            Assert::IsTrue (pipe.GetOriginSemantic() == OriginSemantic::ProgramCounterOnly,
                            L"the synthetic profile must take the FAR side of the axis, or it tests nothing");
            Assert::IsFalse (pipe.GetOriginSemantic() == as65.GetOriginSemantic(),
                             L"same semantic as AS65 would leave the newest axis unexercised");
        }



        //  End to end: a grammar neither shipped dialect can read, assembled by
        //  the shared engine into the bytes the shared opcode table encodes.
        TEST_METHOD (ASyntheticProfileAssemblesThroughTheSharedEngine)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00, 0x03, 0x60 };
            AssemblyResult     result   = AssemblePipe ("!a comment in a third character\n"
                                                        "START|LDA|#$41\n"
                                                        "|STA|$0300\n"
                                                        "|RTS\n");

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"a third grammar must reach the same encoder");
            Assert::AreEqual (0x0400, (int) result.startAddress,
                              L"and take its own default origin, which is neither shipped dialect's");
            Assert::AreEqual (0x0400, (int) result.symbols.at ("START"), L"with its labels bound where it says");
        }



        //  The axis itself, end to end. Two sections at two addresses arriving as
        //  four consecutive bytes is something no seeking dialect can produce.
        TEST_METHOD (TheSyntheticProfilesOriginRelocatesWithoutMovingTheOutput)
        {
            std::vector<Byte>  expected = { 0x11, 0x22, 0x33, 0x44 };
            AssemblyResult     result   = AssemblePipe ("|SEEK|$0400\n"
                                                        "|EMIT|$11,$22\n"
                                                        "HERE|SEEK|$8000\n"
                                                        "|EMIT|$33,$44\n");

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());
            Assert::IsTrue (result.bytes == expected, L"the output must stay contiguous across the origin");
            Assert::AreEqual (0x0400, (int) result.startAddress, L"and load where the first origin put it");
            Assert::AreEqual (0x0402, (int) result.symbols.at ("HERE"),
                              L"a label on the origin line takes the output position, as it does for any dialect");
        }



        //  A label local to the one above it, through a sigil neither shipped
        //  dialect uses. The local-label seam is stateful engine behavior driven
        //  by one character of profile data, which is exactly the shape most
        //  likely to have been written for the dialect that needed it.
        TEST_METHOD (TheSyntheticProfilesLocalLabelSigilScopesThroughTheSharedEngine)
        {
            AssemblyResult  result = AssemblePipe ("ONE|EMIT|$11\n"
                                                   "%SPOT|EMIT|$22\n"
                                                   "TWO|EMIT|$33\n"
                                                   "%SPOT|EMIT|$44\n");

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());
            Assert::AreEqual ((size_t) 4, result.bytes.size(),
                              L"the same local name under two globals is two symbols, not a duplicate");
        }



        //  The discriminating half. The engine must not be able to read this
        //  grammar without the profile, or none of the above is evidence.
        TEST_METHOD (TheSameSourceUnderAs65DoesNotAssemble)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};
            AssemblyResult    result;

            cpu.InitForTest();

            {
                Assembler  as65 (cpu.GetInstructionSet(), options);

                result = as65.Assemble ("START|LDA|#$41\n|STA|$0300\n|RTS\n");
            }

            Assert::IsFalse (result.success,
                             L"AS65 reading a pipe-separated grammar would mean the seam is decoration");
        }



        //  The guard on the guard. A synthetic profile smuggled into the registry
        //  stops being synthetic, and the registry sweeps would then be covering
        //  a dialect nothing ships.
        TEST_METHOD (TheSyntheticProfileIsNotInTheRegistry)
        {
            DialectId  found    = DialectId::As65;
            bool       wasFound = DialectRegistry::TryLookUpByName ("pipe", found);

            Assert::IsFalse (wasFound, L"the third profile must stay outside the shipped table");

            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                Assert::IsFalse (entry.id == DialectId::Count,
                                 L"and no registry row may claim the sentinel it answers to");
            }
        }

    private:

        static AssemblyResult AssemblePipe (const std::string & source)
        {
            PipeDialect       pipe;
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialectProfile = &pipe;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



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
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialectRegistryTests
    //
    //  The mechanism itself, tested apart from any one dialect.
    //
    //  These sweep the registry rather than naming dialects, so a dialect added
    //  to the table is covered without anyone editing this file -- the same
    //  reason DirectiveTokenTests sweeps DirectiveTable::GetAllSpellings and
    //  CommandLineTests sweeps CommandLineParser::GetAllSubcommands.
    //
    //  The synthetic third profile that proves SC-009 is above, and it landed
    //  only once Merlin had pressed on the seam with its field model, its
    //  operand-internal separators, its quoted operands, its mnemonic aliases,
    //  its macro syntax and its origin semantic. Written earlier it would have
    //  passed trivially, because a synthetic profile gets written to fit
    //  whatever seam exists.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialectRegistryTests)
    {
    public:

        //  Sweeps the ENUM, not the table, which is what catches the failure a
        //  table sweep structurally cannot report: an enumerator added without a
        //  profile behind it. Every row a table sweep visits has a profile by
        //  construction, so only the enum side can find the gap.
        //
        //  Valid because DialectId is TOTAL over the profiles. The same sweep
        //  against a partial enum -- CommandLineOptions::Subcommand, where
        //  several values deliberately have no table row -- would fail
        //  correctly-shaped code. DirectiveTokenTests sweeps both directions for
        //  the same reason.
        TEST_METHOD (EveryDialectIdResolvesToAProfile)
        {
            int  idCount = (int) DialectId::Count;



            for (int i = 0; i < idCount; i++)
            {
                DialectId               id      = (DialectId) i;
                const DialectProfile  & profile = DialectRegistry::Get (id);

                Assert::IsTrue (profile.GetId() == id,
                                L"every DialectId must resolve to the profile that claims it");
            }
        }



        //  Sweeps the table rather than a hand-picked sample, so a dialect added
        //  to it is covered without anyone editing this test.
        TEST_METHOD (EveryTableRowNamesItsOwnProfile)
        {
            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                Assert::IsNotNull (entry.profile, L"a table row must carry a profile");
                Assert::IsTrue (entry.profile->GetId() == entry.id,
                                L"a row's profile must report the id the row names");
                Assert::AreEqual (std::string (entry.name), std::string (entry.profile->GetName()),
                                  L"a row's profile must report the name the row names");
            }
        }



        TEST_METHOD (EveryTableNameLooksUpToItsOwnId)
        {
            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                DialectId  found    = DialectId::Count;
                bool       wasFound = DialectRegistry::TryLookUpByName (entry.name, found);

                Assert::IsTrue (wasFound, L"a table row must be findable by its own name");
                Assert::IsTrue (found == entry.id, L"a name must look up to the id its row names");
            }
        }



        TEST_METHOD (DialectNamesAreUnique)
        {
            std::span<const DialectRegistry::Entry>  dialects = DialectRegistry::GetAllDialects();



            for (size_t i = 0; i < dialects.size(); i++)
            {
                for (size_t j = i + 1; j < dialects.size(); j++)
                {
                    Assert::AreNotEqual (std::string (dialects[i].name), std::string (dialects[j].name),
                                         L"two dialects must not answer to the same name");
                }
            }
        }



        TEST_METHOD (UnknownName_IsNotFound)
        {
            DialectId  found    = DialectId::As65;
            bool       wasFound = DialectRegistry::TryLookUpByName ("nosuchdialect", found);

            Assert::IsFalse (wasFound, L"an unrecognized word names no dialect");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialectSeamTests
    //
    //  That parsing actually goes THROUGH the seam, rather than the seam being
    //  decoration over a call that still hard-codes one grammar.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialectSeamTests)
    {
    public:

        //  The defaulted overload must agree with routing through AS65
        //  explicitly. If these ever diverge, the default has stopped being the
        //  dialect it claims to be.
        TEST_METHOD (DefaultOverload_MatchesExplicitAs65)
        {
            const DialectProfile  & as65    = DialectRegistry::Get (DialectId::As65);
            std::string             source  = "LOOP:  LDA #$41  ; a comment";
            ParsedLine  implied    = Parser::ParseLine (source, 7);
            ParsedLine  explicitly = Parser::ParseLine (source, 7, as65);

            Assert::AreEqual (implied.label,      explicitly.label);
            Assert::AreEqual (implied.mnemonic,   explicitly.mnemonic);
            Assert::AreEqual (implied.operand,    explicitly.operand);
            Assert::AreEqual (implied.lineNumber, explicitly.lineNumber);
        }



        TEST_METHOD (ProfileParsesThroughTheSeam)
        {
            const DialectProfile  & as65   = DialectRegistry::Get (DialectId::As65);
            ParsedLine              direct = as65.ParseLine ("  LDA #$41", 3);
            ParsedLine              routed = Parser::ParseLine ("  LDA #$41", 3, as65);

            Assert::AreEqual (direct.mnemonic, routed.mnemonic);
            Assert::AreEqual (direct.operand,  routed.operand);
        }



        //  as65 has no in-source CPU directive, so a command-line target is the
        //  only place one can come from. This is the field the command-line
        //  parser will consult instead of branching on the dialect itself.
        TEST_METHOD (As65_TakesItsCpuFromTheCommandLine)
        {
            const DialectProfile  & as65 = DialectRegistry::Get (DialectId::As65);

            Assert::IsTrue (as65.GetCpuSelectionSource() == CpuSelectionSource::CommandLine);
            Assert::AreEqual (std::string (""), std::string (as65.GetCpuDirectiveName()),
                              L"a command-line dialect has no in-source directive to name");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InstructionSetProviderTests
    //
    //  The CPU axis of the mechanism: holding more than one instruction table so
    //  an in-source directive has something to select.
    //
    //  No dialect selects yet, which is exactly why these exist -- the machinery
    //  would otherwise be untested until the first dialect that uses it, and a
    //  fault in it would surface as wrong bytes rather than as a failure here.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InstructionSetProviderTests)
    {
    public:

        //  One set: nothing to switch to, and asking for the extended one
        //  honestly returns what you already had rather than a null table every
        //  caller would have to test for.
        TEST_METHOD (SingleSet_HasNoExtendedAndFallsBackToBase)
        {
            TestCpu                 cpu;
            InstructionSetProvider  provider (cpu.GetInstructionSet());

            Assert::IsFalse (provider.HasExtended(), L"one set means nothing to select");
            Assert::IsTrue (&provider.GetExtended() == &provider.GetBase(),
                            L"with no extended set, the extended table IS the base table");
        }



        TEST_METHOD (TwoSets_ReportsAnExtendedSet)
        {
            TestCpu                 cpu;
            TestCpu65C02            cmos;
            InstructionSetProvider  provider (cpu.GetInstructionSet(), cmos.GetInstructionSet());

            Assert::IsTrue (provider.HasExtended(), L"two sets means there is something to select");
            Assert::IsTrue (&provider.GetExtended() != &provider.GetBase(),
                            L"the extended table must be distinct from the base");
        }



        //  The tables must differ in the way that matters: the extended one
        //  encodes instructions the base rejects. Identical tables would satisfy
        //  every structural assertion above while making selection pointless.
        TEST_METHOD (ExtendedSet_EncodesInstructionsTheBaseRejects)
        {
            TestCpu                 cpu;
            TestCpu65C02            cmos;
            InstructionSetProvider  provider (cpu.GetInstructionSet(), cmos.GetInstructionSet());

            Assert::IsFalse (provider.GetBase().IsMnemonic ("PHX"),
                             L"PHX is not a 6502 instruction");
            Assert::IsTrue (provider.GetExtended().IsMnemonic ("PHX"),
                            L"PHX is a 65C02 instruction, so the extended table must carry it");
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DirectiveTokenTotalityTests
    //
    //  The totality claim that survives having more than one dialect.
    //
    //  DirectiveTokenTests used to sweep every Directive and require an as65
    //  canonical spelling. That held only while as65 was the only dialect. Merlin
    //  brought tokens as65 must NOT accept -- FR-005 forbids admitting one
    //  dialect's constructs into another -- so the old claim became false without
    //  anything being wrong.
    //
    //  This is the weaker claim that is still true and still worth having: a
    //  token belonging to NO dialect is unreachable, which is exactly the bug the
    //  original sweep was defending against. It lives here because it is the only
    //  place both spelling tables are in scope.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DirectiveTokenTotalityTests)
    {
    public:

        TEST_METHOD (EveryDirectiveTokenIsClaimedByADialect)
        {
            int  token = 0;

            for (token = 1; token < (int) Directive::Count; token++)
            {
                Directive     which    = (Directive) token;
                const char *  as65Name = DirectiveTable::GetCanonicalName (which);
                const char *  merlName = MerlinDirectiveTable::GetCanonicalName (which);
                std::wstring  message  = L"Directive token " + std::to_wstring (token)
                                       + L" has no spelling in any dialect, so nothing can ever produce it";

                Assert::IsTrue ((as65Name[0] != '\0') || (merlName[0] != '\0'), message.c_str());
            }
        }



        //  Every Merlin spelling resolves to its own token, sweeping the table
        //  rather than a sample -- so a spelling added later is covered without
        //  anyone editing this test.
        TEST_METHOD (EveryMerlinSpellingResolvesToItsOwnToken)
        {
            for (const MerlinDirectiveTable::Spelling & entry : MerlinDirectiveTable::GetAllSpellings())
            {
                std::wstring  message = L"Merlin spelling did not resolve to its own token";

                Assert::IsTrue (MerlinDirectiveTable::FromSpelling (entry.name) == entry.token, message.c_str());
            }
        }



        //  as65 must not have quietly gained Merlin's vocabulary. This is the
        //  guard on FR-005 at the table level: HEX and DCI are Merlin words, and
        //  an as65 source using them must still fail.
        TEST_METHOD (As65DoesNotAcceptMerlinOnlySpellings)
        {
            Assert::IsTrue (DirectiveTable::FromSpelling ("HEX")  == Directive::None, L"HEX is not an as65 directive");
            Assert::IsTrue (DirectiveTable::FromSpelling ("DCI")  == Directive::None, L"DCI is not an as65 directive");
            Assert::IsTrue (DirectiveTable::FromSpelling (".HEX") == Directive::None, L"and no dotted form exists either");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MnemonicAliasTests
    //
    //  The instruction-layer twin of the directive spelling table.
    //
    //  Swept rather than sampled, and swept over the ENUM of dialects rather than
    //  over one profile's rows: an alias whose target is not a real instruction
    //  is not an alias at all. It silently becomes an unknown mnemonic on every
    //  line that uses it, which reads as the dialect not supporting the construct
    //  rather than as a one-character typo in a table.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MnemonicAliasTests)
    {
    public:

        TEST_METHOD (EveryAliasTargetsAnInstructionTheOpcodeTableCarries)
        {
            TestCpu  cpu;
            size_t   aliasCount = 0;

            cpu.InitForTest();

            {
                OpcodeTable  table (cpu.GetInstructionSet());

                for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
                {
                    const DialectProfile & profile = DialectRegistry::Get (entry.id);

                    for (const MnemonicAlias & alias : profile.GetMnemonicAliases())
                    {
                        std::string   spelling = alias.spelling;
                        std::string   target   = alias.instruction;
                        std::wstring  message  = L"an alias must name an instruction the base table carries";

                        aliasCount++;

                        Assert::IsTrue (table.IsMnemonic (target), message.c_str());
                        Assert::IsFalse (table.IsMnemonic (spelling),
                                         L"an alias spelled the same as a real instruction would shadow it");
                    }
                }
            }

            //  A loop over an empty table passes while checking nothing, and is
            //  indistinguishable in the output from a full one.
            Assert::IsTrue (aliasCount > 0, L"no dialect declared an alias, so this swept nothing");
        }



        //  FR-005's guard at the table level. as65 must not acquire Merlin's
        //  instruction spellings any more than it acquires Merlin's directives.
        TEST_METHOD (As65DeclaresNoInstructionAliases)
        {
            const DialectProfile & as65 = DialectRegistry::Get (DialectId::As65);

            Assert::AreEqual (static_cast<size_t> (0), as65.GetMnemonicAliases().size(),
                              L"as65 spells every instruction the way the opcode table does");
        }



        //  Merlin's two, named rather than swept: the sweep above proves the
        //  rows are well formed and would pass over an empty-but-for-one table.
        TEST_METHOD (MerlinDeclaresTheTwoCarryBranchAliases)
        {
            const DialectProfile & merlin  = DialectRegistry::Get (DialectId::Merlin);
            bool                   sawBlt  = false;
            bool                   sawBge  = false;

            for (const MnemonicAlias & alias : merlin.GetMnemonicAliases())
            {
                std::string  spelling = alias.spelling;
                std::string  target   = alias.instruction;

                sawBlt = sawBlt || ((spelling == "BLT") && (target == "BCC"));
                sawBge = sawBge || ((spelling == "BGE") && (target == "BCS"));
            }

            Assert::IsTrue (sawBlt, L"BLT is Merlin's name for BCC");
            Assert::IsTrue (sawBge, L"BGE is Merlin's name for BCS");
        }



        //  Resolution happens at parse time, so nothing downstream ever sees the
        //  alternate name. Asserted on the ParsedLine rather than on emitted
        //  bytes, because this is the property the rest of the engine relies on.
        TEST_METHOD (ParsingRewritesTheAliasToItsInstruction)
        {
            const DialectProfile & merlin = DialectRegistry::Get (DialectId::Merlin);
            ParsedLine             parsed = Parser::ParseLine (" BLT AHEAD", 1, merlin);

            Assert::AreEqual (std::string ("BCC"), parsed.mnemonic,
                              L"the mnemonic reaching the engine must be the opcode table's name");
            Assert::AreEqual (std::string ("AHEAD"), parsed.operand, L"and the operand must be untouched");
        }



        //  The AS65 half. Its parse must not rewrite anything, or the alias
        //  mechanism has leaked into the dialect that does not declare one.
        TEST_METHOD (As65LeavesTheSameSpellingAlone)
        {
            const DialectProfile & as65   = DialectRegistry::Get (DialectId::As65);
            ParsedLine             parsed = Parser::ParseLine ("        blt ahead", 1, as65);

            Assert::AreEqual (std::string ("BLT"), parsed.mnemonic,
                              L"as65 has no alias table, so the word stays what the source wrote");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DirectiveTokenComparisonTests
    //
    //  The class of bug this feature exists to avoid, at the three sites where it
    //  was still latent: a directive recognized by its CANONICAL SPELLING rather
    //  than by its token.
    //
    //  The failure mode is silent. A dialect spelling the directive its own way
    //  parses correctly and resolves to exactly the right token, then falls
    //  through every comparison and does nothing at all -- no bytes, no
    //  diagnostic, and an assembly that reports success.
    //
    //  These pin the parse-level FACT that makes the trap real. The behavior each
    //  site drives is exercised where that behavior lives: Merlin's macro
    //  terminator in MerlinDirectiveTests, and the struct terminator below, which
    //  no second dialect can reach today because opening a struct is an as65-only
    //  token.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DirectiveTokenComparisonTests)
    {
    public:

        //  Merlin's END is the same token as as65's .END and shares none of its
        //  spelling. Any site comparing the string sees nothing here.
        TEST_METHOD (MerlinsEndCarriesTheTokenAndNotTheDottedSpelling)
        {
            const DialectProfile & merlin = DialectRegistry::Get (DialectId::Merlin);
            ParsedLine             parsed = Parser::ParseLine (" END", 1, merlin);

            Assert::IsTrue (parsed.directiveToken == Directive::End, L"END resolves to the end token");
            Assert::AreNotEqual (std::string (".END"), parsed.directive,
                                 L"and carries Merlin's spelling, which is what makes a string compare miss it");
        }



        //  Same shape for the macro terminator, which IS reachable from Merlin
        //  and is driven end to end in MerlinDirectiveTests.
        TEST_METHOD (MerlinsMacroTerminatorCarriesTheTokenAndNotTheDottedSpelling)
        {
            const DialectProfile & merlin = DialectRegistry::Get (DialectId::Merlin);
            ParsedLine             parsed = Parser::ParseLine (" <<<", 1, merlin);

            Assert::IsTrue (parsed.directiveToken == Directive::MacroEnd, L"<<< resolves to the macro-end token");
            Assert::AreNotEqual (std::string (".ENDM"), parsed.directive, L"and is spelled nothing like as65's");
        }



        //  as65's own two spellings of the struct terminator, kept as the
        //  regression pin for converting that site to the token. Both reach the
        //  same token, which is why the conversion is invisible to as65 -- and
        //  why no test can discriminate it: Directive::Struct is as65-only, so
        //  no second dialect can be inside a struct body to be affected.
        TEST_METHOD (BothAs65SpellingsOfTheStructTerminatorReachTheSameToken)
        {
            const DialectProfile & as65   = DialectRegistry::Get (DialectId::As65);
            ParsedLine             dotted = Parser::ParseLine ("        .end struct", 1, as65);
            ParsedLine             bare   = Parser::ParseLine ("        end struct", 1, as65);

            Assert::IsTrue (dotted.directiveToken == Directive::End, L".end resolves to the end token");
            Assert::IsTrue (bare.directiveToken   == Directive::End, L"and so does the bare spelling");
            Assert::AreEqual (std::string ("STRUCT"), Parser::ToUpper (dotted.directiveArg),
                              L"what it closes is the argument, not part of the directive");
            Assert::AreEqual (std::string ("STRUCT"), Parser::ToUpper (bare.directiveArg),
                              L"the bare form reaches it the same way");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DirectiveInstructionCollisionTests
    //
    //  A spelling that names both a directive and an instruction, and the rule
    //  that keeps the answer from depending on which table is asked first.
    //
    //  The rule is DISJOINTNESS, plus one sanctioned escape hatch. A spelling in
    //  a dialect's directive table is a directive, and that table may not hold a
    //  spelling the instruction tables answer to -- when the two are disjoint,
    //  consulting either first gives the same answer, which is exactly the
    //  property required. A spelling that genuinely is both stays OUT of the main
    //  table and is resolved from the operand instead, which is what
    //  `DirectiveTable::FromAmbiguousSpelling` holds and what `RMB` demonstrates
    //  below.
    //
    //  Swept from the registry against the real opcode tables rather than from a
    //  list written here. A collision arrives as a table edit somebody makes for
    //  another reason -- a dialect adding a spelling, or an opcode gaining a
    //  mnemonic -- and neither edit passes near a hand-written list.
    //
    //  BOTH instruction sets, because the extended one carries mnemonics the base
    //  does not and a dialect selecting it in source reaches every one of them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DirectiveInstructionCollisionTests)
    {
    public:

        //  Every spelling either instruction table answers to, asked of every
        //  dialect in the registry.
        static void AssertNoDialectClaims (const OpcodeTable & table, const wchar_t * whichSet, size_t & outAsked)
        {
            std::vector<std::string>  mnemonics = table.GetAllMnemonics();

            Assert::IsFalse (mnemonics.empty(), whichSet);

            for (const std::string & mnemonic : mnemonics)
            {
                for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
                {
                    const DialectProfile  &  profile = DialectRegistry::Get (entry.id);
                    Directive                claimed = profile.GetDirectiveForSpelling (mnemonic);
                    std::wstring             what    = std::wstring (mnemonic.begin(), mnemonic.end())
                                                       + L" is an instruction and this dialect's directive table"
                                                         L" claims it, so which one it is depends on lookup order";

                    outAsked++;

                    Assert::IsTrue (claimed == Directive::None, what.c_str());
                }
            }
        }



        TEST_METHOD (NoDialectSpellsADirectiveTheWayAnInstructionIsSpelled)
        {
            TestCpu       cpu;
            TestCpu65C02  cmos;
            size_t        asked = 0;

            cpu.InitForTest();
            cmos.InitForTest();

            {
                OpcodeTable  base     (cpu.GetInstructionSet());
                OpcodeTable  extended (cmos.GetInstructionSet());

                AssertNoDialectClaims (base,     L"the base instruction table answered to no mnemonic at all",     asked);
                AssertNoDialectClaims (extended, L"the extended instruction table answered to no mnemonic at all", asked);
            }

            //  A sweep over an empty registry passes while asking nothing, and
            //  reads in the output exactly like a full one.
            Assert::IsTrue (asked > 0, L"no dialect was asked about any mnemonic");
        }



        //  The escape hatch, shown working rather than described. as65 writes
        //  `rmb <count>` for reserved storage and `rmb <bit>,<zp>` for the
        //  Rockwell instruction, so the spelling is genuinely both.
        //
        //  It is a case the sweep above CANNOT see, which is why both exist. The
        //  opcode table answers to `RMB0` through `RMB7` and not to the bare
        //  word; the bit form is normalized into one of those from the operand,
        //  so a bare `RMB` in the main table collides with nothing the sweep can
        //  ask about and silently turns every Rockwell RMB into storage.
        TEST_METHOD (AGenuinelyAmbiguousSpellingLivesOutsideTheMainTable)
        {
            TestCpu65C02  cmos;

            cmos.InitForTest();

            {
                OpcodeTable  extended (cmos.GetInstructionSet());

                Assert::IsTrue (extended.IsMnemonic ("RMB0"), L"the Rockwell bit instruction has to exist for this to mean anything");
                Assert::IsTrue (DirectiveTable::FromSpelling ("RMB") == Directive::None,
                                L"the main table must not claim it, or the instruction could never win");
                Assert::IsTrue (DirectiveTable::FromAmbiguousSpelling ("RMB") == Directive::Ds,
                                L"and the ambiguous table is where the directive reading lives");
            }
        }



        //  Both readings of that spelling, driven through the assembler, so the
        //  claim is about what as65 does rather than about which table holds a
        //  row. The operand is the whole difference.
        TEST_METHOD (TheOperandDecidesWhichReadingTheAmbiguousSpellingGets)
        {
            TestCpu           cpu;
            TestCpu65C02      cmos;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            cmos.InitForTest();

            {
                Assembler       reserving (cpu.GetInstructionSet(), options);
                Assembler       rockwell  (cmos.GetInstructionSet(), options);
                AssemblyResult  storage     = reserving.Assemble ("        .org $2000\n        rmb 4\n        .byte $FF\n");
                AssemblyResult  instruction = rockwell.Assemble  ("        .org $2000\n        rmb 3,$20\n");

                Assert::IsTrue (storage.errors.empty(), L"one operand reserves storage");
                Assert::AreEqual ((size_t) 5, storage.bytes.size(), L"four reserved bytes and the one after them");

                Assert::IsTrue (instruction.errors.empty(), L"two operands name the Rockwell instruction");
                Assert::AreEqual ((size_t) 2, instruction.bytes.size(), L"which is an opcode and a zero-page address");
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UnrecognizedWordTests
    //
    //  What a dialect does with a word it does not define, which is engine
    //  behavior rather than any one dialect's: whichever field the word arrived
    //  in, it must produce a diagnostic that quotes it back.
    //
    //  Swept over the registry so a dialect added later cannot skip the question.
    //  The row supplies the whole expected message rather than a fragment to
    //  search for, because the two shipped dialects answer in different words --
    //  as65 meets the word in its directive field and Merlin in its opcode field
    //  -- and a shared substring check would be satisfied by either answer given
    //  for the other's source.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UnrecognizedWordTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NoDialectSilentlyAcceptsAWordItDoesNotDefine
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NoDialectSilentlyAcceptsAWordItDoesNotDefine)
        {
            size_t  swept = 0;

            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                const Specimen *  specimen = FindSpecimen (entry.id);
                AssemblyResult    result;

                Assert::IsNotNull (specimen, L"a dialect with no specimen here has never been asked this question");

                result = AssembleWith (entry.id, specimen->source);

                Assert::AreEqual ((size_t) 1, result.errors.size(),
                                  L"one unrecognized word is one diagnostic, and never none");
                Assert::AreEqual (std::string (specimen->message), result.errors[0].message);
                Assert::IsTrue (result.errors[0].kind == DiagnosticKind::SourceError,
                                L"a word never recognized is not a construct understood and declined");
                Assert::IsFalse (result.success, L"and the assembly does not go on to report success");

                swept++;
            }

            //  Against the ENUM, not against the table's own size. A sweep of an
            //  empty registry agrees with itself and reads exactly like a full
            //  one; the enumerator count is the independent number.
            Assert::AreEqual ((size_t) DialectId::Count, swept,
                              L"every dialect that exists must have been asked");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  TheSameSourcesAssembleOnceTheWordIsSpelledRight
        //
        //  The discriminating half. Without it every assertion above is satisfied
        //  by a dialect that rejects its own vocabulary too.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (TheSameSourcesAssembleOnceTheWordIsSpelledRight)
        {
            size_t  swept = 0;

            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                const Specimen *  specimen = FindSpecimen (entry.id);
                AssemblyResult    result;

                Assert::IsNotNull (specimen, L"a dialect with no specimen here has never been asked this question");

                result = AssembleWith (entry.id, specimen->corrected);

                Assert::IsTrue (result.errors.empty(),
                                L"the corrected source must assemble, or the rejection above says nothing");
                Assert::AreEqual ((size_t) 2, result.bytes.size(),
                                  L"and emit the two bytes the misspelled line was silently costing");

                swept++;
            }

            Assert::AreEqual ((size_t) DialectId::Count, swept,
                              L"every dialect that exists must have been asked");
        }

    private:

        //  One dialect's version of the same question: a nonsense word where that
        //  dialect looks for an operation, the word spelled as something it does
        //  define, and what it says about the first.
        struct Specimen
        {
            DialectId     dialect;
            const char *  source;
            const char *  corrected;
            const char *  message;
        };



        static const Specimen * FindSpecimen (DialectId dialect)
        {
            //  ZORKMID rather than a near-miss of a real spelling, so a message
            //  compared by substring cannot match some ordinary word instead.
            static constexpr Specimen  kSpecimens[] =
            {
                { DialectId::As65,
                  "        .org $0300\n        .zorkmid $11, $22\n",
                  "        .org $0300\n        .byte $11, $22\n",
                  "Unknown directive: .ZORKMID" },

                { DialectId::Merlin,
                  "         ORG $0300\n         ZORKMID $11,$22\n",
                  "         ORG $0300\n         DFB $11,$22\n",
                  "Invalid mnemonic: ZORKMID" },
            };

            const Specimen *  found = nullptr;

            for (const Specimen & specimen : kSpecimens)
            {
                if (specimen.dialect == dialect)
                {
                    found = &specimen;
                    break;
                }
            }

            return found;
        }



        static AssemblyResult AssembleWith (DialectId dialect, const std::string & source)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = dialect;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }
    };
}