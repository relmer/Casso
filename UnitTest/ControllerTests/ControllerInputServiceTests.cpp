#include "Pch.h"

#include "Controllers/ControllerInputService.h"

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


        static ControllerSample MakePushedSample()
        {
            ControllerSample  sample;

            sample.connected = true;
            sample.axes[XInputSampleDecoder::kLeftStickX] = 1.0f;
            sample.buttons.set (0);
            return sample;
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
    };
}
