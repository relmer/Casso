#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportHostSettingStateTests
//
//  A snapshot holds whether the Joyport is attached and whether the rear
//  sockets are in use, since a guest read answers differently with each; a
//  replay from the snapshot must start with the settings it had.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (JoyportHostSettingStateTests)
{
public:

    TEST_METHOD (AttachedAndRearSocketsRoundTrip)
    {
        TestMachine      machine ("Apple2e", TestMachine::Slots::Empty);
        SiriusJoyport  * joyport = machine.GetJoyport();
        StateWriter      writer;
        StateWriter      reSaved;
        HRESULT          hr      = S_OK;



        Assert::IsNotNull (joyport);

        joyport->SetAttached         (true);
        joyport->SetPaddlesConnected (true);

        hr = joyport->SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        joyport->SetAttached         (false);
        joyport->SetPaddlesConnected (false);

        {
            StateReader  reader (writer.GetBytes());

            hr = joyport->LoadState (reader);
            AssertSucceeded (hr, L"LoadState");
        }

        Assert::IsTrue (joyport->IsAttached(), L"attached comes back from the snapshot");

        hr = joyport->SaveState (reSaved);
        AssertSucceeded (hr, L"SaveState again");

        Assert::IsTrue (reSaved.GetBytes() == writer.GetBytes(), L"and so do the rear sockets");
    }
};
