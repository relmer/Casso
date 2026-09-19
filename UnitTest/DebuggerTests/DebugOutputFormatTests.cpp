#include "Pch.h"

#include "Cli/DebugBatchSink.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebuggerController.h"
#include "HandlerTestRig.h"
#include "Ui/Debugger/DebuggerViewState.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugOutputFormatTests
    //
    //  FR-013 and data-model OutputFormat: replies are written in a format of
    //  their own, which MODE sets to the mode's and OUTPUT changes alone, and
    //  every reply is rendered by it -- a stop included -- however the line
    //  arrived: batch, the channel, or the window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugOutputFormatTests)
    {
    public:
        //  Every command family over the mock target, with $0300 holding
        //  known bytes.
        struct Rig
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;

            Rig()
            {
                handlers.Attach (session);
                target.memory[0x300] = 0xA9;
                target.memory[0x301] = 0x41;
            }

            Reply Run (const std::string & line)
            {
                Reply  reply = session.ExecuteLine (line);



                session.FormatReply (reply);
                return reply;
            }
        };



        TEST_METHOD (Mode_SetsTheModeAndTheFormat)
        {
            Rig  rig;



            rig.Run ("MODE GSSQUARED");
            Assert::IsTrue (rig.session.GetMode()         == CommandMode::GSSquared);
            Assert::IsTrue (rig.session.GetOutputFormat() == OutputFormat::GSSquared);

            rig.Run ("mode monitor");
            Assert::IsTrue (rig.session.GetMode()         == CommandMode::Monitor);
            Assert::IsTrue (rig.session.GetOutputFormat() == OutputFormat::Monitor);
        }

        TEST_METHOD (Output_SetsTheFormatAlone_AndReportsIt)
        {
            Rig    rig;
            Reply  shown;



            rig.Run ("MODE GSSQUARED");
            Assert::AreEqual (std::string ("Output: APPLEWIN"), rig.Run ("output applewin").text.front());

            Assert::IsTrue (rig.session.GetMode()         == CommandMode::GSSquared);
            Assert::IsTrue (rig.session.GetOutputFormat() == OutputFormat::AppleWin);

            shown = rig.Run ("output");
            Assert::AreEqual (std::string ("Output: APPLEWIN"), shown.text.front());

            Assert::IsTrue (rig.Run ("OUTPUT BASIC").status == CommandStatus::Error);
            Assert::IsTrue (rig.session.GetOutputFormat() == OutputFormat::AppleWin);
        }

        //  US12 scenario 3: a GSSquared line in the AppleWin format, until the
        //  mode changes and takes the format with it.
        TEST_METHOD (EveryReply_IsRenderedByTheCurrentFormat)
        {
            Rig    rig;
            Reply  gssquared;
            Reply  appleWin;
            Reply  monitor;



            rig.Run ("MODE GSSQUARED");
            gssquared = rig.Run ("300.301");

            rig.Run ("OUTPUT APPLEWIN");
            appleWin = rig.Run ("300.301");

            rig.Run ("OUTPUT MONITOR");
            monitor = rig.Run ("300.301");

            Assert::IsTrue (gssquared.text.front().starts_with ("00/0300: A9 41"), Widen (gssquared.text.front()).c_str());
            Assert::IsTrue (appleWin.text.front().starts_with  ("0300: A9 41"),    Widen (appleWin.text.front()).c_str());
            Assert::IsTrue (monitor.text.front().starts_with   ("0300- A9 41"),    Widen (monitor.text.front()).c_str());
        }

        //  A stop is a reply too: batch prints it in the output format, not
        //  in the mode's.
        TEST_METHOD (Stops_AreRenderedByTheCurrentFormat)
        {
            MockDebugTarget  target;
            DebugBatchSink   sink;
            DebugSession     session (target, sink, RunState::Paused);
            StopEvent        stop;
            std::string      appleWin;
            std::string      monitor;



            sink.SetSession (&session);
            stop.reason = StopReason::Step;
            stop.pc     = 0x302;

            sink.OnStopped (stop);
            appleWin = sink.TakePending();

            session.SetOutputFormat (OutputFormat::Monitor);
            sink.OnStopped (stop);
            monitor = sink.TakePending();

            Assert::AreEqual (std::string ("Step at $0302\n"), appleWin);
            Assert::IsTrue   (monitor.starts_with ("A="), Widen (monitor).c_str());

            session.SetOutputFormat (OutputFormat::GSSquared);
            sink.OnStopped (stop);
            Assert::AreEqual (std::string ("Step at $0302\n"), sink.TakePending());
            sink.SetSession (nullptr);
        }

        //  The window runs lines in the session's mode and renders them in the
        //  session's format.
        TEST_METHOD (TheWindow_RendersInTheCurrentFormat)
        {
            Rig    rig;
            Reply  reply;



            rig.Run ("MODE GSSQUARED");
            rig.Run ("OUTPUT MONITOR");

            reply = DebuggerViewState::ExecuteLine (rig.session, "300.301", rig.session.GetMode());
            Assert::IsTrue (reply.text.front().starts_with ("0300- A9 41"), Widen (reply.text.front()).c_str());
        }

        //  A channel line in the session's mode renders in the session's
        //  format; a line a client ran in another mode, for itself, renders in
        //  that mode's own.
        TEST_METHOD (TheChannel_RendersInTheCurrentFormat_OrTheRequestsMode)
        {
            Rig    rig;
            Reply  inSessionMode;
            Reply  inAnotherMode;



            rig.Run ("OUTPUT GSSQUARED");

            inSessionMode = rig.session.ExecuteLine ("D 300:301", CommandMode::AppleWin);
            rig.session.FormatReply (inSessionMode, CommandMode::AppleWin);

            inAnotherMode = rig.session.ExecuteLine ("300.301", CommandMode::Monitor);
            rig.session.FormatReply (inAnotherMode, CommandMode::Monitor);

            Assert::IsTrue (inSessionMode.text.front().starts_with ("00/0300: A9 41"), Widen (inSessionMode.text.front()).c_str());
            Assert::IsTrue (inAnotherMode.text.front().starts_with ("0300- A9 41"),    Widen (inAnotherMode.text.front()).c_str());
        }

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }
    };
}
