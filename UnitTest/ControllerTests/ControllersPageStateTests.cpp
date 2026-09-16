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


        TEST_METHOD (Load_OpensOnTheMachinesSelectedController)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  stick = MakeStick();

            page.Load ({ MakeXbox(), stick }, {}, {}, true, std::string(), stick.unit);

            Assert::IsTrue (page.GetSelectedIndex() == std::optional<size_t> (1), L"the selected controller, not the first attached");
            Assert::IsTrue (page.GetControllers()[1].unit == stick.unit);
        }


        TEST_METHOD (Load_WithNoSelectionOrOneNotAttached_OpensOnTheFirst)
        {
            ControllersPageState  page;

            page.Load ({ MakeXbox(), MakeStick() }, {}, {}, true);
            Assert::IsTrue (page.GetSelectedIndex() == std::optional<size_t> (0), L"nothing selected");

            page.Load ({ MakeXbox(), MakeStick() }, {}, {}, true, std::string(), MakeStick ("{GONE}").unit);
            Assert::IsTrue (page.GetSelectedIndex() == std::optional<size_t> (0), L"selected controller not attached");
        }


        TEST_METHOD (Load_OnTheSelectedController_ShowsTheActiveProfileForItsModel)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  stick = MakeStick();

            page.Load ({ MakeXbox(), stick }, MakeSavedWithSwapped(), {}, true, "Swapped", stick.unit);

            Assert::AreEqual (std::string ("Swapped"), page.GetEditedProfileName());
            Assert::IsFalse  (page.HasUnappliedProfileEdits(), L"opening on it is not an edit");
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


        TEST_METHOD (ResetProfile_RestoresTheBuiltInMappingOfOnlyTheEditedProfile)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.CreateProfile ("Game", ProfileSource::Paddles, std::string());
            page.RemoveBinding (PaddleTarget::Pb0, 0);
            page.SetDeadzone (0.6f);
            page.ResetProfile();

            Assert::IsTrue   (page.GetMapping() == DefaultMapping::For (MakeStick().unit.model, MakeStick().controls), L"the edited profile is back to the default mapping");
            Assert::AreEqual (0.6f, page.GetDeadzone(), 0.0001f, L"the deadzone belongs to the model, not the profile");

            page.RemoveBinding (PaddleTarget::Pb1, 0);
            page.SelectProfile ("Default");
            page.ResetProfile();
            page.SelectProfile ("Game");

            Assert::AreEqual (size_t (0), page.GetMapping().pb1.size(), L"resetting the Default leaves Game as it was");
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


        static std::map<std::string, ControllerModelSettings> MakeSavedWithSwapped()
        {
            std::map<std::string, ControllerModelSettings>  models;
            ControllerModelSettings                         settings;
            ControlMapping                                  swapped = DefaultMapping::For (MakeStick().unit.model, MakeStick().controls);

            swapped.pb0 = { { { ControlKind::Button, 1 } } };
            swapped.pb1 = { { { ControlKind::Button, 0 } } };

            settings.deadzone = DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput);
            settings.profiles.push_back ({ ControllerProfile::kpszDefaultName, true, DefaultMapping::For (MakeStick().unit.model, MakeStick().controls) });
            settings.profiles.push_back ({ "Swapped", false, swapped });

            models[ControllerTokens::ModelToToken (MakeStick().unit.model)] = settings;
            return models;
        }


        TEST_METHOD (OpeningOnAnActiveProfile_EditsThatProfile)
        {
            ControllersPageState  page;
            std::string           token = ControllerTokens::ModelToToken (MakeStick().unit.model);

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true, "swapped");

            Assert::AreEqual (std::string ("Swapped"), page.GetEditedProfileName(), L"the machine's active profile is the one shown, matched ignoring case");
            Assert::IsTrue   (page.GetMapping().pb0[0].control == ControlId { ControlKind::Button, 1 });
            Assert::IsFalse  (page.IsDirty(), L"opening on it is not an edit");

            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });

            Assert::AreEqual (size_t (1), page.GetModels().at (token).FindProfile ("Swapped")->mapping.pb2.size(), L"the edit lands on Swapped");
            Assert::IsTrue   (page.GetModels().at (token).FindProfile ("Default")->mapping == DefaultMapping::For (MakeStick().unit.model, MakeStick().controls),
                L"and the Default is untouched");
        }


        TEST_METHOD (AnActiveProfileTheModelLacks_EditsTheDefault)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true, "Swapped");

            Assert::AreEqual (std::string ("Default"), page.GetEditedProfileName());
            Assert::IsTrue   (page.IsEditingDefaultProfile());
            Assert::AreEqual (std::string ("Swapped"), page.GetActiveProfileName(), L"the machine's choice is kept for a controller that has it");
        }


        TEST_METHOD (CreateProfile_FromEachSource_IsPendingAndRevertedByCancel)
        {
            ControllersPageState                            page;
            std::map<std::string, ControllerModelSettings>  models;
            ControllerModelKey                              model    = MakeStick().unit.model;
            std::vector<ControlId>                          controls = MakeStick().controls;

            page.Load ({ MakeStick() }, models, {}, true);

            Assert::IsTrue  (page.CreateProfile ("From default", ProfileSource::DefaultMapping, std::string()) == ProfileEditResult::Ok);
            Assert::IsTrue  (page.GetMapping() == DefaultMapping::For (model, controls));
            Assert::AreEqual (std::string ("From default"), page.GetEditedProfileName(), L"a new profile is the one edited");

            Assert::IsTrue  (page.CreateProfile ("Paddles", ProfileSource::Paddles, std::string()) == ProfileEditResult::Ok);
            Assert::IsTrue  (page.GetMapping() == DefaultMapping::MakePaddles (model, controls));

            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
            Assert::IsTrue  (page.CreateProfile ("Copy", ProfileSource::CopyOfProfile, "Paddles") == ProfileEditResult::Ok);
            Assert::AreEqual (size_t (1), page.GetMapping().pb2.size(), L"a copy takes the source's pending edits");

            Assert::AreEqual (size_t (4), page.GetProfileNames().size());
            Assert::AreEqual (std::string ("Default"), page.GetProfileNames()[0], L"Default is listed first");
            Assert::IsTrue   (page.IsDirty());
            Assert::IsTrue   (models.empty(), L"nothing reaches the settings the page was opened with");

            page.Revert();

            Assert::AreEqual (size_t (1), page.GetProfileNames().size(), L"Cancel takes the new profiles away");
            Assert::AreEqual (std::string ("Default"), page.GetEditedProfileName());
            Assert::IsFalse  (page.IsDirty());
        }


        TEST_METHOD (RenameAndDelete_AreRefusedForTheDefault)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true);

            Assert::IsTrue  (page.RenameProfile ("Other") == ProfileEditResult::IsDefaultProfile);
            Assert::IsTrue  (page.DeleteProfile() == ProfileEditResult::IsDefaultProfile);
            Assert::IsFalse (page.IsDirty());
        }


        TEST_METHOD (DeletingTheEditedProfile_SelectsTheDefault)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true, "Swapped");

            Assert::IsTrue   (page.DeleteProfile() == ProfileEditResult::Ok);
            Assert::AreEqual (std::string ("Default"), page.GetEditedProfileName());
            Assert::AreEqual (size_t (1), page.GetProfileNames().size());
            Assert::IsTrue   (page.HasActiveProfileChanged(), L"the machine's active profile was deleted");
            Assert::AreEqual (std::string(), page.GetActiveProfileName(), L"so the Default becomes active on OK");
        }


        TEST_METHOD (NameErrors_MapFromTheEditResult)
        {
            ControllersPageState  page;
            std::wstring          label;
            std::wstring          rule;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true);

            Assert::IsTrue (page.CreateProfile ("   ", ProfileSource::DefaultMapping, std::string()) == ProfileEditResult::EmptyName);
            Assert::IsTrue (page.CreateProfile (std::string (41, 'x'), ProfileSource::DefaultMapping, std::string()) == ProfileEditResult::NameTooLong);
            Assert::IsTrue (page.CreateProfile ("default", ProfileSource::DefaultMapping, std::string()) == ProfileEditResult::DuplicateName,
                L"a model with a saved Default refuses its name ignoring case");
            Assert::IsFalse (page.IsDirty(), L"a refused name changes nothing");

            Assert::IsTrue   (ControllersPageState::TryDescribeNameError (ProfileEditResult::EmptyName, label, rule));
            Assert::AreEqual (std::wstring (L"Error: profile name is empty"), label);
            Assert::AreEqual (std::wstring (L"Profile names are 1-40 characters."), rule);

            Assert::IsTrue   (ControllersPageState::TryDescribeNameError (ProfileEditResult::NameTooLong, label, rule));
            Assert::AreEqual (std::wstring (L"Error: profile name is too long"), label);
            Assert::AreEqual (std::wstring (L"Profile names are 1-40 characters."), rule);

            Assert::IsTrue   (ControllersPageState::TryDescribeNameError (ProfileEditResult::DuplicateName, label, rule));
            Assert::AreEqual (std::wstring (L"Error: profile name is in use"), label);
            Assert::AreEqual (std::wstring (L"Each profile name for a controller must be different, ignoring capitalization."), rule);

            Assert::IsFalse  (ControllersPageState::TryDescribeNameError (ProfileEditResult::Ok, label, rule));
            Assert::IsFalse  (ControllersPageState::TryDescribeNameError (ProfileEditResult::IsDefaultProfile, label, rule));
        }


        TEST_METHOD (TheDefaultsNameIsTaken_EvenBeforeTheModelHasOneSaved)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);

            Assert::IsTrue (page.CreateProfile ("DEFAULT", ProfileSource::DefaultMapping, std::string()) == ProfileEditResult::DuplicateName);
        }


        TEST_METHOD (Rename_ToADifferentCaseOfItsOwnName_IsAllowed)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true, "Swapped");

            Assert::IsTrue   (page.RenameProfile ("SWAPPED") == ProfileEditResult::Ok);
            Assert::AreEqual (std::string ("SWAPPED"), page.GetEditedProfileName());
            Assert::AreEqual (std::string ("SWAPPED"), page.GetActiveProfileName(), L"renaming the active profile keeps it active under its new name");
            Assert::IsTrue   (page.IsDirty());
        }


        TEST_METHOD (EditsOnOneProfile_DoNotLeakIntoAnother)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.CreateProfile ("Game", ProfileSource::DefaultMapping, std::string());
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });

            page.SelectProfile ("Default");
            Assert::AreEqual (size_t (0), page.GetMapping().pb2.size(), L"the Default does not have Game's edit");

            page.SelectProfile ("game");
            Assert::AreEqual (std::string ("Game"), page.GetEditedProfileName());
            Assert::AreEqual (size_t (1), page.GetMapping().pb2.size(), L"and Game still has it");
        }


        TEST_METHOD (SwitchingWithUnappliedEdits_DiscardRestores)
        {
            ControllersPageState  page;
            std::string           token = ControllerTokens::ModelToToken (MakeStick().unit.model);

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true, "Swapped");

            Assert::IsFalse (page.HasUnappliedProfileEdits());
            page.RemoveBinding (PaddleTarget::Pb0, 0);
            Assert::IsTrue  (page.HasUnappliedProfileEdits(), L"an edit is what the prompt asks about");

            page.DiscardProfileEdits();
            page.SelectProfile ("Default");

            Assert::IsFalse  (page.HasUnappliedProfileEdits(), L"the Default has none of its own");
            Assert::AreEqual (size_t (1), page.GetModels().at (token).FindProfile ("Swapped")->mapping.pb0.size(),
                L"Discard puts back what Swapped had as committed");
        }


        TEST_METHOD (SwitchingWithUnappliedEdits_CancelKeepsTheProfileAndItsEdits)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true, "Swapped");
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });

            // Cancel on the prompt makes no call on the state at all.
            Assert::AreEqual (std::string ("Swapped"), page.GetEditedProfileName());
            Assert::IsTrue   (page.HasUnappliedProfileEdits());
            Assert::AreEqual (size_t (1), page.GetMapping().pb2.size());
        }


        TEST_METHOD (UnappliedEdits_SurviveAControllerSwitchAndBack)
        {
            ControllersPageState  page;
            std::string           token = ControllerTokens::ModelToToken (MakeStick().unit.model);

            page.Load ({ MakeStick(), MakeXbox() }, MakeSavedWithSwapped(), {}, true, "Swapped");
            page.RemoveBinding (PaddleTarget::Pb0, 0);

            page.SelectController (1);
            page.SelectController (0);

            Assert::IsTrue   (page.HasUnappliedProfileEdits(), L"coming back to the controller is not a save");

            page.DiscardProfileEdits();

            Assert::IsFalse  (page.HasUnappliedProfileEdits());
            Assert::AreEqual (size_t (1), page.GetModels().at (token).FindProfile ("Swapped")->mapping.pb0.size(),
                L"Discard puts back the committed mapping");
        }


        TEST_METHOD (UnappliedEdits_OnAnotherController_DoNotMarkThisOne)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick(), MakeXbox() }, MakeSavedWithSwapped(), {}, true, "Swapped");
            page.SelectController (1);
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 0 } });
            Assert::IsTrue  (page.HasUnappliedProfileEdits());

            page.SelectController (0);
            Assert::IsFalse (page.HasUnappliedProfileEdits());
        }


        TEST_METHOD (SavedEdits_StayAppliedAcrossAControllerSwitch)
        {
            ControllersPageState  page;
            HRESULT               hr = S_OK;

            page.Load ({ MakeStick(), MakeXbox() }, MakeSavedWithSwapped(), {}, true, "Swapped");
            page.RemoveBinding (PaddleTarget::Pb0, 0);

            hr = page.SaveProfileEdits ([] (const std::map<std::string, ControllerModelSettings> &,
                                            const std::map<std::string, ControllerCalibration> &)
            {
                return S_OK;
            });

            page.SelectController (1);
            page.SelectController (0);

            Assert::IsTrue  (SUCCEEDED (hr));
            Assert::IsFalse (page.HasUnappliedProfileEdits());
        }


        TEST_METHOD (CreatedProfile_IsUnappliedUntilSaved_AndDiscardRemovesIt)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.CreateProfile ("Game", ProfileSource::DefaultMapping, std::string());

            Assert::IsTrue  (page.HasUnappliedProfileEdits(), L"a profile not yet saved is itself the change");

            page.DiscardProfileEdits();

            Assert::IsFalse (page.HasUnappliedProfileEdits());
            Assert::IsTrue  (page.IsEditingDefaultProfile());
            Assert::IsTrue  (page.GetModels().empty(), L"nothing is left of it");
        }


        TEST_METHOD (CreatedProfile_ThatIsSaved_HasNoUnappliedEdits)
        {
            ControllersPageState  page;
            HRESULT               hr = S_OK;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.CreateProfile ("Game", ProfileSource::DefaultMapping, std::string());

            hr = page.SaveProfileEdits ([] (const std::map<std::string, ControllerModelSettings> &,
                                            const std::map<std::string, ControllerCalibration> &)
            {
                return S_OK;
            });

            Assert::IsTrue  (SUCCEEDED (hr));
            Assert::IsFalse (page.HasUnappliedProfileEdits());
        }


        TEST_METHOD (RenamedProfile_IsJudgedAgainstItsCommittedMapping)
        {
            ControllersPageState  page;
            std::string           token = ControllerTokens::ModelToToken (MakeStick().unit.model);

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true, "Swapped");
            page.RenameProfile ("Renamed");
            Assert::IsFalse  (page.HasUnappliedProfileEdits(), L"a rename alone is not a mapping edit");

            page.RemoveBinding (PaddleTarget::Pb0, 0);
            Assert::IsTrue   (page.HasUnappliedProfileEdits());

            page.DiscardProfileEdits();
            Assert::AreEqual (size_t (1), page.GetModels().at (token).FindProfile ("Renamed")->mapping.pb0.size());
        }


        TEST_METHOD (SaveProfileEdits_CommitsTheModelAndSurvivesRevert)
        {
            ControllersPageState                            page;
            ControllerDeviceInfo                            xbox       = MakeXbox();
            std::string                                     token      = ControllerTokens::ModelToToken (MakeStick().unit.model);
            std::string                                     xboxToken  = ControllerTokens::ModelToToken (xbox.unit.model);
            std::map<std::string, ControllerModelSettings>  committed;
            std::map<std::string, ControllerCalibration>    calibrated;
            int                                             calls      = 0;
            HRESULT                                         hr         = S_OK;

            page.Load ({ MakeStick(), xbox }, MakeSavedWithSwapped(), {}, true);
            page.SelectController (1);
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 0 } });
            page.SelectController (0);
            page.SelectProfile ("Swapped");
            page.RemoveBinding (PaddleTarget::Pb0, 0);
            page.BeginCalibration();
            page.AdvanceCalibration();
            page.AdvanceCalibration();

            hr = page.SaveProfileEdits ([&] (const std::map<std::string, ControllerModelSettings> & models,
                                             const std::map<std::string, ControllerCalibration>   & calibrations)
            {
                committed  = models;
                calibrated = calibrations;
                calls++;
                return S_OK;
            });

            Assert::IsTrue   (SUCCEEDED (hr));
            Assert::AreEqual (1, calls);
            Assert::AreEqual (size_t (0), committed.at (token).FindProfile ("Swapped")->mapping.pb0.size(), L"the saved edit is committed");
            Assert::IsTrue   (committed.find (xboxToken) == committed.end(), L"another model's pending edits are not");
            Assert::IsTrue   (calibrated.empty(), L"nor is a pending calibration");
            Assert::IsFalse  (page.HasUnappliedProfileEdits(), L"nothing left to ask about");
            Assert::IsTrue   (page.HasActiveProfileChanged(), L"saving did not make Swapped the active profile; OK does that");

            page.SelectProfile ("Default");
            Assert::IsTrue   (page.IsDirty(), L"the other model and the calibration still wait for OK");

            page.Revert();

            Assert::AreEqual (size_t (0), page.GetModels().at (token).FindProfile ("Swapped")->mapping.pb0.size(), L"Cancel keeps the save");
            Assert::IsTrue   (page.GetModels().find (xboxToken) == page.GetModels().end(), L"and reverts what was not saved");
            Assert::IsTrue   (page.GetCalibrations().empty());
            Assert::IsFalse  (page.IsDirty());
        }


        TEST_METHOD (SaveProfileEdits_DoesNotChangeTheActiveProfile)
        {
            ControllersPageState  page;
            HRESULT               hr = S_OK;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true);
            page.SelectProfile ("Swapped");
            page.RemoveBinding (PaddleTarget::Pb0, 0);

            hr = page.SaveProfileEdits ([] (const std::map<std::string, ControllerModelSettings> &,
                                            const std::map<std::string, ControllerCalibration> &)
            {
                return S_OK;
            });

            Assert::IsTrue  (SUCCEEDED (hr));
            Assert::IsTrue  (page.IsDirty(), L"only the switch to Swapped is left for OK");
            page.SelectProfile ("Default");
            Assert::IsFalse (page.IsDirty(), L"switching back leaves the save as the only change, already committed");
        }


        TEST_METHOD (SaveProfileEdits_ThatFails_LeavesTheEditsPending)
        {
            ControllersPageState  page;
            HRESULT               hr = S_OK;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true, "Swapped");
            page.RemoveBinding (PaddleTarget::Pb0, 0);

            hr = page.SaveProfileEdits ([] (const std::map<std::string, ControllerModelSettings> &,
                                            const std::map<std::string, ControllerCalibration> &)
            {
                return E_ACCESSDENIED;
            });

            Assert::IsTrue  (FAILED (hr));
            Assert::IsTrue  (page.HasUnappliedProfileEdits());
            Assert::IsTrue  (page.IsDirty());
            page.Revert();
            Assert::IsFalse (page.IsDirty());
        }


        TEST_METHOD (DiscardOnAModelNeverSaved_LeavesNothingPending)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
            page.DiscardProfileEdits();

            Assert::IsFalse (page.IsDirty());
            Assert::IsTrue  (page.GetModels().empty());
        }


        TEST_METHOD (IsDirty_CoversCreateRenameAndDelete)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true);
            page.CreateProfile ("Game", ProfileSource::DefaultMapping, std::string());
            Assert::IsTrue (page.IsDirty(), L"create");
            page.MarkCommitted();
            Assert::IsFalse (page.IsDirty());

            page.RenameProfile ("Game 2");
            Assert::IsTrue (page.IsDirty(), L"rename");
            page.Revert();
            Assert::AreEqual (std::string ("Game"), page.GetEditedProfileName(), L"Cancel puts the name back");

            page.DeleteProfile();
            Assert::IsTrue (page.IsDirty(), L"delete");
            page.Revert();
            Assert::AreEqual (size_t (3), page.GetProfileNames().size(), L"and Cancel brings a deleted profile back");
        }


        TEST_METHOD (TheChosenProfile_IsTheActiveProfileCommitted)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true);
            Assert::IsFalse (page.HasActiveProfileChanged());

            page.SelectProfile ("Swapped");
            Assert::IsTrue   (page.HasActiveProfileChanged(), L"choosing a profile is a change OK commits");
            Assert::IsTrue   (page.IsDirty());
            Assert::AreEqual (std::string ("Swapped"), page.GetActiveProfileName());

            page.MarkCommitted();
            Assert::IsFalse  (page.HasActiveProfileChanged());

            page.SelectProfile ("Default");
            Assert::AreEqual (std::string(), page.GetActiveProfileName(), L"the Default is committed as no profile name");
        }


        // Two players, each on their own joystick of a four-axis machine.
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


        TEST_METHOD (InPlay_WithTheModeOff_EverythingIsInPlay)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);

            // Single-source mode is what a machine has always done: the one
            // controller drives both paddles and all three buttons.
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pdl1));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pb0));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pb1));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pb2));
        }


        TEST_METHOD (InPlay_AJoystickSlot_PlaysBothPaddlesAndOneButton)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (MakeTwoPlayers (first.unit, second.unit), 4);
            page.SelectController (0);

            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl0), L"a joystick is two paddles wired to one stick");
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl1));
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0),  L"and the player's own button line comes off PB0");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb1),  L"PB1 belongs to the other player while the mode is on");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb2),  L"and PB2 is unused (FR-039)");
        }


        TEST_METHOD (InPlay_ASinglePaddleSlot_PlaysOnePaddleAndOneButton)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            MultiplayerSetup      setup  = MakeTwoPlayers (first.unit, second.unit);

            setup.players[0].target = PlayerAxisTarget::Paddle2;

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (setup, 4);
            page.SelectController (0);

            // The slot's paddles take the mapping's targets in ascending
            // order, so one paddle plays the controller's PDL0 alone.
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pdl1), L"there is no second paddle for PDL1 to land on");
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0));
        }


        TEST_METHOD (InPlay_PlayerTwo_DrivesTheirLineFromTheirOwnPb0)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (MakeTwoPlayers (first.unit, second.unit), 4);
            page.SelectController (1);

            Assert::IsTrue  (page.FindEditedPlayer() == std::optional<size_t> (1));
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0),
                L"player two's PB0 bindings drive PB1, so PB0 is the row they edit");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb1),
                L"their own PB1 bindings are kept and ignored while the mode is on");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb2));
        }


        TEST_METHOD (InPlay_AControllerInNeitherSlot_PlaysNothing)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            ControllerDeviceInfo  spare  = MakeStick ("{C}");

            page.Load ({ first, second, spare }, {}, {}, true);
            page.SetMultiplayer (MakeTwoPlayers (first.unit, second.unit), 4);
            page.SelectController (2);

            Assert::IsTrue  (!page.FindEditedPlayer().has_value());
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pdl1));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb0));
        }


        TEST_METHOD (InPlay_ASlotThisMachineCannotPlay_IsNotInPlay)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (MakeTwoPlayers (first.unit, second.unit), 2);
            page.SelectController (1);

            // Player two is on joystick 1, which is PDL2/PDL3: a //c has
            // neither, so they are not read at all (FR-035).
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb0),
                L"neither their paddles nor their button reach a two-axis machine");

            page.SelectController (0);
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pdl0), L"while player one, on joystick 0, plays as usual");
        }


        TEST_METHOD (TargetChoices_LeaveOutTheOtherPlayersClaimAndTheMissingPaddles)
        {
            ControllersPageState           page;
            ControllerDeviceInfo           first   = MakeStick ("{A}");
            ControllerDeviceInfo           second  = MakeStick ("{B}");
            std::vector<PlayerAxisTarget>  choices;

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (MakeTwoPlayers (first.unit, second.unit), 2);

            choices = page.GetTargetChoices (1);

            Assert::AreEqual (size_t (0), (size_t) std::count (choices.begin(), choices.end(), PlayerAxisTarget::Joystick1),
                L"a //c has no PDL2/PDL3, so joystick 1 is not offered");
            Assert::AreEqual (size_t (0), (size_t) std::count (choices.begin(), choices.end(), PlayerAxisTarget::Joystick0),
                L"nor is what player one already holds (FR-036)");
            Assert::AreEqual (size_t (0), choices.size(),
                L"player one on joystick 0 leaves a two-axis machine with nothing for player two");
        }


        TEST_METHOD (MultiplayerEdits_ApplyAtOnceAndAreNotUndoneByCancel)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first   = MakeStick ("{A}");
            ControllerDeviceInfo  second  = MakeStick ("{B}");
            MultiplayerSetup      applied;
            int                   changes = 0;

            page.Load ({ first, second }, {}, {}, true);
            page.SetOnMultiplayerChanged ([&] (const MultiplayerSetup & setup)
            {
                applied = setup;
                changes++;
            });

            page.SetMultiplayer (MakeTwoPlayers (first.unit, second.unit), 4);
            page.SetMultiplayerTarget (1, PlayerAxisTarget::Paddle2);

            Assert::AreEqual (1, changes, L"the slot reaches the service as it is edited");
            Assert::IsTrue   (applied.players[1].target == PlayerAxisTarget::Paddle2);

            // The mode is a machine input setting, like the toolbar picker's
            // choice, not a profile edit the sheet commits.
            Assert::IsFalse (page.IsDirty(), L"so it is no part of what OK commits");

            page.Revert();

            Assert::IsTrue (page.GetMultiplayer().players[1].target == PlayerAxisTarget::Paddle2,
                L"and Cancel does not take back what already took effect");
        }


        TEST_METHOD (MultiplayerSlots_ASecondSlotRepeatingTheFirst_IsEmptied)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (MakeTwoPlayers (first.unit, second.unit), 4);
            page.SetMultiplayerUnit (1, first.unit);

            Assert::IsFalse (page.GetMultiplayer().players[1].unit.has_value(),
                L"two people cannot share one controller, so the later slot gives way");

            page.SetMultiplayerUnit (1, second.unit);
            page.SetMultiplayerTarget (1, PlayerAxisTarget::Paddle0);

            Assert::IsFalse (page.GetMultiplayer().players[1].unit.has_value(),
                L"nor can they claim a paddle player one already holds (FR-036)");
        }


        TEST_METHOD (MultiplayerRows_AreNamedByThePaddleThePlayerDrives)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            MultiplayerSetup      setup;

            setup.isEnabled         = true;
            setup.players[0].unit   = first.unit;
            setup.players[0].target = PlayerAxisTarget::Joystick0;
            setup.players[1].unit   = second.unit;
            setup.players[1].target = PlayerAxisTarget::Paddle2;

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (setup, 4);

            // Player one holds a joystick: two paddles, named for what they
            // drive rather than for the controller's own targets.
            page.SelectController (0);
            Assert::AreEqual (std::wstring (L"PDL0:"), page.GetTargetPlayLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L"PDL1:"), page.GetTargetPlayLabel (PaddleTarget::Pdl1));
            Assert::AreEqual (std::wstring (L"PB0:"),  page.GetTargetPlayLabel (PaddleTarget::Pb0));

            // Player two holds ONE paddle, so their second axis row drives
            // nothing and has nothing to be named after.
            page.SelectController (1);
            Assert::AreEqual (std::wstring (L"PDL2:"), page.GetTargetPlayLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L""),      page.GetTargetPlayLabel (PaddleTarget::Pdl1));
            Assert::AreEqual (std::wstring (L"PB1:"),  page.GetTargetPlayLabel (PaddleTarget::Pb0));
            Assert::IsFalse  (page.IsTargetInPlay (PaddleTarget::Pdl1), L"so its row is grayed and reads nothing live");
            Assert::IsFalse  (page.IsTargetInPlay (PaddleTarget::Pb1),  L"and only their own button line is in play");
        }


        TEST_METHOD (MultiplayerSlots_FillingASlotMovesItOffAPaddleTheOtherHolds)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            MultiplayerSetup      setup;

            // Both slots start on joystick 0, which is what a user meets the
            // first time: picking a controller for player two must give them
            // somewhere to play rather than being refused as an overlap.
            setup.isEnabled       = true;
            setup.players[0].unit = first.unit;

            page.Load ({ first, second }, {}, {}, true);
            page.SetMultiplayer (setup, 4);
            page.SetMultiplayerUnit (1, second.unit);

            Assert::IsTrue (page.GetMultiplayer().players[1].unit.has_value(), L"the pick sticks");
            Assert::IsTrue (page.GetMultiplayer().players[1].unit.value() == second.unit);
            Assert::IsFalse (page.GetMultiplayer().players[1].target == page.GetMultiplayer().players[0].target,
                L"on a free target rather than the one player one already holds");
        }
    };
}
