#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "Cassque/Model/ContentSniffer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSnifferTests
//
//  The content rule over the host-side fixtures: what a file with no usable
//  suffix becomes when dropped on a disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ContentSnifferTests)
{
public:

    using Verdict = ContentSniffer::Verdict;



    static std::vector<Byte> LoadHostFixture (const char * name)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;

        AssertSucceeded (fixtures.OpenFixture (std::string ("Cassque/Host/") + name, bytes));
        Assert::IsTrue (!bytes.empty(), L"the fixture must hold something");

        return bytes;
    }



    static Verdict Classify (const char * name, Word & outAddress)
    {
        std::vector<Byte>  bytes = LoadHostFixture (name);

        return ContentSniffer::Classify (bytes, outAddress);
    }



    TEST_METHOD (AnApplesoftListing_IsApplesoft)
    {
        Word  address = 0;

        Assert::IsTrue (Classify ("applesoft.txt", address) == Verdict::Applesoft);
    }



    TEST_METHOD (AnImplicitLetListing_IsApplesoft)
    {
        Word  address = 0;

        Assert::IsTrue (Classify ("implicit-let.txt", address) == Verdict::Applesoft);
    }



    TEST_METHOD (ANumberedReadme_IsRejectedOnItsFirstNonKeywordLine)
    {
        Word  address = 0;

        //  "2 Installing the program" opens with an identifier that is not
        //  followed by an equals sign, so the file is text.
        Assert::IsFalse (ContentSniffer::IsApplesoftStatementStart ("Installing the program"));
        Assert::IsTrue  (Classify ("numbered-readme.txt", address) == Verdict::Text);
    }



    TEST_METHOD (AnIntegerListing_FallsToText)
    {
        Word  address = 0;

        //  DSP is Integer BASIC's; Applesoft has no such keyword.
        Assert::IsFalse (ContentSniffer::IsApplesoftStatementStart ("DSP I"));
        Assert::IsTrue  (Classify ("integer-listing.txt", address) == Verdict::Text);
    }



    TEST_METHOD (PrintableText_IsText)
    {
        Word  address = 0;

        Assert::IsTrue (Classify ("notes.txt", address) == Verdict::Text);
    }



    TEST_METHOD (Binaries_SuggestTheGraphicsAddressOnlyAtHiResLength)
    {
        Word  address = 0;

        Assert::IsTrue   (Classify ("hires.bin", address) == Verdict::Binary);
        Assert::AreEqual ((int) 0x2000, (int) address);

        Assert::IsTrue   (Classify ("odd.bin", address) == Verdict::Binary);
        Assert::AreEqual ((int) 0x0803, (int) address);

        Assert::IsTrue   (Classify ("dhires.bin", address) == Verdict::Binary);
        Assert::AreEqual ((int) 0x0803, (int) address);
    }



    TEST_METHOD (StatementStart_KeywordsIdentifiersAndTheQuestionMark)
    {
        Assert::IsTrue  (ContentSniffer::IsApplesoftStatementStart ("PRINT \"HI\""));
        Assert::IsTrue  (ContentSniffer::IsApplesoftStatementStart ("  print x"));
        Assert::IsTrue  (ContentSniffer::IsApplesoftStatementStart ("? 1"));
        Assert::IsTrue  (ContentSniffer::IsApplesoftStatementStart ("A$ = \"X\""));
        Assert::IsTrue  (ContentSniffer::IsApplesoftStatementStart ("X1=2"));
        Assert::IsFalse (ContentSniffer::IsApplesoftStatementStart ("X1 2"));
        Assert::IsFalse (ContentSniffer::IsApplesoftStatementStart (""));
        Assert::IsFalse (ContentSniffer::IsApplesoftStatementStart ("= 5"));
    }



    TEST_METHOD (LineNumbers_MustAscendAndStayInRange)
    {
        std::string  descending = "20 PRINT 1\r\n10 PRINT 2\r\n";
        std::string  tooLarge   = "64000 PRINT 1\r\n";
        std::string  blankLines = "10 PRINT 1\r\n\r\n   \r\n20 END\r\n";
        Word         address    = 0;

        Assert::IsTrue (ContentSniffer::Classify (std::span<const Byte> ((const Byte *) descending.data(), descending.size()), address) == Verdict::Text);
        Assert::IsTrue (ContentSniffer::Classify (std::span<const Byte> ((const Byte *) tooLarge.data(), tooLarge.size()), address) == Verdict::Text);
        Assert::IsTrue (ContentSniffer::Classify (std::span<const Byte> ((const Byte *) blankLines.data(), blankLines.size()), address) == Verdict::Applesoft);
    }



    TEST_METHOD (AnEmptyFile_IsShortText)
    {
        std::vector<Byte>  none;
        Word               address = 0;

        Assert::IsTrue (ContentSniffer::Classify (none, address) == Verdict::Text);
    }
};
