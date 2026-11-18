#include "Pch.h"

#include "Core/JsonParser.h"
#include "Debugger/Channel/DebugChannelServer.h"
#include "InMemoryPipeTransport.h"

#include "CppUnitTest.h"
#include "Core/JsonValue.h"
#include "Core/TextEncoding.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingRunner
    //
    //  Stands in for the machine: remembers what it was asked to run, answers
    //  with a canned reply, and lets a test raise a stop from inside a run --
    //  which is what a synchronous machine does, and the case the cause id has
    //  to survive.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RecordingRunner : public IDebugCommandRunner
    {
    public:
        struct Ran
        {
            std::string                 line;
            std::optional<CommandMode>  mode;
            std::optional<uint64_t>     budget;
            ChannelClient               client;
            bool                        overwrite = false;
        };

        std::vector<Ran>       ran;
        int                    pauses = 0;
        ChannelHello           instance;

        //  Raised from inside the next RunLine, as a machine that stops
        //  before the command returns would.
        IDebugNotificationSink *  stopFrom = nullptr;
        std::optional<StopReason> stopWith;

        Reply RunLine (const std::string & line, std::optional<CommandMode> mode, std::optional<uint64_t> budget,
                       const ChannelClient & client, bool overwrite) override
        {
            Reply  reply;

            ran.push_back (Ran { line, mode, budget, client, overwrite });
            reply.command = line;

            if (startsRun)
            {
                startsRun = false;
                running   = true;
            }

            if (stopFrom != nullptr && stopWith.has_value())
            {
                StopEvent  stop;

                stop.reason = *stopWith;
                stopWith.reset();
                stopFrom->OnStopped (stop);
            }

            return reply;
        }

        void RequestPause() override
        {
            ++pauses;
        }

        //  A run in progress; startsRun makes the next line start one that is
        //  still going when it returns, as the emulator's does.
        bool running   = false;
        bool startsRun = false;

        bool IsRunInProgress() const override
        {
            return running;
        }

        ChannelHello GetInstance() const override
        {
            return instance;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugChannelServerTests
    //
    //  Several clients at once, with no pipe and no threads: the transport
    //  double lets a test say exactly what arrived in what order, which is the
    //  property the contract turns on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugChannelServerTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }



        static JsonValue Parsed (const std::string & record)
        {
            JsonValue       value;
            JsonParseError  error;
            HRESULT         hr = JsonParser::Parse (record, value, error);



            Assert::AreEqual (S_OK, hr, Widen (record + ": " + error.message).c_str());
            return value;
        }



        static std::string TypeOf (const std::string & record)
        {
            std::string  type;

            Assert::IsTrue (Parsed (record).HasString ("type", type), Widen (record).c_str());
            return type;
        }



        //  The record types one client received, in order.
        static std::vector<std::string> TypesFor (const InMemoryPipeTransport & transport, ChannelConnectionId connection)
        {
            std::vector<std::string>  types;

            for (const std::string & record : transport.Written (connection))
            {
                types.push_back (TypeOf (record));
            }

            return types;
        }



        static std::string Command (const std::string & line, int64_t id)
        {
            return std::format ("{{\"type\":\"command\",\"id\":{},\"line\":\"{}\"}}", id, line);
        }



        //  One record a client received, with the bounds checked first.
        //
        //  A REGRESSION THAT STOPS A RECORD BEING SENT MUST FAIL A TEST, NOT
        //  CRASH ONE. Reading `.front()` or `.back()` of what a client
        //  received walks off an empty vector the moment the server stops
        //  sending, and takes the whole test host down with it -- which
        //  reports nothing at all about which behavior broke.
        static std::string RecordAt (const InMemoryPipeTransport & transport, ChannelConnectionId connection, size_t index)
        {
            const std::vector<std::string> &  written = transport.Written (connection);



            Assert::IsTrue (written.size() > index,
                            Widen (std::format ("connection {} received {} record(s), so there is no record {}",
                                                connection, written.size(), index)).c_str());
            return written[index];
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Arrival order, and who each answer goes to
        //
        ////////////////////////////////////////////////////////////////////////

        //  COMMANDS FROM EVERY CLIENT RUN ONE AT A TIME IN THE ORDER THEY
        //  ARRIVED. Two clients interleaving is the case worth pinning,
        //  because it is the one a single client can never produce.
        TEST_METHOD (CommandsRunOneAtATimeInArrivalOrder)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    first  = 0;
            ChannelConnectionId    second = 0;



            Assert::AreEqual (S_OK, server.Open());

            first  = transport.Connect();
            second = transport.Connect();

            transport.Send (first,  Command ("r", 1));
            transport.Send (second, Command ("bp 300", 2));
            transport.Send (first,  Command ("d 300", 3));

            server.Pump();

            Assert::AreEqual ((size_t) 3, runner.ran.size());
            Assert::AreEqual (std::string ("r"),      runner.ran[0].line);
            Assert::AreEqual (std::string ("bp 300"), runner.ran[1].line);
            Assert::AreEqual (std::string ("d 300"),  runner.ran[2].line);
        }



        //  However fast a client writes, one pump runs kMaxLinesPerPump lines,
        //  so the pump returns to the machine; the rest run on later pumps, in
        //  order.
        TEST_METHOD (APumpRunsABoundedNumberOfLines)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;
            size_t                 i      = 0;



            Assert::AreEqual (S_OK, server.Open());

            client = transport.Connect();

            for (i = 0; i < DebugChannelServer::kMaxLinesPerPump + 10; i++)
            {
                transport.Send (client, Command ("r", (int) i + 1));
            }

            server.Pump();
            Assert::AreEqual (DebugChannelServer::kMaxLinesPerPump, runner.ran.size(), L"one pump's worth");

            server.Pump();
            Assert::AreEqual (DebugChannelServer::kMaxLinesPerPump + 10, runner.ran.size(), L"the rest on the next");
        }



        //  A reply goes only to the client that asked, which is what makes the
        //  id worth echoing.
        TEST_METHOD (AReplyGoesOnlyToTheClientThatAsked)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    asked   = 0;
            ChannelConnectionId    silent  = 0;
            int                    id      = 0;



            server.Open();
            asked  = transport.Connect();
            silent = transport.Connect();

            transport.Send (asked, Command ("r", 7));
            server.Pump();

            Assert::AreEqual ((size_t) 1, transport.Written (asked).size());
            Assert::AreEqual (std::string ("reply"), TypeOf (transport.Written (asked).front()));
            Assert::IsTrue   (Parsed (transport.Written (asked).front()).HasInt ("id", id));
            Assert::AreEqual (7, id);

            Assert::IsTrue (transport.Written (silent).empty(), L"the other client asked nothing and hears nothing");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Notifications
        //
        ////////////////////////////////////////////////////////////////////////

        //  Story 3 scenario 6: a stop reaches every client, not just whoever
        //  started the run. A breakpoint one client set stops the machine all
        //  of them are watching.
        //  While the channel is closed nothing is queued, so the first client
        //  after it opens again hears nothing that happened while it was shut.
        TEST_METHOD (NothingRaisedWhileClosedReachesTheNextClient)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;
            StopEvent              stop;



            server.Open();
            server.Close();

            stop.reason = StopReason::Pause;
            server.OnStopped (stop);
            server.OnResumed();

            server.Open();
            client = transport.Connect();
            server.Pump();

            Assert::IsTrue (transport.Written (client).empty(), L"no stale stop or resume");
        }



        //  A logging breakpoint's messages go to the client whose hello set
        //  logs, as log notifications, and to no other; past kMaxLogsPerPump
        //  in one pump, one notification counts the rest.
        TEST_METHOD (HitLogsGoToTheClientsWhoseHelloSetLogs)
        {
            InMemoryPipeTransport     transport;
            RecordingRunner           runner;
            DebugChannelServer        server (transport, runner);
            ChannelConnectionId       logging = 0;
            ChannelConnectionId       silent  = 0;
            std::vector<std::string>  flood (DebugChannelServer::kMaxLogsPerPump + 5, "Breakpoint #3: A=$42");



            server.Open();
            logging = transport.Connect();
            silent  = transport.Connect();
            transport.Send (logging, "{\"type\":\"hello\",\"id\":1,\"logs\":true}");
            transport.Send (silent,  "{\"type\":\"hello\",\"id\":2}");
            server.Pump();

            server.SendHitLogs ({ "Breakpoint #3: A=$42" });
            server.Pump();

            Assert::AreEqual ((size_t) 2, transport.Written (logging).size(), L"its hello's reply, then the log");
            Assert::AreEqual (std::string ("log"), TypeOf (transport.Written (logging).back()));
            Assert::IsTrue   (transport.Written (logging).back().find ("Breakpoint #3: A=$42") != std::string::npos);
            Assert::AreEqual ((size_t) 1, transport.Written (silent).size(), L"its hello's reply alone");

            server.SendHitLogs (flood);
            server.Pump();

            Assert::AreEqual ((size_t) 2 + DebugChannelServer::kMaxLogsPerPump + 1, transport.Written (logging).size());
            Assert::IsTrue   (transport.Written (logging).back().find ("5 more breakpoint messages were not sent.") != std::string::npos);
        }



        TEST_METHOD (EveryNotificationReachesEveryClient)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    first  = 0;
            ChannelConnectionId    second = 0;
            StopEvent              stop;



            server.Open();
            first  = transport.Connect();
            second = transport.Connect();
            server.Pump();

            stop.reason = StopReason::Breakpoint;
            server.OnStopped (stop);
            server.OnResumed();
            server.Pump();

            Assert::AreEqual ((size_t) 2, transport.Written (first).size());
            Assert::AreEqual ((size_t) 2, transport.Written (second).size());
            Assert::AreEqual (std::string ("stopped"), TypeOf (transport.Written (second)[0]));
            Assert::AreEqual (std::string ("resumed"), TypeOf (transport.Written (second)[1]));
        }



        //  A stop a command caused names it, so a client can tell its own run's
        //  outcome from one another client started.
        TEST_METHOD (AStopCausedByACommandNamesThatCommand)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client  = 0;
            int                    causeId = 0;



            server.Open();
            client = transport.Connect();

            //  The machine stops before the command returns, as a synchronous
            //  one does.
            runner.stopFrom = &server;
            runner.stopWith = StopReason::Budget;

            transport.Send (client, Command ("g", 9));
            server.Pump();

            //  The reply comes first, then the stop it caused.
            Assert::AreEqual ((size_t) 2, transport.Written (client).size());
            Assert::AreEqual (std::string ("reply"),   TypeOf (transport.Written (client)[0]));
            Assert::AreEqual (std::string ("stopped"), TypeOf (transport.Written (client)[1]));

            Assert::IsTrue   (Parsed (transport.Written (client)[1]).HasInt ("causeId", causeId));
            Assert::AreEqual (9, causeId);
        }



        //  A command that started no run is not the cause of a stop that comes
        //  later. The user pausing a machine after a client read its registers
        //  must not be reported as that read's outcome.
        TEST_METHOD (ACommandThatStartedNoRunCausesNoLaterStop)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client  = 0;
            StopEvent              stop;
            int                    ignored = 0;



            server.Open();
            client = transport.Connect();

            transport.Send (client, Command ("r", 2));
            server.Pump();

            stop.reason = StopReason::Pause;
            server.OnStopped (stop);
            server.Pump();

            Assert::AreEqual ((size_t) 2, transport.Written (client).size());
            Assert::IsFalse  (Parsed (transport.Written (client)[1]).HasInt ("causeId", ignored),
                              L"the read started nothing, so it caused nothing");

            bool  running = false;

            Assert::IsFalse  (Parsed (transport.Written (client)[0]).HasBool ("running", running),
                              L"and its reply does not say a run is going");
        }



        //  A run a client started before the channel closed ends with no
        //  cause once the channel is open again: the request id is from a
        //  client that is gone, and a new client numbering its requests the
        //  same way could mistake the stop for one it caused.
        TEST_METHOD (ACauseDoesNotOutliveAClose)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client  = 0;
            StopEvent              stop;
            int                    ignored = 0;



            server.Open();
            client = transport.Connect();

            runner.startsRun = true;
            transport.Send (client, Command ("g", 5));
            server.Pump();

            server.Close();
            server.Open();
            client = transport.Connect();
            server.Pump();

            stop.reason = StopReason::Breakpoint;
            server.OnStopped (stop);
            server.Pump();

            Assert::AreEqual ((size_t) 1, transport.Written (client).size());
            Assert::IsFalse  (Parsed (transport.Written (client)[0]).HasInt ("causeId", ignored), L"no cause from before the close");
        }



        //  A run a command started that is still going when the command returns
        //  -- the emulator's case -- is named by the stop that ends it later.
        TEST_METHOD (ARunStillGoingNamesItsCommandWhenItStops)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client  = 0;
            StopEvent              stop;
            int                    causeId = 0;



            server.Open();
            client = transport.Connect();

            runner.startsRun = true;
            transport.Send (client, Command ("g", 5));
            server.Pump();

            bool  running = false;

            Assert::IsTrue (Parsed (transport.Written (client)[0]).HasBool ("running", running) && running,
                            L"the reply says a stop naming this command is still to come");

            stop.reason = StopReason::Breakpoint;
            server.OnStopped (stop);
            server.Pump();

            Assert::IsTrue   (Parsed (transport.Written (client)[1]).HasInt ("causeId", causeId));
            Assert::AreEqual (5, causeId);
        }



        //  A command sent while another's run goes on did not start it: its
        //  reply promises no stop, and the stop still names the command that
        //  started the run.
        TEST_METHOD (ACommandDuringAnotherRunWaitsForNothing)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client  = 0;
            StopEvent              stop;
            int                    causeId = 0;
            bool                   running = false;



            server.Open();
            client = transport.Connect();

            runner.startsRun = true;
            transport.Send (client, Command ("g", 5));
            transport.Send (client, Command ("r", 6));
            server.Pump();

            Assert::IsFalse (Parsed (transport.Written (client)[1]).HasBool ("running", running),
                             L"the R started no run, so there is nothing for it to wait for");

            stop.reason = StopReason::Breakpoint;
            server.OnStopped (stop);
            server.Pump();

            Assert::IsTrue   (Parsed (transport.Written (client)[2]).HasInt ("causeId", causeId));
            Assert::AreEqual (5, causeId, L"the stop is the G's, not the R's");
        }



        //  A stop nobody asked for names nobody: the machine hit a breakpoint
        //  while running freely.
        TEST_METHOD (AStopNoCommandCausedNamesNoCause)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client  = 0;
            StopEvent              stop;
            int                    ignored = 0;



            server.Open();
            client = transport.Connect();
            server.Pump();

            stop.reason = StopReason::Breakpoint;
            server.OnStopped (stop);
            server.Pump();

            Assert::IsFalse (Parsed (RecordAt (transport, client, 0)).HasInt ("causeId", ignored));
        }



        //  And a cause is spent by the stop that used it, so the next
        //  unprompted stop does not inherit it.
        TEST_METHOD (ACauseIsSpentByTheStopThatUsedIt)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client  = 0;
            StopEvent              stop;
            int                    ignored = 0;



            server.Open();
            client = transport.Connect();

            runner.stopFrom = &server;
            runner.stopWith = StopReason::Budget;
            transport.Send (client, Command ("g", 4));
            server.Pump();

            stop.reason = StopReason::Breakpoint;
            server.OnStopped (stop);
            server.Pump();

            Assert::AreEqual ((size_t) 3, transport.Written (client).size());
            Assert::IsFalse  (Parsed (transport.Written (client)[2]).HasInt ("causeId", ignored),
                              L"the second stop was nobody's command");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The handshake, pause, and malformed input
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (HelloIsAnsweredWithTheInstance)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;
            std::string            machine;
            int                    id     = 0;



            runner.instance.machine = "Apple //e Enhanced";
            runner.instance.pid     = 4321;

            server.Open();
            client = transport.Connect();

            transport.Send (client, "{\"type\":\"hello\",\"id\":11}");
            server.Pump();

            Assert::AreEqual (std::string ("hello"), TypeOf (RecordAt (transport, client, 0)));
            Assert::IsTrue   (Parsed (RecordAt (transport, client, 0)).HasString ("machine", machine));
            Assert::AreEqual (std::string ("Apple //e Enhanced"), machine);

            //  Echoed, so a client that sent several can match the answer.
            Assert::IsTrue   (Parsed (RecordAt (transport, client, 0)).HasInt ("id", id));
            Assert::AreEqual (11, id);
        }



        //  Answered at once rather than when the machine stops: a client that
        //  waited for this reply to mean "stopped" would wait through the whole
        //  of whatever was running.
        TEST_METHOD (PauseIsAskedForAndAnsweredAtOnce)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;
            int                    id     = 0;



            server.Open();
            client = transport.Connect();

            transport.Send (client, "{\"type\":\"pause\",\"id\":5}");
            server.Pump();

            Assert::AreEqual (1, runner.pauses);
            Assert::AreEqual ((size_t) 1, transport.Written (client).size());
            Assert::AreEqual (std::string ("reply"), TypeOf (RecordAt (transport, client, 0)));

            //  EVERY reply echoes its id, and a pause is a reply. Nothing
            //  asserted this until a mutation of the id on this very line
            //  went unnoticed.
            Assert::IsTrue   (Parsed (RecordAt (transport, client, 0)).HasInt ("id", id));
            Assert::AreEqual (5, id);
        }



        //  One client's malformed line is answered to that client and costs it
        //  nothing else: the connection stays open and the others never hear.
        TEST_METHOD (AMalformedLineIsOneClientsProblem)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    clumsy = 0;
            ChannelConnectionId    other  = 0;



            server.Open();
            clumsy = transport.Connect();
            other  = transport.Connect();

            transport.Send (clumsy, "not json at all");
            transport.Send (clumsy, Command ("r", 2));
            server.Pump();

            Assert::AreEqual ((size_t) 2, transport.Written (clumsy).size());
            Assert::AreEqual (std::string ("error"), TypeOf (transport.Written (clumsy)[0]));
            Assert::AreEqual (std::string ("reply"), TypeOf (transport.Written (clumsy)[1]));
            Assert::IsTrue   (transport.Written (other).empty());
        }



        //  Lines a broken or hostile client might send: each gets one record
        //  back, an error or a reply, and none throws or stops the next line
        //  from being read.
        TEST_METHOD (HostileLinesEachGetOneRecord)
        {
            static const std::vector<std::string>  kLines =
            {
                "", " ", "{", "}", "[]", "null", "true", "42", "\"text\"", "{}", "{\"type\":null}",
                "{\"type\":\"command\"}", "{\"type\":\"command\",\"id\":1}", "{\"type\":\"command\",\"id\":-1,\"line\":\"r\"}",
                "{\"type\":\"command\",\"id\":1.5,\"line\":\"r\"}", "{\"type\":\"command\",\"id\":1e300,\"line\":\"r\"}",
                "{\"type\":\"command\",\"id\":9007199254740993,\"line\":\"r\"}", "{\"type\":\"command\",\"id\":\"1\",\"line\":\"r\"}",
                "{\"type\":\"command\",\"id\":1,\"line\":42}", "{\"type\":\"command\",\"id\":1,\"line\":\"r\",\"mode\":\"nosuch\"}",
                "{\"type\":\"command\",\"id\":1,\"line\":\"r\",\"budget\":0}", "{\"type\":\"command\",\"id\":1,\"line\":\"r\",\"budget\":-5}",
                "{\"type\":\"command\",\"id\":1,\"line\":\"r\",\"overwrite\":\"yes\"}", "{\"type\":\"hello\",\"id\":1,\"allowPaths\":\"C:\\\\\"}",
                "{\"type\":\"hello\",\"id\":1,\"allowPaths\":[1,2]}", "{\"type\":\"hello\",\"id\":1,\"protocol\":\"x\"}",
                "{\"type\":\"hello\",\"id\":1,\"logs\":1}", "{\"type\":\"nosuch\",\"id\":1}", "{\"type\":\"pause\"}",
                "{\"type\":\"command\",\"id\":1,\"line\":\"r\"", "{\"type\":\"command\",\"id\":1,\"line\":\"\\u00\"}",
                "{\"type\":\"command\",\"id\":1,\"line\":\"\\uD800\"}", "{\"a\":" + std::string (200, '['),
                std::string (500, '[') + std::string (500, ']'), "\xFF\xFE\x00\x01", "{\"type\":\"command\",\"id\":1,\"line\":\"\xC3\x28\"}",
            };

            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;
            size_t                 before = 0;



            server.Open();
            client = transport.Connect();

            for (const std::string & line : kLines)
            {
                before = transport.Written (client).size();
                transport.Send (client, line);
                server.Pump();

                Assert::AreEqual (before + 1, transport.Written (client).size(), (L"one record for: " + Widen (line)).c_str());
            }

            transport.Send (client, Command ("r", 2));
            server.Pump();

            Assert::AreEqual (std::string ("reply"), TypeOf (transport.Written (client).back()), L"a good line after them gets a reply");
        }



        //  A per-line mode and budget reach the runner, which is where the
        //  contract says they apply: this line only.
        TEST_METHOD (APerLineModeAndBudgetReachTheRunner)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;



            server.Open();
            client = transport.Connect();

            transport.Send (client, "{\"type\":\"command\",\"id\":1,\"line\":\"300.30F\",\"mode\":\"monitor\",\"budget\":5000}");
            server.Pump();

            Assert::AreEqual ((size_t) 1, runner.ran.size());
            Assert::IsTrue   (runner.ran[0].mode.has_value());
            Assert::IsTrue   (*runner.ran[0].mode == CommandMode::Monitor);
            Assert::IsTrue   (runner.ran[0].budget.has_value());
            Assert::AreEqual ((uint64_t) 5000, *runner.ran[0].budget);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Leaving
        //
        ////////////////////////////////////////////////////////////////////////

        //  A CLIENT LEAVING CHANGES NOTHING ABOUT THE MACHINE. Someone may be
        //  sitting at it; a debugger client closing its window is not a reason
        //  to start the machine running again.
        TEST_METHOD (AClientDisconnectingLeavesTheMachineAlone)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    leaving = 0;
            ChannelConnectionId    staying = 0;



            server.Open();
            leaving = transport.Connect();
            staying = transport.Connect();
            server.Pump();

            transport.Disconnect (leaving);
            server.Pump();

            Assert::AreEqual (0,          runner.pauses,    L"nothing was asked of the machine");
            Assert::AreEqual ((size_t) 0, runner.ran.size());
            Assert::AreEqual ((size_t) 1, transport.GetConnections().size());
            Assert::AreEqual (staying,    transport.GetConnections().front());
        }



        //  `closing` is the last record, so a client can tell the debugger
        //  being closed from the pipe breaking under it.
        TEST_METHOD (CloseTellsEveryClientBeforeItGoes)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    first  = 0;
            ChannelConnectionId    second = 0;



            server.Open();
            first  = transport.Connect();
            second = transport.Connect();
            server.Pump();

            server.Close();

            Assert::IsFalse  (server.IsOpen());

            //  Checked before either is read, so a server that stopped
            //  sending fails here rather than walking off an empty vector.
            Assert::IsFalse  (transport.Written (first).empty(),  L"the first client was told nothing");
            Assert::IsFalse  (transport.Written (second).empty(), L"the second client was told nothing");

            Assert::AreEqual (std::string ("closing"), TypeOf (transport.Written (first).back()));
            Assert::AreEqual (std::string ("closing"), TypeOf (transport.Written (second).back()));
            Assert::IsTrue   (transport.GetConnections().empty(), L"and everyone is dropped");
        }



        //  A server that never opened does nothing, rather than writing to a
        //  transport that was never listening.
        TEST_METHOD (AServerThatFailedToOpenStaysShut)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;



            transport.SetListenResult (E_ACCESSDENIED);

            Assert::AreEqual (E_ACCESSDENIED, server.Open());
            Assert::IsFalse  (server.IsOpen());

            client = transport.Connect();
            transport.Send (client, Command ("r", 1));
            server.Pump();

            Assert::AreEqual ((size_t) 0, runner.ran.size());
            Assert::IsTrue   (transport.Written (client).empty());
        }



        //  A hello's name, folders and scripts setting apply to that client's
        //  commands and no one else's: a client without a hello has the
        //  defaults.
        TEST_METHOD (AHellosFileAccess_AppliesToThatClientsCommandsOnly)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    first  = 0;
            ChannelConnectionId    second = 0;



            Assert::AreEqual (S_OK, server.Open());

            first  = transport.Connect();
            second = transport.Connect();

            transport.Send (first,  ChannelProtocol::WriteHelloRequest (1, ChannelClient { "MCP client", { L"C:\\Work" } }));
            transport.Send (first,  Command ("r", 2));
            transport.Send (second, Command ("r", 3));

            server.Pump();

            Assert::AreEqual ((size_t) 2, runner.ran.size());
            Assert::AreEqual (std::string ("MCP client"), runner.ran[0].client.name);
            Assert::AreEqual ((size_t) 1, runner.ran[0].client.allowPaths.size());
            Assert::AreEqual (std::wstring (L"C:\\Work"), runner.ran[0].client.allowPaths[0]);

            Assert::IsTrue   (runner.ran[1].client.name.empty(), L"the other client sent no hello");
            Assert::IsTrue   (runner.ran[1].client.allowPaths.empty());
        }



        TEST_METHOD (ACommandsOverwriteOption_ReachesTheRunner)
        {
            InMemoryPipeTransport  transport;
            RecordingRunner        runner;
            DebugChannelServer     server (transport, runner);
            ChannelConnectionId    client = 0;



            Assert::AreEqual (S_OK, server.Open());

            client = transport.Connect();
            transport.Send (client, R"({"type":"command","id":1,"line":"BSAVE x 300,1","overwrite":true})");
            transport.Send (client, Command ("BSAVE x 300,1", 2));
            server.Pump();

            Assert::AreEqual ((size_t) 2, runner.ran.size());
            Assert::IsTrue   (runner.ran[0].overwrite);
            Assert::IsFalse  (runner.ran[1].overwrite);
        }
    };
}
