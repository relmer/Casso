#include "Pch.h"

#include "Ui/Chrome/EmulatorCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PaddleSourceRowsTests
//
//  The rows the command bar's picker is built from, and the word it wears
//  while closed.
//
//  EXACTLY ONE ROW IS EVER CHECKED. One game port means one thing driving it,
//  and a picker showing two checks would be reporting a state the machine
//  cannot be in.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (PaddleSourceRowsTests)
    {
    public:

        static InputModeRules::PaddleSource MakeSource (const std::wstring & label,
                                                        const std::wstring & shortLabel,
                                                        bool                 isChecked,
                                                        bool                 isConnected = true)
        {
            InputModeRules::PaddleSource  source;

            source.label       = label;
            source.shortLabel  = shortLabel;
            source.isChecked   = isChecked;
            source.isConnected = isConnected;
            return source;
        }


        TEST_METHOD (Rows_AreOnePerSourceInOrder)
        {
            EmulatorCommands  commands;

            commands.SetPaddleSources ({ MakeSource (L"Gladiator", L"Gladiator", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false),
                                         MakeSource (L"Use mouse as paddle", L"Mouse", false) });

            Assert::AreEqual (static_cast<size_t> (3), commands.GetPaddleSourceItems().size());
        }


        TEST_METHOD (CheckedRow_IsTheOneDriving)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;

            commands.SetPaddleSources ({ MakeSource (L"Gladiator", L"Gladiator", false),
                                         MakeSource (L"Use keys as joystick", L"Keys", true) });

            items = commands.GetPaddleSourceItems();

            Assert::IsFalse (items[0].command->IsChecked(), L"the controller is not driving");
            Assert::IsTrue  (items[1].command->IsChecked(), L"the keys are");
        }


        TEST_METHOD (DisconnectedRow_IsShownButNotSelectable)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;

            commands.SetPaddleSources ({ MakeSource (L"(not connected)", L"Not connected", true, false),
                                         MakeSource (L"Use keys as joystick", L"Keys", false) });

            items = commands.GetPaddleSourceItems();

            Assert::AreEqual (static_cast<size_t> (2), items.size(),
                L"a chosen controller that is gone still has a row, or the picker would hide the user's choice");
            Assert::IsFalse (items[0].command->IsEnabled(),
                L"but picking it again would choose what is not there");
        }


        TEST_METHOD (Label_IsTheCheckedSourcesShortForm)
        {
            EmulatorCommands  commands;

            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false) });

            Assert::AreEqual (std::wstring (L"Xbox Controller"), commands.GetCheckedPaddleSourceLabel());

            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", false),
                                         MakeSource (L"Use keys as joystick", L"Keys", true) });

            Assert::AreEqual (std::wstring (L"Keys"), commands.GetCheckedPaddleSourceLabel(),
                L"the strip follows the source, not the last one set");
        }


        TEST_METHOD (Label_NamesThePickerWhenNothingIsDriving)
        {
            EmulatorCommands  commands;

            commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", false) });

            Assert::AreEqual (std::wstring (L"Controller"), commands.GetCheckedPaddleSourceLabel(),
                L"an empty strip entry would read as a rendering fault rather than as a state");
        }


        TEST_METHOD (Picking_RaisesTheRowThatWasPicked)
        {
            EmulatorCommands              commands;
            InputModeRules::PaddleSource  picked;
            bool                          wasPicked = false;

            commands.SetPaddleSourcePickedFn ([&] (const InputModeRules::PaddleSource & source)
            {
                picked    = source;
                wasPicked = true;
            });

            commands.SetPaddleSources ({ MakeSource (L"Gladiator", L"Gladiator", false),
                                         MakeSource (L"Use keys as joystick", L"Keys", true) });

            commands.GetPaddleSourceItems()[0].command->dispatch();

            Assert::IsTrue   (wasPicked);
            Assert::AreEqual (std::wstring (L"Gladiator"), picked.label);
        }


        TEST_METHOD (RowsRebuilt_TheOldRowsAreNotDispatchedInto)
        {
            EmulatorCommands  commands;
            int               pickCount = 0;

            commands.SetPaddleSourcePickedFn ([&] (const InputModeRules::PaddleSource &) { pickCount++; });

            commands.SetPaddleSources ({ MakeSource (L"Gladiator", L"Gladiator", true) });
            commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", true) });

            // A controller unplugging rebuilds the rows while the strip may
            // still hold the previous list, so a row's dispatch has to stay
            // safe rather than reaching into what it was built from.
            commands.GetPaddleSourceItems()[0].command->dispatch();

            Assert::AreEqual (1, pickCount);
        }


        TEST_METHOD (AStaleRowPicksTheSourceItWasBuiltFrom)
        {
            EmulatorCommands                commands;
            InputModeRules::PaddleSource    picked;
            std::vector<DxuiPopupMenuItem>  stale;

            commands.SetPaddleSourcePickedFn ([&] (const InputModeRules::PaddleSource & source) { picked = source; });

            // The list the user is looking at: a controller, then the two
            // fallbacks. "Use keys as joystick" is the middle row.
            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false),
                                         MakeSource (L"Use mouse as paddle", L"Mouse", false) });

            stale = commands.GetPaddleSourceItems();

            // The controller is unplugged while the menu is open, which is a
            // quickstart scenario in its own right. The list is now shorter,
            // and the row the user is about to click sits at a different
            // index than the one it was built at.
            commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", true),
                                         MakeSource (L"Use mouse as paddle", L"Mouse", false) });

            stale[1].command->dispatch();

            // Resolving by index would have picked the mouse here, and the
            // mouse takes the pointer -- so the user asks for the arrow keys
            // and the pointer is captured instead.
            Assert::AreEqual (std::wstring (L"Use keys as joystick"), picked.label,
                L"a row picks the source it was built from, whatever the list did since");
        }


        TEST_METHOD (RowsShrinking_DropsTheSpareRowsFromTheList)
        {
            EmulatorCommands  commands;

            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false),
                                         MakeSource (L"Use mouse as paddle", L"Mouse", false) });

            commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", true),
                                         MakeSource (L"Use mouse as paddle", L"Mouse", false) });

            // The row objects are kept so an open menu never holds a freed
            // one, which leaves spares behind; they must not reach the menu.
            Assert::AreEqual (static_cast<size_t> (2), commands.GetPaddleSourceItems().size());
        }

        static ControllerDeviceInfo MakeDevice (const char * unitId, const wchar_t * description)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x0079, 0x0006 };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = description;
            return info;
        }


        TEST_METHOD (StandIn_WearsTheCheckAndTheChosenRowSaysItIsGone)
        {
            InputModeRules::State                      state;
            std::vector<ControllerDeviceInfo>          devices;
            std::vector<InputModeRules::PaddleSource>  sources;
            ControllerDeviceInfo                       gone  = MakeDevice ("{AAAA}", L"VKBsim Gladiator");
            ControllerDeviceInfo                       here  = MakeDevice ("{BBBB}", L"Xbox Controller");

            devices.push_back (here);
            state.hasController = true;

            sources = InputModeRules::BuildPaddleSources (state, devices, gone.unit, here.unit, gone.description);

            Assert::AreEqual (static_cast<size_t> (4), sources.size(),
                L"the attached controller, the chosen one that is gone, and the two fallbacks");
            Assert::IsTrue  (sources[0].isStandIn, L"the attached controller is standing in");
            Assert::IsTrue  (sources[0].isChecked, L"and the check is on it, because it is what is driving");
            Assert::IsFalse (sources[1].isChecked, L"the chosen controller is not driving anything");
            Assert::IsFalse (sources[1].isConnected);
            Assert::AreEqual (std::wstring (L"VKBsim Gladiator (not connected)"), sources[1].label,
                L"and its row names it, rather than only saying something is missing");
        }


        TEST_METHOD (ChosenRow_NamesTheControllerOnlyWhenItHasBeenSeen)
        {
            InputModeRules::State                      state;
            std::vector<InputModeRules::PaddleSource>  sources;
            ControllerDeviceInfo                       gone = MakeDevice ("{AAAA}", L"VKBsim Gladiator");

            state.hasController = true;

            // A selection restored at launch is a token and nothing more
            // until the controller turns up, so there is no name to print.
            sources = InputModeRules::BuildPaddleSources (state, {}, gone.unit);

            Assert::AreEqual (std::wstring (L"Controller (not connected)"), sources[0].label);
            Assert::IsTrue   (sources[0].isChecked,
                L"with nothing standing in, the chosen controller is still what the picker points at");
        }


        TEST_METHOD (Tip_SaysWhatIsStandingInAndForWhat)
        {
            InputModeRules::State                      state;
            std::vector<ControllerDeviceInfo>          devices;
            ControllerDeviceInfo                       gone = MakeDevice ("{AAAA}", L"VKBsim Gladiator");
            ControllerDeviceInfo                       here = MakeDevice ("{BBBB}", L"Xbox Controller");

            devices.push_back (here);
            state.hasController = true;

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator is not connected. Xbox Controller is driving the paddles."),
                InputModeRules::BuildPaddleTip (
                    InputModeRules::BuildPaddleSources (state, devices, gone.unit, here.unit, gone.description)),
                L"the face shows only the stand-in, so the tooltip is the only place the chosen one appears");

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator is not connected."),
                InputModeRules::BuildPaddleTip (
                    InputModeRules::BuildPaddleSources (state, {}, gone.unit, std::nullopt, gone.description)),
                L"with nothing standing in, it says only that");
        }


        TEST_METHOD (Tip_NamesThePickersPurposeWhileEverythingIsWell)
        {
            InputModeRules::State             state;
            std::vector<ControllerDeviceInfo> devices;
            ControllerDeviceInfo              here = MakeDevice ("{BBBB}", L"Xbox Controller");

            devices.push_back (here);
            state.hasController        = true;
            state.isControllerAttached = true;

            Assert::AreEqual (std::wstring (L"Joystick and paddle source"),
                InputModeRules::BuildPaddleTip (
                    InputModeRules::BuildPaddleSources (state, devices, here.unit)),
                L"the face already carries the answer, so the tooltip has nothing to add");
        }


        TEST_METHOD (Tip_ReachesThePickerWhenTheRowsAreSet)
        {
            EmulatorCommands                           commands;
            InputModeRules::State                      state;
            std::vector<ControllerDeviceInfo>          devices;
            ControllerDeviceInfo                       gone = MakeDevice ("{AAAA}", L"VKBsim Gladiator");
            ControllerDeviceInfo                       here = MakeDevice ("{BBBB}", L"Xbox Controller");

            devices.push_back (here);
            state.hasController = true;

            commands.SetPaddleSources (
                InputModeRules::BuildPaddleSources (state, devices, gone.unit, here.unit, gone.description));

            Assert::IsTrue (commands.Find (EmulatorCommands::kIdPaddle) != nullptr);
            Assert::AreEqual (std::wstring (L"VKBsim Gladiator is not connected. Xbox Controller is driving the paddles."),
                commands.Find (EmulatorCommands::kIdPaddle)->tip);
        }


        TEST_METHOD (StandInLabel_IsTheStandInsOwnName)
        {
            InputModeRules::State                      state;
            std::vector<ControllerDeviceInfo>          devices;
            EmulatorCommands                           commands;
            ControllerDeviceInfo                       gone = MakeDevice ("{AAAA}", L"VKBsim Gladiator");
            ControllerDeviceInfo                       here = MakeDevice ("{BBBB}", L"Xbox Controller");

            devices.push_back (here);
            state.hasController = true;

            commands.SetPaddleSources (
                InputModeRules::BuildPaddleSources (state, devices, gone.unit, here.unit, gone.description));

            // The glyph follows the device, and a stand-in is a real device of
            // some kind, so the strip says what is actually driving rather
            // than carrying a third state for the chosen one's absence.
            Assert::AreEqual (std::wstring (L"Xbox Controller"), commands.GetCheckedPaddleSourceLabel());
        }
    };
}
