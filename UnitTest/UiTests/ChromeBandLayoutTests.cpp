#include "Pch.h"

#include "Shell/Layout/ChromeBandLayout.h"
#include "Shell/MachineChromeFacts.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBandLayoutTests
//
//  The chrome bands against synthetic machine state.
//
//  Which bands exist is a fact about the machine and the way it is shown: a
//  ][+ with no controller has no drives to make a band for, the //c alone has
//  case switches, and under the desk scene the drives leave the chrome for
//  the scene. Each of those is a height that must be zero, asserted here from
//  inputs rather than from a window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ChromeBandLayoutTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //
    //  The layout's copy of the machine. A switch rewrites the config and the
    //  device references under the machine's lifetime lock, so the copy is
    //  taken only under it, and a capture made without the lock changes
    //  nothing.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (ChromeFactsTakenDuringARebuildKeepTheMachineTheyHad)
    {
        TestMachine         machine ("Apple2e");
        TestMachine         next    ("Apple2c");
        MachineChromeFacts  facts;
        bool                connected = false;
        std::atomic<bool>   isLocked  { false };
        std::atomic<bool>   isRelease { false };
        bool                isTaken   = false;



        Assert::IsTrue (MachineChromeFacts::TryCapture (machine, connected, facts), L"the first capture");
        Assert::IsTrue  (facts.hasDiskController);
        Assert::IsFalse (facts.hasCaseSwitches);
        Assert::AreEqual (2, facts.GetDeskSceneDriveCount());

        std::thread  rebuild ([&]
        {
            std::unique_lock<std::shared_mutex>  lock (machine.GetLifetimeLock());

            machine.GetConfig()               = next.GetConfig();
            machine.GetRefs().diskController  = nullptr;
            isLocked                          = true;

            while (!isRelease)
            {
                std::this_thread::yield();
            }
        });

        while (!isLocked)
        {
            std::this_thread::yield();
        }

        isTaken = MachineChromeFacts::TryCapture (machine, connected, facts);

        isRelease = true;
        rebuild.join();

        Assert::IsFalse (isTaken, L"a capture during the rebuild does not get the lock");
        Assert::IsFalse (facts.hasCaseSwitches, L"and the facts are the //e's");
        Assert::IsTrue  (facts.hasDiskController);
        Assert::AreEqual (2, facts.GetDeskSceneDriveCount());

        Assert::IsTrue  (MachineChromeFacts::TryCapture (machine, connected, facts), L"the capture after it");
        Assert::IsTrue  (facts.hasCaseSwitches, L"has the //c");
        Assert::IsFalse (facts.hasDiskController);
    }



    //  A Disk II card in slot 6 with the given connectors.
    static MachineConfig MakeCardConfig (std::initializer_list<const char *> ports)
    {
        MachineConfig  config;
        SlotConfig     slot;



        slot.slot   = 6;
        slot.device = "disk-ii";

        for (const char * port : ports)
        {
            slot.ports.push_back ({ "", port });
        }

        config.slots.push_back (slot);

        return config;
    }



    TEST_METHOD (AnExternalDriveOnTheIIcFollowsTheConnectedFlag)
    {
        Disk2Controller  controller (6);
        MachineConfig    config;
        MachineRefs      refs;



        config.systemRom.romBankSize = 0x4000;
        refs.diskController          = &controller;

        Assert::IsFalse (MachineChromeFacts::Capture (config, refs, false).ShouldShowExternalDrive());
        Assert::AreEqual (1, MachineChromeFacts::Capture (config, refs, false).GetDeskSceneDriveCount(), L"the internal drive");
        Assert::IsTrue  (MachineChromeFacts::Capture (config, refs, true).ShouldShowExternalDrive());
        Assert::AreEqual (2, MachineChromeFacts::Capture (config, refs, true).GetDeskSceneDriveCount(), L"and the external one");
    }



    TEST_METHOD (ADetachedSecondPortHidesTheExternalDrive)
    {
        Disk2Controller     controller (6);
        MachineRefs         refs;
        MachineChromeFacts  facts;



        refs.diskController = &controller;
        facts               = MachineChromeFacts::Capture (MakeCardConfig ({ "disk-ii-drive", "" }), refs, true);

        Assert::AreEqual (1, facts.GetDeskSceneDriveCount());
        Assert::IsFalse (facts.ShouldShowExternalDrive(), L"the connected flag is the //c's");
    }



    TEST_METHOD (ACardWithUndeclaredPortsShowsBothDrives)
    {
        Disk2Controller     controller (6);
        MachineRefs         refs;
        MachineChromeFacts  facts;



        refs.diskController = &controller;
        facts               = MachineChromeFacts::Capture (MakeCardConfig ({}), refs, false);

        Assert::AreEqual (2, facts.GetDeskSceneDriveCount());
        Assert::IsTrue (facts.ShouldShowExternalDrive());
    }



    TEST_METHOD (NoControllerMeansNoDrives)
    {
        MachineRefs  refs;



        Assert::AreEqual (0, MachineChromeFacts::Capture (MakeCardConfig ({}), refs, true).GetDeskSceneDriveCount());
    }



    TEST_METHOD (AMachineWithoutAControllerHasNoDriveBand)
    {
        ChromeBandInputs  inputs = Typical();

        inputs.hasDiskController = false;

        Assert::AreEqual (0, ChromeBandLayout::DriveBandDp (inputs));
        Assert::AreEqual (0, ChromeBandLayout::Compute (inputs, At (96)).drive,
                          L"the viewport takes the room the widgets would have had");
    }


    TEST_METHOD (UnderTheDeskSceneTheDrivesLeaveTheChrome)
    {
        ChromeBandInputs  inputs = Typical();

        inputs.crtMonitorActive = true;

        Assert::AreEqual (0, ChromeBandLayout::DriveBandDp (inputs),
                          L"the drives are scene objects there, and a band would double them");
    }


    TEST_METHOD (TheDriveBandZoomsWithTheScene)
    {
        ChromeBandInputs  inputs = Typical();

        inputs.driveBarThicknessDp = 180;

        inputs.chromeSceneScale = 1.0f;
        Assert::AreEqual (180, ChromeBandLayout::DriveBandDp (inputs));

        inputs.chromeSceneScale = 0.5f;
        Assert::AreEqual (90, ChromeBandLayout::DriveBandDp (inputs), L"half the scene, half the band");

        inputs.chromeSceneScale = 0.75f;
        Assert::AreEqual (135, ChromeBandLayout::DriveBandDp (inputs), L"rounded, not truncated");
    }


    TEST_METHOD (OnlyAMachineWithCaseSwitchesHasASwitchBand)
    {
        ChromeBandInputs  inputs = Typical();

        inputs.hasCaseSwitches = false;
        Assert::AreEqual (0, ChromeBandLayout::SwitchBandDp (inputs), L"a //e has nothing to put there");

        inputs.hasCaseSwitches = true;
        Assert::AreEqual (ChromeBandLayout::kSwitchBandDp, ChromeBandLayout::SwitchBandDp (inputs),
                          L"the //c strip");
    }


    TEST_METHOD (EveryBandScalesWithTheDpiExceptTheTwoAlreadyInPixels)
    {
        ChromeBandInputs     inputs = Typical();
        ChromeBandHeightsPx  at96   = ChromeBandLayout::Compute (inputs, At (96));
        ChromeBandHeightsPx  at192  = ChromeBandLayout::Compute (inputs, At (192));

        Assert::AreEqual (ChromeBandLayout::kTitleBarBandDp, at96.title);
        Assert::AreEqual (2 * at96.title,    at192.title,    L"title");
        Assert::AreEqual (2 * at96.nav,      at192.nav,      L"nav");
        Assert::AreEqual (2 * at96.toolbar,  at192.toolbar,  L"toolbar");
        Assert::AreEqual (2 * at96.drive,    at192.drive,    L"drive");
        Assert::AreEqual (2 * at96.switches, at192.switches, L"switches");

        //  The notice bands were measured against the client width by the
        //  caller, in pixels already; scaling them again would double them.
        Assert::AreEqual (inputs.changeBandPx,  at192.change,  L"change band is passed through");
        Assert::AreEqual (inputs.captureBandPx, at192.capture, L"and so is the capture band");
    }


private:

    //  A //c under a flat theme with a change notice up: every band present.
    static ChromeBandInputs Typical()
    {
        ChromeBandInputs  inputs;

        inputs.hasDiskController   = true;
        inputs.crtMonitorActive    = false;
        inputs.hasCaseSwitches     = true;
        inputs.driveBarThicknessDp = 180;
        inputs.chromeSceneScale    = 1.0f;
        inputs.toolbarBandDp       = 44;
        inputs.changeBandPx        = 37;
        inputs.captureBandPx       = 0;

        return inputs;
    }


    static DxuiDpiScaler At (UINT dpi)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (dpi);

        return scaler;
    }
};
