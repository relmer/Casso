#include "Pch.h"

#include "CommandLineParser.h"
#include "Update/PendingUpdateModel.h"
#include "Update/UpdateService.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RelaunchArgumentsTests
//
//  Which options from the original command line a relaunch after an update
//  repeats, and how they are written onto the new command line.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RelaunchArgumentsTests)
{
public:

    static std::vector<std::string> Select (std::vector<std::string> args)
    {
        std::vector<char *>  argv;



        for (std::string & arg : args)
        {
            argv.push_back (arg.data());
        }

        return CommandLineParser::SelectRelaunchArguments ((int) argv.size(), argv.data());
    }



    //  Every emulator option has its relaunch decision made, and every rule
    //  is for an option the emulator takes: a new switch cannot slip in
    //  without one.
    TEST_METHOD (EveryEmulatorOptionHasARelaunchRule)
    {
        std::set<std::string>  options;
        std::set<std::string>  ruled;



        for (const char * option : CommandLineParser::GetEmulatorLongOptions())
        {
            options.insert (option);
        }

        for (const CommandLineParser::EmulatorRelaunchRule & rule : CommandLineParser::GetEmulatorRelaunchRules())
        {
            Assert::IsTrue (ruled.insert (rule.option).second, L"one rule per option");
        }

        for (const std::string & option : options)
        {
            Assert::IsTrue (ruled.contains (option), std::format (L"--{} has no relaunch rule", std::wstring (option.begin(), option.end())).c_str());
        }

        Assert::IsTrue (options == ruled, L"no rule for an option the emulator does not take");
    }



    TEST_METHOD (SelectRelaunchArguments_KeepsOnlyTheSafeOptions)
    {
        std::vector<std::string>  kept = Select ({ "--machine", "Apple2e", "--disk1", "work.dsk", "--title", "my box",
                                                   "--seed", "0x1234", "--trace", "50M", "--tape", "a.wav",
                                                   "--no-image-watch", "--disk2", "b.dsk", "--updated",
                                                   "--cleanup-old", "77" });



        Assert::IsTrue (kept == std::vector<std::string> ({ "--title", "my box", "--trace", "50M", "--no-image-watch" }));
    }



    TEST_METHOD (SelectRelaunchArguments_ReadsValuesAsTheParserDoes)
    {
        Assert::IsTrue (Select ({ "/title", "lab" }) == std::vector<std::string> ({ "--title", "lab" }), L"a / form, canonicalized");
        Assert::IsTrue (Select ({ "--trace", "--title", "x" }) == std::vector<std::string> ({ "--trace", "--title", "x" }),
                        L"--trace's size is optional");
        Assert::IsTrue (Select ({ "--trace=20M" }) == std::vector<std::string> ({ "--trace=20M" }), L"an = form stays whole");
        Assert::IsTrue (Select ({ "--machine", "--title" }).empty(), L"--machine takes the next word as its value");
        Assert::IsTrue (Select ({ "-Embedding", "work.dsk" }).empty(), L"anything else is dropped");
    }



    TEST_METHOD (QuoteArgument_RoundTripsThroughTheWindowsParser)
    {
        std::vector<std::wstring>  samples = { L"plain", L"my box", L"", L"say \"hi\"", L"C:\\dir with space\\", L"a\\\\\"b" };
        std::wstring               line    = L"Casso.exe";
        int                        argc    = 0;
        LPWSTR                   * argv    = nullptr;



        Assert::AreEqual (std::wstring (L"plain"),      UpdateService::QuoteArgument (L"plain"));
        Assert::AreEqual (std::wstring (L"\"my box\""), UpdateService::QuoteArgument (L"my box"));

        for (const std::wstring & sample : samples)
        {
            line += L" " + UpdateService::QuoteArgument (sample);
        }

        argv = CommandLineToArgvW (line.c_str(), &argc);
        Assert::IsNotNull (argv);
        Assert::AreEqual ((int) samples.size() + 1, argc);

        for (size_t i = 0; i < samples.size(); i++)
        {
            Assert::AreEqual (samples[i], std::wstring (argv[i + 1]));
        }

        LocalFree (argv);
    }



    TEST_METHOD (MakeRelaunchArgs_AppendsTheRepeatedOptions)
    {
        Assert::AreEqual (std::wstring (L"--updated --cleanup-old 9"), UpdateService::MakeRelaunchArgs (9, {}));
        Assert::AreEqual (std::wstring (L"--updated --cleanup-old 9 --title \"my box\" --no-image-watch"),
                          UpdateService::MakeRelaunchArgs (9, { L"--title", L"my box", L"--no-image-watch" }));
        Assert::AreEqual (std::wstring (L"--updated --title lab"), UpdateService::MakeRestartArgs ({ L"--title", L"lab" }));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  PendingUpdateModelTests
//
//  What a launch does about an update left to apply when Casso closed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PendingUpdateModelTests)
{
public:

    static PendingUpdate Make (const char * version, LPCSTR kind, UpdateFailure failure = UpdateFailure::None)
    {
        PendingUpdate  pending;



        pending.version = version;
        pending.kind    = kind;
        pending.failure = failure;

        return pending;
    }



    TEST_METHOD (NothingPending_DoesNothing)
    {
        PendingLaunchAction  action = PendingUpdateModel::DecideAtLaunch (PendingUpdate(), { 1, 31, 0 });



        Assert::IsFalse (action.showUpdated || action.removeOldFiles || action.discardStaged || action.clearPending);
        Assert::IsTrue  (action.failure == UpdateFailure::None);
    }



    TEST_METHOD (ZipApplied_SaysSoAndRemovesTheOldFiles)
    {
        PendingLaunchAction  action = PendingUpdateModel::DecideAtLaunch (Make ("1.31.91", PendingUpdate::kpszZip), { 1, 31, 91 });



        Assert::IsTrue  (action.showUpdated);
        Assert::IsTrue  (action.removeOldFiles);
        Assert::IsFalse (action.discardStaged);
        Assert::IsTrue  (action.clearPending);
    }



    TEST_METHOD (ZipNeverSwapped_DiscardsTheStagedFilesQuietly)
    {
        PendingLaunchAction  action = PendingUpdateModel::DecideAtLaunch (Make ("1.31.91", PendingUpdate::kpszZip), { 1, 31, 90 });



        Assert::IsFalse (action.showUpdated, L"Casso ended without closing normally: nothing was swapped");
        Assert::IsTrue  (action.discardStaged);
        Assert::IsTrue  (action.failure == UpdateFailure::None);
        Assert::IsTrue  (action.clearPending);
    }



    TEST_METHOD (ZipFailedAtExit_IsReportedOnce)
    {
        PendingLaunchAction  action = PendingUpdateModel::DecideAtLaunch (Make ("1.31.91", PendingUpdate::kpszZip, UpdateFailure::InstallFailed), { 1, 31, 90 });



        Assert::IsTrue  (action.failure == UpdateFailure::InstallFailed);
        Assert::IsFalse (action.showUpdated);
        Assert::IsTrue  (action.discardStaged);
        Assert::IsTrue  (action.clearPending);
    }



    TEST_METHOD (Msix_StaysPendingUntilWindowsAppliesIt)
    {
        PendingLaunchAction  waiting = PendingUpdateModel::DecideAtLaunch (Make ("1.31.91", PendingUpdate::kpszMsix), { 1, 31, 90 });
        PendingLaunchAction  applied = PendingUpdateModel::DecideAtLaunch (Make ("1.31.91", PendingUpdate::kpszMsix), { 1, 31, 91 });
        PendingLaunchAction  passed  = PendingUpdateModel::DecideAtLaunch (Make ("1.31.91", PendingUpdate::kpszMsix), { 1, 32, 0 });



        Assert::IsFalse (waiting.clearPending || waiting.showUpdated || waiting.discardStaged);
        Assert::IsTrue  (applied.showUpdated && applied.clearPending && !applied.removeOldFiles);
        Assert::IsTrue  (passed.clearPending && !passed.showUpdated, L"something newer came in");
    }



    TEST_METHOD (UnreadableVersion_IsForgotten)
    {
        PendingLaunchAction  action = PendingUpdateModel::DecideAtLaunch (Make ("garbage", PendingUpdate::kpszZip), { 1, 31, 90 });



        Assert::IsTrue  (action.clearPending);
        Assert::IsFalse (action.showUpdated);
    }
};
