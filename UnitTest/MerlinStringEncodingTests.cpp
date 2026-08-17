#include "Pch.h"

#include "StringEncoding.h"
#include "MerlinDialect.h"
#include "MerlinCorpus/MerlinFixture.h"
#include "MerlinCorpus/CorpusHarness.h"
#include "EmuTests/FixtureProvider.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinStringEncodingTests
{
    //  Splits one decoded source line into its string directive and the text
    //  between its delimiters. Deliberately tiny and local: the corpus test
    //  below needs only the string lines, and reusing the real parser for the
    //  field split is what keeps this from becoming a second grammar.
    static bool TryReadStringLine (
        const std::string          &  line,
        StringEncodingMode         &  outMode,
        std::string                &  outText,
        bool                       &  outHighBit)
    {
        MerlinDialect  merlin;
        ParsedLine     parsed    = merlin.ParseLine (line, 1);
        std::string    upper     = Parser::ToUpper (parsed.mnemonic);
        char           delimiter = 0;
        size_t         closing   = 0;
        bool           usable    = false;

        if (parsed.directiveToken != Directive::StringData)
        {
            return false;
        }

        if (parsed.operand.size() < 2)
        {
            return false;
        }

        delimiter = parsed.operand[0];
        closing   = parsed.operand.find (delimiter, 1);

        usable = (closing != std::string::npos);

        if (usable)
        {
            outMode    = MerlinDirectiveTable::EncodingModeForSpelling (upper);
            outText    = parsed.operand.substr (1, closing - 1);
            outHighBit = StringEncoding::HighBitFromDelimiter (delimiter);
        }

        return usable;
    }



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinDciOracleTests
    //
    //  The DCI encoding against vendor object code, through the ENCODER alone.
    //
    //  LABELS.S is 105 DCI lines, one ERR, and one BRK. Those DCI lines account
    //  for 983 of its 984 object bytes and need no labels, no expressions and no
    //  macros, so they can be compared without an assembler at all.
    //
    //  MerlinVendorOracleTests now assembles the whole file and matches all 984,
    //  which is the stronger claim -- but this one stays, and is not redundant.
    //  When the whole-file comparison breaks, it says only that 984 bytes stopped
    //  matching; this one says whether the ENCODING is what moved. Keeping a
    //  narrow oracle beneath a broad one is what turns a failure into a location.
    //
    //  This is the encoding the corpus exists to pin. A high-bit or terminator
    //  error here produces output that still reads as text in a hex dump, raises
    //  no diagnostic, and crashes nothing. Only these bytes catch it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinDciOracleTests)
    {
    public:

        TEST_METHOD (LabelsSourceDciLinesReproduceTheShippedObjectBytes)
        {
            FixtureProvider      provider;
            std::string          source;
            MerlinFixtureFile    object;
            std::vector<Byte>    produced;
            std::vector<Byte>    expected;
            size_t               start      = 0;
            size_t               lineCount  = 0;
            CorpusComparison     comparison = {};

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/LABELS.S", source));
            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/LABELS", object));

            while (start <= source.size())
            {
                size_t              end      = source.find ('\n', start);
                std::string         line     = source.substr (start, (end == std::string::npos) ? std::string::npos : end - start);
                StringEncodingMode  mode     = StringEncodingMode::Plain;
                std::string         text;
                bool                highBit  = false;

                if (TryReadStringLine (line, mode, text, highBit))
                {
                    StringEncoding::Encode (text, mode, highBit, produced);
                    lineCount++;
                }

                if (end == std::string::npos)
                {
                    break;
                }

                start = end + 1;
            }

            Assert::AreEqual (static_cast<size_t> (105), lineCount,
                              L"LABELS.S holds 105 DCI lines; a different count means the parse changed, not the encoding");

            // The 984th byte is the $00 of `END BRK`, which is an instruction
            // rather than string data -- so the comparison covers 983.
            expected.assign (object.payload.begin(), object.payload.begin() + 983);

            comparison = CorpusHarness::Compare (expected, produced);

            {
                std::string   described = CorpusHarness::Describe ("LABELS.S DCI encoding", comparison);
                std::wstring  message (described.begin(), described.end());

                Assert::IsTrue (comparison.verdict == CorpusVerdict::Match, message.c_str());
            }
        }



        //  The load address is half of "byte-identical". Merlin assembles to
        //  $8000 with no origin directive present, and LABELS.S contains none --
        //  so a wrong default produces 984 perfect bytes at the wrong address.
        TEST_METHOD (MerlinDefaultsToTheOriginItsObjectWasBuiltAt)
        {
            FixtureProvider      provider;
            MerlinFixtureFile    object;
            MerlinDialect        merlin;

            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/LABELS", object));

            Assert::AreEqual (static_cast<int> (object.loadAddress), static_cast<int> (merlin.GetDefaultOrigin()),
                              L"LABELS.S names no origin, so the dialect default must be where its object loads");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StringEncodingTests
    //
    //  The encoding rules on their own, so a failure says WHICH rule broke
    //  rather than only that 983 bytes stopped matching.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StringEncodingTests)
    {
    public:

        //  Verified directly against LABELS.S: `DCI "0200IN"` emits
        //  B0 B2 B0 B0 C9 4E -- high bit on every byte but the last.
        TEST_METHOD (DciInvertsTheHighBitOfTheLastCharacterOnly)
        {
            std::vector<Byte>  bytes;
            std::vector<Byte>  expected = { 0xB0, 0xB2, 0xB0, 0xB0, 0xC9, 0x4E };

            StringEncoding::Encode ("0200IN", StringEncodingMode::DciTerminated, true, bytes);

            Assert::IsTrue (bytes == expected, L"DCI marks its end by inverting the last character's high bit");
        }



        //  INVERTS rather than clears. Every DCI on the disk is high-ASCII, so an
        //  implementation that CLEARED the last bit would match the whole corpus
        //  while being wrong for a low-ASCII string -- the case nobody wrote down.
        TEST_METHOD (DciUnderLowAsciiSetsTheLastBitRatherThanClearingIt)
        {
            std::vector<Byte>  bytes;
            std::vector<Byte>  expected = { 0x41, 0x42, 0xC3 };

            StringEncoding::Encode ("ABC", StringEncodingMode::DciTerminated, false, bytes);

            Assert::IsTrue (bytes == expected,
                            L"with the high bit off, DCI's terminator must SET the last bit");
        }



        //  An empty string has no last character to mark, so nothing may be
        //  flipped -- least of all a byte an earlier directive emitted.
        TEST_METHOD (DciOnEmptyTextTouchesNothing)
        {
            std::vector<Byte>  bytes    = { 0xAA };
            std::vector<Byte>  expected = { 0xAA };

            StringEncoding::Encode ("", StringEncodingMode::DciTerminated, true, bytes);

            Assert::IsTrue (bytes == expected, L"an empty DCI must not reach back into bytes it did not write");
        }



        //  Verified against MAKE DUMP: `REV "CALL-151"` emits 151-LLAC in high
        //  ASCII, which is why searching that object for the forward spelling
        //  finds nothing.
        TEST_METHOD (RevEmitsItsTextBackToFront)
        {
            std::vector<Byte>  bytes;
            std::string        expectedText = "151-LLAC";
            std::vector<Byte>  expected;

            for (char ch : expectedText)
            {
                expected.push_back (static_cast<Byte> (ch) | 0x80);
            }

            StringEncoding::Encode ("CALL-151", StringEncodingMode::Reversed, true, bytes);

            Assert::IsTrue (bytes == expected, L"REV reverses the text before encoding it");
        }



        //  Both delimiters the corpus uses give HIGH ascii, which rules out any
        //  rule that orders delimiters by ASCII value: `!` ($21) sorts below `'`
        //  ($27) and would have to give low ASCII under one.
        TEST_METHOD (EveryDelimiterTheCorpusUsesSelectsHighAscii)
        {
            Assert::IsTrue (StringEncoding::HighBitFromDelimiter ('"'), L"the quote delimiter gives high ASCII");
            Assert::IsTrue (StringEncoding::HighBitFromDelimiter ('!'), L"and so does the bang, at a LOWER ASCII value");
            Assert::IsFalse (StringEncoding::HighBitFromDelimiter ('\''), L"the apostrophe is the documented low-ASCII case");
        }
    };
}
