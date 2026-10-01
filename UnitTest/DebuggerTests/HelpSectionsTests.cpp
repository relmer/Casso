#include "Pch.h"

#include "Debugger/CassoCommandReference.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/Handlers/ConfigHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HelpSectionsTests
//
//  HELP alone lists the sections and how to ask for each; HELP ALL lists every
//  command; HELP section lists that section alone; any other text is a search
//  of syntax and descriptions, with wildcards or a regular expression.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HelpSectionsTests)
    {
    public:

        using Rig = HandlerRig<ConfigHandlers>;



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }


        //  Every mode, with the line that runs HELP in it.
        static std::vector<std::pair<std::string, std::string>> GetModes()
        {
            return { { "APPLEWIN", "HELP" }, { "MONITOR", "/HELP" }, { "GSSQUARED", "help" }, { "WINDBG", ".help" }, { "CASSO", "help" } };
        }


        //  The lines HELP prints in a mode, with text after it.
        static std::vector<std::string> RunHelp (const std::string & mode, const std::string & help, const std::string & text)
        {
            Rig  rig;



            (void) rig.session.ExecuteLine ("MODE " + mode, CommandMode::AppleWin);
            return rig.RunOk (text.empty() ? help : help + " " + text).text;
        }


        //  The command lines of a listing: those indented four spaces.
        static std::vector<std::string> GetCommandLines (const std::vector<std::string> & lines)
        {
            std::vector<std::string>  commands;



            for (const std::string & line : lines)
            {
                if (line.starts_with ("    "))
                {
                    commands.push_back (line);
                }
            }

            return commands;
        }


        static std::string ToLower (std::string text)
        {
            for (char & ch : text)
            {
                ch = (char) tolower ((unsigned char) ch);
            }

            return text;
        }



        TEST_METHOD (HELP_Alone_ListsTheSectionsNotTheCommands)
        {
            for (const auto & [mode, help] : GetModes())
            {
                std::vector<std::string>  lines = RunHelp (mode, help, "");
                std::string               all;



                for (const std::string & line : lines)
                {
                    all += line + "\n";
                }

                Assert::IsTrue (GetCommandLines (lines).empty(), Widen (mode + ": no command lines").c_str());
                Assert::IsTrue (all.find ("breakpoints") != std::string::npos, Widen (mode + ": the breakpoints section").c_str());
                Assert::IsTrue (all.find (" all ") != std::string::npos,       Widen (mode + ": how to list every command").c_str());
                Assert::IsTrue (lines.size() < 15, Widen (mode + ": one line per section").c_str());
            }
        }


        TEST_METHOD (HELP_All_ListsEveryCommand)
        {
            for (const auto & [mode, help] : GetModes())
            {
                std::vector<std::string>  lines = RunHelp (mode, help, "all");



                Assert::IsTrue (GetCommandLines (lines).size() > 10, Widen (mode).c_str());
            }
        }


        TEST_METHOD (HELP_Section_ListsOnlyThatSection)
        {
            std::vector<std::string>  lines = RunHelp ("APPLEWIN", "HELP", "breakpoints");
            bool                      hasBp = false;



            for (const std::string & line : lines)
            {
                hasBp |= line.starts_with ("    BP ");

                if (line.starts_with ("  ") && !line.starts_with ("    "))
                {
                    Assert::AreEqual (std::string ("  Breakpoints"), line, L"only the breakpoints heading");
                }
            }

            Assert::IsTrue (hasBp);
            Assert::IsFalse (GetCommandLines (lines).empty());
        }


        //  Together the sections list every command HELP ALL lists, and each
        //  lists only its own.
        TEST_METHOD (HELP_Sections_TogetherListEveryCommand)
        {
            const CommandMode  modes[] = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Casso };



            for (CommandMode mode : modes)
            {
                std::multiset<std::string>  all;
                std::multiset<std::string>  sections;



                for (const std::string & line : GetCommandLines (CommandModeHelp::BuildHelp (mode)))
                {
                    all.insert (line.substr (0, line.find_last_not_of (' ') + 1));
                }

                for (int i = (int) HelpCategory::RunningAndStepping; i <= (int) HelpCategory::SessionAndSettings; i++)
                {
                    for (const std::string & line : GetCommandLines (CommandModeHelp::BuildSection (mode, (HelpCategory) i)))
                    {
                        sections.insert (line.substr (0, line.find_last_not_of (' ') + 1));
                    }
                }

                Assert::AreEqual (all.size(), sections.size(), Widen (CommandModeHelp::GetTitle (mode)).c_str());
            }
        }


        //  WATCH is GSSquared's own command, so there it is described rather
        //  than searched for; every other mode searches.
        TEST_METHOD (HELP_Search_MatchesSyntaxOrDescriptionIgnoringCase)
        {
            for (const auto & [mode, help] : GetModes())
            {
                std::vector<std::string>  lines = GetCommandLines (RunHelp (mode, help, mode == "GSSQUARED" ? "BREAKPOINT" : "WATCH"));
                std::string               word  = mode == "GSSQUARED" ? "breakpoint" : "watch";



                Assert::IsFalse (lines.empty(), Widen (mode).c_str());

                for (const std::string & line : lines)
                {
                    Assert::IsTrue (ToLower (line).find (word) != std::string::npos, Widen (line).c_str());
                }
            }
        }


        TEST_METHOD (HELP_Search_TakesWildcardsAndRegex)
        {
            std::vector<std::string>  wild  = GetCommandLines (RunHelp ("APPLEWIN", "HELP", "dis?ble*point"));
            std::vector<std::string>  regex = GetCommandLines (RunHelp ("APPLEWIN", "HELP", "/^bp[de]/"));



            Assert::IsFalse (wild.empty(), L"? and * match");
            Assert::IsFalse (regex.empty(), L"a regular expression matches");

            for (const std::string & line : regex)
            {
                std::string  syntax = ToLower (line.substr (4));

                Assert::IsTrue (syntax.starts_with ("bpd") || syntax.starts_with ("bpe"), Widen (line).c_str());
            }
        }


        TEST_METHOD (HELP_Search_NoMatch_SaysSo)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("No command matches zqxj."), rig.RunOk ("HELP zqxj").text.at (0));
            Assert::AreEqual (std::string ("No command matches /zq[x]j/."), rig.RunOk ("HELP /zq[x]j/").text.at (0));
            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("HELP /bp[/").status, L"a regular expression that does not compile");
        }


        //  A word that is both a command and a section is described as the
        //  command, then followed by how to ask for the section.
        TEST_METHOD (HELP_WordThatIsBothCommandAndSection_DescribesTheCommand)
        {
            const CommandMode  modes[] = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Casso };
            const char       * words[] = { "running", "breakpoints", "registers", "memory", "disassembly", "symbols", "disks", "display", "session" };
            int                checked = 0;



            for (CommandMode mode : modes)
            {
                for (const char * word : words)
                {
                    std::string               described;
                    std::vector<std::string>  lines;
                    std::string               error;

                    if (!CommandModeHelp::TryDescribe (mode, word, described))
                    {
                        continue;
                    }

                    Assert::IsTrue  (CommandModeHelp::TryBuildWordHelp (mode, word, lines, error));
                    Assert::AreEqual ((size_t) 2, lines.size(), Widen (word).c_str());
                    Assert::AreEqual (described, lines[0]);
                    Assert::IsTrue  (lines[1].find (word) != std::string::npos);
                    checked++;
                }
            }

            {
                std::vector<std::string>  lines;
                std::string               error;

                Assert::IsTrue (CommandModeHelp::TryBuildWordHelp (CommandMode::AppleWin, "Memory", lines, error));
                Assert::IsTrue (lines.size() > 2 || checked > 0, L"a section word with no command lists the section");
            }
        }
    };
}
