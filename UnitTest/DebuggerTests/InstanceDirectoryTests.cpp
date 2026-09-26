#include "Pch.h"

#include "Cli/DebugAttachRunner.h"
#include "Debugger/Channel/IInstanceDirectory.h"
#include "Debugger/Channel/InstanceLister.h"
#include "Debugger/Channel/Win32InstanceDirectory.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace InstanceDirectoryTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ScriptedClient
    //
    //  A channel that answers each request with the records a test queued for
    //  it, in order. A request with nothing queued gets no answer at all, which
    //  is how an instance that never replies is modeled.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ScriptedClient : public IChannelClient
    {
    public:
        std::vector<std::string>               sent;
        std::deque<std::vector<std::string>>   answers;
        bool                                   closeAfterAnswers = false;



        bool WriteLine (const std::string & line) override
        {
            if (m_isClosed)
            {
                return false;
            }

            sent.push_back (line);

            if (!answers.empty())
            {
                for (const std::string & record : answers.front())
                {
                    m_incoming.push_back (record);
                }

                answers.pop_front();
            }
            else if (closeAfterAnswers)
            {
                m_isClosed = true;
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

        bool IsClosed() const override
        {
            return m_isClosed && m_incoming.empty();
        }

    private:
        std::deque<std::string>  m_incoming;
        bool                     m_isClosed = false;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ScriptedDirectory
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ScriptedDirectory : public IInstanceDirectory
    {
    public:
        std::vector<uint32_t>                          ids;
        std::map<uint32_t, std::vector<std::string>>   hellos;     // absent: refuses; empty: never answers



        std::vector<uint32_t> ListProcessIds() override
        {
            return ids;
        }

        std::unique_ptr<IChannelClient> Connect (uint32_t processId) override
        {
            auto                             found  = hellos.find (processId);
            std::unique_ptr<ScriptedClient>  client;



            if (found == hellos.end())
            {
                return nullptr;
            }

            client = std::make_unique<ScriptedClient>();

            if (!found->second.empty())
            {
                client->answers.push_back (found->second);
            }

            return client;
        }
    };





    static std::string Hello (uint32_t pid, const std::string & title, const std::string & machine, const std::string & disks)
    {
        return std::format (R"({{"type":"hello","id":1,"protocol":1,"pid":{},"title":"{}","machine":"{}","disks":{},"mode":"applewin","state":"running"}})",
                            pid, title, machine, disks);
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InstanceListerTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InstanceListerTests)
    {
    public:

        //  Only an instance that connects and answers is listed. One that
        //  refuses is another user's; one that never answers cannot be
        //  debugged. Either would offer `--attach` a pid it then fails on.
        TEST_METHOD (OnlyInstancesThatAnswerAreListed)
        {
            ScriptedDirectory            directory;
            std::vector<ListedInstance>  listed;



            directory.ids    = { 100, 200, 300 };
            directory.hellos = { { 100, { Hello (100, "one", "Apple //e", R"(["C:\\game.woz",null])") } },
                                 { 300, {} } };

            listed = InstanceLister::List (directory, 50);

            Assert::AreEqual ((size_t) 1, listed.size(), L"200 refused and 300 never answered");
            Assert::AreEqual ((uint32_t) 100, listed[0].processId);
            Assert::AreEqual (std::string ("one"), listed[0].title);
            Assert::AreEqual (std::string ("C:\\game.woz"), *listed[0].disks[0]);
            Assert::IsFalse  (listed[0].disks[1].has_value());
        }



        //  A notification that happened to arrive first is read past.
        TEST_METHOD (ARecordBeforeTheHelloIsReadPast)
        {
            ScriptedDirectory            directory;
            std::vector<ListedInstance>  listed;



            directory.ids    = { 7 };
            directory.hellos = { { 7, { R"({"type":"resumed"})", Hello (7, "", "Apple ][", "[]") } } };

            listed = InstanceLister::List (directory, 50);

            Assert::AreEqual ((size_t) 1, listed.size());
            Assert::AreEqual (std::string ("Apple ]["), listed[0].machine);
        }



        TEST_METHOD (TheColumnsArePidTitleMachineAndTwoDisks)
        {
            std::vector<ListedInstance>  instances (2);
            std::string                  text;



            instances[0].processId = 12;
            instances[0].title     = "my game";
            instances[0].machine   = "Apple //e";
            instances[0].disks     = { std::string ("C:\\a.woz"), std::nullopt };

            instances[1].processId = 34;

            text = InstanceLister::Format (instances);

            Assert::AreEqual (std::string ("pid\ttitle\tmachine\tdisk1\tdisk2\n"
                                           "12\tmy game\tApple //e\tC:\\a.woz\t-\n"
                                           "34\t-\t-\t-\t-\n"), text,
                              L"tab-separated, so a title with spaces stays one field, and empty fields print as -");
        }



        //  --list --json prints each instance's hello record, one per line.
        TEST_METHOD (TheJsonListingIsTheHelloRecords)
        {
            std::vector<ListedInstance>  instances (2);
            const std::string            first  = R"({"type":"hello","id":0,"protocol":1,"pid":12,"title":"my game"})";
            const std::string            second = R"({"type":"hello","id":0,"protocol":1,"pid":34})";



            Assert::IsTrue (InstanceLister::TryParseHello (first,  instances[0]));
            Assert::IsTrue (InstanceLister::TryParseHello (second, instances[1]));

            Assert::AreEqual (first + "\n" + second + "\n", InstanceLister::FormatJson (instances));
        }



        TEST_METHOD (APipeNameCarriesItsProcessId)
        {
            uint32_t  pid = 0;



            Assert::IsTrue   (Win32InstanceDirectory::TryParsePipeName (L"Casso.Debug.4242", pid));
            Assert::AreEqual ((uint32_t) 4242, pid);

            Assert::IsFalse  (Win32InstanceDirectory::TryParsePipeName (L"Casso.Debug.",      pid), L"no number");
            Assert::IsFalse  (Win32InstanceDirectory::TryParsePipeName (L"Casso.Debug.12x",   pid), L"not all digits");
            Assert::IsFalse  (Win32InstanceDirectory::TryParsePipeName (L"Other.Debug.12",    pid), L"someone else's pipe");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugAttachRunnerTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugAttachRunnerTests)
    {
    public:

        static CommandLineOptions::DebugOptions Options (std::vector<std::string> commands)
        {
            CommandLineOptions::DebugOptions  options;



            options.commands       = std::move (commands);
            options.isAttach       = true;
            options.timeoutSeconds = 1;
            return options;
        }



        TEST_METHOD (EveryCommandCarriesTheBudget)
        {
            ScriptedClient     client;
            DebugAttachResult  result;
            auto               options = Options ({ "r" });



            options.maxCycles = 12345;
            client.answers.push_back ({ R"({"type":"reply","id":1,"status":"ok","command":"r","text":["A:00"]})" });

            DebugAttachRunner::Run (client, options, "", result);

            Assert::AreEqual ((size_t) 1, client.sent.size());
            Assert::IsTrue   (client.sent[0].find ("\"budget\":12345") != std::string::npos, L"the budget went with the command");
            Assert::AreEqual (DebugAttachRunner::kOk, result.exitStatus);
            Assert::IsTrue   (result.output.find ("A:00") != std::string::npos, L"and the reply's text was printed");
        }



        //  A MODE line switches the lines after it, as in batch: they are
        //  sent in that mode and echoed behind its prompt.
        TEST_METHOD (AModeLineSwitchesTheLinesAfterIt)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            client.answers.push_back ({ R"({"type":"reply","id":1,"status":"ok","command":"mode monitor","text":[],"data":{"kind":"mode","mode":"monitor"}})" });
            client.answers.push_back ({ R"({"type":"reply","id":2,"status":"ok","command":"300","text":[]})" });

            DebugAttachRunner::Run (client, Options ({ "mode monitor", "300" }), "", result);

            Assert::AreEqual ((size_t) 2, client.sent.size());
            Assert::IsTrue   (client.sent[1].find ("\"mode\":\"monitor\"") != std::string::npos, L"the next line went in Monitor mode");
            Assert::IsTrue   (result.output.find ("*300") != std::string::npos, L"behind the Monitor's prompt");
        }



        //  A blank line in a script is sent, so an A block can end.
        TEST_METHOD (ABlankLineIsSent)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            for (int id = 1; id <= 3; id++)
            {
                client.answers.push_back ({ std::format (R"({{"type":"reply","id":{},"status":"ok","command":"","text":[]}})", id) });
            }

            DebugAttachRunner::Run (client, Options ({}), "a 300\nlda #1\n\n", result);

            Assert::AreEqual ((size_t) 3, client.sent.size(), L"the blank line was the third line sent");
        }



        //  A reply marked running is followed by the stop it names before the
        //  next line is sent, so a script's lines act on a stopped machine.
        TEST_METHOD (ARunIsWaitedForBeforeTheNextLine)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            client.answers.push_back ({ R"({"type":"reply","id":1,"status":"ok","command":"g","text":[],"running":true})",
                                        R"({"type":"stopped","reason":"breakpoint","pc":768,"causeId":1})" });
            client.answers.push_back ({ R"({"type":"reply","id":2,"status":"ok","command":"r","text":["A:01"]})" });

            DebugAttachRunner::Run (client, Options ({ "g", "r" }), "", result);

            Assert::AreEqual (DebugAttachRunner::kOk, result.exitStatus);
            Assert::IsTrue   (result.output.find ("Stopped: breakpoint at $0300") < result.output.find (">r"),
                              L"the stop was reported before the next line ran");
        }



        //  A run that outlasts --timeout is paused, so the machine is not left
        //  running after the script has gone, and the status is 3.
        TEST_METHOD (ARunThatOutlastsTheTimeoutIsPausedAndExitsThree)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            client.answers.push_back ({ R"({"type":"reply","id":1,"status":"ok","command":"g","text":[],"running":true})" });
            client.answers.push_back ({ R"({"type":"reply","id":2,"status":"ok","command":"pause","text":[]})",
                                        R"({"type":"stopped","reason":"pause","pc":4096,"causeId":1})" });

            DebugAttachRunner::Run (client, Options ({ "g" }), "", result);

            Assert::AreEqual ((size_t) 2, client.sent.size());
            Assert::IsTrue   (client.sent[1].find ("\"type\":\"pause\"") != std::string::npos, L"a pause was sent");
            Assert::AreEqual (DebugAttachRunner::kRunUnfinished, result.exitStatus);
        }



        TEST_METHOD (ABudgetStopExitsThree)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            client.answers.push_back ({ R"({"type":"reply","id":1,"status":"ok","command":"g","text":[],"running":true})",
                                        R"({"type":"stopped","reason":"budget","pc":1,"causeId":1})" });

            DebugAttachRunner::Run (client, Options ({ "g" }), "", result);

            Assert::AreEqual (DebugAttachRunner::kRunUnfinished, result.exitStatus);
        }



        TEST_METHOD (AFailedCommandExitsOne)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            client.answers.push_back ({ R"({"type":"reply","id":1,"status":"unknown","command":"zz","text":["Error"]})" });

            DebugAttachRunner::Run (client, Options ({ "zz" }), "", result);

            Assert::AreEqual (DebugAttachRunner::kCommandFailed, result.exitStatus);
        }



        TEST_METHOD (AClosedChannelExitsTwo)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            client.closeAfterAnswers = true;

            DebugAttachRunner::Run (client, Options ({ "r" }), "", result);

            Assert::AreEqual (DebugAttachRunner::kChannelClosed, result.exitStatus);
        }



        TEST_METHOD (CommentsAndBlankLinesAreNotSent)
        {
            ScriptedClient     client;
            DebugAttachResult  result;



            client.answers.push_back ({ R"({"type":"reply","id":1,"status":"ok","command":"r","text":[]})" });

            DebugAttachRunner::Run (client, Options ({}), "; a comment\n\n   \nr\n", result);

            Assert::AreEqual ((size_t) 1, client.sent.size());
        }



        TEST_METHOD (TheLineIsEscapedAsJson)
        {
            std::string  request = DebugAttachRunner::BuildCommand (4, "echo \"hi\"\\", "applewin", 10);



            Assert::IsTrue (request.find (R"("line":"echo \"hi\"\\")") != std::string::npos);
        }
    };
}
