#include "Pch.h"

#include "Core/JsonParser.h"
#include "Debugger/Channel/ChannelProtocol.h"
#include "Debugger/ReplyJson.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ChannelProtocolTests
    //
    //  contracts/debug-channel-protocol.md, which is written so a client can
    //  be built from it alone. These tests are what makes that claim checkable
    //  without a pipe, a thread or a machine.
    //
    //  RECORDS ARE PARSED BACK RATHER THAN COMPARED AS TEXT. A client reads
    //  JSON, not a byte sequence, so asserting on the parsed fields tests what
    //  the contract actually promises and leaves key order and spacing free.
    //  The one thing asserted about the text itself is that a record holds no
    //  raw newline, because that is the framing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ChannelProtocolTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        //  A record's JSON, parsed back.
        static JsonValue Parsed (const std::string & record)
        {
            JsonValue       value;
            JsonParseError  error;
            HRESULT         hr = JsonParser::Parse (record, value, error);



            Assert::AreEqual (S_OK, hr, Widen (record + ": " + error.message).c_str());
            Assert::IsTrue (value.GetType() == JsonType::Object, Widen (record).c_str());

            return value;
        }



        static std::string Field (const JsonValue & object, const char * key)
        {
            std::string  value;

            Assert::IsTrue (object.HasString (key, value), Widen (std::string ("no string field: ") + key).c_str());
            return value;
        }



        static int Number (const JsonValue & object, const char * key)
        {
            int  value = 0;

            Assert::IsTrue (object.HasInt (key, value), Widen (std::string ("no number field: ") + key).c_str());
            return value;
        }



        static ChannelRequest ParseOk (const std::string & line)
        {
            ChannelRequest  request;
            ReplyError      error;



            Assert::IsTrue (ChannelProtocol::TryParseRequest (line, request, error),
                            Widen (line + ": " + error.label + ", " + error.detail).c_str());
            return request;
        }



        static ReplyError ParseFails (const std::string & line)
        {
            ChannelRequest  request;
            ReplyError      error;



            Assert::IsFalse (ChannelProtocol::TryParseRequest (line, request, error), Widen (line).c_str());
            Assert::IsFalse (error.label.empty(),  Widen (line + ": no label").c_str());
            Assert::IsFalse (error.detail.empty(), Widen (line + ": no detail").c_str());
            return error;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Framing
        //
        ////////////////////////////////////////////////////////////////////////

        //  No record may hold a raw newline, because the framing is one object
        //  per line. The writer defaults to pretty-printing, so this is a real
        //  hazard rather than a theoretical one.
        //  JSON numbers are doubles: an id, a protocol or a budget that is not
        //  a whole number in range is malformed, never cast into something else.
        TEST_METHOD (NumbersMustBeWholeAndInRange)
        {
            Assert::AreEqual ((int64_t) 9007199254740992, ParseOk (R"({"type":"pause","id":9007199254740992})").id);
            Assert::AreEqual ((uint64_t) 1, *ParseOk (R"({"type":"command","id":1,"line":"G","budget":1})").budget);
            Assert::AreEqual (2, ParseOk (R"({"type":"hello","id":1,"protocol":2})").protocol);

            for (const char * line : { R"({"type":"pause","id":1.5})",
                                       R"({"type":"pause","id":-1})",
                                       R"({"type":"pause","id":1e300})",
                                       R"({"type":"pause","id":"7"})",
                                       R"({"type":"hello","id":1,"protocol":1.5})",
                                       R"({"type":"command","id":1,"line":"G","budget":0.5})",
                                       R"({"type":"command","id":1,"line":"G","budget":0})",
                                       R"({"type":"command","id":1,"line":"G","budget":-5})",
                                       R"({"type":"command","id":1,"line":"G","budget":1e300})",
                                       R"({"type":"command","id":1,"line":"G","budget":"100"})" })
            {
                Assert::AreEqual (std::string ("malformed request"), ParseFails (line).label, Widen (line).c_str());
            }
        }



        TEST_METHOD (NoRecordHoldsARawNewline)
        {
            ChannelHello  hello;
            Reply         reply;



            hello.machine = "Apple //e";
            hello.title   = "a title";

            reply.text.push_back ("first");
            reply.text.push_back ("second");

            for (const std::string & record : { ChannelProtocol::WriteHello (hello),
                                                ChannelProtocol::WriteError ("label", "detail"),
                                                ReplyJson::WriteReply (reply, 7) })
            {
                Assert::IsTrue (record.find ('\n') == std::string::npos, Widen (record).c_str());
                Assert::IsTrue (record.find ('\r') == std::string::npos, Widen (record).c_str());
            }
        }



        //  A newline inside a string is escaped rather than emitted, so a
        //  reply carrying multi-line text is still one line on the wire.
        TEST_METHOD (ANewlineInsideTextIsEscaped)
        {
            Reply  reply;



            reply.text.push_back ("one\ntwo");

            Assert::IsTrue (ReplyJson::WriteReply (reply, 1).find ('\n') == std::string::npos);
        }



        //  The tolerance is the JSON parser's, which treats a trailing CR as
        //  the whitespace it is. Nothing strips it first: a loop that did
        //  passed every test with or without it.
        TEST_METHOD (CarriageReturnBeforeTheNewlineIsTolerated)
        {
            ChannelRequest  request = ParseOk ("{\"type\":\"command\",\"id\":3,\"line\":\"r\"}\r\n");



            Assert::AreEqual ((int64_t) 3, request.id);
            Assert::AreEqual (std::string ("r"), request.line);
        }



        //  A line over the cap is refused and the connection stays open, so
        //  one oversized request cannot end a session.
        TEST_METHOD (ALineOverTheCapIsRefused)
        {
            static constexpr size_t  kOneMebibyte = 1024 * 1024;
            std::string              line;
            ReplyError               error;



            //  THE CONTRACT'S NUMBER, NOT THE CODE'S. Building the oversize
            //  line from kMaxLineBytes compares the constant with itself and
            //  holds at any value, so the size is written out here and the
            //  constant is checked against it separately. Raising the cap
            //  changes the contract, and should fail this.
            Assert::AreEqual (kOneMebibyte, ChannelProtocol::kMaxLineBytes);

            line  = "{\"type\":\"command\",\"id\":1,\"line\":\"" + std::string (kOneMebibyte + 1, 'x') + "\"}";
            error = ParseFails (line);

            Assert::AreEqual (std::string ("line too long"), error.label);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The handshake
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AClientHelloIsRead)
        {
            ChannelRequest  request = ParseOk ("{\"type\":\"hello\",\"id\":1,\"client\":\"vscode-casso\",\"protocol\":1}");



            Assert::IsTrue   (request.type == ChannelRequestType::Hello);
            Assert::AreEqual ((int64_t) 1, request.id);
            Assert::AreEqual (std::string ("vscode-casso"), request.client);
            Assert::AreEqual (1, request.protocol);
        }



        //  A client need not name itself, and a hello with neither field is
        //  still a hello.
        TEST_METHOD (AClientHelloNeedNotNameItself)
        {
            ChannelRequest  request = ParseOk ("{\"type\":\"hello\",\"id\":1}");



            Assert::IsTrue  (request.type == ChannelRequestType::Hello);
            Assert::IsTrue  (request.client.empty());
            Assert::AreEqual (0, request.protocol);
        }



        TEST_METHOD (TheServerHelloCarriesTheInstance)
        {
            ChannelHello       hello;
            JsonValue          record;
            const JsonValue  * disks  = nullptr;



            hello.id       = 4;
            hello.pid      = 1234;
            hello.title    = "035-debugger";
            hello.machine  = "Apple //e Enhanced";
            hello.mode     = CommandMode::Monitor;
            hello.isPaused = false;
            hello.disks.push_back ("C:\\Disks\\game.woz");
            hello.disks.push_back (std::nullopt);

            record = Parsed (ChannelProtocol::WriteHello (hello));

            Assert::AreEqual (std::string ("hello"),              Field  (record, "type"));
            Assert::AreEqual (4,                                  Number (record, "id"));
            Assert::AreEqual (ChannelProtocol::kProtocolVersion,  Number (record, "protocol"));
            Assert::AreEqual (1234,                               Number (record, "pid"));
            Assert::AreEqual (std::string ("035-debugger"),       Field  (record, "title"));
            Assert::AreEqual (std::string ("Apple //e Enhanced"), Field  (record, "machine"));
            Assert::AreEqual (std::string ("monitor"),            Field  (record, "mode"));
            Assert::AreEqual (std::string ("running"),            Field  (record, "state"));

            //  An empty drive is null, not an empty string: "no disk" and "a
            //  disk whose path is empty" are not the same thing.
            Assert::IsTrue   (record.HasArray ("disks", disks));
            Assert::AreEqual ((size_t) 2, disks->GetArraySize());
            Assert::AreEqual (std::string ("C:\\Disks\\game.woz"), disks->GetArrayElement (0).GetString());
            Assert::IsTrue   (disks->GetArrayElement (1).GetType() == JsonType::Null);
        }



        //  THE SERVER ANSWERS WITH ITS OWN VERSION rather than refusing a
        //  client that asked for a higher one. The client learns what it is
        //  talking to and decides for itself.
        TEST_METHOD (AClientAskingForALaterProtocolIsStillAnswered)
        {
            ChannelRequest  request = ParseOk ("{\"type\":\"hello\",\"id\":1,\"protocol\":99}");
            ChannelHello    hello;
            JsonValue       record;



            Assert::AreEqual (99, request.protocol, L"what the client asked for is reported as it asked");

            hello.id = request.id;
            record   = Parsed (ChannelProtocol::WriteHello (hello));

            Assert::AreEqual (ChannelProtocol::kProtocolVersion, Number (record, "protocol"), L"and the answer is the server's own");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Requests
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ACommandCarriesItsLine)
        {
            ChannelRequest  request = ParseOk ("{\"type\":\"command\",\"id\":7,\"line\":\"bp 0300\"}");



            Assert::IsTrue   (request.type == ChannelRequestType::Command);
            Assert::AreEqual ((int64_t) 7, request.id);
            Assert::AreEqual (std::string ("bp 0300"), request.line);
            Assert::IsFalse  (request.mode.has_value(),   L"absent means the session's mode");
            Assert::IsFalse  (request.budget.has_value(), L"absent means the session's budget");
        }



        TEST_METHOD (ACommandMayChooseAModeAndABudget)
        {
            ChannelRequest  monitor = ParseOk ("{\"type\":\"command\",\"id\":8,\"line\":\"300.30F\",\"mode\":\"monitor\"}");
            ChannelRequest  bounded = ParseOk ("{\"type\":\"command\",\"id\":9,\"line\":\"g\",\"budget\":5000000}");



            Assert::IsTrue   (monitor.mode.has_value());
            Assert::IsTrue   (*monitor.mode == CommandMode::Monitor);

            Assert::IsTrue   (bounded.budget.has_value());
            Assert::AreEqual ((uint64_t) 5000000, *bounded.budget);
        }



        TEST_METHOD (AModeThatIsNotAModeIsRefused)
        {
            ReplyError  error = ParseFails ("{\"type\":\"command\",\"id\":1,\"line\":\"r\",\"mode\":\"basic\"}");



            Assert::AreEqual (std::string ("unknown mode"), error.label);
        }



        TEST_METHOD (PauseNeedsNoLine)
        {
            ChannelRequest  request = ParseOk ("{\"type\":\"pause\",\"id\":5}");



            Assert::IsTrue  (request.type == ChannelRequestType::Pause);
            Assert::IsTrue  (request.line.empty());
        }



        //  The versioning promise: a client built against a later contract
        //  sends fields this server has never heard of, and the command still
        //  runs.
        TEST_METHOD (UnknownFieldsAreIgnored)
        {
            ChannelRequest  request = ParseOk ("{\"type\":\"command\",\"id\":2,\"line\":\"r\",\"whenPigsFly\":true,\"extra\":{\"a\":1}}");



            Assert::AreEqual (std::string ("r"), request.line);
            Assert::AreEqual ((int64_t) 2,       request.id);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Malformed input
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MalformedInputIsAnError)
        {
            Assert::AreEqual (std::string ("malformed request"), ParseFails ("not json at all").label);
            Assert::AreEqual (std::string ("malformed request"), ParseFails ("[1,2,3]").label);
            Assert::AreEqual (std::string ("malformed request"), ParseFails ("{\"id\":1}").label);
            Assert::AreEqual (std::string ("malformed request"), ParseFails ("{\"type\":\"command\",\"line\":\"r\"}").label);
            Assert::AreEqual (std::string ("malformed request"), ParseFails ("{\"type\":\"command\",\"id\":1}").label);
            Assert::AreEqual (std::string ("unknown request"),   ParseFails ("{\"type\":\"frobnicate\",\"id\":1}").label);
        }



        TEST_METHOD (TheErrorRecordCarriesTheTwoLineShape)
        {
            JsonValue          record = Parsed (ChannelProtocol::WriteError ("malformed request", "The line is not a JSON object."));
            const JsonValue *  inner  = nullptr;



            Assert::AreEqual (std::string ("error"), Field (record, "type"));
            Assert::IsTrue   (record.HasObject ("error", inner));
            Assert::AreEqual (std::string ("malformed request"),            Field (*inner, "label"));
            Assert::AreEqual (std::string ("The line is not a JSON object."), Field (*inner, "detail"));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Replies and notifications
        //
        //  These are ReplyJson's records; what is asserted here is the part
        //  the channel adds, which is who each one is addressed to.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AReplyEchoesItsId_AndANotificationCarriesNone)
        {
            Reply      reply;
            StopEvent  stop;
            JsonValue  answered;
            JsonValue  announced;
            int        ignored = 0;



            reply.command = "r";
            answered      = Parsed (ReplyJson::WriteReply (reply, 7));

            Assert::AreEqual (std::string ("reply"), Field  (answered, "type"));
            Assert::AreEqual (7,                     Number (answered, "id"));

            stop.reason = StopReason::Breakpoint;
            announced   = Parsed (ReplyJson::WriteStopped (stop, std::nullopt));

            Assert::AreEqual (std::string ("stopped"), Field (announced, "type"));
            Assert::IsFalse  (announced.HasInt ("id", ignored), L"a notification goes to every client and answers nobody");
        }



        //  A stop a command caused names that command, so a client can tell
        //  its own run's outcome from one another client started.
        TEST_METHOD (AStopCausedByACommandCarriesItsCauseId)
        {
            StopEvent  stop;
            JsonValue  record;



            stop.reason = StopReason::Budget;
            record      = Parsed (ReplyJson::WriteStopped (stop, 9));

            Assert::AreEqual (9, Number (record, "causeId"));
            Assert::AreEqual (std::string ("budget"), Field (record, "reason"));
        }
    };
}
