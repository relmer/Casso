#include "Pch.h"

#include "Cli/CommandLineQuoting.h"
#include "Cli/ICassoLauncher.h"
#include "Cli/McpServer.h"
#include "Cli/McpTools.h"
#include "Core/JsonParser.h"
#include "Core/TextEncoding.h"
#include "Debugger/Channel/IInstanceDirectory.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  McpServerTests
//
//  `CassoCli mcp` against a scripted Casso: the protocol's messages, and the
//  tools' command lines, replies, runs and pauses.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FakeCasso
    //
    //  One Casso's side of the channel: hello, a reply to each command, a stop
    //  after each run unless `isRunEndless`, and a stop for a pause. EXIT ends
    //  the process.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct FakeCasso
    {
        std::vector<std::string>  lines;
        std::vector<std::string>  types;
        bool                      isRunEndless = false;
        bool                      isPaused     = false;
        bool                      isRunning    = true;
        std::string               failing;
    };



    class FakeCassoClient : public IChannelClient
    {
    public:
        explicit FakeCassoClient (FakeCasso & casso) : m_casso (casso) {}

        bool WriteLine (const std::string & line) override
        {
            JsonValue       request;
            JsonParseError  error;
            HRESULT         hr      = JsonParser::Parse (line, request, error);
            std::string     type;
            std::string     text;
            int             id      = 0;



            Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (line).c_str());
            Assert::IsTrue (line.find ('\n') == std::string::npos, L"one record a line");

            (void) request.HasString ("type", type);
            (void) request.HasInt ("id", id);
            m_casso.types.push_back (type);

            if (type == "hello")
            {
                m_incoming.push_back (std::format (R"({{"type":"hello","id":{},"protocol":1,"state":"{}"}})", id, m_casso.isPaused ? "paused" : "running"));
                return true;
            }

            if (type == "pause")
            {
                m_incoming.push_back (std::format (R"({{"type":"reply","id":{},"status":"ok","text":[]}})", id));
                m_incoming.push_back (R"({"type":"stopped","reason":"pause","pc":768})");
                return true;
            }

            (void) request.HasString ("line", text);
            m_casso.lines.push_back (text);

            if (text == m_casso.failing)
            {
                m_incoming.push_back (std::format (R"({{"type":"reply","id":{},"status":"error","command":"x","text":[],"error":{{"label":"invalid arguments","detail":"No."}}}})", id));
            }
            else if (text == "G" || text.starts_with ("G ") || text.starts_with ("T ") || text.starts_with ("P "))
            {
                m_incoming.push_back (std::format (R"({{"type":"reply","id":{},"status":"ok","text":[],"running":true}})", id));

                if (!m_casso.isRunEndless)
                {
                    m_incoming.push_back (std::format (R"({{"type":"stopped","reason":"breakpoint","pc":4660,"causeId":{}}})", id));
                }
            }
            else
            {
                m_incoming.push_back (std::format (R"({{"type":"reply","id":{},"status":"ok","text":["ran {}"],"data":{{"kind":"message"}}}})", id, text));
            }

            if (text == "EXIT")
            {
                m_casso.isRunning = false;
            }

            return true;
        }

        bool ReadLine (std::string & line, DWORD) override
        {
            if (m_incoming.empty())
            {
                return false;
            }

            line = m_incoming.front();
            m_incoming.pop_front();
            return true;
        }

        bool IsClosed() const override { return false; }

    private:
        FakeCasso               & m_casso;
        std::deque<std::string>   m_incoming;
    };



    class FakeDirectory : public IInstanceDirectory
    {
    public:
        std::vector<uint32_t>  ListProcessIds() override
        {
            std::vector<uint32_t>  ids;



            for (const auto & [id, casso] : instances)
            {
                if (casso.isRunning)
                {
                    ids.push_back (id);
                }
            }

            return ids;
        }

        std::unique_ptr<IChannelClient>  Connect (uint32_t processId) override
        {
            auto  found = instances.find (processId);



            return (found == instances.end()) ? nullptr : std::make_unique<FakeCassoClient> (found->second);
        }

        std::map<uint32_t, FakeCasso>  instances;
    };



    class FakeLauncher : public ICassoLauncher
    {
    public:
        explicit FakeLauncher (FakeDirectory & directory) : m_directory (directory) {}

        uint32_t  Launch (const std::wstring & arguments, std::string &) override
        {
            launched = arguments;
            m_directory.instances[nextId];
            return nextId;
        }

        bool  IsRunning (uint32_t processId) override
        {
            return m_directory.instances.contains (processId) && m_directory.instances[processId].isRunning;
        }

        void  Wait (DWORD milliseconds) override { waitedMs += milliseconds; }

        std::wstring  launched;
        uint32_t      nextId   = 4242;
        DWORD         waitedMs = 0;

    private:
        FakeDirectory  & m_directory;
    };





    TEST_CLASS (McpServerTests)
    {
    public:

        struct Rig
        {
            FakeDirectory  directory;
            FakeLauncher   launcher { directory };
            McpTools       tools    { directory, launcher };
            McpServer      server   { tools };
            int            nextId   = 1;

            JsonValue  Send (const std::string & method, const std::string & params)
            {
                std::string     line     = std::format (R"({{"jsonrpc":"2.0","id":{},"method":"{}","params":{}}})", nextId++, method, params);
                std::string     response = server.HandleLine (line);
                JsonValue       value;
                JsonParseError  error;
                HRESULT         hr       = JsonParser::Parse (response, value, error);



                Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (response).c_str());
                Assert::IsTrue (response.find ('\n') == std::string::npos, L"one message a line");
                return value;
            }

            JsonValue  Call (const std::string & tool, const std::string & arguments = "{}")
            {
                JsonValue          response = Send ("tools/call", std::format (R"({{"name":"{}","arguments":{}}})", tool, arguments));
                const JsonValue  * result   = nullptr;



                Assert::IsTrue (response.HasObject ("result", result), L"a result");
                return *result;
            }
        };


        static std::string GetText (const JsonValue & result)
        {
            const JsonValue  * content = nullptr;
            std::string        text;
            std::string        each;



            Assert::IsTrue (result.HasArray ("content", content));

            for (size_t i = 0; i < content->GetArraySize(); i++)
            {
                text += content->GetArrayElement (i).HasString ("text", each) ? each + "\n" : std::string();
            }

            return text;
        }


        static bool IsError (const JsonValue & result)
        {
            bool  isError = false;



            (void) result.HasBool ("isError", isError);
            return isError;
        }



        TEST_METHOD (InitializeAnswersWithAVersionItKnows)
        {
            Rig                rig;
            JsonValue          response = rig.Send ("initialize", R"({"protocolVersion":"2025-03-26","capabilities":{}})");
            const JsonValue  * result   = nullptr;
            const JsonValue  * caps     = nullptr;
            const JsonValue  * tools    = nullptr;
            std::string        version;



            Assert::IsTrue   (response.HasObject ("result", result));
            Assert::IsTrue   (result->HasString ("protocolVersion", version));
            Assert::AreEqual (std::string ("2025-03-26"), version, L"the client's version, which the server supports");
            Assert::IsTrue   (result->HasObject ("capabilities", caps) && caps->HasObject ("tools", tools));

            response = rig.Send ("initialize", R"({"protocolVersion":"2099-01-01"})");
            Assert::IsTrue   (response.HasObject ("result", result) && result->HasString ("protocolVersion", version));
            Assert::AreEqual (std::string (McpServer::kProtocolVersion), version, L"the server's latest for one it doesn't support");
        }



        TEST_METHOD (ANotificationGetsNoResponseAndErrorsHaveTheirCodes)
        {
            Rig                rig;
            JsonValue          response;
            const JsonValue  * error    = nullptr;
            int                code     = 0;



            Assert::AreEqual (std::string(), rig.server.HandleLine (R"({"jsonrpc":"2.0","method":"notifications/initialized"})"));

            response = rig.Send ("frob", "{}");
            Assert::IsTrue   (response.HasObject ("error", error) && error->HasInt ("code", code));
            Assert::AreEqual (McpServer::kMethodNotFound, code);

            response = rig.Send ("tools/call", R"({"name":"frob"})");
            Assert::IsTrue   (response.HasObject ("error", error) && error->HasInt ("code", code));
            Assert::AreEqual (McpServer::kInvalidParams, code);

            Assert::IsTrue (rig.server.HandleLine ("not json").find ("-32700") != std::string::npos);
        }



        TEST_METHOD (EveryToolHasADescriptionAndAnObjectSchema)
        {
            Rig                rig;
            JsonValue          response = rig.Send ("tools/list", "{}");
            const JsonValue  * result   = nullptr;
            const JsonValue  * tools    = nullptr;



            if (!response.HasObject ("result", result) || !result->HasArray ("tools", tools) || tools == nullptr)
            {
                Assert::Fail (L"tools/list has a result with tools");
                return;
            }

            Assert::IsTrue (tools->GetArraySize() > 20);

            for (size_t i = 0; i < tools->GetArraySize(); i++)
            {
                const JsonValue  & tool   = tools->GetArrayElement (i);
                const JsonValue  * schema = nullptr;
                std::string        name;
                std::string        text;



                Assert::IsTrue   (tool.HasString ("name", name) && tool.HasString ("description", text) && !text.empty());
                Assert::IsTrue   (tool.HasObject ("inputSchema", schema) && schema->HasString ("type", text), TextEncoding::NarrowToWide (name).c_str());
                Assert::AreEqual (std::string ("object"), text);
            }
        }



        TEST_METHOD (ACommandIsSentAndItsReplyReturned)
        {
            Rig                rig;
            JsonValue          result;
            const JsonValue  * structured = nullptr;



            rig.directory.instances[100];

            result = rig.Call ("command", R"({"line":"R"})");
            Assert::IsFalse  (IsError (result));
            Assert::AreEqual (std::string ("ran R\n"), GetText (result));
            Assert::IsTrue   (result.HasObject ("structuredContent", structured));
            Assert::AreEqual (std::string ("R"), rig.directory.instances[100].lines.back());

            rig.directory.instances[100].failing = "BP FROB";
            result = rig.Call ("set_breakpoint", R"({"location":"FROB"})");
            Assert::IsTrue (IsError (result), L"a command that failed");
            Assert::IsTrue (GetText (result).find ("invalid arguments: No.") != std::string::npos);
        }



        TEST_METHOD (ToolsBuildTheirCommandLines)
        {
            Rig                          rig;
            std::vector<std::string>   & lines = rig.directory.instances[100].lines;



            (void) rig.Call ("read_memory",      R"({"address":"300","length":32})");
            (void) rig.Call ("write_memory",     R"({"address":"300","bytes":[169,65,96]})");
            (void) rig.Call ("set_breakpoint",   R"({"location":"FDED","condition":"A == 41"})");
            (void) rig.Call ("clear_breakpoint", R"({"all":true})");
            (void) rig.Call ("reset",            R"({"kind":"power"})");
            (void) rig.Call ("insert_disk",      R"({"path":"C:\\d\\game.dsk","drive":2})");
            (void) rig.Call ("type_text",        R"({"text":"10 PRINT \"HI\"\n"})");
            (void) rig.Call ("press_keys",       R"({"keys":["A","Return","Ctrl+C","Ctrl+Reset","Escape"]})");

            Assert::AreEqual (std::string ("D 300,20"),                   lines.at (0));
            Assert::AreEqual (std::string ("MEB 300 A9 41 60"),           lines.at (1));
            Assert::AreEqual (std::string ("BP FDED IF A == 41"),         lines.at (2));
            Assert::AreEqual (std::string ("BPC *"),                      lines.at (3));
            Assert::AreEqual (std::string ("RESET POWER"),                lines.at (4));
            Assert::AreEqual (std::string ("DISK INSERT D2 \"C:\\d\\game.dsk\""), lines.at (5));
            Assert::AreEqual (std::string ("PASTE \"10 PRINT \\\"HI\\\"\\n\""), lines.at (6));
            Assert::AreEqual (std::string ("KEY C1 8D 83"),               lines.at (7), L"the keys before Ctrl+Reset");
            Assert::AreEqual (std::string ("RESET SOFT"),                 lines.at (8));
            Assert::AreEqual (std::string ("KEY 9B"),                     lines.at (9));
        }



        TEST_METHOD (ARunWaitsForItsStopAndOnePastItsTimeoutIsPaused)
        {
            Rig        rig;
            JsonValue  result;



            rig.directory.instances[100];

            result = rig.Call ("run", R"({"until":"1234"})");
            Assert::IsFalse (IsError (result));
            Assert::IsTrue  (GetText (result).find ("Stopped: breakpoint at $1234") != std::string::npos, TextEncoding::NarrowToWide (GetText (result)).c_str());

            rig.directory.instances[100].isRunEndless = true;
            result = rig.Call ("run", R"({"timeout_seconds":1})");
            Assert::IsTrue (IsError (result), L"a run that didn't finish");
            Assert::IsTrue (GetText (result).find ("paused") != std::string::npos);
            Assert::AreEqual (std::string ("pause"), rig.directory.instances[100].types.back());
        }



        TEST_METHOD (MoreThanOneCassoNeedsAnAttach)
        {
            Rig        rig;
            JsonValue  result;



            rig.directory.instances[100];
            rig.directory.instances[200];

            result = rig.Call ("registers");
            Assert::IsTrue (IsError (result));
            Assert::IsTrue (GetText (result).find ("attach") != std::string::npos);

            Assert::IsFalse  (IsError (rig.Call ("attach", R"({"pid":200})")));
            Assert::IsFalse  (IsError (rig.Call ("registers")));
            Assert::AreEqual (std::string ("R"), rig.directory.instances[200].lines.back());
        }



        TEST_METHOD (CloseClosesOnlyACassoThisServerLaunched)
        {
            Rig                rig;
            JsonValue          result;
            const JsonValue  * structured = nullptr;
            int                processId  = 0;



            rig.directory.instances[100];

            Assert::IsTrue (IsError (rig.Call ("close")), L"the user's Casso");
            Assert::IsTrue (rig.directory.instances[100].isRunning);

            result = rig.Call ("launch", R"({"machine":"Apple2c","title":"agent"})");
            Assert::IsFalse  (IsError (result), TextEncoding::NarrowToWide (GetText (result)).c_str());
            Assert::IsTrue   (result.HasObject ("structuredContent", structured) && structured->HasInt ("pid", processId));
            Assert::AreEqual (4242, processId);
            Assert::IsTrue   (rig.launcher.launched.find (L"--debugger") != std::wstring::npos);
            Assert::IsTrue   (rig.launcher.launched.find (L"--machine \"Apple2c\"") != std::wstring::npos);

            result = rig.Call ("close");
            Assert::IsFalse  (IsError (result), TextEncoding::NarrowToWide (GetText (result)).c_str());
            Assert::AreEqual (std::string ("EXIT"), rig.directory.instances[4242].lines.back());
            Assert::IsFalse  (rig.directory.instances[4242].isRunning);
        }



        //  A quote in a value used to end its argument, so the rest of the
        //  value became switches on Casso's command line.
        TEST_METHOD (LaunchArgumentsRoundTripThroughCommandLineToArgvW)
        {
            Rig                        rig;
            JsonValue                  result;
            std::wstring               commandLine;
            int                        count       = 0;
            LPWSTR                   * argv        = nullptr;
            std::vector<std::wstring>  args;
            std::vector<std::wstring>  original    = { L"Casso.exe",
                                                       L"--debugger",
                                                       L"--title",   L"x\" --script \"C:\\evil.txt",
                                                       L"--machine", L"Apple2c\\",
                                                       L"--disk1",   L"C:\\a b\\\\",
                                                       L"--disk2",   L"a\\\\\"b\\\" --script c" };



            result = rig.Call ("launch", R"({"title":"x\" --script \"C:\\evil.txt","machine":"Apple2c\\","disk1":"C:\\a b\\\\","disk2":"a\\\\\"b\\\" --script c"})");
            Assert::IsFalse (IsError (result), TextEncoding::NarrowToWide (GetText (result)).c_str());

            commandLine = L"Casso.exe " + rig.launcher.launched;
            argv        = CommandLineToArgvW (commandLine.c_str(), &count);
            Assert::IsNotNull (argv);

            args.assign (argv, argv + count);
            LocalFree (argv);

            Assert::AreEqual (original.size(), args.size(), commandLine.c_str());

            for (size_t i = 0; i < original.size(); i++)
            {
                Assert::AreEqual (original[i], args[i], commandLine.c_str());
            }
        }



        TEST_METHOD (QuoteArgumentEscapesQuotesAndTheBackslashesBeforeThem)
        {
            Assert::AreEqual (std::wstring (L"\"\""),                          CommandLineQuoting::QuoteArgument (L""));
            Assert::AreEqual (std::wstring (L"\"C:\\Program Files\\Casso.exe\""), CommandLineQuoting::QuoteArgument (L"C:\\Program Files\\Casso.exe"));
            Assert::AreEqual (std::wstring (L"\"a\\\\b\""),                    CommandLineQuoting::QuoteArgument (L"a\\\\b"), L"backslashes before a letter are literal");
            Assert::AreEqual (std::wstring (L"\"a\\\\\\\"b\""),                CommandLineQuoting::QuoteArgument (L"a\\\"b"));
            Assert::AreEqual (std::wstring (L"\"a\\\\\""),                     CommandLineQuoting::QuoteArgument (L"a\\"));
        }



        TEST_METHOD (APausedMachineIsReportedWhereTypingWouldBeLost)
        {
            Rig        rig;
            JsonValue  result;



            rig.directory.instances[100].isPaused = true;

            result = rig.Call ("type_text", R"({"text":"RUN\n"})");
            Assert::IsTrue (GetText (result).find ("paused") != std::string::npos, TextEncoding::NarrowToWide (GetText (result)).c_str());

            (void) rig.Call ("resume");
            Assert::AreEqual (std::string ("G"), rig.directory.instances[100].lines.back());
        }



        TEST_METHOD (Base64AndPasteEscapes)
        {
            Assert::AreEqual (std::string ("TWFu"), McpTools::EncodeBase64 ({ 'M', 'a', 'n' }));
            Assert::AreEqual (std::string ("TWE="), McpTools::EncodeBase64 ({ 'M', 'a' }));
            Assert::AreEqual (std::string ("TQ=="), McpTools::EncodeBase64 ({ 'M' }));
            Assert::AreEqual (std::string ("\"A\\nB\\tC\\\\\""), McpTools::EscapePaste ("A\r\nB\tC\\"));
        }
    };
}
