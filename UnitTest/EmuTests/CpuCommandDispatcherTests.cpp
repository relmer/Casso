#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"

#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CpuCommandDispatcherTests
//
//  What each command asks the CPU thread for, read off a target that only
//  writes down what it was told.
//
//  The dispatcher is the one place the UI thread's intent becomes a call:
//  which drive a command id means, what a comma-separated payload is a
//  triple of, in which order a reset remounts and resets. Each of those was
//  a line in a switch statement inside the shell, reachable only by running
//  the emulator; here each is one assertion.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CpuCommandDispatcherTests)
{
public:

    TEST_METHOD (InsertMountsTheDriveTheIdNames)
    {
        Notebook  target;

        Dispatch (IDM_DISK_INSERT1, "C:\\Disks\\one.woz", target);
        Dispatch (IDM_DISK_INSERT2, "C:\\Disks\\two.dsk", target);

        Assert::AreEqual ((size_t) 2, target.calls.size());
        Assert::AreEqual (std::string ("MountDisk 0 C:\\Disks\\one.woz"), target.calls[0]);
        Assert::AreEqual (std::string ("MountDisk 1 C:\\Disks\\two.dsk"), target.calls[1]);
    }


    TEST_METHOD (EjectEmptiesTheDriveTheIdNames)
    {
        Notebook  target;

        Dispatch (IDM_DISK_EJECT2, "", target);
        Dispatch (IDM_DISK_EJECT1, "", target);

        Assert::AreEqual (std::string ("EjectDisk 1"), target.calls[0]);
        Assert::AreEqual (std::string ("EjectDisk 0"), target.calls[1]);
    }


    TEST_METHOD (ResetRemountsBeforeItResets_AndPowerCycleRemountsAfter)
    {
        Notebook  target;

        //  A reset reads the disks back first so a regenerated image is what
        //  boots. A power cycle points the drives at their empty sentinels,
        //  so its remount has to come after or the drives come up empty.
        Dispatch (IDM_MACHINE_RESET, "", target);
        Dispatch (IDM_MACHINE_POWERCYCLE, "", target);

        Assert::AreEqual (std::string ("RemountDisks"), target.calls[0]);
        Assert::AreEqual (std::string ("SoftReset"),    target.calls[1]);
        Assert::AreEqual (std::string ("PowerCycle"),   target.calls[2]);
        Assert::AreEqual (std::string ("RemountDisks"), target.calls[3]);
    }


    TEST_METHOD (AResetCarriesTheAppleKeysThatWereDownWhenItWasAskedFor)
    {
        Notebook  target;

        //  The keys are held before the reset, so the firmware's read of them
        //  a few hundred cycles after /RESET sees what the person was holding
        //  at the click, not whatever the host reports by the time the CPU
        //  thread gets there.
        Dispatch (IDM_MACHINE_RESET, "open ", target);

        Assert::AreEqual (std::string ("HoldAppleKeys open "), target.calls[0]);
        Assert::AreEqual (std::string ("RemountDisks"),        target.calls[1]);
        Assert::AreEqual (std::string ("SoftReset"),           target.calls[2]);

        target.calls.clear();
        Dispatch (IDM_MACHINE_RESET, "open closed", target);
        Assert::AreEqual (std::string ("HoldAppleKeys open closed"), target.calls[0]);
    }


    TEST_METHOD (OpenSwitchesToTheMachineInThePayload)
    {
        Notebook  target;

        Dispatch (IDM_FILE_OPEN, "Apple2c", target);

        Assert::AreEqual (std::string ("SwitchMachine Apple2c"), target.calls[0]);
    }


    TEST_METHOD (StepRetiresOneInstruction)
    {
        Notebook  target;

        Dispatch (IDM_MACHINE_STEP, "", target);

        Assert::AreEqual ((size_t) 1, target.calls.size());
        Assert::AreEqual (std::string ("StepInstruction"), target.calls[0]);
    }


    TEST_METHOD (WriteProtectReadsAOneAsOnAndAnythingElseAsOff)
    {
        Notebook  target;

        Dispatch (IDM_DISK_WRITEPROTECT1, "1", target);
        Dispatch (IDM_DISK_WRITEPROTECT2, "0", target);
        Dispatch (IDM_DISK_WRITEPROTECT2, "",  target);

        Assert::AreEqual (std::string ("SetDriveUserWriteProtect 0 on"),  target.calls[0]);
        Assert::AreEqual (std::string ("SetDriveUserWriteProtect 1 off"), target.calls[1]);
        Assert::AreEqual (std::string ("SetDriveUserWriteProtect 1 off"), target.calls[2]);
    }


    TEST_METHOD (TheImageWriteProtectToggleAndSalvageNameTheirDrive)
    {
        Notebook  target;

        Dispatch (IDM_DISK_WP2,      "", target);
        Dispatch (IDM_DISK_SALVAGE1, "", target);

        Assert::AreEqual (std::string ("ToggleImageWriteProtect 1"), target.calls[0]);
        Assert::AreEqual (std::string ("RunSalvageFlow 0"),          target.calls[1]);
    }


    TEST_METHOD (AChangeResolutionCarriesThePathWithItsSpaces)
    {
        Notebook  target;

        //  "<slot> <drive> <action> <path>": the path is last and takes the
        //  rest of the line, so a folder with a space in it survives.
        Dispatch (IDM_DISK_RESOLVE_CHANGE, "6 1 2 C:\\My Disks\\lost copy.dsk", target);

        Assert::AreEqual (std::string ("ResolvePendingChange 6 1 2 C:\\My Disks\\lost copy.dsk"),
                          target.calls[0]);
    }


    TEST_METHOD (AChangeResolutionThatDoesNotParseAsksForNothing)
    {
        Notebook  target;

        Dispatch (IDM_DISK_RESOLVE_CHANGE, "six one two C:\\x.dsk", target);
        Dispatch (IDM_DISK_RESOLVE_CHANGE, "", target);

        Assert::IsTrue (target.calls.empty(), L"garbage is dropped, not guessed at");
    }


    TEST_METHOD (DriveAudioSettingsArriveAsUnitValues)
    {
        Notebook  target;

        Dispatch (IDM_AUDIO_DRIVE_ENABLE,    "",          target);
        Dispatch (IDM_AUDIO_DRIVE_DISABLE,   "",          target);
        Dispatch (IDM_AUDIO_DRIVE_MECHANISM, "alps",      target);
        Dispatch (IDM_AUDIO_DRIVE_VOLUMES,   "50,25,100", target);
        Dispatch (IDM_AUDIO_DRIVE_PAN,       "-100,50",   target);
        Dispatch (IDM_AUDIO_DRIVE_TEST,      "1,2",       target);

        Assert::AreEqual (std::string ("SetDriveAudioEnabled on"),         target.calls[0]);
        Assert::AreEqual (std::string ("SetDriveAudioEnabled off"),        target.calls[1]);
        Assert::AreEqual (std::string ("SetDriveAudioMechanism alps"),     target.calls[2]);
        Assert::AreEqual (std::string ("SetDriveAudioVolumes 0.5 0.25 1"), target.calls[3]);
        Assert::AreEqual (std::string ("SetDriveAudioPan 0 -1"),           target.calls[4]);
        Assert::AreEqual (std::string ("SetDriveAudioPan 1 0.5"),          target.calls[5]);
        Assert::AreEqual (std::string ("PlayDriveTestSound 1 2"),          target.calls[6]);
    }


    TEST_METHOD (AMalformedAudioPayloadAsksForNothing)
    {
        Notebook  target;

        Dispatch (IDM_AUDIO_DRIVE_VOLUMES, "50,25",  target);   // one short
        Dispatch (IDM_AUDIO_DRIVE_PAN,     "left",   target);
        Dispatch (IDM_AUDIO_DRIVE_TEST,    "",       target);

        Assert::IsTrue (target.calls.empty());
    }


    TEST_METHOD (AnIdTheCpuThreadDoesNotHandleAsksForNothing)
    {
        Notebook  target;

        Dispatch (IDM_EDIT_COPY_SCREENSHOT, "", target);

        Assert::IsTrue (target.calls.empty(), L"a UI-thread command reaching here is ignored, not acted on");
    }


private:

    //  A target that writes down what it was asked, one line per call.
    class Notebook : public ICpuCommandTarget
    {
    public:

        std::vector<std::string>  calls;

        HRESULT  SwitchMachine (const std::wstring & name) override
        {
            calls.push_back ("SwitchMachine " + Narrow (name));
            return S_OK;
        }

        void     SoftReset() override       { calls.push_back ("SoftReset"); }

        void     HoldAppleKeysThroughReset (bool openApple, bool closedApple) override
        {
            if (openApple || closedApple)
            {
                calls.push_back (std::format ("HoldAppleKeys {}{}", openApple ? "open " : "", closedApple ? "closed" : ""));
            }
        }

        void     PowerCycle() override      { calls.push_back ("PowerCycle"); }
        void     StepInstruction() override { calls.push_back ("StepInstruction"); }
        void     RemountDisks() override    { calls.push_back ("RemountDisks"); }

        HRESULT  MountDisk (int drive, const std::string & path) override
        {
            calls.push_back (std::format ("MountDisk {} {}", drive, path));
            return S_OK;
        }

        void     EjectDisk (int drive) override { calls.push_back (std::format ("EjectDisk {}", drive)); }

        void     SetDriveUserWriteProtect (int drive, bool wp) override
        {
            calls.push_back (std::format ("SetDriveUserWriteProtect {} {}", drive, wp ? "on" : "off"));
        }

        HRESULT  ToggleImageWriteProtect (int drive) override
        {
            calls.push_back (std::format ("ToggleImageWriteProtect {}", drive));
            return S_OK;
        }

        void     RunSalvageFlow (int drive) override { calls.push_back (std::format ("RunSalvageFlow {}", drive)); }

        void     ResolvePendingChange (int slot, int drive, int action, const std::string & savePath) override
        {
            calls.push_back (std::format ("ResolvePendingChange {} {} {} {}", slot, drive, action, savePath));
        }

        void     SetDriveAudioEnabled (bool enabled) override
        {
            calls.push_back (std::format ("SetDriveAudioEnabled {}", enabled ? "on" : "off"));
        }

        HRESULT  SetDriveAudioMechanism (const std::wstring & mechanism) override
        {
            calls.push_back ("SetDriveAudioMechanism " + Narrow (mechanism));
            return S_OK;
        }


        //  The payloads here are ASCII machine ids and mechanism tokens, so
        //  narrowing loses nothing and says so.
        static std::string Narrow (const std::wstring & wide)
        {
            std::string  narrow;

            for (wchar_t ch : wide)
            {
                narrow += static_cast<char> (ch);
            }

            return narrow;
        }

        void     SetDriveAudioVolumes (float motor, float head, float door) override
        {
            calls.push_back (std::format ("SetDriveAudioVolumes {} {} {}", motor, head, door));
        }

        void     SetDriveAudioPan (int drive, float pan) override
        {
            calls.push_back (std::format ("SetDriveAudioPan {} {}", drive, pan));
        }

        void     PlayDriveTestSound (int drive, int kind) override
        {
            calls.push_back (std::format ("PlayDriveTestSound {} {}", drive, kind));
        }
    };


    static void Dispatch (WORD id, const char * payload, ICpuCommandTarget & target)
    {
        EmulatorCommand  cmd;

        cmd.id      = id;
        cmd.payload = payload;

        CpuCommandDispatcher::Dispatch (cmd, target);
    }
};
