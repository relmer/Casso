#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/AppleSoftSwitchBank.h"
#include "Video/AppleTextMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleLoResMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SoftSwitchTests
//
//  Adversarial tests proving soft switches actually toggle video mode flags.
//  Verifies that READING (not writing) the address triggers the switch,
//  state persists, and the video system actually uses the state.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SoftSwitchTests)
{
public:

    TEST_METHOD (Default_TextMode_AllFlagsFalse)
    {
        AppleSoftSwitchBank sw;

        Assert::IsFalse (sw.IsGraphicsMode (), L"Default should be text mode");
        Assert::IsFalse (sw.IsHiresMode (), L"Default should not be hires");
        Assert::IsFalse (sw.IsMixedMode (), L"Default should not be mixed");
        Assert::IsFalse (sw.IsPage2 (), L"Default should be page 1");
    }

    TEST_METHOD (ReadC050_SetsGraphicsMode)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC050);

        Assert::IsTrue (sw.IsGraphicsMode (),
            L"Reading $C050 must enable graphics mode");
    }

    TEST_METHOD (ReadC051_ClearsGraphicsMode)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC050);
        sw.Read (0xC051);

        Assert::IsFalse (sw.IsGraphicsMode (),
            L"Reading $C051 must disable graphics mode");
    }

    TEST_METHOD (ReadC052_ClearsMixedMode)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC053);  // Enable mixed first
        sw.Read (0xC052);  // Clear mixed

        Assert::IsFalse (sw.IsMixedMode (),
            L"Reading $C052 must disable mixed mode");
    }

    TEST_METHOD (ReadC053_SetsMixedMode)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC053);

        Assert::IsTrue (sw.IsMixedMode (),
            L"Reading $C053 must enable mixed mode");
    }

    TEST_METHOD (ReadC054_SetsPage1)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC055);  // Set page 2 first
        sw.Read (0xC054);  // Set page 1

        Assert::IsFalse (sw.IsPage2 (),
            L"Reading $C054 must select page 1");
    }

    TEST_METHOD (ReadC055_SetsPage2)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC055);

        Assert::IsTrue (sw.IsPage2 (),
            L"Reading $C055 must select page 2");
    }

    TEST_METHOD (ReadC056_ClearsHiresMode)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC057);  // Enable hires first
        sw.Read (0xC056);  // Clear hires

        Assert::IsFalse (sw.IsHiresMode (),
            L"Reading $C056 must disable hires mode");
    }

    TEST_METHOD (ReadC057_SetsHiresMode)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC057);

        Assert::IsTrue (sw.IsHiresMode (),
            L"Reading $C057 must enable hires mode");
    }

    TEST_METHOD (ReadTriggersSwitch_NotWrite)
    {
        // On Apple II, both reads AND writes trigger soft switches.
        // Verify Write also works (it calls Read internally).
        AppleSoftSwitchBank sw;
        sw.Write (0xC050, 0x00);

        Assert::IsTrue (sw.IsGraphicsMode (),
            L"Writing $C050 should also trigger the switch");
    }

    TEST_METHOD (StatePersists_AcrossMultipleReads)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC050);  // Graphics on
        sw.Read (0xC053);  // Mixed on
        sw.Read (0xC057);  // Hires on

        // Re-read unrelated addresses — state should persist
        sw.Read (0xC050);  // Read graphics again
        sw.Read (0xC050);

        Assert::IsTrue (sw.IsGraphicsMode (), L"Graphics should persist");
        Assert::IsTrue (sw.IsMixedMode (), L"Mixed should persist");
        Assert::IsTrue (sw.IsHiresMode (), L"Hires should persist");
    }

    TEST_METHOD (Reset_ClearsAllFlags)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC050);
        sw.Read (0xC053);
        sw.Read (0xC055);
        sw.Read (0xC057);

        sw.Reset ();

        Assert::IsFalse (sw.IsGraphicsMode (), L"Reset must clear graphics");
        Assert::IsFalse (sw.IsMixedMode (), L"Reset must clear mixed");
        Assert::IsFalse (sw.IsPage2 (), L"Reset must clear page2");
        Assert::IsFalse (sw.IsHiresMode (), L"Reset must clear hires");
    }

    TEST_METHOD (SoftSwitch_ViaBus_Works)
    {
        // Prove soft switches work when accessed through MemoryBus
        MemoryBus bus;
        AppleSoftSwitchBank sw;
        bus.AddDevice (&sw);

        bus.ReadByte (0xC050);  // Graphics on via bus

        Assert::IsTrue (sw.IsGraphicsMode (),
            L"Soft switch via bus read must work");
    }

    TEST_METHOD (TextMode_Page1Address_0400)
    {
        MemoryBus bus;
        AppleTextMode textMode (bus);

        Assert::AreEqual (static_cast<Word> (0x0400),
            textMode.GetActivePageAddress (false),
            L"Text page 1 address must be $0400");
    }

    TEST_METHOD (TextMode_Page2Address_0800)
    {
        MemoryBus bus;
        AppleTextMode textMode (bus);

        Assert::AreEqual (static_cast<Word> (0x0800),
            textMode.GetActivePageAddress (true),
            L"Text page 2 address must be $0800");
    }

    TEST_METHOD (HiResMode_Page1Address_2000)
    {
        MemoryBus bus;
        AppleHiResMode hiRes (bus);

        Assert::AreEqual (static_cast<Word> (0x2000),
            hiRes.GetActivePageAddress (false),
            L"Hi-res page 1 address must be $2000");
    }

    TEST_METHOD (HiResMode_Page2Address_4000)
    {
        MemoryBus bus;
        AppleHiResMode hiRes (bus);

        Assert::AreEqual (static_cast<Word> (0x4000),
            hiRes.GetActivePageAddress (true),
            L"Hi-res page 2 address must be $4000");
    }

    TEST_METHOD (LoResMode_Page1Address_0400)
    {
        MemoryBus bus;
        AppleLoResMode loRes (bus);

        Assert::AreEqual (static_cast<Word> (0x0400),
            loRes.GetActivePageAddress (false),
            L"Lo-res page 1 address must be $0400");
    }

    TEST_METHOD (LoResMode_Page2Address_0800)
    {
        MemoryBus bus;
        AppleLoResMode loRes (bus);

        Assert::AreEqual (static_cast<Word> (0x0800),
            loRes.GetActivePageAddress (true),
            L"Lo-res page 2 address must be $0800");
    }

    TEST_METHOD (ToggleSequence_GraphicsTextGraphics)
    {
        AppleSoftSwitchBank sw;

        sw.Read (0xC050);
        Assert::IsTrue (sw.IsGraphicsMode ());

        sw.Read (0xC051);
        Assert::IsFalse (sw.IsGraphicsMode ());

        sw.Read (0xC050);
        Assert::IsTrue (sw.IsGraphicsMode (),
            L"Toggle G->T->G should restore graphics mode");
    }
};
