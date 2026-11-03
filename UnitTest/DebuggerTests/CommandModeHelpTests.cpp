#include "Pch.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/CassoCommandReference.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/WinDbgParser.h"
#include "HandlerTestRig.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"
#include "Core/TextEncoding.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CommandModeHelpTests
    //
    //  Help lists only what can be typed in the mode in force, written as it is
    //  typed there, by category and alphabetical within each; every Casso
    //  command it lists runs through the mode's parser; Casso mode runs every
    //  command by bare name.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CommandModeHelpTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }


        //  The lines after "Casso commands:", or none.
        static std::vector<std::string> GetCassoSection (const std::vector<std::string> & lines)
        {
            auto  heading = std::find (lines.begin(), lines.end(), std::string ("Casso commands:"));



            return (heading == lines.end()) ? std::vector<std::string>() : std::vector<std::string> (heading + 1, lines.end());
        }


        //  Whether a command line of help begins with the name, as a whole word.
        static bool ListsName (const std::vector<std::string> & lines, const std::string & name)
        {
            for (const std::string & line : lines)
            {
                size_t  first = line.find_first_not_of (' ');

                if (first != std::string::npos && line.compare (first, name.size(), name) == 0 &&
                    (first + name.size() == line.size() || line[first + name.size()] == ' ' || line[first + name.size()] == ','))
                {
                    return true;
                }
            }

            return false;
        }



        TEST_METHOD (GSSquaredHelp_ListsDiskAndLeavesOutWhatItHasOrUses)
        {
            std::vector<std::string>  casso = GetCassoSection (CommandModeHelp::BuildHelp (CommandMode::GSSquared));



            Assert::IsFalse (casso.empty());
            Assert::IsTrue  (ListsName (casso, "DISK"),   L"no disk command in GSSquared");
            Assert::IsFalse (ListsName (casso, "R"),      L"r is GSSquared's step out");
            Assert::IsFalse (ListsName (casso, "BP"),     L"GSSquared has bp");
            Assert::IsFalse (ListsName (casso, "DB"),     L"a hex word is an address to GSSquared");
            Assert::IsTrue  (ListsName (casso, "SWITCHES"), L"an engine command");
        }


        //  Every mode's help lists BPDISK, as that mode writes it, under its
        //  Breakpoints heading: a heading is indented two spaces, a command
        //  four.
        TEST_METHOD (EveryModesHelpListsBpdiskUnderBreakpoints)
        {
            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg })
            {
                std::string  heading;
                std::string  under;



                for (const std::string & line : CommandModeHelp::BuildHelp (mode))
                {
                    std::string  upper = line;



                    std::transform (upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return (char) toupper (c); });

                    heading = (line.starts_with ("  ") && !line.starts_with ("    ")) ? line.substr (2) : heading;
                    under   = (under.empty() && upper.find ("BPDISK") != std::string::npos) ? heading : under;
                }

                Assert::AreEqual (std::string ("Breakpoints"), under, Widen (std::format ("mode {}", (int) mode)).c_str());
            }
        }


        TEST_METHOD (DiskRunsInGSSquaredAsListed)
        {
            MockExpressionContext  context;
            GSSquaredParseResult   parsed = GSSquaredParser::Parse ("disk", context);



            Assert::IsTrue   (parsed.status != ParseStatus::Unknown, Widen (parsed.error).c_str());
            Assert::AreEqual ((int) DebugVerb::DiskCommand, (int) parsed.commands.front().verb);
        }


        TEST_METHOD (WinDbgHelp_WritesCassoCommandsWithItsMarker)
        {
            std::vector<std::string>  casso = GetCassoSection (CommandModeHelp::BuildHelp (CommandMode::WinDbg));



            Assert::IsTrue  (ListsName (casso, "!DISK"));
            Assert::IsFalse (ListsName (casso, "!BPL"), L"WinDbg lists bl instead");
        }


        //  Every WinDbg-mode command has its own help entry, found by its name.
        TEST_METHOD (EveryWinDbgCommand_HasItsOwnHelpEntry)
        {
            for (const WinDbgCommand & command : WinDbgParser::GetCommands())
            {
                Assert::IsNotNull (CommandModeHelp::Find (CommandMode::WinDbg, command.name), Widen (command.name).c_str());
            }
        }


        //  SC-031: every Casso command a mode's help lists runs as listed.
        TEST_METHOD (EveryListedCassoCommand_RunsThroughTheModesParser)
        {
            MockExpressionContext  context;
            size_t                 checked = 0;



            for (const CassoCommandReference::Entry & entry : CassoCommandReference::GetAll())
            {
                std::vector<std::string>  names = { entry.name };

                for (const AppleWinCommand & alias : AppleWinCommandTable::GetAll())
                {
                    if (alias.aliasOf != nullptr && _stricmp (alias.aliasOf, entry.name) == 0)
                    {
                        names.push_back (alias.name);
                    }
                }

                for (const std::string & name : names)
                {
                    if (CommandModeHelp::IsCassoCommandReachable (CommandMode::GSSquared, name))
                    {
                        Assert::IsTrue (GSSquaredParser::Parse (name, context).status != ParseStatus::Unknown, Widen ("GSSquared " + name).c_str());
                        ++checked;
                    }

                    if (CommandModeHelp::IsCassoCommandReachable (CommandMode::WinDbg, name))
                    {
                        Assert::IsTrue (WinDbgParser::Parse ("!" + name, context).status != ParseStatus::Unknown, Widen ("WinDbg !" + name).c_str());
                        ++checked;
                    }

                    if (CommandModeHelp::IsCassoCommandReachable (CommandMode::Monitor, name))
                    {
                        MonitorState        state;
                        MonitorParseResult  monitor = MonitorParser::Parse ("/" + name, state);

                        Assert::AreEqual (name, monitor.appleWinLine, Widen ("Monitor /" + name).c_str());
                        ++checked;
                    }
                }
            }

            Assert::IsTrue (checked > 100);
        }


        TEST_METHOD (EveryWinDbgCommand_HasAHelpEntry)
        {
            size_t  checked = 0;



            for (const WinDbgCommand & command : WinDbgParser::GetCommands())
            {
                Assert::IsNotNull (CommandModeHelp::Find (CommandMode::WinDbg, command.name), Widen (command.name).c_str());
                ++checked;
            }

            Assert::IsTrue (checked > 0);
        }


        TEST_METHOD (MonitorHelp_ListsArithmetic)
        {
            std::vector<std::string>  lines = CommandModeHelp::BuildHelp (CommandMode::Monitor);



            Assert::IsTrue (ListsName (lines, "a+b"));
            Assert::IsTrue (ListsName (lines, "a-b"));
        }


        //  One long syntax doesn't push every description right: a syntax
        //  wider than the column is a row by itself, with its description on
        //  the next row in the description column.
        TEST_METHOD (ALongSyntaxIsARowByItself)
        {
            constexpr size_t           kDescriptionColumn = 4 + 24 + 2;
            std::vector<std::string>   lines              = CommandModeHelp::BuildHelp (CommandMode::Casso);
            size_t                     longOnes           = 0;



            for (size_t i = 0; i + 1 < lines.size(); i++)
            {
                const std::string  & line = lines[i];
                const std::string  & next = lines[i + 1];



                if (!line.starts_with ("    ") || line[4] == ' ' || line.find ("  ", 4) != std::string::npos)
                {
                    continue;
                }

                longOnes++;
                Assert::IsTrue (next.size() > kDescriptionColumn && next.find_first_not_of (' ') == kDescriptionColumn,
                                TextEncoding::NarrowToWide (line + " | " + next).c_str());
            }

            Assert::IsTrue (longOnes > 0, L"PROFILE's syntax is longer than the column");
        }


        //  Where the console fits every row unwrapped, the syntax column is
        //  the widest syntax; where it doesn't, or the width is unknown, the
        //  column is capped.
        TEST_METHOD (TheSyntaxColumnIsUncappedOnlyWhereEveryRowFits)
        {
            Assert::AreEqual ((size_t) 30, CommandModeHelp::GetSyntaxColumn (30, 60, 4 + 30 + 2 + 60), L"every row fits");
            Assert::AreEqual (CommandModeHelp::kMaxSyntaxColumn, CommandModeHelp::GetSyntaxColumn (30, 60, 4 + 30 + 2 + 59), L"one column short");
            Assert::AreEqual (CommandModeHelp::kMaxSyntaxColumn, CommandModeHelp::GetSyntaxColumn (30, 60, 0), L"no console width");
            Assert::AreEqual ((size_t) 10, CommandModeHelp::GetSyntaxColumn (10, 60, 0), L"a column narrower than the cap");
        }


        //  The console lays a listing's tabbed rows out again at each width:
        //  wide enough, every syntax in one column; narrower, the column is
        //  capped and a longer syntax is a row by itself.
        TEST_METHOD (TabbedRowsAreLaidOutAtTheConsolesWidth)
        {
            std::string               longSyntax (30, 'S');
            std::vector<std::string>  lines = { "Commands:", "    BP addr\tSet a breakpoint", "    " + longSyntax + "\tA long one" };
            std::vector<std::string>  wide;
            std::vector<std::string>  narrow;



            {
                CommandModeHelp::TabbedRows  tabbed (true);

                Assert::IsTrue (CommandModeHelp::BuildHelp (CommandMode::Casso)[2].find ('\t') != std::string::npos, L"rows are tabbed in scope");
            }

            Assert::IsTrue (CommandModeHelp::BuildHelp (CommandMode::Casso)[2].find ('\t') == std::string::npos, L"and laid out out of it");

            wide   = CommandModeHelp::LayOutTabbedRows (lines, 200);
            narrow = CommandModeHelp::LayOutTabbedRows (lines, 50);

            Assert::AreEqual ((size_t) 3, wide.size());
            Assert::AreEqual (std::string ("Commands:"), wide[0], L"a line without a tab is as it was");
            Assert::AreEqual ("    " + std::string ("BP addr") + std::string (30 - 7, ' ') + "  Set a breakpoint", wide[1]);

            Assert::AreEqual ((size_t) 4, narrow.size(), L"the long syntax is a row by itself");
            Assert::AreEqual ("    BP addr" + std::string (CommandModeHelp::kMaxSyntaxColumn - 7, ' ') + "  Set a breakpoint", narrow[1]);
            Assert::AreEqual ("    " + longSyntax, narrow[2]);
            Assert::AreEqual (std::string (4 + CommandModeHelp::kMaxSyntaxColumn + 2, ' ') + "A long one", narrow[3]);
        }


        //  Each line laid out has the index of the line it came from, so the
        //  two lines of a long syntax's row have one index.
        TEST_METHOD (EachLaidOutLineHasTheIndexOfItsSource)
        {
            std::vector<std::string>  lines = { "Commands:", "    BP addr\tSet a breakpoint", "    " + std::string (30, 'S') + "\tA long one" };
            std::vector<size_t>       sources;
            std::vector<std::string>  narrow;



            narrow = CommandModeHelp::LayOutTabbedRows (lines, 50, sources);

            Assert::AreEqual ((size_t) 4, narrow.size());
            Assert::IsTrue   ((std::vector<size_t> { 0, 1, 2, 2 }) == sources, L"the long syntax's two lines are from the third");
        }


        TEST_METHOD (HelpIsByCategory_AndAlphabeticalWithinEach)
        {
            std::vector<std::string>  lines    = CommandModeHelp::BuildHelp (CommandMode::Casso);
            std::string               previous;



            Assert::IsFalse (lines.empty());
            Assert::AreEqual (std::string ("Casso commands:"), lines.front(), L"Casso mode has no separate section");

            for (const std::string & line : lines)
            {
                bool  isHeading = line.size() > 2 && line.compare (0, 2, "  ") == 0 && line[2] != ' ';

                if (isHeading)
                {
                    previous.clear();
                    continue;
                }

                //  A description on a row of its own, deeper in, follows a
                //  syntax too wide for the column.
                if (line.compare (0, 4, "    ") == 0 && line[4] != ' ')
                {
                    std::string  syntax = line.substr (4);

                    Assert::IsTrue (previous.empty() || _stricmp (previous.c_str(), syntax.c_str()) <= 0, Widen (previous + " before " + syntax).c_str());
                    previous = syntax;
                }
            }
        }


        TEST_METHOD (AppleWinHelp_SetsCassosAdditionsApart)
        {
            std::vector<std::string>  lines = CommandModeHelp::BuildHelp (CommandMode::AppleWin);
            std::vector<std::string>  casso = GetCassoSection (lines);



            Assert::AreEqual (std::string ("AppleWin commands:"), lines.front());
            Assert::IsTrue   (ListsName (casso, "MODE"),  L"an engine command is Casso's");
            Assert::IsFalse  (ListsName (casso, "BPL"),   L"BPL is AppleWin's own");
        }


        TEST_METHOD (HelpForACommandTheModeCannotRun_SaysWhereItRuns)
        {
            std::string  line;



            Assert::IsTrue  (CommandModeHelp::TryDescribe (CommandMode::GSSquared, "DB", line));
            Assert::IsTrue  (line.find ("does not run in GSSquared mode") != std::string::npos, Widen (line).c_str());
            Assert::IsTrue  (line.find ("AppleWin") != std::string::npos, Widen (line).c_str());

            Assert::IsTrue  (CommandModeHelp::TryDescribe (CommandMode::GSSquared, "disk", line));
            Assert::IsTrue  (line.find ("does not run") == std::string::npos, Widen (line).c_str());

            Assert::IsFalse (CommandModeHelp::TryDescribe (CommandMode::GSSquared, "FROB", line));
        }


        TEST_METHOD (HelpForTheMarkerFormHelpLists_DescribesTheCommand)
        {
            std::string  line;



            Assert::IsTrue (CommandModeHelp::TryDescribe (CommandMode::WinDbg, "!bpm", line));
            Assert::IsTrue (line.starts_with ("!BPM"), Widen (line).c_str());

            Assert::IsTrue (CommandModeHelp::TryDescribe (CommandMode::Monitor, "/DISK", line));
            Assert::IsTrue (line.starts_with ("/DISK"), Widen (line).c_str());
        }


        //  GSSquared's own addr and first.last forms are D, so its help for D
        //  gives those forms rather than saying D does not run there.
        TEST_METHOD (HelpForACommandTheModeWritesItsOwnWay_GivesTheModesForms)
        {
            std::string  line;



            Assert::IsTrue (CommandModeHelp::TryDescribe (CommandMode::GSSquared, "d", line));
            Assert::IsTrue (line.find ("does not run") == std::string::npos, Widen (line).c_str());
            Assert::IsTrue (line.find ("first.last")   != std::string::npos, Widen (line).c_str());
        }


        TEST_METHOD (WinDbgHelp_DescribesWhatSourceModeAndRegisterSetDo)
        {
            std::string  line;



            Assert::IsTrue (CommandModeHelp::TryDescribe (CommandMode::WinDbg, "l+s", line));
            Assert::IsTrue (line.find ("Step by source line") != std::string::npos, Widen (line).c_str());

            Assert::IsTrue (CommandModeHelp::TryDescribe (CommandMode::WinDbg, "l-s", line));
            Assert::IsTrue (line.find ("Step by instruction") != std::string::npos, Widen (line).c_str());

            Assert::IsTrue (CommandModeHelp::TryDescribe (CommandMode::WinDbg, "r", line));
            Assert::IsTrue (line.find ("r [reg[=value]]") != std::string::npos, Widen (line).c_str());
        }


        TEST_METHOD (CassoMode_RunsEveryCommandByBareName_InAppleWinsFormat)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;
            Reply                      reply;



            handlers.Attach (session);
            reply = session.ExecuteLine ("MODE CASSO", CommandMode::AppleWin);

            Assert::IsTrue   (reply.status == CommandStatus::Ok, Widen (reply.error.detail).c_str());
            Assert::AreEqual ((int) CommandMode::Casso, (int) session.GetMode());

            reply = session.ExecuteLine ("BPL");
            Assert::IsTrue (reply.status == CommandStatus::Ok, Widen (reply.error.detail).c_str());

            reply = session.ExecuteLine ("SWITCHES");
            Assert::IsTrue (reply.status == CommandStatus::Ok, Widen (reply.error.detail).c_str());
        }
    };
}
