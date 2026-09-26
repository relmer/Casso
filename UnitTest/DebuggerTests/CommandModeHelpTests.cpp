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
            return std::wstring (text.begin(), text.end());
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

                if (line.compare (0, 4, "    ") == 0)
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
