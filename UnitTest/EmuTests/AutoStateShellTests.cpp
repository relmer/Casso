#include "Pch.h"

#include "EmuTests/FixtureRomSource.h"
#include "EmuTests/TestMachine.h"
#include "Shell/EmulatorShell.h"
#include "Shell/HeadlessMachineFactory.h"
#include "Shell/MachineBuilder.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStateShellTests
//
//  The shell's automatic state save. Its folder is a path under a file, so
//  every write fails and nothing is written anywhere.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AutoStateShellTests)
{
public:

    //  A save that fails at exit is reported through the exit report, since
    //  a notice posted to the window being destroyed never appears; a save
    //  that fails on a switch is not.
    TEST_METHOD (AFailedSaveAtExitIsReportedThroughTheExitReport)
    {
        std::unique_ptr<EmulatorShell>   shell            = std::make_unique<EmulatorShell>();
        MachineBuildServices             services;
        MachineBuilder                   builder          (shell->GetMachine(), services);
        FixtureRomSource                 source;
        std::string                      error;
        std::vector<std::wstring>        reports;
        wchar_t                          module[MAX_PATH] = {};
        DWORD                            length           = 0;
        HRESULT                          hr               = S_OK;



        hr = HeadlessMachineFactory::Build (shell->GetMachine(), builder, source, "Apple2e", TestMachine::Slots::Empty, TestMachine::kSeed, error);
        AssertSucceeded (hr, L"the shell's machine builds");

        //  The test host's executable is a file, so a folder under it cannot
        //  be written.
        length = GetModuleFileNameW (nullptr, module, MAX_PATH);
        Assert::IsTrue (length > 0, L"the test host's path");

        shell->SetStateFolder (std::filesystem::path (module) / L"States");
        shell->SetExitReport  ([&reports] (const std::wstring & text) { reports.push_back (text); });

        shell->SaveAutoState (false);
        Assert::IsTrue (reports.empty(), L"a failure on a switch is posted as a notice");

        shell->SaveAutoState (true);
        Assert::AreEqual ((size_t) 1, reports.size(), L"a failure at exit is reported once");
        Assert::IsTrue   (reports[0].find (L"saved") != std::wstring::npos, L"as a save that failed");
    }


    //  The CPU thread releases the disk store's token as it stops, and the
    //  save at exit reads every drive, so the token is claimed on the window
    //  thread between the two.
    TEST_METHOD (TheSaveAtExitClaimsTheDisksAfterTheCpuThreadStops)
    {
        std::unique_ptr<EmulatorShell>   shell            = std::make_unique<EmulatorShell>();
        MachineBuildServices             services;
        MachineBuilder                   builder          (shell->GetMachine(), services);
        FixtureRomSource                 source;
        std::string                      error;
        std::vector<std::wstring>        reports;
        wchar_t                          module[MAX_PATH] = {};
        DWORD                            length           = 0;
        HRESULT                          hr               = S_OK;



        hr = HeadlessMachineFactory::Build (shell->GetMachine(), builder, source, "Apple2e", TestMachine::Slots::Empty, TestMachine::kSeed, error);
        AssertSucceeded (hr, L"the shell's machine builds");

        //  A folder under the test host's executable cannot be written, so
        //  nothing is saved anywhere.
        length = GetModuleFileNameW (nullptr, module, MAX_PATH);
        Assert::IsTrue (length > 0, L"the test host's path");

        shell->SetStateFolder (std::filesystem::path (module) / L"States");
        shell->SetExitReport  ([&reports] (const std::wstring & text) { reports.push_back (text); });

        //  The token's state after OnCpuThreadStop.
        shell->GetMachine().ReleaseDiskOwnership();

        shell->StopCpuThreadAndSaveState();

        Assert::IsTrue (shell->GetMachine().GetDiskStore().GetThreadOwnership().IsHeldByCurrentThread(),
            L"the window thread has the token after the save");
        Assert::AreEqual ((size_t) 1, reports.size(), L"and the save ran");
    }
};
