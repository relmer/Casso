#include "Pch.h"

#include "Dialect.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"
#include "Parser.h"





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

        //  Sweeps the ENUM, not the table, which is what catches the failure the
        //  table cannot report: an enumerator added without a profile behind it.
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
}
