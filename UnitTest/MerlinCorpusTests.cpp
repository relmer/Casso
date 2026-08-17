#include "Pch.h"

#include "MerlinCorpus/CorpusHarness.h"
#include "MerlinCorpus/MerlinFixture.h"
#include "EmuTests/FixtureProvider.h"
#include "EhmTestHelper.h"
#include "TestHelpers.h"
#include "MockFileReader.h"
#include "Assembler.h"
#include "DialectRegistry.h"
#include "DialectProfile.h"
#include "MerlinSubsetBoundary.h"





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
    //  VendorOracleAnswer
    //
    //  One symbol a source's keyboard-input line names, and the answer given to
    //  it. Merlin asks the operator; a batch assembly is told.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct VendorOracleAnswer
    {
        const char *  symbol;
        int32_t       value;
    };



    //  The answer sets, named rather than written inline, because two of them are
    //  FINDINGS rather than configuration. PRINTFILER's pair is the vendor's own
    //  1984 build configuration, recovered from the shipped bytes by trying all
    //  four; CLOCK's two select which of two shipped objects the one source
    //  produces.
    static constexpr VendorOracleAnswer  s_kSaveObjectOff[]    = { { "SAVOBJ", 0 } };
    static constexpr VendorOracleAnswer  s_kTwentyFourHour[]   = { { "SAVOBJ", 0 }, { "VERSION", 24 } };
    static constexpr VendorOracleAnswer  s_kTwelveHour[]       = { { "SAVOBJ", 0 }, { "VERSION", 12 } };
    static constexpr VendorOracleAnswer  s_kVendorBuild[]      = { { "FORMAT", 1 }, { "MONITOR", 0 } };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  VendorOracleEntry
    //
    //  One unit of correctness evidence taken from the distribution disk: a
    //  vendor source, the object the vendor shipped beside it, and the answers
    //  the source asks for.
    //
    //  The source is a PATH and never text. That is the whole of the
    //  unmodified-source claim: an entry cannot carry a transcribed or tidied
    //  copy, because it carries no copy at all, and the sweep re-derives the
    //  assembled text from the fixture bytes on every run.
    //
    //  `rawBytes` is the complete stored file including its four-byte header, as
    //  recorded in the fixture inventory. It pins the fixture at the size the
    //  extraction produced, so a truncated, padded or re-saved file is a failure
    //  here rather than a comparison against different evidence.
    //
    //  `discriminates` is what stops the corpus being vacuous. Labels, origin,
    //  literals and the expression evaluator are SHARED between dialects, so an
    //  entry built only from those assembles identically whether the Merlin
    //  profile works or is never consulted. An entry claiming a Merlin construct
    //  must also FAIL under AS65.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct VendorOracleEntry
    {
        const char                          *  name;
        const char                          *  sourcePath;
        const char                          *  objectPath;
        size_t                                 sourceRawBytes;
        size_t                                 objectRawBytes;
        std::span<const VendorOracleAnswer>    answers;
        bool                                   discriminates;
    };



    //  The positive corpus: five vendor sources, six shipped objects. Every one
    //  is absolute-mode Merlin with both halves present on the disk, which is
    //  what makes it an oracle rather than a specimen.
    //
    //  The two linker-demo sources are deliberately absent. They ship no object,
    //  and the objects sitting beside them on the disk are POST-LINK, so a
    //  positive comparison against either would be comparing Casso's output
    //  against bytes no assembler produced. They are negative specimens and a
    //  sweep asserts they never appear here.
    static constexpr VendorOracleEntry  s_kVendorOracles[] =
    {
        //  105 string directives and almost nothing else -- a purpose-built probe
        //  for the encoding area. It names no origin anywhere, so its load
        //  address comes entirely from the dialect default.
        { "LABELS",     "Merlin/LABELS.S",     "Merlin/LABELS",     2082, 988, {},                true },

        //  The largest source, and the broadest. Three sections at three
        //  addresses collapse into one contiguous object, which is only true
        //  because the origin directive relocates rather than seeks.
        { "MAKE DUMP",  "Merlin/MAKE DUMP.S",  "Merlin/MAKE DUMP",  6663, 593, {},                true },

        //  Its own save-object answer gates only the save directive, so the bytes
        //  are the same either way and the answer avoiding the out-of-subset
        //  construct is the one to give.
        { "KEYMAC",     "Merlin/KEYMAC.S",     "Merlin/KEYMAC",     5967, 678, s_kSaveObjectOff,  true },

        //  One source, two shipped objects, differing in exactly four bytes of
        //  365 -- so a conditional-assembly defect shows up as a small specific
        //  delta rather than a wall of noise.
        { "CLOCK.24",   "Merlin/CLOCK.S",      "Merlin/CLOCK.24",   6026, 369, s_kTwentyFourHour, true },
        { "CLOCK.12",   "Merlin/CLOCK.S",      "Merlin/CLOCK.12",   6026, 369, s_kTwelveHour,     true },

        //  Assembled at the configuration recovered from its own bytes.
        { "PRINTFILER", "Merlin/PRINTFILER.S", "Merlin/PRINTFILER", 4426, 290, s_kVendorBuild,    true },
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  VendorOracle
    //
    //  Assembling one entry, and the checks every sweep below shares.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class VendorOracle
    {
    public:

        //  The result AND the exact text it was produced from. Both, because the
        //  unmodified-source claim is about the input rather than the output, and
        //  checking a string the caller re-read separately would be checking a
        //  different string from the one that assembled.
        struct Assembly
        {
            std::string     source;
            AssemblyResult  result;
        };



        static Assembly Assemble (const VendorOracleEntry & entry, DialectId dialect)
        {
            FixtureProvider   provider;
            TestCpu           cpu;
            AssemblerOptions  options = {};
            Assembly          out;

            cpu.InitForTest();
            options.dialect = dialect;

            for (const VendorOracleAnswer & answer : entry.answers)
            {
                options.predefinedSymbols[answer.symbol] = answer.value;
            }

            AssertSucceeded (MerlinFixture::LoadSource (provider, entry.sourcePath, out.source));

            {
                Assembler  assembler (cpu.GetInstructionSet(), options);

                out.result = assembler.Assemble (out.source);
            }

            return out;
        }



        //  The entry with a given name. By name rather than by index, because an
        //  index into the table above is a second place the table's ORDER is
        //  recorded, and a row inserted anywhere silently repoints it at a
        //  different oracle.
        static const VendorOracleEntry & Named (const char * name)
        {
            const VendorOracleEntry *  found = nullptr;

            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                if (std::string (entry.name) == name)
                {
                    found = &entry;
                    break;
                }
            }

            Assert::IsNotNull (found, L"the named entry has left the vendor corpus");

            return *found;
        }



        static MerlinFixtureFile LoadObject (const VendorOracleEntry & entry)
        {
            FixtureProvider    provider;
            MerlinFixtureFile  object;

            AssertSucceeded (MerlinFixture::LoadObject (provider, entry.objectPath, object));

            return object;
        }



        //  The raw stored bytes, straight from the provider with no decoding at
        //  all. The unmodified-source check needs the file as committed rather
        //  than as any helper chose to present it.
        static std::vector<Byte> LoadRaw (const char * path)
        {
            FixtureProvider    provider;
            std::vector<Byte>  raw;

            AssertSucceeded (provider.OpenFixture (path, raw));

            return raw;
        }



        //  The first diagnostic, so a failure names what the assembler objected
        //  to instead of only that it objected.
        static std::wstring FirstDiagnostic (const char * name, const AssemblyResult & result)
        {
            std::string   text = std::string (name) + ": assembled clean";
            std::wstring  wide;

            if (!result.errors.empty())
            {
                text = std::string (name) + " line " + std::to_string (result.errors[0].lineNumber) + ": "
                     + result.errors[0].message + " (" + std::to_string (result.errors.size()) + " total)";
            }

            wide.assign (text.begin(), text.end());

            return wide;
        }



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
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

        //  The absent-corpus guard, and it comes FIRST because every sweep below
        //  is a loop: over an empty table each one reports success having
        //  compared nothing, and the output is indistinguishable from a full run.
        //
        //  The floor is stated as an equality rather than a minimum. Five sources
        //  and six objects is what the disk holds in absolute mode with both
        //  halves present -- a measured figure, not a target -- so a table that
        //  grew or shrank is a change to the evidence and should be seen.
        TEST_METHOD (TheVendorCorpusMeetsItsFloor)
        {
            std::vector<std::string>  sources;
            size_t                    distinct = 0;



            Assert::IsTrue (std::size (s_kVendorOracles) > 0, L"a sweep over an empty corpus compares nothing");

            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                if (std::find (sources.begin(), sources.end(), entry.sourcePath) == sources.end())
                {
                    sources.push_back (entry.sourcePath);
                }
            }

            distinct = sources.size();

            Assert::AreEqual ((size_t) 6, std::size (s_kVendorOracles),
                              L"six shipped objects on the Merlin Pro 2.23 disk have a source beside them");
            Assert::AreEqual ((size_t) 5, distinct,
                              L"from five sources -- CLOCK.S is the one that ships two objects");
        }



        //  The linker demo ships no object of its own, and the objects beside it
        //  on the disk are POST-LINK. Comparing against either would be comparing
        //  Casso's output with bytes no assembler produced, so the exclusion is
        //  asserted rather than merely observed -- it is one careless row away
        //  from being lost, and the entry would look entirely reasonable.
        TEST_METHOD (NoLinkerDemoSourceIsUsedAsAPositiveOracle)
        {
            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                std::string  path = entry.sourcePath;

                Assert::IsTrue (path.find ("PI.") == std::string::npos,
                                VendorOracle::Widen (path + " ships no object of its own").c_str());
            }
        }



        //  SC-001. Every entry, whole file through the real assembler, against the
        //  bytes the vendor shipped -- plus the load address, which is half the
        //  claim: a wrong origin yields byte-perfect output in the wrong place.
        TEST_METHOD (EveryVendorEntryAssemblesToItsShippedObjectByteForByte)
        {
            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                VendorOracle::Assembly  assembly   = VendorOracle::Assemble (entry, DialectId::Merlin);
                MerlinFixtureFile       object     = VendorOracle::LoadObject (entry);
                CorpusComparison        comparison = {};

                Assert::IsTrue (assembly.result.errors.empty(),
                                VendorOracle::FirstDiagnostic (entry.name, assembly.result).c_str());

                comparison = CorpusHarness::Compare (object.payload, assembly.result.bytes);

                Assert::IsTrue (comparison.verdict == CorpusVerdict::Match,
                                VendorOracle::Widen (CorpusHarness::Describe (entry.name, comparison)).c_str());

                Assert::AreEqual (static_cast<int> (object.loadAddress),
                                  static_cast<int> (assembly.result.startAddress),
                                  VendorOracle::Widen (std::string (entry.name) + " must load where it was shipped").c_str());
            }
        }



        //  Every entry carrying the flag must FAIL under AS65 as well as matching
        //  under Merlin. This closes the vacuity shape the flag exists for:
        //  labels, origin, literals and the evaluator are shared, so an entry
        //  built from those alone is green whether the Merlin profile works or is
        //  never consulted at all.
        //
        //  Today every vendor entry discriminates, which is asserted rather than
        //  left implicit -- an entry added later with the flag clear is a claim
        //  that it exercises only shared constructs, and that claim should have to
        //  be made deliberately.
        TEST_METHOD (EveryDiscriminatingEntryFailsUnderAs65)
        {
            size_t  discriminating = 0;

            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                if (entry.discriminates)
                {
                    VendorOracle::Assembly  assembly   = VendorOracle::Assemble (entry, DialectId::As65);
                    MerlinFixtureFile       object     = VendorOracle::LoadObject (entry);
                    CorpusComparison        comparison = CorpusHarness::Compare (object.payload, assembly.result.bytes);

                    discriminating++;

                    Assert::IsFalse (comparison.verdict == CorpusVerdict::Match,
                                     VendorOracle::Widen (std::string (entry.name)
                                         + " produced Merlin's bytes under AS65, so the profile was not consulted").c_str());
                }
            }

            Assert::AreEqual (std::size (s_kVendorOracles), discriminating,
                              L"every vendor entry is full of Merlin constructs, so every one must discriminate");
        }



        //  SC-002, and the assertion without which "unmodified" is only inspected.
        //  The corpus already proves the BYTES match; this proves the INPUT was
        //  not touched to get there.
        //
        //  The correspondence asserted is total and order-preserving: one
        //  character of assembled text per stored byte, in order, with only the
        //  documented decoding between them. So a tidied source fails whatever
        //  was tidied -- a re-indented line, a stripped trailing space, a deleted
        //  comment and an inserted origin each move or change a character, and a
        //  transcription would have to be byte-identical to the file to survive,
        //  at which point it is not a transcription.
        //
        //  Everything here is re-derived from the provider rather than taken from
        //  the decoder the sweep above uses. A decoder that normalized whitespace
        //  would otherwise satisfy this test with the same wrong text it handed
        //  the assembler.
        TEST_METHOD (EveryVendorEntryAssemblesTheFixtureBytesUnmodified)
        {
            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                VendorOracle::Assembly  assembly = VendorOracle::Assemble (entry, DialectId::Merlin);
                std::vector<Byte>       raw      = VendorOracle::LoadRaw (entry.sourcePath);

                AssertTextIsTheStoredBytes (entry, assembly.source, raw);
            }
        }



        //  And the fixtures themselves are the ones the extraction produced.
        //  The check above compares text against bytes and cannot see a file
        //  that was edited and re-saved consistently; the recorded sizes can.
        TEST_METHOD (EveryVendorFixtureIsTheSizeTheInventoryRecords)
        {
            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                std::vector<Byte>  source = VendorOracle::LoadRaw (entry.sourcePath);
                std::vector<Byte>  object = VendorOracle::LoadRaw (entry.objectPath);

                Assert::AreEqual (entry.sourceRawBytes, source.size(),
                                  VendorOracle::Widen (std::string (entry.sourcePath) + " is not the size it was extracted at").c_str());
                Assert::AreEqual (entry.objectRawBytes, object.size(),
                                  VendorOracle::Widen (std::string (entry.objectPath) + " is not the size it was extracted at").c_str());
            }
        }



        //  And the two answers must not produce the same bytes, or the CLOCK pair
        //  is two copies of one check. This is the assertion that makes CLOCK.S
        //  worth two entries rather than one.
        TEST_METHOD (TheTwoClockBuildsDifferFromEachOther)
        {
            VendorOracle::Assembly  twentyFour = VendorOracle::Assemble (VendorOracle::Named ("CLOCK.24"), DialectId::Merlin);
            VendorOracle::Assembly  twelve     = VendorOracle::Assemble (VendorOracle::Named ("CLOCK.12"), DialectId::Merlin);

            Assert::IsFalse (twentyFour.result.bytes == twelve.result.bytes,
                             L"the version answer must reach the object, or both entries prove the same thing");
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
            constexpr int      kCombinations = 4;
            MerlinFixtureFile  object;
            FixtureProvider    provider;
            int                matches       = 0;
            int                matched       = -1;

            AssertSucceeded (MerlinFixture::LoadObject (provider, "Merlin/PRINTFILER", object));

            for (int combination = 0; combination < kCombinations; combination++)
            {
                VendorOracleAnswer      answers[] = { { "FORMAT",  combination & 1 },
                                                      { "MONITOR", (combination >> 1) & 1 } };
                VendorOracleEntry       trial     = VendorOracle::Named ("PRINTFILER");

                trial.answers = std::span<const VendorOracleAnswer> (answers, std::size (answers));

                VendorOracle::Assembly  assembly  = VendorOracle::Assemble (trial, DialectId::Merlin);

                if (assembly.result.errors.empty() && assembly.result.bytes == object.payload)
                {
                    matches++;
                    matched = combination;
                }
            }

            Assert::AreEqual (1, matches, L"exactly one of the four answer pairs must reproduce the shipped object");
            Assert::AreEqual (1, matched, L"and it is the pair with formatting on and monitoring off");
        }

    private:

        //  The stored file as text, one character per byte: the four-byte header
        //  removed, the declared length honored, bit 7 masked, and Merlin's CR
        //  line terminators translated. Written out here rather than called,
        //  because this is the independent half of the comparison.
        static void AssertTextIsTheStoredBytes (const VendorOracleEntry & entry,
                                                const std::string       & assembled,
                                                const std::vector<Byte> & raw)
        {
            constexpr size_t  kHeaderBytes    = 4;
            constexpr size_t  kLengthOffset   = 2;
            constexpr Byte    kLowSevenBits   = 0x7F;
            constexpr Byte    kCarriageReturn = 0x0D;
            size_t            declared        = 0;

            Assert::IsTrue (raw.size() > kHeaderBytes,
                            VendorOracle::Widen (std::string (entry.sourcePath) + " is too short to hold a header").c_str());

            declared = (size_t) raw[kLengthOffset] | ((size_t) raw[kLengthOffset + 1] << 8);

            Assert::AreEqual (raw.size() - kHeaderBytes, declared,
                              VendorOracle::Widen (std::string (entry.sourcePath)
                                  + " declares a length its payload does not match").c_str());

            Assert::AreEqual (declared, assembled.size(),
                              VendorOracle::Widen (std::string (entry.name)
                                  + ": the assembled text is not one character per stored byte, so a line was added,"
                                    " removed or reflowed").c_str());

            for (size_t i = 0; i < declared; i++)
            {
                Byte  masked   = raw[kHeaderBytes + i] & kLowSevenBits;
                char  expected = (masked == kCarriageReturn) ? '\n' : (char) masked;

                if (assembled[i] != expected)
                {
                    Assert::Fail (VendorOracle::Widen (std::string (entry.name) + ": the assembled text differs from the"
                                      " stored bytes at offset " + std::to_string (i)
                                      + " -- the source was edited, not assembled as committed").c_str());
                }
            }
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  VendorLibrary
    //
    //  One committed macro library, and the name a source asks for it by.
    //
    //  These are the only type-T files on the disk, and they are NOT standalone
    //  sources -- measured, not assumed. `T.SENDMSG` is a macro BODY: its first
    //  line is an instruction and its second writes to a positional parameter, so
    //  assembling it on its own produces seven expression errors and means
    //  nothing. It only has a reading inside the file that includes it.
    //
    //  So they enter the sweep the way the vendor used them: served to the
    //  assembler under the name the disk stores, for a real vendor source to ask
    //  for. Merlin prepends `T.` when resolving a request, which is why the
    //  operand and the stored name differ.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct VendorLibrary
    {
        const char *  fixturePath;
        const char *  requestedAs;
    };



    static constexpr VendorLibrary  s_kVendorLibraries[] =
    {
        { "Merlin/T.PI.MACS", "T.PI.MACS" },   // equates and five macros, asked for as `USE PI.MACS`
        { "Merlin/T.SENDMSG", "T.SENDMSG" },   // a macro body, asked for as `PUT SENDMSG`
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  VendorSourceEntry
    //
    //  One committed vendor source that Casso can be pointed AT: the five that
    //  ship an object and the two linker-demo specimens that ship none.
    //
    //  Broader than the oracle table on purpose. "Valid Merlin source is rejected
    //  only where the boundary table says so" is a claim about every vendor file
    //  Casso can be pointed at, and a list restricted to the ones that already
    //  assemble cleanly would be a sweep over the files chosen for producing no
    //  rejections. The two specimens that DO reject are what makes the mapping
    //  loop run at all.
    //
    //  `expectedRefusals` is stated per file rather than derived, so a file that
    //  stops reaching the boundary fails here instead of quietly reducing this to
    //  a sweep over nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct VendorSourceEntry
    {
        const char                          *  path;
        std::span<const VendorOracleAnswer>    answers;
        size_t                                 expectedRefusals;

        //  Diagnostics this file draws that are NOT boundary refusals. Every one
        //  of these is an SC-003 defect by definition, so the number is stated
        //  exactly rather than tolerated: a new one fails the sweep, and fixing
        //  the known gap fails it too and forces the count down deliberately.
        size_t                                 unexplainedRejections;
    };



    //  Every vendor file committed under UnitTest/Fixtures/Merlin/ that is a
    //  source in its own right. The objects are not here -- they are compared
    //  against, not assembled -- and neither are the macro libraries, which are
    //  reached through the two sources that include them.
    static constexpr VendorSourceEntry  s_kCommittedVendorSources[] =
    {
        { "Merlin/LABELS.S",     {},                0, 0 },
        { "Merlin/MAKE DUMP.S",  {},                0, 0 },
        { "Merlin/KEYMAC.S",     s_kSaveObjectOff,  0, 0 },
        { "Merlin/CLOCK.S",      s_kTwentyFourHour, 0, 0 },
        { "Merlin/PRINTFILER.S", s_kVendorBuild,    0, 0 },

        //  The linker demo. Both sides of the boundary: one exports only, one
        //  also imports, and the counts are what the two shapes cost.
        //
        //  PI.ADD.S carries the ONE gap in the corpus, and it is one directive
        //  rather than nine problems. Line 123 is `VAR MSGPNT;OUTPUT`, which
        //  binds the positional parameters for the fragment the next line pulls
        //  in with `PUT SENDMSG` -- Merlin's way of parameterizing an included
        //  body without a macro call. `VAR` is not in Casso's directive table at
        //  all, so the line itself draws a diagnostic and the eight `]1`/`]2`
        //  references in the included fragment have nothing to resolve to. That
        //  is nine rejections of valid Merlin source with no boundary row behind
        //  them, which is exactly what SC-003 calls a defect.
        { "Merlin/PI.ADD.S",     s_kSaveObjectOff,  7, 9 },
        { "Merlin/PI.START.S",   s_kSaveObjectOff,  5, 0 },
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinVendorRejectionTests
    //
    //  SC-003, swept rather than listed: a rejection with no boundary row is a
    //  defect rather than a limitation, so the assertion is over the rejections
    //  that HAPPEN and not over a fixed set somebody expected.
    //
    //  A refusal is attributed to a row by recomposing the row's own sentence and
    //  requiring EQUALITY. Not a substring -- this feature has been caught by a
    //  bare-substring assertion once already, and a spelling is three characters
    //  that match inside ordinary words. Equality also means a row whose wording
    //  changed cannot keep passing against a message built some other way.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinVendorRejectionTests)
    {
    public:

        //  The corpus this sweeps, counted before it is walked, and related to the
        //  oracle table so the two lists cannot drift apart: every source with an
        //  object must also be a source this sweep visits.
        TEST_METHOD (TheCommittedSourceListCoversEveryOracleSource)
        {
            Assert::AreEqual ((size_t) 7, std::size (s_kCommittedVendorSources),
                              L"five oracle sources and the two linker-demo specimens");
            Assert::AreEqual ((size_t) 2, std::size (s_kVendorLibraries),
                              L"and both committed macro libraries, which are included rather than assembled");

            for (const VendorOracleEntry & oracle : s_kVendorOracles)
            {
                bool  covered = false;

                for (const VendorSourceEntry & source : s_kCommittedVendorSources)
                {
                    if (std::string (source.path) == oracle.sourcePath)
                    {
                        covered = true;
                        break;
                    }
                }

                Assert::IsTrue (covered, VendorOracle::Widen (std::string (oracle.sourcePath)
                                    + " ships an object but is not swept for rejections").c_str());
            }
        }



        //  SC-003. Every boundary refusal must be one the table composed, and
        //  every OTHER diagnostic is a defect -- counted exactly, never tolerated.
        TEST_METHOD (EveryRejectionOfAVendorSourceMapsToABoundaryRow)
        {
            size_t  attributed  = 0;
            size_t  unexplained = 0;

            for (const VendorSourceEntry & entry : s_kCommittedVendorSources)
            {
                MockFileReader  reader;
                AssemblyResult  result  = Assemble (entry, reader);
                size_t          refused = 0;
                size_t          other   = 0;

                for (const AssemblyError & error : result.errors)
                {
                    if (error.kind == DiagnosticKind::SubsetBoundary)
                    {
                        AssertComposedByExactlyOneRow (entry, error);
                        refused++;
                    }
                    else
                    {
                        other++;
                    }
                }

                Assert::AreEqual (entry.expectedRefusals, refused, Describe (entry, result).c_str());
                Assert::AreEqual (entry.unexplainedRejections, other, Describe (entry, result).c_str());

                attributed  += refused;
                unexplained += other;
            }

            //  Without this the loop above is satisfied by an assembler that
            //  refuses nothing at all, which is the shape a sweep over rejections
            //  fails in.
            Assert::IsTrue (attributed > 0, L"no vendor source reached the boundary, so nothing was attributed");

            //  The corpus-wide count of rejections SC-003 does not account for.
            //  Stated once so it can only change deliberately, in either
            //  direction: a tenth appearing is a new defect, and it dropping to
            //  zero means the parameter-binding directive landed.
            Assert::AreEqual ((size_t) 9, unexplained,
                              L"the only rejections without a boundary row are PI.ADD.S's parameter-binding directive"
                              L" and the eight references in the fragment it parameterizes");
        }



        //  The linker demo is a NEGATIVE specimen and nothing else. Its objects on
        //  the disk are post-link, so a positive comparison would be against bytes
        //  no assembler produced -- and the two files must reach the boundary, or
        //  the sweep above has nothing to attribute.
        TEST_METHOD (TheLinkerDemoSourcesAreTheOnlyOnesThatReject)
        {
            for (const VendorSourceEntry & entry : s_kCommittedVendorSources)
            {
                std::string  path      = entry.path;
                bool         isDemo    = path.find ("PI.") != std::string::npos;
                bool         rejects   = entry.expectedRefusals > 0;

                Assert::AreEqual (isDemo, rejects, VendorOracle::Widen (path
                                      + ": only the linker demo may be expected to reject").c_str());
            }
        }



        //  Both committed macro libraries are REACHED, by the vendor sources that
        //  ask for them and under the names the disk stores. Serving a file the
        //  assembler never requests would make the sweep above look like it
        //  covered inclusion when it covered a table entry.
        //
        //  It also pins the resolution rule from real vendor lines: the sources
        //  write `USE PI.MACS` and `PUT SENDMSG`, and the disk stores `T.PI.MACS`
        //  and `T.SENDMSG`.
        TEST_METHOD (BothMacroLibrariesAreRequestedByAVendorSource)
        {
            for (const VendorLibrary & library : s_kVendorLibraries)
            {
                int  requests = 0;

                for (const VendorSourceEntry & entry : s_kCommittedVendorSources)
                {
                    MockFileReader  reader;
                    AssemblyResult  result = Assemble (entry, reader);

                    Assert::AreEqual (entry.expectedRefusals + entry.unexplainedRejections, result.errors.size(),
                                      Describe (entry, result).c_str());

                    requests += reader.CountRequests (library.requestedAs);
                }

                Assert::IsTrue (requests > 0, VendorOracle::Widen (std::string (library.requestedAs)
                                    + " is committed as an inclusion specimen and no vendor source asks for it").c_str());
            }
        }



        //  And a request that missed must be visible as one. Without this the
        //  sweep passes while every include silently fails, because the two
        //  linker-demo sources are refused at the boundary either way.
        TEST_METHOD (TheServedLibrariesAreTheOnesTheSourcesAskFor)
        {
            for (const VendorSourceEntry & entry : s_kCommittedVendorSources)
            {
                MockFileReader  reader;
                AssemblyResult  result = Assemble (entry, reader);

                for (const std::string & requested : reader.requests)
                {
                    bool  served = reader.files.find (requested) != reader.files.end();

                    Assert::IsTrue (served, VendorOracle::Widen (std::string (entry.path)
                                        + " asked for \"" + requested + "\", which is not a committed fixture -- "
                                        + std::to_string (result.errors.size()) + " diagnostics followed").c_str());
                }
            }
        }

    private:

        //  One vendor source with both macro libraries available to it. They are
        //  offered rather than pushed: a source that includes neither simply never
        //  asks, and the request log is what says which happened.
        static AssemblyResult Assemble (const VendorSourceEntry & entry, MockFileReader & reader)
        {
            FixtureProvider   provider;
            TestCpu           cpu;
            std::string       source;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect    = DialectId::Merlin;
            options.fileReader = &reader;

            for (const VendorOracleAnswer & answer : entry.answers)
            {
                options.predefinedSymbols[answer.symbol] = answer.value;
            }

            for (const VendorLibrary & library : s_kVendorLibraries)
            {
                std::string  text;

                AssertSucceeded (MerlinFixture::LoadTextSource (provider, library.fixturePath, text));

                reader.files[library.requestedAs] = text;
            }

            AssertSucceeded (MerlinFixture::LoadSource (provider, entry.path, source));

            {
                Assembler  assembler (cpu.GetInstructionSet(), options);

                return assembler.Assemble (source);
            }
        }



        //  A refusal belongs to exactly one row. Both linkage wordings are
        //  admitted because which one a module gets is a property of the whole
        //  file, and requiring one of them would be asserting the linkage rather
        //  than the attribution -- that is RelocatableWorkaroundTests' claim, made
        //  against counts, and duplicating it loosely here would weaken both.
        static void AssertComposedByExactlyOneRow (const VendorSourceEntry & entry, const AssemblyError & error)
        {
            const DialectProfile  &  merlin  = DialectRegistry::Get (DialectId::Merlin);
            size_t                   matches = 0;



            for (const SubsetBoundaryRow & row : MerlinSubsetBoundary::GetAll())
            {
                std::string  selfContained = SubsetBoundary::ComposeRefusal (row, ModuleLinkage::SelfContained,
                                                                             merlin.GetName());
                std::string  dependent     = SubsetBoundary::ComposeRefusal (row, ModuleLinkage::DependsOnOther,
                                                                             merlin.GetName());

                if (error.message == selfContained || error.message == dependent)
                {
                    matches++;
                }
            }

            Assert::AreEqual ((size_t) 1, matches, VendorOracle::Widen (std::string (entry.path)
                                  + " line " + std::to_string (error.lineNumber)
                                  + ": a rejection with no boundary row behind it is a defect, not a limitation -- "
                                  + error.message).c_str());
        }



        static std::wstring Describe (const VendorSourceEntry & entry, const AssemblyResult & result)
        {
            std::string  text = std::string (entry.path) + ": expected " + std::to_string (entry.expectedRefusals)
                              + " refusals and " + std::to_string (entry.unexplainedRejections)
                              + " other diagnostics, got " + std::to_string (result.errors.size()) + " in all";

            for (const AssemblyError & error : result.errors)
            {
                text += " | " + std::to_string (error.lineNumber) + ": " + error.message.substr (0, 60);
            }

            return VendorOracle::Widen (text);
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



    //  The macro call punctuated for the other dialect, named once and used
    //  twice: the sweep's entry reads its diagnostic, and a separate test reads
    //  the bytes it must not produce. Two literals would let those drift into
    //  testing different sources, which is the shape where one of them quietly
    //  stops covering anything.
    //
    //  The body assembles CLEANLY under the empty substitution, and that is the
    //  whole design. `DFB $10,$20` is two data bytes and a bare shift is the
    //  accumulator form, so an assembler expanding this anyway emits three bytes
    //  of a different program and says nothing. A body that merely failed one
    //  step later would leave the byte assertion satisfied either way -- and
    //  that was measured rather than reasoned: the first version used
    //  `LDA ]1 / STA ]2`, and the mutation that reported the mismatch and then
    //  expanded regardless went uncaught by it.
    static constexpr const char *  s_kpszMisPunctuatedCall = "STORE     MAC\n"
                                                             "          DFB ]1\n"
                                                             "          LSR ]2\n"
                                                             "          <<<\n"
                                                             "          STORE $10,$20\n";



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
            //
            "macro invoked with as65 argument syntax",
            s_kpszMisPunctuatedCall,
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
        //
        //  The body assembles cleanly under the empty substitution, which is
        //  what makes this discriminating rather than decorative -- see the note
        //  on the entry itself. Without the refusal this source produces three
        //  bytes and no complaint.
        TEST_METHOD (TheMisPunctuatedMacroCallEmitsNothing)
        {
            AssemblyResult  result = AssembleAsMerlin (s_kpszMisPunctuatedCall);

            Assert::IsTrue (result.bytes.empty(), L"a refused invocation must emit no part of the body");
        }



        //  The control, and it is not optional. Without it the rejection above is
        //  evidence of nothing -- a macro mechanism too broken to expand anything
        //  would pass it. The same call with Merlin's own separator assembles the
        //  whole body.
        //
        //  The BYTES are asserted rather than their count. The partial expansion
        //  happens to produce three bytes too -- two from a comma-separated data
        //  directive and one from a bare shift -- so a count would be satisfied
        //  by the very reading this test exists to distinguish from.
        TEST_METHOD (TheSameCallWithMerlinsSeparatorAssembles)
        {
            AssemblyResult  result   = AssembleAsMerlin ("STORE     MAC\n"
                                                         "          DFB ]1\n"
                                                         "          LSR ]2\n"
                                                         "          <<<\n"
                                                         "          STORE $10;$20\n");
            std::vector<Byte>  expected = { 0x10, 0x46, 0x20 };

            Assert::IsTrue (result.errors.empty(), FirstError (result).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"one data byte, then a zero-page shift of the second argument");
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
