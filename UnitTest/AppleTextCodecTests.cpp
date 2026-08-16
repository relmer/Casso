#include "Pch.h"
#include "EhmTestHelper.h"
#include "AppleTextCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextCodecTests
//
//  Round-trip identity is the primary invariant here, not agreement with
//  particular byte values. Whichever high-bit convention a filesystem turns out
//  to use, host -> Apple -> host must return the original text; a put/get cycle
//  is the user-visible form of the same property, so if the round trip holds
//  the feature is right regardless of which convention was chosen.
//
//  The cases below are the ones that break naive implementations: nothing at
//  all, no trailing newline, a lone terminator, consecutive terminators, and
//  exactly one line.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (AppleTextCodecTests)
{
public:

    void AssertRoundTrips (const std::string & text, AppleTextConvention convention)
    {
        vector<Byte>  encoded;
        std::string   decoded;
        size_t        badAt = 0;

        AssertSucceeded (AppleTextCodec::Encode (text, convention, encoded, badAt));
        AppleTextCodec::Decode (encoded, convention, decoded);

        Assert::AreEqual (text, decoded, L"host -> Apple -> host must be the identity");
    }

    void AssertRoundTripsBothConventions (const std::string & text)
    {
        AssertRoundTrips (text, AppleTextConvention::HighAscii);
        AssertRoundTrips (text, AppleTextConvention::PlainAscii);
    }

    TEST_METHOD (RoundTrip_EmptyText)
    {
        AssertRoundTripsBothConventions ("");
    }

    TEST_METHOD (RoundTrip_OneLineWithNoTrailingNewline)
    {
        // The case that tempts an implementation to append a terminator
        // unconditionally, which makes the round trip lossy.
        AssertRoundTripsBothConventions ("10 PRINT \"HELLO\"");
    }

    TEST_METHOD (RoundTrip_OneLineWithTrailingNewline)
    {
        // A file that ends with a newline and one that does not are different
        // files, and the round trip must keep them different.
        AssertRoundTripsBothConventions ("10 PRINT \"HELLO\"\n");
    }

    TEST_METHOD (RoundTrip_LoneTerminator)
    {
        AssertRoundTripsBothConventions ("\n");
    }

    TEST_METHOD (RoundTrip_ConsecutiveTerminators)
    {
        // Blank lines must survive as blank lines, not be collapsed.
        AssertRoundTripsBothConventions ("A\n\n\nB\n");
    }

    TEST_METHOD (RoundTrip_ManyLines)
    {
        AssertRoundTripsBothConventions ("ONE\nTWO\nTHREE\n");
    }

    TEST_METHOD (RoundTrip_PrintableAsciiRange)
    {
        std::string  all;
        int          c   = 0;

        for (c = 0x20; c <= 0x7E; c++)
        {
            all += (char) c;
        }

        AssertRoundTripsBothConventions (all);
    }

    TEST_METHOD (Decode_MixedHighAndLowAscii_IsToleratedNotRejected)
    {
        // Real vendor files are mixed. Merlin's T.MACRO LIBRARY is 1,578 of
        // 1,616 bytes high-bit, with 37 plain $20 spaces sitting among 236
        // high $A0 ones -- a banner comment line padded with low-ASCII inside
        // otherwise high-bit content.
        //
        // So the high bit is STRIPPED IF PRESENT AND TOLERATED IF ABSENT, never
        // validated. A decoder that required bit 7 would reject a genuine file.
        vector<Byte>  mixed;
        std::string   decoded;

        mixed.push_back (0xCC);   // 'L' high
        mixed.push_back (0xC4);   // 'D' high
        mixed.push_back (0xC1);   // 'A' high
        mixed.push_back (0x20);   // a PLAIN space, as the real file contains
        mixed.push_back (0xA0);   // and a high space, as it also contains
        mixed.push_back (0x23);   // plain '#'
        mixed.push_back (0x8D);   // high terminator

        AppleTextCodec::Decode (mixed, AppleTextConvention::HighAscii, decoded);

        // Two spaces, because the input carries both a plain $20 and a high
        // $A0 -- and they must decode to the SAME character, which is the point.
        Assert::AreEqual (std::string ("LDA  #\n"), decoded,
            L"both high and low bytes must decode to the same characters");
    }

    TEST_METHOD (Decode_PlainTerminatorInHighAsciiContent_StillEndsTheLine)
    {
        // The other half of tolerance: comparing the RAW byte to $8D would miss
        // a plain $0D line ending inside an otherwise high-bit file, silently
        // running two lines together. Comparing after stripping catches both.
        vector<Byte>  mixed;
        std::string   decoded;

        mixed.push_back (0xC1);   // 'A' high
        mixed.push_back (0x0D);   // a PLAIN carriage return
        mixed.push_back (0xC2);   // 'B' high

        AppleTextCodec::Decode (mixed, AppleTextConvention::HighAscii, decoded);

        Assert::AreEqual (std::string ("A\nB"), decoded,
            L"a plain terminator must end the line even in high-bit content");
    }

    TEST_METHOD (Encode_CrLfCollapsesToOneLine)
    {
        // Host text arrives with CRLF on this platform. Two terminators would
        // silently double every blank line in the file.
        vector<Byte>  encoded;
        size_t        badAt = 0;

        AssertSucceeded (AppleTextCodec::Encode ("A\r\nB\r\n", AppleTextConvention::HighAscii,
                                                 encoded, badAt));

        Assert::AreEqual (size_t (4), encoded.size(), L"two letters and two terminators");
        Assert::AreEqual (Byte (0xC1), encoded[0], L"'A' with the high bit set");
        Assert::AreEqual (Byte (0x8D), encoded[1]);
        Assert::AreEqual (Byte (0xC2), encoded[2]);
        Assert::AreEqual (Byte (0x8D), encoded[3]);
    }

    TEST_METHOD (Encode_ConventionsDifferOnlyInTheHighBit)
    {
        vector<Byte>  high;
        vector<Byte>  plain;
        size_t        badAt = 0;

        AssertSucceeded (AppleTextCodec::Encode ("Hi\n", AppleTextConvention::HighAscii,  high,  badAt));
        AssertSucceeded (AppleTextCodec::Encode ("Hi\n", AppleTextConvention::PlainAscii, plain, badAt));

        Assert::AreEqual (high.size(), plain.size());
        Assert::AreEqual (Byte (0xC8), high[0]);
        Assert::AreEqual (Byte (0x48), plain[0]);
        Assert::AreEqual (Byte (0x8D), high[2]);
        Assert::AreEqual (Byte (0x0D), plain[2]);
    }

    TEST_METHOD (Encode_UnrepresentableCharacter_IsRefusedWithItsOffset)
    {
        // A smart quote or em-dash from an autocorrecting editor arrives
        // without anyone typing it. Masking it to seven bits would produce a
        // different, plausible letter -- a conversion reporting success while
        // changing the text.
        vector<Byte>  encoded;
        size_t        badAt   = 0;
        std::string   text    = "OK ";
        HRESULT       hr      = S_OK;

        text += (char) 0xE2;   // first byte of a UTF-8 em-dash
        text += (char) 0x80;
        text += (char) 0x94;
        text += " MORE";

        hr = AppleTextCodec::Encode (text, AppleTextConvention::HighAscii, encoded, badAt);

        Assert::IsTrue (FAILED (hr), L"a character with no Apple II encoding must be refused");
        Assert::AreEqual (size_t (3), badAt, L"the offending offset must be reported");
        Assert::AreEqual (size_t (0), encoded.size(),
            L"a refused conversion produces nothing, not a partial file");
    }

    TEST_METHOD (Encode_ControlCharacter_IsRefused)
    {
        vector<Byte>  encoded;
        size_t        badAt = 0;
        std::string   text  = "A";
        HRESULT       hr    = S_OK;

        text += (char) 0x07;   // bell

        hr = AppleTextCodec::Encode (text, AppleTextConvention::HighAscii, encoded, badAt);

        Assert::IsTrue (FAILED (hr));
        Assert::AreEqual (size_t (1), badAt);
    }

    TEST_METHOD (Encode_TabIsAllowed_BecauseSourceListingsUseIt)
    {
        vector<Byte>  encoded;
        size_t        badAt = 0;

        AssertSucceeded (AppleTextCodec::Encode ("LDA\t#1\n", AppleTextConvention::HighAscii,
                                                 encoded, badAt));

        Assert::AreEqual (Byte (0x89), encoded[3], L"tab with the high bit set");
    }
};
