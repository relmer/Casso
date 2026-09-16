#include "Pch.h"

#include "Debugger/Channel/ChannelProtocol.h"
#include "Debugger/Channel/PipeSecurity.h"
#include "Debugger/Channel/Win32PipeTransport.h"
#include "MockNamedPipeApi.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace Win32PipeTransportTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WellKnownSid
    //
    //  A SID the test can name without reading any token: the local system
    //  account, which is never the user running the tests, so a descriptor that
    //  quietly used the process's own SID instead would be caught.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::vector<BYTE> WellKnownSid()
    {
        std::vector<BYTE>  sid (SECURITY_MAX_SID_SIZE);
        DWORD              size = SECURITY_MAX_SID_SIZE;
        BOOL               made = FALSE;



        made = CreateWellKnownSid (WinLocalSystemSid, nullptr, sid.data(), &size);
        Assert::IsTrue (made != FALSE, L"the test SID was made");

        sid.resize (size);
        return sid;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PipeSecurityTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PipeSecurityTests)
    {
    public:

        //  One allow entry, for the SID given, and nothing else. A second entry
        //  -- or the DACL missing entirely, which grants everyone everything --
        //  is exactly the failure this list exists to prevent.
        TEST_METHOD (AGivenSidYieldsExactlyOneAllowEntryForThatSid)
        {
            std::vector<BYTE>       sid        = WellKnownSid();
            PipeSecurityDescriptor  descriptor;
            HRESULT                 hr         = S_OK;
            ACL_SIZE_INFORMATION    info       = {};
            void                  * ace        = nullptr;
            ACCESS_ALLOWED_ACE    * allow      = nullptr;
            BOOL                    got        = FALSE;



            hr = descriptor.BuildForUser ((PSID) sid.data());
            Assert::IsTrue (SUCCEEDED (hr));
            Assert::IsNotNull (descriptor.GetDacl(), L"a DACL is present, not NULL");

            got = GetAclInformation (descriptor.GetDacl(), &info, sizeof (info), AclSizeInformation);
            Assert::IsTrue   (got != FALSE);
            Assert::AreEqual ((DWORD) 1, info.AceCount, L"exactly one entry");

            got = GetAce (descriptor.GetDacl(), 0, &ace);
            Assert::IsTrue (got != FALSE);

            allow = (ACCESS_ALLOWED_ACE *) ace;
            Assert::AreEqual ((BYTE) ACCESS_ALLOWED_ACE_TYPE, allow->Header.AceType, L"an allow entry");
            Assert::IsTrue   (EqualSid ((PSID) &allow->SidStart, (PSID) sid.data()) != FALSE, L"for that SID");
        }



        TEST_METHOD (TheAttributesCarryTheDescriptor)
        {
            std::vector<BYTE>       sid        = WellKnownSid();
            PipeSecurityDescriptor  descriptor;
            HRESULT                 hr         = S_OK;



            hr = descriptor.BuildForUser ((PSID) sid.data());
            Assert::IsTrue (SUCCEEDED (hr));

            Assert::AreEqual ((DWORD) sizeof (SECURITY_ATTRIBUTES), descriptor.GetAttributes()->nLength);
            Assert::IsNotNull (descriptor.GetAttributes()->lpSecurityDescriptor);
            Assert::IsFalse   (descriptor.GetAttributes()->bInheritHandle != FALSE, L"no child process inherits it");
        }



        TEST_METHOD (TheCurrentUserSidCanBeRead)
        {
            std::vector<BYTE>  sid;
            HRESULT            hr = PipeSecurityDescriptor::GetCurrentUserSid (sid);



            Assert::IsTrue (SUCCEEDED (hr));
            Assert::IsTrue (!sid.empty() && IsValidSid ((PSID) sid.data()) != FALSE);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Win32PipeTransportTests
    //
    //  The transport against MockNamedPipeApi. No test opens a real pipe; that
    //  Windows enforces the access list is a manual quickstart step, and the pipe
    //  end to end is the SC-007 client's job.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Win32PipeTransportTests)
    {
    public:

        class Rig
        {
        public:
            MockNamedPipeApi    api;
            Win32PipeTransport  transport;



            Rig() :
                transport (api, 1234, WellKnownSid())
            {
                api.expectedSid = WellKnownSid();
            }



            void ListenOk()
            {
                HRESULT  hr = transport.Listen();

                Assert::IsTrue (SUCCEEDED (hr), L"listening");
            }



            //  Connects the client of listener `index` and accepts it, for a
            //  test that does not need the connection's id.
            void Accept (size_t index)
            {
                ChannelConnectionId  connection = AcceptOk (index);

                Assert::IsTrue (connection != 0);
            }



            //  Connects the client of listener `index` and accepts it.
            ChannelConnectionId AcceptOk (size_t index)
            {
                ChannelConnectionId  connection = 0;
                bool                 accepted   = false;



                api.ClientConnect (index);
                accepted = transport.TryAccept (connection);

                Assert::IsTrue (accepted, L"the client was accepted");
                return connection;
            }
        };





        TEST_METHOD (ThePipeNameCarriesThePid)
        {
            Assert::AreEqual (std::wstring (L"\\\\.\\pipe\\Casso.Debug.4096"), Win32PipeTransport::GetPipeName (4096));
        }



        TEST_METHOD (ListeningCreatesTheNamedInstanceWithTheRightFlags)
        {
            Rig  rig;



            rig.ListenOk();

            Assert::AreEqual ((size_t) 1, rig.api.created.size());
            Assert::AreEqual (std::wstring (L"\\\\.\\pipe\\Casso.Debug.1234"), rig.api.created[0].name);
            Assert::IsTrue   ((rig.api.created[0].pipeMode & PIPE_REJECT_REMOTE_CLIENTS) != 0, L"remote clients refused");
            Assert::IsTrue   ((rig.api.created[0].openMode & FILE_FLAG_FIRST_PIPE_INSTANCE) != 0, L"the name cannot be squatted");
            Assert::AreEqual ((DWORD) PIPE_UNLIMITED_INSTANCES, rig.api.created[0].maxInstances);
        }



        //  Every instance refuses remote clients; only the first claims the name.
        //  A later instance carrying FILE_FLAG_FIRST_PIPE_INSTANCE would fail, and
        //  the second client could never connect.
        TEST_METHOD (OnlyTheFirstInstanceClaimsTheName)
        {
            Rig  rig;



            rig.ListenOk();
            rig.Accept (0);

            Assert::AreEqual ((size_t) 2, rig.api.created.size(), L"a second instance waits for the next client");
            Assert::IsTrue   ((rig.api.created[1].pipeMode & PIPE_REJECT_REMOTE_CLIENTS) != 0);
            Assert::IsTrue   ((rig.api.created[1].openMode & FILE_FLAG_FIRST_PIPE_INSTANCE) == 0);
        }



        TEST_METHOD (TheDescriptorAdmitsOnlyTheGivenUser)
        {
            Rig  rig;



            rig.ListenOk();

            Assert::AreEqual ((DWORD) 1, rig.api.created[0].aceCount);
            Assert::IsTrue   (rig.api.created[0].isAllowForSid);
        }



        TEST_METHOD (NothingIsAcceptedBeforeAClientArrives)
        {
            Rig                  rig;
            ChannelConnectionId  connection = 0;



            rig.ListenOk();

            Assert::IsFalse (rig.transport.TryAccept (connection));
        }



        TEST_METHOD (APendingReadDeliversNothingUntilDataArrives)
        {
            Rig                  rig;
            ChannelConnectionId  connection = 0;
            ChannelConnectionId  from       = 0;
            std::string          line;



            rig.ListenOk();
            connection = rig.AcceptOk (0);

            Assert::IsFalse (rig.transport.TryReadLine (from, line), L"the read is pending");

            rig.api.ClientSend (0, "R\n");

            Assert::IsTrue   (rig.transport.TryReadLine (from, line));
            Assert::AreEqual (connection, from);
            Assert::AreEqual (std::string ("R"), line);
        }



        TEST_METHOD (OneLineSplitAcrossReadsIsJoined)
        {
            Rig                  rig;
            ChannelConnectionId  from = 0;
            std::string          line;



            rig.ListenOk();
            rig.Accept (0);

            rig.api.ClientSend (0, "BP C");
            Assert::IsFalse (rig.transport.TryReadLine (from, line), L"half a line is not a line");

            rig.api.ClientSend (0, "600\n");
            Assert::IsTrue   (rig.transport.TryReadLine (from, line));
            Assert::AreEqual (std::string ("BP C600"), line);
        }



        TEST_METHOD (SeveralLinesInOneReadComeOutInOrder)
        {
            Rig                  rig;
            ChannelConnectionId  from = 0;
            std::string          line;



            rig.ListenOk();
            rig.Accept (0);

            rig.api.ClientSend (0, "one\ntwo\nthree\n");

            Assert::IsTrue (rig.transport.TryReadLine (from, line));  Assert::AreEqual (std::string ("one"),   line);
            Assert::IsTrue (rig.transport.TryReadLine (from, line));  Assert::AreEqual (std::string ("two"),   line);
            Assert::IsTrue (rig.transport.TryReadLine (from, line));  Assert::AreEqual (std::string ("three"), line);
            Assert::IsFalse (rig.transport.TryReadLine (from, line));
        }



        //  A client that leaves in the middle of a read is dropped, and the
        //  handles its instance held are released.
        TEST_METHOD (ADisconnectDuringAReadReleasesTheConnection)
        {
            Rig                  rig;
            ChannelConnectionId  from = 0;
            std::string          line;



            rig.ListenOk();
            rig.Accept (0);

            Assert::AreEqual ((size_t) 1, rig.transport.GetConnections().size());

            rig.api.ClientDisconnect (0);

            Assert::IsFalse  (rig.transport.TryReadLine (from, line));
            Assert::IsTrue   (rig.transport.GetConnections().empty(), L"the client is gone");
            Assert::AreEqual ((size_t) 2, rig.api.open.size(), L"only the waiting listener's pipe and event remain");
        }



        //  The last line a client sent before it left is still answered.
        TEST_METHOD (ALineSentJustBeforeLeavingIsStillDelivered)
        {
            Rig                  rig;
            ChannelConnectionId  from = 0;
            std::string          line;



            rig.ListenOk();
            rig.Accept (0);

            rig.api.ClientSend (0, "bye\n");
            rig.api.ClientDisconnect (0);

            Assert::IsTrue   (rig.transport.TryReadLine (from, line));
            Assert::AreEqual (std::string ("bye"), line);
        }



        //  Over a request's limit, the line is passed up one byte long -- enough
        //  for the protocol to report it -- and the rest of it is dropped rather
        //  than buffered.
        TEST_METHOD (ALineOverOneMebibyteIsPassedUpOnceAndTheRestDropped)
        {
            Rig                  rig;
            ChannelConnectionId  from = 0;
            std::string          line;
            std::string          huge (ChannelProtocol::kMaxLineBytes + 5000, 'x');



            rig.ListenOk();
            rig.Accept (0);

            rig.api.ClientSend (0, huge + "\nnext\n");

            for (int pump = 0; pump < 2000 && !rig.transport.TryReadLine (from, line); pump++)
            {
            }

            Assert::AreEqual (ChannelProtocol::kMaxLineBytes + 1, line.size(), L"one byte over, so the protocol refuses it");

            line.clear();
            for (int pump = 0; pump < 2000 && !rig.transport.TryReadLine (from, line); pump++)
            {
            }

            Assert::AreEqual (std::string ("next"), line, L"the tail of the long line was discarded, the next line was not");
        }



        TEST_METHOD (AWriteSendsTheLineWithANewline)
        {
            Rig                  rig;
            ChannelConnectionId  connection = 0;



            rig.ListenOk();
            connection = rig.AcceptOk (0);

            rig.transport.WriteLine (connection, "{\"type\":\"reply\"}");

            Assert::AreEqual (std::string ("{\"type\":\"reply\"}\n"), rig.api.Received (0));
        }



        //  A client that will not take its bytes is dropped rather than allowed
        //  to hold the CPU thread.
        TEST_METHOD (AStalledWriteDropsTheClient)
        {
            Rig                  rig;
            ChannelConnectionId  connection = 0;
            ChannelConnectionId  from       = 0;
            std::string          line;



            rig.ListenOk();
            connection = rig.AcceptOk (0);

            rig.api.stallWrites = true;
            rig.transport.WriteLine (connection, "hello");

            Assert::IsTrue (rig.transport.GetConnections().empty(), L"no longer counted as a client");

            Assert::IsFalse  (rig.transport.TryReadLine (from, line), L"nothing to read from a dropped client");
            Assert::AreEqual ((size_t) 2, rig.api.open.size(), L"and its handles are released");
        }



        TEST_METHOD (ClosingReleasesEverything)
        {
            Rig  rig;



            rig.ListenOk();
            rig.Accept (0);
            rig.Accept (1);

            rig.transport.Close();

            Assert::IsTrue   (rig.api.open.empty(), L"every pipe and event closed");
            Assert::AreEqual (0, rig.api.doubleCloses, L"and none closed twice");
        }



        //  A failure at each call Listen makes returns that call's error and
        //  leaves nothing open.
        TEST_METHOD (AFailureWhileListeningReleasesWhatWasAcquired)
        {
            static const char * const  kCalls[] = { "CreateEventHandle", "CreatePipeInstance", "ConnectPipe" };



            for (const char * call : kCalls)
            {
                Rig      rig;
                HRESULT  hr = S_OK;



                rig.api.FailOn (call, 1, ERROR_ACCESS_DENIED);

                hr = rig.transport.Listen();

                Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr,
                                  std::format (L"{} fails with its own error", std::wstring (call, call + strlen (call))).c_str());
                Assert::IsTrue   (rig.api.open.empty(), L"and nothing is left open");
                Assert::AreEqual (0, rig.api.doubleCloses);
            }
        }



        //  The write event is the one thing acquired on accept. If it cannot be
        //  had, the client's instance is released and the transport goes on
        //  listening for the next.
        TEST_METHOD (AFailureWhileAcceptingReleasesTheInstanceAndKeepsListening)
        {
            Rig                  rig;
            ChannelConnectionId  connection = 0;



            rig.ListenOk();

            rig.api.FailOn ("CreateEventHandle", 2, ERROR_NOT_ENOUGH_MEMORY);
            rig.api.ClientConnect (0);

            Assert::IsFalse  (rig.transport.TryAccept (connection));
            Assert::IsTrue   (rig.transport.GetConnections().empty());
            Assert::AreEqual ((size_t) 2, rig.api.open.size(), L"only the new listener's pipe and event");
            Assert::AreEqual (0, rig.api.doubleCloses);
        }



        TEST_METHOD (AFailedReadDropsTheClient)
        {
            Rig                  rig;
            ChannelConnectionId  from = 0;
            std::string          line;



            rig.ListenOk();
            rig.api.FailOn ("ReadPipe", 1, ERROR_BROKEN_PIPE);
            rig.Accept (0);

            Assert::IsFalse  (rig.transport.TryReadLine (from, line));
            Assert::IsTrue   (rig.transport.GetConnections().empty());
            Assert::AreEqual ((size_t) 2, rig.api.open.size());
        }



        TEST_METHOD (AFailedWriteDropsTheClient)
        {
            Rig                  rig;
            ChannelConnectionId  connection = 0;



            rig.ListenOk();
            connection = rig.AcceptOk (0);
            rig.api.FailOn ("WritePipe", 1, ERROR_NO_DATA);

            rig.transport.WriteLine (connection, "x");

            Assert::IsTrue (rig.transport.GetConnections().empty());
        }
    };
}
