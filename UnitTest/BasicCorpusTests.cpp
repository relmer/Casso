#include "Pch.h"
#include "EhmTestHelper.h"
#include "ApplesoftTokenizer.h"
#include "EmuTests/DemoAssets.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCorpusTests
//
//  The construct corpus: our own Applesoft program, written to exercise every
//  token the language has rather than the tokens any vendor program happens
//  to use, against the bytes Applesoft ITSELF stored when the same listing
//  was typed into a booted machine.
//
//  THE FIXTURE IS THE ORACLE'S ANSWER, NOT OURS. construct-corpus.tok is a
//  dump of guest memory between TXTTAB and VARTAB, produced by the scenario
//  suite typing construct-corpus.bas into a booted DOS 3.3 master. A
//  tokenizer checked against its own detokenizer agrees with itself perfectly
//  while storing something no guest would recognize; checking against what
//  the ROM stored is what rules that out.
//
//  THE CIRCULARITY GUARD: the fixture may only be regenerated from the guest,
//  never from this tokenizer's own output -- see
//  UnitTest/Fixtures/Basic/construct-inventory.md. Regeneration needs the
//  DOS 3.3 System Master and a booted guest, so it cannot happen by accident,
//  and a fixture diff is visible in review.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (BasicCorpusTests)
{
public:

    //  What a tokenized line costs beyond its body, and what ends it: the
    //  link to the next line, the line number, and the terminator.
    static constexpr size_t  kLineHeaderBytes = 4;

    //  Below this many bytes the corpus is not a corpus. The committed
    //  fixture is several times larger; the floor only has to catch an empty
    //  or truncated embed, which would otherwise pass every loop vacuously.
    static constexpr size_t  kLeastCorpusBytes = 500;


    //
    //  ------------------------------------------------------------------
    //  Material.
    //  ------------------------------------------------------------------
    //

    static std::string CorpusListing()
    {
        std::string  listing = DemoAssets::Text (IDR_BASIC_CORPUS_SRC);

        Assert::IsTrue (listing.size() > 0,
            L"the corpus listing must be embedded, or everything below compares nothing");

        return listing;
    }


    static std::vector<Byte> CorpusFixture()
    {
        std::vector<Byte>  bytes = DemoAssets::Copy (IDR_BASIC_CORPUS_TOK);

        Assert::IsTrue (bytes.size() >= kLeastCorpusBytes,
            L"the tokenized fixture must be a real program: an empty or truncated embed "
            L"would satisfy every structural loop below by comparing nothing. Regenerate "
            L"it from the guest -- scripts/RunTests.ps1 -Scenario -- never from this "
            L"tokenizer's own output");

        return bytes;
    }


    static std::wstring Hex (const std::vector<Byte> & bytes)
    {
        std::wstring  out;
        wchar_t       buf[8] = {};

        for (size_t i = 0; i < bytes.size(); i++)
        {
            swprintf_s (buf, L"%02X ", bytes[i]);
            out += buf;
        }

        return out;
    }


    //  Walks the stored program line by line and reports which body byte
    //  values occur. Line headers -- the link and the line number -- are
    //  stepped over rather than counted, because their bytes can land in the
    //  token range and would fake coverage. The walk asserts the shape as it
    //  goes, so a walk off the rails fails here rather than in what a caller
    //  counts.
    static std::vector<bool> BodyByteValuesIn (const std::vector<Byte> & program)
    {
        std::vector<bool>  seen (256, false);
        size_t             at    = 0;
        size_t             lines = 0;

        while (true)
        {
            bool  hasLink = at + 1 < program.size();

            Assert::IsTrue (hasLink, L"a stored program ends on a null link, not by "
                                     L"running out of bytes");

            if (program[at] == 0 && program[at + 1] == 0)
            {
                break;
            }

            at += kLineHeaderBytes;
            lines++;

            while (at < program.size() && program[at] != 0)
            {
                seen[program[at]] = true;
                at++;
            }

            Assert::IsTrue (at < program.size(),
                L"every line ends on its terminator");

            at++;
        }

        Assert::IsTrue (lines > 0, L"and the program must carry lines at all");

        return seen;
    }


    //  The line numbers, in stored order.
    static std::vector<uint32_t> LineNumbersOf (const std::vector<Byte> & program)
    {
        std::vector<uint32_t>  numbers;
        size_t                 at = 0;

        while (at + 1 < program.size() && !(program[at] == 0 && program[at + 1] == 0))
        {
            numbers.push_back ((uint32_t) (program[at + 2] | (program[at + 3] << 8)));

            at += kLineHeaderBytes;

            while (at < program.size() && program[at] != 0)
            {
                at++;
            }

            at++;
        }

        return numbers;
    }


    //
    //  ------------------------------------------------------------------
    //  The oracle's answer, byte for byte.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (TheCommittedListing_TokenizesToExactlyWhatApplesoftStored)
    {
        std::vector<Byte>      expected = CorpusFixture();
        std::vector<Byte>      actual;
        ApplesoftListingError  error;
        std::wstring           message;



        AssertSucceeded (ApplesoftTokenizer::Tokenize (CorpusListing(), actual, error),
            L"the corpus must tokenize");

        message  = L"the bytes this tokenizer produces for the corpus must be the bytes "
                   L"Applesoft stored for the same listing, links and all\n  Applesoft: ";
        message += Hex (expected);
        message += L"\n  Casso:     ";
        message += Hex (actual);

        Assert::IsTrue (actual == expected, message.c_str());
    }

    TEST_METHOD (RoundTrip_TheFixture_DetokenizesAndRetokenizesByteForByte)
    {
        //  The direction a user did not ask to have changed: extracting a
        //  program and placing it back must be the identity on the bytes.
        std::vector<Byte>      fixture = CorpusFixture();
        std::vector<Byte>      again;
        std::string            listing;
        ApplesoftListingError  error;



        AssertSucceeded (ApplesoftTokenizer::Detokenize (fixture, listing, error),
            L"what Applesoft stored must be readable as a listing");

        AssertSucceeded (ApplesoftTokenizer::Tokenize (listing, again, error),
            L"and the listing must tokenize back");

        Assert::IsTrue (again == fixture,
            L"byte for byte");
    }


    //
    //  ------------------------------------------------------------------
    //  Totality: every token, mechanically, not by inventory prose.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (TheListing_IsPrintableAscii_WhichIsWhatMakesTheTokenSweepSound)
    {
        //  The sweep below reads every body byte at or above $80 as a token.
        //  That is only sound because nothing in the listing -- no string, no
        //  REM, no DATA payload -- carries a high-bit or control byte that
        //  could impersonate one. The inventory beside the fixture states the
        //  same constraint for authors; this is the gate that holds it.
        std::string  listing = CorpusListing();
        size_t       i       = 0;



        for (i = 0; i < listing.size(); i++)
        {
            char  c       = listing[i];
            bool  allowed = (c >= 0x20 && c < 0x7F) || c == '\n' || c == '\r';

            Assert::IsTrue (allowed,
                L"the corpus listing must be printable ASCII plus line endings");
        }
    }

    TEST_METHOD (TheCorpus_ExercisesEveryTokenApplesoftHas)
    {
        //  Swept over the token RANGE, not over what the fixture happens to
        //  hold: a corpus that quietly lost a statement would still satisfy
        //  any loop over its own contents. Every keyword the tokenizer's
        //  table can store must occur in what Applesoft stored.
        std::vector<bool>  seen  = BodyByteValuesIn (CorpusFixture());
        int                token = 0;



        for (token = ApplesoftTokenizer::kFirstToken;
             token <= ApplesoftTokenizer::kLastToken;
             token++)
        {
            std::wstring  message = L"the corpus must exercise every token, and this "
                                    L"one is missing: ";

            for (const char * spelling = ApplesoftTokenizer::GetKeyword ((Byte) token);
                 spelling != nullptr && *spelling != 0;
                 spelling++)
            {
                message += (wchar_t) *spelling;
            }

            Assert::IsTrue (seen[(size_t) token], message.c_str());
        }
    }

    TEST_METHOD (TheCorpus_CarriesItsOperandEdges)
    {
        //  The edges the inventory promises, held mechanically where a
        //  mechanical hold is possible. The listing carries each as text;
        //  the line-number edges are read off the stored program, where a
        //  wrong number is bytes rather than prose.
        std::string            listing = CorpusListing();
        std::vector<uint32_t>  numbers = LineNumbersOf (CorpusFixture());



        Assert::IsTrue (numbers.size() > 0, L"the fixture must carry lines");

        Assert::AreEqual ((uint32_t) 0, numbers.front(),
            L"line zero is a legal line number and the corpus starts on it");

        Assert::AreEqual (ApplesoftTokenizer::kMaxLineNumber, numbers.back(),
            L"and it ends on the highest line number Applesoft accepts");

        Assert::IsTrue (listing.find ("32767") != std::string::npos &&
                        listing.find ("-32768") != std::string::npos,
            L"the integer rims are in the text");

        Assert::IsTrue (listing.find ("1E10") != std::string::npos &&
                        listing.find ("2.5E-7") != std::string::npos,
            L"so is scientific notation, with and without a fraction and sign");

        Assert::IsTrue (listing.find ("\"\"") != std::string::npos,
            L"and the empty string");

        Assert::IsTrue (listing.find ("? \"") != std::string::npos,
            L"and the ? shorthand for PRINT");

        Assert::IsTrue (listing.find ("CHR$ (4)") != std::string::npos,
            L"and a CTRL-D command string built the typeable way");
    }
};
