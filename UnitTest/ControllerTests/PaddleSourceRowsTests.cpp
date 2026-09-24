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


        TEST_METHOD (Picker_SeparatesTheMultiplayerRowFromTheSources)
        {
            EmulatorCommands                commands;
            InputModeRules::PaddleSource    twoPlayer = MakeSource (L"Multiplayer...", L"Multiplayer", false);
            std::vector<DxuiPopupMenuItem>  items;

            twoPlayer.isMultiplayer = true;

            commands.SetPaddleSources ({ MakeSource (L"Gladiator", L"Gladiator", true),
                                         MakeSource (L"Use keys as joystick", L"Keys", false),
                                         twoPlayer });

            items = commands.GetPaddlePickerItems();

            // The rows above it are the one thing driving the game port; this
            // is the mode where two things do, so it does not read as a third
            // source.
            Assert::AreEqual (static_cast<size_t> (6), items.size(),
                L"two sources, a separator, the two-player row, a separator, then the settings entry");
            Assert::IsTrue   (items[2].command == nullptr, L"a separator sits ahead of it");
            Assert::AreEqual (std::wstring (L"Multiplayer..."), items[3].command->label);
            Assert::AreEqual (static_cast<size_t> (3), commands.GetPaddleSourceItems().size(),
                L"and the source rows alone carry no separators");
        }


        static ControllerUnitKey MakeUnit (const char * productId)
        {
            ControllerUnitKey  unit;

            unit.model.kind = ControllerKind::XInput;
            unit.unitId     = productId;
            unit.source     = ControllerUnitSource::XInputProduct;

            return unit;
        }


        static EmulatorCommands::ProfileSection MakeSection (const char *                      productId,
                                                             std::vector<std::string>          names,
                                                             const char *                      active,
                                                             const wchar_t *                   header = L"")
        {
            EmulatorCommands::ProfileSection  section;

            section.unit   = MakeUnit (productId);
            section.names  = std::move (names);
            section.active = active;
            section.header = header;

            return section;
        }


        //  One controller in play: its profiles with Default first, then New...
        //  below a separator, and no header, since there is nothing to tell
        //  apart.
        TEST_METHOD (ProfileRows_AreOnePerProfileDefaultFirst)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;

            commands.SetProfileSections ({ MakeSection ("045e:02e0", { "Paddles", "Default", "Swapped" }, "") });

            items = commands.GetProfileItems();

            Assert::AreEqual (static_cast<size_t> (5), items.size(), L"three profiles, a separator, New...");
            Assert::AreEqual (std::wstring (L"Default"), items[0].command->label, L"Default leads wherever the store keeps it");
            Assert::AreEqual (std::wstring (L"Paddles"), items[1].command->label);
            Assert::AreEqual (std::wstring (L"Swapped"), items[2].command->label);
            Assert::IsTrue   (items[3].kind == DxuiPopupMenuItem::Kind::Separator);
            Assert::AreEqual (std::wstring (L"New..."), items[4].command->label);
        }


        TEST_METHOD (ProfileRows_CheckedIsTheActiveIgnoringCase)
        {
            EmulatorCommands  commands;

            commands.SetProfileSections ({ MakeSection ("045e:02e0", { "Default", "Paddles" }, "PADDLES") });

            Assert::IsFalse (commands.GetProfileItems()[0].command->IsChecked());
            Assert::IsTrue  (commands.GetProfileItems()[1].command->IsChecked());

            commands.SetProfileSections ({ MakeSection ("045e:02e0", { "Default", "Paddles" }, "") });

            Assert::IsTrue  (commands.GetProfileItems()[0].command->IsChecked(), L"empty means Default");
        }


        //  A deleted active profile plays the Default, so the Default is what
        //  the list checks.
        TEST_METHOD (ProfileRows_AnActiveProfileTheModelLacksChecksDefault)
        {
            EmulatorCommands  commands;

            commands.SetProfileSections ({ MakeSection ("045e:02e0", { "Default" }, "Racing") });

            Assert::IsTrue (commands.GetProfileItems()[0].command->IsChecked());
        }


        //  In multiplayer each controller in play has its own section, under a
        //  header saying whose it is, and its own check -- which is what lets
        //  two players on two pads of one model play different profiles.
        TEST_METHOD (ProfileRows_EachControllerHasItsOwnSectionAndCheck)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;
            ControllerUnitKey               pickedUnit;
            std::string                     pickedName = "unset";

            commands.SetProfilePickedFn ([&] (const ControllerUnitKey & unit, const std::string & name)
            {
                pickedUnit = unit;
                pickedName = name;
            });

            commands.SetProfileSections ({ MakeSection ("045e:02e0",   { "Default", "Test" }, "",     L"Player 1"),
                                           MakeSection ("045e:02e0:2", { "Default", "Test" }, "Test", L"Player 2") });

            items = commands.GetProfileItems();

            // Player 1: header, Default, Test; separator; Player 2: header,
            // Default, Test; separator; New...
            Assert::AreEqual (static_cast<size_t> (9), items.size());
            Assert::IsTrue   (items[0].kind == DxuiPopupMenuItem::Kind::Header);
            Assert::AreEqual (std::wstring (L"Player 1"), items[0].command->label);
            Assert::IsTrue   (items[1].command->IsChecked(),  L"player one plays Default");
            Assert::IsFalse  (items[2].command->IsChecked());
            Assert::IsTrue   (items[3].kind == DxuiPopupMenuItem::Kind::Separator);
            Assert::IsTrue   (items[4].kind == DxuiPopupMenuItem::Kind::Header);
            Assert::IsFalse  (items[5].command->IsChecked());
            Assert::IsTrue   (items[6].command->IsChecked(),  L"player two plays Test on a pad of the same model");

            items[6].command->dispatch();
            Assert::IsTrue   (pickedUnit == MakeUnit ("045e:02e0:2"), L"a row picks for the controller it was listed under");
            Assert::AreEqual (std::string ("Test"), pickedName);
        }


        TEST_METHOD (ProfileRows_AStaleRowPicksTheProfileItWasBuiltFrom)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  stale;
            std::string                     picked  = "unset";

            commands.SetProfilePickedFn ([&] (const ControllerUnitKey &, const std::string & name) { picked = name; });

            commands.SetProfileSections ({ MakeSection ("045e:02e0", { "Default", "Paddles", "Swapped" }, "") });
            stale = commands.GetProfileItems();

            // Paddles deleted while the menu is open: Swapped now sits where
            // Paddles was.
            commands.SetProfileSections ({ MakeSection ("045e:02e0", { "Default", "Swapped" }, "") });

            stale[1].command->dispatch();
            Assert::AreEqual (std::string ("Paddles"), picked);

            stale[0].command->dispatch();
            Assert::AreEqual (std::string(), picked, L"Default is picked as the empty name");
        }


        //  With no controller in play there is nothing to switch, so the
        //  paddle picker carries no Profiles submenu; with one, the submenu
        //  sits above Controller settings...
        TEST_METHOD (ProfilesSubmenu_OnlyWhileAControllerIsInPlay)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;
            bool                            hasSubmenu = false;

            commands.SetProfileSections ({});

            for (const DxuiPopupMenuItem & item : commands.GetPaddlePickerItems())
            {
                hasSubmenu = hasSubmenu || item.kind == DxuiPopupMenuItem::Kind::Submenu;
            }

            Assert::IsFalse (hasSubmenu);
            Assert::AreEqual (static_cast<size_t> (0), commands.GetProfileItems().size());

            commands.SetProfileSections ({ MakeSection ("045e:02e0", { "Default" }, "") });
            items = commands.GetPaddlePickerItems();

            Assert::IsTrue   (items.size() >= 2);
            Assert::IsTrue   (items[items.size() - 2].kind == DxuiPopupMenuItem::Kind::Submenu);
            Assert::AreEqual (std::wstring (L"Profiles"), items[items.size() - 2].command->label);
            Assert::AreEqual (static_cast<int> (IDM_VIEW_CONTROLLER_SETTINGS), items.back().command->id);
        }
    };
}
