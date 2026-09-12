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
    };
}
