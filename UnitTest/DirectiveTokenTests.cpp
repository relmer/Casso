#include "Pch.h"

#include "Assembler.h"
#include "Directive.h"
#include "Parser.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTokenTests
//
//  Guards the invariant that pass-1 dispatch depends on: whenever the parser
//  decides a line is a directive, ParsedLine must carry BOTH the canonical
//  string and the matching token. Anything that sets one without the other
//  reaches a token-indexed table as Directive::None and is silently skipped.
//
//  That is not hypothetical. The parser's dual-purpose RMB branch -- `rmb
//  <count>` is a .DS synonym, `rmb <bit>,<zp>` is the Rockwell instruction --
//  set the string and not the token, so `rmb 5` reserved nothing. It survived
//  a byte-for-byte differential over every .a65 source in the repo, because
//  none of them spell .DS that way.
//
//  The sweep is driven off DirectiveTable itself rather than a hand-picked
//  list, so a spelling added to the table is covered without editing a test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DirectiveTokenTests)
{
public:

    // A directive needs an argument for some spellings to parse as intended,
    // and a harmless one for the rest.
    static std::string SampleArgFor (Directive token)
    {
        std::string  arg = "1";

        switch (token)
        {
        case Directive::Text:
        case Directive::Error:
        case Directive::Title:
            arg = "\"x\"";
            break;

        case Directive::Include:
            arg = "\"nonexistent.a65\"";
            break;

        case Directive::Cmap:
            arg = "'a','a'";
            break;

        default:
            break;
        }

        return arg;
    }


    TEST_METHOD (EverySpelling_ParsesToItsOwnToken)
    {
        for (const DirectiveTable::Spelling & entry : DirectiveTable::GetAllSpellings())
        {
            ParsedLine  parsed = {};

            std::string  line   = std::string (entry.name) + " " + SampleArgFor (entry.token);
            parsed = Parser::ParseLine (line, 1);
            std::wstring what (line.begin(), line.end());

            Assert::IsTrue (parsed.isDirective,
                (L"'" + what + L"' must parse as a directive").c_str());

            Assert::IsTrue (parsed.directiveToken == entry.token,
                (L"'" + what + L"' must carry its own token").c_str());

            // The string and the token must denote the same directive, or a
            // consumer that switched to the token silently diverges from one
            // that has not.
            //
            // Deliberately NOT "the string equals the canonical name": some
            // directives have several accepted dotted spellings that share a
            // token (.BSS and .SEGMENT_BSS), and the parser keeps whichever
            // the source used. Round-tripping the string is the real
            // invariant; requiring canonical form would demand a
            // normalization the assembler does not currently do.
            Assert::IsTrue (
                DirectiveTable::FromSpelling (parsed.directive) == parsed.directiveToken,
                (L"'" + what + L"' string and token must denote the same directive").c_str());
        }
    }


    //  Every token AS65 CLAIMS must round-trip name -> token -> name.
    //
    //  This swept all of Directive when as65 was the only dialect, because the
    //  enum was total over as65's table. It no longer is: Merlin introduced
    //  tokens with no as65 spelling, and FR-005 requires them to stay that way --
    //  as65 must not accept another dialect's constructs, so `.HEX` going
    //  unrecognized here is the requirement rather than a gap.
    //
    //  Narrowing the sweep to as65's own tokens keeps the regression it was
    //  written for (RMB) while dropping a claim that is now false. The totality
    //  that remains -- every token is claimed by at least one dialect -- is
    //  checked in DialectMechanismTests, where both tables are in scope.
    TEST_METHOD (EveryAs65Token_HasACanonicalSpelling)
    {
        for (const DirectiveTable::Spelling & entry : DirectiveTable::GetAllSpellings())
        {
            const char *  name = DirectiveTable::GetCanonicalName (entry.token);

            Assert::IsTrue (name[0] == '.',
                L"every as65 Directive needs a canonical dotted spelling");

            Assert::IsTrue (DirectiveTable::FromSpelling (name) == entry.token,
                L"canonical spelling must map back to its own token");
        }
    }


    // The regression that motivated the sweep above, pinned directly: RMB
    // reserves storage in its bare-count form and stays an instruction in its
    // two-operand form, and the token has to follow the string either way.
    TEST_METHOD (Rmb_CountForm_CarriesDsToken)
    {
        ParsedLine  parsed = Parser::ParseLine ("        rmb 5", 1);

        Assert::IsTrue (parsed.isDirective,       L"bare `rmb <count>` is the .DS synonym");
        Assert::IsTrue (parsed.directiveToken == Directive::Ds,
            L"`rmb <count>` must carry Directive::Ds, not None -- a null token "
            L"reaches the pass-1 table as 'not a directive' and reserves nothing");
    }


    TEST_METHOD (Rmb_BitForm_IsNotADirective)
    {
        ParsedLine  parsed = Parser::ParseLine ("        rmb 3,$20", 1);

        Assert::IsFalse (parsed.isDirective,
            L"`rmb <bit>,<zp>` is the Rockwell instruction, not storage");
        Assert::IsTrue (parsed.directiveToken == Directive::None,
            L"a non-directive line must leave the token clear");
    }


    // .CODE / .DATA / .BSS share a token with their .SEGMENT_* long forms, so
    // the two spellings must assemble identically. Before segment dispatch
    // read the token, each pair was two string comparisons in two separate
    // lists, and one list could gain a spelling the other lacked.
    TEST_METHOD (SegmentSpellings_LongAndShort_AssembleIdentically)
    {
        static const char *  s_kPairs[][2] =
        {
            { ".segment_code", ".code" },
            { ".segment_data", ".data" },
            { ".segment_bss",  ".bss"  },
        };

        for (const auto & pair : s_kPairs)
        {
            TestCpu         cpu;
            AssemblyResult  resLong;
            AssemblyResult  resShort;
            cpu.InitForTest();

            Assembler  asm6502 (cpu.GetInstructionSet());
            Assembler  asmShort (cpu.GetInstructionSet());

            std::string  srcLong  = std::string ("        org $800\n        ") + pair[0] + "\n        .byte $ff\n";
            std::string  srcShort = std::string ("        org $800\n        ") + pair[1] + "\n        .byte $ff\n";

            resLong = asm6502.Assemble (srcLong);
            resShort = asmShort.Assemble (srcShort);
            std::string     what     = std::string (pair[0]) + " vs " + pair[1];
            std::wstring    msg (what.begin(), what.end());

            Assert::IsTrue (resLong.success,  (L"long form must assemble: "  + msg).c_str());
            Assert::IsTrue (resShort.success, (L"short form must assemble: " + msg).c_str());
            Assert::IsTrue (resLong.bytes == resShort.bytes,
                (L"both spellings are one token, so both must emit the same bytes: " + msg).c_str());
        }
    }


    // The end-to-end shape of the same bug: if the token is missing, the
    // reservation silently does not happen.
    TEST_METHOD (Rmb_CountForm_ActuallyReservesStorage)
    {
        TestCpu  cpu;
        cpu.InitForTest();

        Assembler       asm6502 (cpu.GetInstructionSet());
        AssemblyResult  result = asm6502.Assemble ("        org $800\n        rmb 5\n        .byte $ff\n");

        Assert::IsTrue (result.success, L"source must assemble");
        Assert::AreEqual ((size_t) 6, result.bytes.size(),
            L"5 reserved bytes plus the trailing .byte");
    }
};
