#include "Pch.h"

#include "Controllers/PlayerSlotPolicy.h"

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
            Assert::AreEqual ((int) PlayerSlotState::Waiting, (int) world.slots[0].state, L"neither is in use until it gives input");
            Assert::AreEqual ((int) PlayerSlotState::Waiting, (int) world.slots[1].state);
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


        TEST_METHOD (Waiting_BecomesPlayingOnItsFirstInput)
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
            Assert::AreEqual ((int) PlayerSlotState::Waiting, (int) world.slots[0].state);
            Assert::AreEqual ((int) PlayerSlotState::Playing, (int) world.slots[1].state);
            Assert::IsTrue   (PlayerSlotPolicy::IsOnePlaying (world.slots, world.entries), L"one in use drives everything");
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


        TEST_METHOD (Target_UserSetOverlapIsRefused)
        {
            World                 world;
            ControllerDeviceInfo  first  = MakePad ("{A}");
            ControllerDeviceInfo  second = MakePad ("{B}");

            world.entries[1].target = PlayerAxisTarget::Paddle1;
            world.devices           = { first, second };
            world.logs.firstInput   = { first.unit, second.unit };
            world.Step();

            Assert::IsFalse (world.slots[1].holder.has_value(),
                L"a slot claiming a paddle of the other player's joystick is refused rather than played (FR-036)");
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


        TEST_METHOD (RealInput_AnAxisOutsideItsDeadzone)
        {
            ControllerSample  sample;

            sample.connected = true;
            sample.axes[1]   = -(kDeadzone - kNudge);
            Assert::IsFalse (PlayerSlotPolicy::IsRealInput (sample, nullptr, kDeadzone), L"an axis just inside its deadzone is at rest");

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


        TEST_METHOD (DescribeAssignment_SaysWhichPlayer)
        {
            Assert::AreEqual (std::wstring (L"Player 1: Pad"), PlayerSlotPolicy::DescribeAssignment (0, L"Pad"));
            Assert::AreEqual (std::wstring (L"Player 2: Pad"), PlayerSlotPolicy::DescribeAssignment (1, L"Pad"));
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
