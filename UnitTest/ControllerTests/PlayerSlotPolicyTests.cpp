#include "Pch.h"

#include "Controllers/PlayerSlotPolicy.h"
#include "Controllers/PlayerModeRules.h"

#include "Controllers/DeadzoneShaper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerSlotPolicyTests
//
//  Who plays for each player, through every sequence of arrivals, first uses
//  and departures the rules cover. Each test runs the policy the way the
//  service does: the slots it returned are the previous slots of the next
//  evaluation, and the logs it was handed come back changed.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (PlayerSlotPolicyTests)
    {
    public:

        static constexpr float  kDeadzone = DeadzoneShaper::kDirectInputDeadzone;
        static constexpr float  kNudge    = 0.01f;


        static constexpr Word   kPadProduct   = 0x0006;
        static constexpr Word   kStickProduct = 0x0121;


        static ControllerDeviceInfo MakePad (const char * unitId, Word product = kPadProduct)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x0079, product };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"Pad";
            return info;
        }


        // The state the tests step through: what is chosen, what is attached,
        // the two logs, and the slots the last evaluation produced.
        struct World
        {
            PlayerEntries                      entries;
            std::vector<ControllerDeviceInfo>  devices;
            PlayerOrderLogs                    logs;
            PlayerSlots                        slots;

            void Step()
            {
                slots = PlayerSlotPolicy::Evaluate (entries, devices, logs, slots, {});
            }

            void Attach (const ControllerDeviceInfo & device)
            {
                devices.push_back (device);
            }

            void Detach (const ControllerDeviceInfo & device)
            {
                std::erase_if (devices, [&device] (const ControllerDeviceInfo & d) { return d.unit == device.unit; });
            }
        };


        static bool Holds (const PlayerSlot & slot, const ControllerDeviceInfo & device)
        {
            return slot.holder.has_value() && slot.holder.value() == device.unit;
        }


        static PlayerEntry MakePick (const ControllerDeviceInfo & device)
        {
            PlayerEntry  entry;

            entry.kind = PlayerEntryKind::Controller;
            entry.unit = device.unit;
            return entry;
        }


        //
        //  Ordering
        //

        TEST_METHOD (ConnectionLog_TwoArrivalsFillThePlayersInArrivalOrder)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");

            // Enumeration lists the second arrival first, so only the log can
            // put them in the order they arrived.
            world.devices        = { second, first };
            world.logs.connected = { first.unit, second.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], first),  L"the first to connect is Player 1 (SC-014)");
            Assert::IsTrue   (Holds (world.slots[1], second), L"and the second Player 2");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state, L"a controller that took its slot by connecting is in use at once");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[1].state);
            Assert::IsFalse  (PlayerSlotPolicy::IsOnePlaying (world.slots, world.entries), L"so the two split what one would drive");
        }


        //  The owner's case: a pad turned on while Casso runs is Player 1
        //  before anyone uses it, and turning on a second puts both in use,
        //  so the players split the jacks at once.
        TEST_METHOD (ConnectionLog_ASecondArrivalPutsTheLonePlayerOneInUse)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");



            world.Attach (first);
            world.logs.connected = { first.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], first));
            Assert::AreEqual ((int) PlayerSlotState::Provisional, (int) world.slots[0].state, L"alone, it waits for input");

            world.Attach (second);
            world.logs.connected.push_back (second.unit);
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], first),  L"the first to connect stays Player 1");
            Assert::IsTrue   (Holds (world.slots[1], second), L"and the second is Player 2");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state, L"both in use with no input from either");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[1].state);
            Assert::IsFalse  (PlayerSlotPolicy::IsOnePlaying (world.slots, world.entries));

            world.Step();

            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state, L"and they stay in use");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[1].state);
        }


        //  A controller that connected while Casso runs and left while the
        //  other played returns to its held slot in use, as a return rather
        //  than a new arrival.
        TEST_METHOD (ConnectionLog_AReturnToAHeldSlotIsInUse)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");



            world.devices        = { first, second };
            world.logs.connected = { first.unit, second.unit };
            world.Step();

            world.Detach (first);
            world.Step();

            Assert::AreEqual ((int) PlayerSlotState::Held, (int) world.slots[0].state);

            world.Attach (first);
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], first), L"it takes its own slot back");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state, L"in use");
            Assert::IsTrue   (Holds (world.slots[1], second), L"and Player 2 is unmoved");
        }


        TEST_METHOD (InputLog_FillsThePlayersWhenFewerThanTwoArrived)
        {
            World                 world;
            ControllerDeviceInfo  stick = MakePad ("{STICK}");
            ControllerDeviceInfo  pad   = MakePad ("{PAD}");

            world.devices         = { stick, pad };
            world.logs.connected  = { stick.unit };
            world.logs.firstInput = { pad.unit, stick.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], pad),   L"one arrival is no order, so the first used is Player 1 (SC-013)");
            Assert::IsTrue   (Holds (world.slots[1], stick), L"and the next used Player 2");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state, L"a controller chosen by its input is in use");
        }


        TEST_METHOD (LoneController_IsPlayerOneBeforeItIsUsed)
        {
            World                 world;
            ControllerDeviceInfo  pad = MakePad ("{PAD}");

            world.devices = { pad };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], pad), L"a lone controller plays as Player 1 at once (FR-032)");
            Assert::AreEqual ((int) PlayerSlotState::Provisional, (int) world.slots[0].state, L"assigned, but not in use");
            Assert::IsTrue   (PlayerSlotPolicy::IsDrivingSlot (world.slots[0]), L"and it drives the port");
        }


        //  If another controller arrives before either is used, whichever is
        //  used first becomes Player 1, even the one that just arrived.
        TEST_METHOD (LoneController_GivesWayToTheFirstControllerUsed)
        {
            World                 world;
            ControllerDeviceInfo  stick = MakePad ("{STICK}");
            ControllerDeviceInfo  pad   = MakePad ("{PAD}");

            world.devices = { stick };
            world.Step();

            world.Attach (pad);
            world.logs.connected = { pad.unit };
            world.Step();

            Assert::IsTrue (Holds (world.slots[0], stick), L"the arrival alone moves nothing");
            Assert::AreEqual ((int) PlayerSlotState::Provisional, (int) world.slots[0].state);

            world.logs.firstInput = { pad.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], pad), L"the controller used first takes Player 1");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state);
            Assert::IsFalse  (world.slots[1].holder.has_value(), L"and the stick plays nothing until it is used");

            world.logs.firstInput.push_back (stick.unit);
            world.Step();

            Assert::IsTrue (Holds (world.slots[1], stick), L"once used, the stick is Player 2");
        }


        TEST_METHOD (SeveralAtLaunch_NoneIsAssignedUntilOneIsUsed)
        {
            World  world;

            world.devices = { MakePad ("{A}"), MakePad ("{B}") };
            world.Step();

            Assert::IsFalse (world.slots[0].holder.has_value(), L"which one the user will pick up cannot be known");
            Assert::IsFalse (world.slots[1].holder.has_value());
        }


        TEST_METHOD (PickedControllers_AreNeverCandidates)
        {
            World                 world;
            ControllerDeviceInfo  picked = MakePad ("{PICKED}");
            ControllerDeviceInfo  other  = MakePad ("{OTHER}");

            world.entries[0]      = MakePick (picked);
            world.devices         = { picked, other };
            world.logs.firstInput = { picked.unit, other.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], picked), L"a pick plays for its player");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state, L"in use as soon as it is attached");
            Assert::IsTrue   (Holds (world.slots[1], other), L"and Automatic chooses from the rest only");
        }


        TEST_METHOD (Pick_WaitsWhileItsControllerIsAbsent)
        {
            World                 world;
            ControllerDeviceInfo  picked = MakePad ("{PICKED}");

            world.entries[0] = MakePick (picked);
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], picked), L"a pick that is not attached stays the pick");
            Assert::AreEqual ((int) PlayerSlotState::Waiting, (int) world.slots[0].state);

            world.Attach (picked);
            world.Step();

            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state, L"and plays when it connects");
        }


        TEST_METHOD (ConnectionLog_FirstInputDoesNotReorderThePlayers)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");

            world.devices        = { first, second };
            world.logs.connected = { first.unit, second.unit };
            world.Step();

            world.logs.firstInput = { second.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], first), L"first input does not reorder players the connection order set");
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[0].state);
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[1].state);
        }


        //  A controller attached at launch still waits for its input: a lone
        //  one is Player 1 but not in use, and one more arriving is no
        //  connection order, so nothing changes until one is used.
        TEST_METHOD (LaunchController_WaitsForInputWhenOneMoreArrives)
        {
            World                 world;
            ControllerDeviceInfo  launch  = MakePad ("{LAUNCH}");
            ControllerDeviceInfo  arrival = MakePad ("{ARRIVAL}");



            world.devices = { launch };
            world.Step();

            world.Attach (arrival);
            world.logs.connected = { arrival.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], launch));
            Assert::AreEqual ((int) PlayerSlotState::Provisional, (int) world.slots[0].state, L"not in use until it gives input");
            Assert::IsFalse  (world.slots[1].holder.has_value(), L"and the arrival alone is not Player 2");
        }


        //
        //  Departures
        //

        TEST_METHOD (Departure_WhileTheOtherPlaysIsHeldForTheLeaver)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");

            world.devices         = { first, second };
            world.logs.firstInput = { first.unit, second.unit };
            world.Step();

            world.Detach (second);
            world.Step();

            Assert::AreEqual ((int) PlayerSlotState::Held, (int) world.slots[1].state,
                L"the leaver's slot is kept for it while the other plays (SC-012)");
            Assert::IsTrue   (Holds (world.slots[1], second));
            Assert::IsFalse  (PlayerSlotPolicy::IsOnePlaying (world.slots, world.entries),
                L"so the player who remains is not widened onto the leaver's paddles");

            world.Attach (second);
            world.Step();

            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[1].state, L"a return takes the slot back");
            Assert::IsTrue   (Holds (world.slots[1], second));
        }


        TEST_METHOD (Departure_OfTheLastPlayerStartsOver)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");
            ControllerDeviceInfo  third  = MakePad ("{C}");

            world.devices         = { first, second, third };
            world.logs.firstInput = { first.unit, second.unit };
            world.logs.connected  = { third.unit };
            world.Step();

            world.Detach (second);
            world.Step();
            world.Detach (first);
            world.Step();

            Assert::AreEqual ((int) PlayerSlotState::Provisional, (int) world.slots[0].state,
                L"with nobody left playing, Automatic starts over, and the one controller left is Player 1");
            Assert::IsTrue   (Holds (world.slots[0], third));
            Assert::AreEqual ((int) PlayerSlotState::Empty, (int) world.slots[1].state, L"and nothing is held any more");
            Assert::IsTrue   (world.logs.firstInput.empty(), L"both logs forget every controller without a slot");
            Assert::IsTrue   (world.logs.connected.empty());
        }


        TEST_METHOD (StartOver_NeverRewritesAPick)
        {
            World                 world;
            ControllerDeviceInfo  picked = MakePad ("{PICKED}", kStickProduct);
            ControllerDeviceInfo  pad    = MakePad ("{PAD}");

            world.entries[0]      = MakePick (picked);
            world.devices         = { pad };
            world.logs.firstInput = { pad.unit };
            world.Step();

            Assert::IsTrue (Holds (world.slots[1], pad), L"the pad plays for Player 2 while Player 1's pick is away");

            world.Detach (pad);
            world.Step();

            Assert::IsTrue   (Holds (world.slots[0], picked), L"the pick is kept through the start over");
            Assert::AreEqual ((int) PlayerSlotState::Waiting, (int) world.slots[0].state);
            Assert::IsFalse  (world.slots[1].holder.has_value());
        }


        TEST_METHOD (Disabled_LeavesPlayerOneAlone)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");

            world.entries[1].kind = PlayerEntryKind::Disabled;
            world.devices         = { first, second };
            world.logs.firstInput = { first.unit, second.unit };
            world.Step();

            Assert::IsTrue  (Holds (world.slots[0], first));
            Assert::IsFalse (world.slots[1].holder.has_value(), L"a disabled Player 2 takes no controller");
            Assert::IsTrue  (PlayerSlotPolicy::IsOnePlaying (world.slots, world.entries),
                L"so Player 1 drives the port alone, as a single controller always has");
        }


        TEST_METHOD (Keys_CountAsPlayingSoAControllerIsPlayerTwo)
        {
            World                 world;
            ControllerDeviceInfo  pad = MakePad ("{PAD}");

            world.entries[0].kind = PlayerEntryKind::ArrowKeys;
            world.devices         = { pad };
            world.Step();

            Assert::IsFalse (world.slots[1].holder.has_value(), L"no lone-controller shortcut for Player 2");

            world.logs.firstInput = { pad.unit };
            world.Step();

            Assert::IsTrue   (Holds (world.slots[1], pad), L"used, the controller is Player 2");
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick1, (int) world.slots[1].target, L"on joystick 1, beside the keys");
            Assert::IsFalse  (PlayerSlotPolicy::IsOnePlaying (world.slots, world.entries), L"the keys are in use from the moment they are picked");
        }


        TEST_METHOD (Pick_OfTheOtherPlayersControllerReturnsThemToAutomatic)
        {
            ControllerDeviceInfo  pad     = MakePad ("{PAD}");
            PlayerEntries         entries;

            entries[1] = MakePick (pad);
            entries    = PlayerSlotPolicy::ApplyPick (entries, 0, MakePick (pad));

            Assert::AreEqual ((int) PlayerEntryKind::Controller, (int) entries[0].kind, L"the pick takes effect (FR-041)");
            Assert::AreEqual ((int) PlayerEntryKind::Automatic,  (int) entries[1].kind, L"and the other player goes back to Automatic");
            Assert::IsFalse  (entries[1].unit.has_value());
        }


        //
        //  Modes
        //

        //  Two players both playing, in the given modes, on a machine with
        //  the given number of paddles.
        static World MakeTwoPlaying (PlayerMode first, PlayerMode second)
        {
            World                 world;
            ControllerDeviceInfo  a     = MakePad ("{A}");
            ControllerDeviceInfo  b     = MakePad ("{B}");

            world.entries[0].mode = first;
            world.entries[1].mode = second;
            world.devices         = { a, b };
            world.logs.firstInput = { a.unit, b.unit };
            world.Step();
            return world;
        }


        //  FR-039's table, through the routes the service plays: which
        //  paddles and lines each player's own bindings reach, for every pair
        //  of modes.
        TEST_METHOD (Modes_EveryPairDrivesWhatTheGamePortWiresIt)
        {
            constexpr size_t  kStickPaddles = 2;
            constexpr size_t  kLines        = PlayerTargetRules::kButtonCount;

            struct Row
            {
                PlayerMode             first;
                PlayerMode             second;
                std::optional<size_t>  firstPaddles[kStickPaddles];
                std::optional<size_t>  firstLines[kLines];
                std::optional<size_t>  secondPaddles[kStickPaddles];
                std::optional<size_t>  secondLines[kLines];
            };

            constexpr size_t  kFourAxes = 4;
            constexpr size_t  kPdl2     = 2;
            constexpr size_t  kPdl3     = 3;
            constexpr size_t  kPb2      = 2;
            const Row         kRows[]   =
            {
                { PlayerMode::Joystick,   PlayerMode::Joystick,   { 0, 1 },            { 0, 1, std::nullopt },            { kPdl2, kPdl3 },        { kPb2, std::nullopt, std::nullopt } },
                { PlayerMode::Joystick,   PlayerMode::Paddle,     { 0, 1 },            { 0, 1, std::nullopt },            { kPdl2, std::nullopt }, { kPb2, std::nullopt, std::nullopt } },
                { PlayerMode::Paddle,     PlayerMode::Joystick,   { 0, std::nullopt }, { 0, std::nullopt, std::nullopt }, { kPdl2, kPdl3 },        { kPb2, std::nullopt, std::nullopt } },
                { PlayerMode::Paddle,     PlayerMode::Paddle,     { 0, std::nullopt }, { 0, std::nullopt, std::nullopt }, { 1, std::nullopt },     { 1, std::nullopt, std::nullopt } },
                { PlayerMode::Joystick,   PlayerMode::TwoPaddles, { 0, 1 },            { 0, 1, std::nullopt },            { kPdl2, kPdl3 },        { kPb2, std::nullopt, std::nullopt } },
                { PlayerMode::TwoPaddles, PlayerMode::Joystick,   { 0, 1 },            { 0, 1, std::nullopt },            { kPdl2, kPdl3 },        { kPb2, std::nullopt, std::nullopt } },
                { PlayerMode::TwoPaddles, PlayerMode::Paddle,     { 0, 1 },            { 0, 1, std::nullopt },            { kPdl2, std::nullopt }, { kPb2, std::nullopt, std::nullopt } },
                { PlayerMode::Paddle,     PlayerMode::TwoPaddles, { 0, std::nullopt }, { 0, std::nullopt, std::nullopt }, { kPdl2, kPdl3 },        { kPb2, std::nullopt, std::nullopt } },
                { PlayerMode::TwoPaddles, PlayerMode::TwoPaddles, { 0, 1 },            { 0, 1, std::nullopt },            { kPdl2, kPdl3 },        { kPb2, std::nullopt, std::nullopt } },
            };
            size_t            checked   = 0;

            for (const Row & row : kRows)
            {
                World                                    world  = MakeTwoPlaying (row.first, row.second);
                std::optional<PlayerTargetRules::Route>  first  = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 0, kFourAxes);
                std::optional<PlayerTargetRules::Route>  second = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 1, kFourAxes);
                std::wstring                             what   = std::format (L"Player 1 {}, Player 2 {}",
                                                                               PlayerModeRules::GetModeLabel (row.first),
                                                                               PlayerModeRules::GetModeLabel (row.second));

                Assert::IsTrue (first.has_value() && second.has_value(), what.c_str());

                if (!first.has_value() || !second.has_value())
                {
                    continue;
                }

                for (size_t i = 0; i < kStickPaddles; i++)
                {
                    Assert::IsTrue (first->paddles[i]  == row.firstPaddles[i],  (what + L": Player 1's paddles").c_str());
                    Assert::IsTrue (second->paddles[i] == row.secondPaddles[i], (what + L": Player 2's paddles").c_str());
                }

                for (size_t i = 0; i < std::size (row.firstLines); i++)
                {
                    Assert::IsTrue (first->buttons[i]  == row.firstLines[i],  (what + L": Player 1's lines").c_str());
                    Assert::IsTrue (second->buttons[i] == row.secondLines[i], (what + L": Player 2's lines").c_str());
                }

                checked++;
            }

            Assert::AreEqual (std::size (kRows), checked, L"every pair of modes was checked");
        }


        //  The //c's two paddles: a second joystick drives nothing, and two
        //  players in Paddle mode each drive their own paddle.
        TEST_METHOD (Modes_OnTheTwoPaddleMachine)
        {
            constexpr size_t                         kTwoAxes = 2;
            World                                    sticks   = MakeTwoPlaying (PlayerMode::Joystick, PlayerMode::Joystick);
            World                                    paddles  = MakeTwoPlaying (PlayerMode::Paddle,   PlayerMode::Paddle);
            std::optional<PlayerTargetRules::Route>  second   = PlayerSlotPolicy::GetDriverRoute (paddles.slots, paddles.entries, 1, kTwoAxes);

            Assert::IsTrue  (PlayerSlotPolicy::GetDriverRoute (sticks.slots, sticks.entries, 0, kTwoAxes).has_value(), L"Player 1's joystick plays");
            Assert::IsFalse (PlayerSlotPolicy::GetDriverRoute (sticks.slots, sticks.entries, 1, kTwoAxes).has_value(), L"a second joystick drives nothing");
            Assert::IsTrue  (second.has_value() && second->paddles[0] == size_t (1), L"a second paddle drives PDL1");
        }


        //  The //c's two paddles in Two paddles mode: Player 1's pair is both
        //  of them, and a second player, whose place is past them, gets
        //  nothing.
        TEST_METHOD (Modes_TwoPaddlesOnTheTwoPaddleMachine)
        {
            constexpr size_t                         kTwoAxes = 2;
            World                                    pairs    = MakeTwoPlaying (PlayerMode::TwoPaddles, PlayerMode::Paddle);
            World                                    beside   = MakeTwoPlaying (PlayerMode::Paddle,     PlayerMode::TwoPaddles);
            std::optional<PlayerTargetRules::Route>  first    = PlayerSlotPolicy::GetDriverRoute (pairs.slots, pairs.entries, 0, kTwoAxes);

            Assert::IsTrue  (first.has_value() && first->paddles[0] == size_t (0) && first->paddles[1] == size_t (1), L"Player 1's two paddles play");
            Assert::IsTrue  (first.has_value() && first->buttons[0] == size_t (0) && first->buttons[1] == size_t (1), L"with PB0 and PB1");
            Assert::IsFalse (PlayerSlotPolicy::GetDriverRoute (pairs.slots,  pairs.entries,  1, kTwoAxes).has_value(), L"a paddle beside them drives nothing");
            Assert::IsFalse (PlayerSlotPolicy::GetDriverRoute (beside.slots, beside.entries, 1, kTwoAxes).has_value(), L"nor does a second pair");
        }


        //  One player playing alone drives what one controller of its mode
        //  always has, from either slot: a joystick PDL0, PDL1 and PB0-PB2, a
        //  paddle PDL0 and PB0.
        TEST_METHOD (Modes_ALonePlayerDrivesWhatOneControllerOfItsModeHas)
        {
            constexpr size_t          kFourAxes = 4;
            constexpr size_t          kPb2      = 2;
            World                     world;
            ControllerDeviceInfo      pad       = MakePad ("{PAD}");
            PlayerTargetRules::Route  route;

            world.devices = { pad };
            world.Step();

            route = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 0, kFourAxes).value();

            Assert::IsTrue (route == PlayerTargetRules::GetSingleRoute (kFourAxes), L"a lone joystick drives PDL0, PDL1 and PB0-PB2");

            world.entries[0].mode = PlayerMode::Paddle;
            world.Step();
            route = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 0, kFourAxes).value();

            Assert::IsTrue (route.paddles[0] == size_t (0) && !route.paddles[1].has_value(), L"a lone paddle drives PDL0");
            Assert::IsTrue (route.buttons[0] == size_t (0) && !route.buttons[1].has_value() && !route.buttons[kPb2].has_value(), L"and PB0");

            world.entries[0]      = PlayerEntry();
            world.entries[1]      = MakePick (pad);
            world.entries[1].mode = PlayerMode::Paddle;
            world.Step();
            route = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 1, kFourAxes).value();

            Assert::IsTrue (route.paddles[0] == size_t (0) && route.buttons[0] == size_t (0), L"Player 2 alone in Paddle mode drives PDL0 and PB0 too");

            world.entries[1].mode = PlayerMode::TwoPaddles;
            world.Step();
            route = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 1, kFourAxes).value();

            Assert::IsTrue (route.paddles[0] == size_t (0) && route.paddles[1] == size_t (1), L"Player 2 alone in Two paddles mode drives PDL0 and PDL1");
            Assert::IsTrue (route.buttons[0] == size_t (0) && route.buttons[1] == size_t (1) && !route.buttons[kPb2].has_value(), L"and PB0 and PB1");
        }


        //  A player in a Joyport jack drives no paddle, so the player beside
        //  it plays what its mode gives a player alone: joystick 0, or
        //  paddle 0. Both in jacks keep the joysticks their numbers give. On a
        //  machine without a Joyport the jacks play as joysticks.
        TEST_METHOD (Modes_BesideTheJoyportAPlayerPlaysAsThoughAlone)
        {
            World  world = MakeTwoPlaying (PlayerMode::JoyportLeft, PlayerMode::Paddle);



            world.slots = PlayerSlotPolicy::Evaluate (world.entries, world.devices, world.logs, world.slots, true);
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle0, (int) world.slots[1].target, L"Player 2's paddle is paddle 0");
            Assert::IsTrue   (world.slots[1].holder.has_value(),                              L"and is not refused for overlapping Player 1's joystick");

            world.entries[1].mode = PlayerMode::Joystick;
            world.slots = PlayerSlotPolicy::Evaluate (world.entries, world.devices, world.logs, world.slots, true);
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick0, (int) world.slots[1].target, L"Player 2's joystick is joystick 0");

            world.entries[1].mode = PlayerMode::SameAsPlayer1;
            world.slots = PlayerSlotPolicy::Evaluate (world.entries, world.devices, world.logs, world.slots, true);
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick0, (int) world.slots[0].target, L"both in jacks: joystick 0");
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick1, (int) world.slots[1].target, L"and joystick 1");

            world.entries[1].mode = PlayerMode::Paddle;
            world.slots = PlayerSlotPolicy::Evaluate (world.entries, world.devices, world.logs, world.slots, false);
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle2, (int) world.slots[1].target, L"no Joyport: Player 1 plays joystick 0, and a paddle beside it is paddle 2");

            world.entries[1].mode = PlayerMode::TwoPaddles;
            world.slots = PlayerSlotPolicy::Evaluate (world.entries, world.devices, world.logs, world.slots, true);
            Assert::AreEqual ((int) PlayerAxisTarget::Paddles01, (int) world.slots[1].target, L"two paddles beside the Joyport are paddles 0 and 1");
            Assert::IsTrue   (PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 1, GamePortContribution::kAxisCount, true)->buttons == PlayerTargetRules::ButtonRoute(),
                              L"with no button line");
        }


        //  The Joyport owns all three button lines: a player beside it
        //  drives its paddles and no button, and a player in a jack drives no
        //  paddle input either, its switches reaching the jack.
        TEST_METHOD (DriverRoute_BesideTheJoyportReachesNoButton)
        {
            constexpr size_t                         kFourAxes = 4;
            World                                    world     = MakeTwoPlaying (PlayerMode::JoyportLeft, PlayerMode::Paddle);
            std::optional<PlayerTargetRules::Route>  route;



            world.slots = PlayerSlotPolicy::Evaluate (world.entries, world.devices, world.logs, world.slots, true);

            route = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 1, kFourAxes, true);
            Assert::IsTrue   (route.has_value());
            Assert::IsTrue   (route->paddles[0] == std::optional<size_t> (0), L"its paddle on PDL0");
            Assert::IsTrue   (route->buttons == PlayerTargetRules::ButtonRoute(), L"and no button line");
            Assert::IsTrue   (PlayerSlotPolicy::GetPlayerRoute (world.slots, world.entries, 1, kFourAxes, true).buttons[0].has_value(),
                              L"the settings still show the line it would have");

            route = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 0, kFourAxes, true);
            Assert::IsTrue   (route.has_value(), L"the player in the jack is read, for its switches");

            route = PlayerSlotPolicy::GetDriverRoute (world.slots, world.entries, 1, kFourAxes, false);
            Assert::IsTrue   (route->buttons[0].has_value(), L"without a Joyport the buttons reach the machine");
        }


        //  A pick keeps the player's mode; the keys are a joystick and the
        //  mouse a paddle, and a mode that cannot have them sends Player 1
        //  back to Automatic.
        TEST_METHOD (Modes_PicksKeepThemAndTheKeysAndMouseSetTheirOwn)
        {
            ControllerDeviceInfo  pad = MakePad ("{PAD}");
            PlayerEntries         entries;
            PlayerEntry           keys;
            PlayerEntry           mouse;

            keys.kind  = PlayerEntryKind::ArrowKeys;
            mouse.kind = PlayerEntryKind::MousePaddle;

            entries = PlayerSlotPolicy::ApplyMode (entries, 1, PlayerMode::Paddle);
            entries = PlayerSlotPolicy::ApplyPick (entries, 1, MakePick (pad));

            Assert::IsTrue (entries[1].mode == PlayerMode::Paddle, L"a pick keeps the player's mode");

            entries = PlayerSlotPolicy::ApplyPick (entries, 0, mouse);
            Assert::IsTrue (entries[0].mode == PlayerMode::Paddle, L"the mouse plays in Paddle mode");

            entries = PlayerSlotPolicy::ApplyPick (entries, 0, keys);
            Assert::IsTrue (entries[0].mode == PlayerMode::Joystick, L"the keys play in Joystick mode");

            entries = PlayerSlotPolicy::ApplyMode (entries, 0, PlayerMode::Paddle);
            Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) entries[0].kind, L"Paddle mode sends the keys back to Automatic");
            Assert::IsTrue   (entries[0].mode == PlayerMode::Paddle);

            entries = PlayerSlotPolicy::ApplyPick (entries, 0, mouse);
            entries = PlayerSlotPolicy::ApplyMode (entries, 0, PlayerMode::Joystick);
            Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) entries[0].kind, L"and Joystick mode the mouse");

            entries = PlayerSlotPolicy::ApplyMode (entries, 1, PlayerMode::Joystick);
            Assert::AreEqual ((int) PlayerEntryKind::Controller, (int) entries[1].kind, L"a controller has either mode");

            entries = PlayerSlotPolicy::ApplyMode (entries, 0, PlayerMode::TwoPaddles);
            entries = PlayerSlotPolicy::ApplyPick (entries, 0, mouse);
            Assert::IsTrue (entries[0].mode == PlayerMode::TwoPaddles, L"the mouse keeps Two paddles mode, its X and Y the two paddles");

            entries = PlayerSlotPolicy::ApplyMode (entries, 0, PlayerMode::Paddle);
            Assert::AreEqual ((int) PlayerEntryKind::MousePaddle, (int) entries[0].kind, L"and moves between the paddle modes");

            entries = PlayerSlotPolicy::ApplyPick (entries, 0, keys);
            entries = PlayerSlotPolicy::ApplyMode (entries, 0, PlayerMode::TwoPaddles);
            Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) entries[0].kind, L"Two paddles mode sends the keys back to Automatic");
        }

        //
        //  What counts as a controller being used
        //

        TEST_METHOD (RealInput_ButtonsDpadAndTriggerPastItsThreshold)
        {
            ControllerSample  sample;

            sample.connected = true;
            Assert::IsFalse (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"a controller at rest is not in use");

            sample.buttons.set (3);
            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"any button");

            sample.buttons.reset();
            sample.hats[0] = ControllerSample::kHatLeft;
            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"a D-pad direction");

            sample.hats[0]     = 0;
            sample.triggers[1] = ButtonBinding::kTriggerThreshold;
            Assert::IsFalse (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"a trigger resting on its threshold");

            sample.triggers[1] = ButtonBinding::kTriggerThreshold + kNudge;
            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"a trigger past it");
        }


        TEST_METHOD (RealInput_AnAxisOutsideItsDeadZone)
        {
            ControllerSample  sample;

            sample.connected = true;
            sample.axes[1]   = -(kDeadzone - kNudge);
            Assert::IsFalse (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"an axis just inside its dead zone is at rest");

            sample.axes[1] = -(kDeadzone + kNudge);
            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"and just outside it is in use");
        }


        //  A worn stick resting off center is judged after its calibration,
        //  so its resting offset does not claim a slot.
        TEST_METHOD (RealInput_IsJudgedAfterCalibration)
        {
            static constexpr float  kRestOffset = 0.2f;
            static constexpr float  kNearRest   = 0.25f;

            ControllerSample       sample;
            ControllerCalibration  calibration;

            calibration.mode = CalibrationMode::User;
            calibration.axes.fill ({ 0.0f, -1.0f, 1.0f });
            calibration.axes[0] = { kRestOffset, -1.0f, 1.0f };

            sample.connected = true;
            sample.axes[0]   = kNearRest;

            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"raw, the offset reads as a push");
            Assert::IsFalse (PlayerSlotPolicy::IsRealInput (sample, &calibration, kDeadzone), L"calibrated, it is near rest");
        }


        //  A stick or trigger left off center when the controller connected
        //  is judged from where it rested, so it does not claim a slot until
        //  it moves.
        TEST_METHOD (RealInput_IsJudgedFromWhereItRestedAtConnect)
        {
            static constexpr float  kRestingStick   = 0.6f;
            static constexpr float  kRestingTrigger = 0.5f;

            ControllerSample  rest;
            ControllerSample  sample;

            rest.connected   = true;
            rest.axes[3]     = kRestingStick;
            rest.triggers[0] = kRestingTrigger;
            sample           = rest;

            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"judged from center, the resting stick reads as a push");
            Assert::IsFalse (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone, &rest), L"judged from its rest, it is not in use");

            sample.axes[3] = kRestingStick - kDeadzone - kNudge;
            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone, &rest), L"a move of more than the dead zone is");

            sample.axes[3]     = kRestingStick;
            sample.triggers[0] = kRestingTrigger + ButtonBinding::kTriggerThreshold + kNudge;
            Assert::IsTrue  (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone, &rest), L"and so is a trigger pulled past its threshold from rest");
        }


        //
        //  Queries
        //

        TEST_METHOD (IsOnePlaying_FalseWhileTheOtherSlotIsHeld)
        {
            PlayerEntries  entries;
            PlayerSlots    slots;

            slots[0].state  = PlayerSlotState::Playing;
            slots[0].holder = MakePad ("{A}").unit;
            slots[1].state  = PlayerSlotState::Held;
            slots[1].holder = MakePad ("{B}").unit;

            Assert::IsFalse (PlayerSlotPolicy::IsOnePlaying (slots, entries));

            slots[1] = PlayerSlot();

            Assert::IsTrue (PlayerSlotPolicy::IsOnePlaying (slots, entries));
        }


        TEST_METHOD (NeedsIdleWatch_WhileAnAutomaticPlayerWaits)
        {
            PlayerEntries  entries;
            PlayerSlots    slots;

            slots[0].state  = PlayerSlotState::Playing;
            slots[0].holder = MakePad ("{A}").unit;

            Assert::IsTrue  (PlayerSlotPolicy::NeedsIdleWatch (entries, slots), L"Player 2 on Automatic has no controller yet");

            entries[1].kind = PlayerEntryKind::Disabled;
            Assert::IsFalse (PlayerSlotPolicy::NeedsIdleWatch (entries, slots), L"nobody is waiting");

            slots[0].state = PlayerSlotState::Provisional;
            Assert::IsTrue  (PlayerSlotPolicy::NeedsIdleWatch (entries, slots), L"a provisional Player 1 may yet give way");

            slots[0].state = PlayerSlotState::Held;
            Assert::IsFalse (PlayerSlotPolicy::NeedsIdleWatch (entries, slots), L"a held slot waits for its own controller");
        }



        //
        //  The changed-holder notice
        //

        static ControllerDeviceInfo MakeDescribedPad (const char * unitId, const wchar_t * description)
        {
            ControllerDeviceInfo  info = MakePad (unitId);

            info.description = description;
            return info;
        }


        // An Xbox-class controller by product, with ":2" and on for a second
        // one of the same product.
        static ControllerDeviceInfo MakeXboxPad (const char * unitId, const wchar_t * description)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::XInput, 0, 0 };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::XInputProduct;
            info.description = description;
            return info;
        }


        TEST_METHOD (Notice_AutomaticGivingTheLastHolderShowsNone)
        {
            World                      world;
            ControllerDeviceInfo       pad = MakeDescribedPad ("{PAD}", L"Blue pad");
            PlayerLastHolders          lastHolders;
            std::vector<std::wstring>  notices;

            lastHolders[0] = pad.unit;
            world.Attach (pad);
            world.Step();

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            Assert::IsTrue (Holds (world.slots[0], pad), L"the lone pad is Player 1");
            Assert::IsTrue (notices.empty(), L"the same controller in the same slot as last time shows nothing");
            Assert::IsTrue (lastHolders[0] == pad.unit);
        }


        TEST_METHOD (Notice_ADifferentUnitShowsThePlayerAndItsDescription)
        {
            World                      world;
            ControllerDeviceInfo       first  = MakeDescribedPad ("{A}", L"Blue pad");
            ControllerDeviceInfo       second = MakeDescribedPad ("{B}", L"Red pad");
            PlayerLastHolders          lastHolders;
            std::vector<std::wstring>  notices;

            lastHolders[0]       = second.unit;
            world.devices        = { first, second };
            world.logs.connected = { first.unit, second.unit };
            world.Step();

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            Assert::AreEqual ((size_t) 2, notices.size(), L"both slots changed holder");
            Assert::AreEqual (std::wstring (L"Player 1: Blue pad"), notices[0]);
            Assert::AreEqual (std::wstring (L"Player 2: Red pad"),  notices[1]);
            Assert::IsTrue   (lastHolders[0] == first.unit,  L"each slot's holder is its last holder from now on");
            Assert::IsTrue   (lastHolders[1] == second.unit);

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);
            Assert::IsTrue   (notices.empty(), L"recording the same slots again shows nothing");
        }


        //  For a player in the Joyport a notice gives the jacks after the
        //  player: both for a controller playing alone, and its own jack once
        //  two play.
        TEST_METHOD (Notice_InTheJoyportGivesTheJacks)
        {
            World                      world;
            ControllerDeviceInfo       first  = MakeDescribedPad ("{A}", L"Blue pad");
            ControllerDeviceInfo       second = MakeDescribedPad ("{B}", L"Red pad");
            PlayerLastHolders          lastHolders;
            PlayerLastHolders          offHolders;
            std::vector<std::wstring>  notices;



            world.entries[0].mode = PlayerMode::JoyportLeft;
            world.entries[1].mode = PlayerMode::SameAsPlayer1;
            world.Attach (first);
            world.Step();

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders, world.entries, true);
            Assert::AreEqual ((size_t) 1, notices.size());
            Assert::AreEqual (std::wstring (L"Player 1 (Joyport left and right): Blue pad"), notices[0], L"the lone pad drives both jacks");

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, offHolders, world.entries, false);
            Assert::AreEqual (std::wstring (L"Player 1: Blue pad"), notices[0], L"and without the Joyport, the player alone");

            world.devices        = { first, second };
            world.logs.connected = { first.unit, second.unit };
            world.Step();
            lastHolders          = PlayerLastHolders();
            world.slots[0].state = PlayerSlotState::Playing;
            world.slots[1].state = PlayerSlotState::Playing;

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders, world.entries, true);
            Assert::AreEqual ((size_t) 2, notices.size());
            Assert::AreEqual (std::wstring (L"Player 1 (Joyport left): Blue pad"), notices[0]);
            Assert::AreEqual (std::wstring (L"Player 2 (Joyport right): Red pad"), notices[1]);
        }


        TEST_METHOD (Notice_APickShowsNoneButUpdatesTheLastHolder)
        {
            World                      world;
            ControllerDeviceInfo       pad   = MakeDescribedPad ("{PAD}", L"Blue pad");
            ControllerDeviceInfo       other = MakeDescribedPad ("{OTHER}", L"Red pad");
            PlayerLastHolders          lastHolders;
            std::vector<std::wstring>  notices;

            lastHolders[0]   = other.unit;
            world.entries[0] = MakePick (pad);
            world.Attach (pad);
            world.Step();

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            Assert::IsTrue (notices.empty(), L"a pick is what the user chose, so it is not announced");
            Assert::IsTrue (lastHolders[0] == pad.unit, L"but it is the last holder from now on");
        }


        TEST_METHOD (Notice_TheProvisionalPlayerOneCountsAsAHolder)
        {
            World                      world;
            ControllerDeviceInfo       pad = MakeDescribedPad ("{PAD}", L"Blue pad");
            PlayerLastHolders          lastHolders;
            std::vector<std::wstring>  notices;

            world.Attach (pad);
            world.Step();

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            Assert::AreEqual ((int) PlayerSlotState::Provisional, (int) world.slots[0].state, L"not used yet");
            Assert::AreEqual ((size_t) 1, notices.size(), L"no last holder is a changed holder");
            Assert::AreEqual (std::wstring (L"Player 1: Blue pad"), notices[0]);
            Assert::IsTrue   (lastHolders[0] == pad.unit);
            Assert::IsFalse  (lastHolders[1].has_value(), L"Player 2 held nothing");
        }


        TEST_METHOD (Notice_AHolderLeavingKeepsTheSavedLastHolder)
        {
            World                      world;
            ControllerDeviceInfo       pad = MakeDescribedPad ("{PAD}", L"Blue pad");
            PlayerLastHolders          lastHolders;
            std::vector<std::wstring>  notices;

            world.Attach (pad);
            world.logs.firstInput = { pad.unit };
            world.Step();
            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            world.Detach (pad);
            world.Step();
            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            Assert::IsTrue (notices.empty());
            Assert::IsTrue (lastHolders[0] == pad.unit, L"the pad that left is still the last to hold Player 1");

            world.Attach (pad);
            world.Step();
            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            Assert::IsTrue (Holds (world.slots[0], pad), L"it plays again");
            Assert::IsTrue (notices.empty(), L"and coming back to the slot it last held shows nothing");
        }


        TEST_METHOD (Notice_TwoPadsOfOneProductAreToldApartByTheirOrdinal)
        {
            World                      world;
            ControllerDeviceInfo       first  = MakeXboxPad ("045e:0b13",   L"Xbox Wireless Controller");
            ControllerDeviceInfo       second = MakeXboxPad ("045e:0b13:2", L"Xbox Wireless Controller");
            PlayerLastHolders          lastHolders;
            std::vector<std::wstring>  notices;

            lastHolders[0] = first.unit;
            world.Attach (second);
            world.Step();

            notices = PlayerSlotPolicy::RecordHolders (world.slots, world.devices, lastHolders);

            Assert::AreEqual ((size_t) 1, notices.size(), L"the second pad of the product is a different controller");
            Assert::IsTrue   (lastHolders[0] == second.unit);
        }
    };
}
