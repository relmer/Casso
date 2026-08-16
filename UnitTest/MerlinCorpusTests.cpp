#include "Pch.h"

#include "MerlinCorpus/CorpusHarness.h"
#include "MerlinCorpus/MerlinFixture.h"
#include "EmuTests/FixtureProvider.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinCorpusTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CorpusHarnessTests
    //
    //  The comparison, tested against synthetic data before the corpus has real
    //  entries to hide a bug behind.
    //
    //  Four guards protect the corpus INPUTS -- non-empty, fresh, discriminating,
    //  provenance-correct -- and every one of them sits upstream of this. If the
    //  comparison is broken, each entry passes no matter how carefully it was
    //  captured: success reported, nothing checked, in the component least likely
    //  to be suspected precisely because it IS the check.
    //
    //  These are cheap now and impossible later. Once the corpus is populated a
    //  broken harness is invisible, because everything is green and everything
    //  looks right.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CorpusHarnessTests)
    {
    public:

        TEST_METHOD (IdenticalBytes_Match)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::Match);
            Assert::IsFalse (result.hasFirstDifference);
        }



        //  THE test. A length-only comparison passes this, and a same-length
        //  divergence in the middle is the likeliest real harness bug -- the
        //  bytes are the right shape and the wrong content.
        TEST_METHOD (SameLengthDifferentMiddle_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0xAD, 0x00, 0x04 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::ByteMismatch,
                            L"a same-length content difference must not pass");
            Assert::IsTrue (result.hasFirstDifference);
            Assert::AreEqual ((size_t) 2, result.firstDifference,
                              L"the offset of the first difference must be reported, not just that one exists");
        }



        //  A difference in the LAST byte catches a comparison that stops early.
        TEST_METHOD (SameLengthDifferentFinalByte_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0x8D, 0x00, 0x05 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::ByteMismatch,
                            L"a comparison that stops short would miss this");
            Assert::AreEqual ((size_t) 4, result.firstDifference);
        }



        TEST_METHOD (ActualShorterThanExpected_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D };
            std::vector<Byte>  actual   = { 0xA9, 0x41 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::LengthMismatch);
            Assert::AreEqual ((size_t) 3, result.expectedLength);
            Assert::AreEqual ((size_t) 2, result.actualLength);
        }



        //  The other direction, because a comparison that walks only the
        //  expected bytes reports a match when the actual output is longer.
        TEST_METHOD (ActualLongerThanExpected_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0x8D };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::LengthMismatch,
                            L"extra trailing output is a mismatch, not a match");
            Assert::AreEqual ((size_t) 2, result.expectedLength);
            Assert::AreEqual ((size_t) 3, result.actualLength);
        }



        //  A length mismatch must still say WHERE content diverged, so a
        //  truncation is distinguishable from output that went wrong and then
        //  also ended early.
        TEST_METHOD (LengthMismatchWithDifferingContent_ReportsTheContentOffset)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00 };
            std::vector<Byte>  actual   = { 0xA9, 0xFF };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::LengthMismatch);
            Assert::AreEqual ((size_t) 1, result.firstDifference,
                              L"content diverged at 1, before the length did");
        }



        //  T020a's guard, exercised THROUGH the harness rather than asserted
        //  about it. Nothing to compare against is indistinguishable from a
        //  passing comparison unless it is called out.
        TEST_METHOD (EmptyExpectation_IsAnErrorNotAMatch)
        {
            std::vector<Byte>  expected;
            std::vector<Byte>  actual = { 0xA9, 0x41 };
            CorpusComparison   result = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::EmptyExpectation,
                            L"an empty expectation must be an error rather than a trivially satisfied comparison");
        }



        //  Both empty is the worst case: a naive comparison calls it a match and
        //  the entry claims to have verified something.
        TEST_METHOD (BothEmpty_IsStillAnError)
        {
            std::vector<Byte>  expected;
            std::vector<Byte>  actual;
            CorpusComparison   result = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::EmptyExpectation,
                            L"two empty vectors comparing equal is exactly the vacuous pass this guards against");
        }



        //  A failing entry has to say where it went wrong, or a corpus failure is
        //  a bare assertion with a byte count and nowhere to start looking.
        TEST_METHOD (Describe_NamesTheOffsetAndTheEntry)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0xAD };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);
            std::string        text     = CorpusHarness::Describe ("hexdata", result);

            Assert::IsTrue (text.find ("hexdata") != std::string::npos, L"the entry must be named");
            Assert::IsTrue (text.find ("offset 2") != std::string::npos, L"the offset must be reported");
        }



        TEST_METHOD (Describe_CallsAnEmptyExpectationAnError)
        {
            std::vector<Byte>  expected;
            std::vector<Byte>  actual = { 0xA9 };
            CorpusComparison   result = CorpusHarness::Compare (expected, actual);
            std::string        text   = CorpusHarness::Describe ("hollow", result);

            Assert::IsTrue (text.find ("EMPTY EXPECTATION") != std::string::npos,
                            L"the message must not read like an ordinary mismatch");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StubFixtureProvider
    //
    //  Hands back bytes chosen by the test, so the decoder's rejection paths can
    //  be reached without a malformed file on disk. A fixture MUST NOT be edited
    //  to exercise a failure -- the fixtures are evidence, and one adjusted to
    //  make a test do something has stopped being evidence.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StubFixtureProvider : public IFixtureProvider
    {
    public:
        explicit StubFixtureProvider (const std::vector<Byte> & bytes)
            : m_bytes (bytes)
        {
        }

        HRESULT OpenFixture (const std::string & relativePath, std::vector<Byte> & outBytes) override
        {
            UNREFERENCED_PARAMETER (relativePath);
            outBytes = m_bytes;
            return S_OK;
        }

    private:
        std::vector<Byte>  m_bytes;
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinFixtureTests
    //
    //  The decode step, pinned against the committed vendor files.
    //
    //  This sits upstream of every corpus entry: entries compare bytes the
    //  decoder produced, so a decoder that strips the wrong number of header
    //  bytes or mangles the high bit yields expectations that are wrong
    //  uniformly -- and a corpus wrong in the same direction everywhere still
    //  looks entirely consistent.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinFixtureTests)
    {
    public:

        //  The measured figures for these two objects were recorded in the spec
        //  from the disk itself. Asserting them here makes the file the authority
        //  rather than the note about the file.
        TEST_METHOD (LoadObject_ReportsTheRecordedAddressAndLength)
        {
            FixtureProvider      provider;
            MerlinFixtureFile    labels;
            MerlinFixtureFile    clock;

            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/LABELS", labels));
            Assert::AreEqual (static_cast<size_t> (984), labels.payload.size(), L"LABELS is 984 bytes");
            Assert::AreEqual (0x8000, static_cast<int> (labels.loadAddress), L"LABELS loads at $8000");

            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/CLOCK.24", clock));
            Assert::AreEqual (static_cast<size_t> (365), clock.payload.size(), L"CLOCK.24 is 365 bytes");
            Assert::AreEqual (0x0240, static_cast<int> (clock.loadAddress), L"CLOCK.24 loads at $0240");
        }



        //  Merlin stores source as high-bit ASCII EXCEPT for spaces, which are
        //  plain $20 -- LABELS.S contains 81 of them. A decoder that required the
        //  high bit would fail on the first space of the first line, and the
        //  field model this whole dialect turns on is made of spaces.
        TEST_METHOD (LoadSource_KeepsSpacesThatCarryNoHighBit)
        {
            FixtureProvider  provider;
            std::string      text;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/LABELS.S", text));

            Assert::IsTrue (text.find (' ') != std::string::npos,
                            L"the decoded source must still contain spaces");
            Assert::IsTrue (text.find ('*') != std::string::npos,
                            L"and high-bit characters must have decoded to plain ASCII");
        }



        //  Every byte lands under $80 once masked, and Merlin's CR terminators
        //  become newlines. A stray $8D would mean the mask was skipped; a stray
        //  $0D would mean the translation was.
        TEST_METHOD (LoadSource_MasksEveryByteAndTranslatesTerminators)
        {
            FixtureProvider  provider;
            std::string      text;
            size_t           highBits    = 0;
            size_t           bareReturns = 0;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/LABELS.S", text));

            for (size_t i = 0; i < text.size(); i++)
            {
                if ((static_cast<Byte> (text[i]) & 0x80) != 0)
                {
                    highBits++;
                }

                if (text[i] == '\r')
                {
                    bareReturns++;
                }
            }

            Assert::AreEqual (static_cast<size_t> (0), highBits, L"no byte may keep its high bit");
            Assert::AreEqual (static_cast<size_t> (0), bareReturns, L"every CR must have become a newline");
            Assert::IsTrue (text.find ('\n') != std::string::npos, L"and the source must have line breaks at all");
        }



        //  The object side of the same rule. LABELS is the DCI-heavy specimen --
        //  105 of them -- and DCI marks its terminator by CLEARING the high bit.
        //  If this file were uniformly high-bit, a decoder could get away with
        //  asserting the bit; it is not, and the corpus exists to pin that.
        TEST_METHOD (LoadObject_PreservesBytesWithTheHighBitClear)
        {
            FixtureProvider      provider;
            MerlinFixtureFile    labels;
            size_t               lowBytes = 0;

            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/LABELS", labels));

            for (size_t i = 0; i < labels.payload.size(); i++)
            {
                if ((labels.payload[i] & 0x80) == 0)
                {
                    lowBytes++;
                }
            }

            Assert::IsTrue (lowBytes > 0,
                            L"LABELS must contain bytes with bit 7 clear, or the DCI evidence is not there");
        }



        //  A length that disagrees with what was read means the file was
        //  truncated, padded to a sector boundary, or extracted wrong. It still
        //  decodes into plausible bytes, which is exactly why it must be refused
        //  rather than skipped past.
        TEST_METHOD (LoadObject_RejectsADeclaredLengthThatDisagrees)
        {
            std::vector<Byte>    raw      = { 0x00, 0x80, 0x10, 0x00, 0xA9, 0x41 };  // claims 16, carries 2
            StubFixtureProvider  provider (raw);
            MerlinFixtureFile    file;
            HRESULT              hrLoad   = MerlinFixture::LoadObject (provider, "anything", file);

            Assert::IsTrue (FAILED (hrLoad),
                            L"a declared length that disagrees with the payload must fail, not decode");
        }



        TEST_METHOD (LoadObject_RejectsAFileTooShortToHoldAHeader)
        {
            std::vector<Byte>    raw      = { 0x00, 0x80 };
            StubFixtureProvider  provider (raw);
            MerlinFixtureFile    file;
            HRESULT              hrLoad   = MerlinFixture::LoadObject (provider, "anything", file);

            Assert::IsTrue (FAILED (hrLoad),
                            L"two bytes cannot carry a four-byte header");
        }



        //  The guard above is only worth having if it can actually fire, so this
        //  proves the accepting path agrees with the same header.
        TEST_METHOD (LoadObject_AcceptsAHeaderThatAgrees)
        {
            std::vector<Byte>    raw      = { 0x00, 0x80, 0x02, 0x00, 0xA9, 0x41 };
            StubFixtureProvider  provider (raw);
            MerlinFixtureFile    file;

            AssertSucceeded (MerlinFixture::LoadObject (provider, "anything", file));
            Assert::AreEqual (static_cast<size_t> (2), file.payload.size(), L"the header must not survive into the payload");
            Assert::AreEqual (0x8000, static_cast<int> (file.loadAddress), L"and the address must come off the header");
        }
    };
}
