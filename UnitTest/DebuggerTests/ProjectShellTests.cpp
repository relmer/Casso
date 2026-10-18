#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Project/DebugProjectStore.h"
#include "EmuTests/FixtureRomSource.h"
#include "InMemoryPipeTransport.h"
#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/HeadlessMachineFactory.h"
#include "Shell/MachineBuilder.h"
#include "Ui/Debugger/ProjectSettingsDialog.h"
#include "UiTests/InMemoryFileSystem.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ProjectShellTests
{
    static constexpr const wchar_t  * s_kpszFolder = L"C:\\Projects";





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ProjectRig
    //
    //  A shell over a //e, its debugger's projects in a folder of the test's
    //  own file system. Attach and Detach open and close a debugger as the
    //  CPU thread does, over a transport of the test's.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ProjectRig
    {
    public:
        InMemoryPipeTransport            transport;
        InMemoryFileSystem               files;
        InMemoryFileSystem               channelFiles;
        std::unique_ptr<EmulatorShell>   shell    = std::make_unique<EmulatorShell>();
        DebuggerController             * debugger = nullptr;



        ProjectRig()
        {
            MachineBuildServices  services;
            MachineBuilder        builder (shell->GetMachine(), services);
            FixtureRomSource      source;
            std::string           error;
            HRESULT               hr      = S_OK;



            hr = HeadlessMachineFactory::Build (shell->GetMachine(), builder, source, "Apple2e", TestMachine::Slots::Empty, TestMachine::kSeed, error);
            AssertSucceeded (hr, L"the shell's machine builds");

            shell->GetMachine().PowerCycle();
            shell->GetDebugger().SetProjectStore (std::make_unique<DebugProjectStore> (files, s_kpszFolder));
        }



        ~ProjectRig()
        {
            Detach();
            shell.reset();
        }



        void Attach()
        {
            std::unique_ptr<DebuggerController>  controller;
            HRESULT                              hr         = S_OK;



            controller = std::make_unique<DebuggerController> (shell->GetMachine(), ControllerRig::Paused (shell->GetCpuManager()), transport, channelFiles, nullptr, 1);

            hr = controller->Open();
            AssertSucceeded (hr, L"the debugger opens");

            debugger = controller.get();
            shell->GetDebugger().AttachDebugger (std::move (controller));
            shell->GetDebugger().ServiceProject();
        }



        void Detach()
        {
            if (shell->GetDebugger().IsDebuggerOpen())
            {
                shell->GetDebugger().CloseDebugger();
            }

            debugger = nullptr;
        }



        Reply Run (const std::string & line)
        {
            Reply  reply = debugger->GetSession().ExecuteLine (line, CommandMode::AppleWin);



            shell->GetDebugger().ServiceProject();
            return reply;
        }



        std::vector<Word> GetBreakpointAddresses()
        {
            Reply                       reply  = debugger->GetSession().ExecuteLine ("BPL", CommandMode::AppleWin);
            const BreakpointListData  * list   = std::get_if<BreakpointListData> (&reply.data);
            std::vector<Word>           result;



            Assert::IsNotNull (list, L"BPL lists the breakpoints");

            for (const BreakpointInfo & info : list->breakpoints)
            {
                result.push_back (info.address);
            }

            return result;
        }
    };





    TEST_CLASS (ProjectShellTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ABreakpointComesBackWithTheNextDebugger
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ABreakpointComesBackWithTheNextDebugger)
        {
            ProjectRig  rig;



            rig.Attach();
            rig.Run ("BP 300");
            rig.Detach();

            Assert::AreEqual ((size_t) 1, rig.files.FileCount(), L"the machine's project is written as the debugger closes");

            rig.Attach();

            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0300 }, L"the breakpoint is set again");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ProjectReportsTheOpenProject
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ProjectReportsTheOpenProject)
        {
            ProjectRig            rig;
            Reply                 reply;
            const ProjectData   * data  = nullptr;



            rig.Attach();
            reply = rig.Run ("PROJECT");
            data  = std::get_if<ProjectData> (&reply.data);

            Assert::IsTrue  (reply.status == CommandStatus::Ok, L"PROJECT succeeds");
            Assert::IsNotNull (data, L"PROJECT replies with the project");
            Assert::AreEqual (std::string ("machine"), data->scope, L"with no disk, program or debug file, the scope is the machine");
            Assert::IsTrue  (data->isAutomatic, L"the project is in its automatic file");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  SaveAsWritesTheFileAndPointsTheScopeAtIt
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (SaveAsWritesTheFileAndPointsTheScopeAtIt)
        {
            ProjectRig  rig;
            Reply       reply;



            rig.Attach();
            rig.Run ("BP 300");
            reply = rig.Run ("PROJECT SAVEAS \"C:\\Work\\game.cassoproj\"");

            Assert::IsTrue (reply.status == CommandStatus::Ok, L"PROJECT SAVEAS succeeds");
            Assert::IsTrue (rig.files.Exists (L"C:\\Work\\game.cassoproj"), L"the chosen file is written");
            Assert::IsTrue (rig.files.PeekContent (L"C:\\Work\\game.cassoproj").find ("BP 0300") != std::string::npos ||
                            rig.files.PeekContent (L"C:\\Work\\game.cassoproj").find ("BP 300")  != std::string::npos,
                            L"with the breakpoint in it");

            rig.Run ("BP 400");
            rig.Detach();
            rig.Attach();

            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0300, 0x0400 }, L"the scope opens the chosen file, saved since");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  StartupCommandsRunWhenTheProjectOpens
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (StartupCommandsRunWhenTheProjectOpens)
        {
            ProjectRig  rig;



            rig.Attach();
            rig.Run ("BP 300");
            rig.shell->GetDebugger().SetDebuggerStartupCommands ({ { "BP 500", CommandMode::AppleWin } });
            rig.shell->GetDebugger().ServiceProject();
            rig.Detach();

            rig.Attach();

            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0300, 0x0500 }, L"the project's breakpoint, then the startup command's");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ABloadOpensTheBinarysProject
        //
        //  A binary new to the store starts from the breakpoints set so far;
        //  the next session's machine project has only its own, and the BLOAD
        //  brings the binary's back.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ABloadOpensTheBinarysProject)
        {
            ProjectRig            rig;
            Reply                 reply;
            const ProjectData   * data  = nullptr;



            rig.channelFiles.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.Attach();
            rig.Run ("BP 300");
            rig.Detach();

            rig.Attach();
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            rig.Run ("BP 400");
            reply = rig.Run ("PROJECT");
            data  = std::get_if<ProjectData> (&reply.data);

            Assert::IsNotNull (data, L"PROJECT replies with the project");
            Assert::AreEqual (std::string ("binary"), data->scope, L"the binary's project is open");
            Assert::AreEqual (std::string ("binary prog.bin"), data->label, L"labeled with the binary's name");

            rig.Detach();
            Assert::AreEqual ((size_t) 2, rig.files.FileCount(), L"the machine's project and the binary's");

            rig.Attach();
            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0300 }, L"the machine's project has its own breakpoint");

            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0300, 0x0400 }, L"the binary's project has both");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  MergeAddsTheLastProjectsItems
        //
        //  The binary's project replaces the machine's breakpoints with its
        //  own; PROJECT MERGE adds the machine project's back.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MergeAddsTheLastProjectsItems)
        {
            ProjectRig  rig;



            rig.channelFiles.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.Attach();
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            rig.Run ("BP 500");
            rig.Detach();

            rig.Attach();
            rig.Run ("BPC *");
            rig.Run ("BP 300");
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");

            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0500 }, L"the binary's project replaces the breakpoints");

            rig.Run ("PROJECT MERGE");

            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0500, 0x0300 }, L"the machine project's breakpoint is added");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  MergeSkipsAnItemTheProjectHas
        //
        //  A breakpoint in both projects is listed once after the merge, and
        //  a disabled one merged is disabled under the id it was given; the
        //  machine project's mode is a setting, and is not merged.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MergeSkipsAnItemTheProjectHas)
        {
            ProjectRig                  rig;
            Reply                       reply;
            const BreakpointListData  * list  = nullptr;



            rig.channelFiles.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.Attach();
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            rig.Run ("BP 500");
            rig.Detach();

            rig.Attach();
            rig.Run ("BPC *");
            rig.Run ("BP 500");
            rig.Run ("BP 300");
            rig.Run ("BP 400");
            rig.Run ("BPD 2");
            rig.Run ("MODE CASSO");
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            rig.Run ("MODE APPLEWIN");
            rig.Run ("PROJECT MERGE");

            Assert::IsTrue (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0500, 0x0300, 0x0400 }, L"$0500 is not added twice");

            reply = rig.debugger->GetSession().ExecuteLine ("BPL", CommandMode::AppleWin);
            list  = std::get_if<BreakpointListData> (&reply.data);

            Assert::IsNotNull (list);
            Assert::IsTrue    (list->breakpoints[0].enabled && list->breakpoints[1].enabled, L"the BPD of the machine project's third is not run on the binary's");
            Assert::IsFalse   (list->breakpoints[2].enabled, L"the merged $0400 is disabled");
            Assert::IsTrue    (rig.debugger->GetSession().GetMode() == CommandMode::AppleWin, L"the machine project's mode is not merged");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ALoadTheUserMakesOffersTheItems
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ALoadTheUserMakesOffersTheItems)
        {
            ProjectRig  rig;



            rig.channelFiles.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.Attach();
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            rig.Run ("BP 500");
            rig.Detach();

            rig.Attach();
            rig.Run ("BP 300");
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");

            Assert::IsTrue (rig.shell->GetDebugger().HasMergeOffer(), L"the machine project's breakpoint is offered");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ALoadTheProjectMakesOffersNothing
        //
        //  The machine project's startup BLOAD opens the binary's project as
        //  part of restoring the machine's, so its breakpoint is not offered.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ALoadTheProjectMakesOffersNothing)
        {
            ProjectRig            rig;
            Reply                 reply;
            const ProjectData   * data  = nullptr;



            rig.channelFiles.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.Attach();
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            rig.Run ("BP 500");
            rig.Detach();

            rig.Attach();
            rig.Run ("BP 300");
            rig.shell->GetDebugger().SetDebuggerStartupCommands ({ { "BLOAD C:\\Work\\prog.bin 300", CommandMode::AppleWin } });
            rig.shell->GetDebugger().ServiceProject();
            rig.Detach();

            rig.Attach();
            rig.shell->GetDebugger().ServiceProject();
            reply = rig.Run ("PROJECT");
            data  = std::get_if<ProjectData> (&reply.data);

            Assert::IsNotNull (data, L"PROJECT replies with the project");
            Assert::AreEqual (std::string ("binary"), data->scope, L"the startup BLOAD opens the binary's project");
            Assert::IsFalse  (rig.shell->GetDebugger().HasMergeOffer(), L"nothing is offered");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  AProjectWithOnlySettingsOffersNothing
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AProjectWithOnlySettingsOffersNothing)
        {
            ProjectRig  rig;



            rig.channelFiles.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));

            rig.Attach();
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            rig.Run ("BP 500");
            rig.Detach();

            rig.Attach();
            rig.Run ("BLOAD C:\\Work\\prog.bin 300");

            Assert::IsTrue  (rig.GetBreakpointAddresses() == std::vector<Word> { 0x0500 }, L"the binary's project is open");
            Assert::IsFalse (rig.shell->GetDebugger().HasMergeOffer(), L"the machine project has no items, so nothing is offered");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  NoStoreMeansNoProject
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NoStoreMeansNoProject)
        {
            ProjectRig  rig;
            Reply       reply;



            rig.shell->GetDebugger().SetProjectStore (nullptr);
            rig.Attach();
            reply = rig.Run ("PROJECT");

            Assert::IsTrue (reply.status == CommandStatus::Error, L"PROJECT fails without a store");
            Assert::AreEqual ((size_t) 0, rig.files.FileCount(), L"and nothing is written");
        }
    };





    TEST_CLASS (ProjectSettingsDialogTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////////////
        //
        //  ARowIsACommandInItsMode
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ARowIsACommandInItsMode)
        {
            DebugProject::StartupCommand  command;
            std::wstring                  error;



            Assert::IsTrue   (ProjectSettingsDialog::TryMakeCommand (L"  300L ", L"monitor", command, error), L"a mode in any case");
            Assert::AreEqual (std::string ("300L"), command.text, L"trimmed");
            Assert::IsTrue   (command.mode == CommandMode::Monitor);

            Assert::IsTrue   (ProjectSettingsDialog::TryMakeCommand (L"BP 300", L"", command, error), L"no mode is AppleWin");
            Assert::IsTrue   (command.mode == CommandMode::AppleWin);

            Assert::IsFalse  (ProjectSettingsDialog::TryMakeCommand (L"   ", L"Casso", command, error), L"no text");
            Assert::IsFalse  (error.empty());

            Assert::IsFalse  (ProjectSettingsDialog::TryMakeCommand (L"BP 300", L"Forth", command, error), L"no such mode");
        }
    };
}
