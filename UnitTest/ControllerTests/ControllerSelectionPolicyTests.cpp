#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSelectionPolicyTests
//
//  Every row of the policy table, driven with made-up devices.
//
//  THE RULE THAT MATTERS MOST IS THAT THE SELECTION STAYS ON THE CONTROLLER
//  IN USE. One arriving never takes it; only the selected one leaving moves
//  it, and then to a controller that is here or to nothing.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControllerSelectionPolicyTests)
    {
    public:

        static ControllerDeviceInfo MakeXbox (int slot = 0)
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.unit.unitId     = std::to_string (slot);
            info.unit.source     = ControllerUnitSource::XInputSlot;
            info.description     = L"Xbox Controller";
            info.xinputSlot      = slot;
            return info;
        }


        // What a preferences file written before XInput units carried a slot
        // holds: an Xbox-class controller with no slot on it.
        static ControllerUnitKey MakeSlotlessXbox()
        {
            ControllerUnitKey  unit;

            unit.model.kind = ControllerKind::XInput;
            return unit;
        }


        static ControllerDeviceInfo MakeStick (const std::string & unitId, unsigned short product = 0x0121)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, product };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"VKBsim Gladiator";
            return info;
        }


        // Two players on the two joysticks, which is the setup every rule
        // below is stated against.
        static MultiplayerSetup MakeTwoPlayers (const ControllerUnitKey & first, const ControllerUnitKey & second)
        {
            MultiplayerSetup  setup;

            setup.isEnabled         = true;
            setup.players[0].unit   = first;
            setup.players[0].target = PlayerAxisTarget::Joystick0;
            setup.players[1].unit   = second;
            setup.players[1].target = PlayerAxisTarget::Joystick1;
            return setup;
        }


        TEST_METHOD (Players_DriveOnlyThePaddlesTheirSlotMapsTo)
        {
            ControllerUnitKey  xbox   = MakeXbox().unit;
            ControllerUnitKey  stick  = MakeStick ("{A}").unit;
            MultiplayerSetup   setup  = MakeTwoPlayers (xbox, stick);

            Assert::AreEqual (0x3ul, ControllerSelectionPolicy::GetAxesForPlayer (setup, 0, 4).to_ulong(),
                L"player one plays joystick 0, which is PDL0 and PDL1");
            Assert::AreEqual (0xCul, ControllerSelectionPolicy::GetAxesForPlayer (setup, 1, 4).to_ulong());

            Assert::IsTrue   (ControllerSelectionPolicy::FindPlayer (setup, stick).value() == 1);
            Assert::IsFalse  (ControllerSelectionPolicy::FindPlayer (setup, MakeStick ("{OTHER}").unit).has_value(),
                L"a controller in neither slot is no player, so it plays nothing");

            setup.isEnabled = false;

            Assert::AreEqual (0x0ul, ControllerSelectionPolicy::GetAxesForPlayer (setup, 0, 4).to_ulong(),
                L"with the mode off the slots drive nothing at all");
            Assert::IsFalse  (ControllerSelectionPolicy::FindPlayer (setup, stick).has_value());
        }


        //
        //  The saved mode is intent. What the machine plays is the mode less
        //  any player whose controller is not plugged in, so a game port with
        //  a controller attached is never left dead (FR-040).
        //

        TEST_METHOD (Playable_NeedsOneOfItsPlayersAttached)
        {
            ControllerDeviceInfo               xbox    = MakeXbox();
            ControllerDeviceInfo               stick   = MakeStick ("{A}");
            MultiplayerSetup                   setup   = MakeTwoPlayers (xbox.unit, stick.unit);
            std::vector<ControllerDeviceInfo>  both    = { xbox, stick };
            std::vector<ControllerDeviceInfo>  oneOnly = { stick };
            std::vector<ControllerDeviceInfo>  neither = { MakeStick ("{OTHER}") };

            Assert::IsTrue  (ControllerSelectionPolicy::IsMultiplayerPlayable (setup, both),
                L"both players are plugged in, so the mode is played as saved");
            Assert::IsTrue  (ControllerSelectionPolicy::IsMultiplayerPlayable (setup, oneOnly),
                L"one player leaving does not end the game for the other (SC-012)");
            Assert::IsFalse (ControllerSelectionPolicy::IsMultiplayerPlayable (setup, neither),
                L"with neither player attached the port would be dead, so one controller takes it");
            Assert::IsFalse (ControllerSelectionPolicy::IsMultiplayerPlayable (setup, {}),
                L"and with nothing attached there is nothing to play");
        }


        // A user who set one player up and left the other slot alone is
        // playing a one-player setup through the mode; the empty slot is not
        // a controller anyone has to plug in.
        TEST_METHOD (Playable_AnEmptySlotIsNotARequirement)
        {
            ControllerDeviceInfo               stick = MakeStick ("{A}");
            MultiplayerSetup                   setup;
            std::vector<ControllerDeviceInfo>  devices = { stick };

            setup.isEnabled       = true;
            setup.players[0].unit = stick.unit;

            Assert::IsTrue (ControllerSelectionPolicy::IsMultiplayerPlayable (setup, devices));
        }


        // With both slots empty there is no two-player game to play, whatever
        // the saved flag says, so the one controller drives as it always did.
        TEST_METHOD (Playable_AnEnabledModeWithNoPlayersIsNotPlayed)
        {
            ControllerDeviceInfo               stick   = MakeStick ("{A}");
            MultiplayerSetup                   setup;
            std::vector<ControllerDeviceInfo>  devices = { stick };

            setup.isEnabled = true;

            Assert::IsFalse (ControllerSelectionPolicy::IsMultiplayerPlayable (setup, devices));
        }


        TEST_METHOD (Playable_ModeOffIsNeverPlayed)
        {
            ControllerDeviceInfo               xbox    = MakeXbox();
            ControllerDeviceInfo               stick   = MakeStick ("{A}");
            MultiplayerSetup                   setup   = MakeTwoPlayers (xbox.unit, stick.unit);
            std::vector<ControllerDeviceInfo>  devices = { xbox, stick };

            setup.isEnabled = false;

            Assert::IsFalse (ControllerSelectionPolicy::IsMultiplayerPlayable (setup, devices));
        }


        TEST_METHOD (Normalize_RefusesARepeatedControllerAndAnOverlap)
        {
            ControllerUnitKey  xbox   = MakeXbox().unit;
            ControllerUnitKey  stick  = MakeStick ("{A}").unit;
            MultiplayerSetup   same   = MakeTwoPlayers (xbox, xbox);
            MultiplayerSetup   shared = MakeTwoPlayers (xbox, stick);
            MultiplayerSetup   fine   = MakeTwoPlayers (xbox, stick);

            same = ControllerSelectionPolicy::Normalize (same);

            Assert::IsFalse (same.players[1].unit.has_value(),
                L"one controller cannot be both players, so the later slot gives way");
            Assert::IsTrue  (same.players[0].unit.has_value(), L"and the first player is left alone");

            // Player two asks for a paddle player one already holds as half of
            // joystick 0.
            shared.players[1].target = PlayerAxisTarget::Paddle1;
            shared                   = ControllerSelectionPolicy::Normalize (shared);

            Assert::IsFalse (shared.players[1].unit.has_value(), L"two players cannot claim one paddle (FR-036)");

            fine.players[1].target = PlayerAxisTarget::Paddle2;
            fine                   = ControllerSelectionPolicy::Normalize (fine);

            Assert::IsTrue  (fine.players[1].unit.has_value(), L"a paddle nobody else holds is kept");
        }


        TEST_METHOD (TargetChoices_LeaveOutTheOtherPlayersAndWhatTheMachineLacks)
        {
            ControllerUnitKey              xbox    = MakeXbox().unit;
            ControllerUnitKey              stick   = MakeStick ("{A}").unit;
            MultiplayerSetup               setup   = MakeTwoPlayers (xbox, stick);
            std::vector<PlayerAxisTarget>  choices = ControllerSelectionPolicy::GetTargetChoices (setup, 1, 4);

            // Player one holds joystick 0, so PDL0 and PDL1 are gone in every
            // form they could be offered in.
            Assert::AreEqual (size_t (3), choices.size(), L"joystick 1, paddle 2 and paddle 3 are what is left");
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick1, (int) choices[0]);
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle2,   (int) choices[1]);
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle3,   (int) choices[2]);

            choices = ControllerSelectionPolicy::GetTargetChoices (setup, 1, 2);

            Assert::AreEqual (size_t (0), choices.size(),
                L"on a //c player one's joystick 0 is the whole game port, so player two has nothing to take");

            setup.players[0].unit.reset();
            choices = ControllerSelectionPolicy::GetTargetChoices (setup, 1, 2);

            Assert::AreEqual (size_t (3), choices.size(), L"with no other player, a //c offers joystick 0, paddle 0 and paddle 1");
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick0, (int) choices[0]);
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle0,   (int) choices[1]);
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle1,   (int) choices[2]);
        }


        TEST_METHOD (Replacement_PrefersAControllerNoPlayerIsHolding)
        {
            ControllerDeviceInfo               gone   = MakeStick ("{GONE}");
            ControllerDeviceInfo               held   = MakeStick ("{HELD}");
            ControllerDeviceInfo               free   = MakeStick ("{FREE}");
            std::vector<ControllerDeviceInfo>  devices = { held, free };
            MultiplayerSetup                   setup;

            setup.isEnabled         = true;
            setup.players[0].unit   = held.unit;
            setup.players[0].target = PlayerAxisTarget::Joystick0;

            ControllerSelectionPolicy::Decision  decision = ControllerSelectionPolicy::Evaluate (gone.unit, devices, true, setup);

            Assert::IsTrue (decision.selection.value() == free.unit,
                L"a controller a player is holding is left to them, though it was attached longer");

            decision = ControllerSelectionPolicy::Evaluate (gone.unit, { held }, true, setup);

            Assert::IsTrue (decision.selection.value() == held.unit, L"with no free controller, a player's still takes the selection");
        }


        TEST_METHOD (NoneSelected_FirstAttachedIsTaken)
        {
            std::vector<ControllerDeviceInfo>          devices  = { MakeStick ("{A}"), MakeXbox() };
            ControllerSelectionPolicy::Decision        decision = ControllerSelectionPolicy::Evaluate (std::nullopt, devices, true);

            Assert::IsTrue  (decision.hasChanged,                                    L"a controller must be chosen when none is");
            Assert::IsTrue  (decision.selection.value() == devices.front().unit,     L"the first one enumerated is the one chosen");
            Assert::IsTrue  (decision.clearsOtherInputModes,                         L"and it takes the axes from the arrows and the paddle");
            Assert::AreEqual ((int) SelectionChangeReason::AutomaticSelection, (int) decision.reason);
        }


        TEST_METHOD (NoneSelected_NothingAttachedChangesNothing)
        {
            std::vector<ControllerDeviceInfo>    devices;
            ControllerSelectionPolicy::Decision  decision = ControllerSelectionPolicy::Evaluate (std::nullopt, devices, true);

            Assert::IsFalse (decision.hasChanged,            L"there is nothing to choose");
            Assert::IsFalse (decision.selection.has_value(), L"so the selection stays empty");
        }


        TEST_METHOD (AlreadySelected_ConnectingAnotherChangesNothing)
        {
            ControllerDeviceInfo               stick    = MakeStick ("{A}");
            std::vector<ControllerDeviceInfo>  devices  = { stick, MakeXbox() };
            auto                               decision = ControllerSelectionPolicy::Evaluate (stick.unit, devices, true);

            Assert::IsFalse (decision.hasChanged,                     L"a controller arriving must not steal the selection");
            Assert::IsTrue  (decision.selection.value() == stick.unit, L"the chosen one keeps it");
        }


        TEST_METHOD (SelectedAbsent_LongestAttachedTakesOver)
        {
            ControllerDeviceInfo               gone     = MakeStick ("{GONE}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeXbox(), MakeStick ("{LATER}", 0x0999) };
            auto                               decision = ControllerSelectionPolicy::Evaluate (gone.unit, devices, true);

            Assert::IsTrue   (decision.hasChanged,                                L"the selection follows the controllers that are here");
            Assert::IsTrue   (decision.selection.value() == devices.front().unit, L"and the one attached longest, listed first, takes it");
            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason);
            Assert::IsFalse  (decision.clearsOtherInputModes,                     L"a controller already had the axes, so nothing else was on");
        }


        TEST_METHOD (SelectedAbsent_NothingAttachedClearsTheSelection)
        {
            ControllerDeviceInfo               gone     = MakeStick ("{GONE}");
            std::vector<ControllerDeviceInfo>  devices;
            auto                               decision = ControllerSelectionPolicy::Evaluate (gone.unit, devices, true);

            Assert::IsTrue   (decision.hasChanged);
            Assert::IsFalse  (decision.selection.has_value(), L"nothing drives the axes, and the arrow keys are not turned on for the user");
            Assert::AreEqual ((int) SelectionChangeReason::Cleared, (int) decision.reason);
            Assert::IsFalse  (decision.clearsOtherInputModes);
        }


        TEST_METHOD (SelectedAbsent_SoleSameModelIsAdopted)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD-PORT}");
            ControllerDeviceInfo               same     = MakeStick ("{NEW-PORT}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeXbox(), same };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            Assert::IsTrue   (decision.hasChanged,                    L"the same stick on another port is still that stick");
            Assert::IsTrue   (decision.selection.value() == same.unit, L"so its new identity is adopted, ahead of a controller attached longer");
            Assert::AreEqual ((int) SelectionChangeReason::Adoption, (int) decision.reason);
        }


        TEST_METHOD (SelectedAbsent_TwoOfTheSameModelIsNotAnAdoption)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD-PORT}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeStick ("{ONE}"), MakeStick ("{TWO}") };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            // Which of the two moved is a coin flip, so they count as any
            // other controller would, and the one attached longest takes over.
            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason);
            Assert::IsTrue   (decision.selection.value() == devices.front().unit);
        }


        TEST_METHOD (SelectedAbsent_ADifferentModelIsNotAdopted)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD}", 0x0121);
            std::vector<ControllerDeviceInfo>  devices  = { MakeStick ("{OTHER}", 0x0999) };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason,
                L"another model is another controller, whatever port it is on");
        }


        TEST_METHOD (XboxSelection_KeepsTheSlotItWasMadeOn)
        {
            ControllerDeviceInfo               xbox     = MakeXbox (1);
            std::vector<ControllerDeviceInfo>  devices  = { xbox };
            auto                               decision = ControllerSelectionPolicy::Evaluate (xbox.unit, devices, true);

            Assert::IsFalse (decision.hasChanged, L"the selected controller is attached, so nothing moves");
            Assert::IsTrue  (ControllerSelectionPolicy::IsSelectedAttached (xbox.unit, devices));
        }


        // Two of them are two controllers, so both can be picked and both can
        // fill a player slot.
        TEST_METHOD (Xbox_TwoAttachedAreDistinctAndBothSelectable)
        {
            ControllerDeviceInfo               first    = MakeXbox (0);
            ControllerDeviceInfo               second   = MakeXbox (1);
            std::vector<ControllerDeviceInfo>  devices  = { first, second };
            MultiplayerSetup                   setup    = MakeTwoPlayers (first.unit, second.unit);
            auto                               decision = ControllerSelectionPolicy::Evaluate (second.unit, devices, true);

            Assert::IsFalse (first.unit == second.unit,       L"two Xbox controllers are two units");
            Assert::IsTrue  (first.unit.model == second.unit.model, L"and one model, so they share profiles and deadzone");
            Assert::IsFalse (decision.hasChanged,             L"the selection stays on the one that was picked");
            Assert::IsTrue  (decision.selection.value() == second.unit);

            setup = ControllerSelectionPolicy::Normalize (setup);

            Assert::IsTrue  (setup.players[1].unit.has_value(), L"two of them can play as two players");
            Assert::IsTrue  (ControllerSelectionPolicy::FindPlayer (setup, first.unit).value() == 0);
            Assert::IsTrue  (ControllerSelectionPolicy::FindPlayer (setup, second.unit).value() == 1);
        }


        // Slots are assigned in connection order and can change across a
        // replug, so the sole attached Xbox controller is taken to be the
        // saved one.
        TEST_METHOD (XboxSelectedAbsent_SoleXboxIsAdopted)
        {
            std::vector<ControllerDeviceInfo>  devices  = { MakeXbox (2) };
            auto                               decision = ControllerSelectionPolicy::Evaluate (MakeXbox (0).unit, devices, true);

            Assert::AreEqual ((int) SelectionChangeReason::Adoption, (int) decision.reason);
            Assert::IsTrue   (decision.selection.value() == devices.front().unit);

            decision = ControllerSelectionPolicy::Evaluate (MakeSlotlessXbox(), devices, true);

            Assert::AreEqual ((int) SelectionChangeReason::Adoption, (int) decision.reason,
                L"a selection saved before slots existed adopts the one Xbox controller attached");
            Assert::IsTrue   (decision.selection.value() == devices.front().unit);
        }


        TEST_METHOD (XboxSelectedAbsent_TwoAttachedIsNotAnAdoption)
        {
            std::vector<ControllerDeviceInfo>  devices  = { MakeXbox (1), MakeXbox (2) };
            auto                               decision = ControllerSelectionPolicy::Evaluate (MakeXbox (0).unit, devices, true);

            // Which one the user had is a coin flip, so the one attached
            // longest takes over, as for any other controller.
            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason);
            Assert::IsTrue   (decision.selection.value() == devices.front().unit);
        }


        TEST_METHOD (NoGamePort_PolicyIsInertButKeepsTheSelection)
        {
            ControllerDeviceInfo               stick    = MakeStick ("{A}");
            std::vector<ControllerDeviceInfo>  devices  = { stick };
            auto                               decision = ControllerSelectionPolicy::Evaluate (std::nullopt, devices, false);

            Assert::IsFalse (decision.hasChanged,            L"a machine with no game port must not select anything");
            Assert::IsFalse (decision.selection.has_value(), L"and nothing is invented for it");

            decision = ControllerSelectionPolicy::Evaluate (stick.unit, devices, false);
            Assert::IsTrue (decision.selection.value() == stick.unit,
                L"a selection made on another machine survives being carried to one with no port");
        }


        TEST_METHOD (IsSelectedAttached_FalseWithoutASelection)
        {
            std::vector<ControllerDeviceInfo>  devices = { MakeXbox() };

            Assert::IsFalse (ControllerSelectionPolicy::IsSelectedAttached (std::nullopt, devices));
        }
    };
}
