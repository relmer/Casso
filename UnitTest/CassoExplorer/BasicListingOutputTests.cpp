#include "Pch.h"
#include "../EhmTestHelper.h"
#include "ApplesoftTokenizer.h"
#include "CassoExplorer/Model/BasicListingOutput.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutputTests
//
//  Applesoft programs tokenized in each test and listed for the host with
//  each copy option.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BasicListingOutputTests)
{
public:

    TEST_METHOD (RawControls_AreTheBytesInTheListing)
    {
        std::vector<Byte>      program;
        std::vector<Byte>      out;
        std::wstring           error;
        ApplesoftListingError  listingError;
        DecodeOptions          options;



        AssertSucceeded (ApplesoftTokenizer::Tokenize ("10 PRINT \"^DCATALOG\"\n", program, listingError));

        options.basicRawControls = true;

        AssertSucceeded (BasicListingOutput::Write (program, false, options, out, error));

        Assert::IsTrue (std::string (out.begin(), out.end()).find ("\"\x04" "CATALOG\"") != std::string::npos, L"^D is the byte $04");
        Assert::IsTrue (std::string (out.begin(), out.end()).ends_with ("\r\n"),                           L"lines end in CR LF");
    }


    TEST_METHOD (RestoreControls_KeepsALiteralCaret)
    {
        std::string  listing = "A^^B^GC^1";



        BasicListingOutput::RestoreControls (listing);

        Assert::AreEqual (std::string ("A^B\x07" "C^1"), listing);
    }


    TEST_METHOD (ColorLine_ColorsEachPart)
    {
        std::vector<Run>  runs;



        BasicListingOutput::ColorLine ("10  PRINT \"HI\": REM DONE", false, runs);

        Assert::AreEqual ((size_t) 7, runs.size());
        Assert::IsTrue   (runs[0].color == RunColor::Muted,   L"the line number");
        Assert::IsTrue   (runs[1].color == RunColor::Keyword && runs[1].text == L"PRINT");
        Assert::IsTrue   (runs[3].color == RunColor::String  && runs[3].text == L"\"HI\"");
        Assert::IsTrue   (runs[5].color == RunColor::Keyword && runs[5].text == L"REM");
        Assert::IsTrue   (runs[6].color == RunColor::Comment && runs[6].text == L" DONE");
    }


    TEST_METHOD (ColorLine_LeavesDataAsText)
    {
        std::vector<Run>  runs;



        BasicListingOutput::ColorLine ("20  DATA TO,AT: END", false, runs);

        Assert::IsTrue (runs[1].color == RunColor::Keyword && runs[1].text == L"DATA");
        Assert::IsTrue (runs[2].color == RunColor::Text    && runs[2].text == L" TO,AT: ", L"a DATA item that is a keyword's letters is data");
        Assert::IsTrue (runs[3].color == RunColor::Keyword && runs[3].text == L"END");
    }


    TEST_METHOD (Color_IsWrittenAsRtf)
    {
        std::vector<Byte>      program;
        std::vector<Byte>      out;
        std::wstring           error;
        ApplesoftListingError  listingError;
        DecodeOptions          options;
        std::wstring           name    = L"HELLO.Applesoft BASIC.txt";



        AssertSucceeded (ApplesoftTokenizer::Tokenize ("10 PRINT \"HI\"\n", program, listingError));

        options.basicColor = true;

        AssertSucceeded (BasicListingOutput::Write (program, false, options, out, error));
        BasicListingOutput::RenameForOptions (name, options);

        Assert::IsTrue   (std::string (out.begin(), out.end()).starts_with ("{\\rtf1"));
        Assert::IsTrue   (std::string (out.begin(), out.end()).find ("PRINT") != std::string::npos);
        Assert::AreEqual (std::wstring (L"HELLO.Applesoft BASIC.rtf"), name);
    }
};
