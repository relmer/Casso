#include "Pch.h"

#include "Shell/AutoStatePolicy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStatePolicyTests
//
//  When a machine's automatic state is resumed and written: with the setting
//  on, at startup and on a switch to the machine, and at exit and on a switch
//  away; never with the setting off; a launch with command-line media starts
//  cold and replaces an earlier automatic state only after a prompt.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AutoStatePolicyTests)
{
public:

    TEST_METHOD (StartupResumesTheAutomaticStateWhenTheSettingIsOn)
    {
        Assert::IsTrue  (AutoStatePolicy::ShouldResumeAtStartup (true,  false, true),  L"setting on, a state, no command-line media");
        Assert::IsFalse (AutoStatePolicy::ShouldResumeAtStartup (true,  false, false), L"no state to resume");
        Assert::IsFalse (AutoStatePolicy::ShouldResumeAtStartup (false, false, true),  L"setting off");
    }


    TEST_METHOD (ALaunchWithCommandLineMediaStartsCold)
    {
        Assert::IsFalse (AutoStatePolicy::ShouldResumeAtStartup (true, true, true), L"command-line media start cold");
    }


    TEST_METHOD (ASwitchResumesTheIncomingMachineUnlessAStateIsLoading)
    {
        Assert::IsTrue  (AutoStatePolicy::ShouldResumeOnSwitch (true,  false, true),  L"a switch resumes the machine's state");
        Assert::IsFalse (AutoStatePolicy::ShouldResumeOnSwitch (true,  true,  true),  L"a switch to load a state file does not");
        Assert::IsFalse (AutoStatePolicy::ShouldResumeOnSwitch (true,  false, false), L"no state to resume");
        Assert::IsFalse (AutoStatePolicy::ShouldResumeOnSwitch (false, false, true),  L"setting off");
    }


    TEST_METHOD (TheStateIsWrittenAtExitWhenTheSettingIsOn)
    {
        Assert::IsTrue (AutoStatePolicy::GetSaveAction (true, false, true)  == AutoStateSave::Write, L"replacing an earlier state");
        Assert::IsTrue (AutoStatePolicy::GetSaveAction (true, false, false) == AutoStateSave::Write, L"the first state");
    }


    TEST_METHOD (NothingIsWrittenWhenTheSettingIsOff)
    {
        Assert::IsTrue (AutoStatePolicy::GetSaveAction (false, false, true) == AutoStateSave::Skip, L"an ordinary session");
        Assert::IsTrue (AutoStatePolicy::GetSaveAction (false, true,  true) == AutoStateSave::Skip, L"a command-line session");
    }


    TEST_METHOD (ACommandLineSessionPromptsBeforeReplacingAnEarlierState)
    {
        Assert::IsTrue (AutoStatePolicy::GetSaveAction (true, true, true)  == AutoStateSave::PromptFirst, L"an earlier state exists");
        Assert::IsTrue (AutoStatePolicy::GetSaveAction (true, true, false) == AutoStateSave::Write,       L"no earlier state to protect");
    }


    TEST_METHOD (TheAutomaticStateFileUsesTheMachinesName)
    {
        std::filesystem::path  path = AutoStatePolicy::GetAutoStatePath (L"C:\\States", L"Apple2e");



        Assert::AreEqual (std::wstring (L"C:\\States\\Apple2e.autosave.cassostate"), path.wstring());
    }


    TEST_METHOD (ACharacterNotAllowedInAFileNameIsReplaced)
    {
        std::filesystem::path  path = AutoStatePolicy::GetAutoStatePath (L"C:\\States", L"Apple //e: \"test\"");



        Assert::AreEqual (std::wstring (L"C:\\States\\Apple __e_ _test_.autosave.cassostate"), path.wstring());
    }


    TEST_METHOD (AStateThatDoesNotResumeIsDescribedByWhyItFailed)
    {
        Assert::AreEqual (std::wstring (L"Could not resume the Apple //e because its saved state is damaged."),
                          AutoStatePolicy::FormatResumeFailure (L"Apple //e", "damaged state file"));
        Assert::AreEqual (std::wstring (L"Could not resume the Apple //e because its saved state is damaged."),
                          AutoStatePolicy::FormatResumeFailure (L"Apple //e", "not a machine state"));
        Assert::AreEqual (std::wstring (L"Could not resume the Apple //e because its saved state is from a newer version of Casso."),
                          AutoStatePolicy::FormatResumeFailure (L"Apple //e", "newer state format"));
        Assert::AreEqual (std::wstring (L"Could not resume the Apple //c because its saved state is from a machine with different ROM images."),
                          AutoStatePolicy::FormatResumeFailure (L"Apple //c", "different ROM images"));
        Assert::AreEqual (std::wstring (L"Could not resume the Apple //c because its saved state is unreadable."),
                          AutoStatePolicy::FormatResumeFailure (L"Apple //c", "file not read"));
    }
};





