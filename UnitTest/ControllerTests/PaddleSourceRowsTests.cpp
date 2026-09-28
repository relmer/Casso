#include "Pch.h"

#include "resource.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/JoyportLabels.h"
#include "Ui/Chrome/EmulatorCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PaddleSourceRowsTests
//
//  The command bar's picker: a row for each player, what each player's
//  submenu lists and checks, the profile section at its foot, and the word
//  the closed picker wears. The row model is pure data from the players'
//  entries and slots; the chrome turns it into commands.
//
//  EXACTLY ONE ENTRY IN EACH SUBMENU IS EVER CHECKED. A player has one entry,
//  and a submenu showing two checks would be reporting a choice the player
//  cannot have made.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (PaddleSourceRowsTests)
    {
    public:

        static ControllerDeviceInfo MakePad (const char * productId, const wchar_t * description)
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.unit.unitId     = productId;
            info.unit.source     = ControllerUnitSource::XInputProduct;
            info.description     = description;
            info.formFactor      = ControllerFormFactor::Gamepad;
            return info;
        }


        static ControllerDeviceInfo MakeStick (const char * guid, const wchar_t * description)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            info.unit.unitId = guid;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = description;
            info.formFactor  = ControllerFormFactor::Joystick;
            return info;
        }


        static PlayerEntry MakeEntry (PlayerEntryKind kind, std::optional<ControllerUnitKey> unit = std::nullopt)
        {
            PlayerEntry  entry;

            entry.kind = kind;
            entry.unit = unit;
            return entry;
        }


        static PlayerSlot MakeSlot (PlayerSlotState state, std::optional<ControllerUnitKey> holder = std::nullopt)
        {
            PlayerSlot  slot;

            slot.state  = state;
            slot.holder = holder;
            return slot;
        }


        //  A pad and a stick attached, both players on Automatic with nothing
        //  chosen, and each controller listing the normal-mode profiles.
        static InputModeRules::PickerSource MakeSource()
        {
            InputModeRules::PickerSource  source;

            source.devices = { MakePad ("045e:0b13", L"Xbox Controller (045e:0b13)"),
                               MakeStick ("{A}",     L"VKBsim Gladiator") };

            for (const ControllerDeviceInfo & device : source.devices)
            {
                source.profiles[ControllerTokens::UnitToToken (device.unit)] = { { "Default", "Paddles", "Swapped" }, "" };
            }

            return source;
        }


        static const ControllerUnitKey & Pad   (const InputModeRules::PickerSource & source) { return source.devices[0].unit; }
        static const ControllerUnitKey & Stick (const InputModeRules::PickerSource & source) { return source.devices[1].unit; }


        static std::vector<std::wstring> GetChoiceLabels (const InputModeRules::PlayerRow & row)
        {
            std::vector<std::wstring>  labels;

            for (const InputModeRules::PlayerChoice & choice : row.choices)
            {
                labels.push_back (choice.label);
            }

            return labels;
        }


        static size_t CountChecked (const InputModeRules::PlayerRow & row)
        {
            size_t  checked = 0;

            for (const InputModeRules::PlayerChoice & choice : row.choices)
            {
                checked += choice.isChecked ? 1 : 0;
            }

            return checked;
        }


        static const InputModeRules::PlayerChoice * FindChecked (const InputModeRules::PlayerRow & row)
        {
            for (const InputModeRules::PlayerChoice & choice : row.choices)
            {
                if (choice.isChecked)
                {
                    return &choice;
                }
            }

            return nullptr;
        }


        //
        //  The rows
        //

        TEST_METHOD (Rows_ShowWhatIsPlayingForEachPlayer)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;

            source.slots[0] = MakeSlot (PlayerSlotState::Playing, Pad (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 1: Xbox Controller (045e:0b13)"), picker.rows[0].label, L"the controller Automatic gave Player 1");
            Assert::AreEqual (std::wstring (L"Player 2: Automatic"),                   picker.rows[1].label, L"Automatic with nothing chosen yet");

            source.entries[0] = MakeEntry (PlayerEntryKind::ArrowKeys);
            source.entries[1] = MakeEntry (PlayerEntryKind::Disabled);
            source.slots      = PlayerSlots();
            picker            = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 1: Keys"),     picker.rows[0].label);
            Assert::AreEqual (std::wstring (L"Player 2: Disabled"), picker.rows[1].label);

            source.entries[0] = MakeEntry (PlayerEntryKind::MousePaddle);
            picker            = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 1: Mouse"), picker.rows[0].label);
        }


        //  The labels are supplied, so another mode can put its own words on
        //  the players and on Disabled.
        TEST_METHOD (Rows_TakeTheirWordsFromTheLabelsGiven)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;

            source.labels.players  = { L"Left", L"Right" };
            source.labels.disabled = L"Off";
            source.entries[1]      = MakeEntry (PlayerEntryKind::Disabled);
            picker                 = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Left: Automatic"), picker.rows[0].label);
            Assert::AreEqual (std::wstring (L"Right: Off"),      picker.rows[1].label);
            Assert::AreEqual (std::wstring (L"Off"),             picker.rows[1].choices.back().label, L"and Player 2's Disabled entry");
        }


        //  With the Joyport in effect the rows are the jacks: Joyport right on
        //  Automatic with no controller playing is the same as the left, and
        //  Player 2's Disabled reads Same as left. Off, the players keep their
        //  own words.
        TEST_METHOD (Rows_WithTheJoyportAreTheJacks)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;



            source.labels = JoyportLabels::GetPickerLabels (true);
            picker        = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Joyport left: Automatic"),     picker.rows[0].label);
            Assert::AreEqual (std::wstring (L"Joyport right: same as left"), picker.rows[1].label, L"Automatic with no controller");
            Assert::AreEqual (std::wstring (L"Same as left"),                picker.rows[1].choices.back().label, L"Player 2's Disabled entry");

            source.slots[1] = MakeSlot (PlayerSlotState::Waiting, Stick (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Joyport right: same as left"), picker.rows[1].label, L"a holder that has not given input plays nothing yet");

            source.slots[1] = MakeSlot (PlayerSlotState::Playing, Stick (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Joyport right: VKBsim Gladiator"), picker.rows[1].label, L"a controller playing");

            source.labels = JoyportLabels::GetPickerLabels (false);
            source.slots  = PlayerSlots();
            picker        = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 1: Automatic"), picker.rows[0].label);
            Assert::AreEqual (std::wstring (L"Player 2: Automatic"), picker.rows[1].label);
            Assert::AreEqual (std::wstring (L"Disabled"),            picker.rows[1].choices.back().label);
        }


        TEST_METHOD (PlayerOneSubmenu_ListsAutomaticTheControllersTheKeysAndTheMouse)
        {
            InputModeRules::Picker  picker = InputModeRules::BuildPicker (MakeSource());

            Assert::IsTrue (GetChoiceLabels (picker.rows[0]) == std::vector<std::wstring> {
                                L"Automatic", L"Xbox Controller (045e:0b13)", L"VKBsim Gladiator",
                                L"Use keys as joystick", L"Use mouse as paddle" });
            Assert::IsTrue (picker.rows[0].choices[3].entry.kind == PlayerEntryKind::ArrowKeys);
            Assert::IsTrue (picker.rows[0].choices[4].entry.kind == PlayerEntryKind::MousePaddle);
        }


        TEST_METHOD (PlayerTwoSubmenu_ListsAutomaticTheControllersAndDisabled)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker = InputModeRules::BuildPicker (source);

            Assert::IsTrue (GetChoiceLabels (picker.rows[1]) == std::vector<std::wstring> {
                                L"Automatic", L"Xbox Controller (045e:0b13)", L"VKBsim Gladiator", L"Disabled" });
            Assert::IsTrue (picker.rows[1].choices[1].entry.kind == PlayerEntryKind::Controller);
            Assert::IsTrue (picker.rows[1].choices[1].entry.unit == Pad (source), L"a controller's entry picks that controller");
            Assert::IsTrue (picker.rows[1].choices[3].entry.kind == PlayerEntryKind::Disabled);
        }


        //  Every entry each player can have, against every slot state with and
        //  without a controller in the slot: one check per submenu, always on
        //  the player's own entry.
        TEST_METHOD (EverySubmenu_HasExactlyOneCheckedEntry)
        {
            const PlayerSlotState         states[] = { PlayerSlotState::Empty,   PlayerSlotState::Provisional, PlayerSlotState::Waiting,
                                                       PlayerSlotState::Playing, PlayerSlotState::Held };
            InputModeRules::PickerSource  source   = MakeSource();
            ControllerDeviceInfo          gone     = MakeStick ("{GONE}", L"Gone");
            std::vector<PlayerEntry>      entries[PlayerSlotPolicy::kPlayerCount];
            size_t                        combos   = 0;

            for (size_t player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
            {
                entries[player] = { MakeEntry (PlayerEntryKind::Automatic),
                                    MakeEntry (PlayerEntryKind::Controller, Pad (source)),
                                    MakeEntry (PlayerEntryKind::Controller, gone.unit) };
            }

            entries[0].push_back (MakeEntry (PlayerEntryKind::ArrowKeys));
            entries[0].push_back (MakeEntry (PlayerEntryKind::MousePaddle));
            entries[1].push_back (MakeEntry (PlayerEntryKind::Disabled));

            for (size_t player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
            {
                for (const PlayerEntry & entry : entries[player])
                {
                    for (PlayerSlotState state : states)
                    {
                        for (std::optional<ControllerUnitKey> holder : { std::optional<ControllerUnitKey>(), std::optional<ControllerUnitKey> (Stick (source)) })
                        {
                            InputModeRules::PickerSource  combo = source;
                            InputModeRules::Picker        picker;

                            combo.entries[player] = entry;
                            combo.slots[player]   = MakeSlot (state, holder);
                            picker                = InputModeRules::BuildPicker (combo);

                            Assert::AreEqual (size_t (1), CountChecked (picker.rows[player]), L"one entry checked");
                            Assert::IsTrue   (FindChecked (picker.rows[player])->entry.kind == entry.kind, L"and it is the player's own");
                            Assert::IsTrue   (FindChecked (picker.rows[player])->entry.unit == entry.unit);
                            combos++;
                        }
                    }
                }
            }

            Assert::AreEqual (size_t ((5 + 4) * 5 * 2), combos, L"every entry of both players, in every slot state");
        }


        TEST_METHOD (Automatic_ShowsTheControllerItChoseOnceItHasOne)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Automatic"), picker.rows[0].choices[0].label, L"nothing chosen yet");

            source.slots[0] = MakeSlot (PlayerSlotState::Provisional, Stick (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Automatic (VKBsim Gladiator)"), picker.rows[0].choices[0].label);
            Assert::IsTrue   (picker.rows[0].choices[0].isChecked);
        }


        //  A pick that is unplugged stays the pick, and the player waits for
        //  it, so the submenu keeps it checked and says it is away.
        TEST_METHOD (AnAbsentPick_StaysCheckedAndIsMarkedNotConnected)
        {
            InputModeRules::PickerSource          source  = MakeSource();
            ControllerDeviceInfo                  gone    = MakeStick ("{GONE}", L"Old Gladiator");
            InputModeRules::Picker                picker;
            const InputModeRules::PlayerChoice  * checked = nullptr;

            source.entries[1] = MakeEntry (PlayerEntryKind::Controller, gone.unit);
            source.slots[1]   = MakeSlot (PlayerSlotState::Waiting, gone.unit);
            source.knownDescriptions[ControllerTokens::UnitToToken (gone.unit)] = gone.description;
            picker            = InputModeRules::BuildPicker (source);
            checked           = FindChecked (picker.rows[1]);

            Assert::IsNotNull (checked);
            Assert::AreEqual  (std::wstring (L"Old Gladiator (not connected)"), checked->label);
            Assert::IsFalse   (checked->isConnected);
            Assert::AreEqual  (std::wstring (L"Player 2: Old Gladiator (not connected)"), picker.rows[1].label);
            Assert::AreEqual  (size_t (5), picker.rows[1].choices.size(), L"Automatic, the two attached, the absent pick, Disabled");

            source.knownDescriptions.clear();
            picker = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Controller (231d:0121) (not connected)"), FindChecked (picker.rows[1])->label,
                L"one never seen this session is described from its model");
        }


        //
        //  The profile section
        //

        TEST_METHOD (ProfileSection_IsUnderThePlayingControllerWithTheActiveOneChecked)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;

            source.entries[1] = MakeEntry (PlayerEntryKind::Controller, Pad (source));
            source.slots[1]   = MakeSlot  (PlayerSlotState::Playing, Pad (source));
            source.profiles[ControllerTokens::UnitToToken (Pad (source))].active = "PADDLES";
            picker            = InputModeRules::BuildPicker (source);

            Assert::IsTrue   (picker.rows[1].profiles.has_value());
            Assert::AreEqual (std::wstring (L"Xbox Controller (045e:0b13)"), picker.rows[1].profiles->header);
            Assert::IsTrue   (picker.rows[1].profiles->unit == Pad (source));
            Assert::IsTrue   (picker.rows[1].profiles->names == std::vector<std::string> { "Default", "Paddles", "Swapped" });
            Assert::AreEqual (size_t (1), picker.rows[1].profiles->checked, L"the active profile, matched ignoring case");

            source.profiles[ControllerTokens::UnitToToken (Pad (source))].active = "Racing";
            picker = InputModeRules::BuildPicker (source);

            Assert::AreEqual (size_t (0), picker.rows[1].profiles->checked, L"one the list lacks plays the built-in profile, which leads");
        }


        //  A section only while a controller is in the slot and attached,
        //  including Automatic's choice before it has been used.
        TEST_METHOD (ProfileSection_IsAbsentWithNoControllerPlaying)
        {
            InputModeRules::PickerSource  source = MakeSource();
            ControllerDeviceInfo          gone   = MakeStick ("{GONE}", L"Gone");
            InputModeRules::Picker        picker = InputModeRules::BuildPicker (source);

            Assert::IsFalse (picker.rows[0].profiles.has_value(), L"Automatic before it has chosen");
            Assert::IsFalse (picker.rows[1].profiles.has_value());

            source.slots[0] = MakeSlot (PlayerSlotState::Provisional, Stick (source));
            source.slots[1] = MakeSlot (PlayerSlotState::Waiting,     Pad (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::IsTrue  (picker.rows[0].profiles.has_value(), L"Automatic's lone Player 1 before any input");
            Assert::IsTrue  (picker.rows[1].profiles.has_value(), L"Automatic's choice waiting for its first input");

            source.entries[0] = MakeEntry (PlayerEntryKind::ArrowKeys);
            source.entries[1] = MakeEntry (PlayerEntryKind::Disabled);
            source.slots      = PlayerSlots();
            picker            = InputModeRules::BuildPicker (source);

            Assert::IsFalse (picker.rows[0].profiles.has_value(), L"the keys");
            Assert::IsFalse (picker.rows[1].profiles.has_value(), L"Disabled");

            source.entries[0] = MakeEntry (PlayerEntryKind::MousePaddle);
            source.entries[1] = MakeEntry (PlayerEntryKind::Controller, gone.unit);
            source.slots[1]   = MakeSlot  (PlayerSlotState::Held, gone.unit);
            picker            = InputModeRules::BuildPicker (source);

            Assert::IsFalse (picker.rows[0].profiles.has_value(), L"the mouse");
            Assert::IsFalse (picker.rows[1].profiles.has_value(), L"a pick that is not connected");
        }


        //
        //  The face
        //

        TEST_METHOD (Label_IsWhatPlayerOnePlaysWithTheParentheticalDropped)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Controller"), picker.label, L"nothing drives the game port");
            Assert::IsTrue   (picker.driver == InputModeRules::PickerDriver::None);

            source.slots[0] = MakeSlot (PlayerSlotState::Provisional, Pad (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Xbox Controller"), picker.label, L"the vendor and product are dropped");
            Assert::IsTrue   (picker.driver == InputModeRules::PickerDriver::Controller);
            Assert::IsTrue   (picker.formFactor == ControllerFormFactor::Gamepad);

            source.entries[0] = MakeEntry (PlayerEntryKind::ArrowKeys);
            source.slots[0]   = PlayerSlot();
            picker            = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Keys"), picker.label);
            Assert::IsTrue   (picker.driver == InputModeRules::PickerDriver::ArrowKeys);

            source.entries[0] = MakeEntry (PlayerEntryKind::MousePaddle);
            picker            = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Mouse"), picker.label);
            Assert::IsTrue   (picker.driver == InputModeRules::PickerDriver::MousePaddle);
        }


        //  " +1" while Player 2 is playing, and not while Player 2 only has a
        //  controller waiting for its first input.
        TEST_METHOD (Label_EndsInPlusOneWhilePlayerTwoPlays)
        {
            InputModeRules::PickerSource  source = MakeSource();

            source.slots[0] = MakeSlot (PlayerSlotState::Playing, Stick (source));
            source.slots[1] = MakeSlot (PlayerSlotState::Waiting, Pad (source));

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"), InputModeRules::BuildPicker (source).label);

            source.slots[1] = MakeSlot (PlayerSlotState::Playing, Pad (source));

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator +1"), InputModeRules::BuildPicker (source).label);

            source.entries[0] = MakeEntry (PlayerEntryKind::ArrowKeys);
            source.slots[0]   = PlayerSlot();

            Assert::AreEqual (std::wstring (L"Keys +1"), InputModeRules::BuildPicker (source).label);
        }


        TEST_METHOD (Label_PlayerTwoPlayingAloneShowsItsControllerWithoutTheMark)
        {
            InputModeRules::PickerSource  source = MakeSource();
            ControllerDeviceInfo          gone   = MakeStick ("{GONE}", L"Gone");
            InputModeRules::Picker        picker;

            source.entries[0] = MakeEntry (PlayerEntryKind::Controller, gone.unit);
            source.slots[0]   = MakeSlot  (PlayerSlotState::Waiting, gone.unit);
            source.slots[1]   = MakeSlot  (PlayerSlotState::Playing, Stick (source));
            picker            = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"), picker.label);
            Assert::IsTrue   (picker.formFactor == ControllerFormFactor::Joystick);
        }


        //
        //  The chrome
        //

        static InputModeRules::Picker MakeTwoPlaying (InputModeRules::PickerSource & source)
        {
            source.entries[1] = MakeEntry (PlayerEntryKind::Controller, Stick (source));
            source.slots[0]   = MakeSlot  (PlayerSlotState::Playing, Pad (source));
            source.slots[1]   = MakeSlot  (PlayerSlotState::Playing, Stick (source));
            return InputModeRules::BuildPicker (source);
        }


        TEST_METHOD (Picker_ListsThePlayerRowsThenControllerSettings)
        {
            EmulatorCommands                commands;
            InputModeRules::PickerSource    source = MakeSource();
            std::vector<DxuiPopupMenuItem>  items;

            commands.SetPicker (MakeTwoPlaying (source));
            items = commands.GetPaddlePickerItems();

            Assert::AreEqual (size_t (4), items.size(), L"two player rows, a separator, then the settings entry");
            Assert::IsTrue   (items[0].kind == DxuiPopupMenuItem::Kind::Submenu);
            Assert::AreEqual (std::wstring (L"Player 1: Xbox Controller (045e:0b13)"), items[0].command->label);
            Assert::IsTrue   (items[1].kind == DxuiPopupMenuItem::Kind::Submenu);
            Assert::AreEqual (std::wstring (L"Player 2: VKBsim Gladiator"), items[1].command->label);
            Assert::IsTrue   (items[2].kind == DxuiPopupMenuItem::Kind::Separator);
            Assert::AreEqual ((int) IDM_VIEW_CONTROLLER_SETTINGS, items[3].command->id);
            Assert::AreEqual (std::wstring (L"Xbox Controller +1"), commands.GetPickerLabel());
        }


        //  The entries, a separator, then the section: the controller's
        //  description as a header, its profiles, and New... last.
        TEST_METHOD (Submenu_EndsWithTheProfileSection)
        {
            EmulatorCommands                commands;
            InputModeRules::PickerSource    source = MakeSource();
            std::vector<DxuiPopupMenuItem>  children;

            commands.SetPicker (MakeTwoPlaying (source));
            children = commands.GetPlayerItems()[1].children;

            Assert::AreEqual (size_t (4 + 1 + 1 + 3 + 1), children.size(), L"four entries, a separator, a header, three profiles, New...");
            Assert::IsTrue   (children[2].command->IsChecked(), L"Player 2's pick");
            Assert::IsTrue   (children[4].kind == DxuiPopupMenuItem::Kind::Separator);
            Assert::IsTrue   (children[5].kind == DxuiPopupMenuItem::Kind::Header);
            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"), children[5].command->label);
            Assert::AreEqual (std::wstring (L"Default"),          children[6].command->label);
            Assert::IsTrue   (children[6].command->IsChecked(),   L"with nothing chosen, the built-in profile");
            Assert::IsFalse  (children[7].command->IsChecked());
            Assert::AreEqual (std::wstring (L"New..."),           children.back().command->label);
        }


        TEST_METHOD (PickingAnEntry_RaisesThePlayerAndTheEntry)
        {
            EmulatorCommands              commands;
            InputModeRules::PickerSource  source = MakeSource();
            PlayerEntry                   picked;
            size_t                        player = 99;

            commands.SetPlayerPickedFn ([&] (size_t p, const PlayerEntry & entry) { player = p; picked = entry; });
            commands.SetPicker (InputModeRules::BuildPicker (source));

            commands.GetPlayerItems()[1].children[2].command->dispatch();

            Assert::AreEqual (size_t (1), player);
            Assert::IsTrue   (picked.kind == PlayerEntryKind::Controller);
            Assert::IsTrue   (picked.unit == Stick (source), L"the entry picks the controller it lists");

            commands.GetPlayerItems()[0].children[3].command->dispatch();

            Assert::AreEqual (size_t (0), player);
            Assert::IsTrue   (picked.kind == PlayerEntryKind::ArrowKeys);
        }


        TEST_METHOD (PickingAProfile_RaisesTheControllerAndTheName)
        {
            EmulatorCommands                commands;
            InputModeRules::PickerSource    source = MakeSource();
            std::vector<DxuiPopupMenuItem>  children;
            ControllerUnitKey               unit;
            std::string                     name   = "unset";

            commands.SetProfilePickedFn ([&] (const ControllerUnitKey & u, const std::string & n) { unit = u; name = n; });
            commands.SetPicker (MakeTwoPlaying (source));
            children = commands.GetPlayerItems()[1].children;

            children[7].command->dispatch();

            Assert::IsTrue   (unit == Stick (source), L"for the controller the section is under");
            Assert::AreEqual (std::string ("Paddles"), name);

            children[6].command->dispatch();

            Assert::AreEqual (std::string(), name, L"the built-in profile is picked as the empty name");
        }


        //  New... belongs to its section: it opens the New profile dialog for
        //  the controller the section is under, not whichever one Settings
        //  happens to open on.
        TEST_METHOD (NewProfile_IsForTheControllerOfItsOwnSection)
        {
            EmulatorCommands                   commands;
            InputModeRules::PickerSource       source = MakeSource();
            std::vector<ControllerUnitKey>     units;
            std::vector<DxuiPopupMenuItem>     players;



            commands.SetNewProfileFn ([&units] (const ControllerUnitKey & unit) { units.push_back (unit); });
            commands.SetPicker (MakeTwoPlaying (source));
            players = commands.GetPlayerItems();

            players[1].children.back().command->dispatch();
            players[0].children.back().command->dispatch();

            Assert::AreEqual (size_t (2), units.size());
            Assert::IsTrue   (units[0] == Stick (source), L"Player 2's New... is for Player 2's controller");
            Assert::IsTrue   (units[1] == Pad (source),   L"and Player 1's for Player 1's");
        }


        //  The list is rebuilt whenever a controller comes or goes, and that
        //  changes its length: an entry from a list built a moment earlier
        //  still picks what it showed.
        TEST_METHOD (AStaleEntryPicksWhatItWasBuiltFrom)
        {
            EmulatorCommands                commands;
            InputModeRules::PickerSource    source = MakeSource();
            std::vector<DxuiPopupMenuItem>  stale;
            PlayerEntry                     picked;

            commands.SetPlayerPickedFn ([&] (size_t, const PlayerEntry & entry) { picked = entry; });
            commands.SetPicker (InputModeRules::BuildPicker (source));
            stale = commands.GetPlayerItems()[0].children;

            source.devices.clear();
            commands.SetPicker (InputModeRules::BuildPicker (source));

            stale[3].command->dispatch();

            Assert::IsTrue (picked.kind == PlayerEntryKind::ArrowKeys, L"the keys, not what now sits at that index");
        }


        TEST_METHOD (RebuiltRows_AreReleasedOnceNothingHoldsThem)
        {
            EmulatorCommands                   commands;
            InputModeRules::PickerSource       source = MakeSource();
            std::weak_ptr<const DxuiCommand>   oldRow;

            commands.SetPicker (InputModeRules::BuildPicker (source));

            {
                std::vector<DxuiPopupMenuItem>  held = commands.GetPlayerItems();

                oldRow = held[0].children[0].command;

                commands.SetPicker (InputModeRules::BuildPicker (source));

                Assert::IsFalse (oldRow.expired(), L"the held items keep the replaced row alive");
            }

            Assert::IsTrue   (oldRow.expired(), L"with the items dropped, nothing keeps the replaced row");
            Assert::AreEqual (size_t (2), commands.GetPlayerItems().size());
        }


        TEST_METHOD (Glyph_FollowsTheDriver)
        {
            EmulatorCommands              commands;
            InputModeRules::PickerSource  source = MakeSource();

            commands.SetPicker (InputModeRules::BuildPicker (source));
            Assert::IsTrue (commands.GetPickerGlyph() == InputMonoGlyphKind::Gamepad, L"what could go there while nothing drives");

            source.slots[0] = MakeSlot (PlayerSlotState::Playing, Stick (source));
            commands.SetPicker (InputModeRules::BuildPicker (source));
            Assert::IsTrue (commands.GetPickerGlyph() == InputMonoGlyphKind::Joystick);

            source.entries[0] = MakeEntry (PlayerEntryKind::ArrowKeys);
            source.slots[0]   = PlayerSlot();
            commands.SetPicker (InputModeRules::BuildPicker (source));
            Assert::IsTrue (commands.GetPickerGlyph() == InputMonoGlyphKind::Keys);

            source.entries[0] = MakeEntry (PlayerEntryKind::MousePaddle);
            commands.SetPicker (InputModeRules::BuildPicker (source));
            Assert::IsTrue (commands.GetPickerGlyph() == InputMonoGlyphKind::Paddle);
        }


        TEST_METHOD (PickerCommand_WearsTheLabelFittedInTheMiddleKeepingPlusOne)
        {
            EmulatorCommands                    commands;
            std::shared_ptr<const DxuiCommand>  paddle = commands.Find (EmulatorCommands::kIdPaddle);

            Assert::IsTrue   (paddle != nullptr);
            Assert::AreEqual (std::wstring (L"Joystick and paddle source"), paddle->tip,
                L"the face already carries the answer, so the tooltip gives the purpose");
            Assert::IsTrue   (paddle->labelFit.has_value());
            Assert::IsTrue   (paddle->labelFit->mode == DxuiElide::Middle);
            Assert::AreEqual (std::wstring (L" +1"), paddle->labelFit->keptSuffix);
            Assert::AreEqual (EmulatorCommands::kPickerLabelMaxDip, paddle->labelFit->maxWidthDip);
        }


        //
        //  The Joyport row
        //

        static size_t FindRow (const std::vector<DxuiPopupMenuItem> & items, const std::wstring & label)
        {
            for (size_t i = 0; i < items.size(); i++)
            {
                if (items[i].command != nullptr && items[i].command->label == label)
                {
                    return i;
                }
            }

            return items.size();
        }


        //  It is a device on the game port, not one more thing that drives
        //  it, so it has a group of its own after the player rows.
        TEST_METHOD (Joyport_HasAGroupOfItsOwnAfterThePlayerRows)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;
            size_t                          row = 0;

            commands.SetPicker (InputModeRules::BuildPicker (MakeSource()));
            commands.SetJoyportFns ([] { return false; }, [] { return true; }, [] {});

            items = commands.GetPaddlePickerItems();
            row   = FindRow (items, L"Joyport (Atari mode)");

            Assert::AreEqual (size_t (3), row, L"after the two player rows and a separator");
            Assert::IsTrue   (items[2].kind == DxuiPopupMenuItem::Kind::Separator);
            Assert::IsTrue   (items[row + 1].kind == DxuiPopupMenuItem::Kind::Separator, L"and one below it, above Controller settings");
        }


        TEST_METHOD (Joyport_IsCheckedExactlyWhileAttached)
        {
            EmulatorCommands                commands;
            bool                            attached = false;
            std::vector<DxuiPopupMenuItem>  items;
            size_t                          row      = 0;

            commands.SetPicker (InputModeRules::BuildPicker (MakeSource()));
            commands.SetJoyportFns ([&attached] { return attached; }, [] { return true; }, [] {});

            items = commands.GetPaddlePickerItems();
            row   = FindRow (items, L"Joyport (Atari mode)");

            Assert::IsFalse (items[row].command->IsChecked(), L"detached");

            //  The check is read when the menu draws, so it follows the
            //  attach state however it was changed, with no rebuild.
            attached = true;
            Assert::IsTrue (items[row].command->IsChecked(), L"attached");
        }


        TEST_METHOD (Joyport_PickingTheRowTogglesOnce)
        {
            EmulatorCommands                commands;
            int                             toggles = 0;
            std::vector<DxuiPopupMenuItem>  items;

            commands.SetPicker (InputModeRules::BuildPicker (MakeSource()));
            commands.SetJoyportFns ([] { return false; }, [] { return true; }, [&toggles] { toggles++; });

            items = commands.GetPaddlePickerItems();
            items[FindRow (items, L"Joyport (Atari mode)")].command->dispatch();

            Assert::AreEqual (1, toggles);
        }


        //  The //c has no row, even with the setting on: it reads the Joyport
        //  as off, and the row's check would show a setting it ignores.
        TEST_METHOD (Joyport_IsLeftOutWhereItCannotBeAttached)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;



            commands.SetPicker (InputModeRules::BuildPicker (MakeSource()));
            commands.SetJoyportFns ([] { return true; }, [] { return false; }, [] {});

            items = commands.GetPaddlePickerItems();

            Assert::AreEqual (items.size(), FindRow (items, L"Joyport (Atari mode)"), L"the //c has no row");
        }


        //  The row is the unit's own Apple / Atari switch, and its label is the
        //  position it turns on.
        TEST_METHOD (Joyport_RowReadsJoyportAtariMode)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;
            size_t                          row = 0;



            commands.SetPicker (InputModeRules::BuildPicker (MakeSource()));
            commands.SetJoyportFns ([] { return false; }, [] { return true; }, [] {});

            items = commands.GetPaddlePickerItems();
            row   = FindRow (items, L"Joyport (Atari mode)");

            Assert::IsTrue   (row < items.size(), L"the row is listed");
            Assert::AreEqual (std::wstring (L"Joyport (Atari mode)"), items[row].command->label);
        }


        //  An Atari stick has no paddle for the mouse to stand in for, so
        //  Player 1's submenu leaves the mouse out while the Joyport is in
        //  effect, and lists it again once it is not.
        TEST_METHOD (MousePaddle_IsLeftOutWhileTheJoyportIsInEffect)
        {
            InputModeRules::PickerSource  source = MakeSource();



            source.isJoyportInEffect = true;

            Assert::IsTrue (GetChoiceLabels (InputModeRules::BuildPicker (source).rows[0]) == std::vector<std::wstring> {
                                L"Automatic", L"Xbox Controller (045e:0b13)", L"VKBsim Gladiator", L"Use keys as joystick" },
                            L"no mouse while the Joyport is in effect");

            source.isJoyportInEffect = false;

            Assert::IsTrue (GetChoiceLabels (InputModeRules::BuildPicker (source).rows[0]).back() == L"Use mouse as paddle",
                            L"and the mouse again once it is not");
        }


        //  The mouse picked before the Joyport was turned on stays Player 1's
        //  entry, driving nothing until it is turned off, so the submenu keeps
        //  it listed and checked rather than showing no entry at all.
        TEST_METHOD (MousePaddle_AlreadyPickedStaysCheckedWhileTheJoyportIsInEffect)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;



            source.isJoyportInEffect = true;
            source.entries[0]        = MakeEntry (PlayerEntryKind::MousePaddle);
            picker                   = InputModeRules::BuildPicker (source);

            Assert::AreEqual (size_t (1), CountChecked (picker.rows[0]));
            Assert::IsTrue   (FindChecked (picker.rows[0])->entry.kind == PlayerEntryKind::MousePaddle);
        }
    };
}
