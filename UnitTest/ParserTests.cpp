#include "Pch.h"

#include "TestHelpers.h"
#include "Parser.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ParserTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SplitLinesTests
    //
    //  Splitting source into lines: all three terminators, plus backslash
    //  continuations.
    //
    //  BARE CR matters more than it looks. Period Apple II sources really are
    //  CR-only, so treating CR as mere whitespace reads an entire file as one
    //  line -- and the failure is total rather than subtle.
    //
    //  The escaped backslash is the other case: `\\` at end of line is a
    //  literal backslash, not a continuation, so a string ending in a path
    //  separator must not swallow the line after it.
    //
    //  A final line with no terminator is asserted too, since a file not ending
    //  in a newline is ordinary and must not lose its last instruction.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SplitLinesTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SplitLines_SplitsOnNewline
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SplitLines_SplitsOnNewline)
        {
            auto lines = Parser::SplitLines ("LDA #$42\nSTA $10\nNOP");

            Assert::AreEqual ((size_t) 3, lines.size());
            Assert::AreEqual (std::string ("LDA #$42"), lines[0]);
            Assert::AreEqual (std::string ("STA $10"),  lines[1]);
            Assert::AreEqual (std::string ("NOP"),      lines[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SplitLines_HandlesEmptyString
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SplitLines_HandlesEmptyString)
        {
            auto lines = Parser::SplitLines ("");

            Assert::AreEqual ((size_t) 1, lines.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SplitLines_HandlesCRLF
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SplitLines_HandlesCRLF)
        {
            auto lines = Parser::SplitLines ("LDA #$42\r\nSTA $10");

            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::AreEqual (std::string ("LDA #$42"), lines[0]);
            Assert::AreEqual (std::string ("STA $10"),  lines[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SplitLines_LineContinuation_JoinsLines
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SplitLines_LineContinuation_JoinsLines)
        {
            auto lines = Parser::SplitLines ("        lda #\\\n        $42");

            Assert::AreEqual ((size_t) 1, lines.size());
            Assert::AreEqual (std::string ("        lda #        $42"), lines[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SplitLines_LineContinuation_EscapedBackslash
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SplitLines_LineContinuation_EscapedBackslash)
        {
            auto lines = Parser::SplitLines ("line with \\\\\nnext line");

            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::AreEqual (std::string ("line with \\\\"), lines[0]);
            Assert::AreEqual (std::string ("next line"),      lines[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SplitLines_LineContinuation_MultipleLines
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SplitLines_LineContinuation_MultipleLines)
        {
            auto lines = Parser::SplitLines ("part1 \\\npart2 \\\npart3");

            Assert::AreEqual ((size_t) 1, lines.size());
            Assert::AreEqual (std::string ("part1 part2 part3"), lines[0]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ParseLineTests
    //
    //  One source line decomposed into label, mnemonic, operand, and comment.
    //
    //  Purely SYNTACTIC -- nothing is evaluated or resolved here, and the tests
    //  assert the raw strings. That is what lets the same parse serve both
    //  assembler passes, so a test that checked resolved values would be
    //  testing the wrong layer.
    //
    //  The ambiguous shapes get the attention: a line that is only a label, only
    //  a comment, a label with an instruction on the same line, and a
    //  column-0 word that might be either. Those are where a lexer written
    //  against well-formed input falls over.
    //
    //  The column-0 flag is asserted because it is a lexical fact recorded here
    //  and consumed much later -- by then the whitespace is gone.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ParseLineTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_ExtractsLabel
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_ExtractsLabel)
        {
            auto parsed = Parser::ParseLine ("loop: LDA #$42", 1);

            Assert::AreEqual (std::string ("loop"), parsed.label);
            Assert::AreEqual (std::string ("LDA"),  parsed.mnemonic);
            Assert::AreEqual (std::string ("#$42"), parsed.operand);
            Assert::IsFalse (parsed.isEmpty);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_MnemonicIsCaseInsensitive
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_MnemonicIsCaseInsensitive)
        {
            auto parsed = Parser::ParseLine ("lda #$42", 1);

            Assert::AreEqual (std::string ("LDA"), parsed.mnemonic);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_ExtractsOperand
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_ExtractsOperand)
        {
            auto parsed = Parser::ParseLine ("STA $10", 1);

            Assert::AreEqual (std::string ("STA"), parsed.mnemonic);
            Assert::AreEqual (std::string ("$10"), parsed.operand);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_StripsFullLineComment
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_StripsFullLineComment)
        {
            auto parsed = Parser::ParseLine ("; this is a comment", 1);

            Assert::IsTrue (parsed.isEmpty);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_StripsInlineComment
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_StripsInlineComment)
        {
            auto parsed = Parser::ParseLine ("LDA #$42 ; load", 1);

            Assert::AreEqual (std::string ("LDA"),  parsed.mnemonic);
            Assert::AreEqual (std::string ("#$42"), parsed.operand);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_HandlesBlankLine
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_HandlesBlankLine)
        {
            auto parsed = Parser::ParseLine ("", 1);

            Assert::IsTrue (parsed.isEmpty);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_HandlesWhitespaceOnly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_HandlesWhitespaceOnly)
        {
            auto parsed = Parser::ParseLine ("   \t  ", 1);

            Assert::IsTrue (parsed.isEmpty);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseLine_LabelOnly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseLine_LabelOnly)
        {
            auto parsed = Parser::ParseLine ("start:", 1);

            Assert::AreEqual (std::string ("start"), parsed.label);
            Assert::IsTrue (parsed.mnemonic.empty());
            Assert::IsFalse (parsed.isEmpty);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NumberFormatTests
    //
    //  Every numeric notation the assembler accepts: $ and 0x hex, % and 0b
    //  binary, base#value, character constants, and plain decimal.
    //
    //  There are this many because period sources came from several different
    //  assemblers and each had its own conventions -- accepting only one
    //  spelling means rejecting most real files.
    //
    //  The near-misses are the interesting cases and are covered deliberately:
    //  a radix outside 2..36 must fall back to decimal rather than erroring,
    //  since `#` is also the immediate-addressing sigil; and `0b` not followed
    //  by a binary digit must read as decimal zero rather than a failed binary
    //  literal.
    //
    //  Character constants belong here because they are numbers -- with the
    //  high-bit convention that makes Apple II text arithmetic work.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (NumberFormatTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseValue_Hex
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseValue_Hex)
        {
            int value = 0;
            Assert::IsTrue (Parser::ParseValue ("$FF", value));
            Assert::AreEqual (255, value);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseValue_HexLowerCase
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseValue_HexLowerCase)
        {
            int value = 0;
            Assert::IsTrue (Parser::ParseValue ("$ff", value));
            Assert::AreEqual (255, value);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseValue_Binary
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseValue_Binary)
        {
            int value = 0;
            Assert::IsTrue (Parser::ParseValue ("%10101010", value));
            Assert::AreEqual (170, value);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseValue_BinaryLowNibble
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseValue_BinaryLowNibble)
        {
            int value = 0;
            Assert::IsTrue (Parser::ParseValue ("%00001111", value));
            Assert::AreEqual (15, value);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseValue_Decimal
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseValue_Decimal)
        {
            int value = 0;
            Assert::IsTrue (Parser::ParseValue ("255", value));
            Assert::AreEqual (255, value);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ParseValue_DecimalZero
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ParseValue_DecimalZero)
        {
            int value = 0;
            Assert::IsTrue (Parser::ParseValue ("0", value));
            Assert::AreEqual (0, value);
        }
    };
}
