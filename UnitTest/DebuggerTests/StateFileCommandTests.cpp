#include "Pch.h"

#include "Debugger/CassoCommandReference.h"
#include "Debugger/DebugHandlerSet.h"
#include "HandlerTestRig.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StateFileCommandTests
//
//  SAVESTATE and LOADSTATE hand the host a state file to save the machine to
//  or load it from, in every dialect through its Casso-command form, and
//  report what they started.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (StateFileCommandTests)
    {
    public:

        struct Taken
        {
            std::optional<StateFileRequest>  request;
            std::wstring                     path;
            int                              calls = 0;
        };


        static void Hook (DebugSession & session, Taken & taken)
        {
            session.SetStateFileRequester ([&taken] (StateFileRequest request, const std::wstring & path)
            {
                taken.request = request;
                taken.path    = path;
                ++taken.calls;
                return true;
            });
        }



        TEST_METHOD (EveryDialectReachesBothCommands)
        {
            struct Case
            {
                const char        * mode;
                const char        * line;
                StateFileRequest    request;
            };

            static constexpr Case  kCases[] =
            {
                { "APPLEWIN",  "SAVESTATE game.cassostate",  StateFileRequest::Save },
                { "CASSO",     "LOADSTATE game.cassostate",  StateFileRequest::Load },
                { "MONITOR",   "/SAVESTATE game.cassostate", StateFileRequest::Save },
                { "MONITOR",   "/LOADSTATE game.cassostate", StateFileRequest::Load },
                { "WINDBG",    "!savestate game.cassostate", StateFileRequest::Save },
                { "WINDBG",    "!loadstate game.cassostate", StateFileRequest::Load },
                { "GSSQUARED", "savestate game.cassostate",  StateFileRequest::Save },
                { "GSSQUARED", "loadstate game.cassostate",  StateFileRequest::Load },
            };



            for (const Case & each : kCases)
            {
                MockDebugTarget            target;
                RecordingNotificationSink  sink;
                DebugSession               session (target, sink, RunState::Paused);
                DebugHandlerSet            handlers;
                Taken                      taken;
                Reply                      reply;
                std::wstring               label (each.line, each.line + strlen (each.line));



                handlers.Attach (session);
                Hook (session, taken);
                session.ExecuteLine (std::string ("MODE ") + each.mode, CommandMode::AppleWin);

                reply = session.ExecuteLine (each.line);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, (label + L": " + std::wstring (reply.error.detail.begin(), reply.error.detail.end())).c_str());
                Assert::AreEqual (1, taken.calls, label.c_str());
                Assert::IsTrue   (taken.request == each.request, label.c_str());
                Assert::IsTrue   (taken.path.ends_with (L"game.cassostate"), taken.path.c_str());
            }
        }



        TEST_METHOD (TheFileNameKeepsItsCaseAndLosesItsQuotes)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;
            Taken                      taken;
            Reply                      reply;



            handlers.Attach (session);
            Hook (session, taken);

            reply = session.ExecuteLine ("SAVESTATE \"My Game.cassostate\"", CommandMode::AppleWin);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::IsTrue   (taken.path.ends_with (L"My Game.cassostate"), taken.path.c_str());
            Assert::IsFalse  (taken.path.find (L'"') != std::wstring::npos, taken.path.c_str());
        }



        TEST_METHOD (AMissingFileNameIsRefusedAndRequestsNothing)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;
            Taken                      taken;



            handlers.Attach (session);
            Hook (session, taken);

            for (const char * line : { "SAVESTATE", "LOADSTATE" })
            {
                Reply  reply = session.ExecuteLine (line, CommandMode::AppleWin);



                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
                Assert::IsTrue   (reply.error.detail.find ("needs a file name") != std::string::npos);
            }

            Assert::AreEqual (0, taken.calls);
        }



        TEST_METHOD (WithoutTheEmulatorTheCommandsAreNotAvailable)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;
            Reply                      reply;



            handlers.Attach (session);

            reply = session.ExecuteLine ("LOADSTATE game.cassostate", CommandMode::AppleWin);

            Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status);
        }



        TEST_METHOD (BothCommandsHaveHelpRows)
        {
            for (const char * name : { "SAVESTATE", "LOADSTATE" })
            {
                const CassoCommandReference::Entry  * entry = CassoCommandReference::Find (name);



                Assert::IsNotNull (entry);
                Assert::IsTrue    (std::string (entry->syntax).ends_with (" file"));
            }
        }
    };
}
