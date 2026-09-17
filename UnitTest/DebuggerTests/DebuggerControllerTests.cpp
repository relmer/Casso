#include "Pch.h"

#include "Debugger/DebuggerController.h"
#include "EmuTests/TestMachine.h"
#include "InMemoryPipeTransport.h"
#include "Shell/CpuManager.h"
#include "UiTests/InMemoryFileSystem.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerControllerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerControllerTests
    //
    //  The debugger inside the running emulator, over a real machine and the
    //  in-memory transport, so a whole client conversation runs with no pipe.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerControllerTests)
    {
    public:

        class Rig
        {
        public:
            TestMachine            machine;
            CpuManager             cpuManager;
            InMemoryPipeTransport  transport;
            InMemoryFileSystem     files;
            DebuggerController     controller;



            explicit Rig (bool paused = false) :
                machine    (std::string ("Apple2e"), TestMachine::Slots::Empty),
                controller (machine, InitCpu (cpuManager, paused), transport, files,
                            [] (ChannelHello & hello) { hello.title = "test"; hello.machine = "Apple2e"; },
                            4242)
            {
            }



            static CpuManager & InitCpu (CpuManager & cpu, bool paused)
            {
                cpu.SetPaused (paused);
                return cpu;
            }



            //  Opens, connects one client, sends one line, and pumps it through.
            ChannelConnectionId OpenAndSend (const std::string & line)
            {
                HRESULT              hr         = controller.Open();
                ChannelConnectionId  connection = 0;



                Assert::IsTrue (SUCCEEDED (hr), L"the channel opened");

                connection = transport.Connect();
                transport.Send (connection, line);
                controller.Pump();

                return connection;
            }
        };





        TEST_METHOD (OpeningStartsListening)
        {
            Rig      rig;
            HRESULT  hr = S_OK;



            Assert::IsFalse (rig.transport.IsListening());

            hr = rig.controller.Open();

            Assert::IsTrue (SUCCEEDED (hr));
            Assert::IsTrue (rig.controller.IsOpen());
            Assert::IsTrue (rig.transport.IsListening());
        }





        //  Closing stops the server and drops clients, after telling them so.
        TEST_METHOD (ClosingDisconnectsClientsAfterSayingSo)
        {
            Rig                  rig;
            ChannelConnectionId  connection = rig.OpenAndSend (R"({"type":"command","id":1,"line":"R"})");
            size_t               before     = 0;



            before = rig.transport.Written (connection).size();

            rig.controller.Close();

            Assert::IsFalse (rig.controller.IsOpen());
            Assert::IsFalse (rig.transport.IsListening(), L"no longer accepting");
            Assert::IsTrue  (rig.transport.Written (connection).size() > before, L"a closing record went out first");
            Assert::IsTrue  (rig.transport.Written (connection).back().find ("closing") != std::string::npos);
            Assert::IsTrue  (rig.transport.GetConnections().empty(), L"and every client was dropped");
        }





        //  A debugger closed to free the pipe is not a request to forget what
        //  the user set up: breakpoints stay armed and a paused machine stays
        //  paused.
        TEST_METHOD (ClosingLeavesBreakpointsAndPauseAlone)
        {
            Rig                  rig (true);
            ChannelConnectionId  connection = 0;



            connection = rig.OpenAndSend (R"({"type":"command","id":1,"line":"BP C600"})");
            Assert::IsTrue (connection != 0);

            Assert::AreEqual ((size_t) 1, rig.controller.GetSession().GetBreakpoints().GetAll().size(),
                              L"the client set a breakpoint");

            rig.controller.Close();

            Assert::AreEqual ((size_t) 1, rig.controller.GetSession().GetBreakpoints().GetAll().size(),
                              L"closing left it armed");
            Assert::IsTrue   (rig.cpuManager.IsPaused(), L"and left the machine paused");
        }





        //  `--debugger` opens at start without pausing: a debugger attaching to a
        //  running program must not stop it.
        TEST_METHOD (OpeningDoesNotPauseARunningMachine)
        {
            Rig      rig (false);
            HRESULT  hr = S_OK;



            hr = rig.controller.Open();

            Assert::IsTrue  (SUCCEEDED (hr));
            Assert::IsFalse (rig.cpuManager.IsPaused());
            Assert::IsTrue  (rig.controller.GetSession().GetRunState() == RunState::FreeRunning);
        }





        TEST_METHOD (TheHandshakeDescribesThisInstance)
        {
            Rig           rig (true);
            ChannelHello  hello = rig.controller.GetInstance();



            Assert::AreEqual ((uint32_t) 4242, hello.pid);
            Assert::AreEqual (1, hello.protocol);
            Assert::AreEqual (std::string ("test"),    hello.title);
            Assert::AreEqual (std::string ("Apple2e"), hello.machine);
            Assert::IsTrue   (hello.isPaused);

            //  The machine's drives are reported, empty ones as null.
            Assert::IsFalse  (hello.disks.empty(), L"the drives are listed");
            Assert::IsFalse  (hello.disks[0].has_value(), L"an empty drive reads as null");
        }





        //  A mode a request names applies to that line only. Switching the
        //  session's mode would change it for every other client.
        TEST_METHOD (ARequestModeDoesNotSwitchTheSession)
        {
            Rig    rig (true);
            Reply  reply;



            //  `300.30F` is an examine in the Monitor and not a command at all
            //  in AppleWin, so its status says which mode the line ran in.
            reply = rig.controller.RunLine ("300.30F", CommandMode::Monitor, {});
            Assert::IsTrue (reply.status == CommandStatus::Ok, L"the line ran in the mode the request named");

            Assert::IsTrue (rig.controller.GetSession().GetMode() == CommandMode::AppleWin,
                            L"and the session's own mode is unchanged");

            reply = rig.controller.RunLine ("300.30F", {}, {});
            Assert::IsTrue (reply.status == CommandStatus::Unknown, L"a request naming no mode uses the session's");
        }





        //  A budget a request carries applies to that line's run, and the
        //  session's own budget comes back afterwards.
        TEST_METHOD (ARequestBudgetIsPutBackAfterTheLine)
        {
            Rig    rig (true);
            Reply  reply;



            rig.controller.GetSession().SetBudget (5000);

            reply = rig.controller.RunLine ("R", {}, 123);
            Assert::IsFalse (reply.command.empty(), L"the line ran");

            Assert::IsTrue   (rig.controller.GetSession().GetBudget().has_value());
            Assert::AreEqual ((uint64_t) 5000, *rig.controller.GetSession().GetBudget());
        }





        TEST_METHOD (APauseWithNoRunStopsTheMachineAndSaysSo)
        {
            Rig  rig (false);



            rig.controller.RequestPause();

            Assert::IsTrue (rig.cpuManager.IsPaused());
            Assert::IsTrue (rig.controller.GetSession().GetRunState() == RunState::Paused);
        }



        //  A running Casso's debugger reads and writes host files: SYM LOAD in the
        //  window or from a pipe client goes through the file system it was built
        //  with, as it does in batch mode.
        TEST_METHOD (FileCommandsUseTheFileSystemItWasGiven)
        {
            Rig    rig (true);
            Reply  reply;



            rig.files.WriteAllText (L"C:\\syms\\demo.dbg", "Speak=$6067\n");

            reply = rig.controller.GetSession().ExecuteLine ("SYM LOAD \"C:\\syms\\demo.dbg\"", CommandMode::AppleWin);
            Assert::IsTrue (reply.status == CommandStatus::Ok, std::wstring (reply.error.detail.begin(), reply.error.detail.end()).c_str());

            reply = rig.controller.GetSession().ExecuteLine ("BSAVE \"C:\\out\\zp.bin\" 0:F", CommandMode::AppleWin);
            Assert::IsTrue (reply.status == CommandStatus::Ok, std::wstring (reply.error.detail.begin(), reply.error.detail.end()).c_str());
            Assert::IsTrue (rig.files.Exists (L"C:\\out\\zp.bin"), L"BSAVE wrote through it");

            reply = rig.controller.GetSession().ExecuteLine ("BP Speak", CommandMode::AppleWin);
            Assert::IsTrue (reply.status == CommandStatus::Ok, L"the loaded symbol resolves");
        }
    };
}
