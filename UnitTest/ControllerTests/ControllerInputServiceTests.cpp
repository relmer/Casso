#include "Pch.h"

#include "Controllers/ControllerInputService.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/XInputSampleDecoder.h"
#include "FakeControllerBackend.h"
#include "RecordingGamePortSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerInputServiceTests
//
//  The service is what makes a plugged-in controller play a game, so these
//  drive it exactly as the controller thread does: a scripted backend on one
//  side, the real mixer and a recording sink on the other.
//
//  THE RULE THAT MATTERS MOST IS THAT A FAILED READ IS A DISCONNECT. A
//  controller that has been unplugged must not be reported as one resting at
//  center with its buttons up, because a game cannot tell that apart from a
//  controller being held still, and the paddles would sit wherever the last
//  read left them.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControllerInputServiceTests)
    {
    public:

        static constexpr Byte    kCenter      = 127;
        static constexpr Byte    kFullHigh    = 255;
        static constexpr Byte    kFullLow     = 0;
        static constexpr size_t  kPdl2        = 2;
        static constexpr size_t  kPdl3        = 3;
        static constexpr size_t  kPb2         = 2;
        static constexpr double  kMsPerSecond = 1000.0;
        static constexpr double  kIdleSeconds = ControllerInputService::kIdleWatchPeriodMs / kMsPerSecond;


        static ControllerDeviceInfo MakeXboxDevice()
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.description     = L"Xbox Controller";
            info.xinputSlot      = 0;
            info.controls        = XInputSampleDecoder::ListControls();
            return info;
        }


        static ControllerDeviceInfo MakeStickDevice()
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            info.unit.unitId = "{01661270-ADF7-11F1-8005-444553540000}";
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"VKBsim Gladiator";
            info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                                 { ControlKind::Button, 0 }, { ControlKind::Button, 1 } };
            return info;
        }



        static ControllerDeviceInfo MakePadDevice (const char * unitId, const wchar_t * description)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x0079, 0x0006 };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = description;
            info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                                 { ControlKind::Button, 0 }, { ControlKind::Button, 1 } };
            return info;
        }


        // A full-range user calibration for each unit, so the first reading is
        // taken as it arrives. These tests push a stick hard over on that
        // first reading, which automatic calibration would take as the rest
        // position -- right for a real stick, and beside the point here.
        static void SkipCalibration (ControllerInputService & service, std::initializer_list<ControllerUnitKey> units)
        {
            std::map<std::string, ControllerCalibration>  calibrations;
            ControllerCalibration                         fullRange;

            fullRange.mode = CalibrationMode::User;
            fullRange.axes.fill ({ 0.0f, -1.0f, 1.0f });

            for (const ControllerUnitKey & unit : units)
            {
                calibrations[ControllerTokens::UnitToToken (unit)] = fullRange;
            }

            service.SetCalibrations (calibrations);
        }


        static ControllerSample MakePushedSample()
        {
            ControllerSample  sample;

            sample.connected = true;
            sample.axes[XInputSampleDecoder::kLeftStickX] = 1.0f;
            sample.buttons.set (0);
            return sample;
        }


        static ControllerSample MakeRestSample()
        {
            ControllerSample  sample;

            sample.connected = true;
            return sample;
        }


        static PlayerEntry MakePick (const ControllerUnitKey & unit, PlayerMode mode = PlayerMode::Joystick)
        {
            return { PlayerEntryKind::Controller, unit, mode };
        }


        static void Pick (ControllerInputService & service, const ControllerUnitKey & unit)
        {
            service.PickPlayerEntry (0, MakePick (unit));
        }


        // Player 1 into the Joyport's left jack on a machine that has one, or
        // back to Joystick mode, with Player 2 on Same as Player 1.
        static void SetPlayerOneInJoyport (ControllerInputService & service, bool isInJoyport)
        {
            service.SetJoyportAvailable (true);
            service.SetPlayerMode (0, isInJoyport ? PlayerMode::JoyportLeft : PlayerMode::Joystick);
            service.SetPlayerMode (1, PlayerMode::SameAsPlayer1);
        }


        // Whether the last write closed one switch of the left jack.
        static bool IsLeftSwitchClosed (const RecordingGamePortSink & sink, JoystickSwitch which)
        {
            return sink.writes.back().state.jacks.jack[JoyportJacks::kLeftJack].test (static_cast<size_t> (which));
        }


        // Two players, each picked, both in Paddle mode: the first on PDL0
        // and the second on PDL1.
        static PlayerEntries MakeTwoPlayers (const ControllerUnitKey &  first,
                                             const ControllerUnitKey &  second)
        {
            return { MakePick (first, PlayerMode::Paddle), MakePick (second, PlayerMode::Paddle) };
        }


        // Each device's Paddle choice: a user Paddle profile, Knob, that reads
        // axis 0 as it stands and fires with button 0, so a push reads at
        // once rather than at the built-in Paddles profile's rate.
        static void UsePaddleKnobs (ControllerInputService & service, std::initializer_list<ControllerDeviceInfo> devices)
        {
            std::map<std::string, ControllerModelSettings>  models;
            std::map<std::string, std::string>              choices;
            ControlMapping                                  knob;

            knob.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 0 } });
            knob.pb0.push_back  ({ { ControlKind::Button, 0 } });

            for (const ControllerDeviceInfo & device : devices)
            {
                ControllerModelSettings &  settings = models[ControllerTokens::ModelToToken (device.unit.model)];

                settings.EnsureBuiltInProfiles (device.unit.model, device.formFactor, device.controls);
                settings.AddProfile ("Knob", knob, ProfileMode::Paddle);
                choices[ControllerTokens::UnitToToken (device.unit)] = "Knob";
            }

            service.SetModelSettings  (models);
            service.SetActiveProfiles (ProfileMode::Paddle, choices);
        }


        // Each player pushes their stick hard over and holds their first
        // button. The Xbox controller's Y is pushed too, so a merge that let
        // player one reach PDL1 would show.
        static void SetUpTwoPlayers (FakeControllerBackend  & backend,
                                     ControllerInputService & service,
                                     GamePortInputMixer     & mixer,
                                     ControllerDeviceInfo   & xbox,
                                     ControllerDeviceInfo   & stick)
        {
            ControllerSample  xboxSample  = MakePushedSample();
            ControllerSample  stickSample = MakeRestSample();

            xbox  = MakeXboxDevice();
            stick = MakeStickDevice();

            xboxSample.axes[XInputSampleDecoder::kLeftStickY] = 1.0f;

            stickSample.axes[0] = -1.0f;
            stickSample.buttons.set (0);

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  xboxSample);
            backend.SetSample (stick.unit, stickSample);
            SkipCalibration (service, { stick.unit });
            UsePaddleKnobs  (service, { xbox, stick });

            service.SetPlayerEntries (MakeTwoPlayers (xbox.unit, stick.unit));
            service.Tick();
        }


        // Player 1 into the left jack and Player 2 into the right, on a
        // machine that has a Joyport.
        static void SetBothInJoyport (ControllerInputService & service)
        {
            service.SetJoyportAvailable (true);
            service.SetPlayerMode (0, PlayerMode::JoyportLeft);
            service.SetPlayerMode (1, PlayerMode::JoyportRight);
        }


        // The stick joining and giving input after the Xbox controller: two
        // Automatic players on joystick 0 and joystick 1.
        static void SetUpTwoJoysticks (FakeControllerBackend  & backend,
                                       ControllerInputService & service,
                                       GamePortInputMixer     & mixer,
                                       RecordingGamePortSink  & sink,
                                       ControllerDeviceInfo   & xbox,
                                       ControllerDeviceInfo   & stick)
        {
            xbox  = MakeXboxDevice();
            stick = MakeStickDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, MakePushedSample());
            SkipCalibration (service, { stick.unit });
            service.Tick();

            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakeRestSample());
            service.OnDevicesChanged();
            service.Tick();
        }


        //
        //  Two players
        //

        TEST_METHOD (TwoPlayers_EachDrivesItsOwnPaddleAndButtonLineAtOnce)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;

            mixer.SetSink (&sink);
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"PDL0 follows player one");
            Assert::AreEqual (kFullLow,  sink.writes.back().state.paddle[1],
                L"PDL1 follows player two, played through the same default mapping, and not player one's Y");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"player one's button reaches PB0, paddle 0's line");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (1), L"and player two's reaches PB1, paddle 1's, at the same time");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (kPb2), L"PB2 is not wired to either paddle");
        }


        TEST_METHOD (TwoPlayers_OnlyAPlayersFirstButtonReachesAPaddlesLine)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox      = MakeXboxDevice();
            ControllerDeviceInfo    stick     = MakeStickDevice();
            ControllerSample        xboxOnly  = MakeRestSample();
            ControllerSample        stickOnly = MakeRestSample();

            // Each player presses ONLY the control their profile binds to PB1.
            // A single paddle has one line, taken from the first button, so
            // nothing may reach the game port.
            xboxOnly.buttons.set (1);
            stickOnly.buttons.set (1);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  xboxOnly);
            backend.SetSample (stick.unit, stickOnly);
            SkipCalibration (service, { stick.unit });

            service.SetPlayerEntries (MakeTwoPlayers (xbox.unit, stick.unit));
            service.Tick();

            Assert::IsFalse (mixer.GetTargetState().buttons.test (0),
                L"player one's second button has no line on a paddle");
            Assert::IsFalse (mixer.GetTargetState().buttons.test (1),
                L"and player two's PB1 binding is kept and ignored; only their first button drives PB1");
        }


        //  US7 #1-#3: two joysticks, each on its own paddles, and the buttons
        //  as the hardware wires them.
        TEST_METHOD (TwoJoysticks_EachPlaysItsOwnPaddlesAndItsJoysticksLines)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            ControllerSample        stickSample = MakeRestSample();
            ControllerSample        xboxSample  = MakePushedSample();

            SetUpTwoJoysticks (backend, service, mixer, sink, xbox, stick);

            stickSample.axes[0] = -1.0f;
            stickSample.buttons.set (0);
            stickSample.buttons.set (1);
            backend.SetSample (stick.unit, stickSample);
            service.Tick();

            Assert::IsTrue   (service.GetPlayerSlots()[1].holder.value() == stick.unit, L"the stick, used second, is Player 2");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0],     L"player one's stick on PDL0");
            Assert::AreEqual (kFullLow,  sink.writes.back().state.paddle[kPdl2], L"player two's on PDL2, both at once");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),    L"player one's first button is PB0");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (kPb2), L"player two's first button is PB2 (FR-039)");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (1),
                L"player two's second button has no line on joystick 1, and player one's is up");

            xboxSample.buttons.set (1);
            backend.SetSample (xbox.unit, xboxSample);
            service.Tick();

            Assert::IsTrue (sink.writes.back().state.buttons.test (1), L"player one's second button is PB1");
        }


        //  Player 2 moving from Paddle mode to Joystick mode leaves paddle 1
        //  for joystick 1, playing its Joystick profile there.
        TEST_METHOD (TwoPlayers_MovingAPlayerFreesThePaddleTheyLeft)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;

            mixer.SetSink (&sink);
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);

            service.SetPlayerMode (1, PlayerMode::Joystick);
            service.Tick();

            Assert::AreEqual (kCenter,   sink.writes.back().state.paddle[1],     L"the paddle player two left centers");
            Assert::AreEqual (kFullLow,  sink.writes.back().state.paddle[kPdl2], L"and they play joystick 1, which they moved to");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (kPb2),      L"with its line, PB2");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (1),         L"and paddle 1's line released");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0],     L"player one is untouched by it");
        }


        //  SC-012, US7 #6: a leaver's slot is held, so the remaining player is
        //  not widened onto the leaver's paddles or lines.
        TEST_METHOD (TwoPlayers_OneDisconnectingReleasesOnlyItsOwn)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            size_t                  before = 0;
            size_t                  i      = 0;

            mixer.SetSink (&sink);
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);
            before = sink.writes.size();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue (sink.writes.size() > before, L"the departure must be written");

            for (i = before; i < sink.writes.size(); i++)
            {
                Assert::AreEqual (kFullHigh, sink.writes[i].state.paddle[0],
                    L"player one's paddle is never interrupted by player two leaving (SC-012)");
                Assert::IsTrue (sink.writes[i].state.buttons.test (0), L"nor is their button line");
            }

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[1],
                L"player two's paddle centers within the tick, and player one does not move onto it");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (1), L"player two's button line is released");
            Assert::AreEqual ((int) PlayerSlotState::Held, (int) service.GetPlayerSlots()[1].state, L"the slot is kept for the leaver");
        }


        //  The same with both players on Automatic: the remaining player keeps
        //  only joystick 0, and the returning one takes joystick 1 back.
        TEST_METHOD (TwoJoysticks_ALeaverIsHeldAndTakesItsSlotBack)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            ControllerSample        stickSample = MakeRestSample();

            SetUpTwoJoysticks (backend, service, mixer, sink, xbox, stick);

            stickSample.axes[0] = -1.0f;
            backend.SetSample (stick.unit, stickSample);
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0],     L"player one plays on");
            Assert::AreEqual (kCenter,   sink.writes.back().state.paddle[kPdl2], L"player two's paddle centers");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (kPb2),      L"and PB2 is not handed to player one");

            backend.AddDevice (stick);
            backend.SetSample (stick.unit, stickSample);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue   (service.GetPlayerSlots()[1].holder.value() == stick.unit, L"the stick takes its slot back");
            Assert::AreEqual (kFullLow, sink.writes.back().state.paddle[kPdl2],          L"and plays joystick 1 again");
        }


        TEST_METHOD (TwoPlayers_AControllerInNeitherSlotPlaysNothing)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            ControllerDeviceInfo    spare = MakePadDevice ("{CCCC}", L"Spare Pad");

            mixer.SetSink (&sink);
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);

            backend.AddDevice (spare);
            backend.SetSample (spare.unit, MakePushedSample());
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"player one is unaffected");
            Assert::AreEqual (kFullLow,  sink.writes.back().state.paddle[1], L"and so is player two");
            Assert::AreEqual (kCenter,   sink.writes.back().state.paddle[kPdl2],
                L"a controller no player holds drives no paddle of its own");
        }


        //  US7 #7: Player 2 set to Disabled leaves Player 1 driving the game
        //  port alone, as a single controller always has.
        TEST_METHOD (PlayerTwoDisabled_PlayerOneDrivesAlone)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            PlayerEntries           entries;

            mixer.SetSink (&sink);
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);

            entries         = service.GetPlayerEntries();
            entries[1]      = PlayerEntry();
            entries[1].kind = PlayerEntryKind::Disabled;
            service.SetPlayerEntries (entries);
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"player one, alone in Paddle mode, drives PDL0");
            Assert::AreEqual (kCenter,   sink.writes.back().state.paddle[1], L"and not PDL1, which the stick released");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),     L"and PB0");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (1),     L"the stick drives nothing, so nothing holds PB1");

            service.SetPlayerMode (0, PlayerMode::Joystick);
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0],
                L"in Joystick mode player one drives PDL0 and PDL1 on its own");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[1],
                L"including the Y that player two was holding a moment ago");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"and it drives PB0-PB2 as it always has");
        }


        //  A player playing alone drives everything a single controller has,
        //  whichever slot it holds.
        TEST_METHOD (OnePlaying_FromPlayerTwoDrivesPdl0Pdl1AndTheButtons)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox   = MakeXboxDevice();
            ControllerUnitKey       absent = MakeStickDevice().unit;
            ControllerSample        sample = MakePushedSample();

            sample.buttons.set (1);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, sample);

            // Player 1 is a pick that is not attached, so the Xbox controller
            // plays as Player 2 once it is used.
            service.PickPlayerEntry (0, MakePick (absent));
            service.Tick();

            Assert::IsTrue   (service.GetPlayerSlots()[1].holder.value() == xbox.unit, L"the controller used is Player 2");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"and alone it drives PDL0, not joystick 1's PDL2");
            Assert::AreEqual (kCenter,   sink.writes.back().state.paddle[kPdl2]);
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"its first button is PB0");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (1), L"and its second PB1");
        }


        //  SC-014: a second person joining never takes PDL0, PDL1, PB0 or PB1
        //  from the first.
        TEST_METHOD (SecondJoining_NeverChangesWhatTheFirstPlays)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            ControllerSample        stickSample = MakeRestSample();
            GamePortState           first;
            size_t                  before      = 0;
            size_t                  i           = 0;

            SetUpTwoJoysticks (backend, service, mixer, sink, xbox, stick);
            first  = sink.writes.back().state;
            before = sink.writes.size();

            stickSample.axes[0] = -1.0f;
            stickSample.axes[1] = -1.0f;
            stickSample.buttons.set (0);
            stickSample.buttons.set (1);
            backend.SetSample (stick.unit, stickSample);
            service.Tick();

            Assert::IsTrue (sink.writes.size() > before, L"the second player's input is written");

            for (i = before; i < sink.writes.size(); i++)
            {
                Assert::AreEqual (first.paddle[0], sink.writes[i].state.paddle[0], L"PDL0 stays player one's");
                Assert::AreEqual (first.paddle[1], sink.writes[i].state.paddle[1], L"and PDL1");
                Assert::AreEqual (first.buttons.test (0), sink.writes[i].state.buttons.test (0), L"and PB0");
                Assert::AreEqual (first.buttons.test (1), sink.writes[i].state.buttons.test (1), L"and PB1");
            }
        }


        //  US7 #11: two paddle profiles make a two-paddle game with nothing set
        //  by hand.
        TEST_METHOD (TwoPaddleProfiles_PlayPdl0AndPdl1)
        {
            FakeControllerBackend                           backend;
            GamePortInputMixer                              mixer;
            RecordingGamePortSink                           sink;
            ControllerInputService                          service (backend, mixer);
            ControllerDeviceInfo                            first  = MakePadDevice ("{AAAA}", L"First Pad");
            ControllerDeviceInfo                            second = MakePadDevice ("{BBBB}", L"Second Pad");
            ControllerSample                                right  = MakeRestSample();
            ControllerSample                                left   = MakeRestSample();
            ControlMapping                                  knob;
            std::map<std::string, ControllerModelSettings>  models;

            knob.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 0 } });
            knob.pb0.push_back  ({ { ControlKind::Button, 0 } });
            models = MakeModelSettings (first, "Knob", knob, ProfileMode::Paddle);

            right.axes[0] = 1.0f;
            right.buttons.set (0);
            left.axes[0]  = -1.0f;
            left.buttons.set (0);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (first);
            backend.AddDevice (second);
            backend.SetSample (first.unit,  right);
            backend.SetSample (second.unit, left);
            SkipCalibration (service, { first.unit, second.unit });

            service.SetModelSettings  (models);
            service.SetActiveProfiles (ProfileMode::Paddle, { { ControllerTokens::UnitToToken (first.unit),  "Knob" },
                                                              { ControllerTokens::UnitToToken (second.unit), "Knob" } });
            service.SetPlayerEntries  ({ MakePick (first.unit, PlayerMode::Paddle), MakePick (second.unit, PlayerMode::Paddle) });
            service.Tick();

            Assert::AreEqual ((int) PlayerAxisTarget::Paddle1, (int) service.GetPlayerSlots()[1].target, L"player two sits beside player one's paddle 0");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"player one drives PDL0");
            Assert::AreEqual (kFullLow,  sink.writes.back().state.paddle[1], L"player two drives PDL1");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"player one's button is PB0");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (1), L"and player two's is PB1");
        }


        //  The player's mode decides the target, and which kind of profile
        //  its controller plays; the Joyport makes both players joysticks.
        TEST_METHOD (Target_FollowsThePlayersMode)
        {
            FakeControllerBackend                           backend;
            GamePortInputMixer                              mixer;
            RecordingGamePortSink                           sink;
            ControllerInputService                          service (backend, mixer);
            ControllerDeviceInfo                            xbox        = MakeXboxDevice();
            ControllerDeviceInfo                            stick       = MakeStickDevice();
            ControllerSample                                stickSample = MakeRestSample();
            ControlMapping                                  knob;

            knob.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 0 } });
            knob.pb0.push_back  ({ { ControlKind::Button, 0 } });

            stickSample.axes[0] = -1.0f;
            stickSample.buttons.set (0);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  MakePushedSample());
            backend.SetSample (stick.unit, stickSample);
            SkipCalibration (service, { stick.unit });

            service.SetModelSettings (MakeModelSettings (stick, "Knob", knob, ProfileMode::Paddle));
            service.SetPlayerEntries ({ MakePick (xbox.unit), MakePick (stick.unit) });
            service.Tick();

            Assert::AreEqual ((int) PlayerAxisTarget::Joystick1, (int) service.GetPlayerSlots()[1].target, L"Joystick mode plays joystick 1");

            service.SetPlayerMode    (1, PlayerMode::Paddle);
            service.SetActiveProfile (stick.unit, "Knob");
            service.Tick();

            Assert::AreEqual ((int) PlayerAxisTarget::Paddle2, (int) service.GetPlayerSlots()[1].target,
                L"Paddle mode beside player one's joystick plays paddle 2");
            Assert::AreEqual (std::string ("Knob"), service.GetActiveProfiles (ProfileMode::Paddle).at (ControllerTokens::UnitToToken (stick.unit)),
                L"the choice is recorded as the stick's Paddle profile");
            Assert::AreEqual (kFullLow, sink.writes.back().state.paddle[kPdl2], L"its knob on PDL2");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (kPb2),     L"and its button on paddle 2's line, PB2");

            service.SetJoyportAvailable (true);
            service.SetPlayerMode (1, PlayerMode::JoyportRight);
            service.Tick();

            Assert::AreEqual ((int) PlayerAxisTarget::Joystick1, (int) service.GetPlayerSlots()[1].target,
                L"in a jack, player two keeps the joystick its number gives");
            Assert::AreEqual (std::string(), service.GetActiveProfile (stick.unit), L"and plays its Joyport choice, none here");
        }


        //  A controller plays the profile chosen for its player's kind, and
        //  moves to the other kind's choice when the mode changes, releasing
        //  what the old profile held.
        TEST_METHOD (Profile_FollowsThePlayersMode)
        {
            FakeControllerBackend                           backend;
            GamePortInputMixer                              mixer;
            RecordingGamePortSink                           sink;
            ControllerInputService                          service (backend, mixer);
            ControllerDeviceInfo                            stick  = MakeStickDevice();
            ControllerSample                                pushed = MakeRestSample();
            std::string                                     token  = ControllerTokens::UnitToToken (stick.unit);
            ControlMapping                                  knob;
            ControlMapping                                  upward;
            std::map<std::string, ControllerModelSettings>  models;

            knob.pdl0.push_back   ({ AxisBindingKind::Analog, { ControlKind::Axis, 0 } });
            upward.pdl1.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 0 } });
            models = MakeModelSettings (stick, "Knob", knob, ProfileMode::Paddle);
            models.begin()->second.AddProfile ("Upward", upward, ProfileMode::Joystick);

            pushed.axes[0] = -1.0f;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, pushed);
            SkipCalibration (service, { stick.unit });

            service.SetModelSettings  (models);
            service.SetActiveProfiles (ProfileMode::Joystick, { { token, "Upward" } });
            service.SetActiveProfiles (ProfileMode::Paddle,   { { token, "Knob" } });
            service.Tick();

            Assert::AreEqual (std::string ("Upward"), service.GetActiveProfile (stick.unit), L"Joystick mode plays the Joystick choice");
            Assert::AreEqual (kFullLow, sink.writes.back().state.paddle[1], L"which drives PDL1");

            service.SetPlayerMode (0, PlayerMode::Paddle);
            service.Tick();

            Assert::AreEqual (std::string ("Knob"), service.GetActiveProfile (stick.unit), L"Paddle mode plays the Paddle choice");
            Assert::AreEqual (kFullLow, sink.writes.back().state.paddle[0], L"which drives PDL0");
            Assert::AreEqual (kCenter,  sink.writes.back().state.paddle[1], L"and what the Joystick profile held is released");
            Assert::IsTrue   (service.GetSnapshot().profileModes.at (token) == ProfileMode::Paddle, L"the picker is told the kind it plays");
        }


        TEST_METHOD (SingleController_ClaimsPdl0AndPdl1Only)
        {
            FakeControllerBackend                           backend;
            GamePortInputMixer                              mixer;
            RecordingGamePortSink                           sink;
            ControllerInputService                          service (backend, mixer);
            ControllerDeviceInfo                            xbox    = MakeXboxDevice();
            ControllerSample                                sample;
            ControlMapping                                  mapping;
            ControllerModelSettings                         settings;
            std::map<std::string, ControllerModelSettings>  models;

            mapping.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kLeftStickX } });
            mapping.pdl1.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kLeftStickY } });
            mapping.pdl2.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kRightStickX } });
            mapping.pdl3.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kRightStickY } });

            settings.profiles.push_back ({ "Default", ControllerProfileKind::Default, mapping });
            models[ControllerTokens::ModelToToken (xbox.unit.model)] = settings;

            sample.connected = true;
            sample.axes[XInputSampleDecoder::kLeftStickX]  =  1.0f;
            sample.axes[XInputSampleDecoder::kLeftStickY]  = -1.0f;
            sample.axes[XInputSampleDecoder::kRightStickX] = -1.0f;
            sample.axes[XInputSampleDecoder::kRightStickY] =  1.0f;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, sample);

            service.SetModelSettings (models);
            Pick (service, xbox.unit);
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"left stick X on PDL0");
            Assert::AreEqual (kFullLow,  sink.writes.back().state.paddle[1], L"left stick Y on PDL1");

            // The one controller drives the first two axes however many its
            // profile binds; the rest belong to a second player.
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[kPdl2], L"PDL2 is left free");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[kPdl3], L"as is PDL3");
        }


        //
        //  Automatic
        //

        //  US7 #9, SC-014: two controllers connecting in turn are Player 1 and
        //  Player 2 in that order.
        TEST_METHOD (Automatic_TwoConnectingInTurnArePlayersOneAndTwo)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    first  = MakePadDevice ("{BBBB}", L"First Pad");
            ControllerDeviceInfo    second = MakePadDevice ("{AAAA}", L"Second Pad");

            service.Tick();

            backend.AddDevice (first);
            service.OnDevicesChanged();
            service.Tick();

            // Enumeration lists the second first, so only the arrival order
            // can put them the right way round.
            backend.devices.insert (backend.devices.begin(), FakeControllerBackend::Device { second, MakeRestSample() });
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue (service.GetPlayerSlots()[0].holder.value() == first.unit,  L"the first to connect is Player 1");
            Assert::IsTrue (service.GetPlayerSlots()[1].holder.value() == second.unit, L"and the second Player 2");
        }


        //  US7 #10, SC-013: with a stick and a gamepad attached at launch, the
        //  one used first is Player 1 and drives everything.
        TEST_METHOD (Automatic_TheFirstUsedAtLaunchIsPlayerOne)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            double                  now   = 0.0;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            service.SetClock ([&now]() { return now; });
            backend.AddDevice (stick);
            backend.AddDevice (xbox, true);
            backend.SetSample (stick.unit, MakeRestSample());
            backend.SetSample (xbox.unit,  MakeRestSample());
            service.Tick();

            Assert::IsFalse (service.GetPlayerSlots()[0].holder.has_value(), L"two attached and none used: none plays yet");

            // The pad is watched on the idle period, so it is read again once
            // that has passed.
            backend.SetSample (xbox.unit, MakePushedSample());
            now += kIdleSeconds;
            service.Tick();

            Assert::IsTrue   (service.GetPlayerSlots()[0].holder.value() == xbox.unit, L"the gamepad, used first, is Player 1");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"and drives PDL0 on the reading that chose it");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"and PB0");
            Assert::IsFalse  (service.GetPlayerSlots()[1].holder.has_value(), L"the stick drives nothing until it is used");
        }


        TEST_METHOD (Automatic_InputWhileInactiveClaimsNothing)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    first  = MakePadDevice ("{AAAA}", L"First Pad");
            ControllerDeviceInfo    second = MakePadDevice ("{BBBB}", L"Second Pad");

            SkipCalibration (service, { first.unit, second.unit });
            backend.AddDevice (first);
            backend.AddDevice (second);
            backend.SetSample (first.unit,  MakeRestSample());
            backend.SetSample (second.unit, MakePushedSample());

            service.SetActive (false);
            service.Tick();

            Assert::IsFalse (service.GetPlayerSlots()[0].holder.has_value(),
                L"playing another game on a controller does not give it a slot (FR-033)");

            service.SetActive (true);
            service.Tick();

            Assert::IsTrue (service.GetPlayerSlots()[0].holder.value() == second.unit, L"the same input with Casso active does");
        }


        TEST_METHOD (LoneController_DrivesWithItsDefaultMappingBeforeItIsUsed)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());

            // No pick: a lone controller already attached at startup plays as
            // Player 1 at once (FR-032). It has to be given its default
            // mapping too -- an empty mapping leaves every control unbound, so
            // the controller would read as resting however far it was pushed.
            service.Tick();

            Assert::IsTrue (service.GetPlayerSlots()[0].holder.value() == device.unit, L"the lone controller is Player 1");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0],
                L"a controller attached before any pick was made must still drive the paddles");
            Assert::IsTrue (sink.writes.back().state.buttons.test (0),
                L"and its buttons must reach the game port");
        }


        //  US3 #2a: one person playing whose controller disconnected, and a
        //  second controller used in its place, keeps the second as Player 1
        //  when the first returns.
        TEST_METHOD (StartOver_TheControllerUsedInThePlaceOfOneThatLeftStaysPlayerOne)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerDeviceInfo    pad   = MakePadDevice ("{AAAA}", L"First Pad");
            ControllerSample        left  = MakePushedSample();
            ControllerSample        right = MakePushedSample();

            left.axes[0] = -1.0f;

            SkipCalibration (service, { stick.unit, pad.unit });
            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, left);
            service.Tick();

            backend.AddDevice (pad);
            backend.SetSample (pad.unit, MakeRestSample());
            service.OnDevicesChanged();
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue (service.GetPlayerSlots()[0].holder.value() == pad.unit,
                L"with nobody left playing, Automatic starts over and the one controller left is Player 1");

            backend.SetSample (pad.unit, right);
            service.Tick();

            backend.AddDevice (stick);
            backend.SetSample (stick.unit, left);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue   (service.GetPlayerSlots()[0].holder.value() == pad.unit,   L"the pad stays Player 1");
            Assert::IsTrue   (service.GetPlayerSlots()[1].holder.value() == stick.unit, L"and the stick, used, is Player 2");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0],     L"the pad still drives PDL0");
            Assert::AreEqual (kFullLow,  sink.writes.back().state.paddle[kPdl2], L"and the stick plays joystick 1");
        }


        //  US3 #2: a lone controller that disconnects and returns plays again.
        TEST_METHOD (LoneController_ReturnsAsPlayerOne)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();

            SkipCalibration (service, { stick.unit });

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakePushedSample());
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakePushedSample());
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue   (service.GetPlayerSlots()[0].holder.value() == stick.unit, L"it plays as Player 1 again");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"and drives again");
        }


        //
        //  Picks
        //

        TEST_METHOD (PickedController_DrivesTheGamePort)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());

            Pick (service, device.unit);
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"a pushed stick reaches the end");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),     L"and its first button is PB0");
        }


        TEST_METHOD (NoSlot_ControllersAreIgnored)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();
            PlayerEntry             disabled;

            disabled.kind = PlayerEntryKind::Disabled;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit, MakePushedSample());

            service.SetPlayerEntries ({ MakePick (stick.unit), disabled });
            service.Tick();

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"a controller in no slot must not reach the game port (FR-009)");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"nor its buttons");
        }


        //  US2 #4, US7 #8: a picked controller that is absent stays picked and
        //  plays when it connects.
        TEST_METHOD (PickedAbsent_WaitsAndPlaysWhenItConnects)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox = MakeXboxDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            Pick (service, xbox.unit);
            service.Tick();

            Assert::AreEqual ((int) PlayerSlotState::Waiting, (int) service.GetPlayerSlots()[0].state, L"the pick is kept, and waits");

            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, MakePushedSample());
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"plugging it in plays it, with no user action");
        }


        //  FR-032: a picked DirectInput unit absent at launch, with exactly
        //  one unit of its model attached, is taken as the pick.
        TEST_METHOD (PickedAbsent_TheSoleUnitOfItsModelIsTakenAsIt)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    moved   = MakePadDevice ("{OLD-PORT}", L"Pad");
            ControllerDeviceInfo    same    = MakePadDevice ("{NEW-PORT}", L"Pad");
            bool                    isSaved = false;

            service.SetSlotsChangedFn ([&isSaved] (const ControllerInputService::SlotsChange & change) { isSaved = isSaved || change.haveEntriesChanged; });
            backend.AddDevice (same);
            Pick (service, moved.unit);
            service.Tick();

            Assert::IsTrue (service.GetPlayerSlots()[0].holder.value() == same.unit, L"the same model on another port is the picked controller");
            Assert::IsTrue (service.GetPlayerEntries()[0].unit.value() == same.unit, L"and the pick follows it");
            Assert::IsTrue (isSaved, L"which the shell is told, so it is saved");
        }


        //  FR-018a: the same for an Xbox-class controller in another slot.
        TEST_METHOD (PickedAbsent_TheSoleXboxControllerIsTakenAsIt)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    picked = MakeXboxDevice();
            ControllerDeviceInfo    here   = MakeXboxDevice();

            picked.unit.unitId = "045e:02e0";
            picked.unit.source = ControllerUnitSource::XInputProduct;
            here.unit.unitId   = "045e:0b13";
            here.unit.source   = ControllerUnitSource::XInputProduct;

            backend.AddDevice (here, true);
            Pick (service, picked.unit);
            service.Tick();

            Assert::IsTrue (service.GetPlayerSlots()[0].holder.value() == here.unit, L"the one Xbox controller attached is the pick");
        }


        TEST_METHOD (FailedRead_IsReportedAsDisconnectedNotAsRest)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());

            Pick (service, device.unit);
            service.Tick();

            backend.failNextRead = HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED);
            service.Tick();

            Assert::IsFalse  (service.GetSnapshot().isAnyDriverConnected, L"the controller must read as disconnected");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"and its held stick must be released, not left where it was");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"and its held button released with it");
        }


        TEST_METHOD (Inactive_ReleasesAndResumesOnReactivation)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());

            Pick (service, device.unit);
            service.Tick();

            service.SetActive (false);
            service.Tick();

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"an inactive Casso must not keep driving the game port");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"a button held as the user switches away must not stay down");

            service.SetActive (true);
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"and input resumes when Casso is active again");
        }


        TEST_METHOD (OtherSources_KeepTheirButtonsWhileTheControllerReleases)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();
            GamePortContribution    fireKeys;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());

            fireKeys.buttons.set (0);
            mixer.Submit (GamePortSource::FireKeys, fireKeys);

            Pick (service, device.unit);
            service.Tick();
            service.SetActive (false);
            service.Tick();

            Assert::IsTrue (sink.writes.back().state.buttons.test (0),
                L"the keyboard still holds PB0, so releasing the controller must not release it");
        }


        TEST_METHOD (WaitTimeout_OnlyWhileAPolledControllerPlays)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();

            Assert::IsFalse (service.Tick().timeoutMs.has_value(), L"with nothing attached the thread must not wake on a timer at all");

            backend.AddDevice (xbox, true);    // XInput: must be polled
            service.OnDevicesChanged();
            Pick (service, xbox.unit);
            Assert::AreEqual (ControllerInputService::kPollPeriodMs, service.Tick().timeoutMs.value(),
                L"a playing Xbox controller is polled at the measured period");

            backend.RemoveDevice (xbox.unit);
            backend.AddDevice (stick, false);  // DirectInput: signals its own changes
            service.OnDevicesChanged();
            Pick (service, stick.unit);
            Assert::IsFalse (service.Tick().timeoutMs.has_value(),
                L"a stick that signals its own changes needs no timer at all");
        }


        TEST_METHOD (Snapshot_ReportsWhatIsAttached)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox     = MakeXboxDevice();
            ControllerDeviceInfo    stick    = MakeStickDevice();

            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual (static_cast<size_t> (2), service.GetSnapshot().devices.size(), L"both controllers are listed");
        }


        TEST_METHOD (DeviceChange_IsPickedUpOnTheNextTick)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();

            // The controller thread hands the service to the backend as its
            // event sink; without that wiring an arrival reaches nobody.
            backend.Initialize (&service);

            service.Tick();
            Assert::AreEqual (static_cast<size_t> (0), service.GetSnapshot().devices.size(), L"nothing attached yet");

            backend.AddDevice (stick);
            backend.RaiseDevicesChanged();
            service.Tick();

            Assert::AreEqual (static_cast<size_t> (1), service.GetSnapshot().devices.size(), L"the arrival is picked up on the next tick");
        }


        //  FR-010, FR-013: a controller that held a slot releases within the
        //  tick that saw it go, and the shell is given its description.
        TEST_METHOD (Disconnect_ReleasesWithinOneTickAndGivesTheControllersDescription)
        {
            FakeControllerBackend       backend;
            GamePortInputMixer          mixer;
            RecordingGamePortSink       sink;
            ControllerInputService      service (backend, mixer);
            ControllerDeviceInfo        stick = MakeStickDevice();
            std::vector<std::wstring>   departed;

            SkipCalibration (service, { stick.unit });

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            service.SetSlotsChangedFn ([&departed] (const ControllerInputService::SlotsChange & change)
            {
                departed.insert (departed.end(), change.departedDescriptions.begin(), change.departedDescriptions.end());
            });
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakePushedSample());
            service.Tick();
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"the stick is driving");

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual ((size_t) 1, departed.size(), L"the controller that left is reported once");
            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"), departed.front(), L"by its description");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0],
                L"the axes return to center within the tick that saw the disconnect");
            Assert::IsFalse (sink.writes.back().state.buttons.test (0), L"and its buttons are released");
        }


        //  The saved last holder filling its slot again is not announced;
        //  another controller is, and becomes the last holder.
        TEST_METHOD (Notice_OnlyAControllerOtherThanTheSlotsLastHolderIsAnnounced)
        {
            FakeControllerBackend       backend;
            GamePortInputMixer          mixer;
            ControllerInputService      service (backend, mixer);
            ControllerDeviceInfo        stick     = MakeStickDevice();
            ControllerDeviceInfo        pad       = MakePadDevice ("{PAD}", L"Pad");
            PlayerLastHolders           lastHolders;
            std::vector<std::wstring>   notices;
            bool                        haveMoved = false;

            lastHolders[0] = stick.unit;
            service.SetLastHolders (lastHolders);
            service.SetSlotsChangedFn ([&notices, &haveMoved] (const ControllerInputService::SlotsChange & change)
            {
                notices.insert (notices.end(), change.notices.begin(), change.notices.end());
                haveMoved = haveMoved || change.haveLastHoldersChanged;
            });

            backend.AddDevice (stick);
            service.Tick();

            Assert::IsTrue  (service.GetPlayerSlots()[0].holder == stick.unit, L"the stick is Player 1");
            Assert::IsTrue  (notices.empty(), L"it held the slot last time, so nothing is shown");
            Assert::IsFalse (haveMoved);

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();
            backend.AddDevice (pad);
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual ((size_t) 1, notices.size(), L"a different controller in the slot is announced once");
            Assert::AreEqual (std::wstring (L"Player 1: Pad"), notices.front());
            Assert::IsTrue   (service.GetLastHolders()[0] == pad.unit, L"and is the last holder from now on");
            Assert::IsTrue   (haveMoved, L"which the shell is told, so it is saved");
        }


        TEST_METHOD (Disconnect_OfAControllerInNoSlotIsNotReported)
        {
            FakeControllerBackend       backend;
            GamePortInputMixer          mixer;
            ControllerInputService      service (backend, mixer);
            ControllerDeviceInfo        xbox  = MakeXboxDevice();
            ControllerDeviceInfo        stick = MakeStickDevice();
            std::vector<std::wstring>   departed;

            service.SetSlotsChangedFn ([&departed] (const ControllerInputService::SlotsChange & change)
            {
                departed.insert (departed.end(), change.departedDescriptions.begin(), change.departedDescriptions.end());
            });
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            Pick (service, xbox.unit);
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue (departed.empty(), L"a controller that played nothing leaves without a notice");
        }


        TEST_METHOD (Disconnect_ReleaseRefusedBySinkStillArrivesThroughFlushPending)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakePushedSample());
            Pick (service, stick.unit);
            service.Tick();

            // A machine rebuild holds the sink off. The release must not be
            // dropped on the floor: the paddles would stay where the last
            // read left them, with the buttons held down.
            sink.refuseNext = 1;
            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual (1, sink.refusedCount,    L"the release was refused");
            Assert::IsTrue   (mixer.HasPendingWrite(), L"and is still pending");
            Assert::IsTrue   (mixer.FlushPending(),    L"the flush writes it");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"and the axes reach center after all");
        }


        TEST_METHOD (DeviceListChange_IsAnnouncedEvenWhenNothingElseChanges)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick         = MakeStickDevice();
            ControllerDeviceInfo    xbox          = MakeXboxDevice();
            int                     announcements = 0;

            service.SetStateChangedFn ([&announcements] () { announcements++; });
            backend.AddDevice (stick);
            Pick (service, stick.unit);
            service.Tick();

            announcements = 0;

            // The Xbox controller arrives while the stick plays. The players
            // do not move -- but the picker's rows do, and without an
            // announcement they never learn of it.
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, MakeRestSample());
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue (announcements > 0, L"an arrival that changes only the device list is still announced");
            Assert::IsTrue (service.GetPlayerSlots()[0].holder.value() == stick.unit, L"and it does not take Player 1");

            announcements = 0;
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual (0, announcements, L"a rescan that finds the same controllers announces nothing");
        }


        TEST_METHOD (Rescan_MovesNobody)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick   = MakeStickDevice();
            ControllerDeviceInfo    pad     = MakePadDevice ("{AAAA}", L"First Pad");
            int                     changes = 0;

            SkipCalibration (service, { stick.unit, pad.unit });

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (stick);
            backend.AddDevice (pad);
            backend.SetSample (stick.unit, MakeRestSample());
            backend.SetSample (pad.unit,   MakePushedSample());
            service.Tick();

            service.SetSlotsChangedFn ([&changes] (const ControllerInputService::SlotsChange &) { changes++; });

            // The shell rescans after a device notification at +300 ms and
            // +2 s, because arrival and readiness are not the same moment.
            // Neither rescan may move a player.
            for (int scan = 0; scan < 2; scan++)
            {
                service.OnDevicesChanged();
                service.Tick();
            }

            Assert::AreEqual (0, changes, L"a rescan moves nothing");
            Assert::IsTrue   (service.GetPlayerSlots()[0].holder.value() == pad.unit, L"the pad, used first, keeps Player 1");
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"and it is still driving");
        }


        //
        //  Profiles
        //

        // A model's settings holding the built-in Default plus one extra
        // profile with the given mapping.
        static std::map<std::string, ControllerModelSettings> MakeModelSettings (const ControllerDeviceInfo & device,
                                                                                 const char                 * pszName,
                                                                                 const ControlMapping       & mapping,
                                                                                 ProfileMode                  mode = ProfileMode::Joystick)
        {
            std::map<std::string, ControllerModelSettings>  models;
            ControllerProfileStore                          store;
            ControllerModelSettings                       & settings = store.GetOrCreateModel (device.unit.model, ControllerFormFactor::Gamepad, device.controls);



            settings.AddProfile (pszName, mapping, mode);
            models[ControllerTokens::ModelToToken (device.unit.model)] = settings;
            return models;
        }


        static ControlMapping MakeButtonOneToPb1Mapping()
        {
            ControlMapping  mapping;
            ButtonBinding   binding;

            binding.control = { ControlKind::Button, 0 };
            mapping.pb1.push_back (binding);
            return mapping;
        }


        //  A controller with no profile chosen plays the Joyport profile while
        //  its player is in a jack, where the D-pad steers and X fires, and
        //  the Default otherwise, where neither does anything.
        TEST_METHOD (UnchosenProfile_IsTheModesBuiltInProfile)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();
            ControllerSample        sample = MakeRestSample();

            sample.hats[0] = ControllerSample::kHatRight;
            sample.buttons.set (2);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, sample);

            Pick (service, device.unit);
            service.Tick();

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"the Default leaves the D-pad unbound");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"and X is not fire");

            SetPlayerOneInJoyport (service, true);
            service.Tick();

            Assert::IsTrue   (IsLeftSwitchClosed (sink, JoystickSwitch::Right), L"the Joyport profile steers with the D-pad");
            Assert::IsTrue   (IsLeftSwitchClosed (sink, JoystickSwitch::Fire),  L"and fires with X");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0],      L"and reaches no paddle input");

            SetPlayerOneInJoyport (service, false);
            service.Tick();

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"leaving the jack goes back to the Default");
        }


        // One normal-mode profile, "Dpad", that steers PDL0 with the D-pad,
        // and one Joyport-mode profile, "Atari", that binds nothing but fire
        // on X. Neither built-in profile plays like either of them.
        static std::map<std::string, ControllerModelSettings> MakeProfilesOfEachMode (const ControllerDeviceInfo & device)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (device.unit.model, ControllerFormFactor::Gamepad, device.controls);
            ControlMapping             dpad;
            ControlMapping             atari;
            AxisBinding                pair;

            pair.kind     = AxisBindingKind::DigitalPair;
            pair.negative = { ControlKind::DpadLeft,  0 };
            pair.positive = { ControlKind::DpadRight, 0 };
            dpad.pdl0.push_back (pair);

            atari.pb0.push_back ({ { ControlKind::Button, 2 } });

            settings.AddProfile ("Dpad",  dpad,  ProfileMode::Joystick);
            settings.AddProfile ("Atari", atari, ProfileMode::Joyport);
            return store.models;
        }


        //  A profile picked with the player in a jack is that controller's
        //  Joyport-mode choice, and one picked without it is its normal-mode
        //  choice. Moving into or out of the jack plays the choice made for
        //  the new mode.
        TEST_METHOD (ChosenProfile_IsRememberedForEachMode)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();
            ControllerSample        sample = MakeRestSample();

            sample.hats[0] = ControllerSample::kHatRight;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, sample);
            service.SetModelSettings (MakeProfilesOfEachMode (device));

            Pick (service, device.unit);
            service.SetActiveProfile (device.unit, "Dpad");
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"a normal-mode profile, picked without a Joyport");

            SetPlayerOneInJoyport (service, true);
            service.SetActiveProfile (device.unit, "Atari");
            service.Tick();

            Assert::IsFalse (IsLeftSwitchClosed (sink, JoystickSwitch::Right), L"a Joyport-mode profile, picked in a jack");

            SetPlayerOneInJoyport (service, false);
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"leaving the jack plays the normal-mode choice");
            Assert::AreEqual (std::string ("Dpad"), service.GetActiveProfile (device.unit));

            SetPlayerOneInJoyport (service, true);
            service.Tick();

            Assert::IsFalse  (IsLeftSwitchClosed (sink, JoystickSwitch::Right), L"and going back into it the Joyport-mode one");
            Assert::AreEqual (std::string ("Atari"), service.GetActiveProfile (device.unit));
            Assert::AreEqual (std::string ("Dpad"), service.GetActiveProfiles (ProfileMode::Joystick).at (ControllerTokens::UnitToToken (device.unit)));
        }


        //  With the Joyport on and nothing chosen, a DirectInput gamepad plays
        //  the Joyport profile made for its form factor, so its second stick
        //  steers; a joystick with the same axes gets no second stick.
        TEST_METHOD (JoyportProfile_OnADirectInputGamepad_SteersWithItsSecondStick)
        {
            const ControllerFormFactor  formFactors[] = { ControllerFormFactor::Gamepad, ControllerFormFactor::Joystick };
            const Byte                  expected[]    = { kFullHigh, kCenter };

            for (size_t i = 0; i < std::size (formFactors); i++)
            {
                FakeControllerBackend   backend;
                GamePortInputMixer      mixer;
                RecordingGamePortSink   sink;
                ControllerInputService  service (backend, mixer);
                ControllerDeviceInfo    device = MakePadDevice ("{PAD}", L"USB gamepad");
                ControllerSample        sample = MakeRestSample();

                device.formFactor = formFactors[i];
                device.controls.push_back ({ ControlKind::Axis, DefaultMapping::kAxisZ });
                device.controls.push_back ({ ControlKind::Axis, DefaultMapping::kAxisRz });

                sample.axes[DefaultMapping::kAxisZ] = 1.0f;

                mixer.SetSink (&sink);
                mixer.SetAxisOwner (AxisOwner::Controller);
                backend.AddDevice (device, true);
                backend.SetSample (device.unit, sample);
                SkipCalibration (service, { device.unit });

                Pick (service, device.unit);
                SetPlayerOneInJoyport (service, true);
                service.Tick();

                Assert::AreEqual (expected[i] == kFullHigh, IsLeftSwitchClosed (sink, JoystickSwitch::Right),
                    i == 0 ? L"a gamepad's Z steers" : L"a joystick's Z does not");
            }
        }


        //  A profile of the other mode cannot be chosen: picking one leaves
        //  the choice as it was, and a choice of one loaded from the prefs
        //  plays the mode's built-in profile and is not saved back.
        TEST_METHOD (ChosenProfile_OfTheOtherMode_IsIgnored)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();
            std::string             token  = ControllerTokens::UnitToToken (device.unit);
            ControllerSample        sample = MakeRestSample();

            sample.hats[0] = ControllerSample::kHatRight;
            sample.buttons.set (2);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, sample);
            service.SetModelSettings (MakeProfilesOfEachMode (device));
            Pick (service, device.unit);

            service.SetActiveProfile (device.unit, "Joyport");
            service.SetActiveProfile (device.unit, "Atari");
            service.Tick();

            Assert::AreEqual (std::string(), service.GetActiveProfile (device.unit), L"neither Joyport-mode profile is chosen without a Joyport");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0],            L"so the Default plays");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0));

            SetPlayerOneInJoyport (service, true);
            service.SetActiveProfile (device.unit, "Default");
            service.SetActiveProfile (device.unit, "Dpad");
            service.Tick();

            Assert::AreEqual (std::string(), service.GetActiveProfile (device.unit), L"nor either normal-mode profile in a jack");
            Assert::IsTrue   (IsLeftSwitchClosed (sink, JoystickSwitch::Right),     L"so the Joyport profile plays");

            // Choices loaded from the prefs, each of the other mode's profile.
            service.SetActiveProfiles (ProfileMode::Joystick,  { { token, "Atari" } });
            service.SetActiveProfiles (ProfileMode::Joyport, { { token, "Default" } });
            service.Tick();

            Assert::IsTrue (IsLeftSwitchClosed (sink, JoystickSwitch::Right), L"in a jack, the Joyport profile plays, not the Default");

            SetPlayerOneInJoyport (service, false);
            service.Tick();

            Assert::IsFalse (sink.writes.back().state.buttons.test (0), L"without it, the Default plays, not Atari");
            Assert::IsTrue  (service.GetActiveProfiles (ProfileMode::Joystick).empty(),  L"and neither choice is saved back");
            Assert::IsTrue  (service.GetActiveProfiles (ProfileMode::Joyport).empty());
        }


        TEST_METHOD (ActiveProfile_ItsMappingDrivesTheGamePort)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());
            service.SetModelSettings (MakeModelSettings (device, "Swapped", MakeButtonOneToPb1Mapping()));

            service.SetActiveProfile (device.unit, "swapped");
            Pick (service, device.unit);
            service.Tick();

            Assert::AreEqual (std::string ("swapped"), service.GetSnapshot().activeProfiles.at (ControllerTokens::UnitToToken (device.unit)), L"the snapshot carries the controller's active profile");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (1),                   L"the profile's binding drives PB1");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),                   L"and PB0, which it does not bind, stays up");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0],                 L"and the stick it does not bind rests");
        }


        // Two players on two pads of ONE model, each on its own profile. The
        // active profile belongs to the controller, so the model's shared
        // profile list does not make the players share a choice.
        TEST_METHOD (ActiveProfile_TwoPadsOfOneModelPlayTheirOwn)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    first  = MakeXboxDevice();
            ControllerDeviceInfo    second = MakeXboxDevice();

            first.unit.unitId  = "045e:02e0";
            first.unit.source  = ControllerUnitSource::XInputProduct;
            second.unit.unitId = "045e:02e0:2";
            second.unit.source = ControllerUnitSource::XInputProduct;
            second.xinputSlot  = 1;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (first,  true);
            backend.AddDevice (second, true);
            backend.SetSample (first.unit,  MakePushedSample());
            backend.SetSample (second.unit, MakePushedSample());
            service.SetModelSettings (MakeModelSettings (first, "Swapped", MakeButtonOneToPb1Mapping()));

            service.SetPlayerEntries ({ MakePick (first.unit), MakePick (second.unit) });
            service.SetActiveProfile (second.unit, "Swapped");
            service.Tick();

            Assert::IsTrue   (service.GetActiveProfile (first.unit).empty(),                L"player one's pad keeps the Default");
            Assert::AreEqual (std::string ("Swapped"), service.GetActiveProfile (second.unit));
            Assert::IsTrue   (sink.writes.back().state.paddle[0] > kCenter,                 L"player one's Default drives their stick");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),                    L"and their first button line");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[kPdl2],              L"player two's profile binds no stick, so their joystick rests");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (kPb2),                 L"and binds no first button, so their line stays up");
        }


        TEST_METHOD (ProfileSwitch_ReleasesWhatTheNewProfileDoesNotDrive)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());
            service.SetModelSettings (MakeModelSettings (device, "Swapped", MakeButtonOneToPb1Mapping()));

            Pick (service, device.unit);
            service.Tick();

            Assert::IsTrue (sink.writes.back().state.buttons.test (0), L"Default holds PB0 down");

            service.SetActiveProfile (device.unit, "Swapped");

            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"the switch releases a button the new profile does not bind");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"and centers an axis it does not drive, before the next reading");

            service.Tick();

            Assert::IsTrue   (sink.writes.back().state.buttons.test (1),   L"the next reading plays the new profile");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"with PB0 still up");
        }


        TEST_METHOD (ProfileSwitch_ReturnsRatePaddlesToCenter)
        {
            FakeControllerBackend                           backend;
            GamePortInputMixer                              mixer;
            RecordingGamePortSink                           sink;
            ControllerInputService                          service (backend, mixer);
            ControllerDeviceInfo                            device  = MakeXboxDevice();
            ControlMapping                                  mapping;
            AxisBinding                                     rate;
            std::map<std::string, ControllerModelSettings>  models;
            double                                          now     = 0.0;
            int                                             i       = 0;

            static constexpr int     kTicks    = 10;
            static constexpr double  kStepSecs = 0.05;
            static constexpr Byte    kNearEnd  = 140;



            rate.analog   = { ControlKind::Axis, XInputSampleDecoder::kLeftStickX };
            rate.response = AxisResponse::Rate;
            mapping.pdl0.push_back (rate);

            // Two profiles with the same rate binding, both in place before
            // any reading: only the switch's own reset can bring the paddle
            // back.
            models = MakeModelSettings (device, "Rate A", mapping);
            models.begin()->second.AddProfile ("Rate B", mapping, ProfileMode::Joystick);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());
            service.SetClock ([&now]() { return now; });
            service.SetModelSettings (models);

            service.SetActiveProfile (device.unit, "Rate A");
            Pick (service, device.unit);

            for (i = 0; i < kTicks; i++)
            {
                service.Tick();
                now += kStepSecs;
            }

            Assert::IsTrue (sink.writes.back().state.paddle[0] > kNearEnd, L"a held stick moves a rate paddle away from center");

            backend.SetSample (device.unit, MakeRestSample());
            service.SetActiveProfile (device.unit, "Rate B");
            service.Tick();

            Assert::IsTrue (sink.writes.back().state.paddle[0] <= kCenter + 1, L"the new profile starts its rate paddle at center");
        }


        TEST_METHOD (ActiveProfileMissing_PlaysDefaultAndSavesNothing)
        {
            FakeControllerBackend                           backend;
            GamePortInputMixer                              mixer;
            RecordingGamePortSink                           sink;
            ControllerInputService                          service (backend, mixer);
            ControllerDeviceInfo                            device = MakeXboxDevice();
            std::map<std::string, ControllerModelSettings>  models = MakeModelSettings (device, "Swapped", MakeButtonOneToPb1Mapping());

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());
            service.SetModelSettings (models);

            service.SetActiveProfile (device.unit, "Deleted Elsewhere");
            Pick (service, device.unit);
            service.Tick();

            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"a missing profile plays the Default mapping");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),     L"buttons included");
            Assert::IsTrue   (service.GetModelSettings() == models,          L"and nothing is created for the missing name");
            Assert::AreEqual (std::string ("Deleted Elsewhere"), service.GetActiveProfile (device.unit), L"the remembered name is kept as it was");
        }


        //  A remembered profile that no longer exists plays the built-in
        //  profile of the mode in effect: for a player in a jack, the Joyport
        //  profile, which steers with the D-pad and fires with X, not the
        //  Default, which does neither.
        TEST_METHOD (ActiveProfileMissing_PlaysTheBuiltInProfileOfTheModeInEffect)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    device = MakeXboxDevice();
            std::string             token  = ControllerTokens::UnitToToken (device.unit);
            ControllerSample        sample = MakeRestSample();



            sample.hats[0] = ControllerSample::kHatRight;
            sample.buttons.set (2);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, sample);
            service.SetModelSettings (MakeProfilesOfEachMode (device));
            service.SetActiveProfiles (ProfileMode::Joyport, { { token, "Gone" } });
            SetPlayerOneInJoyport (service, true);
            Pick (service, device.unit);
            service.Tick();

            Assert::IsTrue (IsLeftSwitchClosed (sink, JoystickSwitch::Right), L"the Joyport profile steers with the D-pad");
            Assert::IsTrue (IsLeftSwitchClosed (sink, JoystickSwitch::Fire),  L"and fires with X");
            Assert::AreEqual (std::string ("Gone"), service.GetActiveProfile (device.unit), L"the remembered name is kept as it was");
        }


        //  Deleting the profile a controller plays makes the built-in profile
        //  of the deleted profile's mode play in its place, in either mode.
        TEST_METHOD (DeletingTheActiveProfile_PlaysTheBuiltInProfileOfItsMode)
        {
            FakeControllerBackend                           backend;
            GamePortInputMixer                              mixer;
            RecordingGamePortSink                           sink;
            ControllerInputService                          service (backend, mixer);
            ControllerDeviceInfo                            device = MakeXboxDevice();
            ControllerSample                                sample = MakeRestSample();
            std::map<std::string, ControllerModelSettings>  models = MakeProfilesOfEachMode (device);



            sample.hats[0] = ControllerSample::kHatRight;
            sample.buttons.set (2);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, sample);
            service.SetModelSettings (models);
            Pick (service, device.unit);

            service.SetActiveProfile (device.unit, "Dpad");
            service.Tick();
            Assert::AreEqual (kFullHigh, sink.writes.back().state.paddle[0], L"Dpad steers with the D-pad");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),     L"and binds no fire");

            models.begin()->second.DeleteProfile ("Dpad");
            service.SetModelSettings (models);
            service.Tick();
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"with Dpad deleted, the Default plays, which leaves the D-pad unbound");

            SetPlayerOneInJoyport (service, true);
            service.SetActiveProfile (device.unit, "Atari");
            service.Tick();
            Assert::IsFalse (IsLeftSwitchClosed (sink, JoystickSwitch::Right), L"Atari binds fire and no steering");
            Assert::IsTrue  (IsLeftSwitchClosed (sink, JoystickSwitch::Fire));

            models.begin()->second.DeleteProfile ("Atari");
            service.SetModelSettings (models);
            service.Tick();
            Assert::IsTrue (IsLeftSwitchClosed (sink, JoystickSwitch::Right), L"with Atari deleted, the Joyport profile plays and steers");
        }


        TEST_METHOD (UnrecognizedUnit_PlaysItsModelsSavedProfile)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    known   = MakePadDevice ("{11111111-0000-0000-0000-000000000001}", L"Pad");
            ControllerDeviceInfo    newUnit = MakePadDevice ("{22222222-0000-0000-0000-000000000002}", L"Pad");
            ControllerSample        sample  = MakeRestSample();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);

            // Saved against the model through another unit; this unit has no
            // calibration or anything else of its own.
            service.SetModelSettings (MakeModelSettings (known, "Swapped", MakeButtonOneToPb1Mapping()));
            service.SetActiveProfile (newUnit.unit, "Swapped");

            sample.buttons.set (0);
            backend.AddDevice (newUnit);
            backend.SetSample (newUnit.unit, sample);

            service.Tick();

            Assert::IsTrue  (service.GetPlayerSlots()[0].holder.value() == newUnit.unit, L"the new unit is picked up on first connection");
            Assert::IsTrue  (sink.writes.back().state.buttons.test (1),                   L"and plays its model's saved profile");
            Assert::IsFalse (sink.writes.back().state.buttons.test (0),                   L"not the Default");
        }


        TEST_METHOD (InspectedUnit_RequestWakesTheThreadUntilItIsRead)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            int                     wakes = 0;

            backend.AddDevice (stick, true);
            backend.AddDevice (xbox, true);
            backend.SetSample (stick.unit, MakePushedSample());
            service.SetWakeFn ([&wakes] { wakes++; });

            service.SetInspectedUnit (stick.unit);
            Assert::AreEqual (1, wakes, L"a new unit wakes the thread, which may be asleep on another controller's events");

            service.SetInspectedUnit (stick.unit);
            Assert::AreEqual (2, wakes, L"asking again before it is read wakes it again");

            service.Tick();
            Assert::IsTrue (service.GetInspectedSample (stick.unit).has_value(), L"the tick reads the inspected unit");

            service.SetInspectedUnit (stick.unit);
            Assert::AreEqual (2,    wakes,                                              L"once read, the same request is a no-op");
            Assert::IsTrue   (service.GetInspectedSample (stick.unit).has_value(),     L"and keeps the reading");

            service.SetInspectedUnit (xbox.unit);
            Assert::AreEqual (3,    wakes,                                              L"another unit wakes it");
            Assert::IsFalse  (service.GetInspectedSample (stick.unit).has_value(),     L"and the old reading is gone");
        }


        //
        //  Joyport jacks
        //

        static JoystickSwitches MakeSwitches (std::initializer_list<JoystickSwitch> closed)
        {
            JoystickSwitches  switches;

            for (JoystickSwitch sw : closed)
            {
                switches.set (static_cast<size_t> (sw));
            }

            return switches;
        }


        TEST_METHOD (Joyport_OneControllerIsOnBothJacks)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox = MakeXboxDevice();
            JoyportJacks            jacks;

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, MakePushedSample());
            Pick (service, xbox.unit);
            SetPlayerOneInJoyport (service, true);
            service.Tick();

            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack]  == MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Fire }),
                L"the one controller drives the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == jacks.jack[JoyportJacks::kLeftJack],
                L"and appears on the right, so a game played by passing the controller reads it on either");
        }


        TEST_METHOD (Joyport_NoControllerLeavesEverySwitchOpen)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);

            mixer.SetAxisOwner (AxisOwner::Controller);
            service.Tick();

            Assert::IsTrue (mixer.GetTargetState().jacks == JoyportJacks(), L"a Joyport with nothing plugged in");
        }


        TEST_METHOD (Joyport_EachPlayerIsOnTheirOwnJack)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            JoyportJacks            jacks;

            //  Both players start in Paddle mode and move into the jacks.
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);
            SetBothInJoyport (service);
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack]  == MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Down, JoystickSwitch::Fire }),
                L"player one is the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == MakeSwitches ({ JoystickSwitch::Left, JoystickSwitch::Fire }),
                L"player two is the right jack, with none of player one's input");
        }


        TEST_METHOD (Joyport_PlayerTwosFireIsOnlyTheRightJacks)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerSample        fire  = MakeRestSample();
            JoyportJacks            jacks;

            fire.buttons.set (0);

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  MakeRestSample());
            backend.SetSample (stick.unit, fire);
            SkipCalibration (service, { stick.unit });

            service.SetPlayerEntries (MakeTwoPlayers (xbox.unit, stick.unit));
            SetBothInJoyport (service);
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsFalse (jacks.jack[JoyportJacks::kLeftJack].test  (static_cast<size_t> (JoystickSwitch::Fire)), L"AN0 low reads fire open");
            Assert::IsTrue  (jacks.jack[JoyportJacks::kRightJack].test (static_cast<size_t> (JoystickSwitch::Fire)), L"AN0 high reads it closed");
        }


        //  A player beside the Joyport drives its paddle, as a lone player of
        //  its mode would, and none of its buttons: the Joyport owns all
        //  three lines. The player in the jack reaches no paddle input, and,
        //  with the other jack free, drives both jacks.
        TEST_METHOD (Joyport_APlayerBesideItKeepsItsPaddleAndLosesItsButtons)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            GamePortState           state;
            JoystickSwitches        xboxOn = MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Down, JoystickSwitch::Fire });



            SetUpTwoPlayers (backend, service, mixer, xbox, stick);
            service.SetJoyportAvailable (true);
            service.SetPlayerMode (0, PlayerMode::JoyportLeft);
            service.Tick();
            state = mixer.GetTargetState();

            Assert::AreEqual (kFullLow, state.paddle[0],                  L"Player 2's knob on paddle 0, as though alone");
            Assert::AreEqual (kCenter,  state.paddle[1],                  L"and Player 1's stick on no paddle input");
            Assert::IsTrue   (state.buttons.none(),                       L"no button line from either");
            Assert::IsTrue   (state.jacks.jack[JoyportJacks::kLeftJack]  == xboxOn, L"Player 1 in the left jack");
            Assert::IsTrue   (state.jacks.jack[JoyportJacks::kRightJack] == xboxOn, L"and the right, which is free");
        }


        //  The mode is the jack, not the player's number: Player 1 in the
        //  right jack and Player 2 in the left.
        TEST_METHOD (Joyport_TheModeIsTheJackWhateverThePlayer)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();
            PlayerEntries           setup = MakeTwoPlayers (xbox.unit, stick.unit);

            setup[0].mode = PlayerMode::JoyportRight;
            setup[1].mode = PlayerMode::JoyportLeft;

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  MakePushedSample());
            backend.SetSample (stick.unit, MakeRestSample());
            SkipCalibration (service, { stick.unit });

            service.SetJoyportAvailable (true);
            service.SetPlayerEntries (setup);
            service.Tick();

            Assert::IsTrue (mixer.GetTargetState().jacks.jack[JoyportJacks::kRightJack].test (static_cast<size_t> (JoystickSwitch::Right)),
                L"Player 1 in the right jack");
            Assert::IsTrue (mixer.GetTargetState().jacks.jack[JoyportJacks::kLeftJack].none(), L"and Player 2, at rest, in the left");
        }


        TEST_METHOD (Joyport_PlayerTwoLeavingOpensOnlyTheRightJack)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            JoyportJacks            jacks;

            SetUpTwoPlayers (backend, service, mixer, xbox, stick);
            SetBothInJoyport (service);
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack].none(), L"a jack with no controller reads every switch open");
            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack] == MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Down, JoystickSwitch::Fire }),
                L"and player one's jack is unaffected, not widened onto the right one");
        }


        TEST_METHOD (Joyport_APlayerPlayingAloneIsOnBothJacks)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox   = MakeXboxDevice();
            ControllerUnitKey       absent = MakeStickDevice().unit;
            ControllerSample        pushed = MakeRestSample();
            JoyportJacks            jacks;

            pushed.axes[XInputSampleDecoder::kLeftStickY] = 1.0f;

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, pushed);

            //  Player 2's controller has never connected, so Player 1 plays
            //  alone.
            service.SetPlayerEntries   (MakeTwoPlayers (xbox.unit, absent));
            SetBothInJoyport (service);
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack].test  (static_cast<size_t> (JoystickSwitch::Down)), L"Player 1 is on the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack].test (static_cast<size_t> (JoystickSwitch::Down)), L"and on the right");
        }


        //  A stick and a gamepad attached at launch, both players on
        //  Automatic, neither used. `secondEntry` is Player 2's entry.
        static void SetUpTwoAtLaunch (FakeControllerBackend   & backend,
                                      ControllerInputService  & service,
                                      GamePortInputMixer      & mixer,
                                      double                  & now,
                                      ControllerDeviceInfo    & xbox,
                                      ControllerDeviceInfo    & stick,
                                      PlayerEntryKind           secondEntry = PlayerEntryKind::Automatic)
        {
            PlayerEntries  entries;



            xbox            = MakeXboxDevice();
            stick           = MakeStickDevice();
            entries[1].kind = secondEntry;

            mixer.SetAxisOwner (AxisOwner::Controller);
            service.SetClock ([&now]() { return now; });
            backend.AddDevice (stick);
            backend.AddDevice (xbox, true);
            backend.SetSample (stick.unit, MakeRestSample());
            backend.SetSample (xbox.unit,  MakeRestSample());
            SkipCalibration (service, { stick.unit });
            service.SetPlayerEntries (entries);
            service.Tick();
        }


        //  The stick pushed left, read once the idle period has passed.
        static void UseStick (FakeControllerBackend & backend, ControllerInputService & service, double & now, const ControllerDeviceInfo & stick)
        {
            ControllerSample  left = MakeRestSample();



            left.axes[0] = -1.0f;
            backend.SetSample (stick.unit, left);
            now += kIdleSeconds;
            service.Tick();
            service.Tick();
        }


        //  US3 #4 and #5: of two controllers attached at launch, the first used
        //  becomes Joyport left and drives both jacks while the other drives
        //  nothing; once the other is used it becomes Joyport right, and the
        //  first drives the left jack alone.
        TEST_METHOD (Joyport_TheFirstUsedDrivesBothJacksUntilTheSecondIsUsed)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            JoyportJacks            jacks;
            double                  now     = 0.0;
            JoystickSwitches        xboxOn  = MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Fire });



            SetUpTwoAtLaunch (backend, service, mixer, now, xbox, stick);
            SetPlayerOneInJoyport (service, true);

            backend.SetSample (xbox.unit, MakePushedSample());
            now += kIdleSeconds;
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (service.GetPlayerSlots()[0].holder == xbox.unit, L"the pad, used first, is Joyport left");
            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack]  == xboxOn,  L"it drives the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == xboxOn,  L"and the right");

            UseStick (backend, service, now, stick);
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (service.GetPlayerSlots()[1].holder == stick.unit,                         L"the stick, used next, is Joyport right");
            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack]  == xboxOn,                           L"the pad keeps the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == MakeSwitches ({ JoystickSwitch::Left }), L"and the stick alone is on the right");
        }


        //  With Player 2 Disabled, which holds no jack, a second controller
        //  used changes nothing: Player 1 still drives both jacks.
        TEST_METHOD (Joyport_ADisabledPlayerTwoLeavesPlayerOneOnBothJacks)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            JoyportJacks            jacks;
            double                  now     = 0.0;
            JoystickSwitches        xboxOn  = MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Fire });



            SetUpTwoAtLaunch (backend, service, mixer, now, xbox, stick, PlayerEntryKind::Disabled);
            SetPlayerOneInJoyport (service, true);

            backend.SetSample (xbox.unit, MakePushedSample());
            now += kIdleSeconds;
            service.Tick();
            UseStick (backend, service, now, stick);
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack]  == xboxOn, L"Player 1 on the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == xboxOn, L"and on the right, with none of the stick's input");
        }


        //  A player who leaves while the other plays keeps their jack, which
        //  reads open, and the one who stayed keeps only their own.
        TEST_METHOD (Joyport_ALeaversJackReadsOpenAndIsNotHandedOver)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;
            ControllerSample        stickSample = MakeRestSample();
            JoyportJacks            jacks;



            SetUpTwoJoysticks (backend, service, mixer, sink, xbox, stick);
            SetBothInJoyport  (service);

            stickSample.axes[0] = -1.0f;
            backend.SetSample (stick.unit, stickSample);
            service.Tick();

            backend.RemoveDevice (xbox.unit);
            service.OnDevicesChanged();
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (service.GetPlayerSlots()[0].state == PlayerSlotState::Held, L"Player 1's slot is held for the pad");
            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack].none(),                 L"its jack reads every switch open");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == MakeSwitches ({ JoystickSwitch::Left }), L"and the stick keeps only the right");
        }


        //  Player 1 on the arrow keys and Player 2 on a controller split the
        //  jacks: the keys on the left, the controller on the right.
        TEST_METHOD (Joyport_KeysAndAControllerSplitTheJacks)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox    = MakeXboxDevice();
            PlayerEntries           entries;
            GamePortContribution    arrows;
            JoyportJacks            jacks;



            entries[0].kind  = PlayerEntryKind::ArrowKeys;
            entries[0].mode  = PlayerMode::JoyportLeft;
            entries[1]       = MakePick (xbox.unit, PlayerMode::JoyportRight);
            arrows.paddle[1] = kFullLow;

            service.SetJoyportAvailable (true);

            mixer.SetAxisOwner (AxisOwner::ArrowKeys);
            mixer.Submit (GamePortSource::ArrowKeys, arrows);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, MakePushedSample());
            service.SetPlayerEntries (entries);
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack]  == MakeSwitches ({ JoystickSwitch::Up }),                         L"the keys on the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Fire }), L"the controller on the right");
        }


        //  The mouse as paddle closes no switch, so Player 2's controller is
        //  the one player driving the Joyport, on both jacks.
        TEST_METHOD (Joyport_BesideTheMouseAControllerIsOnBothJacks)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox    = MakeXboxDevice();
            PlayerEntries           entries;
            JoyportJacks            jacks;
            JoystickSwitches        xboxOn  = MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Fire });



            entries[0].kind = PlayerEntryKind::MousePaddle;
            entries[1]      = MakePick (xbox.unit, PlayerMode::JoyportRight);

            service.SetJoyportAvailable (true);
            mixer.SetAxisOwner (AxisOwner::MousePaddle);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, MakePushedSample());
            service.SetPlayerEntries (entries);
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack]  == xboxOn);
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack] == xboxOn);
        }


        //  For a player in the Joyport the notice gives the jacks: both for a
        //  controller playing alone.
        TEST_METHOD (Joyport_TheNoticeGivesTheJacks)
        {
            FakeControllerBackend      backend;
            GamePortInputMixer         mixer;
            ControllerInputService     service (backend, mixer);
            ControllerDeviceInfo       xbox;
            ControllerDeviceInfo       stick;
            std::vector<std::wstring>  notices;
            double                     now     = 0.0;



            service.SetSlotsChangedFn ([&notices] (const ControllerInputService::SlotsChange & change)
            {
                notices.insert (notices.end(), change.notices.begin(), change.notices.end());
            });

            SetUpTwoAtLaunch (backend, service, mixer, now, xbox, stick);
            SetPlayerOneInJoyport (service, true);

            backend.SetSample (xbox.unit, MakePushedSample());
            now += kIdleSeconds;
            service.Tick();

            Assert::AreEqual (size_t (1), notices.size());
            Assert::AreEqual (std::wstring (L"Player 1 (Joyport left and right): Xbox Controller"), notices.back(), L"the pad plays alone");

            UseStick (backend, service, now, stick);

            Assert::AreEqual (size_t (2), notices.size());
            Assert::AreEqual (std::wstring (L"Player 2 (Joyport right): VKBsim Gladiator"), notices.back(), L"Same as Player 1: the other jack");
        }

        //
        //  Idle watch
        //

        static ControllerDeviceInfo MakeSecondXboxDevice()
        {
            ControllerDeviceInfo  info = MakeXboxDevice();

            info.unit.unitId = "045e:0b13";
            info.unit.source = ControllerUnitSource::XInputProduct;
            info.xinputSlot  = 1;
            return info;
        }


        // Player 1 on a stick that signals its own changes, Player 2 on
        // Automatic with nobody yet, and an Xbox controller nobody plays,
        // which has no change events and so is read on the idle period.
        static void SetUpIdleWatch (FakeControllerBackend   & backend,
                                    ControllerInputService  & service,
                                    double                  & now,
                                    ControllerDeviceInfo    & stick,
                                    ControllerDeviceInfo    & xbox)
        {
            stick = MakeStickDevice();
            xbox  = MakeXboxDevice();

            service.SetClock ([&now]() { return now; });
            backend.AddDevice (stick);
            backend.AddDevice (xbox, true);
            backend.SetSample (stick.unit, MakeRestSample());
            backend.SetSample (xbox.unit,  MakeRestSample());
            SkipCalibration (service, { stick.unit });
            Pick (service, stick.unit);
        }


        TEST_METHOD (IdleWatch_ReadsAPadNobodyPlaysOnTheIdlePeriod)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick;
            ControllerDeviceInfo    xbox;
            ControllerWaitSources   wait;
            double                  now   = 0.0;

            SetUpIdleWatch (backend, service, now, stick, xbox);

            wait = service.Tick();

            Assert::AreEqual (1, backend.GetReadCount (xbox.unit), L"the pad is read when the watch starts");
            Assert::AreEqual (ControllerInputService::kIdleWatchPeriodMs, wait.timeoutMs.value(),
                L"and the thread wakes for the next idle read, not at the poll period");

            now += kIdleSeconds / 2;
            wait = service.Tick();

            Assert::AreEqual (1, backend.GetReadCount (xbox.unit), L"a wake before the period does not read it again");
            Assert::AreEqual (ControllerInputService::kIdleWatchPeriodMs / 2, wait.timeoutMs.value(),
                L"and the wait runs only to the next idle read");

            now += kIdleSeconds / 2;
            service.Tick();

            Assert::AreEqual (2, backend.GetReadCount (xbox.unit), L"the period passing reads it again");
        }


        TEST_METHOD (IdleWatch_ThePollPeriodWinsWhileAPlayerNeedsPolling)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    first  = MakeXboxDevice();
            ControllerDeviceInfo    second = MakeSecondXboxDevice();
            PlayerEntry             disabled;

            disabled.kind = PlayerEntryKind::Disabled;

            backend.AddDevice (first,  true);
            backend.AddDevice (second, true);
            backend.SetSample (first.unit,  MakeRestSample());
            backend.SetSample (second.unit, MakeRestSample());
            Pick (service, first.unit);

            Assert::AreEqual (ControllerInputService::kPollPeriodMs, service.Tick().timeoutMs.value(),
                L"a playing Xbox controller and a watched one: the shorter period wins");
            Assert::AreEqual (1, backend.GetReadCount (second.unit), L"and the watched one is read");

            service.PickPlayerEntry (1, disabled);

            Assert::AreEqual (ControllerInputService::kPollPeriodMs, service.Tick().timeoutMs.value(),
                L"with nobody waiting, the playing controller is still polled at its period");
        }


        //  SC-007: with nobody waiting for a controller, a controller nobody
        //  plays is not read and nothing sets a timer.
        TEST_METHOD (IdleWatch_IsOffWhileNobodyWaits)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick;
            ControllerDeviceInfo    xbox;
            ControllerWaitSources   wait;
            PlayerEntry             disabled;
            double                  now   = 0.0;

            disabled.kind = PlayerEntryKind::Disabled;

            SetUpIdleWatch (backend, service, now, stick, xbox);
            service.PickPlayerEntry (1, disabled);

            service.Tick();
            now += kIdleSeconds;
            wait = service.Tick();

            Assert::AreEqual (0, backend.GetReadCount (xbox.unit), L"no player waits, so the pad is not watched");
            Assert::IsFalse  (wait.timeoutMs.has_value(), L"and the stick that plays signals its own changes, so no timer is set");
        }


        //  Input made while another application is active never claims a
        //  slot, so the watch reads nothing then.
        TEST_METHOD (IdleWatch_IsOffWhileCassoIsInactive)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick;
            ControllerDeviceInfo    xbox;
            ControllerWaitSources   wait;
            double                  now   = 0.0;

            SetUpIdleWatch (backend, service, now, stick, xbox);
            service.SetActive (false);

            service.Tick();
            now += kIdleSeconds;
            wait = service.Tick();

            Assert::AreEqual (0, backend.GetReadCount (xbox.unit), L"an inactive Casso watches nothing");
            Assert::IsFalse  (wait.timeoutMs.has_value(), L"and sets no timer for it");

            service.SetActive (true);
            service.Tick();

            Assert::AreEqual (1, backend.GetReadCount (xbox.unit), L"the watch resumes when Casso is active again");
        }


        TEST_METHOD (IdleWatch_AWatchedDirectInputDeviceWakesTheThreadWithItsEvent)
        {
            static constexpr uintptr_t  kEventValue = 0x1234;

            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    pad   = MakePadDevice ("{AAAA}", L"First Pad");
            HANDLE                  event = reinterpret_cast<HANDLE> (kEventValue);
            ControllerWaitSources   wait;

            backend.AddDevice (xbox, true);
            backend.AddDevice (pad);
            backend.SetSample (xbox.unit, MakeRestSample());
            backend.SetSample (pad.unit,  MakeRestSample());
            backend.FindDevice (pad.unit)->wakeEvent = event;
            Pick (service, xbox.unit);

            wait = service.Tick();

            Assert::IsTrue (std::find (wait.events.begin(), wait.events.end(), event) != wait.events.end(),
                L"the watched device's change event joins the wait");

            service.Tick();

            Assert::AreEqual (2, backend.GetReadCount (pad.unit), L"and it is read on every wake, not on the idle period");
        }


        TEST_METHOD (IdleWatch_AWatchedReadNeverReachesTheMixer)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick;
            ControllerDeviceInfo    xbox;
            ControllerSample        light  = MakeRestSample();
            ControlMapping          mapping;
            ButtonBinding           binding;
            double                  now    = 0.0;

            static constexpr float  kLightThreshold = 0.05f;
            static constexpr float  kLightPress     = 0.1f;



            // A trigger bound to PB0 with a light threshold: pressed this far,
            // it presses PB0 when evaluated, but is short of what counts as a
            // controller being used.
            binding.control   = { ControlKind::Trigger, 0 };
            binding.threshold = kLightThreshold;
            mapping.pb0.push_back (binding);
            light.triggers[0] = kLightPress;

            mixer.SetAxisOwner (AxisOwner::Controller);
            SetUpIdleWatch (backend, service, now, stick, xbox);
            service.SetModelSettings (MakeModelSettings (xbox, "Light", mapping));
            service.SetActiveProfile (xbox.unit, "Light");
            backend.SetSample (xbox.unit, light);

            service.Tick();

            Assert::AreEqual (1, backend.GetReadCount (xbox.unit), L"the pad is read for its first input");
            Assert::IsFalse  (service.GetPlayerSlots()[1].holder.has_value(), L"which it has not given");
            Assert::IsFalse  (mixer.GetTargetState().buttons.test (0), L"and the reading does not reach the game port");

            service.PickPlayerEntry (1, MakePick (xbox.unit));
            service.Tick();

            Assert::IsTrue (mixer.GetTargetState().buttons.test (kPb2), L"picked, the same reading presses its line");
        }


        TEST_METHOD (IdleWatch_AFailedWatchedReadIsReportedAndLeftOut)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox   = MakeXboxDevice();
            ControllerUnitKey       absent = MakeStickDevice().unit;
            double                  now    = 0.0;

            service.SetClock ([&now]() { return now; });
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, MakeRestSample());

            // Player 1's pick is not attached, so the pad is the only thing
            // read, and it is read only by the watch.
            Pick (service, absent);
            backend.failNextRead = HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED);
            service.Tick();

            Assert::AreEqual ((size_t) 1, service.GetSnapshot().unreadable.size(), L"the failed read is reported");
            Assert::IsTrue   (service.GetSnapshot().unreadable.front() == xbox.unit);
            Assert::IsFalse  (service.GetPlayerSlots()[1].holder.has_value(), L"and never taken as a pad at rest");

            now += kIdleSeconds;
            service.Tick();

            Assert::AreEqual (1, backend.GetReadCount (xbox.unit), L"it is left out of the watch until it reconnects");
        }
    };
}
