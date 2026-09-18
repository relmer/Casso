#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "IntegerBasicDetokenizer.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizerTests
//
//  The hand-tokenized fixture is the oracle: its bytes were written from the
//  ROM's stored form, byte by byte, and the listing they must produce is
//  stated here rather than derived from the code under test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IntegerBasicDetokenizerTests)
{
public:

    static constexpr const char *  kProgramFixture = "Cassque/Host/integer.tok";
    static constexpr const char *  kDos33Fixture   = "Cassque/dos33.dsk";

    static constexpr const char *  kExpectedListing =
        "10 PRINT \"HELLO\"\n"
        "15 DSP I\n"
        "20 FOR I=1 TO 3\n"
        "30 PRINT I\n"
        "40 NEXT I\n"
        "50 END\n";



    static std::vector<Byte> LoadProgram()
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;

        AssertSucceeded (fixtures.OpenFixture (kProgramFixture, bytes));
        Assert::IsTrue (!bytes.empty(), L"the fixture must hold a program");

        return bytes;
    }



    TEST_METHOD (FixtureProgram_DetokenizesToTheExpectedListing)
    {
        std::vector<Byte>         bytes = LoadProgram();
        std::string               listing;
        IntegerBasicListingError  error;

        AssertSucceeded (IntegerBasicDetokenizer::Detokenize (bytes, listing, error));

        Assert::AreEqual (std::string (kExpectedListing), listing);
        Assert::IsTrue   (error.reason.empty());
    }



    TEST_METHOD (ProgramOnTheDos33Fixture_MatchesTheHostCopy)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         fileBytes;
        std::vector<Byte>         sectors;
        std::vector<Byte>         hostCopy  = LoadProgram();
        SectorDecodeReport        report;
        FilePayload               payload;
        std::string               listing;
        IntegerBasicListingError  error;

        AssertSucceeded (fixtures.OpenFixture (kDos33Fixture, fileBytes));
        AssertSucceeded (VolumeImage::Load (fileBytes, kDos33Fixture, sectors, report));

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Read (FilePath::Parse ("INTPROG"), payload));
        }

        Assert::IsTrue (payload.type == Dos33Volume::kTypeInteger);
        Assert::IsTrue (payload.bytes == hostCopy, L"the volume must hand back the bytes that were put");

        AssertSucceeded (IntegerBasicDetokenizer::Detokenize (payload.bytes, listing, error));
        Assert::AreEqual (std::string (kExpectedListing), listing);
    }



    TEST_METHOD (RemarkConstantAndNumberedVariable_RenderAsListed)
    {
        //  100 REM HI THERE
        //  110 LET A1=16384
        std::vector<Byte>  bytes =
        {
            0x0D, 0x64, 0x00, 0x5D, 0xC8, 0xC9, 0xA0, 0xD4, 0xC8, 0xC5, 0xD2, 0xC5, 0x01,
            0x0B, 0x6E, 0x00, 0x5E, 0xC1, 0xB1, 0x71, 0xB1, 0x00, 0x40, 0x01,
        };
        std::string               listing;
        IntegerBasicListingError  error;

        AssertSucceeded (IntegerBasicDetokenizer::Detokenize (bytes, listing, error));

        Assert::AreEqual (std::string ("100 REM HI THERE\n110 LET A1=16384\n"), listing);
    }



    TEST_METHOD (TruncatedProgram_ReportsTheOffsetOfTheCutLine)
    {
        std::vector<Byte>         bytes    = LoadProgram();
        size_t                    lastLine = bytes.size() - 5;   // "50 END" is five bytes
        HRESULT                   hr       = S_OK;
        std::string               listing;
        IntegerBasicListingError  error;

        bytes.resize (bytes.size() - 2);

        hr = IntegerBasicDetokenizer::Detokenize (bytes, listing, error);
        Assert::IsTrue (FAILED (hr));

        Assert::AreEqual (lastLine, error.offset);
        Assert::IsFalse  (error.reason.empty());
        Assert::IsTrue   (listing.empty(), L"a refusal must not hand back a partial listing");
    }



    TEST_METHOD (LineNotEndingWhereItsLengthSays_IsRefused)
    {
        std::vector<Byte>         bytes = LoadProgram();
        HRESULT                   hr    = S_OK;
        std::string               listing;
        IntegerBasicListingError  error;

        //  The first line is twelve bytes; its terminator is the twelfth.
        bytes[11] = 0xC1;

        hr = IntegerBasicDetokenizer::Detokenize (bytes, listing, error);
        Assert::IsTrue (FAILED (hr));

        Assert::AreEqual ((size_t) 11, error.offset);
        Assert::IsTrue   (error.hasLineNumber);
        Assert::AreEqual ((uint32_t) 10, error.lineNumber);
    }



    TEST_METHOD (EmptyProgram_YieldsAnEmptyListing)
    {
        std::vector<Byte>         bytes;
        std::string               listing = "stale";
        IntegerBasicListingError  error;

        AssertSucceeded (IntegerBasicDetokenizer::Detokenize (bytes, listing, error));

        Assert::IsTrue (listing.empty());
    }



    TEST_METHOD (GetKeyword_AnswersTokensAndNotCharacters)
    {
        Assert::AreEqual ("PRINT", IntegerBasicDetokenizer::GetKeyword (0x61));
        Assert::AreEqual ("DSP",   IntegerBasicDetokenizer::GetKeyword (0x7B));
        Assert::IsNull   (IntegerBasicDetokenizer::GetKeyword (0x80));
    }
};
