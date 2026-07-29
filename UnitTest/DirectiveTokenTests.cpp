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
            std::string  line   = std::string (entry.name) + " " + SampleArgFor (entry.token);
            ParsedLine   parsed = Parser::ParseLine (line, 1);
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


    TEST_METHOD (EveryToken_HasACanonicalSpelling)
    {
        int  token = 0;

        // Skips None; every real token must round-trip name -> token -> name.
        for (token = 1; token < (int) Directive::Count; token++)
        {
            const char *  name = DirectiveTable::GetCanonicalName ((Directive) token);

            Assert::IsTrue (name[0] == '.',
                L"every Directive needs a canonical dotted spelling");

            Assert::IsTrue (DirectiveTable::FromSpelling (name) == (Directive) token,
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
