#include "Pch.h"

#include "Cpu6502.h"
#include "CycleReference.h"
#include "Core/Cpu65C02Table.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CycleReferenceTests
//
//  The guard on docs/cycle-reference.md. It regenerates the document from the
//  live instruction tables and compares it against the copy in the tree, so a
//  changed cycle count cannot leave a stale reference behind it.
//
//  This is a repo-integration check, not a pure unit test: it reads a committed
//  file, the way the Merlin corpus and Dormann suites read theirs. It NEVER
//  writes into the tree. The regenerated copy goes to a fixed path under the
//  system temp directory, which is where scripts/UpdateCycleReference.ps1 picks
//  it up; a test that rewrote the document would make the comparison vacuous
//  and would trip the working-tree guard in RunTests.ps1 besides.
//
//  A missing or unreadable document FAILS rather than skipping. The file is
//  committed, so its absence means something is wrong -- and a check that
//  quietly passes when it could not reach its data is the exact failure this
//  project has been bitten by repeatedly.
//
//  The tables come from the SHIPPING types (Cpu6502 and the CMOS table the
//  assembler is handed), not from the test doubles, so what is pinned is what
//  the product bills.
//
////////////////////////////////////////////////////////////////////////////////

namespace CycleReferenceTests
{
    TEST_CLASS (CycleReferenceTests)
    {
    public:

        static constexpr int      kMaxAncestorWalk  = 10;
        static constexpr int      kOpcodeCount      = CycleReference::kOpcodeCount;
        static constexpr wchar_t  kDocumentPath[]   = L"docs/cycle-reference.md";
        static constexpr wchar_t  kRefreshCommand[] = L"pwsh scripts/UpdateCycleReference.ps1";


        //////////////////////////////////////////////////////////////////////
        //  Document_MatchesTheInstructionTables
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Document_MatchesTheInstructionTables)
        {
            std::string  generated = BuildDocument();
            fs::path     artifact  = WriteRegeneratedCopy (generated);
            fs::path     committed = FindRepoFile (kDocumentPath);
            std::string  onDisk;
            size_t       line      = 0;

            Assert::IsFalse (committed.empty(),
                             std::format (L"{} was not found in any ancestor of the test's working "
                                          L"directory. It is a committed file; a checkout without it "
                                          L"is broken. Regenerate with: {}",
                                          kDocumentPath, kRefreshCommand).c_str());

            onDisk = Normalize (ReadTextFile (committed));

            Assert::IsFalse (onDisk.empty(),
                             std::format (L"{} is empty. Regenerate with: {}",
                                          kDocumentPath, kRefreshCommand).c_str());

            generated = Normalize (generated);

            if (generated == onDisk)
            {
                return;
            }

            line = FindFirstDifferingLine (onDisk, generated);

            Assert::Fail (std::format (L"{} no longer matches the emulator's instruction tables; "
                                       L"the first difference is at line {}.\n"
                                       L"  committed: {}\n"
                                       L"  generated: {}\n"
                                       L"Regenerate the document and commit it: {}\n"
                                       L"A freshly generated copy is already at {}.",
                                       kDocumentPath,
                                       line,
                                       Widen (GetLine (onDisk,    line)),
                                       Widen (GetLine (generated, line)),
                                       kRefreshCommand,
                                       artifact.wstring()).c_str());
        }


