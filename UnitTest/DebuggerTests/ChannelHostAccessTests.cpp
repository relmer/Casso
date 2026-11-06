#include "Pch.h"

#include "Cli/DebugAttachRunner.h"
#include "Core/JsonParser.h"
#include "Debugger/Channel/ChannelProtocol.h"
#include "Debugger/Channel/HostAccessGuard.h"
#include "Debugger/DebuggerController.h"
#include "EmuTests/TestMachine.h"
#include "FakeHostPathResolver.h"
#include "InMemoryPipeTransport.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/ConsoleLinks.h"
#include "UiTests/InMemoryFileSystem.h"
#include "Core/JsonValue.h"
#include "Core/TextEncoding.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerControllerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ChannelHostAccessTests
    //
    //  A debug channel client's host file access, end to end: a hello with
    //  the client's folders and scripts setting, its commands through the
    //  controller over a real machine, the reply, the files, and the console
    //  lines. C:\Work is the folder a client can be given, C:\Elsewhere is
    //  never allowed, and Documents\Casso always is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ChannelHostAccessTests)
    {
    public:
        class Rig
        {
        public:
            TestMachine               machine;
            CpuManager                cpuManager;
            InMemoryPipeTransport     transport;
            InMemoryFileSystem        files;
            FakeHostPathResolver      resolver { &files };
            DebuggerController        controller;
            std::vector<std::string>  console;
            ChannelConnectionId       connection = 0;
            int64_t                   nextId     = 1;



            Rig() :
                machine    (std::string ("Apple2e"), TestMachine::Slots::DiskOnly),
                controller (machine, InitCpu (cpuManager), transport, files, nullptr, 4242)
            {
                HRESULT  hr = S_OK;



                resolver.AddFolder (L"C:\\Work");
                resolver.AddFolder (L"C:\\Elsewhere");
                resolver.AddFolder (L"C:\\Users\\Tester\\Documents\\Casso");

                controller.SetHostPathResolver (resolver);
                controller.SetConsoleSink ([this] (const std::vector<std::string> & lines)
                {
                    for (const std::string & line : lines)
                    {
                        console.push_back (ConsoleLinks::GetShownText (line));
                    }
                });

                hr = controller.Open();
                Assert::IsTrue (SUCCEEDED (hr), L"the channel opened");

                connection = transport.Connect();
            }



            static CpuManager & InitCpu (CpuManager & cpu)
            {
                cpu.SetPaused (true);
                return cpu;
            }



            void Hello (const std::string & name, std::vector<std::wstring> allowPaths, bool allowScripts)
            {
                transport.Send (connection, ChannelProtocol::WriteHelloRequest (nextId++, ChannelClient { name, std::move (allowPaths), allowScripts }));
                controller.Pump();
            }



            //  The reply to one command line.
            JsonValue Send (const std::string & line, bool overwrite = false)
            {
                JsonValue       reply;
                JsonParseError  error;
                HRESULT         hr = S_OK;



                transport.Send (connection, DebugAttachRunner::BuildCommand (nextId++, line, "applewin", 1000, overwrite));
                controller.Pump();

                hr = JsonParser::Parse (transport.Written (connection).back(), reply, error);
                Assert::AreEqual (S_OK, hr);
                return reply;
            }



            static std::string StatusOf (const JsonValue & reply)
            {
                std::string  status;



                (void) reply.HasString ("status", status);
                return status;
            }



            static std::string DetailOf (const JsonValue & reply)
            {
                const JsonValue  * error  = nullptr;
                std::string        detail;



                if (reply.HasObject ("error", error))
                {
                    (void) error->HasString ("detail", detail);
                }

                return detail;
            }
        };



        static std::wstring Widen (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }



        static std::wstring Joined (const std::vector<std::string> & lines)
        {
            std::string  text;



            for (const std::string & line : lines)
            {
                text += line + "\n";
            }

            return Widen (text);
        }





        //  The rule, the reply's reason and both console lines.
        TEST_METHOD (AWriteOutsideTheAllowedFolders_FailsWithTheReason)
        {
            Rig        rig;
            JsonValue  reply;



            rig.Hello ("MCP client", {}, false);
            reply = rig.Send ("BSAVE C:\\Elsewhere\\x.bin 300,10");

            Assert::AreEqual (std::string ("error"), Rig::StatusOf (reply));
            Assert::AreEqual (std::string ("C:\\Elsewhere\\x.bin is outside the allowed folders."), Rig::DetailOf (reply));
            Assert::IsFalse  (rig.files.Exists (L"C:\\Elsewhere\\x.bin"), L"nothing was written");

            Assert::AreEqual ((size_t) 2, rig.console.size(), Joined (rig.console).c_str());
            Assert::AreEqual (std::string ("MCP client write C:\\Elsewhere\\x.bin"), rig.console[0]);
            Assert::AreEqual (std::string ("Failed: C:\\Elsewhere\\x.bin is outside the allowed folders."), rig.console[1]);
        }





        TEST_METHOD (AWriteInAnAllowPathFolder_IsWrittenAndListed)
        {
            Rig        rig;
            JsonValue  reply;



            rig.Hello ("MCP client", { L"C:\\Work" }, false);
            reply = rig.Send ("BSAVE C:\\Work\\x.bin 300,10");

            Assert::AreEqual (std::string ("ok"), Rig::StatusOf (reply), Widen (Rig::DetailOf (reply)).c_str());
            Assert::IsTrue   (rig.files.Exists (L"C:\\Work\\x.bin"));
            Assert::AreEqual ((size_t) 1, rig.console.size(), Joined (rig.console).c_str());
            Assert::AreEqual (std::string ("MCP client write C:\\Work\\x.bin"), rig.console[0]);
        }





        TEST_METHOD (AReadOutsideTheAllowedFolders_Fails)
        {
            Rig        rig;
            JsonValue  reply;
            HRESULT    hr = rig.files.WriteAllText (L"C:\\Elsewhere\\x.bin", std::string (16, 'A'));



            Assert::AreEqual (S_OK, hr);

            reply = rig.Send ("BLOAD C:\\Elsewhere\\x.bin 300");

            Assert::AreEqual (std::string ("error"), Rig::StatusOf (reply));
            Assert::AreEqual (std::string ("C:\\Elsewhere\\x.bin is outside the allowed folders."), Rig::DetailOf (reply));
            Assert::AreNotEqual ((Byte) 'A', rig.machine.GetMemoryBus().ReadByte (0x300), L"nothing was loaded");
        }





        TEST_METHOD (AWriteOverAnExistingFile_FailsWithoutTheOverwriteOption)
        {
            Rig          rig;
            JsonValue    reply;
            std::string  content;
            HRESULT      hr = rig.files.WriteAllText (L"C:\\Work\\x.bin", "old");



            Assert::AreEqual (S_OK, hr);
            rig.Hello ("MCP client", { L"C:\\Work" }, false);

            reply = rig.Send ("BSAVE C:\\Work\\x.bin 300,10");

            Assert::AreEqual (std::string ("error"), Rig::StatusOf (reply));
            Assert::AreEqual (std::string ("C:\\Work\\x.bin already exists. Set the overwrite option to replace it."), Rig::DetailOf (reply));

            hr = rig.files.ReadAllText (L"C:\\Work\\x.bin", content);
            Assert::AreEqual (std::string ("old"), content, L"the file is as it was");

            reply = rig.Send ("BSAVE C:\\Work\\x.bin 300,10", true);

            Assert::AreEqual (std::string ("ok"), Rig::StatusOf (reply), Widen (Rig::DetailOf (reply)).c_str());
            hr = rig.files.ReadAllText (L"C:\\Work\\x.bin", content);
            Assert::AreEqual ((size_t) 0x10, content.size(), L"with the option it was replaced");
        }





        TEST_METHOD (AScriptFails_WithoutAllowScripts)
        {
            Rig        rig;
            JsonValue  reply;
            HRESULT    hr = rig.files.WriteAllText (L"C:\\Work\\s.txt", "R\n");



            Assert::AreEqual (S_OK, hr);
            rig.Hello ("MCP client", { L"C:\\Work" }, false);

            reply = rig.Send ("RUN C:\\Work\\s.txt");

            Assert::AreEqual (std::string ("error"), Rig::StatusOf (reply));
            Assert::AreEqual (HostAccessGuard::GetScriptReason(), Rig::DetailOf (reply));
            Assert::AreEqual (std::string ("MCP client run C:\\Work\\s.txt"), rig.console.at (0));
            Assert::AreEqual ("Failed: " + HostAccessGuard::GetScriptReason(), rig.console.at (1));
        }





        TEST_METHOD (AScriptRuns_WithAllowScripts)
        {
            Rig        rig;
            JsonValue  reply;
            HRESULT    hr = rig.files.WriteAllText (L"C:\\Work\\s.txt", "R\n");



            Assert::AreEqual (S_OK, hr);
            rig.Hello ("MCP client", { L"C:\\Work" }, true);

            reply = rig.Send ("RUN C:\\Work\\s.txt");

            Assert::AreEqual (std::string ("ok"), Rig::StatusOf (reply), Widen (Rig::DetailOf (reply)).c_str());
        }





        //  The console's commands, and a script it runs, access any folder,
        //  write over a file without the option, and add no lines.
        TEST_METHOD (ConsoleCommands_AreUnaffected)
        {
            Rig      rig;
            Reply    reply;
            HRESULT  hr = rig.files.WriteAllText (L"C:\\Elsewhere\\s.txt", "BSAVE C:\\Elsewhere\\y.bin 300,10\n");



            Assert::AreEqual (S_OK, hr);
            hr = rig.files.WriteAllText (L"C:\\Elsewhere\\x.bin", "old");
            Assert::AreEqual (S_OK, hr);

            reply = rig.controller.GetSession().ExecuteLine ("BSAVE C:\\Elsewhere\\x.bin 300,10");
            Assert::IsTrue (reply.status == CommandStatus::Ok, Widen (reply.error.detail).c_str());

            reply = rig.controller.GetSession().ExecuteLine ("RUN C:\\Elsewhere\\s.txt");
            Assert::IsTrue (reply.status == CommandStatus::Ok, Widen (reply.error.detail).c_str());
            Assert::IsTrue (rig.files.Exists (L"C:\\Elsewhere\\y.bin"), L"the script's write was made");

            Assert::IsTrue (rig.console.empty(), Joined (rig.console).c_str());
            Assert::IsTrue (rig.controller.GetSession().GetHostAccessGuard() == nullptr);
        }





        //  A client without a hello, as an older one is: no extra
        //  folders, no scripts, and the default name.
        TEST_METHOD (AClientWithoutAHello_HasTheDefaults)
        {
            Rig        rig;
            JsonValue  reply;



            reply = rig.Send ("BSAVE C:\\Work\\x.bin 300,10");

            Assert::AreEqual (std::string ("C:\\Work\\x.bin is outside the allowed folders."), Rig::DetailOf (reply));
            Assert::AreEqual (std::string (HostAccessGuard::kDefaultClientName) + " write C:\\Work\\x.bin", rig.console.at (0));

            reply = rig.Send ("BSAVE C:\\Users\\Tester\\Documents\\Casso\\x.bin 300,10");
            Assert::AreEqual (std::string ("ok"), Rig::StatusOf (reply), Widen (Rig::DetailOf (reply)).c_str());
        }





        //  A loaded symbol file's folder is allowed.
        TEST_METHOD (ALoadedSymbolFilesFolder_IsAllowed)
        {
            Rig        rig;
            JsonValue  reply;



            rig.resolver.AddFolder (L"C:\\Syms");
            rig.controller.GetSession().NoteSymbolLoad (SymbolTableId::User, L"C:\\Syms\\prog.sym", 0);

            reply = rig.Send ("BSAVE C:\\Syms\\out.bin 300,10");

            Assert::AreEqual (std::string ("ok"), Rig::StatusOf (reply), Widen (Rig::DetailOf (reply)).c_str());
        }





        //  CD takes a path without a file system call, so it checks it itself,
        //  and the directory is left as it was.
        TEST_METHOD (CdOutsideTheAllowedFolders_Fails)
        {
            Rig           rig;
            JsonValue     reply;
            std::wstring  before = rig.controller.GetSession().GetCurrentDirectory();



            reply = rig.Send ("CD C:\\Elsewhere");

            Assert::AreEqual (std::string ("C:\\Elsewhere is outside the allowed folders."), Rig::DetailOf (reply));
            Assert::AreEqual (before, rig.controller.GetSession().GetCurrentDirectory());
        }





        //  The session has its file system back after the line, and a
        //  command's failure text is replaced by the reason only when the
        //  policy failed it.
        TEST_METHOD (AnAllowedFileThatIsMissing_FailsWithTheCommandsError)
        {
            Rig        rig;
            JsonValue  reply;



            rig.Hello ("MCP client", { L"C:\\Work" }, false);
            reply = rig.Send ("BLOAD C:\\Work\\missing.bin 300");

            Assert::AreEqual (std::string ("error"), Rig::StatusOf (reply));
            Assert::IsTrue   (Rig::DetailOf (reply).find ("allowed folders") == std::string::npos, Widen (Rig::DetailOf (reply)).c_str());
            Assert::IsTrue   (rig.controller.GetSession().GetFileSystem() == &rig.files);
        }
    };
}
