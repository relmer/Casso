#include "Pch.h"

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
    //  DialectRegistryTests
    //
    //  The mechanism itself, tested apart from any one dialect.
    //
    //  These sweep the registry rather than naming dialects, so a dialect added
    //  to the table is covered without anyone editing this file -- the same
    //  reason DirectiveTokenTests sweeps DirectiveTable::GetAllSpellings and
    //  CommandLineTests sweeps CommandLineParser::GetAllSubcommands.
    //
    //  The synthetic third profile that proves SC-009 belongs here too, but
    //  deliberately does NOT land yet. Against a seam shaped by one dialect it
    //  would pass trivially, because it would be written to fit the seam that
    //  exists; it only carries weight once Merlin has pressed on the seam.
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
}