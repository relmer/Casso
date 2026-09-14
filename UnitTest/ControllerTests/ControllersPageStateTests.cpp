#include "Pch.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"
#include "Ui/Settings/ControllersPageState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPageStateTests
//
//  The Controllers page with no window: what the user edits, and what OK and
//  Cancel do with it.
//
//  THE RULE THAT MATTERS MOST IS THAT NOTHING IS APPLIED UNTIL OK. The page
//  edits copies, and Cancel puts every one back, calibrations included; a
//  user trying a mapping out must not find it saved because they looked.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControllersPageStateTests)
    {
    public:

        static ControllerDeviceInfo MakeStick (const char * unitId = "{STICK}")
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"VKBsim Gladiator";
            info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                                 { ControlKind::Button, 0 }, { ControlKind::Button, 1 }, { ControlKind::Button, 2 } };
            return info;
        }


        static ControllerDeviceInfo MakeXbox()
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.description     = L"Xbox Controller";
            info.xinputSlot      = 0;
            info.controls        = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Button, 0 } };
            return info;
        }


        static ControllerSample Rest()
        {
            ControllerSample  sample;

            sample.connected = true;
            return sample;
        }


        TEST_METHOD (Edits_StayPendingAndTheMapsHandedInAreUntouched)
        {
            ControllersPageState                            page;
            std::map<std::string, ControllerModelSettings>  models;
            std::map<std::string, ControllerCalibration>    calibrations;

            page.Load ({ MakeStick() }, models, calibrations, true);
            page.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, 2 } });
            page.SetDeadzone (0.3f);

            Assert::IsTrue (page.IsDirty(), L"the page knows it has changes");
            Assert::IsTrue (models.empty(), L"but the settings it was opened with are copies, so nothing is applied yet");
        }


        TEST_METHOD (OpeningAndClosingWithoutEdits_IsNotDirty)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);

            Assert::IsFalse (page.IsDirty(), L"looking at a controller writes nothing new");
            Assert::IsTrue  (page.GetMapping() == DefaultMapping::For (MakeStick().unit.model, MakeStick().controls),
                L"and a model never edited shows its built-in mapping");
        }


        TEST_METHOD (Cancel_RestoresEverythingIncludingACalibration)
        {
            ControllersPageState                            page;
            std::map<std::string, ControllerCalibration>    saved;
            ControllerCalibration                           user;
            std::string                                     token = ControllerTokens::UnitToToken (MakeStick().unit);

            user.mode = CalibrationMode::User;
            user.axes.fill ({ 0.0f, -1.0f, 1.0f });
            saved[token] = user;

            page.Load ({ MakeStick() }, {}, saved, true);
            page.SetDeadzone (0.5f);
            page.UseAutomaticCalibration();
            page.Revert();

            Assert::IsFalse (page.IsDirty());
            Assert::IsTrue  (page.GetCalibrations().at (token) == user, L"a calibration discarded on the page comes back on Cancel");
            Assert::AreEqual (DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput), page.GetDeadzone(), 0.0001f);
        }


        TEST_METHOD (Cancel_UndoesACapturedCalibration)
        {
            ControllersPageState  page;
            ControllerSample      sample = Rest();

            page.Load ({ MakeStick() }, {}, {}, true);
            page.BeginCalibration();
            page.FeedCalibration (sample);
            page.AdvanceCalibration();
            sample.axes[0] = 0.8f;
            page.FeedCalibration (sample);
            page.AdvanceCalibration();

            Assert::IsFalse (page.GetCalibrations().empty(), L"the calibration is pending");

            page.Revert();

            Assert::IsTrue (page.GetCalibrations().empty(), L"and Cancel takes it away");
        }


        TEST_METHOD (Calibrate_CentersThenMeasuresTravel)
        {
            ControllersPageState  page;
            ControllerSample      sample = Rest();
            std::string           token  = ControllerTokens::UnitToToken (MakeStick().unit);

            page.Load ({ MakeStick() }, {}, {}, true);
            page.BeginCalibration();
            Assert::IsTrue ((int) page.GetCalibrationStep() == (int) CalibrationStep::Center);

            sample.axes[0] = 0.1f;
            page.FeedCalibration (sample);
            page.AdvanceCalibration();
            Assert::IsTrue ((int) page.GetCalibrationStep() == (int) CalibrationStep::Travel);

            sample.axes[0] = 0.9f;
            page.FeedCalibration (sample);
            sample.axes[0] = -0.7f;
            page.FeedCalibration (sample);
            page.AdvanceCalibration();

            const ControllerCalibration &  measured = page.GetCalibrations().at (token);

            Assert::IsTrue   (measured.mode == CalibrationMode::User);
            Assert::AreEqual ( 0.1f, measured.axes[0].center,  0.0001f, L"the rest reading is the center");
            Assert::AreEqual ( 0.9f, measured.axes[0].maximum, 0.0001f, L"and the travel shown is the range");
            Assert::AreEqual (-0.7f, measured.axes[0].minimum, 0.0001f);
            Assert::IsTrue   (ControllerCalibration::IsValid (measured.axes[3], CalibrationMode::User),
                L"an axis the user did not move still yields a calibration that can be saved");
        }


        TEST_METHOD (Calibrate_IsNotOfferedForAnXboxController)
        {
            ControllersPageState  page;

            page.Load ({ MakeXbox() }, {}, {}, true);
            page.BeginCalibration();

            Assert::IsFalse (page.IsCalibratable());
            Assert::IsTrue  ((int) page.GetCalibrationStep() == (int) CalibrationStep::None);
        }


        TEST_METHOD (SharedControls_AreReported)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.AddButtonBinding (PaddleTarget::Pb1, { { ControlKind::Button, 0 } });   // button 0 is already PB0

            std::vector<ControlId>  shared = page.GetSharedControls();

            Assert::AreEqual (size_t (1), shared.size(), L"a control on two targets is reported (FR-025)");
            Assert::IsTrue   (shared[0] == ControlId { ControlKind::Button, 0 });
        }


        TEST_METHOD (AddingABinding_NeverRemovesAnother)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.AddButtonBinding (PaddleTarget::Pb1, { { ControlKind::Button, 0 } });

            Assert::AreEqual (size_t (1), page.GetMapping().pb0.size(), L"button 0 is still PB0");
            Assert::AreEqual (size_t (2), page.GetMapping().pb1.size(), L"and PB1 has both its buttons");
        }


        TEST_METHOD (ACaptureAddsTheActivatedControl)
        {
            ControllersPageState  page;
            ControllerSample      pressed = Rest();

            page.Load ({ MakeStick() }, {}, {}, true);
            page.BeginCapture (PaddleTarget::Pb0, Rest());
            pressed.buttons.set (2);

            Assert::IsTrue   (page.FeedCapture (pressed));
            Assert::IsFalse  (page.IsCapturing());
            Assert::AreEqual (size_t (2), page.GetMapping().pb0.size());
            Assert::IsTrue   (page.GetMapping().pb0.back().control == ControlId { ControlKind::Button, 2 });
        }


        TEST_METHOD (ADpadCaptureOnAnAxis_TakesBothDirections)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  stick   = MakeStick();
            ControllerSample      pressed = Rest();
            AxisBinding           binding;

            stick.controls.push_back ({ ControlKind::DpadLeft,  0 });
            stick.controls.push_back ({ ControlKind::DpadRight, 0 });
            page.Load ({ stick }, {}, {}, true);
            page.BeginCapture (PaddleTarget::Pdl0, Rest(), 0);

            pressed.buttons.set (1);
            Assert::IsFalse (page.FeedCapture (pressed), L"a plain button has no opposite, so it cannot drive an axis");

            pressed.hats[0] = ControllerSample::kHatRight;
            Assert::IsTrue  (page.FeedCapture (pressed));

            binding = page.GetMapping().pdl0[0];
            Assert::IsTrue (binding.kind == AxisBindingKind::DigitalPair);
            Assert::IsTrue (binding.negative == ControlId { ControlKind::DpadLeft,  0 }, L"pressing right still takes left as the low end");
            Assert::IsTrue (binding.positive == ControlId { ControlKind::DpadRight, 0 });
        }


        TEST_METHOD (AControllerUnpluggedWhileOpen_KeepsItsEditsAndShowsDisconnected)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.SetDeadzone (0.4f);
            page.UpdateDevices ({});

            Assert::AreEqual (size_t (1), page.GetControllers().size(), L"its row stays");
            Assert::IsFalse  (page.GetControllers()[0].isConnected,     L"marked not connected");
            Assert::AreEqual (0.4f, page.GetDeadzone(), 0.0001f,        L"and the edit is still there");

            page.UpdateDevices ({ MakeStick() });
            Assert::IsTrue (page.GetControllers()[0].isConnected, L"plugged back in, it is the same row");
        }


        TEST_METHOD (ResetToDefaults_RestoresTheBuiltInMappingAndDeadzone)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.RemoveBinding (PaddleTarget::Pb0, 0);
            page.SetDeadzone (0.6f);
            page.ResetToDefaults();

            Assert::IsTrue   (page.GetMapping() == DefaultMapping::For (MakeStick().unit.model, MakeStick().controls));
            Assert::AreEqual (DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput), page.GetDeadzone(), 0.0001f);
        }


        TEST_METHOD (Pb2_IsUnavailableOnAMachineWithoutIt)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, false);

            Assert::IsFalse (page.IsTargetAvailable (PaddleTarget::Pb2), L"the //c's $C063 is the mouse button");
            Assert::IsFalse (page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } }),
                L"so nothing can be bound to it there");
            Assert::IsTrue  (page.IsTargetAvailable (PaddleTarget::Pb1));
        }


        TEST_METHOD (LiveReading_UsesThePendingMapping)
        {
            ControllersPageState  page;
            ControllerSample      pressed = Rest();

            page.Load ({ MakeStick() }, {}, {}, true);
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
            pressed.buttons.set (2);

            Assert::IsTrue (page.ComputeLiveReading (pressed).buttons.test (2),
                L"the readout shows what the edit will do before OK");
        }


        TEST_METHOD (MarkCommitted_MakesTheEditsTheNewBaseline)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.SetDeadzone (0.3f);
            page.MarkCommitted();
            page.SetDeadzone (0.5f);
            page.Revert();

            Assert::AreEqual (0.3f, page.GetDeadzone(), 0.0001f, L"Cancel after an Apply goes back to what was applied");
        }


        TEST_METHOD (ReplacingARow_KeepsTheTargetsOtherControlsInOrder)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, 1 } });
            page.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, 2 } });

            Assert::IsTrue (page.ReplaceButtonBinding (PaddleTarget::Pb0, 1, { { ControlKind::Button, 4 } }));

            Assert::AreEqual (size_t (3), page.GetMapping().pb0.size());
            Assert::IsTrue   (page.GetMapping().pb0[0].control == ControlId { ControlKind::Button, 0 });
            Assert::IsTrue   (page.GetMapping().pb0[1].control == ControlId { ControlKind::Button, 4 }, L"the second row takes the new control");
            Assert::IsTrue   (page.GetMapping().pb0[2].control == ControlId { ControlKind::Button, 2 }, L"and the third keeps its place");
            Assert::IsFalse  (page.ReplaceButtonBinding (PaddleTarget::Pb0, 9, { { ControlKind::Button, 4 } }), L"a row that does not exist is refused");
        }


        TEST_METHOD (ACaptureOnARow_ReplacesThatRow)
        {
            ControllersPageState  page;
            ControllerSample      pressed = Rest();

            page.Load ({ MakeStick() }, {}, {}, true);
            page.BeginCapture (PaddleTarget::Pb0, Rest(), 0);
            pressed.buttons.set (2);
            page.FeedCapture (pressed);

            Assert::AreEqual (size_t (1), page.GetMapping().pb0.size(), L"the row's control is replaced, not added to");
            Assert::IsTrue   (page.GetMapping().pb0[0].control == ControlId { ControlKind::Button, 2 });
        }


        TEST_METHOD (ReplacingAnAxisRow_CanTurnItIntoAPair)
        {
            ControllersPageState  page;
            AxisBinding           pair;

            pair.kind     = AxisBindingKind::DigitalPair;
            pair.negative = { ControlKind::DpadLeft, 0 };
            pair.positive = { ControlKind::DpadRight, 0 };

            page.Load ({ MakeStick() }, {}, {}, true);

            Assert::IsTrue (page.ReplaceAxisBinding (PaddleTarget::Pdl0, 0, pair));
            Assert::IsTrue (page.GetMapping().pdl0[0] == pair);
        }
    };
}
