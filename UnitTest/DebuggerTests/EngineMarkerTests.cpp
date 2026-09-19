#include "Pch.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/MonitorParser.h"
#include "HandlerTestRig.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EngineMarkerTests
    //
    //  FR-014 and R-037: Casso's engine commands, the Engine family of
    //  AppleWinCommandTable, are reachable in every mode through that mode's
    //  own marker -- a bare name in AppleWin and GSSquared modes, `/` in
    //  Monitor mode -- and mean the same thing in each.
    //
    //  THE TESTS WALK THE TABLE, NOT A LIST OF NAMES, so a command added to
    //  the Engine family is covered here, and reachable in every mode, with
    //  no change to any parser: each parser hands its marker-stripped line to
    //  AppleWinParser. WinDbg mode's `!` joins these rows when that mode
    //  exists.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EngineMarkerTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }

        static std::vector<std::string> GetEngineNames()
        {
            std::vector<std::string>  names;



            for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
            {
                if (command.family == AppleWinCommandFamily::Engine)
                {
                    names.push_back (command.name);
                }
            }

            return names;
        }

        //  The one line each engine command is tried with: the bare name,
        //  which every one of them accepts except those that need arguments.
        static std::string GetSampleLine (const std::string & name)
        {
            if (name == "BUDGET") { return "BUDGET 1000"; }
            if (name == "PATCH")  { return "PATCH 300 EA"; }
            if (name == "MODE")   { return "MODE APPLEWIN"; }
            if (name == "OUTPUT") { return "OUTPUT APPLEWIN"; }
            return name;
        }



        //  The commands FR-014 names, as far as they exist yet; PANEL,
        //  HISTORY and CALLS join the family when they are built.
        TEST_METHOD (TheEngineFamily_HoldsTheEngineCommands)
        {
            std::vector<std::string>  names = GetEngineNames();



            for (const char * name : { "MODE", "PAUSE", "BUDGET", "SWITCHES", "STACK", "PATCH", "SRC", "OUTPUT", "PROFILE", "SKIP" })
            {
                Assert::IsTrue (std::find (names.begin(), names.end(), name) != names.end(), Widen (name).c_str());
                Assert::IsTrue (AppleWinCommandTable::IsEngineCommand (name), Widen (name).c_str());
            }

            Assert::IsFalse (AppleWinCommandTable::IsEngineCommand ("BP"));
            Assert::IsFalse (AppleWinCommandTable::IsEngineCommand ("FROB"));
        }

        //  Each mode's marker reaches the same command AppleWin mode parses.
        TEST_METHOD (EveryEngineCommand_IsReachableInEveryMode_WithItsMarker)
        {
            MockExpressionContext  context;
            size_t                 checked = 0;



            for (const std::string & name : GetEngineNames())
            {
                std::string           line      = GetSampleLine (name);
                AppleWinParseResult   appleWin  = AppleWinParser::Parse (line, context);
                MonitorState          state;
                MonitorParseResult    monitor   = MonitorParser::Parse ("/" + line, state);
                GSSquaredParseResult  gssquared = GSSquaredParser::Parse (line, context);
                GSSquaredParseResult  lowered   = GSSquaredParser::Parse (ToLower (line), context);
                AppleWinParseResult   viaSlash  = AppleWinParser::Parse (monitor.appleWinLine, context);



                Assert::IsTrue (appleWin.status == ParseStatus::Ok, Widen (line + ": " + appleWin.error).c_str());

                Assert::AreEqual (line, monitor.appleWinLine, Widen ("/" + line).c_str());
                Assert::IsTrue   (viaSlash.status == ParseStatus::Ok, Widen ("/" + line).c_str());
                Assert::AreEqual ((int) appleWin.command.verb, (int) viaSlash.command.verb, Widen ("/" + line).c_str());

                Assert::IsTrue   (gssquared.status == ParseStatus::Ok, Widen (line + ": " + gssquared.error).c_str());
                Assert::AreEqual ((int) appleWin.command.verb, (int) gssquared.commands.front().verb, Widen (line).c_str());
                Assert::IsTrue   (lowered.status == ParseStatus::Ok, Widen (ToLower (line)).c_str());

                ++checked;
            }

            Assert::IsTrue (checked >= 10);
        }

        //  The same command through each mode's marker leaves the same reply
        //  kind, executed end to end.
        TEST_METHOD (EveryEngineCommand_RunsTheSameInEveryMode)
        {
            for (const std::string & name : GetEngineNames())
            {
                std::string  line = GetSampleLine (name);
                Reply        replies[3];
                int          i    = 0;



                for (const char * mode : { "APPLEWIN", "MONITOR", "GSSQUARED" })
                {
                    MockDebugTarget            target;
                    RecordingNotificationSink  sink;
                    DebugSession               session (target, sink, RunState::Paused);
                    DebugHandlerSet            handlers;
                    std::string                typed = (std::string (mode) == "MONITOR") ? "/" + line : line;



                    handlers.Attach (session);
                    session.ExecuteLine (std::string ("MODE ") + mode, CommandMode::AppleWin);
                    replies[i++] = session.ExecuteLine (typed);
                }

                for (const Reply & reply : replies)
                {
                    Assert::AreEqual ((int) replies[0].status,       (int) reply.status,       Widen (line).c_str());
                    Assert::AreEqual (replies[0].data.index(),       reply.data.index(),       Widen (line).c_str());
                }

                Assert::AreEqual ((int) CommandStatus::Ok, (int) replies[0].status, Widen (line + ": " + replies[0].error.detail).c_str());
            }
        }

        //  `/` is GSSquared's bank separator, so it is no marker there, and
        //  no GSSquared word is an engine command's name.
        TEST_METHOD (GSSquaredWords_AndEngineNames_DoNotCollide)
        {
            MockExpressionContext  context;



            for (const GSSquaredCommand & command : GSSquaredParser::GetCommands())
            {
                Assert::IsFalse (AppleWinCommandTable::IsEngineCommand (command.name), Widen (command.name).c_str());
            }

            Assert::IsTrue (GSSquaredParser::Parse ("/switches", context).status == ParseStatus::Unknown);
        }

        static std::string ToLower (const std::string & text)
        {
            std::string  lower (text);



            for (char & ch : lower)
            {
                ch = (char) tolower ((unsigned char) ch);
            }

            return lower;
        }
    };
}
