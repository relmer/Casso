#include "Pch.h"

#include "TestHelpers.h"
#include "Parser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace ParserTests
{
    TEST_CLASS (SplitLinesTests)
    {
    public:

        TEST_METHOD (SplitLines_SplitsOnNewline)
        {
            auto lines = Parser::SplitLines ("LDA #$42\nSTA $10\nNOP");

            Assert::AreEqual ((size_t) 3, lines.size ());
            Assert::AreEqual (std::string ("LDA #$42"), lines[0]);
            Assert::AreEqual (std::string ("STA $10"),  lines[1]);
            Assert::AreEqual (std::string ("NOP"),      lines[2]);
        }

        TEST_METHOD (SplitLines_HandlesEmptyString)
        {
            auto lines = Parser::SplitLines ("");

            Assert::AreEqual ((size_t) 1, lines.size ());
        }

        TEST_METHOD (SplitLines_HandlesCRLF)
        {
            auto lines = Parser::SplitLines ("LDA #$42\r\nSTA $10");

            Assert::AreEqual ((size_t) 2, lines.size ());
            Assert::AreEqual (std::string ("LDA #$42"), lines[0]);
            Assert::AreEqual (std::string ("STA $10"),  lines[1]);
        }
    };



    TEST_CLASS (ParseLineTests)
    {
    public:

        TEST_METHOD (ParseLine_ExtractsLabel)
        {
            auto parsed = Parser::ParseLine ("loop: LDA #$42", 1);

            Assert::AreEqual (std::string ("loop"), parsed.label);
            Assert::AreEqual (std::string ("LDA"),  parsed.mnemonic);
            Assert::AreEqual (std::string ("#$42"), parsed.operand);
            Assert::IsFalse (parsed.isEmpty);
        }

        TEST_METHOD (ParseLine_MnemonicIsCaseInsensitive)
        {
            auto parsed = Parser::ParseLine ("lda #$42", 1);

            Assert::AreEqual (std::string ("LDA"), parsed.mnemonic);
        }

        TEST_METHOD (ParseLine_ExtractsOperand)
        {
            auto parsed = Parser::ParseLine ("STA $10", 1);

            Assert::AreEqual (std::string ("STA"), parsed.mnemonic);
            Assert::AreEqual (std::string ("$10"), parsed.operand);
        }

        TEST_METHOD (ParseLine_StripsFullLineComment)
        {
            auto parsed = Parser::ParseLine ("; this is a comment", 1);

            Assert::IsTrue (parsed.isEmpty);
        }

        TEST_METHOD (ParseLine_StripsInlineComment)
        {
            auto parsed = Parser::ParseLine ("LDA #$42 ; load", 1);

            Assert::AreEqual (std::string ("LDA"),  parsed.mnemonic);
            Assert::AreEqual (std::string ("#$42"), parsed.operand);
        }

        TEST_METHOD (ParseLine_HandlesBlankLine)
        {
            auto parsed = Parser::ParseLine ("", 1);

            Assert::IsTrue (parsed.isEmpty);
        }

        TEST_METHOD (ParseLine_HandlesWhitespaceOnly)
        {
            auto parsed = Parser::ParseLine ("   \t  ", 1);

            Assert::IsTrue (parsed.isEmpty);
        }

        TEST_METHOD (ParseLine_LabelOnly)
        {
            auto parsed = Parser::ParseLine ("start:", 1);

            Assert::AreEqual (std::string ("start"), parsed.label);
            Assert::IsTrue (parsed.mnemonic.empty ());
            Assert::IsFalse (parsed.isEmpty);
        }
    };
}
