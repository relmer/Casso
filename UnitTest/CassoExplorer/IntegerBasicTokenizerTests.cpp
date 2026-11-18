#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "IntegerBasicDetokenizer.h"
#include "IntegerBasicOracleRows.h"
#include "IntegerBasicTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizerTests
//
//  The oracle is the ROM: IntegerBasicOracleRows has lines typed on the
//  machine and the bytes it stored for each.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IntegerBasicTokenizerTests)
{
public:
    static constexpr Byte  kRem = 0x5D;

    static std::vector<Byte> FromHex (const char * hex)
    {
        std::vector<Byte>  bytes;
        std::istringstream stream (hex);
        std::string        pair;



        while (stream >> pair)
        {
            bytes.push_back ((Byte) std::stoul (pair, nullptr, 16));
        }

        return bytes;
    }


    static std::string ToHex (const std::vector<Byte> & bytes)
    {
        std::string  hex;



        for (Byte b : bytes)
        {
            hex += std::format ("{:02X} ", b);
        }

        return hex;
    }


    //  The stored line without the $01 that ends it.
    static std::vector<Byte> GetBody (const IntegerBasicOracleRows::Row & row)
    {
        std::vector<Byte>  body = FromHex (row.stored);



        body.pop_back();

        return body;
    }


    static bool HasRemark (const std::vector<Byte> & body)
    {
        return std::find (body.begin(), body.end(), kRem) != body.end();
    }


    //  A constant typed with leading zeros is stored with a zero as its first
    //  digit, and the listing has the value alone.
    static bool HasLeadingZero (const std::vector<Byte> & body)
    {
        size_t  i = 0;



        for (i = 0; i + 2 < body.size(); i++)
        {
            bool  startsConstant = i == 0 || body[i - 1] < 0x80;

            if (startsConstant && body[i] == 0xB0 && (body[i + 1] != 0 || body[i + 2] != 0))
            {
                return true;
            }
        }

        return false;
    }


    static std::vector<Byte> MakeProgram (uint16_t number, const std::vector<Byte> & body)
    {
        std::vector<Byte>  program;



        program.push_back ((Byte) (body.size() + 4));
        program.push_back ((Byte) (number & 0xFF));
        program.push_back ((Byte) (number >> 8));
        program.insert (program.end(), body.begin(), body.end());
        program.push_back (0x01);

        return program;
    }


    TEST_METHOD (TypedLines_TokenizeToTheBytesTheRomStored)
    {
        std::vector<Byte>  body;
        size_t             i = 0;



        for (i = 0; i < IntegerBasicOracleRows::s_kRowCount; i++)
        {
            const IntegerBasicOracleRows::Row & row     = IntegerBasicOracleRows::s_kRows[i];
            std::wstring                        message = TextEncoding::NarrowToWide (row.typed);
            bool                                parsed  = IntegerBasicTokenizer::TokenizeStatements (row.typed, body);

            if (row.stored == nullptr)
            {
                Assert::IsFalse (parsed, message.c_str());
                continue;
            }

            Assert::IsTrue (parsed, message.c_str());

            //  A remark typed on the machine begins with the space typed
            //  after REM, and the listing form has one more there.
            if (!HasRemark (body))
            {
                Assert::AreEqual (ToHex (GetBody (row)), ToHex (body), message.c_str());
            }
        }
    }


    TEST_METHOD (StoredLines_ListedAndTokenizedAgainAreTheSameBytes)
    {
        size_t  i = 0;



        for (i = 0; i < IntegerBasicOracleRows::s_kRowCount; i++)
        {
            const IntegerBasicOracleRows::Row & row = IntegerBasicOracleRows::s_kRows[i];
            std::vector<Byte>                   program;
            std::vector<Byte>                   again;
            std::string                         listing;
            IntegerBasicListingError            listingError;
            ApplesoftListingError               error;

            if (row.stored == nullptr || HasLeadingZero (GetBody (row)))
            {
                continue;
            }

            //  The listing trims the spaces that end a remark.
            if (GetBody (row).back() == 0xA0)
            {
                continue;
            }

            program = MakeProgram (10, GetBody (row));

            AssertSucceeded (IntegerBasicDetokenizer::Detokenize (program, listing, listingError));

            std::wstring  message = TextEncoding::NarrowToWide (std::string (row.typed) + " -> " + listing + error.reason);

            AssertSucceeded (IntegerBasicTokenizer::Tokenize (listing, again, error), message.c_str());
            Assert::AreEqual (ToHex (program), ToHex (again), message.c_str());
        }
    }


    TEST_METHOD (Tokenize_OrdersLinesByNumberAndAcceptsEachLineEnding)
    {
        std::vector<Byte>      bytes;
        ApplesoftListingError  error;



        AssertSucceeded (IntegerBasicTokenizer::Tokenize ("20 END\r\n\r\n10 print \"hi\"\r5 REM x y\n", bytes, error));

        Assert::AreEqual (std::string ("08 05 00 5D F8 A0 F9 01 "
                                       "09 0A 00 61 28 E8 E9 29 01 "
                                       "05 14 00 51 01 "),
                          ToHex (bytes));
    }


    static void AssertRefused (const char * listing, const char * reasonPart, uint32_t lineIndex)
    {
        std::vector<Byte>      bytes;
        ApplesoftListingError  error;
        std::wstring           message = TextEncoding::NarrowToWide (listing);



        AssertFailed (IntegerBasicTokenizer::Tokenize (listing, bytes, error), message.c_str());
        Assert::IsTrue   (bytes.empty(), message.c_str());
        Assert::IsTrue   (error.reason.find (reasonPart) != std::string::npos, TextEncoding::NarrowToWide (error.reason).c_str());
        Assert::AreEqual ((size_t) lineIndex, error.sourceLineIndex, message.c_str());
    }


    TEST_METHOD (Tokenize_FailsWithTheLineAndTheReason)
    {
        AssertRefused ("10 END\nPRINT 1\n",          "has no line number",        2);
        AssertRefused ("32768 END\n",                "32767",                     1);
        AssertRefused ("10 END\n20\n",               "no statement",              2);
        AssertRefused ("10 END\n20 SCORE=1\n",       "column",                    2);
        AssertRefused ("10 END\n20 A=1\n10 GR\n",    "same number",               3);
        AssertRefused ("10 PRINT \"\xC9\"\n",        "no Apple II representation", 1);
        AssertRefused ("",                           "no numbered lines",         0);
        AssertRefused ("10 A=32768\n",               "column",                    1);
    }


    TEST_METHOD (Tokenize_ALineTooLongToStoreFails)
    {
        std::string            listing = "10 REM " + std::string (260, 'A') + "\n";
        std::vector<Byte>      bytes;
        ApplesoftListingError  error;



        AssertFailed (IntegerBasicTokenizer::Tokenize (listing, bytes, error));
        Assert::IsTrue (error.reason.find ("longer") != std::string::npos);
    }


    TEST_METHOD (TokenizeStatements_ADeepExpressionFailsWithoutOverflowingTheStack)
    {
        std::string        text = "A=" + std::string (5000, '(') + "1" + std::string (5000, ')');
        std::vector<Byte>  body;



        Assert::IsFalse (IntegerBasicTokenizer::TokenizeStatements (text, body));
        Assert::IsTrue  (body.empty());
    }
};