        //////////////////////////////////////////////////////////////////////
        //  Document_CoversEveryOpcode
        //
        //  A generator that produced nothing would compare equal to a document
        //  that also had nothing in it, and both would look healthy. Count the
        //  rows before trusting the comparison above.
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Document_CoversEveryOpcode)
        {
            std::string  generated = BuildDocument();
            int          rows      = 0;
            int          opcode    = 0;

            for (opcode = 0; opcode < kOpcodeCount; ++opcode)
            {
                std::string  marker = std::format ("\n| ${:02X} | ", opcode);

                if (generated.find (marker) != std::string::npos)
                {
                    ++rows;
                }
            }

            Assert::AreEqual (kOpcodeCount, rows, L"the generated reference must carry one row per opcode");
        }


        //////////////////////////////////////////////////////////////////////
        //  Document_StatesTheConditionalCycles
        //
        //  The counts in the table are useless without the three run-time
        //  additions that are deliberately not in them. Pin the prose so a
        //  future edit cannot quietly drop it.
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Document_StatesTheConditionalCycles)
        {
            std::string  generated = BuildDocument();

            Assert::AreNotEqual (std::string::npos, generated.find ("Page crossing, +1"),   L"page-crossing rule");
            Assert::AreNotEqual (std::string::npos, generated.find ("Branch taken, +1"),    L"branch rule");
            Assert::AreNotEqual (std::string::npos, generated.find ("Decimal arithmetic"),  L"65C02 decimal rule");
        }


        //////////////////////////////////////////////////////////////////////
        //  Document_NamesTheDisputedSlots
        //
        //  The three opcodes where Casso and the upstream vectors disagree are
        //  the document's only claim about its own trustworthiness, so a reader
        //  must not have to go looking for them.
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Document_NamesTheDisputedSlots)
        {
            std::string  generated = BuildDocument();

            Assert::AreNotEqual (std::string::npos, generated.find ("`$DB`"), L"the skipped opcode");
            Assert::AreNotEqual (std::string::npos, generated.find ("`$5C`"), L"the first cycle-exempt opcode");
            Assert::AreNotEqual (std::string::npos, generated.find ("`$CB`"), L"the second cycle-exempt opcode");
        }


    private:

        //////////////////////////////////////////////////////////////////////
        //  BuildDocument
        //////////////////////////////////////////////////////////////////////

        static std::string BuildDocument()
        {
            Cpu6502  nmos;

            return CycleReference::Format (nmos.GetInstructionSet(), GetCpu65C02InstructionSet());
        }


        //////////////////////////////////////////////////////////////////////
        //  WriteRegeneratedCopy
        //
        //  Drops the freshly generated document somewhere the refresh script
        //  can find it. Deliberately outside the repository: a test that writes
        //  into the tree is how a corrupted asset once got committed.
        //////////////////////////////////////////////////////////////////////

        static fs::path WriteRegeneratedCopy (const std::string & document)
        {
            std::error_code  ec;
            fs::path         directory = fs::temp_directory_path (ec) / "Casso";
            fs::path         target    = directory / "cycle-reference.md";

            fs::create_directories (directory, ec);

            std::ofstream out (target, std::ios::binary);

            out.write (document.data(), (std::streamsize) document.size());

            return target;
        }


        //////////////////////////////////////////////////////////////////////
        //  FindRepoFile
        //
        //  Walks up from the working directory looking for a repo-relative
        //  path. vstest runs from the repository root and the VS test explorer
        //  runs from the build output, so neither can be assumed.
        //////////////////////////////////////////////////////////////////////

        static fs::path FindRepoFile (const std::wstring & relative)
        {
            std::error_code  ec;
            fs::path         cursor  = fs::current_path (ec);
            fs::path         found;
            bool             walking = !ec;
            int              step    = 0;

            for (step = 0; walking && found.empty() && step < kMaxAncestorWalk; ++step)
            {
                fs::path  candidate = cursor / relative;

                if (fs::exists (candidate, ec))
                {
                    found = candidate;
                }
                else if (!cursor.has_parent_path() || cursor == cursor.parent_path())
                {
                    walking = false;
                }
                else
                {
                    cursor = cursor.parent_path();
                }
            }

            return found;
        }


        //////////////////////////////////////////////////////////////////////
        //  ReadTextFile
        //////////////////////////////////////////////////////////////////////

        static std::string ReadTextFile (const fs::path & path)
        {
            std::ifstream      in (path, std::ios::binary);
            std::stringstream  buffer;

            buffer << in.rdbuf();

            return buffer.str();
        }


        //////////////////////////////////////////////////////////////////////
        //  Normalize
        //
        //  Line endings are not content. The generator emits LF; the tree is
        //  checked out with CRLF on Windows and with LF elsewhere, so comparing
        //  the raw bytes would fail on whichever machine did not write the file.
        //////////////////////////////////////////////////////////////////////

        static std::string Normalize (const std::string & text)
        {
            std::string  stripped = text;

            stripped.erase (std::remove (stripped.begin(), stripped.end(), '\r'), stripped.end());

            return stripped;
        }


        //////////////////////////////////////////////////////////////////////
        //  FindFirstDifferingLine
        //
        //  One-based, so the number can be typed straight into an editor.
        //////////////////////////////////////////////////////////////////////

        static size_t FindFirstDifferingLine (const std::string & left, const std::string & right)
        {
            size_t  index = 0;
            size_t  line  = 1;

            while (index < left.size() && index < right.size() && left[index] == right[index])
            {
                if (left[index] == '\n')
                {
                    ++line;
                }

                ++index;
            }

            return line;
        }


        //////////////////////////////////////////////////////////////////////
        //  GetLine
        //////////////////////////////////////////////////////////////////////

        static std::string GetLine (const std::string & text, size_t oneBasedLine)
        {
            size_t  start   = 0;
            size_t  end     = 0;
            size_t  current = 1;

            while (current < oneBasedLine && start < text.size())
            {
                start = text.find ('\n', start);

                if (start == std::string::npos)
                {
                    return "<past end of file>";
                }

                ++start;
                ++current;
            }

            end = text.find ('\n', start);

            if (end == std::string::npos)
            {
                end = text.size();
            }

            return (start <= text.size()) ? text.substr (start, end - start) : "<past end of file>";
        }


        //////////////////////////////////////////////////////////////////////
        //  Widen
        //////////////////////////////////////////////////////////////////////

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }
    };
}
