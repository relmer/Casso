#include "Pch.h"

#include "MerlinCorpus/CorpusHarness.h"
#include "MerlinCorpus/MerlinFixture.h"
#include "EmuTests/FixtureProvider.h"
#include "EhmTestHelper.h"
#include "TestHelpers.h"
#include "MockFileReader.h"
#include "Assembler.h"
#include "CommandLineParser.h"
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
    //  CorpusText
    //
    //  Widening, for failure messages that name the file they are about. Carried
    //  here rather than inside one test class because two of them need it, and
    //  the second copy is where two versions of the same helper start.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CorpusText
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }
    };



    //  Every source committed under UnitTest/Fixtures/Merlin/, in one list so the
    //  form sweep below can cover all of them rather than the ones a test author
    //  happened to remember. The oracle table holds five of these, the rejection
    //  table two, and the library table three -- no existing table sees the whole
    //  set, and the property being swept belongs to the whole set.
    static constexpr const char *  s_kCommittedSources[] =
    {
        "Merlin/LABELS.S",
        "Merlin/KEYMAC.S",
        "Merlin/PRINTFILER.S",
        "Merlin/MAKE DUMP.S",
        "Merlin/CLOCK.S",
        "Merlin/PI.START.S",
        "Merlin/PI.ADD.S",
        "Merlin/T.PI.MACS",
        "Merlin/T.SENDMSG",
        "Merlin/T.MACRO LIBRARY",
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinFixtureTests
    //
    //  The read step, pinned against the committed vendor files.
    //
    //  This sits upstream of every corpus entry: entries compare bytes an object
    //  file supplied, so a reader that strips the wrong number of header bytes
    //  yields expectations that are wrong uniformly -- and a corpus wrong in the
    //  same direction everywhere still looks entirely consistent.
    //
    //  It also guards the FORM of the sources. They are committed as ordinary
    //  Windows text rather than as the Apple II text the disk holds, which is
    //  what lets the same file feed both this project and `CassoCli merlin`;
    //  a fixture that quietly reverted to disk form would assemble here and
    //  nowhere else.
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



        //  Spaces appeared BOTH ways in stored source: $A0 separating fields,
        //  $20 inside comment text. LABELS.S held 214 of the first and 81 of the
        //  second, and across all ten sources spaces were the only bytes below
        //  $80 -- so masking bit 7 changed no character's identity, and both
        //  forms are now one ordinary $20.
        //
        //  The distinction is gone on purpose, not by accident. The parser was
        //  never allowed to tell the two apart (see MerlinFixture.h), because
        //  source reaching Casso by any other route carries no such marking;
        //  transcoding removes the temptation along with the encoding.
        TEST_METHOD (LoadSource_ReadsBothStoredSpaceFormsAsOneOrdinarySpace)
        {
            FixtureProvider  provider;
            std::string      text;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/LABELS.S", text));

            //  "END BRK ;table end" -- $A0 separators before and after BRK, then
            //  $20 spaces inside the comment. If either form had survived the
            //  transcoding as itself, this substring could not match.
            Assert::IsTrue (text.find ("END BRK ;table end") != std::string::npos,
                            L"field-separating $A0 and comment-text $20 must both read as ' '");
        }



        //  The guard on the FORM of every committed source, which is what the
        //  transcoding established and what a later extraction could quietly
        //  undo. A file re-extracted verbatim arrives as high bytes and a
        //  four-byte header; one re-saved through an Apple II editor arrives as
        //  bare CRs. Either fails here by name, instead of as a wall of parse
        //  errors somewhere downstream that reads like a dialect defect.
        //
        //  The CRLF assertion also pins the .gitattributes rule that produces
        //  it: without `eol=crlf` these files check out with whatever the
        //  developer's core.autocrlf says, and "Windows text file" stops being
        //  a property of the repository.
        //
        //  The count is asserted first, so a source committed later joins this
        //  sweep deliberately rather than escaping it silently.
        TEST_METHOD (EverySourceIsCommittedAsPlainWindowsText)
        {
            FixtureProvider  provider;

            Assert::AreEqual (static_cast<size_t> (10), std::size (s_kCommittedSources),
                              L"ten sources are committed; a new one belongs in this sweep");

            for (const char * path : s_kCommittedSources)
            {
                std::string  text;
                size_t       highBits = 0;
                size_t       controls = 0;
                size_t       loneCr   = 0;

                AssertSucceeded (MerlinFixture::LoadSource (provider, path, text));

                for (size_t i = 0; i < text.size(); i++)
                {
                    Byte  value = static_cast<Byte> (text[i]);

                    if ((value & 0x80) != 0)
                    {
                        highBits++;
                    }
                    else if (value == '\r')
                    {
                        if (i + 1 >= text.size() || text[i + 1] != '\n')
                        {
                            loneCr++;
                        }
                    }
                    else if (value < 0x20 && value != '\n')
                    {
                        //  A surviving BIN header shows up here: $01 and $09 are
                        //  the first two bytes of every source file on the disk.
                        controls++;
                    }
                }

                Assert::AreEqual (static_cast<size_t> (0), highBits,
                                  CorpusText::Widen (std::string (path) + " keeps a high bit, so it was not transcoded").c_str());
                Assert::AreEqual (static_cast<size_t> (0), loneCr,
                                  CorpusText::Widen (std::string (path) + " holds a bare CR, so its line endings are Apple II's").c_str());
                Assert::AreEqual (static_cast<size_t> (0), controls,
                                  CorpusText::Widen (std::string (path) + " holds a control byte, most likely a BIN header").c_str());
                Assert::IsTrue (text.find ("\r\n") != std::string::npos,
                                CorpusText::Widen (std::string (path) + " has no CRLF, so it did not check out as Windows text").c_str());
            }
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



        //  A macro library begins at its first character. These files carried no
        //  header even on the disk -- DOS 3.3 gives a type-T file none -- and the
        //  transcoding removed the one the sources had, so both halves of the
        //  corpus now start where their text starts.
        TEST_METHOD (LoadSource_ReadsAMacroLibraryFromItsFirstCharacter)
        {
            FixtureProvider  provider;
            std::string      text;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/T.SENDMSG", text));

            Assert::IsTrue (text.rfind ("SENDMSG", 0) == 0,
                            L"the file must start at its first byte, with nothing skipped");
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
        { "LABELS",     "Merlin/LABELS.S",     "Merlin/LABELS",     2195, 988, {},                true },

        //  The largest source, and the broadest. Three sections at three
        //  addresses collapse into one contiguous object, which is only true
        //  because the origin directive relocates rather than seeks.
        { "MAKE DUMP",  "Merlin/MAKE DUMP.S",  "Merlin/MAKE DUMP",  7034, 593, {},                true },

        //  Its own save-object answer gates only the save directive, so the bytes
        //  are the same either way and the answer avoiding the out-of-subset
        //  construct is the one to give.
        { "KEYMAC",     "Merlin/KEYMAC.S",     "Merlin/KEYMAC",     6270, 678, s_kSaveObjectOff,  true },

        //  One source, two shipped objects, differing in exactly four bytes of
        //  365 -- so a conditional-assembly defect shows up as a small specific
        //  delta rather than a wall of noise.
        { "CLOCK.24",   "Merlin/CLOCK.S",      "Merlin/CLOCK.24",   6298, 369, s_kTwentyFourHour, true },
        { "CLOCK.12",   "Merlin/CLOCK.S",      "Merlin/CLOCK.12",   6298, 369, s_kTwelveHour,     true },

        //  Assembled at the configuration recovered from its own bytes.
        { "PRINTFILER", "Merlin/PRINTFILER.S", "Merlin/PRINTFILER", 4637, 290, s_kVendorBuild,    true },
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
                                CorpusText::Widen (path + " ships no object of its own").c_str());
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
                                CorpusText::Widen (CorpusHarness::Describe (entry.name, comparison)).c_str());

                Assert::AreEqual (static_cast<int> (object.loadAddress),
                                  static_cast<int> (assembly.result.startAddress),
                                  CorpusText::Widen (std::string (entry.name) + " must load where it was shipped").c_str());
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
                                     CorpusText::Widen (std::string (entry.name)
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
        //  the reader the sweep above uses. A reader that normalized whitespace
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
                                  CorpusText::Widen (std::string (entry.sourcePath) + " is not the size it was extracted at").c_str());
                Assert::AreEqual (entry.objectRawBytes, object.size(),
                                  CorpusText::Widen (std::string (entry.objectPath) + " is not the size it was extracted at").c_str());
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

        //  The text handed to the assembler, against the file as it sits in the
        //  repository.
        //
        //  Sources are committed as text, so this is an equality rather than the
        //  independent decode it used to be. It is still worth making: it is what
        //  catches a reader that trims, reflows or re-encodes on the way through,
        //  which would hand the assembler text nobody committed while every byte
        //  comparison downstream carried on passing.
        static void AssertTextIsTheStoredBytes (const VendorOracleEntry & entry,
                                                const std::string       & assembled,
                                                const std::vector<Byte> & raw)
        {
            Assert::AreEqual (raw.size(), assembled.size(),
                              CorpusText::Widen (std::string (entry.name)
                                  + ": the assembled text is not one character per stored byte, so a line was added,"
                                    " removed or reflowed").c_str());

            for (size_t i = 0; i < raw.size(); i++)
            {
                if (static_cast<Byte> (assembled[i]) != raw[i])
                {
                    Assert::Fail (CorpusText::Widen (std::string (entry.name) + ": the assembled text differs from the"
                                      " stored bytes at offset " + std::to_string (i)
                                      + " -- the source was edited, not assembled as committed").c_str());
                }
            }
        }

    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinInvocationOracleTests
    //
    //  The same six objects, reproduced from an ARGV.
    //
    //  The sweep above hands the assembler an answer map built in C++. That proves
    //  the assembler, and proves nothing about whether an operator can get an
    //  answer to it: three of these five sources ask questions, and until the
    //  grammar carried an answer they were unreachable from a command line while
    //  the tool printed advice on how to answer them.
    //
    //  So every entry here is spelled as words, parsed by the real grammar, and the
    //  answers the assembler receives are the ones the parse produced. Nothing is
    //  read back from the entry except the file to assemble and the object to
    //  compare against.
    //
    //  What this cannot cover is the executable's own wiring, which is not linked
    //  into this project. That link is checked by running the tool.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinInvocationOracleTests)
    {
    public:

        //  Every entry, driven from words. A flag that is accepted and discarded
        //  fails four of these six with the very diagnostic that named it.
        TEST_METHOD (EveryVendorEntryReproducesItsShippedObjectFromAnArgv)
        {
            size_t  answered = 0;

            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                CommandLineOptions  options    = ParseInvocation (entry.sourcePath, entry.answers);
                AssemblyResult      result     = AssembleAsInvoked (entry.sourcePath, options);
                MerlinFixtureFile   object     = VendorOracle::LoadObject (entry);
                CorpusComparison    comparison = {};

                if (!entry.answers.empty())
                {
                    answered++;
                }

                Assert::IsTrue (options.dialect == DialectId::Merlin,
                                L"the invocation must have selected Merlin, or this sweep tests the default");

                Assert::IsTrue (result.errors.empty(),
                                VendorOracle::FirstDiagnostic (entry.name, result).c_str());

                comparison = CorpusHarness::Compare (object.payload, result.bytes);

                Assert::IsTrue (comparison.verdict == CorpusVerdict::Match,
                                CorpusText::Widen (CorpusHarness::Describe (entry.name, comparison)).c_str());

                Assert::AreEqual (static_cast<int> (object.loadAddress),
                                  static_cast<int> (result.startAddress),
                                  CorpusText::Widen (std::string (entry.name)
                                      + " must load where it was shipped").c_str());
            }

            Assert::AreEqual ((size_t) 4, answered,
                              L"four of the six objects need an answer typed, and they are the reason the flag exists");
        }



        //  And the answers are LOAD-BEARING. Without this, the sweep above is
        //  equally green against a grammar that drops every answer on the floor
        //  for any source that turns out not to need one -- so each answering
        //  entry is run again with the answers withheld and required to refuse.
        //
        //  Each refusal is required to NAME the question it is about. "The
        //  assembly failed" is satisfied by any failure whatever, including one
        //  arising downstream from an answer that was never asked for, and a
        //  vendor source this size fails for a dozen consequential reasons once
        //  its first symbol is undefined.
        TEST_METHOD (EveryAnsweringEntryRefusesByNameWhenNoAnswerIsTyped)
        {
            size_t  refused   = 0;
            size_t  questions = 0;

            for (const VendorOracleEntry & entry : s_kVendorOracles)
            {
                if (!entry.answers.empty())
                {
                    CommandLineOptions  options = ParseInvocation (entry.sourcePath, {});
                    AssemblyResult      result  = AssembleAsInvoked (entry.sourcePath, options);

                    refused++;

                    Assert::IsTrue (options.predefinedSymbols.empty(),
                                    L"no answer was typed, so none may arrive");

                    for (const VendorOracleAnswer & answer : entry.answers)
                    {
                        questions++;

                        Assert::IsTrue (RefusalNames (result, answer.symbol),
                                        CorpusText::Widen (std::string (entry.name) + " never refused "
                                            + answer.symbol + ", so that answer reaches no byte").c_str());
                    }
                }
            }

            Assert::AreEqual ((size_t) 4, refused,   L"four entries carry answers");
            Assert::AreEqual ((size_t) 7, questions,
                              L"seven questions between them -- CLOCK asks its two once per shipped object");
        }



        //  The headline claim, stated as one assertion: ONE source, TWO command
        //  lines, the two different objects the vendor shipped. Neither half of a
        //  matching pair can be produced by ignoring what was typed.
        TEST_METHOD (OneSourceProducesBothShippedClockObjectsFromTwoCommandLines)
        {
            const VendorOracleEntry &  twentyFour = VendorOracle::Named ("CLOCK.24");
            const VendorOracleEntry &  twelve     = VendorOracle::Named ("CLOCK.12");
            AssemblyResult             builtLate  = AssembleAsInvoked (twentyFour.sourcePath,
                                                        ParseInvocation (twentyFour.sourcePath, twentyFour.answers));
            AssemblyResult             builtEarly = AssembleAsInvoked (twelve.sourcePath,
                                                        ParseInvocation (twelve.sourcePath, twelve.answers));

            Assert::AreEqual (std::string (twentyFour.sourcePath), std::string (twelve.sourcePath),
                              L"the pair is only interesting because it is one file");

            Assert::IsFalse (builtLate.bytes == builtEarly.bytes,
                             L"two command lines produced one object, so the answer never reached the bytes");

            AssertMatchesShippedObject (twentyFour, builtLate);
            AssertMatchesShippedObject (twelve,     builtEarly);
        }

    private:

        //  Owns the storage behind a synthetic argv. The parser takes `char *[]`
        //  the way main does, so the strings must be mutable and outlive the call.
        class ArgVector
        {
        public:
            explicit ArgVector (const std::vector<std::string> & words)
                : m_storage (words)
            {
                for (std::string & word : m_storage)
                {
                    m_pointers.push_back (word.data());
                }
            }

            int      Count() const { return (int) m_pointers.size(); }
            char * * Data()        { return m_pointers.data(); }

        private:
            std::vector<std::string>  m_storage;
            std::vector<char *>       m_pointers;
        };



        //  The invocation an operator would type, parsed by the real grammar.
        static CommandLineOptions ParseInvocation (const char                          *  sourcePath,
                                                   std::span<const VendorOracleAnswer>    answers)
        {
            std::vector<std::string>  words = { "CassoCli", "merlin", sourcePath };

            for (const VendorOracleAnswer & answer : answers)
            {
                words.push_back ("-d");
                words.push_back (std::string (answer.symbol) + "=" + std::to_string (answer.value));
            }

            ArgVector  argv (words);

            return CommandLineParser::Parse (argv.Count(), argv.Data(),
                                             [] (const std::string &) { return false; });
        }



        //  Assembled from what the PARSE decided, so an answer the grammar dropped
        //  is an answer the assembler never sees.
        static AssemblyResult AssembleAsInvoked (const char * sourcePath, const CommandLineOptions & options)
        {
            FixtureProvider   provider;
            TestCpu           cpu;
            AssemblerOptions  asmOptions = {};
            std::string       source;

            cpu.InitForTest();

            asmOptions.dialect           = options.dialect;
            asmOptions.dialectSelection  = options.dialectSelection;
            asmOptions.outputFileName    = options.outputFile;
            asmOptions.predefinedSymbols = options.predefinedSymbols;

            AssertSucceeded (MerlinFixture::LoadSource (provider, sourcePath, source));

            Assembler  assembler (cpu.GetInstructionSet(), asmOptions);

            return assembler.Assemble (source);
        }



        //  Whether some diagnostic refused the named question, rather than
        //  merely whether the assembly failed.
        static bool RefusalNames (const AssemblyResult & result, const char * symbol)
        {
            std::string  wanted = std::string ("No answer supplied for ") + symbol;
            bool         found  = false;

            for (const AssemblyError & error : result.errors)
            {
                if (error.message.find (wanted) != std::string::npos)
                {
                    found = true;
                    break;
                }
            }

            return found;
        }



        static void AssertMatchesShippedObject (const VendorOracleEntry & entry, const AssemblyResult & result)
        {
            MerlinFixtureFile  object     = VendorOracle::LoadObject (entry);
            CorpusComparison   comparison = CorpusHarness::Compare (object.payload, result.bytes);

            Assert::IsTrue (result.errors.empty(),
                            VendorOracle::FirstDiagnostic (entry.name, result).c_str());

            Assert::IsTrue (comparison.verdict == CorpusVerdict::Match,
                            CorpusText::Widen (CorpusHarness::Describe (entry.name, comparison)).c_str());
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
        //  PI.ADD.S carried the ONE gap in the corpus, and it was one directive
        //  rather than nine problems. Line 123 is `VAR MSGPNT;OUTPUT`, which
        //  binds the positional parameters for the fragment the next line pulls
        //  in with `PUT SENDMSG` -- Merlin's way of parameterizing an included
        //  body without a macro call. The directive was absent from the table
        //  entirely, so the line itself drew a diagnostic and the eight `]1`/`]2`
        //  references in the included fragment had nothing to resolve to: nine
        //  rejections of valid Merlin source with no boundary row behind them,
        //  which is exactly what SC-003 calls a defect. The directive is now
        //  implemented and the count is zero -- reached by fixing the cause, and
        //  the assertion below is what forced it to be reached deliberately.
        { "Merlin/PI.ADD.S",     s_kSaveObjectOff,  7, 0 },
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

                Assert::IsTrue (covered, CorpusText::Widen (std::string (oracle.sourcePath)
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

            //  The corpus-wide count of rejections SC-003 does not account for,
            //  which is now NONE. Stated as an exact figure rather than left
            //  implied by the per-file column, so a defect appearing in a file
            //  whose own row nobody revisited still fails here.
            Assert::AreEqual ((size_t) 0, unexplained,
                              L"every rejection of a vendor source is a boundary refusal, and anything else is a defect");
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

                Assert::AreEqual (isDemo, rejects, CorpusText::Widen (path
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

                Assert::IsTrue (requests > 0, CorpusText::Widen (std::string (library.requestedAs)
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

                    Assert::IsTrue (served, CorpusText::Widen (std::string (entry.path)
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

                AssertSucceeded (MerlinFixture::LoadSource (provider, library.fixturePath, text));

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
                std::string  selfContained = SubsetBoundary::ComposeRefusal (row, ModuleLinkage::SelfContained);
                std::string  dependent     = SubsetBoundary::ComposeRefusal (row, ModuleLinkage::DependsOnOther);

                if (error.message == selfContained || error.message == dependent)
                {
                    matches++;
                }
            }

            Assert::AreEqual ((size_t) 1, matches, CorpusText::Widen (std::string (entry.path)
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

            return CorpusText::Widen (text);
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinMacroLibraryOracleTests
    //
    //  The distribution's general-purpose macro library, `T.MACRO LIBRARY`, as an
    //  ordinary source/object oracle -- with the difference that its object had to
    //  be MADE. Nothing on the disk includes the library, so the vendor shipped no
    //  object to compare against; the source below was authored here, assembled
    //  under real Merlin Pro 2.23, and the 279 bytes it produced are what the
    //  assembler must reproduce. Same standard as the five shipped pairs, and the
    //  same procedure: a work copy of the disk, an object name that had never been
    //  on it, and the source read BACK off the disk after saving so the text
    //  committed here is the text Merlin assembled. The round trip was clean.
    //
    //  WHY THIS LIBRARY AND NOT A HAND-WRITTEN EQUIVALENT. It is the only evidence
    //  anywhere that two constructs are ORDINARY Merlin rather than exotic:
    //
    //    * `ADDX` has no terminator of its own and falls into `ADDA`, which is
    //      where the shorthand came from -- one definition written as the prefix
    //      of the next, sharing its tail and its `<<<`. The first seven bytes
    //      below are that macro's expansion.
    //    * `MOVD`, `LDHI`, `ADD` and `SUB` dispatch on addressing mode with the
    //      first-character conditional, which is absent from most descriptions of
    //      the dialect. Every branch of `MOVD`'s three-deep nest is invoked here.
    //
    //  Both went unimplemented for a long time because no committed fixture used
    //  them. This entry is what makes that impossible to repeat.
    //
    ////////////////////////////////////////////////////////////////////////////////

    //  Merlin Pro 2.23, assembled from the source below with the library served as
    //  `T.MACRO LIBRARY`. Saved as an object name that had never existed on the
    //  disk, so its presence afterwards proves this assembly wrote it.
    static constexpr Byte  s_kMacroLibraryBytes[] =
    {
        0x8A, 0x18, 0x6D, 0x00, 0x81, 0x8D, 0x02, 0x81, 0xAD, 0x01, 0x81, 0x69, 0x00, 0x8D, 0x03, 0x81,
        0x18, 0x6D, 0x00, 0x81, 0x8D, 0x02, 0x81, 0xAD, 0x01, 0x81, 0x69, 0x00, 0x8D, 0x03, 0x81, 0x98,
        0x18, 0x6D, 0x00, 0x81, 0x8D, 0x02, 0x81, 0xAD, 0x01, 0x81, 0x69, 0x00, 0x8D, 0x03, 0x81, 0xB1,
        0x10, 0x91, 0x12, 0xC8, 0xB1, 0x10, 0x91, 0x12, 0xB1, 0x10, 0x8D, 0x02, 0x81, 0xC8, 0xB1, 0x10,
        0x8D, 0x03, 0x81, 0xA9, 0x00, 0x91, 0x12, 0xC8, 0xA9, 0x81, 0x91, 0x12, 0xAD, 0x00, 0x81, 0x91,
        0x12, 0xC8, 0xAD, 0x01, 0x81, 0x91, 0x12, 0xA9, 0x00, 0x8D, 0x02, 0x81, 0xA9, 0x81, 0x8D, 0x03,
        0x81, 0xAD, 0x00, 0x81, 0x8D, 0x02, 0x81, 0xAD, 0x01, 0x81, 0x8D, 0x03, 0x81, 0xA9, 0x81, 0xAD,
        0x01, 0x81, 0x18, 0xAD, 0x00, 0x81, 0x6D, 0x02, 0x81, 0x8D, 0x04, 0x81, 0xAD, 0x01, 0x81, 0x6D,
        0x03, 0x81, 0x8D, 0x05, 0x81, 0x18, 0xAD, 0x00, 0x81, 0x69, 0x02, 0x8D, 0x04, 0x81, 0xAD, 0x01,
        0x81, 0x69, 0x81, 0x8D, 0x05, 0x81, 0x38, 0xAD, 0x00, 0x81, 0xED, 0x02, 0x81, 0x8D, 0x04, 0x81,
        0xAD, 0x01, 0x81, 0xED, 0x03, 0x81, 0x8D, 0x05, 0x81, 0x38, 0xAD, 0x00, 0x81, 0xE9, 0x02, 0x8D,
        0x04, 0x81, 0xAD, 0x01, 0x81, 0xE9, 0x81, 0x8D, 0x05, 0x81, 0xEE, 0x00, 0x81, 0xD0, 0x03, 0xEE,
        0x01, 0x81, 0xAD, 0x00, 0x81, 0xD0, 0x03, 0xCE, 0x01, 0x81, 0xCE, 0x00, 0x81, 0xAD, 0x00, 0x81,
        0x8D, 0x02, 0x81, 0xA9, 0x05, 0x18, 0x6D, 0x00, 0x81, 0x8D, 0x00, 0x81, 0x90, 0x03, 0xEE, 0x01,
        0x81, 0xAD, 0x00, 0x81, 0x48, 0xAD, 0x02, 0x81, 0x8D, 0x00, 0x81, 0x68, 0x8D, 0x02, 0x81, 0xAD,
        0x00, 0x81, 0xCD, 0x02, 0x81, 0xAD, 0x01, 0x81, 0xED, 0x03, 0x81, 0xA9, 0x42, 0x8D, 0x00, 0x81,
        0xA9, 0xC1, 0x20, 0xED, 0xFD, 0xAE, 0x00, 0x81, 0xAD, 0x01, 0x81, 0x20, 0x41, 0xF9, 0xA9, 0x05,
        0x85, 0x24, 0xA9, 0x0A, 0x20, 0x5B, 0xFB,
    };



    //  As Merlin's editor stored it, read back off the disk after saving. That is
    //  the only text guaranteed to correspond to the bytes above.
    static const char *  s_kpszMacroLibrarySource =
        "* MACRO LIBRARY ORACLE\n"
        " ORG $8000\n"
        "ZP = $10\n"
        "ZP2 = $12\n"
        "SRC = $8100\n"
        "DEST = $8102\n"
        "RES = $8104\n"
        " USE MACRO LIBRARY\n"
        " ADDX SRC;DEST\n"
        " ADDA SRC;DEST\n"
        " ADDY SRC;DEST\n"
        " MOVD (ZP),Y;(ZP2),Y\n"
        " MOVD (ZP),Y;DEST\n"
        " MOVD #SRC;(ZP2),Y\n"
        " MOVD SRC;(ZP2),Y\n"
        " MOVD #SRC;DEST\n"
        " MOVD SRC;DEST\n"
        " LDHI #SRC\n"
        " LDHI SRC\n"
        " ADD SRC;DEST;RES\n"
        " ADD SRC;#DEST;RES\n"
        " SUB SRC;DEST;RES\n"
        " SUB SRC;#DEST;RES\n"
        " INCD SRC\n"
        " DECD SRC\n"
        " MOV SRC;DEST\n"
        " ADDNUM 5;SRC\n"
        " SWAP SRC;DEST\n"
        " COMPARE SRC;DEST\n"
        " POKE SRC;$42\n"
        " PRCHR \"A\"\n"
        " PRADRS SRC\n"
        " GOTOXY 5;10\n";



    TEST_CLASS (MerlinMacroLibraryOracleTests)
    {
    public:

        TEST_METHOD (TheVendorMacroLibraryExpandsToTheBytesMerlinProduced)
        {
            MockFileReader     reader;
            AssemblyResult     result   = Assemble (reader, DialectId::Merlin);
            std::vector<Byte>  expected (std::begin (s_kMacroLibraryBytes), std::end (s_kMacroLibraryBytes));
            CorpusComparison   compared = CorpusHarness::Compare (expected, result.bytes);

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());

            Assert::IsTrue (compared.verdict == CorpusVerdict::Match,
                            CorpusText::Widen (CorpusHarness::Describe ("macro library", compared)).c_str());

            Assert::AreEqual (0x8000, (int) result.startAddress,
                              L"the library entry loads where Merlin put it");
        }



        //  The library really is the one being expanded, asked for by the name the
        //  disk stores. Without this the byte comparison would still pass against
        //  an assembler that ignored the inclusion and found the macros somewhere
        //  else -- or, more likely, would fail with no indication that the request
        //  and the fixture disagreed about a name.
        TEST_METHOD (TheLibraryIsRequestedUnderTheNameTheDiskStores)
        {
            MockFileReader  reader;
            AssemblyResult  result = Assemble (reader, DialectId::Merlin);

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());

            Assert::AreEqual (1, reader.CountRequests ("T.MACRO LIBRARY"),
                              L"the source writes `USE MACRO LIBRARY` and the disk stores `T.MACRO LIBRARY`");
        }



        //  THE SPACE IS PART OF THE FILENAME. Measured against Merlin Pro 2.23 and
        //  not inferred: ` USE MACRO LIBRARY` assembles and defines every macro in
        //  the library, and appending ` ;NOTE` assembles identically -- so a space
        //  does not end the name and the comment introducer does.
        //
        //  Neither vendor inclusion could have settled this. Both name a file with
        //  no space in it and neither carries a trailing comment, so a scanner
        //  stopping at the first space reproduces every byte on the disk and
        //  requests `T.MACRO` here.
        TEST_METHOD (ACommentAfterTheFilenameIsNotPartOfIt)
        {
            MockFileReader  reader;
            AssemblyResult  result = Assemble (reader, DialectId::Merlin,
                                               " USE MACRO LIBRARY ;THE VENDOR LIBRARY\n"
                                               "SRC = $8100\n"
                                               " MOV SRC;SRC\n");

            Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());
            Assert::AreEqual (1, reader.CountRequests ("T.MACRO LIBRARY"),
                              L"the name runs to the comment, and the comment is not part of it");
        }



        //  The vacuity guard the captured corpus applies to every entry. The
        //  library is Merlin source through and through -- positional parameters,
        //  `MAC`, `<<<`, the first-character conditional -- so an assembler
        //  reproducing these bytes under the other dialect would mean the profile
        //  was never consulted.
        TEST_METHOD (TheSameSourceFailsUnderAs65)
        {
            MockFileReader     reader;
            AssemblyResult     result   = Assemble (reader, DialectId::As65);
            std::vector<Byte>  expected (std::begin (s_kMacroLibraryBytes), std::end (s_kMacroLibraryBytes));
            CorpusComparison   compared = CorpusHarness::Compare (expected, result.bytes);

            Assert::IsTrue (!result.errors.empty() || compared.verdict != CorpusVerdict::Match,
                            L"the library reproduces its bytes under AS65 too, so it tests nothing about the dialect");
        }

    private:

        static AssemblyResult Assemble (MockFileReader & reader, DialectId dialect, const char * source = nullptr)
        {
            FixtureProvider   provider;
            TestCpu           cpu;
            std::string       library;
            AssemblerOptions  options = {};

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/T.MACRO LIBRARY", library));

            reader.files["T.MACRO LIBRARY"] = library;

            cpu.InitForTest();
            options.dialect    = dialect;
            options.fileReader = &reader;

            {
                Assembler  assembler (cpu.GetInstructionSet(), options);

                return assembler.Assemble (source ? source : s_kpszMacroLibrarySource);
            }
        }



        static std::wstring FirstDiagnostic (const AssemblyResult & result)
        {
            if (result.errors.empty())
            {
                return L"";
            }

            return CorpusText::Widen ("line " + std::to_string (result.errors[0].lineNumber)
                                        + ": " + result.errors[0].message);
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
            //  The denial that disk file access would settle this lives in the
            //  boundary table now; the refusal itself only names the directive.
            "save-object directive",
            "        SAV OBJECT\n",
            DiagnosticKind::SubsetBoundary, 1, 9,
            "SAV", "Invalid mnemonic",
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



        //  An alternate instruction NAME rather than a directive, which is the
        //  other way a dialect can claim a word. BLT is a real instruction under
        //  another name, so "invalid mnemonic" is true of the text and false of
        //  the operation -- and the two categories must not read alike.
        TEST_METHOD (AMerlinBranchAliasUnderAs65IsReportedAsAnAlternateName)
        {
            AssemblyResult  result = AssembleAsAs65 ("  .org $800\nHERE: BLT HERE\n");

            Assert::IsFalse (result.errors.empty(), L"as65 must not accept Merlin's branch aliases");
            Assert::IsTrue (result.errors[0].message.find ("alternate instruction name") != std::string::npos,
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




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CapturedEntry
    //
    //  Source authored HERE, paired with the bytes real Merlin produced from it.
    //
    //  The vendor oracles above can only test what Bredon happened to write. These
    //  cover what he did not: the three string encodings his disk never uses, the
    //  low-ASCII delimiter, the reversed-word directive, the loop and the dummy
    //  section, and every construct whose only evidence would otherwise be the
    //  manual.
    //
    //  How they were captured, because it decides what a failure here means. Each
    //  group was typed into Merlin's own editor under emulation, saved to a WORK
    //  COPY of the disk, assembled with the listing on, and the object read back
    //  off the disk. The source committed here is the copy read BACK, not the text
    //  that was typed -- the disk copy is what Merlin assembled, so it is the only
    //  text guaranteed to correspond to the bytes beside it. Every one of them
    //  round-tripped clean, which also settles a question that had been open: the
    //  editor stores what is typed byte for byte and normalizes nothing.
    //
    //  The string rows were split from ONE assembly. A single composite carried all
    //  eleven forms separated by a four-byte marker, so one typing session and one
    //  object yielded eleven independently named expectations. That is sound here
    //  and only here: the rows are pure data directives with no labels and no
    //  branches, so a segment assembled on its own produces the identical bytes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct CapturedEntry
    {
        const char             *  name;
        const char             *  source;
        int                       loadAddress;
        std::span<const Byte>     expected;
        const char             *  merlinVersion;
        bool                      discriminates;
    };



    //  One machine, one session, one version. Recorded per row rather than once,
    //  because edge semantics differ across Merlin revisions and a row captured
    //  later on another disk has to be able to say so.
    static constexpr const char *  s_kpszCaptureVersion = "Merlin Pro 2.23";



    //  The six string encodings. Only three of them have any vendor oracle at all:
    //  the disk uses DCI, ASC and one REV, and contains no INV with an object, no
    //  FLS and no STR anywhere.
    static constexpr Byte  s_kAscHighBytes[]  = { 0xC1, 0xC2, 0xC3 };
    static constexpr Byte  s_kInverseBytes[]  = { 0x01, 0x02, 0x03 };
    static constexpr Byte  s_kFlashingBytes[] = { 0x41, 0x42, 0x43 };
    static constexpr Byte  s_kLengthBytes[]   = { 0x03, 0xC1, 0xC2, 0xC3 };
    static constexpr Byte  s_kReversedBytes[] = { 0xC3, 0xC2, 0xC1 };
    static constexpr Byte  s_kAscLowBytes[]   = { 0x41, 0x42, 0x43 };

    //  The pair the vendor corpus provably CANNOT settle. Every DCI on the disk is
    //  high-ASCII, so its terminator always ends up with bit 7 clear and an
    //  implementation that CLEARS rather than inverts reproduces all 984 bytes of
    //  LABELS exactly. Only a low-ASCII string tells the two apart, and the disk
    //  holds none -- so these two rows are the whole of the evidence.
    static constexpr Byte  s_kDciHighBytes[]  = { 0xC1, 0xC2, 0x43 };
    static constexpr Byte  s_kDciLowBytes[]   = { 0x41, 0x42, 0xC3 };

    //  Spaces inside the quotes are payload, not separators -- a whitespace-ended
    //  operand scanner truncates this and changes emitted data without a word.
    static constexpr Byte  s_kQuotedSpaceBytes[] = { 0xA0, 0xC1, 0xC2, 0xA0, 0xC3, 0xA0 };

    //  The delimiter is whatever character follows the directive, chosen here so
    //  the text can contain a quote.
    static constexpr Byte  s_kOwnDelimiterBytes[] = { 0xC1, 0xC2, 0xA2, 0xC3, 0xC4 };

    //  Hexadecimal digits after the closing delimiter, emitted verbatim -- the
    //  trailing run does NOT go through the delimiter's high-bit convention, which
    //  the 00 staying 00 is what proves.
    static constexpr Byte  s_kTrailingHexBytes[] = { 0xC1, 0xC2, 0x8D, 0x00 };

    //  Every value here is hand-derivable from the manual, and was derived before
    //  the capture rather than read off it: 2+3*4 is 20 because evaluation is
    //  strictly left to right, 1+2!3 is 0 because ! is exclusive-or, 4.1 is 5
    //  because . is inclusive-or, and the reversed-order word puts its HIGH byte
    //  first. Agreement is what discharges the "did the emulator run Merlin
    //  correctly on the day" question rather than leaving it assumed.
    static constexpr Byte  s_kExpressionBytes[] = { 0x14, 0x05, 0x00, 0x05, 0x30, 0x05, 0x10, 0x07, 0x0A };

    //  A character constant, in both of Merlin's spellings. The double-quoted form
    //  is HIGH ascii and the apostrophe form is low -- the same convention the
    //  string directives take from their delimiter, applied to one character.
    static constexpr Byte  s_kHighAsciiCharBytes[] = { 0xC1 };
    static constexpr Byte  s_kLowAsciiCharBytes[]  = { 0x41 };

    //  A negative literal, and the two word directives side by side so the byte
    //  order of each is stated against the other. Neither value is a palindrome,
    //  since a palindrome satisfies both orders and proves only that two bytes
    //  came out.
    static constexpr Byte  s_kWordDataBytes[] = { 0xFF, 0x34, 0x12, 0x12, 0x34 };

    //  The loop, the dummy section, the conditional and the assembly-time
    //  assertion. No vendor source uses the loop or the dummy section at all, so
    //  before this row there was no oracle for either.
    static constexpr Byte  s_kStructureBytes[] =
    {
        0xEA, 0xEA, 0xEA, 0xA5, 0x50, 0xA5, 0x52, 0x60,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    };

    //  Local labels reused under two different globals, a variable symbol
    //  reassigned between two references, a question mark inside a label, and a
    //  symbol at the longest length Merlin accepts.
    static constexpr Byte  s_kSymbolBytes[] =
    {
        0xA9, 0x00, 0x85, 0x00, 0xD0, 0xFC, 0xA9, 0x01,
        0x85, 0x01, 0xD0, 0xFC, 0xA9, 0x10, 0xA9, 0x20,
        0xA9, 0x7F, 0xA9, 0x11, 0x06,
    };

    //  A macro definition with no terminator of its own falling into the next, and
    //  the first-character conditional macros dispatch addressing modes with.
    static constexpr Byte  s_kMacroBytes[] = { 0x8A, 0x18, 0x65, 0x10, 0x18, 0x65, 0x20, 0xEA, 0xE8, 0xC8 };

    //  Both explicit invocation spellings, with and without arguments.
    static constexpr Byte  s_kExplicitCallBytes[] = { 0xEA, 0xEA, 0xA5, 0x10, 0x85, 0x20, 0xA5, 0x30, 0x85, 0x40 };

    //  The line model, settled empirically rather than from the manual. A run of
    //  whitespace is ONE separator however long it is, and a fourth field is a
    //  comment whatever character starts it -- the semicolon is not required.
    static constexpr Byte  s_kLineModelBytes[] = { 0xA9, 0x41, 0xA9, 0x42, 0xA9, 0x43, 0xA9, 0x44 };

    //  One inclusion, served through the mock reader rather than from the table --
    //  see the test below, which also asserts the name that was REQUESTED.
    static constexpr Byte  s_kInclusionBytes[] = { 0x18, 0x65, 0x11, 0x18, 0x65, 0x22 };



    static constexpr CapturedEntry  s_kCapturedCorpus[] =
    {
        { "ASC high ASCII",        " ASC \"ABC\"\n",      0x8000, s_kAscHighBytes,      s_kpszCaptureVersion, true  },
        { "DCI high ASCII",        " DCI \"ABC\"\n",      0x8000, s_kDciHighBytes,      s_kpszCaptureVersion, true  },
        { "INV",                   " INV \"ABC\"\n",      0x8000, s_kInverseBytes,      s_kpszCaptureVersion, true  },
        { "FLS",                   " FLS \"ABC\"\n",      0x8000, s_kFlashingBytes,     s_kpszCaptureVersion, true  },
        { "STR",                   " STR \"ABC\"\n",      0x8000, s_kLengthBytes,       s_kpszCaptureVersion, true  },
        { "REV",                   " REV \"ABC\"\n",      0x8000, s_kReversedBytes,     s_kpszCaptureVersion, true  },
        { "ASC low ASCII",         " ASC 'ABC'\n",        0x8000, s_kAscLowBytes,       s_kpszCaptureVersion, true  },
        { "DCI low ASCII",         " DCI 'ABC'\n",        0x8000, s_kDciLowBytes,       s_kpszCaptureVersion, true  },
        { "quoted spaces",         " ASC \" AB C \"\n",   0x8000, s_kQuotedSpaceBytes,  s_kpszCaptureVersion, true  },
        { "source-chosen delimiter", " ASC !AB\"CD!\n",   0x8000, s_kOwnDelimiterBytes, s_kpszCaptureVersion, true  },
        { "trailing hexadecimal",  " ASC \"AB\"8D00\n",   0x8000, s_kTrailingHexBytes,  s_kpszCaptureVersion, true  },

        {
            "expression operators",
            " ORG $1000\n"
            " DFB 2+3*4\n"
            " DFB 8/2+1\n"
            " DFB 1+2!3\n"
            " DFB 4.1\n"
            " DFB $F0&$3C\n"
            " DA *\n"
            " DFB *-$1000\n"
            " DFB %1010\n",
            0x1000, s_kExpressionBytes, s_kpszCaptureVersion, true,
        },
        { "low-ASCII character constant", " DFB 'A'\n", 0x8000, s_kLowAsciiCharBytes, s_kpszCaptureVersion, true },
        {
            "negative literal and both word orders",
            " DFB -1\n"
            " DA $1234\n"
            " DDB $1234\n",
            0x8000, s_kWordDataBytes, s_kpszCaptureVersion, true,
        },
        {
            "structure",
            " ORG $2000\n"
            "COUNT = 3\n"
            " LUP 3\n"
            " NOP\n"
            " --^\n"
            " DUM $50\n"
            "DPTR DS 2\n"
            "DFLG DS 1\n"
            " DEND\n"
            " LDA DPTR\n"
            " LDA DFLG\n"
            " DO COUNT-3\n"
            " BRK\n"
            " ELSE\n"
            " RTS\n"
            " FIN\n"
            " ERR *-*\n"
            " DS 4\n"
            " HEX 0102\n",
            0x2000, s_kStructureBytes, s_kpszCaptureVersion, true,
        },
        {
            "symbols",
            " ORG $2100\n"
            "GLOB LDA #$00\n"
            ":LOC STA $00\n"
            " BNE :LOC\n"
            "GLOB2 LDA #$01\n"
            ":LOC STA $01\n"
            " BNE :LOC\n"
            "]V = $10\n"
            " LDA #]V\n"
            "]V = $20\n"
            " LDA #]V\n"
            "CMD? = $7F\n"
            " LDA #CMD?\n"
            "A234567890123 = $11\n"
            " LDA #A234567890123\n"
            " DFB GLOB2-GLOB\n",
            0x2100, s_kSymbolBytes, s_kpszCaptureVersion, true,
        },
        {
            //  The only row whose constructs are shared with the other dialect --
            //  an immediate load and a label. It carries the flag CLEAR
            //  deliberately, so the sweep below has a row that exercises the
            //  conditional's other branch rather than being a loop with a
            //  condition nothing ever fails.
            "line model",
            " LDA #$41\n"
            "  LDA #$42\n"
            "LBL   LDA #$43\n"
            " LDA #$44 THIS IS A COMMENT\n",
            0x8000, s_kLineModelBytes, s_kpszCaptureVersion, false,
        },
        {
            //  Merlin's byte directive takes an EXPRESSION, so a double-quoted
            //  single character in one is the high-ASCII character constant the
            //  string directives already spell that way -- not a one-character
            //  string literal. Names no origin, so the default origin is asserted
            //  beside the byte.
            "high-ASCII character constant in a byte directive",
            " DFB \"A\"\n",
            0x8000, s_kHighAsciiCharBytes, s_kpszCaptureVersion, true,
        },
        {
            //  Explicit invocation, in both of its spellings and with and without
            //  arguments. The macro NAME sits in the operand field and the
            //  arguments in the field after it, so the name is separated from
            //  them by a space and only they are separated from each other by the
            //  macro separator.
            "explicit macro invocation",
            "NOPS MAC\n"
            " NOP\n"
            " <<<\n"
            "MOV2 MAC\n"
            " LDA ]1\n"
            " STA ]2\n"
            " <<<\n"
            " ORG $2300\n"
            " >>> NOPS\n"
            " PMC NOPS\n"
            " >>> MOV2 $10;$20\n"
            " PMC MOV2 $30;$40\n",
            0x2300, s_kExplicitCallBytes, s_kpszCaptureVersion, true,
        },
        {
            //  A COMPOSITE OF TWO CONSTRUCTS, captured as one assembly and kept
            //  whole. Splitting it would mean counting the byte offsets between
            //  the two halves by hand, which is exactly how a self-consistent and
            //  wrong entry gets made.
            //
            //  The first half is a definition with no terminator of its own
            //  falling into the next: `ADDX` runs on into `ADDA`'s body and one
            //  `<<<` closes both. The second is the first-character conditional
            //  three macros deep, which is how a Merlin macro dispatches on
            //  addressing mode.
            "macro fall-through and first-character conditional",
            "ADDX MAC\n"
            " TXA\n"
            "ADDA MAC\n"
            " CLC\n"
            " ADC ]1\n"
            " <<<\n"
            "DISP MAC\n"
            " IF (=]1\n"
            " NOP\n"
            " ELSE\n"
            " IF #=]1\n"
            " INX\n"
            " ELSE\n"
            " INY\n"
            " FIN\n"
            " FIN\n"
            " <<<\n"
            " ORG $2200\n"
            " ADDX $10\n"
            " ADDA $20\n"
            " DISP (ZZ),Y\n"
            " DISP #5\n"
            " DISP QQQ\n",
            0x2200, s_kMacroBytes, s_kpszCaptureVersion, true,
        },
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PendingCapture
    //
    //  Bytes captured from real Merlin for a construct Casso does not yet
    //  reproduce. The evidence is committed HERE rather than held outside the
    //  suite, because a capture living in a document is a capture nobody re-runs.
    //
    //  The sweep over these asserts each one still DIVERGES, and that is the whole
    //  design: implementing the construct makes this table's test fail, which
    //  forces the row to be moved up into the corpus proper rather than left
    //  behind as a stale note claiming a gap that has been closed. It is the same
    //  shape as pinning a known rejection count at its measured value.
    //
    //  Every row was captured the same way as the corpus above, so none of them is
    //  a guess about what Merlin would do.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct PendingCapture
    {
        const char             *  name;
        const char             *  source;
        std::span<const Byte>     expected;
        const char             *  merlinVersion;
        const char             *  divergence;
    };



    //  `PRINT "X"` through the vendor macro library. The library is served from
    //  its committed fixture; see MerlinMacroLibraryOracleTests, whose source this
    //  is a cut-down copy of and whose 279 bytes DO reproduce.
    //
    //  JSR SENDMSG, the message as one high-ASCII byte, BRK, and the RTS the label
    //  sits on.
    static constexpr Byte  s_kPrintMacroBytes[] = { 0x20, 0x05, 0x80, 0xD8, 0x00, 0x60 };



    static constexpr PendingCapture  s_kPendingCaptures[] =
    {
        {
            //  THE FIRST-CHARACTER CONDITIONAL WITH THE PARAMETER WRITTEN FIRST,
            //  which the vendor's own PRINT macro uses and which the corpus was
            //  previously recorded as structurally unable to settle. It is settled
            //  now, and the answer is that Merlin does exactly what Casso does:
            //  the test is PURELY POSITIONAL, comparing the first and third
            //  characters of the operand after substitution, and it never learns
            //  which position held the reference.
            //
            //  Which means the vendor macro is broken as written, and that is the
            //  measurement rather than an inference. `IF ]1="` with a one-
            //  character message substitutes to `"X"="`, whose first and third
            //  characters are both `"` -- so the quoted branch is taken by
            //  accident and the six bytes below are produced. The same macro with
            //  `PRINT "HI"` substitutes to `"HI"="`, whose first and third are `"`
            //  and `I`, takes the hex branch, and Merlin produces NO OBJECT at all.
            //
            //  WHAT DIVERGES is not the conditional. Casso refuses the invocation
            //  before evaluating it, because the branch that is not taken refers to
            //  `]2` and only one argument was supplied; Merlin binds an unsupplied
            //  parameter to nothing and assembles. Implementing that moves this row
            //  up into the corpus.
            "first-character conditional with the parameter written first",
            "* PRINT MACRO PROBE\n"
            " ORG $8000\n"
            " USE MACRO LIBRARY\n"
            " PRINT \"X\"\n"
            "SENDMSG RTS\n",
            s_kPrintMacroBytes,
            s_kpszCaptureVersion,
            "a macro invocation is refused when a branch that is not taken refers to a"
            " parameter the call did not supply",
        },
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinCapturedCorpusTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinCapturedCorpusTests)
    {
    public:

        //  The absent-corpus guard, first, because every sweep below is a loop and
        //  a loop over an empty table reports success having compared nothing.
        TEST_METHOD (TheCapturedCorpusIsNotEmpty)
        {
            Assert::IsTrue (std::size (s_kCapturedCorpus) >= 15,
                            L"the captured corpus must cover the constructs the vendor disk never uses");

            for (const CapturedEntry & entry : s_kCapturedCorpus)
            {
                Assert::IsFalse (entry.expected.empty(),
                                 CorpusText::Widen (std::string (entry.name) + ": an empty expectation compares nothing").c_str());
                Assert::IsNotNull (entry.merlinVersion,
                                   L"a captured entry without a version stamp cannot say what produced it");
            }
        }



        //  Every string encoding is present exactly once, asserted rather than
        //  eyeballed. Three of the six have no vendor oracle at all, so a row
        //  quietly dropped here would take the only evidence for that spelling
        //  with it and every remaining test would stay green.
        TEST_METHOD (EveryStringEncodingHasACapturedEntry)
        {
            for (const char * spelling : { "ASC", "DCI", "INV", "FLS", "STR", "REV" })
            {
                size_t  found = 0;

                for (const CapturedEntry & entry : s_kCapturedCorpus)
                {
                    if (std::string (entry.source).find (std::string (" ") + spelling + " ") != std::string::npos)
                    {
                        found++;
                    }
                }

                Assert::IsTrue (found > 0,
                                CorpusText::Widen (std::string (spelling) + " has no captured entry, and half the family has no vendor oracle either").c_str());
            }
        }



        TEST_METHOD (EveryCapturedEntryAssemblesToTheBytesMerlinProduced)
        {
            for (const CapturedEntry & entry : s_kCapturedCorpus)
            {
                AssemblyResult     result   = Assemble (entry, DialectId::Merlin);
                std::vector<Byte>  expected (entry.expected.begin(), entry.expected.end());
                CorpusComparison   compared = CorpusHarness::Compare (expected, result.bytes);

                Assert::IsTrue (result.errors.empty(), Diagnose (entry, result).c_str());

                Assert::IsTrue (compared.verdict == CorpusVerdict::Match,
                                CorpusText::Widen (CorpusHarness::Describe (entry.name, compared)
                                       + At (compared, expected, result.bytes)).c_str());

                //  Half the claim. A wrong default origin yields byte-perfect
                //  output in the wrong place, which reads as a far deeper problem
                //  than it is -- and eleven of these rows name no origin at all.
                Assert::AreEqual (entry.loadAddress, static_cast<int> (result.startAddress),
                                  CorpusText::Widen (std::string (entry.name) + " must load where Merlin put it").c_str());
            }
        }



        //  The vacuity guard. Labels, origin, literals and the evaluator are SHARED
        //  between the dialects, so an entry built from those alone is green
        //  whether the Merlin profile works or is never consulted at all.
        TEST_METHOD (EveryDiscriminatingCapturedEntryFailsUnderAs65)
        {
            size_t  discriminating = 0;
            size_t  shared         = 0;

            for (const CapturedEntry & entry : s_kCapturedCorpus)
            {
                if (!entry.discriminates)
                {
                    shared++;
                    continue;
                }

                AssemblyResult     result   = Assemble (entry, DialectId::As65);
                std::vector<Byte>  expected (entry.expected.begin(), entry.expected.end());
                CorpusComparison   compared = CorpusHarness::Compare (expected, result.bytes);

                discriminating++;

                Assert::IsTrue (!result.errors.empty() || compared.verdict != CorpusVerdict::Match,
                                CorpusText::Widen (std::string (entry.name)
                                       + " reproduces its bytes under AS65 too, so it tests nothing about the dialect").c_str());
            }

            //  Both counts, because the conditional above is only a conditional if
            //  each branch is reached. A table where every row discriminates makes
            //  this an unconditional sweep and nothing says so.
            Assert::IsTrue (discriminating > 0, L"no captured entry claims a Merlin construct");
            Assert::IsTrue (shared > 0, L"no captured entry leaves the flag clear, so the false branch is never taken");
        }



        //  Every pending row must still DIVERGE. The assertion runs in that
        //  direction on purpose: implementing one of these constructs turns this
        //  test red, which is what forces the row to be moved into the corpus
        //  above instead of being left behind claiming a gap that is closed.
        //
        //  Divergence means the bytes differ or the source is refused. Both count,
        //  because a construct that is not implemented can fail either way and the
        //  claim being pinned is "Casso does not yet reproduce this", not "Casso
        //  produces this particular wrong answer".
        TEST_METHOD (EveryPendingCaptureStillDiverges)
        {
            Assert::IsTrue (std::size (s_kPendingCaptures) > 0,
                            L"an empty pending table is not evidence that nothing is pending");

            for (const PendingCapture & pending : s_kPendingCaptures)
            {
                CapturedEntry      entry    = { pending.name, pending.source, 0, pending.expected,
                                                pending.merlinVersion, true };
                MockFileReader     reader;
                AssemblyResult     result   = Assemble (entry, DialectId::Merlin, reader);
                std::vector<Byte>  expected (pending.expected.begin(), pending.expected.end());
                CorpusComparison   compared = CorpusHarness::Compare (expected, result.bytes);

                //  Diverging because a file the row includes was never served
                //  would satisfy the assertion below while proving nothing about
                //  the construct -- and would keep proving nothing after the
                //  construct was implemented. A row must fail for its own reason.
                for (const std::string & requested : reader.requests)
                {
                    Assert::IsTrue (reader.files.find (requested) != reader.files.end(),
                                    CorpusText::Widen (std::string (pending.name) + " asked for \"" + requested
                                           + "\", which was not served -- its divergence says nothing"
                                             " about the construct").c_str());
                }

                Assert::IsTrue (!result.errors.empty() || compared.verdict != CorpusVerdict::Match,
                                CorpusText::Widen (std::string (pending.name)
                                       + " now reproduces its captured bytes -- move it into the corpus. Recorded divergence: "
                                       + pending.divergence).c_str());
            }
        }



        //  Inclusion, which the table cannot hold because it needs two sources.
        //  The name REQUESTED is asserted beside the bytes: Merlin resolves the
        //  operand by prepending a prefix, so an assembler asking for the name as
        //  written would find nothing and the mock would report a missing file
        //  rather than the wrong include.
        TEST_METHOD (AnIncludedMacroLibraryExpandsToTheBytesMerlinProduced)
        {
            MockFileReader     reader;
            TestCpu            cpu;
            AssemblerOptions   options  = {};
            std::vector<Byte>  expected (std::begin (s_kInclusionBytes), std::end (s_kInclusionBytes));

            reader.files["T.MYMAC"] = "MYADD MAC\n"
                                      " CLC\n"
                                      " ADC ]1\n"
                                      " <<<\n";

            cpu.InitForTest();
            options.dialect    = DialectId::Merlin;
            options.fileReader = &reader;

            {
                Assembler       assembler (cpu.GetInstructionSet(), options);
                AssemblyResult  result   = assembler.Assemble (" ORG $2400\n"
                                                               " USE MYMAC\n"
                                                               " MYADD $11\n"
                                                               " MYADD $22\n");
                CorpusComparison  compared = CorpusHarness::Compare (expected, result.bytes);

                Assert::IsTrue (result.errors.empty(), FirstDiagnostic (result).c_str());
                Assert::IsTrue (compared.verdict == CorpusVerdict::Match,
                                CorpusText::Widen (CorpusHarness::Describe ("inclusion", compared)).c_str());
                Assert::AreEqual (1, reader.CountRequests ("T.MYMAC"),
                                  L"the operand names MYMAC and the file is T.MYMAC -- the prefix is the dialect's, not the caller's");
            }
        }



    private:

        //  The vendor macro library is OFFERED to every entry rather than pushed:
        //  an entry that includes nothing never asks for it, and the request log
        //  is what says which happened. Offering it is what lets an entry whose
        //  subject is a library macro be assembled through this one path instead
        //  of growing a second one that could decode or configure differently.
        static AssemblyResult Assemble (const CapturedEntry & entry, DialectId dialect)
        {
            MockFileReader  reader;

            return Assemble (entry, dialect, reader);
        }



        static AssemblyResult Assemble (const CapturedEntry & entry, DialectId dialect, MockFileReader & reader)
        {
            FixtureProvider   provider;
            TestCpu           cpu;
            std::string       library;
            AssemblerOptions  options = {};
            AssemblyResult    result;

            AssertSucceeded (MerlinFixture::LoadSource (provider, "Merlin/T.MACRO LIBRARY", library));

            reader.files["T.MACRO LIBRARY"] = library;

            cpu.InitForTest();
            options.dialect    = dialect;
            options.fileReader = &reader;

            {
                Assembler  assembler (cpu.GetInstructionSet(), options);

                result = assembler.Assemble (entry.source);
            }

            return result;
        }



        //  The two bytes that disagree, which is what turns "offset 9" into a
        //  statement about the construct on that line. An offset alone sends the
        //  reader counting bytes in the source by hand.
        static std::string At (const CorpusComparison & compared,
                               const std::vector<Byte> & expected,
                               const std::vector<Byte> & actual)
        {
            std::string  text;
            char         line[64] = {};

            if (!compared.hasFirstDifference)
            {
                return text;
            }

            if (compared.firstDifference < expected.size() && compared.firstDifference < actual.size())
            {
                sprintf_s (line, " -- Merlin wrote $%02X, Casso wrote $%02X",
                           expected[compared.firstDifference], actual[compared.firstDifference]);
                text = line;
            }

            return text;
        }



        static std::wstring FirstDiagnostic (const AssemblyResult & result)
        {
            std::string  text = "assembled clean";

            if (!result.errors.empty())
            {
                text = "line " + std::to_string (result.errors[0].lineNumber) + ": " + result.errors[0].message
                     + " (" + std::to_string (result.errors.size()) + " total)";
            }

            return CorpusText::Widen (text);
        }



        static std::wstring Diagnose (const CapturedEntry & entry, const AssemblyResult & result)
        {
            std::string  text = std::string (entry.name) + " (" + entry.merlinVersion + "): ";

            if (result.errors.empty())
            {
                text += "assembled clean";
            }
            else
            {
                text += "line " + std::to_string (result.errors[0].lineNumber) + ": " + result.errors[0].message
                      + " (" + std::to_string (result.errors.size()) + " total)";
            }

            return CorpusText::Widen (text);
        }
    };
}
