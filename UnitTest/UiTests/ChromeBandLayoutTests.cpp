#include "Pch.h"

#include "Shell/Layout/ChromeBandLayout.h"

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
