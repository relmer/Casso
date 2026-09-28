#include "Pch.h"

#include "resource.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/PlayerModeRules.h"
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


        static std::vector<std::wstring> GetModeLabels (const InputModeRules::PlayerRow & row)
        {
            std::vector<std::wstring>  labels;

            for (const InputModeRules::PlayerModeChoice & choice : row.modes)
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


        //  The rows are the players whatever their modes: a player in a
        //  Joyport jack is not relabeled as the jack, and Player 2's Disabled
        //  keeps its word. The mode is in the submenu.
        TEST_METHOD (Rows_InTheJoyportAreStillThePlayers)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;



            source.hasJoyport      = true;
            source.entries[0].mode = PlayerMode::JoyportLeft;
            source.entries[1].mode = PlayerMode::SameAsPlayer1;
            source.slots[0]        = MakeSlot (PlayerSlotState::Playing, Stick (source));
            picker                 = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 1: VKBsim Gladiator"), picker.rows[0].label);
            Assert::AreEqual (std::wstring (L"Player 2: Automatic"),        picker.rows[1].label);
            Assert::AreEqual (std::wstring (L"Disabled"),                   picker.rows[1].choices.back().label);

            source.entries[1] = MakeEntry (PlayerEntryKind::Disabled);
            picker            = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 2: Disabled"), picker.rows[1].label);
        }


        //  The keys are a joystick and the mouse a paddle, so Player 1's
        //  submenu lists the keys in Joystick mode and the mouse in Paddle
        //  mode.
        TEST_METHOD (PlayerOneSubmenu_ListsTheKeysInJoystickModeAndTheMouseInPaddleMode)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker = InputModeRules::BuildPicker (source);

            Assert::IsTrue (GetChoiceLabels (picker.rows[0]) == std::vector<std::wstring> {
                                L"Automatic", L"Xbox Controller (045e:0b13)", L"VKBsim Gladiator", L"Use keys as joystick" },
                            L"Joystick mode: the keys and no mouse");
            Assert::IsTrue (picker.rows[0].choices[3].entry.kind == PlayerEntryKind::ArrowKeys);

            source.entries[0].mode = PlayerMode::Paddle;
            picker                 = InputModeRules::BuildPicker (source);

            Assert::IsTrue (GetChoiceLabels (picker.rows[0]) == std::vector<std::wstring> {
                                L"Automatic", L"Xbox Controller (045e:0b13)", L"VKBsim Gladiator", L"Use mouse as paddle" },
                            L"Paddle mode: the mouse and no keys");
            Assert::IsTrue (picker.rows[0].choices[3].entry.kind == PlayerEntryKind::MousePaddle);
        }


        //  Each submenu offers the player's modes with the player's own
        //  checked: Joystick then Paddle on a machine without a Joyport, and
        //  Automatic ahead of them for Player 2. With a Joyport the
        //  two jacks come between, and a jack the other player holds cannot
        //  be chosen.
        TEST_METHOD (Modes_EachSubmenuOffersThePlayersModes)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;
            size_t                        player = 0;



            source.entries[1].mode = PlayerMode::Paddle;
            picker                 = InputModeRules::BuildPicker (source);

            for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
            {
                const std::vector<InputModeRules::PlayerModeChoice> &  modes = picker.rows[player].modes;

                Assert::AreEqual (size_t (2 + player), modes.size());
                Assert::AreEqual (std::wstring (L"Joystick"), modes[player].label);
                Assert::AreEqual (std::wstring (L"Paddle"),   modes.back().label);
                Assert::IsTrue   (modes[player].isEnabled && modes.back().isEnabled);
            }

            Assert::AreEqual (std::wstring (L"Automatic (joystick)"), picker.rows[1].modes[0].label, L"with the mode it resolves to");
            Assert::IsTrue (picker.rows[0].modes[0].isChecked && !picker.rows[0].modes[1].isChecked, L"Player 1 in Joystick mode");
            Assert::IsTrue (!picker.rows[1].modes[1].isChecked && picker.rows[1].modes[2].isChecked, L"Player 2 in Paddle mode");

            source.hasJoyport      = true;
            source.entries[1].mode = PlayerMode::JoyportLeft;
            picker                 = InputModeRules::BuildPicker (source);

            Assert::IsTrue (GetModeLabels (picker.rows[0]) == std::vector<std::wstring> { L"Joystick", L"Joyport left (Atari)", L"Joyport right (Atari)", L"Paddle" });
            Assert::IsFalse (picker.rows[0].modes[1].isEnabled, L"Player 2 holds the left jack");
            Assert::IsTrue  (picker.rows[0].modes[2].isEnabled, L"the right jack is free");
            Assert::IsTrue  (picker.rows[1].modes[2].isChecked, L"Player 2 in the left jack");
        }


        //  A player in Paddle mode reads "(paddle)" after what plays for it;
        //  Joystick mode, and the keys and the mouse, need no mark.
        TEST_METHOD (Rows_InPaddleModeReadPaddle)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;



            source.entries[0].mode = PlayerMode::Paddle;
            source.entries[1].mode = PlayerMode::Paddle;
            source.slots[0]        = MakeSlot (PlayerSlotState::Playing, Stick (source));
            picker                 = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 1: VKBsim Gladiator (paddle)"), picker.rows[0].label);
            Assert::AreEqual (std::wstring (L"Player 2: Automatic (paddle)"),        picker.rows[1].label);

            source.entries[0]      = MakeEntry (PlayerEntryKind::MousePaddle);
            source.entries[0].mode = PlayerMode::Paddle;
            source.slots[0]        = PlayerSlot();
            picker                 = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 1: Mouse"), picker.rows[0].label, L"the mouse is a paddle already");

            source.hasJoyport      = true;
            source.entries[1].mode = PlayerMode::JoyportRight;
            picker                 = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Player 2: Automatic"), picker.rows[1].label, L"a jack takes no mark");
        }


        //  Player 1's controller left while Player 2 plays on: the face keeps
        //  Player 1's controller, marked disconnected, and "+1".
        TEST_METHOD (Label_ReadsDisconnectedWhilePlayerOnesSlotIsHeld)
        {
            InputModeRules::PickerSource  source = MakeSource();
            ControllerDeviceInfo          gone   = MakeStick ("{GONE}", L"Gone Stick (231d:0121)");
            InputModeRules::Picker        picker;



            source.knownDescriptions[ControllerTokens::UnitToToken (gone.unit)] = gone.description;
            source.slots[0] = MakeSlot (PlayerSlotState::Held,    gone.unit);
            source.slots[1] = MakeSlot (PlayerSlotState::Playing, Pad (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Gone Stick (disconnected) +1"), picker.label);
            Assert::IsTrue   (picker.driver == InputModeRules::PickerDriver::Controller);

            source.slots[1] = MakeSlot (PlayerSlotState::Waiting, Pad (source));
            picker          = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Controller"), picker.label, L"with nobody playing on, nothing is marked");
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

            Assert::AreEqual (size_t (4 + 1 + 3 + 1 + 1 + 3 + 1), children.size(),
                              L"four entries, a separator, three modes, a separator, a header, three profiles, New...");
            Assert::IsTrue   (children[2].command->IsChecked(), L"Player 2's pick");
            Assert::IsTrue   (children[4].kind == DxuiPopupMenuItem::Kind::Separator);
            Assert::AreEqual (std::wstring (L"Automatic (joystick)"), children[5].command->label);
            Assert::AreEqual (std::wstring (L"Joystick"),         children[6].command->label);
            Assert::AreEqual (std::wstring (L"Paddle"),           children[7].command->label);
            Assert::IsTrue   (children[6].command->IsChecked(),   L"the player's mode");
            Assert::IsFalse  (children[5].command->IsChecked() || children[7].command->IsChecked());
            Assert::IsTrue   (children[8].kind == DxuiPopupMenuItem::Kind::Separator);
            Assert::IsTrue   (children[9].kind == DxuiPopupMenuItem::Kind::Header);
            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"), children[9].command->label);
            Assert::AreEqual (std::wstring (L"Default"),          children[10].command->label);
            Assert::IsTrue   (children[10].command->IsChecked(),  L"with nothing chosen, the built-in profile");
            Assert::IsFalse  (children[11].command->IsChecked());
            Assert::AreEqual (std::wstring (L"New..."),           children.back().command->label);
        }


        //  A mode row raises its player and its mode, and is disabled while
        //  the Joyport is in effect.
        TEST_METHOD (PickingAMode_RaisesThePlayerAndTheMode)
        {
            constexpr size_t                kPaddleRow = 6;   // four entries and a separator, then Joystick
            EmulatorCommands                commands;
            InputModeRules::PickerSource    source     = MakeSource();
            std::vector<DxuiPopupMenuItem>  children;
            size_t                          player     = 99;
            PlayerMode                      mode       = PlayerMode::Joystick;



            commands.SetPlayerModeFn ([&] (size_t p, PlayerMode m) { player = p; mode = m; });
            commands.SetPicker (InputModeRules::BuildPicker (source));
            children = commands.GetPlayerItems()[0].children;

            Assert::AreEqual (std::wstring (L"Paddle"), children[kPaddleRow].command->label);
            children[kPaddleRow].command->dispatch();

            Assert::AreEqual (size_t (0), player);
            Assert::IsTrue   (mode == PlayerMode::Paddle);
            Assert::IsTrue   (children[kPaddleRow].command->IsEnabled());

            // With a Joyport the jacks follow Joystick, and the one Player 2
            // holds is listed and cannot be chosen.
            source.hasJoyport      = true;
            source.entries[1].mode = PlayerMode::JoyportLeft;
            commands.SetPicker (InputModeRules::BuildPicker (source));
            children = commands.GetPlayerItems()[0].children;

            Assert::AreEqual (std::wstring (L"Joyport left (Atari)"), children[kPaddleRow].command->label);
            Assert::IsFalse  (children[kPaddleRow].command->IsEnabled(), L"Player 2's jack");
            Assert::IsTrue   (children[kPaddleRow + 1].command->IsEnabled(), L"and the other one can be chosen");
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

            children[11].command->dispatch();

            Assert::IsTrue   (unit == Stick (source), L"for the controller the section is under");
            Assert::AreEqual (std::string ("Paddles"), name);

            children[10].command->dispatch();

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


        //  The Joyport has no row of its own: it is on while a player's mode,
        //  in that player's submenu, puts the player in one of its jacks. The
        //  picker lists the player rows and Controller settings.
        TEST_METHOD (Picker_HasNoJoyportRow)
        {
            EmulatorCommands                commands;
            std::vector<DxuiPopupMenuItem>  items;



            commands.SetPicker (InputModeRules::BuildPicker (MakeSource()));
            items = commands.GetPaddlePickerItems();

            Assert::AreEqual (items.size(), FindRow (items, L"Joyport (Atari mode)"), L"no Joyport row");
            Assert::IsTrue   (items[2].kind == DxuiPopupMenuItem::Kind::Separator, L"the player rows, then a separator");
            Assert::AreEqual (size_t (4), items.size(), L"and Controller settings");
        }


        //  The mouse is a paddle, so Player 1's submenu lists it only in
        //  Paddle mode: not in a Joyport jack, where the keys are offered.
        TEST_METHOD (MousePaddle_IsOfferedOnlyInPaddleMode)
        {
            InputModeRules::PickerSource  source = MakeSource();



            source.hasJoyport      = true;
            source.entries[0].mode = PlayerMode::JoyportLeft;

            Assert::IsTrue (GetChoiceLabels (InputModeRules::BuildPicker (source).rows[0]) == std::vector<std::wstring> {
                                L"Automatic", L"Xbox Controller (045e:0b13)", L"VKBsim Gladiator", L"Use keys as joystick" },
                            L"in a jack, the keys and no mouse");

            source.entries[0].mode = PlayerMode::Paddle;

            Assert::IsTrue (GetChoiceLabels (InputModeRules::BuildPicker (source).rows[0]).back() == L"Use mouse as paddle",
                            L"and the mouse in Paddle mode");
        }


        //  The mouse picked plays in Paddle mode, beside Player 2 in a jack
        //  as anywhere, and stays listed and checked.
        TEST_METHOD (MousePaddle_PickedStaysCheckedBesideTheJoyport)
        {
            InputModeRules::PickerSource  source = MakeSource();
            InputModeRules::Picker        picker;



            source.hasJoyport      = true;
            source.entries         = PlayerSlotPolicy::NormalizeEntries ({ MakeEntry (PlayerEntryKind::MousePaddle), PlayerEntry() });
            source.entries[1].mode = PlayerMode::JoyportRight;
            picker                 = InputModeRules::BuildPicker (source);

            Assert::AreEqual (size_t (1), CountChecked (picker.rows[0]));
            Assert::IsTrue   (FindChecked (picker.rows[0])->entry.kind == PlayerEntryKind::MousePaddle);
        }


        //  The face while Player 1's slot is held ends in "(disconnected) +1",
        //  and the command's fit keeps that whole suffix when the toolbar
        //  cuts the description in the middle, not only its " +1".
        TEST_METHOD (PickerCommand_KeepsTheDisconnectedSuffixWhole)
        {
            EmulatorCommands                    commands;
            std::shared_ptr<const DxuiCommand>  paddle = commands.Find (EmulatorCommands::kIdPaddle);
            InputModeRules::PickerSource        source = MakeSource();
            ControllerDeviceInfo                gone   = MakeStick ("{GONE}", L"Gone Stick (231d:0121)");
            InputModeRules::Picker              picker;
            bool                                isKept = false;



            source.entries[1]                                                   = MakeEntry (PlayerEntryKind::Controller, Pad (source));
            source.slots[0]                                                     = MakeSlot (PlayerSlotState::Held, gone.unit);
            source.slots[1]                                                     = MakeSlot (PlayerSlotState::Playing, Pad (source));
            source.knownDescriptions[ControllerTokens::UnitToToken (gone.unit)] = gone.description;
            picker                                                              = InputModeRules::BuildPicker (source);

            Assert::AreEqual (std::wstring (L"Gone Stick (disconnected) +1"), picker.label);
            Assert::IsTrue   (paddle != nullptr && paddle->labelFit.has_value());

            for (const std::wstring & suffix : paddle->labelFit->keptSuffixes)
            {
                isKept = isKept || (suffix == L" (disconnected) +1" && picker.label.ends_with (suffix));
            }

            Assert::IsTrue (isKept, L"the whole suffix is kept");
        }    };
}
