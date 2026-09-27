#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSelectionPolicyTests
//
//  The rules both player slots share: the paddles a target maps to, what a
//  slot may be offered, how an overlapping pair is refused, and how a picked
//  controller that came back under another identity is recognized.
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
        //  A setup saved when Xbox-class units were keyed by XInput slot moves
        //  onto the product-keyed unit in that slot, once, so from then on the
        //  player follows the controller rather than the slot.
        //

        static ControllerDeviceInfo MakeXboxInSlot (int slot, const char * productId)
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.unit.unitId     = productId;
            info.unit.source     = ControllerUnitSource::XInputProduct;
            info.xinputSlot      = slot;

            return info;
        }


        static ControllerUnitKey MakeSlotKey (const char * slot)
        {
            ControllerUnitKey  key;

            key.model.kind = ControllerKind::XInput;
            key.unitId     = slot;
            key.source     = ControllerUnitSource::XInputSlot;

            return key;
        }


        TEST_METHOD (AdoptSlotKeyed_MovesEachPlayerOntoTheUnitInItsSlot)
        {
            ControllerDeviceInfo               oneS    = MakeXboxInSlot (0, "045e:02e0");
            ControllerDeviceInfo               series  = MakeXboxInSlot (1, "045e:0b13");
            std::vector<ControllerDeviceInfo>  devices = { oneS, series };
            MultiplayerSetup                   setup   = MakeTwoPlayers (MakeSlotKey ("0"), MakeSlotKey ("1"));

            Assert::IsTrue (ControllerSelectionPolicy::AdoptSlotKeyedPlayers (setup, devices), L"a move is reported, so it is saved");
            Assert::IsTrue (setup.players[0].unit.value() == oneS.unit);
            Assert::IsTrue (setup.players[1].unit.value() == series.unit);

            Assert::IsFalse (ControllerSelectionPolicy::AdoptSlotKeyedPlayers (setup, devices),
                L"a setup already keyed by product is left alone, so the move happens once");
        }


        // A slot with nothing in it is kept as it is, to be adopted when a
        // controller connects there, rather than being thrown away.
        TEST_METHOD (AdoptSlotKeyed_AnEmptySlotWaits)
        {
            std::vector<ControllerDeviceInfo>  devices = { MakeXboxInSlot (0, "045e:02e0") };
            MultiplayerSetup                   setup   = MakeTwoPlayers (MakeSlotKey ("0"), MakeSlotKey ("1"));

            Assert::IsTrue  (ControllerSelectionPolicy::AdoptSlotKeyedPlayers (setup, devices));
            Assert::IsTrue  (setup.players[0].unit->source == ControllerUnitSource::XInputProduct);
            Assert::IsTrue  (setup.players[1].unit.value() == MakeSlotKey ("1"), L"player two still names slot 1");
        }


        TEST_METHOD (AdoptSlotKeyed_OtherPlayersAreUntouched)
        {
            ControllerDeviceInfo               stick   = MakeStick ("{A}");
            std::vector<ControllerDeviceInfo>  devices = { MakeXboxInSlot (0, "045e:02e0"), stick };
            MultiplayerSetup                   setup   = MakeTwoPlayers (stick.unit, MakeXboxInSlot (0, "045e:02e0").unit);

            Assert::IsFalse (ControllerSelectionPolicy::AdoptSlotKeyedPlayers (setup, devices),
                L"a DirectInput player and a product-keyed player have nothing to adopt");
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


        TEST_METHOD (AdoptedUnit_AnAttachedPickIsItself)
        {
            ControllerDeviceInfo               stick   = MakeStick ("{A}");
            std::vector<ControllerDeviceInfo>  devices = { MakeXbox(), stick };

            Assert::IsTrue (ControllerSelectionPolicy::FindAdoptedUnit (stick.unit, devices).value() == stick.unit,
                L"a picked controller that is attached is played as itself");
        }


        TEST_METHOD (AdoptedUnit_SoleSameModelIsAdopted)
        {
            ControllerDeviceInfo               moved   = MakeStick ("{OLD-PORT}");
            ControllerDeviceInfo               same    = MakeStick ("{NEW-PORT}");
            std::vector<ControllerDeviceInfo>  devices = { MakeXbox(), same };

            Assert::IsTrue (ControllerSelectionPolicy::FindAdoptedUnit (moved.unit, devices).value() == same.unit,
                L"the same stick on another port is still that stick");
        }


        TEST_METHOD (AdoptedUnit_TwoOfTheSameModelIsNotAnAdoption)
        {
            ControllerDeviceInfo               moved   = MakeStick ("{OLD-PORT}");
            std::vector<ControllerDeviceInfo>  devices = { MakeStick ("{ONE}"), MakeStick ("{TWO}") };

            Assert::IsFalse (ControllerSelectionPolicy::FindAdoptedUnit (moved.unit, devices).has_value(),
                L"which of the two moved is a coin flip, so neither is taken to be it");
        }


        TEST_METHOD (AdoptedUnit_ADifferentModelIsNotAdopted)
        {
            ControllerDeviceInfo               moved   = MakeStick ("{OLD}", 0x0121);
            std::vector<ControllerDeviceInfo>  devices = { MakeStick ("{OTHER}", 0x0999) };

            Assert::IsFalse (ControllerSelectionPolicy::FindAdoptedUnit (moved.unit, devices).has_value(),
                L"another model is another controller, whatever port it is on");
        }


        // Slots are assigned in connection order and can change across a
        // replug, so the sole attached Xbox controller is taken to be the
        // picked one.
        TEST_METHOD (AdoptedUnit_SoleXboxIsAdopted)
        {
            std::vector<ControllerDeviceInfo>  devices = { MakeXbox (2) };

            Assert::IsTrue (ControllerSelectionPolicy::FindAdoptedUnit (MakeXbox (0).unit, devices).value() == devices.front().unit);
            Assert::IsTrue (ControllerSelectionPolicy::FindAdoptedUnit (MakeSlotlessXbox(), devices).value() == devices.front().unit,
                L"a pick saved before slots existed adopts the one Xbox controller attached");

            devices.push_back (MakeXbox (1));

            Assert::IsFalse (ControllerSelectionPolicy::FindAdoptedUnit (MakeXbox (0).unit, devices).has_value(),
                L"with two attached, which one the user had is a coin flip");
        }


        // Two of them are two controllers, so both can fill a player slot.
        TEST_METHOD (Xbox_TwoAttachedAreDistinctPlayers)
        {
            ControllerDeviceInfo  first  = MakeXbox (0);
            ControllerDeviceInfo  second = MakeXbox (1);
            MultiplayerSetup      setup  = MakeTwoPlayers (first.unit, second.unit);

            Assert::IsFalse (first.unit == second.unit,             L"two Xbox controllers are two units");
            Assert::IsTrue  (first.unit.model == second.unit.model, L"and one model, so they share profiles and deadzone");

            setup = ControllerSelectionPolicy::Normalize (setup);

            Assert::IsTrue  (setup.players[1].unit.has_value(), L"two of them can play as two players");
            Assert::IsTrue  (ControllerSelectionPolicy::FindPlayer (setup, first.unit).value() == 0);
            Assert::IsTrue  (ControllerSelectionPolicy::FindPlayer (setup, second.unit).value() == 1);
        }
    };
}
