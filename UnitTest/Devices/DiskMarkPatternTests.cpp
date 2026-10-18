#include "Pch.h"

#include "DiskMarkPattern.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPatternTests
//
//  Field marks with ?? wildcards: parsing, matching and text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskMarkPatternTests)
{
public:

    TEST_METHOD (ThreeExactNibblesParse)
    {
        DiskMarkPattern  pattern;
        std::string      error;
        bool             isParsed = false;



        isParsed = DiskMarkPattern::TryParse ("D5 AA 96", pattern, error);

        Assert::IsTrue (isParsed);
        Assert::AreEqual (3, pattern.GetLength());
        Assert::AreEqual (static_cast<int> (0x96), static_cast<int> (pattern.GetValue (2)));
        Assert::IsFalse (pattern.IsAny (2));
    }



    TEST_METHOD (WildcardAndLowerCaseParse)
    {
        DiskMarkPattern  pattern;
        std::string      error;
        bool             isParsed = false;



        isParsed = DiskMarkPattern::TryParse ("d5 aa ??", pattern, error);

        Assert::IsTrue (isParsed);
        Assert::IsTrue (pattern.IsAny (2));
        Assert::AreEqual (std::string ("D5 AA ??"), pattern.ToText());
    }



    TEST_METHOD (TwoNibblesParse)
    {
        DiskMarkPattern  pattern;
        std::string      error;
        bool             isParsed = false;



        isParsed = DiskMarkPattern::TryParse ("  DE   AA ", pattern, error);

        Assert::IsTrue (isParsed);
        Assert::AreEqual (2, pattern.GetLength());
    }



    TEST_METHOD (BadTextIsNotAccepted)
    {
        static constexpr const char *  kpszBad[] = { "", "   ", "D5 AA 96 EB", "D5A", "GG", "D5 ? AA", "D5,AA" };

        DiskMarkPattern  pattern  = DiskMarkPattern::MakeExact (0xD5, 0xAA, 0x96);
        std::string      error;
        bool             isParsed = false;



        for (const char * pszText : kpszBad)
        {
            error.clear();
            isParsed = DiskMarkPattern::TryParse (pszText, pattern, error);

            Assert::IsFalse (isParsed, std::wstring (pszText, pszText + strlen (pszText)).c_str());
            Assert::IsFalse (error.empty());
        }

        Assert::IsTrue (pattern == DiskMarkPattern::MakeExact (0xD5, 0xAA, 0x96), L"a failed parse leaves the pattern alone");
    }



    TEST_METHOD (MatchesHonorsWildcards)
    {
        static constexpr Byte  kD5AA96[] = { 0xD5, 0xAA, 0x96, 0xFF };
        static constexpr Byte  kD5AAB5[] = { 0xD5, 0xAA, 0xB5 };
        static constexpr Byte  kD4AA96[] = { 0xD4, 0xAA, 0x96 };
        static constexpr Byte  kShort[]  = { 0xD5, 0xAA };

        DiskMarkPattern  pattern;
        std::string      error;
        bool             isParsed = false;



        isParsed = DiskMarkPattern::TryParse ("D5 AA ??", pattern, error);

        Assert::IsTrue (isParsed);
        Assert::IsTrue  (pattern.Matches (kD5AA96));
        Assert::IsTrue  (pattern.Matches (kD5AAB5));
        Assert::IsFalse (pattern.Matches (kD4AA96));
        Assert::IsFalse (pattern.Matches (kShort), L"a span shorter than the pattern never matches");
    }



    TEST_METHOD (AnEmptyPatternMatchesNothing)
    {
        static constexpr Byte  kNibbles[] = { 0xD5, 0xAA, 0x96 };

        DiskMarkPattern  pattern;



        Assert::IsFalse (pattern.Matches (kNibbles));
    }
};
