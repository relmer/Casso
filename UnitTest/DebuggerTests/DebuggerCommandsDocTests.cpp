#include "Pch.h"

#include "Debugger/CommandModeHelp.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandsDocTests
//
//  The guard on docs/Debugger-Commands.md. It regenerates the reference from
//  the help table every mode's HELP ALL reads and compares it with the copy
//  in the tree. The regenerated copy goes to %TEMP%\Casso, where
//  scripts/UpdateDebuggerCommands.ps1 picks it up; the test never writes into
//  the tree. A missing document fails rather than skipping.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerCommandsDocTests)
    {
    public:

        static constexpr int      kMaxAncestorWalk  = 10;
        static constexpr wchar_t  kDocumentPath[]   = L"docs/Debugger-Commands.md";
        static constexpr wchar_t  kRefreshCommand[] = L"pwsh scripts/UpdateDebuggerCommands.ps1";



        TEST_METHOD (Document_MatchesHelp)
        {
            std::string  generated = Normalize (CommandModeHelp::BuildReference());
            fs::path     committed = FindRepoFile (kDocumentPath);
            std::string  onDisk;
            size_t       line      = 0;



            WriteRegeneratedCopy (generated);

            Assert::IsFalse (committed.empty(), std::format (L"{} was not found. Regenerate with: {}", kDocumentPath, kRefreshCommand).c_str());

            onDisk = Normalize (ReadTextFile (committed));

            if (generated == onDisk)
            {
                return;
            }

            line = FindFirstDifferingLine (onDisk, generated);

            Assert::Fail (std::format (L"{} no longer matches help; the first difference is at line {}. Regenerate it with: {}",
                                       kDocumentPath, line, kRefreshCommand).c_str());
        }



        //  A generator that produced nothing would match an empty document.
        TEST_METHOD (Document_ListsEveryModesCommands)
        {
            std::string        generated = CommandModeHelp::BuildReference();
            const CommandMode  modes[]   = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Casso };



            for (CommandMode mode : modes)
            {
                Assert::IsTrue (generated.find (std::format ("## {} mode", CommandModeHelp::GetTitle (mode))) != std::string::npos);

                for (const std::string & line : CommandModeHelp::BuildHelp (mode))
                {
                    Assert::IsTrue (generated.find (line) != std::string::npos);
                }
            }
        }


    private:

        static void WriteRegeneratedCopy (const std::string & document)
        {
            std::error_code  ec;
            fs::path         directory = fs::temp_directory_path (ec) / "Casso";



            fs::create_directories (directory, ec);

            std::ofstream  out (directory / "Debugger-Commands.md", std::ios::binary);

            out.write (document.data(), (std::streamsize) document.size());
        }


        static fs::path FindRepoFile (const std::wstring & relative)
        {
            std::error_code  ec;
            fs::path         cursor = fs::current_path (ec);



            for (int step = 0; !ec && step < kMaxAncestorWalk; ++step)
            {
                if (fs::exists (cursor / relative, ec))
                {
                    return cursor / relative;
                }

                if (!cursor.has_parent_path() || cursor == cursor.parent_path())
                {
                    break;
                }

                cursor = cursor.parent_path();
            }

            return {};
        }


        static std::string ReadTextFile (const fs::path & path)
        {
            std::ifstream      in (path, std::ios::binary);
            std::stringstream  buffer;



            buffer << in.rdbuf();
            return buffer.str();
        }


        static std::string Normalize (const std::string & text)
        {
            std::string  stripped = text;



            stripped.erase (std::remove (stripped.begin(), stripped.end(), '\r'), stripped.end());
            return stripped;
        }


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
    };
}
