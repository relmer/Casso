#include "Pch.h"

#include "Controllers/ControllerCalibration.h"
#include "Controllers/ControllerInputService.h"
#include "Controllers/ControllerTokens.h"
#include "FakeControllerBackend.h"
#include "RecordingGamePortSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CalibrationTests
//
//  A worn stick has to read center at rest and reach both ends, so these
//  drive calibration with offset rests and short throws, then check the
//  readings a game would see.
//
//  THE RULE THAT MATTERS MOST IS THAT REST READS CENTER. A stick resting off
//  center, uncorrected, is a paddle that drifts in every game.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (CalibrationTests)
    {
    public:

        static constexpr float  kTolerance = 0.001f;


        static ControllerSample MakeSample (float x, float y = 0.0f)
        {
            ControllerSample  sample;

            sample.connected = true;
            sample.axes[0]   = x;
            sample.axes[1]   = y;
            return sample;
        }


        TEST_METHOD (Automatic_AnOffsetRestReadsCenter)
        {
            ControllerCalibration  calibration;

            calibration.CaptureCenter (MakeSample (0.2f, -0.15f));

            Assert::AreEqual (0.0f, calibration.Apply (MakeSample (0.2f, -0.15f)).axes[0], kTolerance, L"X rests at center");
            Assert::AreEqual (0.0f, calibration.Apply (MakeSample (0.2f, -0.15f)).axes[1], kTolerance, L"and so does Y");
        }


        TEST_METHOD (Automatic_AShortThrowReachesBothEndsOnceSeen)
        {
            ControllerCalibration  calibration;

            calibration.CaptureCenter (MakeSample (0.0f));
            calibration.Observe       (MakeSample (0.7f));
            calibration.Observe       (MakeSample (-0.6f));

            Assert::AreEqual ( 1.0f, calibration.Apply (MakeSample (0.7f)).axes[0],  kTolerance, L"the furthest right it went is full right");
            Assert::AreEqual (-1.0f, calibration.Apply (MakeSample (-0.6f)).axes[0], kTolerance, L"the furthest left it went is full left");
            Assert::AreEqual ( 0.5f, calibration.Apply (MakeSample (0.35f)).axes[0], kTolerance, L"and halfway is halfway");
        }


        TEST_METHOD (Automatic_LimitsWidenOnlyOutward)
        {
            ControllerCalibration  calibration;

            calibration.CaptureCenter (MakeSample (0.0f));
            calibration.Observe       (MakeSample (0.8f));
            calibration.Observe       (MakeSample (0.5f));

            Assert::AreEqual (0.8f, calibration.axes[0].maximum, kTolerance,
                L"a smaller push afterwards must not pull the limit back in");
        }


        TEST_METHOD (Automatic_RestJitterStaysSmallBeforeTravelIsSeen)
        {
            ControllerCalibration  calibration;

            calibration.CaptureCenter (MakeSample (0.0f));
            calibration.Observe       (MakeSample (0.02f));

            Assert::IsTrue (calibration.Apply (MakeSample (0.02f)).axes[0] < 0.05f,
                L"a hair's movement is not full deflection just because it is the furthest seen");
        }


        TEST_METHOD (Automatic_AnAxisPinnedAtARailReadsCenter)
        {
            ControllerCalibration  calibration;
            ControllerSample       pinned = MakeSample (0.0f);

            pinned.axes[2] = -1.0f;   // a rudder axis with its pedals unplugged

            calibration.CaptureCenter (pinned);
            calibration.Observe       (pinned);

            Assert::AreEqual (0.0f, calibration.Apply (pinned).axes[2], kTolerance,
                L"an axis that never moves reads center, not a control jammed hard over");
            Assert::AreEqual (calibration.axes[2].minimum, calibration.axes[2].maximum,
                L"and it never widens");
        }


        TEST_METHOD (Automatic_ReconnectKeepsLearnedLimitsAndRecapturesCenter)
        {
            ControllerCalibration  calibration;

            calibration.CaptureCenter (MakeSample (0.0f));
            calibration.Observe       (MakeSample (0.9f));
            calibration.CaptureCenter (MakeSample (0.1f));

            Assert::AreEqual (0.1f, calibration.axes[0].center,  kTolerance, L"the center is taken again at connect");
            Assert::AreEqual (0.9f, calibration.axes[0].maximum, kTolerance, L"the travel it already showed is kept");
        }


        TEST_METHOD (Automatic_AStickHeldOverAtConnectReadsCenterOnceReleased)
        {
            ControllerCalibration  calibration;

            calibration.CaptureCenter (MakeSample (0.8f));
            calibration.Observe       (MakeSample (0.8f));

            Assert::AreEqual (0.0f, calibration.axes[0].center, kTolerance,
                L"a reading that far out is a stick being held over, not where it rests");
            Assert::AreEqual (0.0f, calibration.Apply (MakeSample (0.8f)).axes[0], kTolerance,
                L"and while it is held still it reads center rather than a jammed control");

            calibration.Observe (MakeSample (0.0f));
            Assert::AreEqual (0.0f, calibration.Apply (MakeSample (0.0f)).axes[0], kTolerance, L"let go, it rests at center");

            calibration.Observe (MakeSample (1.0f));
            Assert::AreEqual (1.0f, calibration.Apply (MakeSample (1.0f)).axes[0], kTolerance, L"and pushed over, it reaches the end");
        }


        TEST_METHOD (Automatic_AnAxisThatHasNotMovedReadsCenter)
        {
            ControllerCalibration  calibration;

            calibration.CaptureCenter (MakeSample (0.1f));
            calibration.Observe       (MakeSample (0.11f));

            Assert::AreEqual (0.0f, calibration.Apply (MakeSample (0.11f)).axes[0], kTolerance,
                L"rest jitter is not movement");
        }


        TEST_METHOD (User_SkipsTheConnectTimeCapture)
        {
            ControllerCalibration  calibration;

            calibration.mode    = CalibrationMode::User;
            calibration.axes[0] = { 0.1f, -0.9f, 0.95f };

            calibration.CaptureCenter (MakeSample (0.4f));
            calibration.Observe       (MakeSample (1.0f));

            Assert::AreEqual (0.1f,  calibration.axes[0].center,  kTolerance, L"a user calibration is not recaptured at connect");
            Assert::AreEqual (0.95f, calibration.axes[0].maximum, kTolerance, L"nor widened");
            Assert::AreEqual (0.0f,  calibration.Apply (MakeSample (0.1f)).axes[0],   kTolerance, L"its center reads center");
            Assert::AreEqual (1.0f,  calibration.Apply (MakeSample (0.95f)).axes[0],  kTolerance, L"its limit reads full");
            Assert::AreEqual (-1.0f, calibration.Apply (MakeSample (-0.9f)).axes[0],  kTolerance);
        }


        TEST_METHOD (ResetToAutomatic_RelearnsFromTheNextConnect)
        {
            ControllerCalibration  calibration;

            calibration.mode    = CalibrationMode::User;
            calibration.axes[0] = { 0.1f, -0.9f, 0.95f };

            calibration.ResetToAutomatic();
            calibration.CaptureCenter (MakeSample (-0.2f));

            Assert::IsTrue   (calibration.mode == CalibrationMode::Automatic);
            Assert::AreEqual (-0.2f, calibration.axes[0].center, kTolerance, L"the center comes from the connect, not the discarded calibration");
        }


        TEST_METHOD (IsValid_AUserCenterMustSitBetweenItsLimits)
        {
            Assert::IsTrue  (ControllerCalibration::IsValid ({ 0.0f, -1.0f, 1.0f },  CalibrationMode::User));
            Assert::IsFalse (ControllerCalibration::IsValid ({ -1.0f, -1.0f, 1.0f }, CalibrationMode::User), L"a center on its limit");
            Assert::IsFalse (ControllerCalibration::IsValid ({ 0.0f, 0.5f, -0.5f },  CalibrationMode::User), L"limits out of order");
            Assert::IsFalse (ControllerCalibration::IsValid ({ 0.0f, -2.0f, 1.0f },  CalibrationMode::User), L"a limit past the rail");
            Assert::IsTrue  (ControllerCalibration::IsValid ({ 0.0f, 0.0f, 0.0f },   CalibrationMode::Automatic),
                L"an automatic axis that has not moved is fine: its center is recaptured");
        }


        static ControllerDeviceInfo MakeStick (const char * unitId)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"VKBsim Gladiator";
            info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                                 { ControlKind::Button, 0 }, { ControlKind::Button, 1 } };
            return info;
        }


        TEST_METHOD (Service_AnOffsetStickDrivesCenterAtRest)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    stick = MakeStick ("{A}");

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, MakeSample (0.2f, 0.2f));
            service.SetSelection (stick.unit);
            service.Tick();

            Assert::IsTrue (std::abs ((int) sink.writes.back().state.paddle[0] - 127) <= 1,
                L"the rest it connected at is center, not a paddle pushed right");
        }


        TEST_METHOD (Service_AnXboxControllerIsNeverCalibrated)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox;
            ControllerSample        sample = MakeSample (0.3f);

            xbox.unit.model.kind = ControllerKind::XInput;
            xbox.description     = L"Xbox Controller";
            xbox.xinputSlot      = 0;

            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, sample);
            service.SetSelection (xbox.unit);
            service.Tick();

            Assert::IsTrue (service.GetCalibrations().empty(), L"Xbox-class controllers are factory-calibrated (FR-018a)");
        }


        TEST_METHOD (Service_AUnitGetsOnlyItsOwnSavedCalibration)
        {
            FakeControllerBackend                         backend;
            GamePortInputMixer                            mixer;
            ControllerInputService                        service (backend, mixer);
            ControllerDeviceInfo                          first  = MakeStick ("{FIRST}");
            ControllerDeviceInfo                          second = MakeStick ("{SECOND}");
            std::map<std::string, ControllerCalibration>  saved;
            ControllerCalibration                         user;

            user.mode = CalibrationMode::User;
            user.axes.fill ({ 0.5f, 0.0f, 0.9f });
            saved[ControllerTokens::UnitToToken (first.unit)] = user;

            service.SetCalibrations (saved);
            backend.AddDevice (second);
            backend.SetSample (second.unit, MakeSample (0.0f));
            service.SetSelection (second.unit);
            service.Tick();

            Assert::IsTrue (service.GetCalibrations().at (ControllerTokens::UnitToToken (second.unit)).mode == CalibrationMode::Automatic,
                L"a second unit of the same model starts automatic");
            Assert::IsTrue (service.GetCalibrations().at (ControllerTokens::UnitToToken (first.unit)) == user,
                L"and the first unit's calibration is untouched");
        }
    };
}
