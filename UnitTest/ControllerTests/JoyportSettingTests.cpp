#include "Pch.h"

#include "Controllers/JoyportSetting.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportSettingTests
//
//  The Joyport setting is global, and one rule decides what the running
//  machine reads from it: the Joyport is in effect only on a machine with
//  annunciators, so the //c reads it as off and leaves the setting alone.
//
//  THE FIRST LAUNCH ADOPTS THE LAUNCHED MACHINE'S OLD VALUE, ONCE. Before the
//  setting went global it was saved with each machine; an empty global token
//  means it has never been set, and the launch takes the value the launched
//  machine saved and reports it adopted so the caller saves it. A set token
//  is never adopted over, whatever the machine's block holds.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (JoyportSettingTests)
    {
    public:

        static JsonValue ParseOrFail (const char * text)
        {
            JsonValue       v;
            JsonParseError  err;
            HRESULT         hr = JsonParser::Parse (text, v, err);



            Assert::IsTrue (SUCCEEDED (hr), L"fixture JSON did not parse");
            return v;
        }


        TEST_METHOD (IsInEffect_OnlyForTheJoyportOnAMachineWithAnnunciators)
        {
            Assert::IsTrue  (JoyportSetting::IsInEffect (GamePortAdapter::SiriusJoyport, true));
            Assert::IsFalse (JoyportSetting::IsInEffect (GamePortAdapter::SiriusJoyport, false), L"the //c reads it as off");
            Assert::IsFalse (JoyportSetting::IsInEffect (GamePortAdapter::None,          true));
            Assert::IsFalse (JoyportSetting::IsInEffect (GamePortAdapter::None,          false));
        }


        TEST_METHOD (IsMousePaddleOffered_OnlyWhileTheJoyportIsNotInEffect)
        {
            Assert::IsTrue  (JoyportSetting::IsMousePaddleOffered (false));
            Assert::IsFalse (JoyportSetting::IsMousePaddleOffered (true), L"an Atari stick has no paddle for the mouse to stand in for");
        }


        TEST_METHOD (ResolveAtLaunch_ASetGlobalTokenWinsAndIsNotAdopted)
        {
            JsonValue             joyport = ParseOrFail (R"({"gamePortAdapter":"siriusJoyport"})");
            JsonValue             none    = ParseOrFail (R"({"gamePortAdapter":"none"})");
            JoyportLaunchSetting  resolved;



            resolved = JoyportSetting::ResolveAtLaunch ("none", &joyport, true);
            Assert::IsTrue  (resolved.setting == GamePortAdapter::None, L"the machine's saved Joyport does not override a set None");
            Assert::IsFalse (resolved.isAdopted);

            resolved = JoyportSetting::ResolveAtLaunch ("siriusJoyport", &none, true);
            Assert::IsTrue  (resolved.setting == GamePortAdapter::SiriusJoyport);
            Assert::IsFalse (resolved.isAdopted);

            resolved = JoyportSetting::ResolveAtLaunch ("siriusJoyport", nullptr, false);
            Assert::IsTrue  (resolved.setting == GamePortAdapter::SiriusJoyport, L"launching the //c leaves the setting as it is");
            Assert::IsFalse (resolved.isAdopted);
        }


        TEST_METHOD (ResolveAtLaunch_AnEmptyTokenAdoptsTheLaunchedMachinesValue)
        {
            JsonValue             joyport = ParseOrFail (R"({"gamePortAdapter":"siriusJoyport"})");
            JsonValue             empty   = ParseOrFail (R"({})");
            JoyportLaunchSetting  resolved;



            resolved = JoyportSetting::ResolveAtLaunch ("", &joyport, true);
            Assert::IsTrue (resolved.setting == GamePortAdapter::SiriusJoyport, L"the //e saved a Joyport");
            Assert::IsTrue (resolved.isAdopted, L"so the caller saves it and later launches do not adopt again");

            resolved = JoyportSetting::ResolveAtLaunch ("", &empty, true);
            Assert::IsTrue (resolved.setting == GamePortAdapter::None, L"no machine key");
            Assert::IsTrue (resolved.isAdopted);

            resolved = JoyportSetting::ResolveAtLaunch ("", nullptr, true);
            Assert::IsTrue (resolved.setting == GamePortAdapter::None, L"no machine block");
            Assert::IsTrue (resolved.isAdopted);

            resolved = JoyportSetting::ResolveAtLaunch ("", &joyport, false);
            Assert::IsTrue (resolved.setting == GamePortAdapter::None, L"the //c cannot have saved one, whatever its block says");
            Assert::IsTrue (resolved.isAdopted);
        }


        TEST_METHOD (ResolveAtLaunch_AnUnknownTokenIsNoneAndNotAdopted)
        {
            JsonValue             joyport  = ParseOrFail (R"({"gamePortAdapter":"siriusJoyport"})");
            JoyportLaunchSetting  resolved = JoyportSetting::ResolveAtLaunch ("atariAdapter", &joyport, true);



            Assert::IsTrue  (resolved.setting == GamePortAdapter::None);
            Assert::IsFalse (resolved.isAdopted, L"it was set, by some build, so it is not replaced by the machine's value");
        }
    };
}
