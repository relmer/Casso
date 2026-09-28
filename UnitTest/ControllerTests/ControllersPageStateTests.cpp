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


        //  The controller in Editing as Player 1, alone, in Paddle mode, so
        //  the page lists and edits Paddle profiles.
        static void SetPaddlePlayer (ControllersPageState & page)
        {
            PlayerEntries  entries;
            PlayerSlots    slots;

            entries[0].mode = PlayerMode::Paddle;
            slots[0].state  = PlayerSlotState::Playing;
            slots[0].holder = page.GetControllers()[page.GetSelectedIndex().value()].unit;
            slots[0].target = PlayerAxisTarget::Paddle0;
            page.SetPlayers (entries, slots, GamePortContribution::kAxisCount);
        }


        //  The controller in Editing as Player 1, alone, in the Joyport's left
        //  jack on a machine that has one, so the page lists and edits
        //  Joyport profiles; or back in Joystick mode.
        static void SetJoyportPlayer (ControllersPageState & page, bool isInJoyport)
        {
            PlayerEntries  entries;
            PlayerSlots    slots;

            entries[0].mode = isInJoyport ? PlayerMode::JoyportLeft : PlayerMode::Joystick;
            entries[1].mode = PlayerMode::SameAsPlayer1;
            slots[0].state  = PlayerSlotState::Playing;
            slots[0].holder = page.GetControllers()[page.GetSelectedIndex().value()].unit;
            page.SetJoyportAvailable (true);
            page.SetPlayers (entries, slots, GamePortContribution::kAxisCount);
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



        //  How the page is opened on one controller: New... in a player's
        //  profile section in the picker is for that player's controller.
        TEST_METHOD (FindController_GivesTheRowOfAnAttachedUnitOnly)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  stick = MakeStick();



            page.Load ({ MakeXbox(), stick }, {}, {}, true);

            Assert::IsTrue  (page.FindController (stick.unit)      == std::optional<size_t> (1));
            Assert::IsTrue  (page.FindController (MakeXbox().unit) == std::optional<size_t> (0));
            Assert::IsFalse (page.FindController (MakeStick ("{GONE}").unit).has_value(), L"a controller the page does not list");
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


        //  In a Joyport jack PB1 is not shown and plays nothing, so a control
        //  on fire and on PB1 is not reported. On a joystick PB1 shows, and
        //  the same pair is.
        TEST_METHOD (SharedControls_CountOnlyTheTargetsInPlay)
        {
            ControllersPageState  jack;
            ControllersPageState  joystick;
            ControlId             button0 = { ControlKind::Button, 0 };



            jack.Load ({ MakeStick() }, {}, {}, true);
            SetJoyportPlayer (jack, true);
            jack.AddButtonBinding (PaddleTarget::Pb1, { button0 });   // button 0 already fires

            joystick.Load ({ MakeStick() }, {}, {}, true);
            SetJoyportPlayer (joystick, false);
            joystick.AddButtonBinding (PaddleTarget::Pb1, { button0 });   // button 0 is already PB0

            Assert::IsTrue   (jack.GetSharedControls().empty(), L"PB1 is out of play in a jack");
            Assert::IsTrue   (jack.GetControlTargets (button0) == std::vector<PaddleTarget> { PaddleTarget::Pb0 });
            Assert::AreEqual (size_t (1), joystick.GetSharedControls().size(), L"PB1 shows on a joystick");
            Assert::IsTrue   (joystick.GetSharedControls()[0] == button0);
        }


        TEST_METHOD (ControlTargets_ListEachTargetOnceInRowOrder)
        {
            ControllersPageState       page;
            std::vector<PaddleTarget>  targets;



            page.Load ({ MakeStick() }, {}, {}, true);
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 0 } });
            page.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, 0 } });   // a second PB0 row
            page.AddButtonBinding (PaddleTarget::Pb1, { { ControlKind::Button, 0 } });

            targets = page.GetControlTargets ({ ControlKind::Button, 0 });

            Assert::IsTrue (targets == std::vector<PaddleTarget> { PaddleTarget::Pb0, PaddleTarget::Pb1, PaddleTarget::Pb2 });
        }


        TEST_METHOD (JoinWithAnd_JoinsOneTwoAndThreeItems)
        {
            Assert::AreEqual (std::wstring (L""),                         ControllersPageState::JoinWithAnd ({}));
            Assert::AreEqual (std::wstring (L"Fire"),                     ControllersPageState::JoinWithAnd ({ L"Fire" }));
            Assert::AreEqual (std::wstring (L"Fire and PB1"),             ControllersPageState::JoinWithAnd ({ L"Fire", L"PB1" }));
            Assert::AreEqual (std::wstring (L"PDL0 (X), Fire and PB1"),   ControllersPageState::JoinWithAnd ({ L"PDL0 (X)", L"Fire", L"PB1" }));
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
            page.CreateProfile ("Game", ProfileSource::DefaultMapping, std::string());
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


        //  A binding drop-down with nothing it can assign says why: no
        //  controller is attached, or the machine lacks the target. The
        //  machine's lack is true whatever is attached, so it comes first.
        TEST_METHOD (UnassignedLabel_SaysWhyNothingCanBeAssigned)
        {
            ControllersPageState  page;



            page.SetMachineName (L"Apple //c");
            page.Load ({}, {}, {}, false);
            Assert::AreEqual (std::wstring (L"No controller attached"),    page.GetUnassignedLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L"No controller attached"),    page.GetUnassignedLabel (PaddleTarget::Pb0));
            Assert::AreEqual (std::wstring (L"Not supported on Apple //c"), page.GetUnassignedLabel (PaddleTarget::Pb2));

            page.Load ({ MakeStick() }, {}, {}, false);
            Assert::AreEqual (std::wstring (L"None"),                      page.GetUnassignedLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L"Not supported on Apple //c"), page.GetUnassignedLabel (PaddleTarget::Pb2));

            page.SetMachineName (L"");
            Assert::AreEqual (std::wstring (L"Not supported on this machine"), page.GetUnassignedLabel (PaddleTarget::Pb2));

            page.Load ({}, {}, {}, true);
            Assert::AreEqual (std::wstring (L"No controller attached"),    page.GetUnassignedLabel (PaddleTarget::Pb2), L"a machine with PB2 and nothing attached");
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
            settings.profiles.push_back ({ ControllerProfile::kpszDefaultName, ControllerProfileKind::Default, DefaultMapping::For (MakeStick().unit.model, MakeStick().controls) });
            settings.profiles.push_back ({ "Swapped", ControllerProfileKind::User, swapped });

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
            Assert::IsTrue   (page.IsEditingBuiltInProfile());
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

            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
            Assert::IsTrue  (page.CreateProfile ("Copy", ProfileSource::CopyOfProfile, "From default") == ProfileEditResult::Ok);
            Assert::AreEqual (size_t (1), page.GetMapping().pb2.size(), L"a copy takes the source's pending edits");

            Assert::IsTrue   (page.GetProfileNames() == std::vector<std::string> { "Default", "From default", "Copy" },
                              L"the Default first, then the new Joystick profiles, and no Paddle or Joyport profile");

            SetPaddlePlayer (page);

            Assert::IsTrue  (page.CreateProfile ("Pong", ProfileSource::PaddleMapping, std::string()) == ProfileEditResult::Ok);
            Assert::IsTrue  (page.GetMapping() == DefaultMapping::MakePaddles (model, controls), L"the Paddles mapping, for a Paddle profile");
            Assert::IsTrue  (page.GetModels().begin()->second.FindProfile ("Pong")->mode == ProfileMode::Paddle);
            Assert::IsTrue  (page.GetProfileNames() == std::vector<std::string> { "Paddles", "Pong" }, L"Paddles first, then the Paddle profiles");
            Assert::IsTrue  (page.IsDirty());
            Assert::IsTrue  (models.empty(), L"nothing reaches the settings the page was opened with");

            page.Revert();

            Assert::IsTrue   (page.GetProfileNames() == std::vector<std::string> { "Paddles" }, L"Cancel takes the new profiles away");
            Assert::AreEqual (std::string ("Paddles"), page.GetEditedProfileName());
            Assert::IsFalse  (page.IsDirty());
        }


        //  For a player in a jack, a controller with no profile chosen edits
        //  the Joyport profile. Choosing it records no choice, and the Default,
        //  a normal-mode profile, cannot be chosen. Reset puts the Joyport
        //  profile back to its own mapping, and it can be neither renamed nor
        //  deleted.
        TEST_METHOD (WithAJoyport_TheUnchosenProfileIsTheJoyportProfile)
        {
            ControllersPageState    page;
            ControllerModelKey      model    = MakeStick().unit.model;
            std::vector<ControlId>  controls = MakeStick().controls;

            page.Load ({ MakeStick() }, {}, {}, true);
            SetJoyportPlayer (page, true);

            Assert::AreEqual (std::string ("Joyport"), page.GetEditedProfileName());
            Assert::IsTrue   (page.GetMapping() == DefaultMapping::MakeJoyport (model, ControllerFormFactor::Gamepad, controls));
            Assert::IsTrue   (page.IsEditingBuiltInProfile());

            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
            page.ResetProfile();

            Assert::IsTrue   (page.GetMapping() == DefaultMapping::MakeJoyport (model, ControllerFormFactor::Gamepad, controls), L"Reset restores the Joyport mapping");
            Assert::IsTrue   (page.RenameProfile ("Atari") == ProfileEditResult::IsBuiltInProfile);
            Assert::IsTrue   (page.DeleteProfile() == ProfileEditResult::IsBuiltInProfile);

            page.SelectProfile ("Default");
            Assert::AreEqual (std::string(), page.GetActiveProfileName(), L"the Default is not chosen in a jack");
            Assert::AreEqual (std::string ("Joyport"), page.GetEditedProfileName());

            page.SelectProfile ("Joyport");
            Assert::AreEqual (std::string(), page.GetActiveProfileName(), L"the Joyport profile is recorded as no choice");
        }


        // The stick's model with both built-in profiles, the normal-mode
        // "Swapped" and the Joyport-mode "Atari", each binding a button no
        // built-in profile binds.
        static std::map<std::string, ControllerModelSettings> MakeSavedWithBothModes()
        {
            std::map<std::string, ControllerModelSettings>  models   = MakeSavedWithSwapped();
            ControllerModelSettings                       & settings = models.begin()->second;
            ControlMapping                                  atari;

            atari.pb0 = { { { ControlKind::Button, 2 } } };

            settings.EnsureBuiltInProfiles (MakeStick().unit.model, ControllerFormFactor::Gamepad, MakeStick().controls);
            settings.AddProfile ("Atari", atari, ProfileMode::Joyport);
            return models;
        }


        //  The page shows the Joyport profile made for the controller's form
        //  factor: a DirectInput gamepad's second stick is in it, and a
        //  joystick's same axes are not.
        TEST_METHOD (JoyportProfile_FollowsTheControllersFormFactor)
        {
            ControllersPageState  gamepadPage;
            ControllersPageState  joystickPage;
            ControllerDeviceInfo  gamepad  = MakeStick();
            ControllerDeviceInfo  joystick = MakeStick();

            gamepad.controls.push_back ({ ControlKind::Axis, DefaultMapping::kAxisZ });
            gamepad.controls.push_back ({ ControlKind::Axis, DefaultMapping::kAxisRz });
            gamepad.formFactor  = ControllerFormFactor::Gamepad;
            joystick.controls   = gamepad.controls;
            joystick.formFactor = ControllerFormFactor::Joystick;

            gamepadPage.Load ({ gamepad }, {}, {}, true);
            SetJoyportPlayer (gamepadPage, true);
            joystickPage.Load ({ joystick }, {}, {}, true);
            SetJoyportPlayer (joystickPage, true);

            Assert::IsTrue   (gamepadPage.GetControllers()[0].formFactor == ControllerFormFactor::Gamepad, L"the entry keeps the device's form factor");
            Assert::IsTrue   (gamepadPage.GetMapping() == DefaultMapping::MakeJoyport (gamepad.unit.model, ControllerFormFactor::Gamepad, gamepad.controls));
            Assert::AreEqual (size_t (2), gamepadPage.GetMapping().pdl0.size(),  L"the primary stick and Z");
            Assert::AreEqual (size_t (1), joystickPage.GetMapping().pdl0.size(), L"the primary stick alone");
        }


        TEST_METHOD (ProfileNames_FollowThePagesMode)
        {
            ControllersPageState  normal;
            ControllersPageState  joyport;

            normal.Load ({ MakeStick() }, MakeSavedWithBothModes(), {}, true);

            joyport.Load ({ MakeStick() }, MakeSavedWithBothModes(), {}, true);
            SetJoyportPlayer (joyport, true);

            Assert::IsTrue (normal.GetProfileNames()  == std::vector<std::string> { "Default", "Swapped" }, L"normal mode: the Default first, and no Joyport-mode profile");
            Assert::IsTrue (joyport.GetProfileNames() == std::vector<std::string> { "Joyport", "Atari" },   L"Joyport mode: the Joyport profile first, and no normal-mode profile");
        }


        //  A new profile is a copy only of a profile of the page's mode, and
        //  the mode's built-in profile is offered only once its mapping is no
        //  longer the built-in mapping, which a copy would only duplicate.
        TEST_METHOD (CopySources_ListThePagesModeLessAnUneditedBuiltIn)
        {
            ControllersPageState  normal;
            ControllersPageState  joyport;
            ControllersPageState  none;

            normal.Load ({ MakeStick() }, MakeSavedWithBothModes(), {}, true);

            joyport.Load ({ MakeStick() }, MakeSavedWithBothModes(), {}, true);
            SetJoyportPlayer (joyport, true);

            Assert::IsTrue (normal.GetCopySourceNames()  == std::vector<std::string> { "Swapped" }, L"normal mode: its own profiles, less the unedited Default");
            Assert::IsTrue (joyport.GetCopySourceNames() == std::vector<std::string> { "Atari" },   L"Joyport mode: its own profiles, less the unedited Joyport profile");

            normal.AddButtonBinding  (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
            joyport.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });

            Assert::IsTrue (normal.GetCopySourceNames()  == std::vector<std::string> { "Default", "Swapped" }, L"an edited Default is offered, a pending edit included");
            Assert::IsTrue (joyport.GetCopySourceNames() == std::vector<std::string> { "Joyport", "Atari" });

            none.Load ({}, MakeSavedWithBothModes(), {}, true);

            Assert::IsTrue (none.GetCopySourceNames().empty(), L"with no controller there is nothing to copy");
        }


        //  A model with nothing saved has only its built-in profiles, each
        //  still its built-in mapping, so there is nothing to copy.
        TEST_METHOD (CopySources_AModelWithNothingSavedHasNone)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);

            Assert::IsTrue (page.GetCopySourceNames().empty());
        }


        TEST_METHOD (CreateProfile_ACopyOfTheOtherModesProfile_IsRefused)
        {
            ControllersPageState  page;
            std::string           token = ControllerTokens::ModelToToken (MakeStick().unit.model);

            page.Load ({ MakeStick() }, MakeSavedWithBothModes(), {}, true);
            SetJoyportPlayer (page, true);

            Assert::IsTrue  (page.CreateProfile ("Copy", ProfileSource::CopyOfProfile, "Swapped") == ProfileEditResult::NotFound, L"a normal-mode profile in Joyport mode");
            Assert::IsTrue  (page.CreateProfile ("Copy", ProfileSource::CopyOfProfile, "Default") == ProfileEditResult::NotFound, L"the Default in Joyport mode");
            Assert::IsTrue  (page.GetModels().at (token).FindProfile ("Copy") == nullptr, L"nothing is created");
            Assert::IsFalse (page.IsDirty());
        }


        TEST_METHOD (StartingPoints_AreTheModesOwn)
        {
            using Sources = std::vector<ProfileSource>;

            Assert::IsTrue (ControllersPageState::GetStartingPoints (ProfileMode::Joystick, true)
                            == Sources { ProfileSource::DefaultMapping, ProfileSource::CopyOfProfile },
                            L"a Joystick profile: the Default mapping, or a copy");
            Assert::IsTrue (ControllersPageState::GetStartingPoints (ProfileMode::Paddle, true)
                            == Sources { ProfileSource::PaddleMapping, ProfileSource::CopyOfProfile },
                            L"a Paddle profile: the Paddles mapping, or a copy");
            Assert::IsTrue (ControllersPageState::GetStartingPoints (ProfileMode::Joyport, true)
                            == Sources { ProfileSource::JoyportMapping, ProfileSource::CopyOfProfile },
                            L"a Joyport profile: the Joyport mapping, or a copy");
            Assert::IsTrue (ControllersPageState::GetStartingPoints (ProfileMode::Joystick, false)
                            == Sources { ProfileSource::DefaultMapping },
                            L"no copy with nothing to copy");
            Assert::IsTrue (ControllersPageState::GetStartingPoints (ProfileMode::Paddle, false)
                            == Sources { ProfileSource::PaddleMapping });
            Assert::IsTrue (ControllersPageState::GetStartingPoints (ProfileMode::Joyport, false)
                            == Sources { ProfileSource::JoyportMapping });
        }


        TEST_METHOD (StartingPointLabels_ForEverySource)
        {
            const std::pair<ProfileSource, const wchar_t *>  kLabels[] =
            {
                { ProfileSource::DefaultMapping, L"Default mapping" },
                { ProfileSource::JoyportMapping, L"Joyport mapping" },
                { ProfileSource::CopyOfProfile,  L"Copy of"         },
                { ProfileSource::PaddleMapping,  L"Paddles mapping" },
            };
            int                                              swept = 0;

            for (const auto & label : kLabels)
            {
                Assert::AreEqual (std::wstring (label.second), ControllersPageState::GetStartingPointLabel (label.first));
                swept |= 1 << (int) label.first;
            }

            Assert::AreEqual ((1 << ((int) ProfileSource::PaddleMapping + 1)) - 1, swept, L"every source has its label");
        }


        //  Moving the mode after the page opened swaps the list and the edited
        //  profile in place: each controller to its choice for the new mode,
        //  or to that mode's built-in profile with none. An unsaved edit stays
        //  pending on the profile it was made on.
        TEST_METHOD (SetProfileMode_AfterLoad_SwapsTheListAndTheEditedProfile)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  stick = MakeStick();
            std::string           token = ControllerTokens::UnitToToken (stick.unit);

            page.Load ({ stick }, MakeSavedWithBothModes(), {}, true, std::map<std::string, std::string> { { token, "Swapped" } }, stick.unit);
            page.SetActiveProfiles (ProfileMode::Joyport, { { token, "Atari" } });
            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
            page.BeginCapture (PaddleTarget::Pb0, Rest());

            SetJoyportPlayer (page, true);

            Assert::IsTrue   (page.GetProfileNames() == std::vector<std::string> { "Joyport", "Atari" });
            Assert::AreEqual (std::string ("Atari"), page.GetEditedProfileName(), L"the controller's Joyport-mode choice");
            Assert::IsFalse  (page.IsCapturing(), L"a capture belongs to the profile it was started on");

            SetJoyportPlayer (page, false);

            Assert::AreEqual (std::string ("Swapped"), page.GetEditedProfileName(), L"and back to its normal-mode choice");
            Assert::AreEqual (size_t (1), page.GetMapping().pb2.size(),             L"with its edit still pending");
            Assert::IsTrue   (page.HasUnappliedProfileEdits());

            page.SetActiveProfiles (ProfileMode::Joyport, {});
            SetJoyportPlayer (page, true);

            Assert::AreEqual (std::string ("Joyport"), page.GetEditedProfileName(), L"with no choice for the mode, its built-in profile");
        }


        //  A choice made after switching is kept for the mode it was made in.
        TEST_METHOD (SetProfileMode_KeepsEachModesChoicesForOk)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  stick = MakeStick();
            std::string           token = ControllerTokens::UnitToToken (stick.unit);

            page.Load ({ stick }, MakeSavedWithBothModes(), {}, true, std::string(), stick.unit);
            SetJoyportPlayer (page, true);
            page.SelectProfile ("Atari");

            Assert::IsTrue   (page.HasActiveProfileChanged (ProfileMode::Joyport));
            Assert::IsFalse  (page.HasActiveProfileChanged (ProfileMode::Joystick));
            Assert::AreEqual (std::string ("Atari"), page.GetActiveProfiles (ProfileMode::Joyport).at (token));
            Assert::IsTrue   (page.GetActiveProfiles (ProfileMode::Joystick).find (token) == page.GetActiveProfiles (ProfileMode::Joystick).end());

            page.Revert();

            Assert::IsFalse (page.HasActiveProfileChanged(), L"Cancel takes back a choice made in either mode");
        }


        //  A profile created in Joyport mode belongs to Joyport mode, from
        //  either of its starting points, and resets to the Joyport mapping.
        TEST_METHOD (CreateProfile_BelongsToThePagesModeAndResetsToItsBuiltIn)
        {
            ControllersPageState    page;
            ControllerModelKey      model    = MakeStick().unit.model;
            std::vector<ControlId>  controls = MakeStick().controls;
            std::string             token    = ControllerTokens::ModelToToken (model);
            ControlMapping          joyport  = DefaultMapping::MakeJoyport (model, ControllerFormFactor::Gamepad, controls);

            page.Load ({ MakeStick() }, MakeSavedWithBothModes(), {}, true);
            SetJoyportPlayer (page, true);

            Assert::IsTrue (page.CreateProfile ("From Joyport", ProfileSource::JoyportMapping, std::string()) == ProfileEditResult::Ok);
            Assert::IsTrue (page.GetMapping() == joyport);
            Assert::IsTrue (page.GetModels().at (token).FindProfile ("From Joyport")->mode == ProfileMode::Joyport);

            page.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 7 } });

            Assert::IsFalse (page.GetMapping() == joyport, L"the edit changed the mapping");

            page.ResetProfile();

            Assert::IsTrue (page.GetMapping() == joyport, L"a Joyport-mode profile resets to the Joyport mapping");

            Assert::IsTrue (page.CreateProfile ("Copy", ProfileSource::CopyOfProfile, "Atari") == ProfileEditResult::Ok, L"a copy of a Joyport-mode profile");
            Assert::IsTrue (page.GetModels().at (token).FindProfile ("Copy")->mode == ProfileMode::Joyport);
            Assert::IsTrue (page.CreateProfile ("swapped", ProfileSource::JoyportMapping, std::string()) == ProfileEditResult::DuplicateName,
                L"a name used in the other mode is taken");
            Assert::IsTrue (page.GetProfileNames() == std::vector<std::string> { "Joyport", "Atari", "From Joyport", "Copy" });
        }


        //  Each kind accepts only its own built-in starting point: the default
        //  mapping for a Joystick profile, the Paddles mapping for a Paddle
        //  profile and the Joyport mapping for a Joyport profile.
        TEST_METHOD (CreateProfile_ABuiltInStartingPointOfTheOtherMode_IsRefused)
        {
            ControllersPageState  page;
            std::string           token = ControllerTokens::ModelToToken (MakeStick().unit.model);

            page.Load ({ MakeStick() }, MakeSavedWithBothModes(), {}, true);
            SetJoyportPlayer (page, true);

            Assert::IsTrue (page.CreateProfile ("A", ProfileSource::DefaultMapping, std::string()) == ProfileEditResult::NotFound, L"the default mapping in Joyport mode");
            Assert::IsTrue (page.CreateProfile ("B", ProfileSource::PaddleMapping,        std::string()) == ProfileEditResult::NotFound, L"Paddles in Joyport mode");

            SetJoyportPlayer (page, false);

            Assert::IsTrue  (page.CreateProfile ("C", ProfileSource::JoyportMapping, std::string()) == ProfileEditResult::NotFound, L"the Joyport mapping for a Joystick profile");
            Assert::IsTrue  (page.CreateProfile ("D", ProfileSource::PaddleMapping,  std::string()) == ProfileEditResult::NotFound, L"the Paddles mapping for a Joystick profile");

            SetPaddlePlayer (page);

            Assert::IsTrue  (page.CreateProfile ("E", ProfileSource::DefaultMapping, std::string()) == ProfileEditResult::NotFound, L"the default mapping for a Paddle profile");
            Assert::IsTrue  (page.GetModels().at (token).FindProfile ("A") == nullptr, L"nothing is created");
            Assert::IsTrue  (page.GetModels().at (token).FindProfile ("B") == nullptr);
            Assert::IsTrue  (page.GetModels().at (token).FindProfile ("C") == nullptr);
            Assert::IsTrue  (page.GetModels().at (token).FindProfile ("D") == nullptr);
            Assert::IsTrue  (page.GetModels().at (token).FindProfile ("E") == nullptr);
            Assert::IsFalse (page.IsDirty());
        }


        TEST_METHOD (RenameAndDelete_AreRefusedForTheDefault)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, MakeSavedWithSwapped(), {}, true);

            Assert::IsTrue  (page.RenameProfile ("Other") == ProfileEditResult::IsBuiltInProfile);
            Assert::IsTrue  (page.DeleteProfile() == ProfileEditResult::IsBuiltInProfile);
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
            Assert::IsFalse  (ControllersPageState::TryDescribeNameError (ProfileEditResult::IsBuiltInProfile, label, rule));
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
            Assert::IsTrue  (page.IsEditingBuiltInProfile());
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


        // Two players each playing their own controller, both on Automatic
        // and in Joystick mode, as the service reports them.
        static PlayerSlots MakeTwoPlaying (const ControllerUnitKey & first,
                                           const ControllerUnitKey & second)
        {
            PlayerSlots  slots;

            slots[0].state  = PlayerSlotState::Playing;
            slots[0].holder = first;
            slots[0].target = PlayerAxisTarget::Joystick0;
            slots[1].state  = PlayerSlotState::Playing;
            slots[1].holder = second;
            slots[1].target = PlayerAxisTarget::Joystick1;
            return slots;
        }


        // The same in the given modes, the slots' targets as the modes give
        // them, handed to the page.
        static void SetTwoPlaying (ControllersPageState     & page,
                                   const ControllerUnitKey  & first,
                                   const ControllerUnitKey  & second,
                                   PlayerMode                 firstMode,
                                   PlayerMode                 secondMode,
                                   size_t                     axisCount = GamePortContribution::kAxisCount)
        {
            PlayerEntries  entries;
            PlayerSlots    slots       = MakeTwoPlaying (first, second);
            bool           isOnePaddle = firstMode == PlayerMode::Paddle;

            entries[0].mode = firstMode;
            entries[1].mode = secondMode;
            slots[0].target = PlayerTargetRules::GetModeTarget (0, isOnePaddle, isOnePaddle);
            slots[1].target = PlayerTargetRules::GetModeTarget (1, secondMode == PlayerMode::Paddle, isOnePaddle);
            page.SetPlayers (entries, slots, axisCount);
        }


        //  Every pick the page hands on to the service, in order.
        using Picks = std::vector<std::pair<size_t, PlayerEntry>>;


        static ControllersPageState::PlayerPickedFn RecordPicks (Picks & picks)
        {
            return [&picks] (size_t player, const PlayerEntry & entry) { picks.push_back ({ player, entry }); };
        }


        TEST_METHOD (InPlay_WithTheModeOff_EverythingIsInPlay)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);

            // One player is what a machine has always done: the one
            // controller drives both paddles and all three buttons.
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pdl1));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pb0));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pb1));
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pb2));
        }


        TEST_METHOD (InPlay_AJoystickSlot_PlaysBothPaddlesAndBothItsButtons)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            page.SetPlayers (PlayerEntries(), MakeTwoPlaying (first.unit, second.unit), 4);
            page.SelectController (0);

            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl0), L"a joystick is two paddles wired to one stick");
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl1));
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0),  L"and joystick 0 is wired to PB0");
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb1),  L"and PB1");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb2),  L"PB2 is joystick 1's while two play");
        }


        TEST_METHOD (InPlay_APaddlePlayer_PlaysOnePaddleAndOneButton)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::Paddle, PlayerMode::Joystick);
            page.SelectController (0);

            // The player's paddles take the mapping's targets in ascending
            // order, so one paddle plays the controller's PDL0 alone.
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pdl1), L"there is no second paddle for PDL1 to land on");
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb1),  L"a paddle has one line");
        }


        //  One player alone in Paddle mode drives PDL0 and PB0, so only those
        //  rows are shown; in Joystick mode it is edited whole.
        TEST_METHOD (InPlay_ALonePaddlePlayer_PlaysPdl0AndPb0)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            SetPaddlePlayer (page);

            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pdl1));
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb1));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb2));
        }


        TEST_METHOD (InPlay_PlayerTwo_DrivesTheirLineFromTheirOwnPb0)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            page.SetPlayers (PlayerEntries(), MakeTwoPlaying (first.unit, second.unit), 4);
            page.SelectController (1);

            Assert::IsTrue  (page.FindEditedPlayer() == std::optional<size_t> (1));
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0),
                L"player two's PB0 bindings drive PB2, so PB0 is the row they edit");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb1),
                L"their own PB1 bindings are kept and ignored while two play");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb2));
        }


        TEST_METHOD (InPlay_AControllerInNeitherSlot_PlaysNothing)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            ControllerDeviceInfo  spare  = MakeStick ("{C}");

            page.Load ({ first, second, spare }, {}, {}, true);
            page.SetPlayers (PlayerEntries(), MakeTwoPlaying (first.unit, second.unit), 4);
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
            page.SetPlayers (PlayerEntries(), MakeTwoPlaying (first.unit, second.unit), 2);
            page.SelectController (1);

            // Player two is on joystick 1, which is PDL2/PDL3: a //c has
            // neither, so they are not read at all.
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pdl0));
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb0),
                L"neither their paddles nor their button reach a two-axis machine");

            page.SelectController (0);
            Assert::IsTrue (page.IsTargetInPlay (PaddleTarget::Pdl0), L"while player one, on joystick 0, plays as usual");
        }


        //  The players' entries and their modes apply as they are changed,
        //  like a pick from the picker, and are no part of what OK commits
        //  or Cancel reverts.
        TEST_METHOD (PlayerEdits_ApplyAtOnceAndAreNotUndoneByCancel)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            Picks                 picks;
            PlayerEntry           pick;
            size_t                modes  = 0;



            page.Load ({ first, second }, {}, {}, true);
            page.SetPlayers (PlayerEntries(), MakeTwoPlaying (first.unit, second.unit), 4);
            page.SetOnPlayerPicked  (RecordPicks (picks));
            page.SetOnPlayerModeSet ([&modes] (size_t, PlayerMode) { modes++; });

            pick.kind = PlayerEntryKind::Controller;
            pick.unit = second.unit;
            page.PickPlayerEntry (0, pick);
            page.SetPlayerMode (0, PlayerMode::Paddle);
            pick      = PlayerEntry();
            pick.kind = PlayerEntryKind::Disabled;
            page.PickPlayerEntry (1, pick);

            Assert::AreEqual (size_t (2), picks.size(), L"each change reaches the service as it is made");
            Assert::AreEqual (size_t (1), modes,        L"the mode as well");
            Assert::IsFalse  (page.IsDirty(), L"so none of them is part of what OK commits");

            page.Revert();

            Assert::IsTrue (page.GetPlayerEntries()[0].kind == PlayerEntryKind::Controller, L"Cancel does not take back the entry");
            Assert::IsTrue (page.GetPlayerEntries()[0].unit == second.unit);
            Assert::IsTrue (page.GetPlayerEntries()[0].mode == PlayerMode::Paddle, L"nor the mode");
            Assert::IsTrue  (page.GetPlayerEntries()[1].kind == PlayerEntryKind::Disabled, L"nor Player 2's Disabled");
        }


        //  A player's mode goes to the service as it is set, a new entry for
        //  the player keeps it, and while the Joyport is in effect it is not
        //  set at all.
        TEST_METHOD (PlayerMode_IsSetAtOnceAndKeptThroughAPick)
        {
            ControllersPageState                             page;
            ControllerDeviceInfo                             first  = MakeStick ("{A}");
            ControllerDeviceInfo                             second = MakeStick ("{B}");
            std::vector<std::pair<size_t, PlayerMode>>       sets;
            PlayerEntry                                      pick;



            page.Load ({ first, second }, {}, {}, true);
            page.SetPlayers (PlayerEntries(), MakeTwoPlaying (first.unit, second.unit), 4);
            page.SetOnPlayerModeSet ([&sets] (size_t player, PlayerMode mode) { sets.push_back ({ player, mode }); });

            page.SetPlayerMode (1, PlayerMode::Paddle);

            Assert::AreEqual (size_t (1), sets.size(), L"the mode goes to the service");
            Assert::IsTrue   (sets.back() == std::make_pair (size_t (1), PlayerMode::Paddle));
            Assert::IsTrue   (page.GetPlayerEntries()[1].mode == PlayerMode::Paddle);

            page.SetPlayerMode (1, PlayerMode::Paddle);
            Assert::AreEqual (size_t (1), sets.size(), L"setting the mode it has sets nothing");

            pick.kind = PlayerEntryKind::Controller;
            pick.unit = second.unit;
            page.PickPlayerEntry (1, pick);
            Assert::IsTrue (page.GetPlayerEntries()[1].mode == PlayerMode::Paddle, L"a new entry keeps the mode");

            page.SetJoyportAvailable (true);
            page.SetPlayerMode (0, PlayerMode::JoyportLeft);
            page.SetPlayerMode (1, PlayerMode::JoyportLeft);
            Assert::AreEqual (size_t (2), sets.size(),                             L"a jack the other player holds is not set");
            Assert::IsTrue   (page.GetPlayerEntries()[1].mode == PlayerMode::Paddle, L"and the player keeps its mode");

            page.SetPlayerMode (1, PlayerMode::JoyportRight);
            Assert::AreEqual (size_t (3), sets.size(), L"the free jack is");
        }


        //  The mode drop-down lists the picker's modes: the jacks on a
        //  machine that has a Joyport, Same as Player 1 first for Player 2,
        //  and the jack the other player holds disabled.
        TEST_METHOD (ModeChoices_AreThePickersWithTheTakenJackDisabled)
        {
            ControllersPageState                           page;
            PlayerEntries                                  entries;
            std::vector<InputModeRules::PlayerModeChoice>  two;



            entries[0].mode = PlayerMode::JoyportLeft;
            entries[1].mode = PlayerMode::SameAsPlayer1;

            page.SetJoyportAvailable (true);
            page.SetPlayers (entries, PlayerSlots(), 4);
            two = page.GetModeChoices (1);

            Assert::AreEqual (size_t (5), two.size());
            Assert::IsTrue   (two[0].mode == PlayerMode::SameAsPlayer1 && two[0].isChecked);
            Assert::IsTrue   (two[2].mode == PlayerMode::JoyportLeft && !two[2].isEnabled, L"Player 1 holds the left jack");
            Assert::IsTrue   (two[3].mode == PlayerMode::JoyportRight && two[3].isEnabled);
            Assert::AreEqual (size_t (4), page.GetModeChoices (0).size(), L"Player 1 has no Same as Player 1");

            page.SetJoyportAvailable (false);
            Assert::AreEqual (size_t (2), page.GetModeChoices (0).size(), L"no jacks on the //c");
        }


        //  A player beside the Joyport keeps its paddles and loses its
        //  buttons: the page disables its button rows and shows a warning
        //  under it that gives the player in the Joyport.
        TEST_METHOD (ButtonsCut_BesideTheJoyport)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            PlayerEntries         entries;



            entries[0].mode = PlayerMode::JoyportLeft;
            entries[1].mode = PlayerMode::Paddle;

            page.Load ({ first, second }, {}, {}, true);
            page.SetJoyportAvailable (true);
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);

            Assert::AreEqual (std::wstring(), page.GetButtonsCutNotice (0), L"the Joyport player fires through its jack");
            Assert::AreEqual (std::wstring (L"This controller's buttons are disabled because Player 1 is using the Joyport."), page.GetButtonsCutNotice (1));

            page.SelectController (1);
            Assert::IsTrue  (page.AreEditedButtonsCut(), L"Player 2's controller in Editing");
            Assert::IsFalse (page.IsEditedOnJoyport());
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pdl0), L"its paddle stays in play");
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0),  L"and its button row stays on the page, to be disabled");

            page.SelectController (0);
            Assert::IsFalse (page.AreEditedButtonsCut());
            Assert::IsTrue  (page.IsEditedOnJoyport(), L"Player 1's controller in Editing is in a jack");
            Assert::IsTrue  (page.IsTargetInPlay (PaddleTarget::Pb0),  L"fire");
            Assert::IsFalse (page.IsTargetInPlay (PaddleTarget::Pb1),  L"which reads no PB1");

            entries[1].mode = PlayerMode::SameAsPlayer1;
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);
            Assert::AreEqual (std::wstring(), page.GetButtonsCutNotice (1), L"both in jacks, nothing cut");
        }


        //  The page lists and edits the profiles of the kind the controller
        //  in Editing plays: its player's mode, either Joyport jack being the
        //  Joyport kind. Changing it swaps the list and the edited profile.
        TEST_METHOD (ProfileKind_FollowsTheEditedPlayersMode)
        {
            ControllersPageState                            page;
            ControllerDeviceInfo                            first  = MakeStick ("{A}");
            ControllerDeviceInfo                            second = MakeStick ("{B}");
            std::string                                     token  = ControllerTokens::UnitToToken (second.unit);
            std::map<std::string, ControllerModelSettings>  models = MakeSavedWithBothModes();



            models.begin()->second.AddProfile ("Pong", ControlMapping(), ProfileMode::Paddle);

            page.Load ({ first, second }, models, {}, true);
            page.SetActiveProfiles (ProfileMode::Paddle, { { token, "Pong" } });
            page.SetPlayers (PlayerEntries(), MakeTwoPlaying (first.unit, second.unit), 4);
            page.SelectController (1);

            Assert::IsTrue   (page.GetProfileMode() == ProfileMode::Joystick, L"Player 2 in Joystick mode edits Joystick profiles");
            Assert::IsTrue   (page.GetProfileNames() == std::vector<std::string> { "Default", "Swapped" });

            page.SetPlayerMode (1, PlayerMode::Paddle);

            Assert::IsTrue   (page.GetProfileMode() == ProfileMode::Paddle, L"and Paddle profiles in Paddle mode");
            Assert::IsTrue   (page.GetProfileNames() == std::vector<std::string> { "Paddles", "Pong" });
            Assert::AreEqual (std::string ("Pong"), page.GetEditedProfileName(), L"its Paddle choice is edited");

            page.SelectController (0);
            Assert::IsTrue   (page.GetProfileMode() == ProfileMode::Joystick, L"Player 1 is still in Joystick mode");

            page.SetJoyportAvailable (true);
            page.SetPlayerMode (0, PlayerMode::JoyportLeft);
            Assert::IsTrue   (page.GetProfileMode() == ProfileMode::Joyport, L"in a jack, Joyport profiles");

            page.SelectController (1);
            Assert::IsTrue   (page.GetProfileMode() == ProfileMode::Paddle, L"and only for the player in the jack");
        }

        //  Two people cannot share one controller, so picking the one the
        //  other player picked returns the other player to Automatic.
        TEST_METHOD (PickingTheOtherPlayersController_ReturnsThemToAutomatic)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            PlayerEntries         entries;
            PlayerEntry           pick;



            entries[0].kind = PlayerEntryKind::Controller;
            entries[0].unit = first.unit;
            entries[1].kind = PlayerEntryKind::Controller;
            entries[1].unit = second.unit;

            page.Load ({ first, second }, {}, {}, true);
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);

            pick.kind = PlayerEntryKind::Controller;
            pick.unit = first.unit;
            page.PickPlayerEntry (1, pick);

            Assert::IsTrue (page.GetPlayerEntries()[1].unit == first.unit, L"player two takes the pick");
            Assert::IsTrue (page.GetPlayerEntries()[0].kind == PlayerEntryKind::Automatic, L"and player one goes back to Automatic");
        }


        //  Each player's drop-down lists what the picker's submenu lists,
        //  Player 2's Disabled included.
        TEST_METHOD (EntryChoices_AreThePickers)
        {
            ControllersPageState                       page;
            ControllerDeviceInfo                       stick = MakeStick();
            std::vector<InputModeRules::PlayerChoice>  one;
            std::vector<InputModeRules::PlayerChoice>  two;



            page.Load ({ stick }, {}, {}, true);
            page.SetPlayers (PlayerEntries(), PlayerSlots(), 4);

            one = page.GetEntryChoices (0);
            two = page.GetEntryChoices (1);

            Assert::AreEqual (size_t (3), one.size(), L"Automatic, the stick and, in Joystick mode, the keys");
            Assert::IsTrue   (one[0].entry.kind == PlayerEntryKind::Automatic);
            Assert::IsTrue   (one[1].entry.unit == stick.unit);
            Assert::IsTrue   (one[2].entry.kind == PlayerEntryKind::ArrowKeys);
            Assert::AreEqual (size_t (3), two.size(), L"Automatic, the stick and Disabled");
            Assert::AreEqual (std::wstring (L"Disabled"), two.back().label);

            page.SetPlayerMode (0, PlayerMode::Paddle);
            one = page.GetEntryChoices (0);
            Assert::IsTrue   (one.back().entry.kind == PlayerEntryKind::MousePaddle, L"in Paddle mode, the mouse in place of the keys");

            page.SetJoyportAvailable (true);
            page.SetPlayerMode (0, PlayerMode::JoyportLeft);
            one = page.GetEntryChoices (0);
            Assert::IsTrue   (one.back().entry.kind == PlayerEntryKind::ArrowKeys, L"in a jack, the keys and no mouse as paddle");
            Assert::AreEqual (std::wstring (L"Disabled"), page.GetEntryChoices (1).back().label, L"Player 2's Disabled keeps its word");
        }


        //  The note beside each player's row: the joystick or paddle its mode
        //  gives it, nothing on a machine without it, the paddles the mouse
        //  drives, the jacks of a player in the Joyport, and no note for
        //  Player 2 Disabled.
        TEST_METHOD (PlayerNote_SaysWhatThePlayerDrives)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");
            PlayerEntries         entries;



            page.Load ({ first, second }, {}, {}, true);

            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::Joystick, PlayerMode::Joystick);
            Assert::AreEqual (std::wstring (L"joystick 0"), page.GetPlayerNote (0));
            Assert::AreEqual (std::wstring (L"joystick 1"), page.GetPlayerNote (1));

            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::Joystick, PlayerMode::Paddle);
            Assert::AreEqual (std::wstring (L"paddle 2"), page.GetPlayerNote (1), L"a paddle beside a joystick");

            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::Paddle, PlayerMode::Paddle);
            Assert::AreEqual (std::wstring (L"paddle 0"), page.GetPlayerNote (0));
            Assert::AreEqual (std::wstring (L"paddle 1"), page.GetPlayerNote (1), L"a paddle beside a paddle");

            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::Joystick, PlayerMode::Joystick, 2);
            Assert::AreEqual (std::wstring (L"nothing on this machine"), page.GetPlayerNote (1), L"a second joystick on the //c");

            entries[0].kind = PlayerEntryKind::MousePaddle;
            entries[1].kind = PlayerEntryKind::Disabled;
            page.SetPlayers (entries, PlayerSlots(), 4);
            Assert::AreEqual (std::wstring (L"paddles 0 and 1"), page.GetPlayerNote (0), L"the mouse drives both of Player 1's paddles");
            Assert::AreEqual (std::wstring(),                    page.GetPlayerNote (1), L"and Disabled drives nothing");

            entries[1].kind = PlayerEntryKind::Automatic;
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);
            Assert::AreEqual (std::wstring (L"paddle 0"), page.GetPlayerNote (0), L"while Player 2 plays, the mouse drives paddle 0 alone");

            entries[0]      = PlayerEntry();
            entries[0].mode = PlayerMode::JoyportLeft;
            entries[1].kind = PlayerEntryKind::Disabled;
            entries[1].mode = PlayerMode::SameAsPlayer1;
            page.SetJoyportAvailable (true);
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);
            Assert::AreEqual (std::wstring (L"both jacks"), page.GetPlayerNote (0), L"with Player 2 Disabled, Player 1 drives both jacks");

            entries[1].kind = PlayerEntryKind::Automatic;
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);
            Assert::AreEqual (std::wstring (L"left jack"),  page.GetPlayerNote (0));
            Assert::AreEqual (std::wstring (L"right jack"), page.GetPlayerNote (1), L"Same as Player 1 is the other jack");

            entries[1].mode = PlayerMode::Joystick;
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);
            Assert::AreEqual (std::wstring (L"both jacks"), page.GetPlayerNote (0), L"the right jack is free");
            Assert::AreEqual (std::wstring (L"joystick 0"), page.GetPlayerNote (1), L"beside the Joyport a joystick plays as though alone");

            entries[1].mode = PlayerMode::Paddle;
            page.SetPlayers (entries, MakeTwoPlaying (first.unit, second.unit), 4);
            Assert::AreEqual (std::wstring (L"paddle 0"), page.GetPlayerNote (1), L"and a paddle too");
        }


        TEST_METHOD (MultiplayerRows_AreNamedByThePaddleThePlayerDrives)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            page.Load ({ first, second }, {}, {}, true);
            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::Joystick, PlayerMode::Paddle);

            // Player one holds joystick 0, which the guest reads on the
            // controller's own targets, so its rows keep their own names.
            page.SelectController (0);
            Assert::AreEqual (std::wstring (L""), page.GetTargetPlayLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L""), page.GetTargetPlayLabel (PaddleTarget::Pdl1));
            Assert::AreEqual (std::wstring (L""), page.GetTargetPlayLabel (PaddleTarget::Pb0));

            // Player two in Paddle mode beside a joystick holds ONE paddle,
            // paddle 2, so their second axis row drives nothing and their
            // PB0 bindings drive paddle 2's line.
            page.SelectController (1);
            Assert::AreEqual (std::wstring (L"PDL2:"), page.GetTargetPlayLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L""),      page.GetTargetPlayLabel (PaddleTarget::Pdl1));
            Assert::AreEqual (std::wstring (L"PB2:"),  page.GetTargetPlayLabel (PaddleTarget::Pb0));
            Assert::IsFalse  (page.IsTargetInPlay (PaddleTarget::Pdl1), L"so its row is not shown");
            Assert::IsFalse  (page.IsTargetInPlay (PaddleTarget::Pb1),  L"and only their own button line is in play");

            // Beside a paddle, paddle 1 and PB1.
            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::Paddle, PlayerMode::Paddle);
            Assert::AreEqual (std::wstring (L"PDL1:"), page.GetTargetPlayLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L"PB1:"),  page.GetTargetPlayLabel (PaddleTarget::Pb0));
        }


        TEST_METHOD (Joyport_OneControllerDrivesBothJacks)
        {
            ControllersPageState  page;

            page.Load ({ MakeStick() }, {}, {}, true);
            SetJoyportPlayer (page, true);

            Assert::IsTrue (page.GetJoyportJack() == JoyportJack::Both);
            Assert::AreEqual (std::wstring (L"Atari joystick: both jacks"), ControllersPageState::GetJoyportHeading (page.GetJoyportJack()));
        }


        TEST_METHOD (Joyport_InMultiplayerEditingFollowsTheSlotsJack)
        {
            ControllersPageState  page;
            ControllerDeviceInfo  first  = MakeStick ("{A}");
            ControllerDeviceInfo  second = MakeStick ("{B}");

            //  Player 1 in the right jack and Player 2 in the left: the modes,
            //  not the numbers, give the jacks.
            page.Load ({ first, second }, {}, {}, true);
            page.SetJoyportAvailable (true);
            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::JoyportRight, PlayerMode::JoyportLeft);

            page.SelectController (0);
            Assert::IsTrue (page.GetJoyportJack() == JoyportJack::Right, L"player 1 is the right jack");

            page.SelectController (1);
            Assert::IsTrue (page.GetJoyportJack() == JoyportJack::Left, L"and player 2, once Editing moves to them, the left");
            Assert::AreEqual (std::wstring (L"Atari joystick: left jack"), ControllersPageState::GetJoyportHeading (page.GetJoyportJack()));

            SetTwoPlaying (page, first.unit, second.unit, PlayerMode::JoyportLeft, PlayerMode::Paddle);
            Assert::IsTrue (page.GetJoyportJack() == JoyportJack::None, L"a player on Paddle is in no jack");
        }


        TEST_METHOD (Joyport_NoControllerHasNoJack)
        {
            ControllersPageState  page;

            page.Load ({}, {}, {}, true);

            Assert::IsTrue (page.GetJoyportJack() == JoyportJack::None);
            Assert::AreEqual (std::wstring (L"Atari joystick"), ControllersPageState::GetJoyportHeading (page.GetJoyportJack()));
        }


        TEST_METHOD (Joyport_TheLiveReadingCarriesThePendingMappingsSwitches)
        {
            ControllersPageState  page;
            ControllerSample      sample;
            AxisBinding           dpad;

            page.Load ({ MakeStick() }, {}, {}, true);

            //  An edit that is not yet applied: the D-pad on PDL1. The lights
            //  follow it at once, which is what makes a profile checkable
            //  against the Joyport before OK.
            dpad.kind     = AxisBindingKind::DigitalPair;
            dpad.negative = { ControlKind::DpadUp, 0 };
            dpad.positive = { ControlKind::DpadDown, 0 };
            page.AddAxisBinding (PaddleTarget::Pdl1, dpad);

            sample.connected = true;
            sample.hats[0]   = ControllerSample::kHatUp;

            Assert::IsTrue (page.ComputeLiveReading (sample).switches.test (static_cast<size_t> (JoystickSwitch::Up)));
        }


        TEST_METHOD (Joyport_OnlyTheRowsTheJoyportReadsAreShownAndTheyAreLabeledForIt)
        {
            //  PB1 and PB2 drive nothing through the Joyport, so the page
            //  leaves them out rather than offering rows that do nothing.
            Assert::IsTrue  (ControllersPageState::IsJoyportTarget (PaddleTarget::Pdl0));
            Assert::IsTrue  (ControllersPageState::IsJoyportTarget (PaddleTarget::Pdl1));
            Assert::IsTrue  (ControllersPageState::IsJoyportTarget (PaddleTarget::Pb0));
            Assert::IsFalse (ControllersPageState::IsJoyportTarget (PaddleTarget::Pb1));
            Assert::IsFalse (ControllersPageState::IsJoyportTarget (PaddleTarget::Pb2));

            Assert::AreEqual (std::wstring (L"Left/right:"), ControllersPageState::GetJoyportRowLabel (PaddleTarget::Pdl0));
            Assert::AreEqual (std::wstring (L"Up/down:"),    ControllersPageState::GetJoyportRowLabel (PaddleTarget::Pdl1));
            Assert::AreEqual (std::wstring (L"Fire:"),       ControllersPageState::GetJoyportRowLabel (PaddleTarget::Pb0));
            Assert::IsTrue   (ControllersPageState::GetJoyportRowLabel (PaddleTarget::Pb1).empty());
        }
    };
}
