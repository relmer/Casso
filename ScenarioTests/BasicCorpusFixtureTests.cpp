#include "Pch.h"
#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "HeadlessHost.h"
#include "ApplesoftTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCorpusFixtureTests
//
//  The construct corpus against the only oracle that is not our own code:
//  Applesoft itself. The committed listing is typed, line by line, into a
//  booted DOS 3.3 master, and the bytes the ROM stored between TXTTAB and
//  VARTAB must be byte-identical to the committed fixture the unit suite
//  checks the tokenizer against.
//
//  THIS CASE IS ALSO THE FIXTURE GENERATOR, deliberately. On a mismatch it
//  writes what Applesoft actually stored to a file in the temp directory and
//  names the path, and copying that file over construct-corpus.tok is the
//  ONLY sanctioned way the fixture is ever regenerated -- never from the
//  tokenizer's own output, which would make the unit-suite check circular.
//  Regeneration therefore needs the master and a booted guest, so it cannot
//  happen by accident, and a fixture diff is visible in review. See
//  UnitTest/Fixtures/Basic/construct-inventory.md.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (BasicCorpusFixtureTests)
{
public:

    static constexpr const char *  kListingFixture = "Basic/construct-corpus.bas";
    static constexpr const char *  kTokensFixture  = "Basic/construct-corpus.tok";

    static constexpr const wchar_t *  kpszDumpName = L"casso-construct-corpus.tok";

    //  Where Applesoft keeps the start of the program and the start of its
    //  variables, which is the byte after the program's final null link.
    static constexpr Word  kTxtTab = 0x0067;
    static constexpr Word  kVarTab = 0x0069;

    //  Below this the corpus is not a corpus; the committed listing is dozens
    //  of lines, and a truncated read must fail rather than compare nothing.
    static constexpr size_t  kLeastCorpusLines = 20;

    //  What a stored line costs ahead of its body: the link and the number.
    static constexpr size_t  kLineHeaderBytes = 4;


    //
    //  ------------------------------------------------------------------
    //  Material.
    //  ------------------------------------------------------------------
    //

    static std::vector<std::string> CorpusLines()
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         bytes;
        std::vector<std::string>  lines;
        std::string               current;
        size_t                    i = 0;

        AssertSucceeded (fixtures.OpenFixture (kListingFixture, bytes),
            L"the corpus listing is REQUIRED: a generator with nothing to type would "
            L"produce an empty fixture that the unit suite then trusts");

        for (i = 0; i < bytes.size(); i++)
        {
            char  c = (char) bytes[i];

            if (c == '\r')
            {
                continue;
            }

            if (c == '\n')
            {
                if (!current.empty())
                {
                    lines.push_back (current);
                    current.clear();
                }

                continue;
            }

            current += c;
        }

        if (!current.empty())
        {
            lines.push_back (current);
        }

        Assert::IsTrue (lines.size() >= kLeastCorpusLines,
            L"and must carry the whole corpus, not a truncated read of it");

        return lines;
    }


    static std::vector<Byte> CommittedFixtureOrEmpty()
    {
        //  Absent is legal HERE and only here: the first generation run has
        //  nothing committed yet, and this case still has to boot, type and
        //  dump so there is something to commit. The comparison below still
        //  fails on empty, with the dump path in its message.
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;
        HRESULT            hr = fixtures.OpenFixture (kTokensFixture, bytes);

        if (FAILED (hr))
        {
            bytes.clear();
        }

        return bytes;
    }


    static Word ReadGuestWord (EmulatorCore & core, Word address)
    {
        std::vector<Byte>  bytes = GuestSession::GuestBytesAt (core, address, 2);

        return (Word) (bytes[0] | (bytes[1] << 8));
    }


    //  The program proper: everything through its final null link. VARTAB
    //  points one byte past the program on a real machine, and that byte is
    //  memory, not program -- measured, and committing it would make the
    //  fixture disagree with the tokenizer over a byte neither owns.
    static std::vector<Byte> TrimToTheNullLink (const std::vector<Byte> & stored)
    {
        size_t  at = 0;

        while (at + 1 < stored.size() && !(stored[at] == 0 && stored[at + 1] == 0))
        {
            at += kLineHeaderBytes;

            while (at < stored.size() && stored[at] != 0)
            {
                at++;
            }

            at++;
        }

        Assert::IsTrue (at + 1 < stored.size(),
            L"the stored program must end on a null link inside what VARTAB bounds, or "
            L"the walk above ran off something that is not a program");

        return std::vector<Byte> (stored.begin(), stored.begin() + (ptrdiff_t) (at + 2));
    }


    //  Applesoft's stored bytes, written where a human can pick them up to
    //  commit them. The temp directory rather than the tree: a test run may
    //  never write into the repository, even the bytes it wishes were there.
    static std::wstring DumpForCommitting (const std::vector<Byte> & stored)
    {
        std::error_code        ec;
        std::filesystem::path  full = std::filesystem::temp_directory_path (ec) / kpszDumpName;
        std::ofstream          out (full, std::ios::binary | std::ios::trunc);
        bool                   open = out.good();

        Assert::IsTrue (!ec && open,
            L"the dump file must open, or a mismatch leaves nothing to regenerate from");

        out.write ((const char *) stored.data(), (std::streamsize) stored.size());
        out.close();

        return full.wstring();
    }


    //
    //  ------------------------------------------------------------------
    //  The gate, and the generator.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (TheCommittedFixture_IsWhatApplesoftStoresForTheCommittedListing)
    {
        HeadlessHost              host;
        EmulatorCore              core;
        std::vector<Byte>         master   = GuestSession::RequireDos33Master();
        std::vector<std::string>  lines    = CorpusLines();
        std::vector<Byte>         expected = CommittedFixtureOrEmpty();
        std::vector<Byte>         stored;
        std::wstring              message;
        Word                      txtTab   = 0;
        Word                      varTab   = 0;
        size_t                    i        = 0;



        GuestSession::BootToPrompt (host, core, master);

        GuestSession::TypeAndCollect (core, "NEW");

        for (i = 0; i < lines.size(); i++)
        {
            GuestSession::TypeAndCollect (core, lines[i]);
        }

        txtTab = ReadGuestWord (core, kTxtTab);
        varTab = ReadGuestWord (core, kVarTab);

        Assert::AreEqual ((int) ApplesoftTokenizer::kProgramBase, (int) txtTab,
            L"Applesoft must be holding its program where the tokenizer builds links for");

        Assert::IsTrue (varTab > txtTab,
            L"and must be holding a program at all: a VARTAB at or below TXTTAB means "
            L"nothing was typed, and dumping that would commit an empty oracle");

        stored = TrimToTheNullLink (
                     GuestSession::GuestBytesAt (core, txtTab, (size_t) (varTab - txtTab)));

        if (stored != expected)
        {
            message  = L"what Applesoft stored for the corpus is not the committed "
                       L"fixture. Its actual bytes were written to\n  ";
            message += DumpForCommitting (stored);
            message += L"\nIf the LISTING changed, review the dump and copy it over "
                       L"UnitTest/Fixtures/Basic/construct-corpus.tok, then rebuild so "
                       L"the unit suite re-embeds it. If the listing did NOT change, "
                       L"this is a real divergence between the machine and the fixture "
                       L"-- do not regenerate over it.";

            Assert::Fail (message.c_str());
        }
    }
};
