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

        static constexpr Byte  kCenter = 127;


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


        // Two players, each on a paddle of their own: the Xbox controller on
        // PDL0 and the stick on PDL1.
        static MultiplayerSetup MakeTwoPlayers (const ControllerUnitKey &  first,
                                                const ControllerUnitKey &  second,
                                                PlayerAxisTarget           secondTarget = PlayerAxisTarget::Paddle1)
        {
            MultiplayerSetup  setup;

            setup.isEnabled         = true;
            setup.players[0].unit   = first;
            setup.players[0].target = PlayerAxisTarget::Paddle0;
            setup.players[1].unit   = second;
            setup.players[1].target = secondTarget;
            return setup;
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
            ControllerSample  stickSample;

            xbox  = MakeXboxDevice();
            stick = MakeStickDevice();

            xboxSample.axes[XInputSampleDecoder::kLeftStickY] = 1.0f;

            stickSample.connected = true;
            stickSample.axes[0]   = -1.0f;
            stickSample.buttons.set (0);

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  xboxSample);
            backend.SetSample (stick.unit, stickSample);
            SkipCalibration (service, { stick.unit });

            service.SetSelection (xbox.unit);
            service.SetMultiplayer (MakeTwoPlayers (xbox.unit, stick.unit));
            service.Tick();
        }


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

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"PDL0 follows player one");
            Assert::AreEqual (static_cast<Byte> (0),   sink.writes.back().state.paddle[1],
                L"PDL1 follows player two, played through the same default mapping, and not player one's Y");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"player one's button reaches PB0");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (1), L"and player two's reaches PB1 at the same time");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (2), L"PB2 is unused while two people play");
        }


        TEST_METHOD (TwoPlayers_OnlyAPlayersFirstButtonReachesTheirLine)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox      = MakeXboxDevice();
            ControllerDeviceInfo    stick     = MakeStickDevice();
            ControllerSample        xboxOnly;
            ControllerSample        stickOnly;

            // Each player presses ONLY the control their profile binds to PB1.
            // Those bindings belong to the other player's line now, so nothing
            // may reach the game port -- which is exactly what OR-ing every
            // controller's buttons together used to get wrong.
            xboxOnly.connected  = true;
            xboxOnly.buttons.set (1);
            stickOnly.connected = true;
            stickOnly.buttons.set (1);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  xboxOnly);
            backend.SetSample (stick.unit, stickOnly);
            SkipCalibration (service, { stick.unit });

            service.SetMultiplayer (MakeTwoPlayers (xbox.unit, stick.unit));
            service.Tick();

            Assert::IsFalse (mixer.GetTargetState().buttons.test (0),
                L"player one's second button is not their line, and never was another player's either");
            Assert::IsFalse (mixer.GetTargetState().buttons.test (1),
                L"player two's PB1 binding is ignored while two people play; only their first button drives PB1");
        }


        TEST_METHOD (TwoPlayers_TurningTheModeOnFillsEmptySlotsFromWhatIsAttached)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();
            MultiplayerSetup        setup;

            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            service.OnDevicesChanged();
            service.Tick();

            // Turning the mode on with nothing set up must not leave both
            // slots empty: that is a mode in which nobody plays.
            service.SetMultiplayerEnabled (true);
            setup = service.GetMultiplayer();

            Assert::IsTrue (setup.players[0].unit.has_value(), L"player one takes the first attached controller");
            Assert::IsTrue (setup.players[1].unit.has_value(), L"and player two the second");
            Assert::IsTrue (setup.players[0].target == PlayerAxisTarget::Joystick0);
            Assert::IsTrue (setup.players[1].target == PlayerAxisTarget::Joystick1, L"on the two joysticks, so neither overlaps");
            Assert::IsFalse (setup.players[0].unit.value() == setup.players[1].unit.value(), L"and never the same controller twice");
        }


        TEST_METHOD (TwoPlayers_TurningTheModeOnFillsTheSlotTheUserLeftEmpty)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();
            MultiplayerSetup        chosen;
            MultiplayerSetup        setup;

            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            service.OnDevicesChanged();
            service.Tick();

            // A user who has already chosen player one is still owed a second
            // player; filling only when BOTH slots were empty left them with a
            // two-player mode one person could play.
            chosen.players[0].unit   = stick.unit;
            chosen.players[0].target = PlayerAxisTarget::Joystick0;
            service.SetMultiplayer (chosen);

            service.SetMultiplayerEnabled (true);
            setup = service.GetMultiplayer();

            Assert::IsTrue (setup.players[0].unit.value() == stick.unit, L"the chosen player is kept");
            Assert::IsTrue (setup.players[1].unit.has_value(), L"and the empty slot takes the other controller");
            Assert::IsTrue (setup.players[1].unit.value() == xbox.unit, L"never the one player one already holds");
        }


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

            service.SetMultiplayerSlot (1, stick.unit, PlayerAxisTarget::Paddle3);
            service.Tick();

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[1], L"the paddle player two left centers");
            Assert::AreEqual (static_cast<Byte> (0), sink.writes.back().state.paddle[3], L"and they play the one they moved to");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"player one is untouched by it");
        }


        TEST_METHOD (TwoPlayers_AnOverlappingSlotIsRefusedRatherThanPlayed)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;

            mixer.SetSink (&sink);
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);

            // Player two asks for the paddle player one is already playing.
            service.SetMultiplayerSlot (1, stick.unit, PlayerAxisTarget::Paddle0);
            service.Tick();

            Assert::IsFalse  (service.GetMultiplayer().players[1].unit.has_value(),
                L"the slot is emptied rather than left claiming a paddle it cannot have (FR-036)");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"player one keeps playing it");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[1], L"and player two's old paddle centers");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (1),   L"with their button line released");
        }


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
                Assert::AreEqual (static_cast<Byte> (255), sink.writes[i].state.paddle[0],
                    L"player one's paddle is never interrupted by player two leaving (SC-012)");
                Assert::IsTrue (sink.writes[i].state.buttons.test (0), L"nor is their button line");
            }

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[1],
                L"player two's paddle centers, and player one does not move onto it");
            Assert::IsFalse (sink.writes.back().state.buttons.test (1), L"player two's button line is released");
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

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"player one is unaffected");
            Assert::AreEqual (static_cast<Byte> (0),   sink.writes.back().state.paddle[1], L"and so is player two");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[2],
                L"a controller no player holds drives no paddle of its own while two people play");
        }


        TEST_METHOD (MultiplayerOff_HandsTheGamePortBackToTheSelection)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerDeviceInfo    stick;

            mixer.SetSink (&sink);
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);

            // What picking a single source from the toolbar picker does.
            service.SetMultiplayerEnabled (false);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0],
                L"the selection drives PDL0 and PDL1 on its own again");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[1],
                L"including the Y that player two was holding a moment ago");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0), L"and it drives PB0-PB2 as it always has");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (1), L"player two is gone, so nothing holds PB1");
            Assert::IsTrue   (service.GetMultiplayer().players[1].unit.has_value(),
                L"both players are kept, so turning the mode back on is a click rather than a setup job");
        }


        TEST_METHOD (SingleSource_ClaimsPdl0AndPdl1Only)
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

            settings.profiles.push_back ({ "Default", true, mapping });
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
            service.SetSelection (xbox.unit);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"left stick X on PDL0");
            Assert::AreEqual (static_cast<Byte> (0),   sink.writes.back().state.paddle[1], L"left stick Y on PDL1");

            // The one source drives the first two axes however many its
            // profile binds; the rest belong to a second player, and there is
            // none in single-source mode (FR-038).
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[2], L"PDL2 is left free");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[3], L"as is PDL3");
        }


        TEST_METHOD (Selected_ControllerDrivesTheGamePort)
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

            service.SetSelection (device.unit);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"a pushed stick reaches the end");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),                   L"and its first button is PB0");
        }


        TEST_METHOD (NoSelection_AttachedControllerStillGetsItsDefaultMapping)
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

            // No SetSelection: a controller already attached at startup is
            // picked up by the service itself. It has to be given its default
            // mapping too -- an empty mapping leaves every control unbound, so
            // the controller would read as resting however far it was pushed.
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0],
                L"a controller attached before any selection was made must still drive the paddles");
            Assert::IsTrue (sink.writes.back().state.buttons.test (0),
                L"and its buttons must reach the game port");
        }


        TEST_METHOD (Unselected_ControllersAreIgnored)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit, MakePushedSample());

            service.SetSelection (stick.unit);
            service.Tick();

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"the unselected controller must not reach the game port");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"nor its buttons");
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

            service.SetSelection (device.unit);
            service.Tick();

            backend.failNextRead = HRESULT_FROM_WIN32 (ERROR_DEVICE_NOT_CONNECTED);
            service.Tick();

            Assert::IsFalse  (service.GetSnapshot().isSelectedConnected, L"the controller must read as disconnected");
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

            service.SetSelection (device.unit);
            service.Tick();

            service.SetActive (false);
            service.Tick();

            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0], L"an inactive Casso must not keep driving the game port");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (0),   L"a button held as the user switches away must not stay down");

            service.SetActive (true);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"and input resumes when Casso is active again");
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

            service.SetSelection (device.unit);
            service.Tick();
            service.SetActive (false);
            service.Tick();

            Assert::IsTrue (sink.writes.back().state.buttons.test (0),
                L"the keyboard still holds PB0, so releasing the controller must not release it");
        }


        TEST_METHOD (WaitTimeout_OnlyWhileAPolledControllerIsSelected)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();

            Assert::IsFalse (service.Tick().timeoutMs.has_value(), L"with nothing attached the thread must not wake on a timer at all");

            backend.AddDevice (xbox, true);    // XInput: must be polled
            backend.AddDevice (stick, false);  // DirectInput: signals its own changes

            service.SetSelection (xbox.unit);
            Assert::AreEqual (ControllerInputService::kPollPeriodMs, service.Tick().timeoutMs.value(),
                L"a selected Xbox controller is polled at the measured period");

            service.SetSelection (stick.unit);
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

        TEST_METHOD (Disconnect_TheLongestAttachedControllerBecomesTheSelection)
        {
            FakeControllerBackend                backend;
            GamePortInputMixer                   mixer;
            RecordingGamePortSink                sink;
            ControllerInputService               service (backend, mixer);
            ControllerDeviceInfo                 stick = MakeStickDevice();
            ControllerDeviceInfo                 first = MakePadDevice ("{AAAA}", L"First Pad");
            ControllerDeviceInfo                 later = MakePadDevice ("{BBBB}", L"Later Pad");
            ControllerSelectionPolicy::Decision  decision;

            SkipCalibration (service, { stick.unit, first.unit, later.unit });

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            service.SetSelectionChangedFn ([&decision] (const ControllerSelectionPolicy::Decision & d) { decision = d; });
            backend.AddDevice (stick);
            backend.AddDevice (first);
            service.SetSelection (stick.unit);
            service.Tick();

            // The later arrival must not take over from the one that was
            // already there: a successor that changes as unrelated controllers
            // come and go would move the stick out from under the player.
            backend.AddDevice (later);
            backend.SetSample (first.unit, MakePushedSample());
            backend.SetSample (later.unit, MakePushedSample());
            service.OnDevicesChanged();
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue   (service.GetSnapshot().selection.value() == first.unit,
                L"the controller attached longest becomes the selection");
            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason,
                L"and the shell is told, so it persists it and says so");
            Assert::IsTrue   (decision.isAnnounced);
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0],
                L"it drives the axes, under its own default mapping");
        }


        TEST_METHOD (Disconnect_WithNothingAttachedClearsAndReleasesWithinOneTick)
        {
            FakeControllerBackend                backend;
            GamePortInputMixer                   mixer;
            RecordingGamePortSink                sink;
            ControllerInputService               service (backend, mixer);
            ControllerDeviceInfo                 stick = MakeStickDevice();
            ControllerSelectionPolicy::Decision  decision;

            SkipCalibration (service, { stick.unit });

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            service.SetSelectionChangedFn ([&decision] (const ControllerSelectionPolicy::Decision & d) { decision = d; });
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakePushedSample());
            service.SetSelection (stick.unit);
            service.Tick();
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"the stick is driving");

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsFalse  (service.GetSnapshot().selection.has_value(), L"nothing is attached to take over");
            Assert::AreEqual ((int) SelectionChangeReason::Cleared, (int) decision.reason);
            Assert::IsTrue   (decision.isAnnounced, L"a controller that was driving has gone, which the user hears about");
            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"), decision.departedDescription, L"and the notice names the one that left");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[0],
                L"the axes return to center within the tick that saw the disconnect");
            Assert::IsFalse (sink.writes.back().state.buttons.test (0), L"and its buttons are released");
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
            service.SetSelection (stick.unit);
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


        TEST_METHOD (Reconnect_DoesNotTakeTheAxesBack)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerDeviceInfo    pad   = MakePadDevice ("{AAAA}", L"First Pad");
            ControllerSample        left  = MakePushedSample();

            SkipCalibration (service, { stick.unit, pad.unit });

            left.axes[0] = -1.0f;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (stick);
            backend.AddDevice (pad);
            backend.SetSample (stick.unit, left);
            backend.SetSample (pad.unit,   MakePushedSample());
            service.SetSelection (stick.unit);
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"the pad took over");

            backend.AddDevice (stick);
            backend.SetSample (stick.unit, left);
            service.OnDevicesChanged();
            service.Tick();

            // Handing the axes back would move the stick out from under a
            // player who kept using the pad, and picking the pad again
            // to keep it would mean picking what the picker already checks.
            Assert::IsTrue   (service.GetSnapshot().selection.value() == pad.unit,
                L"the controller in use keeps the selection");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0],
                L"and it is still the pad that is read");
        }


        TEST_METHOD (Reconnect_WithNothingElseAttachedIsSelectedAgain)
        {
            FakeControllerBackend                backend;
            GamePortInputMixer                   mixer;
            RecordingGamePortSink                sink;
            ControllerInputService               service (backend, mixer);
            ControllerDeviceInfo                 stick = MakeStickDevice();
            ControllerSelectionPolicy::Decision  decision;

            SkipCalibration (service, { stick.unit });

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            service.SetSelectionChangedFn ([&decision] (const ControllerSelectionPolicy::Decision & d) { decision = d; });
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakePushedSample());
            service.SetSelection (stick.unit);
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakePushedSample());
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue   (service.GetSnapshot().selection.value() == stick.unit, L"it is the next controller to connect");
            Assert::AreEqual ((int) SelectionChangeReason::AutomaticSelection, (int) decision.reason);
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"and it drives again");
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
            service.SetSelection (stick.unit);
            service.Tick();

            announcements = 0;

            // The Xbox controller arrives while the stick drives. The
            // selection does not move -- but the picker's rows do, and
            // without an announcement they never learn of it.
            backend.AddDevice (xbox, true);
            service.OnDevicesChanged();
            service.Tick();

            Assert::IsTrue (announcements > 0, L"an arrival that changes only the device list is still announced");
            Assert::IsTrue (service.GetSnapshot().selection.value() == stick.unit, L"and it does not take the selection");

            announcements = 0;
            service.OnDevicesChanged();
            service.Tick();

            Assert::AreEqual (0, announcements, L"a rescan that finds the same controllers announces nothing");
        }


        TEST_METHOD (Rescan_AfterATakeoverIsIdempotent)
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
            backend.SetSample (pad.unit, MakePushedSample());
            service.SetSelection (stick.unit);
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();

            service.SetSelectionChangedFn ([&changes] (const ControllerSelectionPolicy::Decision &) { changes++; });

            // The shell rescans after a device notification at +300 ms and
            // +2 s, because arrival and readiness are not the same moment.
            // Neither rescan may move the selection again.
            for (int scan = 0; scan < 2; scan++)
            {
                service.OnDevicesChanged();
                service.Tick();
            }

            Assert::AreEqual (0, changes, L"a rescan moves nothing");
            Assert::IsTrue   (service.GetSnapshot().selection.value() == pad.unit, L"the controller that took over keeps it");
            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"and it is still driving");
        }


        TEST_METHOD (SavedSelectionAbsent_IsClearedWithoutANotice)
        {
            FakeControllerBackend                backend;
            GamePortInputMixer                   mixer;
            ControllerInputService               service (backend, mixer);
            ControllerDeviceInfo                 stick = MakeStickDevice();
            ControllerSelectionPolicy::Decision  decision;

            service.SetSelectionChangedFn ([&decision] (const ControllerSelectionPolicy::Decision & d) { decision = d; });

            // A saved controller restored at launch, with nothing plugged in.
            service.SetSelection (stick.unit);
            service.Tick();

            Assert::IsFalse  (service.GetSnapshot().selection.has_value(), L"an absent saved controller is not kept");
            Assert::AreEqual ((int) SelectionChangeReason::Cleared, (int) decision.reason);
            Assert::IsFalse  (decision.isAnnounced, L"it was never driving anything, so dropping it is not news");
        }


        TEST_METHOD (SavedSelectionAbsent_FirstAttachedTakesOver)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerDeviceInfo    pad   = MakePadDevice ("{AAAA}", L"First Pad");

            backend.AddDevice (pad);
            service.Tick();
            service.SetSelection (stick.unit);   // restored for a machine switched to, and not attached
            service.Tick();

            Assert::IsTrue (service.GetSnapshot().selection.value() == pad.unit,
                L"the attached controller replaces it, without waiting for a device to arrive");
        }

        TEST_METHOD (MachineSwitchWithNothingSaved_SelectsAnAttachedController)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    pad = MakePadDevice ("{AAAA}", L"First Pad");

            backend.AddDevice (pad);
            service.Tick();
            Assert::IsTrue (service.GetSnapshot().selection.has_value(), L"the first tick selects it");

            // The user picked the keys: that holds, since nothing connected.
            service.SetSelection (std::nullopt);
            service.Tick();
            Assert::IsFalse (service.GetSnapshot().selection.has_value(), L"a pick of the keys is not undone by a tick");

            // Switching to a machine with nothing saved counts as connecting.
            service.SetSelection (std::nullopt);
            service.RequestRescan();
            service.Tick();
            Assert::IsTrue (service.GetSnapshot().selection.value() == pad.unit, L"the machine switched to selects the attached controller");
        }

        TEST_METHOD (Saved_AClearDoesNotOverwriteIt)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerUnitKey       xbox  = MakeXboxDevice().unit;

            // A machine with the Xbox controller saved, switched to while it
            // is off and the stick is attached.
            backend.AddDevice (stick);
            service.SetSelection (xbox);
            service.Tick();
            Assert::IsTrue (service.GetSnapshot().saved.value() == stick.unit, L"the stick takes over, and is saved");

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();
            Assert::IsFalse (service.GetSnapshot().selection.has_value(),        L"with nothing attached, nothing is in use");
            Assert::IsTrue  (service.GetSnapshot().saved.value() == stick.unit, L"but nothing is never saved over the stick");
        }


        TEST_METHOD (Saved_AMachineSwitchedBackToGetsItsControllerAfterAnUnplug)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerUnitKey       xbox  = MakeXboxDevice().unit;
            ControllerUnitKey       saved;

            backend.AddDevice (stick);
            service.SetSelection (stick.unit);
            service.Tick();

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();
            saved = service.GetSnapshot().saved.value();

            // Away to another machine, where the Xbox controller is turned on
            // and the stick plugged back in; then back, with its saved token.
            backend.AddDevice (MakeXboxDevice());
            backend.AddDevice (stick);
            service.SetSelection (xbox);
            service.RequestRescan();
            service.Tick();
            service.SetSelection (saved);
            service.RequestRescan();
            service.Tick();

            Assert::IsTrue (service.GetSnapshot().selection.value() == stick.unit,
                L"the machine comes back to its own controller, not the one attached longest");
        }


        // A model's settings holding the built-in Default plus one extra
        // profile with the given mapping.
        static std::map<std::string, ControllerModelSettings> MakeModelSettings (const ControllerDeviceInfo & device,
                                                                                 const char                 * pszName,
                                                                                 const ControlMapping       & mapping)
        {
            std::map<std::string, ControllerModelSettings>  models;
            ControllerProfileStore                          store;
            ControllerModelSettings                       & settings = store.GetOrCreateModel (device.unit.model, device.controls);



            settings.AddProfile (pszName, mapping);
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
            service.SetSelection (device.unit);
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

            service.SetMultiplayer    (MakeTwoPlayers (first.unit, second.unit));
            service.SetActiveProfile  (second.unit, "Swapped");
            service.SetSelection      (first.unit);
            service.Tick();

            Assert::IsTrue   (service.GetActiveProfile (first.unit).empty(),                L"player one's pad keeps the Default");
            Assert::AreEqual (std::string ("Swapped"), service.GetActiveProfile (second.unit));
            Assert::IsTrue   (sink.writes.back().state.paddle[0] > kCenter,                 L"player one's Default drives their paddle");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),                    L"and their button line");
            Assert::AreEqual (kCenter, sink.writes.back().state.paddle[1],                  L"player two's profile binds no stick, so their paddle rests");
            Assert::IsFalse  (sink.writes.back().state.buttons.test (1),                    L"and binds no first button, so their line stays up");
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

            service.SetSelection (device.unit);
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
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo                            device  = MakeXboxDevice();
            ControlMapping                                  mapping;
            AxisBinding                                     rate;
            ControllerSample                                rest;
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
            models.begin()->second.AddProfile ("Rate B", mapping);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (device, true);
            backend.SetSample (device.unit, MakePushedSample());
            service.SetClock ([&now]() { return now; });
            service.SetModelSettings (models);

            service.SetActiveProfile (device.unit, "Rate A");
            service.SetSelection (device.unit);

            for (i = 0; i < kTicks; i++)
            {
                service.Tick();
                now += kStepSecs;
            }

            Assert::IsTrue (sink.writes.back().state.paddle[0] > kNearEnd, L"a held stick moves a rate paddle away from center");

            rest.connected = true;
            backend.SetSample (device.unit, rest);
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
            service.SetSelection (device.unit);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), sink.writes.back().state.paddle[0], L"a missing profile plays the Default mapping");
            Assert::IsTrue   (sink.writes.back().state.buttons.test (0),                   L"buttons included");
            Assert::IsTrue   (service.GetModelSettings() == models,                        L"and nothing is created for the missing name");
            Assert::AreEqual (std::string ("Deleted Elsewhere"), service.GetActiveProfile (device.unit), L"the remembered name is kept as it was");
        }


        TEST_METHOD (UnrecognizedUnit_PlaysItsModelsSavedProfile)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    known   = MakePadDevice ("{11111111-0000-0000-0000-000000000001}", L"Pad");
            ControllerDeviceInfo    newUnit = MakePadDevice ("{22222222-0000-0000-0000-000000000002}", L"Pad");
            ControllerSample        sample;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);

            // Saved against the model through another unit; this unit has no
            // calibration or anything else of its own.
            service.SetModelSettings (MakeModelSettings (known, "Swapped", MakeButtonOneToPb1Mapping()));
            service.SetActiveProfile (newUnit.unit, "Swapped");

            sample.connected = true;
            sample.buttons.set (0);
            backend.AddDevice (newUnit);
            backend.SetSample (newUnit.unit, sample);

            service.Tick();

            Assert::IsTrue  (service.GetSnapshot().selection.value() == newUnit.unit, L"the new unit is picked up on first connection");
            Assert::IsTrue  (sink.writes.back().state.buttons.test (1),                L"and plays its model's saved profile");
            Assert::IsFalse (sink.writes.back().state.buttons.test (0),                L"not the Default");
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
            service.SetSelection (xbox.unit);
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

            //  Slot 1 plays PDL0 and slot 2 PDL1 here: single paddles, not
            //  joysticks, which must not matter to the jacks.
            SetUpTwoPlayers (backend, service, mixer, xbox, stick);
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
            ControllerSample        idle;
            ControllerSample        fire;
            JoyportJacks            jacks;

            idle.connected = true;
            fire.connected = true;
            fire.buttons.set (0);

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  idle);
            backend.SetSample (stick.unit, fire);
            SkipCalibration (service, { stick.unit });

            service.SetMultiplayer (MakeTwoPlayers (xbox.unit, stick.unit));
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsFalse (jacks.jack[JoyportJacks::kLeftJack].test  (static_cast<size_t> (JoystickSwitch::Fire)), L"AN0 low reads fire open");
            Assert::IsTrue  (jacks.jack[JoyportJacks::kRightJack].test (static_cast<size_t> (JoystickSwitch::Fire)), L"AN0 high reads it closed");
        }


        TEST_METHOD (Joyport_TheSlotIsTheJackWhateverItsPaddles)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();
            MultiplayerSetup        setup = MakeTwoPlayers (xbox.unit, stick.unit, PlayerAxisTarget::Paddle0);
            ControllerSample        idle;

            idle.connected          = true;
            setup.players[0].target = PlayerAxisTarget::Joystick1;

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (xbox.unit,  MakePushedSample());
            backend.SetSample (stick.unit, idle);
            SkipCalibration (service, { stick.unit });

            service.SetMultiplayer (setup);
            service.Tick();

            Assert::IsTrue (mixer.GetTargetState().jacks.jack[JoyportJacks::kLeftJack].test (static_cast<size_t> (JoystickSwitch::Right)),
                L"slot 1 on the second joystick's paddles is still the left jack");
            Assert::IsTrue (mixer.GetTargetState().jacks.jack[JoyportJacks::kRightJack].none());
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

            backend.RemoveDevice (stick.unit);
            service.OnDevicesChanged();
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack].none(), L"a jack with no controller reads every switch open");
            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack] == MakeSwitches ({ JoystickSwitch::Right, JoystickSwitch::Down, JoystickSwitch::Fire }),
                L"and player one's jack is unaffected");
        }


        TEST_METHOD (Joyport_MultiplayerWithNobodyConnectedFallsBackToOneController)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox  = MakeXboxDevice();
            ControllerDeviceInfo    stick = MakeStickDevice();
            ControllerDeviceInfo    spare = MakePadDevice ("{DDDD}", L"Spare Pad");
            ControllerSample        pushed;
            JoyportJacks            jacks;

            pushed.connected = true;
            pushed.axes[1]   = -1.0f;

            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (spare);
            backend.SetSample (spare.unit, pushed);
            SkipCalibration (service, { spare.unit });

            //  Both players' controllers are unplugged, so the mode cannot be
            //  played and the selection plays alone.
            service.SetMultiplayer (MakeTwoPlayers (xbox.unit, stick.unit));
            service.SetSelection (spare.unit);
            service.Tick();
            jacks = mixer.GetTargetState().jacks;

            Assert::IsTrue (jacks.jack[JoyportJacks::kLeftJack].test  (static_cast<size_t> (JoystickSwitch::Up)), L"the selection is on the left jack");
            Assert::IsTrue (jacks.jack[JoyportJacks::kRightJack].test (static_cast<size_t> (JoystickSwitch::Up)), L"and on the right");
        }
    };
}
