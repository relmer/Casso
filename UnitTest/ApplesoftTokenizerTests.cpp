#include "Pch.h"
#include "EhmTestHelper.h"
#include "ApplesoftTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizerTests
//
//  THE ORACLE FOR MOST OF THIS FILE IS APPLESOFT ITSELF. The three byte vectors
//  below were not derived from a token table or from this code; they were typed
//  into a booted machine, one line at a time, and read back out of the memory
//  between TXTTAB and VARTAB. A tokenizer checked only against its own
//  detokenizer agrees with itself perfectly while storing something no guest
//  would recognize, which is the same shape as the sector-order defect this
//  branch spent a phase learning to distrust.
//
//  What those vectors settled, none of it guessable:
//
//    - Spaces outside a string, REM or DATA are DROPPED, including in the middle
//      of a keyword. `PR INT` is one PRINT token.
//    - REM text and DATA payloads are stored VERBATIM from the character after
//      the keyword, spaces included.
//    - `?` is the PRINT token.
//    - `ATN` is one token and `A TO` is a letter and a token, both by special
//      case; `TOTAL` is TO followed by TAL, which is NOT special-cased.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ApplesoftTokenizerTests)
{
public:

    //
    //  ------------------------------------------------------------------
    //  Helpers.
    //  ------------------------------------------------------------------
    //

    static std::vector<Byte> Tokenized (const std::string & listing)
    {
        std::vector<Byte>      bytes;
        ApplesoftListingError  error;

        AssertSucceeded (ApplesoftTokenizer::Tokenize (listing, bytes, error),
            L"this listing was expected to tokenize");

        return bytes;
    }


    static std::string Detokenized (const std::vector<Byte> & bytes)
    {
        std::string            listing;
        ApplesoftListingError  error;

        AssertSucceeded (ApplesoftTokenizer::Detokenize (bytes, listing, error),
            L"these bytes were expected to detokenize");

        return listing;
    }


    static ApplesoftListingError RefusalFor (const std::string & listing)
    {
        std::vector<Byte>      bytes;
        ApplesoftListingError  error;

        AssertFailed (ApplesoftTokenizer::Tokenize (listing, bytes, error),
            L"this listing was expected to be refused");

        Assert::IsTrue (bytes.empty(),
            L"and a refusal must hand back nothing -- bytes plus an error is a caller "
            L"one missing check away from placing a program that was rejected");

        Assert::IsFalse (error.reason.empty(),
            L"and must say what is wrong with it");

        return error;
    }


    //  Renders bytes as space-separated hex, so a mismatch names the byte rather
    //  than only the length.
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


    static void AssertBytesAre (const std::vector<Byte>  & actual,
                                const std::vector<Byte>  & expected,
                                const wchar_t            * what)
    {
        std::wstring  message = what;

        message += L"\n  expected: ";
        message += Hex (expected);
        message += L"\n  actual:   ";
        message += Hex (actual);

        Assert::IsTrue (actual == expected, message.c_str());
    }


    //  Placing a listing and reading it back must be the identity on the BYTES,
    //  which is the direction a user did not ask to be changed.
    static void AssertBytesRoundTrip (const std::vector<Byte> & program)
    {
        std::string  listing = Detokenized (program);

        AssertBytesAre (Tokenized (listing), program,
            L"tokens -> listing -> tokens must be the identity");
    }


    //
    //  ------------------------------------------------------------------
    //  What a real Applesoft stored, byte for byte.
    //  ------------------------------------------------------------------
    //

    //  Typed into a booted DOS 3.3 master and read back from $0801.
    static std::string SpacingSource()
    {
        return "10 PRINT \"HI\"\n"
               "20 A = 1\n"
               "30 REM  X Y\n"
               "40 DATA 1, 2 ,3\n"
               "50 ?\"Z\"\n"
               "60 FOR I = 1 TO 9: NEXT\n";
    }


    static std::vector<Byte> SpacingBytes()
    {
        return std::vector<Byte>
        {
            0x0B, 0x08, 0x0A, 0x00, 0xBA, 0x22, 0x48, 0x49, 0x22, 0x00,
            0x13, 0x08, 0x14, 0x00, 0x41, 0xD0, 0x31, 0x00,
            0x1E, 0x08, 0x1E, 0x00, 0xB2, 0x20, 0x20, 0x58, 0x20, 0x59, 0x00,
            0x2C, 0x08, 0x28, 0x00, 0x83, 0x20, 0x31, 0x2C, 0x20, 0x32, 0x20, 0x2C, 0x33, 0x00,
            0x35, 0x08, 0x32, 0x00, 0xBA, 0x22, 0x5A, 0x22, 0x00,
            0x42, 0x08, 0x3C, 0x00, 0x81, 0x49, 0xD0, 0x31, 0xC1, 0x39, 0x3A, 0x82, 0x00,
            0x00, 0x00,
        };
    }


    static std::string BoundarySource()
    {
        return "10 X = ATN(1)\n"
               "20 TOTAL = 5\n"
               "30 HLIN 0,39 AT 10\n"
               "40 PRINT \"PRINT AT TO\"\n"
               "50 print 1\n"
               "60 DATA \"A:B\",C: PRINT 1\n"
               "70 REM X: PRINT\n"
               "80 PR INT\n";
    }


    static std::vector<Byte> BoundaryBytes()
    {
        return std::vector<Byte>
        {
            0x0C, 0x08, 0x0A, 0x00, 0x58, 0xD0, 0xE1, 0x28, 0x31, 0x29, 0x00,
            0x17, 0x08, 0x14, 0x00, 0xC1, 0x54, 0x41, 0x4C, 0xD0, 0x35, 0x00,
            0x24, 0x08, 0x1E, 0x00, 0x8E, 0x30, 0x2C, 0x33, 0x39, 0xC5, 0x31, 0x30, 0x00,
            0x37, 0x08, 0x28, 0x00, 0xBA, 0x22, 0x50, 0x52, 0x49, 0x4E, 0x54, 0x20,
                                    0x41, 0x54, 0x20, 0x54, 0x4F, 0x22, 0x00,
            0x3E, 0x08, 0x32, 0x00, 0xBA, 0x31, 0x00,
            0x4F, 0x08, 0x3C, 0x00, 0x83, 0x20, 0x22, 0x41, 0x3A, 0x42, 0x22, 0x2C,
                                    0x43, 0x3A, 0xBA, 0x31, 0x00,
            0x5E, 0x08, 0x46, 0x00, 0xB2, 0x20, 0x58, 0x3A, 0x20, 0x50, 0x52, 0x49,
                                    0x4E, 0x54, 0x00,
            0x64, 0x08, 0x50, 0x00, 0xBA, 0x00,
            0x00, 0x00,
        };
    }


    static std::string CollisionSource()
    {
        return "10 FOR I = A TO B: NEXT\n"
               "20 A$ = \"HI THERE\"\n"
               "30 IF A<=B THEN 10\n"
               "40 DATA A, B\n"
               "50 REM LOWER\n"
               "60 PRINT A$ ; CHR$ (7)\n"
               "70 ATN = 1\n"
               "80 X = A TOTAL\n";
    }


    static std::vector<Byte> CollisionBytes()
    {
        return std::vector<Byte>
        {
            0x0E, 0x08, 0x0A, 0x00, 0x81, 0x49, 0xD0, 0x41, 0xC1, 0x42, 0x3A, 0x82, 0x00,
            0x20, 0x08, 0x14, 0x00, 0x41, 0x24, 0xD0, 0x22, 0x48, 0x49, 0x20, 0x54,
                                    0x48, 0x45, 0x52, 0x45, 0x22, 0x00,
            0x2D, 0x08, 0x1E, 0x00, 0xAD, 0x41, 0xD1, 0xD0, 0x42, 0xC4, 0x31, 0x30, 0x00,
            0x38, 0x08, 0x28, 0x00, 0x83, 0x20, 0x41, 0x2C, 0x20, 0x42, 0x00,
            0x44, 0x08, 0x32, 0x00, 0xB2, 0x20, 0x4C, 0x4F, 0x57, 0x45, 0x52, 0x00,
            0x51, 0x08, 0x3C, 0x00, 0xBA, 0x41, 0x24, 0x3B, 0xE7, 0x28, 0x37, 0x29, 0x00,
            0x59, 0x08, 0x46, 0x00, 0xE1, 0xD0, 0x31, 0x00,
            0x65, 0x08, 0x50, 0x00, 0x58, 0xD0, 0x41, 0xC1, 0x54, 0x41, 0x4C, 0x00,
            0x00, 0x00,
        };
    }


    TEST_METHOD (Tokenize_SpacingAndVerbatimRuns_MatchWhatApplesoftStored)
    {
        // Line 20 is the one that would pass against a tokenizer keeping
        // spaces: `A = 1` is three bytes stored, not five.
        AssertBytesAre (Tokenized (SpacingSource()), SpacingBytes(),
            L"the stored form must be what the guest itself produced for these lines");
    }


    TEST_METHOD (Tokenize_TheCoverageBoundary_MatchesWhatApplesoftStored)
    {
        // Token spellings inside a string, a DATA payload ending at a colon it
        // does not own, a REM swallowing one, a keyword split by a space, and a
        // lowercase keyword.
        AssertBytesAre (Tokenized (BoundarySource()), BoundaryBytes(),
            L"the boundary cases must be what the guest itself produced");
    }


    TEST_METHOD (Tokenize_TheKeywordCollisions_MatchWhatApplesoftStored)
    {
        // AT against ATN against `A TO`, and TOTAL left alone.
        AssertBytesAre (Tokenized (CollisionSource()), CollisionBytes(),
            L"the AT family must resolve the way the guest resolves it");
    }


    TEST_METHOD (Tokenize_ASpaceInsideAKeyword_IsStillOneToken)
    {
        std::vector<Byte>  split = Tokenized ("10 PR INT\n");
        std::vector<Byte>  whole = Tokenized ("10 PRINT\n");

        AssertBytesAre (split, whole,
            L"Applesoft skips spaces while matching a keyword, measured on the machine");
    }


    TEST_METHOD (Tokenize_AListingWithNoTrailingNewline_KeepsItsLastLine)
    {
        // A host file whose last line has no newline behind it is ordinary, and
        // dropping that line would place a program missing its last statement
        // while reporting success.
        AssertBytesAre (Tokenized ("10 END\n20 END"), Tokenized ("10 END\n20 END\n"),
            L"the last line counts whether or not a newline follows it");
    }


    TEST_METHOD (Tokenize_AQuestionMark_IsThePrintToken)
    {
        AssertBytesAre (Tokenized ("10 ?1\n"), Tokenized ("10 PRINT 1\n"),
            L"? is PRINT, and it is the one abbreviation that does not come back");
    }


    //
    //  ------------------------------------------------------------------
    //  The table, swept rather than sampled.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Tokens_EveryByteInTheRange_SpellsSomething)
    {
        int  token = 0;
        int  spelt = 0;

        for (token = ApplesoftTokenizer::kFirstToken;
             token <= ApplesoftTokenizer::kLastToken;
             token++)
        {
            const char *  keyword = ApplesoftTokenizer::GetKeyword ((Byte) token);

            Assert::IsNotNull (keyword,
                L"every byte in the token range must spell a keyword -- a table sweep "
                L"visits only the rows that exist, so this sweeps the RANGE");

            Assert::IsTrue (keyword[0] != '\0', L"and none of them may be empty");

            spelt++;
        }

        Assert::AreEqual (107, spelt,
            L"and there must be exactly as many as Applesoft has");

        Assert::IsNull (ApplesoftTokenizer::GetKeyword (0x7F),
            L"a byte below the range is not a token");

        Assert::IsNull (ApplesoftTokenizer::GetKeyword (0xEB),
            L"and neither is one above it");
    }


    TEST_METHOD (Tokens_EverySpelling_TokenizesBackToItsOwnByte)
    {
        int     token    = 0;
        size_t  bodyAt   = 4;
        int     resolved = 0;

        for (token = ApplesoftTokenizer::kFirstToken;
             token <= ApplesoftTokenizer::kLastToken;
             token++)
        {
            std::string        source = "10 " + std::string (ApplesoftTokenizer::GetKeyword ((Byte) token)) + "\n";
            std::vector<Byte>  bytes  = Tokenized (source);

            Assert::IsTrue (bytes.size() > bodyAt,
                L"a line holding one keyword still has a body");

            Assert::AreEqual ((int) token, (int) bytes[bodyAt],
                L"a keyword typed on its own must resolve to its own token -- this is what "
                L"catches a table whose ORDER lets an earlier spelling swallow a later one");

            resolved++;
        }

        Assert::AreEqual (107, resolved, L"and every one of them must have been checked");
    }


    //
    //  ------------------------------------------------------------------
    //  Detokenizing, and the round trip that is the whole point of it.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Detokenize_TheMeasuredProgram_ReadsBackAsALocatableListing)
    {
        // The spacing is close to but NOT identical to LIST's, and the
        // difference is deliberate: LIST writes a space after REM and DATA too,
        // and that space would be swallowed into the payload on the way back,
        // growing the listing by one space per round trip.
        Assert::AreEqual (std::string ("10  PRINT \"HI\"\n"
                                       "20 A = 1\n"
                                       "30  REM  X Y\n"
                                       "40  DATA 1, 2 ,3\n"
                                       "50  PRINT \"Z\"\n"
                                       "60  FOR I = 1 TO 9: NEXT\n"),
                          Detokenized (SpacingBytes()),
            L"the rendered listing must be exactly this, character for character");
    }


    TEST_METHOD (RoundTrip_TokensToListingAndBack_IsTheIdentity)
    {
        AssertBytesRoundTrip (SpacingBytes());
        AssertBytesRoundTrip (BoundaryBytes());
        AssertBytesRoundTrip (CollisionBytes());
    }


    TEST_METHOD (RoundTrip_ADataPayloadEndingInASpace_KeepsIt)
    {
        // The one place a trailing space is significant, and the one an editor
        // that trims line ends would silently change.
        std::vector<Byte>  program = Tokenized ("10 DATA A \n");

        Assert::AreEqual ((size_t) 11, program.size(),
            L"link, number, DATA, space, A, space, terminator, and the null link");

        AssertBytesRoundTrip (program);
    }


    TEST_METHOD (RoundTrip_ListingToTokensAndBack_IsNOTTheIdentity_AndTheLossIsNamed)
    {
        // Stated as a test rather than as prose, so the day it stops being true
        // somebody finds out here rather than from a user whose program came
        // back reformatted.
        std::string  source = "10 print a$ ; \"Hi There\"\n"
                              "5 ?1\n";

        std::string  back   = Detokenized (Tokenized (source));

        Assert::AreEqual (std::string ("5  PRINT 1\n"
                                       "10  PRINT A$;\"Hi There\"\n"),
                          back,
            L"lines are ordered, ? becomes PRINT, code is upper-cased and spacing is "
            L"normalized -- and the string's own case and spaces are untouched");

        Assert::AreNotEqual (source, back,
            L"which is exactly what makes this direction lossy");
    }


    TEST_METHOD (RoundTrip_TheNormalizedFormIsStable_SoASecondTripChangesNothing)
    {
        // The loss is one trip deep, not cumulative. A conversion that kept
        // adding a space per pass would satisfy the test above and still be
        // wrong.
        std::string  once  = Detokenized (Tokenized ("10 REM  X\n20 DATA  1\n30 PRINT \"A\"\n"));
        std::string  twice = Detokenized (Tokenized (once));

        Assert::AreEqual (once, twice, L"the second pass must change nothing");
    }


    //
    //  ------------------------------------------------------------------
    //  Links, which are the part that fails quietly.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Tokenize_EachLink_IsTheAbsoluteAddressOfTheLineAfterIt)
    {
        std::vector<Byte>  program = Tokenized ("10 END\n20 END\n30 END\n");
        Word               first   = (Word) (program[0] | (program[1] << 8));
        Word               second  = (Word) (program[6] | (program[7] << 8));
        Word               third   = (Word) (program[12] | (program[13] << 8));

        Assert::AreEqual ((int) 0x0807, (int) first,
            L"a six-byte line placed at $0801 puts the next one at $0807");
        Assert::AreEqual ((int) 0x080D, (int) second);
        Assert::AreEqual ((int) 0x0813, (int) third);

        Assert::AreEqual ((size_t) 20, program.size(),
            L"three six-byte lines and the two-byte null link");
    }


    TEST_METHOD (Detokenize_ALinkPointingElsewhere_IsRefusedRatherThanFollowed)
    {
        std::vector<Byte>      program = Tokenized ("10 END\n20 END\n");
        std::string            listing;
        ApplesoftListingError  error;

        program[0] = (Byte) (program[0] + 1);

        AssertFailed (ApplesoftTokenizer::Detokenize (program, listing, error),
            L"a link that disagrees with the layout is a damaged program, and walking by "
            L"the terminator regardless would hide it");

        Assert::IsTrue (listing.empty(), L"and nothing may be handed back");
        Assert::AreEqual ((uint32_t) 10, error.lineNumber, L"the refusal names the line");
    }


    //
    //  ------------------------------------------------------------------
    //  Refusals. Each one names the line number and quotes the text.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Refuse_ALineWithNoNumber_QuotesTheLine)
    {
        ApplesoftListingError  error = RefusalFor ("10 PRINT 1\nPRINT 2\n");

        Assert::IsFalse (error.hasLineNumber,
            L"there is no number to name, and inventing one would point at the wrong line");
        Assert::AreEqual (std::string ("PRINT 2"), error.sourceLine,
            L"so the text is what identifies it");
        Assert::AreEqual ((size_t) 2, error.sourceLineIndex,
            L"together with where it sits in the file");
    }


    TEST_METHOD (Refuse_ALineNumberAboveApplesoftsCeiling_NamesIt)
    {
        ApplesoftListingError  error = RefusalFor ("64000 END\n");

        Assert::IsTrue (error.hasLineNumber);
        Assert::AreEqual ((uint32_t) 64000, error.lineNumber);
        Assert::AreEqual (std::string ("64000 END"), error.sourceLine);
    }


    TEST_METHOD (Refuse_ABareLineNumber_IsNotAnEmptyLine)
    {
        // Applesoft reads a bare number as DELETE that line, so there is no
        // stored form of one. Placing it silently would drop whatever the writer
        // believed they had written.
        ApplesoftListingError  error = RefusalFor ("10 PRINT 1\n20\n");

        Assert::AreEqual ((uint32_t) 20, error.lineNumber);
        Assert::AreEqual (std::string ("20"), error.sourceLine);
    }


    TEST_METHOD (Refuse_TwoLinesWithOneNumber_NamesTheNumber)
    {
        ApplesoftListingError  error = RefusalFor ("10 PRINT 1\n10 PRINT 2\n");

        Assert::AreEqual ((uint32_t) 10, error.lineNumber);
        Assert::AreEqual (std::string ("10 PRINT 2"), error.sourceLine);
    }


    TEST_METHOD (Refuse_ACharacterWithNoAppleRepresentation_NamesTheLine)
    {
        ApplesoftListingError  error = RefusalFor ("10 PRINT \"caf\xC3\xA9\"\n");

        Assert::AreEqual ((uint32_t) 10, error.lineNumber);
        Assert::IsTrue (error.reason.find ("Apple II") != std::string::npos,
            L"and says what is wrong with the character rather than only that something is");
    }


    TEST_METHOD (Refuse_AListingWithNoLines_RatherThanPlacingAnEmptyProgram)
    {
        ApplesoftListingError  error = RefusalFor ("\n   \n\n");

        Assert::IsFalse (error.hasLineNumber);
        Assert::IsTrue (error.reason.find ("no numbered lines") != std::string::npos);
    }


    TEST_METHOD (Refuse_ALineLongerThanApplesoftCanHold_NamesIt)
    {
        std::string  source = "10 REM " + std::string (300, 'X') + "\n";

        ApplesoftListingError  error = RefusalFor (source);

        Assert::AreEqual ((uint32_t) 10, error.lineNumber);
        Assert::IsTrue (error.reason.find ("one line") != std::string::npos);
    }


    TEST_METHOD (Refuse_AProgramPastTheMemoryApplesoftHas_NamesTheLineItRanOutOn)
    {
        // Each line is 10 REM plus 240 X's, so roughly 250 bytes; a couple of
        // hundred of them run past $C000.
        std::string  source;
        int          line = 0;

        for (line = 1; line <= 250; line++)
        {
            source += std::to_string (line) + " REM " + std::string (240, 'X') + "\n";
        }

        ApplesoftListingError  error = RefusalFor (source);

        Assert::IsTrue (error.hasLineNumber, L"the refusal names the line it ran out on");
        Assert::IsTrue (error.reason.find ("memory") != std::string::npos);
    }


    //
    //  ------------------------------------------------------------------
    //  Detokenizing refuses what it cannot render exactly.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Detokenize_AByteThatIsNoToken_IsRefused)
    {
        std::vector<Byte>      program = Tokenized ("10 END\n");
        std::string            listing;
        ApplesoftListingError  error;

        program[4] = 0xEB;

        AssertFailed (ApplesoftTokenizer::Detokenize (program, listing, error));
        Assert::IsTrue (listing.empty());
    }


    TEST_METHOD (Detokenize_ATokenInsideAString_IsRefusedRatherThanSpelled)
    {
        // Applesoft cannot store this, and spelling it out would hand back a
        // listing that tokenizes to different bytes -- a round trip that looks
        // like it worked.
        std::vector<Byte>      program = Tokenized ("10 PRINT \"AB\"\n");
        std::string            listing;
        ApplesoftListingError  error;

        program[6] = ApplesoftTokenizer::kTokenPrint;

        AssertFailed (ApplesoftTokenizer::Detokenize (program, listing, error));
        Assert::IsTrue (listing.empty());
    }


    TEST_METHOD (Detokenize_ATokenInsideARemsText_IsRefusedToo)
    {
        std::vector<Byte>      program = Tokenized ("10 REM AB\n");
        std::string            listing;
        ApplesoftListingError  error;

        program[5] = ApplesoftTokenizer::kTokenPrint;

        AssertFailed (ApplesoftTokenizer::Detokenize (program, listing, error));
    }


    TEST_METHOD (Detokenize_BytesPastTheNullLink_AreRefused)
    {
        std::vector<Byte>      program = Tokenized ("10 END\n");
        std::string            listing;
        ApplesoftListingError  error;

        program.push_back (0x42);

        AssertFailed (ApplesoftTokenizer::Detokenize (program, listing, error),
            L"a file longer than the program it holds is not the program it holds");
    }


    TEST_METHOD (Detokenize_AProgramWithNoLines_IsRefusedRatherThanReturningNothing)
    {
        std::vector<Byte>      program = { 0x00, 0x00 };
        std::string            listing;
        ApplesoftListingError  error;

        AssertFailed (ApplesoftTokenizer::Detokenize (program, listing, error),
            L"an empty listing and a file that could not be read are not the same answer");
    }


    TEST_METHOD (Detokenize_ATruncatedLine_IsRefused)
    {
        std::vector<Byte>      program = Tokenized ("10 END\n");
        std::string            listing;
        ApplesoftListingError  error;

        program.resize (5);

        AssertFailed (ApplesoftTokenizer::Detokenize (program, listing, error));
    }
};
