#include "Pch.h"

#include "MerlinCorpus/CorpusHarness.h"
#include "MerlinCorpus/MerlinFixture.h"
#include "EmuTests/FixtureProvider.h"
#include "EhmTestHelper.h"
#include "TestHelpers.h"
#include "Assembler.h"





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



        //  Spaces appear BOTH ways in stored source: $A0 separating fields, $20
        //  inside comment text. LABELS.S holds 214 of the first and 81 of the
        //  second, and across all nine committed sources spaces are the only
        //  bytes below $80. A decoder requiring the high bit dies on the first
        //  comment line of the first file -- long before reaching any DCI.
        //
        //  Both forms must arrive as one ordinary space. The parser is not
        //  allowed to tell them apart (see MerlinFixture.h): source reaching
        //  Casso by any other route carries no such distinction.
        TEST_METHOD (LoadSource_DecodesBothSpaceFormsToOneOrdinarySpace)
        {
            FixtureProvider  provider;
            std::string      text;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/LABELS.S", text));

            //  "END BRK ;table end" -- $A0 separators before and after BRK, then
            //  $20 spaces inside the comment. If either form survived undecoded
            //  this substring could not match.
            Assert::IsTrue (text.find ("END BRK ;table end") != std::string::npos,
                            L"field-separating $A0 and comment-text $20 must both decode to ' '");
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
            HRESULT              hrLoad   = S_OK;

            //  The decoder validates with asserting macros, because a fixture
            //  that fails these checks means the extraction is broken rather
            //  than that a user typed something odd. This test drives that path
            //  deliberately, so the assertion is expected rather than a failure.
            {
                UnitTestHelpers::ExpectedEhmAssert  expected;

                hrLoad = MerlinFixture::LoadObject (provider, "anything", file);
            }

            Assert::IsTrue (FAILED (hrLoad),
                            L"a declared length that disagrees with the payload must fail, not decode");
        }



        TEST_METHOD (LoadObject_RejectsAFileTooShortToHoldAHeader)
        {
            std::vector<Byte>    raw      = { 0x00, 0x80 };
            StubFixtureProvider  provider (raw);
            MerlinFixtureFile    file;
            HRESULT              hrLoad   = S_OK;

            {
                UnitTestHelpers::ExpectedEhmAssert  expected;

                hrLoad = MerlinFixture::LoadObject (provider, "anything", file);
            }

            Assert::IsTrue (FAILED (hrLoad),
                            L"two bytes cannot carry a four-byte header");
        }



        //  DOS 3.3 gives a type-T file no header at all: T.SENDMSG's bytes begin
        //  with the literal characters "SE".
        TEST_METHOD (LoadTextSource_ReadsATypeTFileFromOffsetZero)
        {
            FixtureProvider  provider;
            std::string      text;

            AssertSucceeded (MerlinFixture::LoadTextSource (provider, "Merlin/T.SENDMSG", text));

            Assert::IsTrue (text.rfind ("SE", 0) == 0,
                            L"a type-T file starts at its first byte, with no header to skip");
        }



        //  Reaching for the wrong loader must fail rather than quietly return
        //  text missing its first four characters. T.SENDMSG's leading bytes read
        //  as a header claiming 50382 bytes of a 149-byte file, so the length
        //  check catches it -- which is the second job that check does, beyond
        //  the truncated-extraction case it was written for.
        TEST_METHOD (LoadSource_RefusesATypeTFileRatherThanEatingFourCharacters)
        {
            FixtureProvider  provider;
            std::string      text;
            HRESULT          hrLoad = S_OK;

            {
                UnitTestHelpers::ExpectedEhmAssert  expected;

                hrLoad = MerlinFixture::LoadSource (provider, "Merlin/T.SENDMSG", text);
            }

            Assert::IsTrue (FAILED (hrLoad),
                            L"a headerless file decoded as type-B must fail, not lose its first four bytes");
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



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinVendorOracleTests
    //
    //  Whole vendor sources through the real assembler, compared against the
    //  objects the vendor shipped beside them.
    //
    //  This is a stronger claim than any encoder-level comparison. An encoder test
    //  can be fed exactly the lines it knows how to handle; here the assembler is
    //  handed the file as it sits on the disk, and every line has to be understood
    //  -- the comment conventions, the column-0 label, the string encoding, the
    //  instruction, and the assembly-time assertion -- for the byte count alone to
    //  come out right.
    //
    //  The LOAD ADDRESS is asserted alongside the bytes. LABELS.S names no origin
    //  anywhere, so its address comes entirely from the dialect's default; without
    //  this assertion a wrong default yields 984 perfect bytes in the wrong place.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinVendorOracleTests)
    {
    public:

        TEST_METHOD (LabelsSourceAssemblesToItsShippedObjectByteForByte)
        {
            FixtureProvider    provider;
            TestCpu            cpu;
            std::string        source;
            MerlinFixtureFile  object;
            AssemblerOptions   options    = {};
            AssemblyResult     result;
            CorpusComparison   comparison = {};

            cpu.InitForTest();
            options.dialect = DialectId::Merlin;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/LABELS.S", source));
            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/LABELS", object));

            {
                Assembler  merlin (cpu.GetInstructionSet(), options);

                result = merlin.Assemble (source);
            }

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());

            comparison = CorpusHarness::Compare (object.payload, result.bytes);

            {
                std::string   described = CorpusHarness::Describe ("LABELS.S", comparison);
                std::wstring  message (described.begin(), described.end());

                Assert::IsTrue (comparison.verdict == CorpusVerdict::Match, message.c_str());
            }

            Assert::AreEqual (static_cast<int> (object.loadAddress), static_cast<int> (result.startAddress),
                              L"LABELS.S names no origin, so its load address comes from the dialect default alone");
        }



        //  The same source under AS65. This is the discriminating half: labels,
        //  expressions and the evaluator are shared, so an entry that passes under
        //  both dialects is not evidence that the Merlin profile was consulted.
        TEST_METHOD (LabelsSourceUnderAs65DoesNotProduceMerlinsBytes)
        {
            FixtureProvider    provider;
            TestCpu            cpu;
            std::string        source;
            MerlinFixtureFile  object;
            AssemblerOptions   options    = {};
            AssemblyResult     result;
            CorpusComparison   comparison = {};

            cpu.InitForTest();
            options.dialect = DialectId::As65;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/LABELS.S", source));
            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/LABELS", object));

            {
                Assembler  as65 (cpu.GetInstructionSet(), options);

                result = as65.Assemble (source);
            }

            comparison = CorpusHarness::Compare (object.payload, result.bytes);

            Assert::IsFalse (comparison.verdict == CorpusVerdict::Match,
                             L"Merlin source assembling identically under AS65 would mean the profile is not being consulted");
        }

        //  The second whole-file oracle, and a far broader one than LABELS.S. It
        //  is the largest source on the disk, it assembles THREE sections at
        //  three different addresses into one contiguous object, and it exercises
        //  macros, per-expansion body labels, local labels, raw hexadecimal, the
        //  string family with trailing hexadecimal runs, branch aliases,
        //  assembly-time assertions in both forms, and the byte selectors.
        //
        //  The 589 bytes are what makes it worth having. Nothing was relaxed to
        //  reach them: the comparison is whole-file, byte-for-byte, against the
        //  object the vendor shipped beside the source.
        TEST_METHOD (MakeDumpSourceAssemblesToItsShippedObjectByteForByte)
        {
            FixtureProvider    provider;
            TestCpu            cpu;
            std::string        source;
            MerlinFixtureFile  object;
            AssemblerOptions   options    = {};
            AssemblyResult     result;
            CorpusComparison   comparison = {};

            cpu.InitForTest();
            options.dialect = DialectId::Merlin;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/MAKE DUMP.S", source));
            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/MAKE DUMP", object));

            {
                Assembler  merlin (cpu.GetInstructionSet(), options);

                result = merlin.Assemble (source);
            }

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());

            comparison = CorpusHarness::Compare (object.payload, result.bytes);

            {
                std::string   described = CorpusHarness::Describe ("MAKE DUMP.S", comparison);
                std::wstring  message (described.begin(), described.end());

                Assert::IsTrue (comparison.verdict == CorpusVerdict::Match, message.c_str());
            }

            //  The load address is half the claim. Three sections at $9000,
            //  $0300 and $0900 collapse to ONE object loading at $9000, which is
            //  only true because the origin directive relocates rather than
            //  seeking -- and 589 contiguous bytes cannot span that range.
            Assert::AreEqual (static_cast<int> (object.loadAddress), static_cast<int> (result.startAddress),
                              L"the object loads where its first origin put it, not where its lowest section runs");
        }



        //  The discriminating half. Every construct above is Merlin's, so a
        //  result identical under AS65 would mean the profile was never consulted.
        TEST_METHOD (MakeDumpSourceUnderAs65DoesNotProduceMerlinsBytes)
        {
            FixtureProvider    provider;
            TestCpu            cpu;
            std::string        source;
            MerlinFixtureFile  object;
            AssemblerOptions   options    = {};
            AssemblyResult     result;
            CorpusComparison   comparison = {};

            cpu.InitForTest();
            options.dialect = DialectId::As65;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/MAKE DUMP.S", source));
            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/MAKE DUMP", object));

            {
                Assembler  as65 (cpu.GetInstructionSet(), options);

                result = as65.Assemble (source);
            }

            comparison = CorpusHarness::Compare (object.payload, result.bytes);

            Assert::IsFalse (comparison.verdict == CorpusVerdict::Match,
                             L"Merlin source assembling identically under AS65 would mean the profile is not being consulted");
        }

        //  CLOCK.S -- ONE source, TWO shipped objects, and the whole reason the
        //  keyboard-input directive was worth solving rather than routing around.
        //  `VERSION` selects which build assembles, so a single 6022-byte file
        //  yields two independent byte-identical checks and the corpus's only
        //  coverage of conditional assembly deciding an object's contents.
        //
        //  The two objects differ in exactly four bytes of 365, which is the
        //  useful part: a conditional-assembly defect shows up as a specific
        //  small delta rather than a wall of noise.
        TEST_METHOD (ClockSourceAssemblesToItsTwentyFourHourObjectByteForByte)
        {
            AssertOracleMatches ("Merlin/CLOCK.S", "Merlin/CLOCK.24",
                                 { { "SAVOBJ", 0 }, { "VERSION", 24 } }, L"CLOCK.S at VERSION 24");
        }



        TEST_METHOD (ClockSourceAssemblesToItsTwelveHourObjectByteForByte)
        {
            AssertOracleMatches ("Merlin/CLOCK.S", "Merlin/CLOCK.12",
                                 { { "SAVOBJ", 0 }, { "VERSION", 12 } }, L"CLOCK.S at VERSION 12");
        }



        //  And the two answers must not produce the same bytes, or the pair above
        //  is two copies of one check. This is the assertion that makes CLOCK.S
        //  worth two entries rather than one.
        TEST_METHOD (TheTwoClockBuildsDifferFromEachOther)
        {
            AssemblyResult  twentyFour = AssembleOracle ("Merlin/CLOCK.S", { { "SAVOBJ", 0 }, { "VERSION", 24 } });
            AssemblyResult  twelve     = AssembleOracle ("Merlin/CLOCK.S", { { "SAVOBJ", 0 }, { "VERSION", 12 } });

            Assert::IsFalse (twentyFour.bytes == twelve.bytes,
                             L"the version answer must reach the object, or both entries prove the same thing");
        }



        //  KEYMAC.S -- general-purpose, and the source that needs `?` accepted
        //  inside a symbol. Its own `SAVOBJ` gates only the save directive, so the
        //  bytes are the same either way and the answer that avoids the
        //  out-of-subset construct is the one to give.
        TEST_METHOD (KeymacSourceAssemblesToItsShippedObjectByteForByte)
        {
            AssertOracleMatches ("Merlin/KEYMAC.S", "Merlin/KEYMAC",
                                 { { "SAVOBJ", 0 } }, L"KEYMAC.S");
        }



        //  PRINTFILER.S -- both of its answers are SEMANTIC, and which pair the
        //  vendor used to build the shipped object is recorded nowhere. So the
        //  test searches: assemble all four and require exactly one match.
        //
        //  Exactly one is the claim worth making. More than one would mean an
        //  answer reaches no byte, which would make the search meaningless; none
        //  would mean the assembler is wrong or the combinations are not what the
        //  source says they are. Either is a finding.
        TEST_METHOD (ExactlyOneAnswerPairReproducesPrintfilersShippedObject)
        {
            MerlinFixtureFile  object;
            FixtureProvider    provider;
            int                matches = 0;
            int                matched = -1;

            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/PRINTFILER", object));

            for (int combination = 0; combination < 4; combination++)
            {
                AssemblyResult  result = AssembleOracle ("Merlin/PRINTFILER.S",
                                                         { { "FORMAT",  combination & 1 },
                                                           { "MONITOR", (combination >> 1) & 1 } });

                if (result.errors.empty() && result.bytes == object.payload)
                {
                    matches++;
                    matched = combination;
                }
            }

            Assert::AreEqual (1, matches, L"exactly one of the four answer pairs must reproduce the shipped object");
            Assert::AreEqual (1, matched, L"and it is the pair with formatting on and monitoring off");
        }



        //  The discriminating half for the three sources added here, in one test
        //  rather than three: each is full of Merlin constructs, so a result
        //  identical under AS65 would mean the profile was never consulted. AS65
        //  does not even know the directive that supplies the answers, so these
        //  cannot assemble at all there -- which is itself the evidence.
        TEST_METHOD (TheKeyboardInputSourcesDoNotAssembleUnderAs65)
        {
            const char *  sources[] = { "Merlin/CLOCK.S", "Merlin/KEYMAC.S", "Merlin/PRINTFILER.S" };

            Assert::AreEqual ((size_t) 3, std::size (sources), L"three sources, or this sweep covers less than it claims");

            for (const char * path : sources)
            {
                FixtureProvider   provider;
                TestCpu           cpu;
                std::string       source;
                AssemblerOptions  options = {};
                AssemblyResult    result;

                cpu.InitForTest();
                options.dialect = DialectId::As65;

                AssertSucceeded (MerlinFixture::LoadSource (provider, path, source));

                {
                    Assembler  as65 (cpu.GetInstructionSet(), options);

                    result = as65.Assemble (source);
                }

                Assert::IsFalse (result.errors.empty(),
                                 L"Merlin source assembling cleanly under AS65 would mean the profile is not consulted");
            }
        }

    private:

        //  One vendor source through the real assembler, with the answers its
        //  keyboard-input lines name.
        static AssemblyResult AssembleOracle (const char * sourcePath,
                                              const std::unordered_map<std::string, int32_t> & answers)
        {
            FixtureProvider   provider;
            TestCpu           cpu;
            std::string       source;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect           = DialectId::Merlin;
            options.predefinedSymbols = answers;

            AssertSucceeded (MerlinFixture::LoadSource (provider, sourcePath, source));

            Assembler  merlin (cpu.GetInstructionSet(), options);

            return merlin.Assemble (source);
        }



        //  The whole comparison for one oracle: no diagnostics, the shipped bytes
        //  exactly, and the shipped load address. The address is half the claim --
        //  a wrong origin yields byte-perfect output in the wrong place.
        static void AssertOracleMatches (const char * sourcePath,
                                         const char * objectPath,
                                         const std::unordered_map<std::string, int32_t> & answers,
                                         const wchar_t * what)
        {
            FixtureProvider    provider;
            MerlinFixtureFile  object;
            AssemblyResult     result     = AssembleOracle (sourcePath, answers);
            CorpusComparison   comparison = {};

            AssertSucceeded (MerlinFixture::LoadObject (provider, objectPath, object));

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());

            comparison = CorpusHarness::Compare (object.payload, result.bytes);

            {
                std::string   described = CorpusHarness::Describe (objectPath, comparison);
                std::wstring  message (described.begin(), described.end());

                Assert::IsTrue (comparison.verdict == CorpusVerdict::Match, message.c_str());
            }

            Assert::AreEqual (static_cast<int> (object.loadAddress), static_cast<int> (result.startAddress), what);
        }


        //  The first diagnostic, so a failure names what the assembler objected to
        //  instead of only that it objected. Empty when the assembly was clean.
        static std::wstring FirstDiagnostic (const AssemblyResult & result)
        {
            std::string   text;
            std::wstring  wide;

            if (!result.errors.empty())
            {
                text = "line " + std::to_string (result.errors[0].lineNumber) + ": " + result.errors[0].message
                     + " (" + std::to_string (result.errors.size()) + " total)";
            }

            wide.assign (text.begin(), text.end());

            return wide;
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NegativeEntry
    //
    //  One hand-authored piece of source that must FAIL, and what its failure has
    //  to say.
    //
    //  A separate class from the captured entries, because the two are evidence of
    //  different things and must not be mistaken for one another. A captured entry
    //  says "real Merlin produced these bytes from this source" and is only as good
    //  as the capture. These say "Casso must refuse this and explain why", which no
    //  capture can establish -- real Merlin ACCEPTS the relocatable constructs here,
    //  so an oracle would disagree with every one of them.
    //
    //  Each row states four things a wrong implementation gets wrong differently,
    //  which is why all four are stated. The KIND separates a deliberate refusal
    //  from a failure to parse -- structurally, not by wording, since a message can
    //  be made to contain any phrase. The LINE and COLUMN are exact, and no two
    //  entries share a column, so an implementation stamping a constant fails. And
    //  `mustNotSay` names the diagnostic that would appear if the feature were
    //  absent, which is the assertion a bare substring check cannot make: `mustSay`
    //  alone is satisfied by a message that ALSO reports the symptom, and reporting
    //  both is not what was asked for.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct NegativeEntry
    {
        const char      *  name;
        const char      *  source;
        DiagnosticKind     kind;
        int                line;
        int                column;

        // A phrase the diagnostic must carry, and one it must not. Null for an
        // entry with nothing to rule out.
        const char      *  mustSay;
        const char      *  mustNotSay;
    };



    //  The negative corpus. Indentation is chosen per entry so that no two
    //  expected columns are the same number.
    static constexpr NegativeEntry  s_kNegativeCorpus[] =
    {
        {
            "relocatable mode",
            "   REL\n"
            "        LDA #$00\n",
            DiagnosticKind::SubsetBoundary, 1, 4,
            "REL", "Invalid mnemonic",
        },
        {
            "entry symbol declaration",
            "      ENT START\n"
            "START LDA #$00\n",
            DiagnosticKind::SubsetBoundary, 1, 7,
            "ENT", "Invalid mnemonic",
        },
        {
            "external symbol declaration",
            "     EXT OTHER\n"
            "     LDA OTHER\n",
            DiagnosticKind::SubsetBoundary, 1, 6,
            "EXT", "Invalid mnemonic",
        },
        {
            //  The one refusal that must DENY a dependency rather than omit it:
            //  disk file access arriving will not settle what this should do.
            "save-object directive",
            "        SAV OBJECT\n",
            DiagnosticKind::SubsetBoundary, 1, 9,
            "disk file access will not settle", "Invalid mnemonic",
        },
        {
            "output file-type directive",
            "                  TYP $06\n",
            DiagnosticKind::SubsetBoundary, 1, 19,
            "TYP", "Invalid mnemonic",
        },
        {
            //  A LABEL written where another assembler would put it. Merlin's
            //  line model reads it as the opcode, so without this the developer
            //  is told their own label is not an instruction -- true, useless,
            //  and silent about the column rule that caused it.
            "indented label",
            "               LOOP LDA #$00\n",
            DiagnosticKind::SourceError, 1, 16,
            "must begin in column 1", "Invalid mnemonic",
        },
        {
            //  Source written for the other dialect. The diagnostic has to name
            //  the dialect that DOES define the construct; "not an instruction"
            //  sends the reader hunting for a typo that is not there.
            "an as65 directive under merlin",
            "             .BYTE $01\n",
            DiagnosticKind::SourceError, 1, 14,
            "belonging to the as65 dialect", "belonging to the merlin dialect",
        },
        {
            //  A shared-engine diagnostic about a directive BOTH dialects have.
            //  It has to quote the spelling the source could actually have
            //  written: a Merlin file holds no dotted form of anything, so a
            //  message naming one describes a construct that cannot exist here.
            "origin directive quoted in Merlin's own spelling",
            "       ORG $10+\n",
            DiagnosticKind::SourceError, 1, 12,
            "ORG expression must be resolvable", ".org",
        },
        {
            //  A macro invoked with the OTHER dialect's argument separator. One
            //  argument arrives where two were meant, and the body's second
            //  parameter has nothing to take. Refused rather than expanded with
            //  the gap left empty -- see the byte assertion below, which is the
            //  half that says "rather than partially expanded".
            "macro invoked with as65 argument syntax",
            "STORE     MAC\n"
            "          LDA ]1\n"
            "          STA ]2\n"
            "          <<<\n"
            "          STORE $10,$20\n",
            DiagnosticKind::SourceError, 5, 11,
            "supplies 1 argument", "Invalid mnemonic",
        },
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinNegativeCorpusTests
    //
    //  The sweep over the negative corpus, plus the claims that need more than a
    //  diagnostic to state.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinNegativeCorpusTests)
    {
    public:

        //  The absent-corpus guard, in the direction a loop cannot check itself.
        //  A sweep over an empty table reports success having compared nothing,
        //  and is indistinguishable in the output from a full one.
        TEST_METHOD (TheNegativeCorpusIsNotEmpty)
        {
            Assert::IsTrue (std::size (s_kNegativeCorpus) >= 8,
                            L"the negative corpus must cover the refused constructs and the diagnostic expectations");
        }



        TEST_METHOD (EveryNegativeEntryFailsWhereAndAsItSays)
        {
            for (const NegativeEntry & entry : s_kNegativeCorpus)
            {
                AssemblyResult  result = AssembleAsMerlin (entry.source);
                std::wstring    what   = Describe (entry, result);

                Assert::AreEqual ((size_t) 1, result.errors.size(), what.c_str());
                Assert::IsTrue (result.errors[0].kind == entry.kind, what.c_str());
                Assert::AreEqual (entry.line, result.errors[0].lineNumber, what.c_str());
                Assert::AreEqual (entry.column, result.errors[0].column, what.c_str());

                Assert::IsTrue (result.errors[0].message.find (entry.mustSay) != std::string::npos, what.c_str());

                if (entry.mustNotSay != nullptr)
                {
                    Assert::IsTrue (result.errors[0].message.find (entry.mustNotSay) == std::string::npos,
                                    what.c_str());
                }
            }
        }



        //  Every column in the table is distinct, so an implementation stamping
        //  one constant everywhere cannot satisfy the sweep. Asserted rather than
        //  left as a comment, because the property is a fact about the TABLE and a
        //  row added later would quietly cost it.
        TEST_METHOD (NoTwoEntriesExpectTheSameColumn)
        {
            for (size_t i = 0; i < std::size (s_kNegativeCorpus); i++)
            {
                for (size_t j = i + 1; j < std::size (s_kNegativeCorpus); j++)
                {
                    Assert::AreNotEqual (s_kNegativeCorpus[i].column, s_kNegativeCorpus[j].column,
                                         L"two entries sharing a column would let one constant satisfy both");
                }
            }
        }



        //  The half of the macro entry a diagnostic cannot state. "Rejected
        //  rather than partially expanded" is a claim about what was NOT emitted,
        //  and an implementation reporting the mismatch and then expanding the
        //  body anyway satisfies every assertion in the sweep above.
        TEST_METHOD (TheMisPunctuatedMacroCallEmitsNothing)
        {
            AssemblyResult  result = AssembleAsMerlin ("STORE     MAC\n"
                                                       "          LDA ]1\n"
                                                       "          STA ]2\n"
                                                       "          <<<\n"
                                                       "          STORE $10,$20\n");

            Assert::IsTrue (result.bytes.empty(), L"a refused invocation must emit no part of the body");
        }



        //  The control, and it is not optional. Without it the rejection above is
        //  evidence of nothing -- a macro mechanism too broken to expand anything
        //  would pass it. The same call with Merlin's own separator assembles the
        //  whole body.
        TEST_METHOD (TheSameCallWithMerlinsSeparatorAssembles)
        {
            AssemblyResult  result = AssembleAsMerlin ("STORE     MAC\n"
                                                       "          LDA ]1\n"
                                                       "          STA ]2\n"
                                                       "          <<<\n"
                                                       "          STORE $10;$20\n");

            Assert::IsTrue (result.errors.empty(), FirstError (result).c_str());
            Assert::AreEqual ((size_t) 4, result.bytes.size(),
                              L"two zero-page instructions, two bytes each");
        }



        //  The mirror of the as65-directive entry, and the reason it is a test
        //  rather than a ninth row: it runs under the OTHER dialect, which the
        //  sweep's fixture cannot do. Both directions are needed, or "names the
        //  foreign dialect" is satisfied by an implementation that always names
        //  merlin.
        TEST_METHOD (AMerlinDirectiveUnderAs65NamesMerlin)
        {
            //  A directive with NO operand, deliberately. `DCI "HI"` fails one
            //  step earlier under as65 -- the quoted text is not an expression it
            //  can evaluate -- so it never reaches the point where a mnemonic is
            //  looked up at all, and the attribution would go untested.
            AssemblyResult  result = AssembleAsAs65 ("  .org $800\n  FIN\n");

            Assert::IsFalse (result.errors.empty(), L"FIN is not an as65 construct and must fail under as65");
            Assert::IsTrue (result.errors[0].message.find ("belonging to the merlin dialect") != std::string::npos,
                            FirstError (result).c_str());
        }



        //  An instruction SPELLING rather than a directive, which is the other
        //  way a dialect can claim a word. BLT is a real instruction under
        //  another name, so "invalid mnemonic" is true of the spelling and false
        //  of the operation -- and the two categories must not read alike.
        TEST_METHOD (AMerlinBranchAliasUnderAs65IsNamedAsASpelling)
        {
            AssemblyResult  result = AssembleAsAs65 ("  .org $800\nHERE: BLT HERE\n");

            Assert::IsFalse (result.errors.empty(), L"as65 must not accept Merlin's branch spellings");
            Assert::IsTrue (result.errors[0].message.find ("alternate instruction spelling") != std::string::npos,
                            FirstError (result).c_str());
            Assert::IsTrue (result.errors[0].message.find ("belonging to the merlin dialect") != std::string::npos,
                            FirstError (result).c_str());
        }

    private:

        static AssemblyResult AssembleAsMerlin (const std::string & source)
        {
            return AssembleUnder (DialectId::Merlin, source);
        }



        static AssemblyResult AssembleAsAs65 (const std::string & source)
        {
            return AssembleUnder (DialectId::As65, source);
        }



        static AssemblyResult AssembleUnder (DialectId dialect, const std::string & source)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};
            AssemblyResult    result;

            cpu.InitForTest();
            options.dialect = dialect;

            {
                Assembler  assembler (cpu.GetInstructionSet(), options);

                result = assembler.Assemble (source);
            }

            return result;
        }



        static std::wstring FirstError (const AssemblyResult & result)
        {
            std::string   text = "no diagnostic at all";
            std::wstring  wide;

            if (!result.errors.empty())
            {
                text = "line " + std::to_string (result.errors[0].lineNumber) + " col "
                     + std::to_string (result.errors[0].column) + ": " + result.errors[0].message
                     + " (" + std::to_string (result.errors.size()) + " total)";
            }

            wide.assign (text.begin(), text.end());

            return wide;
        }



        static std::wstring Describe (const NegativeEntry & entry, const AssemblyResult & result)
        {
            std::string   text;
            std::wstring  wide;

            text = std::string ("entry \"") + entry.name + "\" expected line " + std::to_string (entry.line)
                 + " col " + std::to_string (entry.column) + " saying \"" + entry.mustSay + "\"; got ";

            if (result.errors.empty())
            {
                text += "no diagnostic at all";
            }
            else
            {
                text += "line " + std::to_string (result.errors[0].lineNumber) + " col "
                      + std::to_string (result.errors[0].column) + ": " + result.errors[0].message
                      + " (" + std::to_string (result.errors.size()) + " total)";
            }

            wide.assign (text.begin(), text.end());

            return wide;
        }
    };
}
