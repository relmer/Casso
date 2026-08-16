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
}