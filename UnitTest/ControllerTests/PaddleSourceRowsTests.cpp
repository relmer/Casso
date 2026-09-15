#include "Pch.h"

#include "resource.h"
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
                                                        bool                 isChecked)
        {
            InputModeRules::PaddleSource  source;

            source.label      = label;
            source.shortLabel = shortLabel;
            source.isChecked  = isChecked;
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


        TEST_METHOD (APickThatRebuildsTheRows_StillReadsTheSourceItPicked)
        {
            EmulatorCommands                commands;
            InputModeRules::PaddleSource    stick   = MakeSource (L"VKBsim Gladiator", L"VKBsim Gladiator", false);
            ControllerUnitKey               unit;
            std::wstring                    labelAfterRebuilds;
            bool                            unitIntact = false;

            unit.model      = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            unit.unitId     = "{01661270-ADF7-11F1-8005-444553540000}";
            unit.source     = ControllerUnitSource::InstanceGuid;
            stick.controller = unit;

            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true),
                                         stick,
                                         MakeSource (L"Use keys as joystick", L"Keys", false) });

            // What the shell does on a pick: turning the arrows and the paddle
            // off re-syncs the picker, which rebuilds the rows -- more than once
            // -- all before the dispatch returns. The source handed in belongs
            // to the row being rebuilt away.
            commands.SetPaddleSourcePickedFn ([&] (const InputModeRules::PaddleSource & source)
            {
                commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", true) });
                commands.SetPaddleSources ({ MakeSource (L"Use mouse as paddle", L"Mouse", true) });

                labelAfterRebuilds = source.label;
                unitIntact         = source.controller.has_value() && source.controller.value() == unit;
            });

            commands.GetPaddleSourceItems()[1].command->dispatch();

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"), labelAfterRebuilds,
                L"the picked source outlives the rows rebuilt during its own dispatch");
            Assert::IsTrue (unitIntact,
                L"and so does the controller key the selection is copied from");
        }


        TEST_METHOD (RebuiltRows_AreReleasedOnceNothingHoldsThem)
        {
            EmulatorCommands                   commands;
            std::weak_ptr<const DxuiCommand>   oldRow;

            commands.SetPaddleSources ({ MakeSource (L"Gladiator", L"Gladiator", true) });

            {
                std::vector<DxuiPopupMenuItem>  held = commands.GetPaddleSourceItems();

                oldRow = held[0].command;

                commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", true) });

                Assert::IsFalse (oldRow.expired(), L"the held items keep the replaced row alive");
            }

            Assert::IsTrue   (oldRow.expired(), L"with the items dropped, nothing keeps the replaced row");
            Assert::AreEqual (static_cast<size_t> (1), commands.GetPaddleSourceItems().size());
        }


        TEST_METHOD (RebuildsWhileItemsAreHeld_KeepEveryRowTheItemsHold)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  open;

            commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", true) });
            open = commands.GetPaddleSourceItems();

            // An Xbox controller plugged in while the picker is open: the rows
            // are rebuilt as it arrives, again as it is selected, and again as
            // the selection settles.
            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", false),
                                         MakeSource (L"Use keys as joystick", L"Keys", true) });
            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false) });
            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true) });

            Assert::AreEqual (std::wstring (L"Use keys as joystick"), open[0].command->label,
                L"the rows the open menu paints are still there after every rebuild");
            Assert::IsTrue   (open[0].command->IsChecked());
            Assert::AreEqual (static_cast<size_t> (1), commands.GetPaddleSourceItems().size());
        }


        TEST_METHOD (RowsShrinking_DropsTheSpareRowsFromTheList)
        {
            EmulatorCommands  commands;

            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false),
                                         MakeSource (L"Use mouse as paddle", L"Mouse", false) });

            commands.SetPaddleSources ({ MakeSource (L"Use keys as joystick", L"Keys", true),
                                         MakeSource (L"Use mouse as paddle", L"Mouse", false) });

            // An open menu may still hold the longer list's rows; none of
            // them may reach a list handed out afterwards.
            Assert::AreEqual (static_cast<size_t> (2), commands.GetPaddleSourceItems().size());
        }

        TEST_METHOD (Tip_IsThePickersPurpose)
        {
            EmulatorCommands  commands;

            commands.SetPaddleSources ({ MakeSource (L"Xbox Controller", L"Xbox Controller", true) });

            Assert::IsTrue   (commands.Find (EmulatorCommands::kIdPaddle) != nullptr);
            Assert::AreEqual (std::wstring (L"Joystick and paddle source"), commands.Find (EmulatorCommands::kIdPaddle)->tip,
                L"the face already carries the answer, so the tooltip has nothing to add");
        }


        TEST_METHOD (Picker_EndsWithControllerSettings)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;

            commands.SetPaddleSources ({ MakeSource (L"Gladiator", L"Gladiator", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false) });

            items = commands.GetPaddlePickerItems();

            Assert::AreEqual (static_cast<size_t> (4), items.size(), L"the two sources, a separator, then the settings entry");
            Assert::IsTrue   (items.back().command != nullptr);
            Assert::AreEqual ((int) IDM_VIEW_CONTROLLER_SETTINGS, items.back().command->id,
                L"the settings for a controller are one click from where it was chosen");
            Assert::AreEqual (static_cast<size_t> (2), commands.GetPaddleSourceItems().size(),
                L"and the source rows alone are unchanged");
        }
    };
}
