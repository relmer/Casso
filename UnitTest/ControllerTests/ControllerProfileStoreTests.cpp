#include "Pch.h"

#include "../UiTests/InMemoryFileSystem.h"

#include "Config/GlobalUserPrefs.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"
#include "Controllers/XInputSampleDecoder.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfileStoreTests
//
//  What survives a restart. The round trip runs through the real global
//  prefs file on an in-memory filesystem, since a calibration that saves
//  correctly into a section the prefs never write is still lost.
//
//  A SAVED ENTRY THAT CANNOT BE USED IS REPORTED, NOT QUIETLY REPLACED. The
//  unit falls back to automatic either way; the report is what tells the user
//  the calibration they made is gone.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControllerProfileStoreTests)
    {
    public:

        static std::string StickToken (const char * unitId)
        {
            ControllerUnitKey  unit;

            unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            unit.unitId = unitId;
            unit.source = ControllerUnitSource::InstanceGuid;
            return ControllerTokens::UnitToToken (unit);
        }


        static JsonValue Parse (const std::string & text)
        {
            JsonValue       doc;
            JsonParseError  err;

            AssertSucceeded (JsonParser::Parse (text, doc, err));
            return doc;
        }


        TEST_METHOD (RoundTrip_ThroughTheGlobalPrefsFile)
        {
            InMemoryFileSystem        fs;
            GlobalUserPrefs           saved;
            GlobalUserPrefs           loaded;
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            ControllerCalibration     user;
            ControllerCalibration     automatic;
            std::vector<std::string>  rejected;

            user.mode = CalibrationMode::User;
            user.axes.fill ({ 0.0f, -1.0f, 1.0f });
            user.axes[0] = { 0.05f, -0.9f, 0.93f };

            automatic.axes[1] = { 0.0f, -0.6f, 0.7f };

            store.calibrations[StickToken ("{USER}")]      = user;
            store.calibrations[StickToken ("{AUTOMATIC}")] = automatic;

            saved.controllers = store.ToJson (saved.controllers);
            AssertSucceeded (saved.Save  (L"C:\\Casso", fs));
            AssertSucceeded (loaded.Load (L"C:\\Casso", fs));

            readBack.FromJson (loaded.controllers, rejected);

            Assert::IsTrue (rejected.empty());
            Assert::IsTrue (readBack.calibrations.at (StickToken ("{USER}")) == user, L"a user calibration comes back exactly");

            const ControllerCalibration &  learned = readBack.calibrations.at (StickToken ("{AUTOMATIC}"));

            Assert::IsTrue   (learned.mode == CalibrationMode::Automatic);
            Assert::AreEqual (-0.6f, learned.axes[1].minimum, 0.0001f, L"the travel it learned is kept");
            Assert::AreEqual ( 0.7f, learned.axes[1].maximum, 0.0001f);
        }


        TEST_METHOD (InvalidEntry_IsDroppedAndReportedWhileOthersLoad)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               bad  = StickToken ("{BAD}");
            std::string               good = StickToken ("{GOOD}");
            JsonValue                 doc  = Parse (
                "{\"calibration\":{"
                  "\"" + bad  + "\":{\"mode\":\"user\",\"axes\":[{\"index\":0,\"center\":0.9,\"min\":-0.5,\"max\":0.5}]},"
                  "\"" + good + "\":{\"mode\":\"user\",\"axes\":[{\"index\":0,\"center\":0.0,\"min\":-0.5,\"max\":0.5}]}"
                "}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (1), rejected.size(), L"the entry whose center is outside its limits is reported");
            Assert::AreEqual (bad, rejected[0]);
            Assert::IsTrue   (store.calibrations.find (bad) == store.calibrations.end(), L"and not used, so its unit starts automatic");
            Assert::IsTrue   (store.calibrations.find (good) != store.calibrations.end(), L"the valid entry still loads");
        }


        TEST_METHOD (AnEntryForAnXboxControllerIsRejected)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            JsonValue                 doc = Parse (
                "{\"calibration\":{\"xinput\":{\"mode\":\"user\",\"axes\":[]}}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (1), rejected.size(), L"Xbox-class controllers are never calibrated (FR-018a)");
            Assert::IsTrue   (store.calibrations.empty());
        }


        TEST_METHOD (UnknownModeOrMissingFields_AreRejected)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               a   = StickToken ("{A}");
            std::string               b   = StickToken ("{B}");
            JsonValue                 doc = Parse (
                "{\"calibration\":{"
                  "\"" + a + "\":{\"mode\":\"sometimes\",\"axes\":[]},"
                  "\"" + b + "\":{\"mode\":\"user\",\"axes\":[{\"index\":0,\"center\":0.0}]}"
                "}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (2), rejected.size());
        }


        TEST_METHOD (UnknownMembersOfTheSection_SurviveASave)
        {
            ControllerProfileStore  store;
            JsonValue               section = Parse ("{\"models\":{\"xinput\":{\"deadzone\":0.24}},\"futureKey\":1}");
            JsonValue                written;
            const JsonValue        * member    = nullptr;
            ControllerCalibration    user;
            int                      futureKey = 0;

            user.mode = CalibrationMode::User;
            user.axes.fill ({ 0.0f, -1.0f, 1.0f });
            store.calibrations[StickToken ("{A}")] = user;

            written = store.ToJson (section);

            Assert::IsTrue (written.HasObject ("models", member),  L"a section member this class does not own is kept");
            Assert::IsTrue (written.HasInt ("futureKey", futureKey), L"including one no build knows yet");
            Assert::IsTrue (written.HasObject ("calibration", member));
        }


        TEST_METHOD (NothingLearned_WritesNothing)
        {
            ControllerProfileStore  store;

            store.calibrations[StickToken ("{A}")] = ControllerCalibration();

            Assert::IsTrue (store.ToJson (JsonValue()).GetType() == JsonType::Null,
                L"a controller that never moved leaves the prefs file as it was");
        }


        static ControllerModelKey Stick()
        {
            return { ControllerKind::DirectInput, 0x231d, 0x0121 };
        }


        static ControlMapping MakeFullMapping()
        {
            ControlMapping  mapping;
            AxisBinding     rate;
            AxisBinding     pair;
            ButtonBinding   trigger;
            ButtonBinding   axisButton;

            rate.analog   = { ControlKind::Axis, 3 };
            rate.response = AxisResponse::Rate;
            rate.maxSpeed = 512.0f;
            rate.inverted = true;

            pair.kind     = AxisBindingKind::DigitalPair;
            pair.negative = { ControlKind::DpadUp, 0 };
            pair.positive = { ControlKind::DpadDown, 0 };

            trigger.control   = { ControlKind::Trigger, 1 };
            trigger.threshold = 0.12f;

            axisButton.control           = { ControlKind::Axis, 2 };
            axisButton.threshold         = 0.5f;
            axisButton.negativeDirection = true;

            mapping.pdl0.push_back (rate);
            mapping.pdl1.push_back (pair);
            mapping.pb0.push_back  ({ { ControlKind::Button, 0 } });
            mapping.pb0.push_back  (trigger);
            mapping.pb1.push_back  (axisButton);
            mapping.pb2.push_back  ({ { ControlKind::Button, 4 } });
            return mapping;
        }


        TEST_METHOD (DefaultProfileAndDeadZone_RoundTripThroughTheGlobalPrefsFile)
        {
            InMemoryFileSystem        fs;
            GlobalUserPrefs           saved;
            GlobalUserPrefs           loaded;
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            ControllerModelSettings   settings;
            std::vector<std::string>  rejected;
            std::string               token = ControllerTokens::ModelToToken (Stick());

            settings.deadzone = 0.3f;
            settings.profiles.push_back ({ "Default", ControllerProfileKind::Default, MakeFullMapping() });
            store.models[token] = settings;

            saved.controllers = store.ToJson (saved.controllers);
            AssertSucceeded (saved.Save  (L"C:\\Casso", fs));
            AssertSucceeded (loaded.Load (L"C:\\Casso", fs));

            readBack.FromJson (loaded.controllers, rejected);

            Assert::IsTrue   (rejected.empty());
            Assert::AreEqual (0.3f, readBack.models.at (token).deadzone, 0.0001f, L"the model's dead zone comes back");
            Assert::IsTrue   (readBack.models.at (token).profiles[0].mapping == MakeFullMapping(),
                L"every binding comes back: rate response and speed, inversion, a D-pad pair, thresholds, a direction and PB2");
            Assert::IsTrue   (readBack.models.at (token).FindDefaultProfile() != nullptr);
        }


        TEST_METHOD (Pdl2AndPdl3_RoundTripThroughTheGlobalPrefsFile)
        {
            InMemoryFileSystem        fs;
            GlobalUserPrefs           saved;
            GlobalUserPrefs           loaded;
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            ControllerModelSettings   settings;
            ControlMapping            mapping = MakeFullMapping();
            AxisBinding               rightX;
            AxisBinding               pair;
            std::vector<std::string>  rejected;
            std::string               token   = ControllerTokens::ModelToToken (Stick());

            rightX.analog   = { ControlKind::Axis, XInputSampleDecoder::kRightStickX };
            pair.kind       = AxisBindingKind::DigitalPair;
            pair.negative   = { ControlKind::Button, 2 };
            pair.positive   = { ControlKind::Button, 3 };

            mapping.pdl2.push_back (rightX);
            mapping.pdl3.push_back (pair);

            settings.profiles.push_back ({ "Default", ControllerProfileKind::Default, mapping });
            store.models[token] = settings;

            saved.controllers = store.ToJson (saved.controllers);
            AssertSucceeded (saved.Save  (L"C:\\Casso", fs));
            AssertSucceeded (loaded.Load (L"C:\\Casso", fs));

            readBack.FromJson (loaded.controllers, rejected);

            Assert::IsTrue (rejected.empty());
            Assert::IsTrue (readBack.models.at (token).profiles[0].mapping == mapping, L"bindings on PDL2 and PDL3 come back");
        }


        TEST_METHOD (Pdl2AndPdl3_AbsentReadsEmptyAndEmptyIsNotWritten)
        {
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            ControllerModelSettings   settings;
            std::vector<std::string>  rejected;
            std::string               token    = ControllerTokens::ModelToToken (Stick());
            JsonValue                 written;
            const JsonValue         * models   = nullptr;
            const JsonValue         * model    = nullptr;
            const JsonValue         * profiles = nullptr;
            const JsonValue         * mapping  = nullptr;
            const JsonValue         * axes     = nullptr;

            settings.profiles.push_back ({ "Default", ControllerProfileKind::Default, MakeFullMapping() });
            store.models[token] = settings;

            written = store.ToJson (JsonValue());

            Assert::IsTrue    (written.HasObject ("models", models));
            Assert::IsNotNull (models);
            Assert::IsTrue    (models->HasObject (token, model));
            Assert::IsNotNull (model);
            Assert::IsTrue    (model->HasArray ("profiles", profiles));
            Assert::IsNotNull (profiles);
            Assert::IsTrue    (profiles->GetArrayElement (0).HasObject ("mapping", mapping));
            Assert::IsNotNull (mapping);
            Assert::IsTrue  (mapping->HasArray ("pdl0", axes), L"PDL0 is written as before");
            Assert::IsFalse (mapping->HasArray ("pdl2", axes), L"an unbound PDL2 is not written, so the file reads as it did before four axes");
            Assert::IsFalse (mapping->HasArray ("pdl3", axes));

            readBack.FromJson (written, rejected);

            Assert::IsTrue (rejected.empty());
            Assert::IsTrue (readBack.models.at (token).profiles[0].mapping.pdl2.empty(), L"absent reads back as empty");
            Assert::IsTrue (readBack.models.at (token).profiles[0].mapping == MakeFullMapping());
        }


        TEST_METHOD (Pdl2_AnUnreadableBindingRejectsItsProfile)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               token = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc   = Parse (
                "{\"models\":{\"" + token + "\":{\"profiles\":["
                  "{\"name\":\"Default\",\"default\":true,\"mapping\":{}},"
                  "{\"name\":\"Broken\",\"mapping\":{\"pdl3\":[{\"analog\":\"axis:0\",\"negative\":\"button:1\",\"positive\":\"button:2\"}]}}"
                "]}}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (1), rejected.size(), L"a PDL3 binding is held to the same rules as PDL0");
            Assert::AreEqual (size_t (1), store.models.at (token).profiles.size());
        }


        TEST_METHOD (OutOfRangeValues_AreClamped)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               token = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc   = Parse (
                "{\"models\":{\"" + token + "\":{\"deadzone\":2.5,\"profiles\":[{\"name\":\"Default\",\"default\":true,"
                "\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\",\"response\":\"rate\",\"maxSpeed\":5000}]}}]}}}");

            store.FromJson (doc, rejected);

            Assert::IsTrue   (rejected.empty(), L"a value out of range is clamped, not rejected");
            Assert::AreEqual (DeadzoneShaper::kMaxDeadzone, store.models.at (token).deadzone, 0.0001f);
            Assert::AreEqual (ControllerProfileStore::kMaxMaxSpeed, store.models.at (token).profiles[0].mapping.pdl0[0].maxSpeed, 0.0001f);
        }


        //  A Rate binding saved without a speed plays at the default, 768 per
        //  second: one sweep of the paddle in about a third of a second.
        TEST_METHOD (RateWithoutASpeed_ReadsTheDefaultSpeed)
        {
            constexpr float           kOwnersSpeed = 768.0f;
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               token        = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc          = Parse (
                "{\"models\":{\"" + token + "\":{\"profiles\":[{\"name\":\"Default\",\"default\":true,"
                "\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\",\"response\":\"rate\"}]}}]}}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (kOwnersSpeed, AxisBinding::kDefaultMaxSpeed, 0.0001f, L"the default speed");
            Assert::AreEqual (kOwnersSpeed, store.models.at (token).profiles[0].mapping.pdl0[0].maxSpeed, 0.0001f, L"a Rate binding with no speed saved");
        }


        TEST_METHOD (AnUnreadableProfile_IsDroppedAndReportedWhileTheRestLoad)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               token = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc   = Parse (
                "{\"models\":{\"" + token + "\":{\"deadzone\":0.2,\"profiles\":["
                  "{\"name\":\"Default\",\"default\":true,\"mapping\":{\"pb0\":[{\"control\":\"button:0\"}]}},"
                  "{\"name\":\"Broken\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\",\"negative\":\"button:1\",\"positive\":\"button:2\"}]}},"
                  "{\"name\":\"Nonsense\",\"mapping\":{\"pb0\":[{\"control\":\"lever:9\"}]}}"
                "]}}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (2), rejected.size(), L"a binding with both an analog control and a pair, and an unknown control, are each reported");
            Assert::AreEqual (size_t (1), store.models.at (token).profiles.size(), L"the readable profile still loads");
            Assert::AreEqual (std::string ("Default"), store.models.at (token).profiles[0].name);
        }


        TEST_METHOD (ADuplicateProfileName_IgnoringCase_IsDropped)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               token = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc   = Parse (
                "{\"models\":{\"" + token + "\":{\"profiles\":["
                  "{\"name\":\"Lode Runner\",\"mapping\":{}},"
                  "{\"name\":\"LODE RUNNER\",\"mapping\":{}}"
                "]}}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (1), rejected.size());
            Assert::AreEqual (size_t (1), store.models.at (token).profiles.size());
            Assert::AreEqual (std::string ("Lode Runner"), store.models.at (token).profiles[0].name, L"the later duplicate is the one dropped");
        }


        TEST_METHOD (AMissingDefaultProfile_PlaysWithTheDefaultMapping)
        {
            ControllerProfileStore    store;
            ControllerModelSettings   settings;
            ControlMapping            mapping;
            float                     deadzone = 0.0f;
            std::vector<ControlId>    controls = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Button, 0 } };

            settings.deadzone = 0.4f;
            settings.profiles.push_back ({ "Lode Runner", ControllerProfileKind::User, ControlMapping() });
            store.models[ControllerTokens::ModelToToken (Stick())] = settings;

            store.GetDefaultSettings (Stick(), ControllerFormFactor::Gamepad, controls, mapping, deadzone);

            Assert::IsTrue   (mapping == DefaultMapping::For (Stick(), controls), L"with no Default profile, the default mapping is the Default");
            Assert::AreEqual (0.4f, deadzone, 0.0001f, L"but the model's own dead zone still applies");
        }


        TEST_METHOD (AModelNeverEdited_PlaysWithTheBuiltInDefaults)
        {
            ControllerProfileStore  store;
            ControlMapping          mapping;
            float                   deadzone = 0.0f;
            std::vector<ControlId>  controls = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 } };

            store.GetDefaultSettings (Stick(), ControllerFormFactor::Gamepad, controls, mapping, deadzone);

            Assert::IsTrue   (mapping == DefaultMapping::For (Stick(), controls));
            Assert::AreEqual (DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput), deadzone, 0.0001f);
        }


        static ControllerModelKey Xbox()
        {
            return { ControllerKind::XInput, 0, 0 };
        }


        TEST_METHOD (BuiltInProfiles_CanNeitherBeDeletedNorRenamed)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls());

            Assert::IsTrue (settings.DeleteProfile ("Default") == ProfileEditResult::IsBuiltInProfile);
            Assert::IsTrue (settings.RenameProfile ("default", "Main") == ProfileEditResult::IsBuiltInProfile);
            Assert::IsTrue (settings.DeleteProfile ("Joyport") == ProfileEditResult::IsBuiltInProfile);
            Assert::IsTrue (settings.RenameProfile ("JOYPORT", "Atari") == ProfileEditResult::IsBuiltInProfile);
            Assert::IsTrue (settings.DeleteProfile ("Paddles") == ProfileEditResult::IsBuiltInProfile);
            Assert::IsTrue (settings.RenameProfile ("paddles", "Pong") == ProfileEditResult::IsBuiltInProfile);
            Assert::AreEqual (size_t (3), settings.profiles.size());
            Assert::AreEqual (std::string ("Default"), settings.profiles[0].name);
            Assert::AreEqual (std::string ("Joyport"), settings.profiles[1].name);
            Assert::AreEqual (std::string ("Paddles"), settings.profiles[2].name);
            Assert::IsTrue (settings.DeleteProfile ("Missing") == ProfileEditResult::NotFound);
        }


        //  The three built-in profiles exist for every model, whatever its
        //  kind and form factor, each of its own kind; none can be renamed or
        //  deleted by any case of its name; and each resets to its own
        //  built-in mapping.
        TEST_METHOD (BuiltInProfiles_ExistForEveryModelAndResetToTheirOwnMappings)
        {
            const std::vector<ControlId>             stickControls = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Axis, DefaultMapping::kAxisZ },
                                                                       { ControlKind::Axis, DefaultMapping::kAxisRz }, { ControlKind::Button, 0 }, { ControlKind::Button, 1 } };
            const ControllerModelKey                 keys[]        = { Xbox(), Stick(), Stick() };
            const ControllerFormFactor               formFactors[] = { ControllerFormFactor::Gamepad, ControllerFormFactor::Joystick, ControllerFormFactor::Gamepad };
            const std::vector<ControlId>             controls[]    = { XInputSampleDecoder::ListControls(), stickControls, stickControls };
            size_t                                   i             = 0;



            for (i = 0; i < std::size (keys); i++)
            {
                ControllerProfileStore     store;
                ControllerModelSettings &  settings        = store.GetOrCreateModel (keys[i], formFactors[i], controls[i]);
                ControllerProfile       *  normal          = settings.FindBuiltInProfile (ControllerProfileKind::Default);
                ControllerProfile       *  joyport         = settings.FindBuiltInProfile (ControllerProfileKind::Joyport);
                ControllerProfile       *  paddles         = settings.FindBuiltInProfile (ControllerProfileKind::Paddles);
                ControlMapping             expectedNormal  = ControllerModelSettings::MakeBuiltInMapping (ControllerProfileKind::Default, keys[i], formFactors[i], controls[i]);
                ControlMapping             expectedJoyport = ControllerModelSettings::MakeBuiltInMapping (ControllerProfileKind::Joyport, keys[i], formFactors[i], controls[i]);
                ControlMapping             expectedPaddles = ControllerModelSettings::MakeBuiltInMapping (ControllerProfileKind::Paddles, keys[i], formFactors[i], controls[i]);



                Assert::IsNotNull (normal,  L"every model has a Default");
                Assert::IsNotNull (joyport, L"and a Joyport profile");
                Assert::IsNotNull (paddles, L"and a Paddles profile");
                Assert::IsTrue    (normal->mode  == ProfileMode::Joystick);
                Assert::IsTrue    (joyport->mode == ProfileMode::Joyport);
                Assert::IsTrue    (paddles->mode == ProfileMode::Paddle);
                Assert::IsTrue    (expectedPaddles == DefaultMapping::MakePaddles (keys[i], formFactors[i], controls[i]), L"the Paddles mapping is the old Paddles starting point");

                for (const char * name : { "Default", "DEFAULT", "Joyport", "joyport", "Paddles", "PADDLES" })
                {
                    Assert::IsTrue (settings.RenameProfile (name, "Renamed") == ProfileEditResult::IsBuiltInProfile);
                    Assert::IsTrue (settings.DeleteProfile (name)            == ProfileEditResult::IsBuiltInProfile);
                }

                normal->mapping  = MakeFullMapping();
                joyport->mapping = MakeFullMapping();
                paddles->mapping = MakeFullMapping();

                Assert::IsTrue (store.ResetProfile (keys[i], formFactors[i], controls[i], "Default") == ProfileEditResult::Ok);
                Assert::IsTrue (store.ResetProfile (keys[i], formFactors[i], controls[i], "Joyport") == ProfileEditResult::Ok);
                Assert::IsTrue (store.ResetProfile (keys[i], formFactors[i], controls[i], "Paddles") == ProfileEditResult::Ok);
                Assert::IsTrue (settings.FindBuiltInProfile (ControllerProfileKind::Default)->mapping == expectedNormal,  L"the Default resets to the default mapping");
                Assert::IsTrue (settings.FindBuiltInProfile (ControllerProfileKind::Joyport)->mapping == expectedJoyport, L"the Joyport profile to the Joyport mapping");
                Assert::IsTrue (settings.FindBuiltInProfile (ControllerProfileKind::Paddles)->mapping == expectedPaddles, L"the Paddles profile to the Paddles mapping");
                Assert::IsTrue (expectedNormal != expectedJoyport, L"which differ, or the reset would prove nothing");
                Assert::IsTrue (expectedNormal != expectedPaddles);
            }
        }


        //  A machine's active profile saved by an earlier build passes to that
        //  machine's saved controller as its Joystick choice, once: only
        //  while the controller has none of its own, a choice of the Default
        //  included, and never without a saved controller.
        TEST_METHOD (LegacyProfile_PassesOnceAsTheJoystickChoice)
        {
            ControllerUnitKey                   unit;
            std::string                         token   = StickToken ("{STICK}");
            std::map<std::string, std::string>  profiles;



            AssertSucceeded (ControllerTokens::UnitFromToken (token, unit));

            Assert::IsFalse (ControllerProfileStore::TryAdoptLegacyProfile (profiles, std::nullopt, "Lode Runner"), L"no saved controller, nothing to pass to");
            Assert::IsFalse (ControllerProfileStore::TryAdoptLegacyProfile (profiles, unit, ""),                     L"no legacy profile");
            Assert::IsTrue  (profiles.empty());

            Assert::IsTrue   (ControllerProfileStore::TryAdoptLegacyProfile (profiles, unit, "Lode Runner"));
            Assert::AreEqual (std::string ("Lode Runner"), profiles.at (token));

            profiles[token] = "Flight";
            Assert::IsFalse  (ControllerProfileStore::TryAdoptLegacyProfile (profiles, unit, "Lode Runner"), L"a later launch passes nothing over the controller's own choice");
            Assert::AreEqual (std::string ("Flight"), profiles.at (token));

            profiles[token] = "";
            Assert::IsFalse  (ControllerProfileStore::TryAdoptLegacyProfile (profiles, unit, "Lode Runner"), L"a choice of the Default is a choice");
            Assert::AreEqual (std::string(), profiles.at (token));
        }


        TEST_METHOD (ProfileNames_AreTrimmedAndOneToFortyCharacters)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls());
            std::string                forty (ControllerModelSettings::kMaxProfileNameLength, 'a');
            std::string                fortyOne (ControllerModelSettings::kMaxProfileNameLength + 1, 'b');

            Assert::IsTrue   (settings.AddProfile ("", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::EmptyName);
            Assert::IsTrue   (settings.AddProfile (" \t ", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::EmptyName, L"whitespace alone is empty");
            Assert::IsTrue   (settings.AddProfile (fortyOne, ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::NameTooLong);
            Assert::IsTrue   (settings.AddProfile ("  " + forty + "  ", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::Ok, L"length is measured after trimming");
            Assert::IsTrue   (settings.AddProfile ("  Flight  ", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.FindProfile ("Flight") != nullptr, L"and the name is stored trimmed");
            Assert::AreEqual (std::string ("Flight"), settings.FindProfile ("flight")->name);
            Assert::IsTrue   (settings.RenameProfile ("Flight", "   ") == ProfileEditResult::EmptyName);
            Assert::IsTrue   (settings.RenameProfile ("Flight", fortyOne) == ProfileEditResult::NameTooLong);
        }


        TEST_METHOD (ProfileNames_AreUniqueIgnoringCase)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls());

            Assert::IsTrue   (settings.AddProfile ("Flight", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.AddProfile ("Lode Runner", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.AddProfile ("FLIGHT", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::DuplicateName);
            Assert::IsTrue   (settings.AddProfile ("default", ControlMapping(), ProfileMode::Joystick) == ProfileEditResult::DuplicateName, L"never a second Default");
            Assert::IsTrue   (settings.RenameProfile ("Lode Runner", " flight ") == ProfileEditResult::DuplicateName);
            Assert::IsTrue   (settings.RenameProfile ("Flight", "FLIGHT") == ProfileEditResult::Ok, L"a profile may change the case of its own name");
            Assert::AreEqual (std::string ("FLIGHT"), settings.FindProfile ("flight")->name);
            Assert::AreEqual (size_t (5), settings.profiles.size());
        }


        TEST_METHOD (CreateProfile_FromTheDefaultACopyOrPaddles)
        {
            ControllerProfileStore         store;
            std::vector<ControlId>         controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings      & settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);
            int                            defaults = 0;

            settings.AddProfile ("Flight", MakeFullMapping(), ProfileMode::Joystick);

            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Plain", ProfileSource::DefaultMapping, ProfileMode::Joystick) == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Flight 2", ProfileSource::CopyOfProfile, ProfileMode::Joystick, "flight") == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Pong", ProfileSource::PaddleMapping, ProfileMode::Paddle) == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Ghost", ProfileSource::CopyOfProfile, ProfileMode::Joystick, "Missing") == ProfileEditResult::NotFound);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "PONG", ProfileSource::PaddleMapping, ProfileMode::Paddle) == ProfileEditResult::DuplicateName);

            Assert::IsTrue (settings.FindProfile ("Plain")->mapping == DefaultMapping::For (Xbox(), controls));
            Assert::IsTrue (settings.FindProfile ("Flight 2")->mapping == MakeFullMapping(), L"a copy carries the source's mapping");
            Assert::IsTrue (settings.FindProfile ("Pong")->mapping == DefaultMapping::MakePaddles (Xbox(), ControllerFormFactor::Gamepad, controls));
            Assert::IsTrue (settings.FindProfile ("Ghost") == nullptr);

            for (const ControllerProfile & profile : settings.profiles)
            {
                defaults += profile.kind == ControllerProfileKind::Default ? 1 : 0;
            }

            Assert::AreEqual (1, defaults, L"creating profiles never adds a second Default");
        }


        TEST_METHOD (ResetProfile_RestoresTheDefaultMapping)
        {
            ControllerProfileStore         store;
            std::vector<ControlId>         controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings      & settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);

            settings.AddProfile ("Flight", MakeFullMapping(), ProfileMode::Joystick);
            settings.profiles[0].mapping = MakeFullMapping();

            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Flight") == ProfileEditResult::Ok);
            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Default") == ProfileEditResult::Ok, L"the Default can be reset");
            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Missing") == ProfileEditResult::NotFound);
            Assert::IsTrue (settings.FindProfile ("Flight")->mapping == DefaultMapping::For (Xbox(), controls));
            Assert::IsTrue (settings.FindDefaultProfile()->mapping == DefaultMapping::For (Xbox(), controls));
        }


        TEST_METHOD (ResetProfile_RestoresTheJoyportProfileToItsOwnMapping)
        {
            ControllerProfileStore         store;
            std::vector<ControlId>         controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings      & settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);
            ControllerProfile            * joyport  = settings.FindBuiltInProfile (ControllerProfileKind::Joyport);

            Assert::IsNotNull (joyport, L"every model has a Joyport profile");
            Assert::IsTrue    (joyport->mapping == DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, controls), L"made from the Joyport mapping");

            joyport->mapping = MakeFullMapping();

            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "joyport") == ProfileEditResult::Ok);
            Assert::IsTrue (joyport->mapping == DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, controls), L"not to the Default's mapping");
        }


        // One profile object as ToJson wrote it, or null.
        static const JsonValue * FindWrittenProfile (const JsonValue & written, const std::string & token, const std::string & name)
        {
            const JsonValue *  models   = nullptr;
            const JsonValue *  model    = nullptr;
            const JsonValue *  profiles = nullptr;
            std::string        found;

            if (!written.HasObject ("models", models) || models == nullptr ||
                !models->HasObject (token, model)     || model == nullptr  ||
                !model->HasArray ("profiles", profiles) || profiles == nullptr)
            {
                return nullptr;
            }

            for (size_t i = 0; i < profiles->GetArraySize(); i++)
            {
                if (profiles->GetArrayElement (i).HasString ("name", found) && found == name)
                {
                    return &profiles->GetArrayElement (i);
                }
            }

            return nullptr;
        }


        //  Every profile is saved with its kind, Joystick included, so a
        //  Joystick profile that binds PDL0 alone is not read back as a
        //  Paddle one the way a profile saved before kinds would be.
        TEST_METHOD (ProfileMode_IsSavedForEveryKindAndReadBack)
        {
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            std::vector<std::string>  rejected;
            std::vector<ControlId>    controls = XInputSampleDecoder::ListControls();
            std::string               token    = ControllerTokens::ModelToToken (Xbox());
            JsonValue                 written;
            const JsonValue         * atari    = nullptr;
            const JsonValue         * apple    = nullptr;
            const JsonValue         * pong     = nullptr;
            ControlMapping            oneAxis;
            std::string               mode;

            oneAxis.pdl0.push_back ({});

            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Atari", ProfileSource::JoyportMapping, ProfileMode::Joyport)  == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Apple", ProfileSource::DefaultMapping, ProfileMode::Joystick) == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Pong",  ProfileSource::PaddleMapping,  ProfileMode::Paddle)   == ProfileEditResult::Ok);
            Assert::IsTrue (store.models.at (token).AddProfile ("Knob", oneAxis, ProfileMode::Joystick) == ProfileEditResult::Ok);

            written = store.ToJson (JsonValue());
            atari   = FindWrittenProfile (written, token, "Atari");
            apple   = FindWrittenProfile (written, token, "Apple");
            pong    = FindWrittenProfile (written, token, "Pong");

            Assert::IsNotNull (atari);
            Assert::IsNotNull (apple);
            Assert::IsNotNull (pong);
            Assert::IsTrue    (atari->HasString ("profileMode", mode), L"a Joyport profile writes its kind");
            Assert::AreEqual  (std::string ("joyport"), mode);
            Assert::IsTrue    (apple->HasString ("profileMode", mode), L"a Joystick profile writes its kind");
            Assert::AreEqual  (std::string ("joystick"), mode);
            Assert::IsTrue    (pong->HasString ("profileMode", mode), L"a Paddle profile writes its kind");
            Assert::AreEqual  (std::string ("paddle"), mode);

            readBack.FromJson (written, rejected);

            Assert::IsTrue (rejected.empty());
            Assert::IsTrue (readBack.models.at (token).FindProfile ("Atari")->mode == ProfileMode::Joyport, L"and each reads back as its kind");
            Assert::IsTrue (readBack.models.at (token).FindProfile ("Apple")->mode == ProfileMode::Joystick);
            Assert::IsTrue (readBack.models.at (token).FindProfile ("Pong")->mode  == ProfileMode::Paddle);
            Assert::IsTrue (readBack.models.at (token).FindProfile ("Knob")->mode  == ProfileMode::Joystick, L"a Joystick profile binding PDL0 alone stays one");
        }


        //  A profile saved before profiles had three kinds carries no kind,
        //  or "joyport". One that binds PDL0 and not PDL1, as the Paddles
        //  starting point made them, becomes a Paddle profile; the rest become
        //  Joystick profiles; and a Joyport profile stays one. An
        //  unrecognized kind is read the same way, and the profile is kept.
        TEST_METHOD (ProfileMode_AbsentOrUnknownIsClassifiedByTheMappingAndKeepsTheProfile)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               token = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc   = Parse (
                "{\"models\":{\"" + token + "\":{\"profiles\":["
                  "{\"name\":\"Older\",\"mapping\":{}},"
                  "{\"name\":\"Flight\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\"}],\"pdl1\":[{\"analog\":\"axis:1\"}]}},"
                  "{\"name\":\"Pong\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\",\"response\":\"rate\",\"maxSpeed\":256}],\"pb0\":[{\"control\":\"button:0\"}]}},"
                  "{\"name\":\"Tilt\",\"mapping\":{\"pdl1\":[{\"analog\":\"axis:1\"}]}},"
                  "{\"name\":\"Odd\",\"profileMode\":\"sideways\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\"}]}},"
                  "{\"name\":\"Atari\",\"profileMode\":\"joyport\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\"}]}}"
                "]}}}");

            store.FromJson (doc, rejected);

            Assert::IsTrue   (rejected.empty(), L"an unknown kind rejects nothing");
            Assert::AreEqual (size_t (6), store.models.at (token).profiles.size());
            Assert::IsTrue   (store.models.at (token).FindProfile ("Older")->mode  == ProfileMode::Joystick, L"a profile binding nothing");
            Assert::IsTrue   (store.models.at (token).FindProfile ("Flight")->mode == ProfileMode::Joystick, L"a profile binding PDL0 and PDL1");
            Assert::IsTrue   (store.models.at (token).FindProfile ("Pong")->mode   == ProfileMode::Paddle,   L"a profile binding PDL0 alone");
            Assert::IsTrue   (store.models.at (token).FindProfile ("Tilt")->mode   == ProfileMode::Joystick, L"a profile binding PDL1 alone");
            Assert::IsTrue   (store.models.at (token).FindProfile ("Odd")->mode    == ProfileMode::Paddle,   L"an unknown kind is classified the same way");
            Assert::IsTrue   (store.models.at (token).FindProfile ("Atari")->mode  == ProfileMode::Joyport,  L"a Joyport profile keeps its kind whatever it binds");
        }


        //  Before profiles had three kinds, a Paddle profile was chosen in the
        //  one map for play without the Joyport. The choice moves to the
        //  Paddle map, unless the controller has a Paddle choice already, and
        //  leaves the Joystick map either way; a Joystick choice stays put.
        TEST_METHOD (ActiveProfiles_ALegacyPaddleChoiceMovesToThePaddleMap)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               model  = ControllerTokens::ModelToToken (Stick());
            std::string               first  = StickToken ("{FIRST}");
            std::string               second = StickToken ("{SECOND}");
            std::string               third  = StickToken ("{THIRD}");
            std::string               fourth = StickToken ("{FOURTH}");
            JsonValue                 doc    = Parse (
                "{\"models\":{\"" + model + "\":{\"profiles\":["
                  "{\"name\":\"Pong\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\"}]}},"
                  "{\"name\":\"Flight\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:0\"}],\"pdl1\":[{\"analog\":\"axis:1\"}]}}"
                "]}},"
                "\"activeProfiles\":{\"" + first + "\":\"Pong\",\"" + second + "\":\"Flight\",\"" + third + "\":\"pong\",\"" + fourth + "\":\"Paddles\"},"
                "\"paddleActiveProfiles\":{\"" + third + "\":\"Breakout\"}}");

            store.FromJson (doc, rejected);

            Assert::IsTrue   (rejected.empty());
            Assert::AreEqual (std::string ("Pong"),     store.paddleActiveProfiles.at (first),  L"a Paddle choice moves to the Paddle map");
            Assert::AreEqual (std::string ("Flight"),   store.activeProfiles.at (second),       L"a Joystick choice stays");
            Assert::AreEqual (std::string ("Breakout"), store.paddleActiveProfiles.at (third),  L"a Paddle choice already made is kept");
            Assert::AreEqual (std::string ("Paddles"),  store.paddleActiveProfiles.at (fourth), L"the built-in Paddles profile moves by its name");
            Assert::AreEqual (size_t (1), store.activeProfiles.size(),                          L"and only the Joystick choice is left in the Joystick map");
        }


        //  The Default is a Joystick profile, Paddles a Paddle profile and the
        //  Joyport profile a Joyport one, whatever the file says.
        TEST_METHOD (ProfileMode_OfTheBuiltInProfilesComesFromTheirKind)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::string               token = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc   = Parse (
                "{\"models\":{\"" + token + "\":{\"profiles\":["
                  "{\"name\":\"Default\",\"default\":true,\"profileMode\":\"joyport\",\"mapping\":{}},"
                  "{\"name\":\"Joyport\",\"joyport\":true,\"mapping\":{}},"
                  "{\"name\":\"Paddles\",\"paddles\":true,\"profileMode\":\"joystick\",\"mapping\":{}}"
                "]}}}");

            store.FromJson (doc, rejected);

            Assert::IsTrue (rejected.empty());
            Assert::IsTrue (store.models.at (token).FindDefaultProfile()->mode == ProfileMode::Joystick);
            Assert::IsTrue (store.models.at (token).FindBuiltInProfile (ControllerProfileKind::Joyport)->mode == ProfileMode::Joyport);
            Assert::IsTrue (store.models.at (token).FindBuiltInProfile (ControllerProfileKind::Paddles)->mode == ProfileMode::Paddle);
        }


        //  A user profile saved as "Paddles" before Paddles was built in
        //  becomes the built-in one, keeping its mapping and becoming a
        //  Paddle profile, rather than the model gaining a second "Paddles".
        TEST_METHOD (AUserProfileCalledPaddles_BecomesTheBuiltInOne)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::vector<ControlId>    controls = XInputSampleDecoder::ListControls();
            std::string               token    = ControllerTokens::ModelToToken (Xbox());
            JsonValue                 doc      = Parse (
                "{\"models\":{\"" + token + "\":{\"profiles\":["
                  "{\"name\":\"Default\",\"default\":true,\"mapping\":{}},"
                  "{\"name\":\"paddles\",\"mapping\":{\"pdl0\":[{\"analog\":\"axis:1\"}],\"pdl1\":[{\"analog\":\"axis:0\"}]}}"
                "]}}}");
            ControllerModelSettings * settings = nullptr;
            size_t                    count    = 0;

            store.FromJson (doc, rejected);
            settings = &store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);

            for (const ControllerProfile & profile : settings->profiles)
            {
                count += _stricmp (profile.name.c_str(), "Paddles") == 0 ? 1 : 0;
            }

            Assert::AreEqual (size_t (1), count, L"no second Paddles profile");
            Assert::IsTrue   (settings->FindProfile ("Paddles")->kind == ControllerProfileKind::Paddles);
            Assert::IsTrue   (settings->FindProfile ("Paddles")->mode == ProfileMode::Paddle);
            Assert::IsFalse  (settings->FindProfile ("Paddles")->mapping.pdl1.empty(), L"the user's mapping is kept");
        }


        //  Each kind lists its own built-in profile first and then only its
        //  own profiles, in the order they are kept.
        TEST_METHOD (GetProfileNames_ListsTheModesBuiltInFirstThenItsOwnProfiles)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls());

            settings.AddProfile ("Flight",   ControlMapping(), ProfileMode::Joystick);
            settings.AddProfile ("Atari",    ControlMapping(), ProfileMode::Joyport);
            settings.AddProfile ("Pong",     ControlMapping(), ProfileMode::Paddle);
            settings.AddProfile ("Boulder",  ControlMapping(), ProfileMode::Joyport);
            settings.AddProfile ("Choplift", ControlMapping(), ProfileMode::Joystick);
            settings.AddProfile ("Warlords", ControlMapping(), ProfileMode::Paddle);

            Assert::IsTrue (settings.GetProfileNames (ProfileMode::Joystick) == std::vector<std::string> { "Default", "Flight", "Choplift" });
            Assert::IsTrue (settings.GetProfileNames (ProfileMode::Paddle)   == std::vector<std::string> { "Paddles", "Pong", "Warlords" });
            Assert::IsTrue (settings.GetProfileNames (ProfileMode::Joyport)  == std::vector<std::string> { "Joyport", "Atari", "Boulder" });
            Assert::IsTrue (ControllerModelSettings().GetProfileNames (ProfileMode::Joyport) == std::vector<std::string> { "Joyport" },
                L"a model with nothing saved still lists the built-in profile it plays");
            Assert::IsTrue (ControllerModelSettings().GetProfileNames (ProfileMode::Paddle) == std::vector<std::string> { "Paddles" });
        }


        //  The new profile belongs to the mode in effect whichever of that
        //  mode's starting points it comes from. A built-in mapping of the
        //  other mode, or a copy of the other mode's profile, is refused.
        TEST_METHOD (CreateProfile_FromEverySourceInEachMode_StampsTheModeInEffect)
        {
            //  Each case's source, and the kind it belongs to: a built-in
            //  mapping's own kind, or the kind of the profile it copies.
            struct Case
            {
                ProfileSource  source;
                const char   * pszSourceName;
                ProfileMode    kind;
            };

            const Case                    cases[] =
            {
                { ProfileSource::DefaultMapping, "",       ProfileMode::Joystick },
                { ProfileSource::PaddleMapping,  "",       ProfileMode::Paddle   },
                { ProfileSource::JoyportMapping, "",       ProfileMode::Joyport  },
                { ProfileSource::CopyOfProfile,  "Flight", ProfileMode::Joystick },
                { ProfileSource::CopyOfProfile,  "Pong",   ProfileMode::Paddle   },
                { ProfileSource::CopyOfProfile,  "Atari",  ProfileMode::Joyport  },
            };
            const ProfileMode             modes[]  = { ProfileMode::Joystick, ProfileMode::Paddle, ProfileMode::Joyport };
            ControllerProfileStore        store;
            std::vector<ControlId>        controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings     & settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);
            ControlMapping                flight   = MakeFullMapping();
            ControlMapping                pong;
            ControlMapping                atari;
            ControlMapping                expected;
            int                           created  = 0;
            int                           made     = 0;

            atari.pb0.push_back ({ { ControlKind::Button, 3 } });
            pong.pb0.push_back  ({ { ControlKind::Button, 2 } });

            settings.AddProfile ("Flight", flight, ProfileMode::Joystick);
            settings.AddProfile ("Pong",   pong,   ProfileMode::Paddle);
            settings.AddProfile ("Atari",  atari,  ProfileMode::Joyport);

            for (ProfileMode mode : modes)
            {
                for (const Case & c : cases)
                {
                    std::string  name = "New " + std::to_string (created++);

                    if (c.kind != mode)
                    {
                        Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, name, c.source, mode, c.pszSourceName) == ProfileEditResult::NotFound,
                                        L"another kind's starting point is refused");
                        Assert::IsTrue (settings.FindProfile (name) == nullptr);
                        continue;
                    }

                    switch (c.source)
                    {
                        case ProfileSource::DefaultMapping: expected = DefaultMapping::For (Xbox(), controls);                                      break;
                        case ProfileSource::JoyportMapping: expected = DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, controls); break;
                        case ProfileSource::PaddleMapping:  expected = DefaultMapping::MakePaddles (Xbox(), ControllerFormFactor::Gamepad, controls);                              break;
                        case ProfileSource::CopyOfProfile:  expected = settings.FindProfile (c.pszSourceName)->mapping;                             break;
                    }

                    Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, name, c.source, mode, c.pszSourceName) == ProfileEditResult::Ok);
                    Assert::IsTrue (settings.FindProfile (name)->mode == mode,        L"the new profile belongs to the kind in effect");
                    Assert::IsTrue (settings.FindProfile (name)->mapping == expected, L"and starts from the source's mapping");
                    made++;
                }
            }

            Assert::AreEqual (18, created, L"every source was tried for every kind");
            Assert::AreEqual (6,  made,    L"and each was made for its own kind");
        }


        TEST_METHOD (CreateProfile_ABuiltInStartingPointOfTheOtherMode_IsRefused)
        {
            ControllerProfileStore     store;
            std::vector<ControlId>     controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings  & settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);
            size_t                     count    = settings.profiles.size();

            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "A", ProfileSource::JoyportMapping, ProfileMode::Joystick) == ProfileEditResult::NotFound, L"the Joyport mapping for a Joystick profile");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "B", ProfileSource::DefaultMapping, ProfileMode::Joyport)  == ProfileEditResult::NotFound, L"the default mapping for a Joyport profile");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "C", ProfileSource::PaddleMapping,  ProfileMode::Joyport)  == ProfileEditResult::NotFound, L"the Paddles mapping for a Joyport profile");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "D", ProfileSource::PaddleMapping,  ProfileMode::Joystick) == ProfileEditResult::NotFound, L"the Paddles mapping for a Joystick profile");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "E", ProfileSource::DefaultMapping, ProfileMode::Paddle)   == ProfileEditResult::NotFound, L"the default mapping for a Paddle profile");
            Assert::AreEqual (count, settings.profiles.size(), L"and nothing is added");
        }


        TEST_METHOD (CreateProfile_ACopyOfTheOtherModesProfile_IsRefused)
        {
            ControllerProfileStore     store;
            std::vector<ControlId>     controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings  & settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);
            size_t                     count    = 0;

            settings.AddProfile ("Flight", MakeFullMapping(), ProfileMode::Joystick);
            settings.AddProfile ("Atari",  ControlMapping(),  ProfileMode::Joyport);
            count = settings.profiles.size();

            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "A", ProfileSource::CopyOfProfile, ProfileMode::Joystick,  "Atari")   == ProfileEditResult::NotFound, L"a Joyport-mode profile in normal mode");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "B", ProfileSource::CopyOfProfile, ProfileMode::Joystick,  "Joyport") == ProfileEditResult::NotFound, L"the Joyport profile in normal mode");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "C", ProfileSource::CopyOfProfile, ProfileMode::Joyport, "Flight")  == ProfileEditResult::NotFound, L"a normal-mode profile in Joyport mode");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "D", ProfileSource::CopyOfProfile, ProfileMode::Joyport, "default") == ProfileEditResult::NotFound, L"the Default in Joyport mode");
            Assert::AreEqual (count, settings.profiles.size(), L"and nothing is added");
            Assert::IsTrue   (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "E", ProfileSource::CopyOfProfile, ProfileMode::Joyport, "Joyport") == ProfileEditResult::Ok, L"a copy within the mode is made");
        }


        //  Names stay unique across both modes, so a profile of the other
        //  mode, or the other mode's built-in profile, holds its name.
        TEST_METHOD (CreateProfile_ANameUsedInTheOtherMode_IsADuplicate)
        {
            ControllerProfileStore  store;
            std::vector<ControlId>  controls = XInputSampleDecoder::ListControls();

            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Atari", ProfileSource::JoyportMapping, ProfileMode::Joyport) == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "ATARI", ProfileSource::DefaultMapping, ProfileMode::Joystick)  == ProfileEditResult::DuplicateName);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "joyport", ProfileSource::DefaultMapping, ProfileMode::Joystick) == ProfileEditResult::DuplicateName);
            Assert::IsTrue (store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "default", ProfileSource::JoyportMapping, ProfileMode::Joyport) == ProfileEditResult::DuplicateName);
        }


        //  A user profile resets to the built-in mapping of its own mode, and
        //  each built-in profile to its own.
        TEST_METHOD (ResetProfile_RestoresTheBuiltInMappingOfTheProfilesMode)
        {
            ControllerProfileStore         store;
            std::vector<ControlId>         controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings      & settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);

            settings.AddProfile ("Apple", MakeFullMapping(), ProfileMode::Joystick);
            settings.AddProfile ("Atari", MakeFullMapping(), ProfileMode::Joyport);
            settings.FindBuiltInProfile (ControllerProfileKind::Default)->mapping = MakeFullMapping();
            settings.FindBuiltInProfile (ControllerProfileKind::Joyport)->mapping = MakeFullMapping();

            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Apple")   == ProfileEditResult::Ok);
            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Atari")   == ProfileEditResult::Ok);
            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Default") == ProfileEditResult::Ok);
            Assert::IsTrue (store.ResetProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Joyport") == ProfileEditResult::Ok);

            Assert::IsTrue (settings.FindProfile ("Apple")->mapping   == DefaultMapping::For (Xbox(), controls),         L"a normal-mode profile to the Default's");
            Assert::IsTrue (settings.FindProfile ("Atari")->mapping   == DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, controls), L"a Joyport-mode profile to the Joyport profile's");
            Assert::IsTrue (settings.FindProfile ("Default")->mapping == DefaultMapping::For (Xbox(), controls));
            Assert::IsTrue (settings.FindProfile ("Joyport")->mapping == DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, controls));
        }


        //  Every stick and the D-pad steer, and every face button, bumper and
        //  trigger fires. Back, Start and the stick clicks do not.
        TEST_METHOD (JoyportMapping_SteersWithEverythingAndFiresWithEverything)
        {
            std::vector<ControlId>  controls = XInputSampleDecoder::ListControls();
            ControlMapping          mapping  = DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, controls);
            size_t                  triggers = 0;
            size_t                  buttons  = 0;

            Assert::AreEqual (size_t (3), mapping.pdl0.size(), L"left stick, right stick and D-pad on X");
            Assert::AreEqual (size_t (3), mapping.pdl1.size(), L"and on Y");
            Assert::IsTrue   (mapping.pdl0[2].kind == AxisBindingKind::DigitalPair);
            Assert::IsTrue   (mapping.pdl1[2].negative == ControlId { ControlKind::DpadUp, 0 }, L"up is toward 0, as on the sticks");

            for (const ButtonBinding & binding : mapping.pb0)
            {
                triggers += binding.control.kind == ControlKind::Trigger ? 1 : 0;
                buttons  += binding.control.kind == ControlKind::Button  ? 1 : 0;

                Assert::IsTrue (binding.control.kind != ControlKind::Button || binding.control.index < 6, L"no Back, Start or stick click");
            }

            Assert::AreEqual (size_t (2), triggers, L"both triggers fire");
            Assert::AreEqual (size_t (6), buttons,  L"A, B, X, Y and both bumpers fire");
        }


        static std::vector<ControlId> MakeAxes (std::initializer_list<int> axes)
        {
            std::vector<ControlId>  controls;

            for (int axis : axes)
            {
                controls.push_back ({ ControlKind::Axis, axis });
            }

            controls.push_back ({ ControlKind::Button, 0 });
            return controls;
        }


        static std::optional<std::pair<ControlId, ControlId>> MakePair (int x, int y)
        {
            return std::make_pair (ControlId { ControlKind::Axis, x }, ControlId { ControlKind::Axis, y });
        }


        //  A DirectInput gamepad's second stick is Z and Rz when it reports
        //  both, which is the common layout: there Rx and Ry are its triggers.
        TEST_METHOD (FindSecondStick_OnAGamepad_PrefersZAndRz)
        {
            std::optional<std::pair<ControlId, ControlId>>  pair = DefaultMapping::FindSecondStick (ControllerFormFactor::Gamepad, MakeAxes ({ 0, 1, 2, 3, 4, 5 }));

            Assert::IsTrue (pair == MakePair (DefaultMapping::kAxisZ, DefaultMapping::kAxisRz), L"Z and Rz, ahead of Rx and Ry");
        }


        //  An Xbox-class pad read through DirectInput reports its triggers as
        //  one Z axis and no Rz, and its right stick as Rx and Ry.
        TEST_METHOD (FindSecondStick_OnAGamepad_WithZButNoRz_TakesRxAndRy)
        {
            Assert::IsTrue (DefaultMapping::FindSecondStick (ControllerFormFactor::Gamepad, MakeAxes ({ 0, 1, 2, 3, 4 })) == MakePair (DefaultMapping::kAxisRx, DefaultMapping::kAxisRy),
                L"Z alone is a trigger axis, not half a stick");
            Assert::IsTrue (DefaultMapping::FindSecondStick (ControllerFormFactor::Gamepad, MakeAxes ({ 0, 1, 3, 4 })) == MakePair (DefaultMapping::kAxisRx, DefaultMapping::kAxisRy),
                L"Rx and Ry alone");
            Assert::IsFalse (DefaultMapping::FindSecondStick (ControllerFormFactor::Gamepad, MakeAxes ({ 0, 1, 2, 3 })).has_value(),
                L"neither pair whole is no second stick");
        }


        //  A joystick's or a wheel's other axes can be a throttle, a twist or
        //  pedals resting off center, so neither gets a second stick.
        TEST_METHOD (FindSecondStick_OnAJoystickOrWheel_IsNone)
        {
            Assert::IsFalse (DefaultMapping::FindSecondStick (ControllerFormFactor::Joystick, MakeAxes ({ 0, 1, 2, 3, 4, 5 })).has_value());
            Assert::IsFalse (DefaultMapping::FindSecondStick (ControllerFormFactor::Wheel,    MakeAxes ({ 0, 1, 2, 3, 4, 5 })).has_value());
        }


        //  The second stick is bound Absolute on PDL0 and PDL1 after the
        //  primary stick, as an Xbox controller's right stick is.
        TEST_METHOD (JoyportMapping_OnADirectInputGamepad_SteersWithTheSecondStick)
        {
            std::vector<ControlId>  controls = MakeAxes ({ 0, 1, 2, 5 });
            ControlMapping          mapping  = DefaultMapping::MakeJoyport (Stick(), ControllerFormFactor::Gamepad, controls);

            Assert::AreEqual (size_t (2), mapping.pdl0.size(), L"the primary stick and the second stick on X");
            Assert::AreEqual (size_t (2), mapping.pdl1.size(), L"and on Y");
            Assert::IsTrue   (mapping.pdl0[0].analog == ControlId { ControlKind::Axis, 0 }, L"the primary stick first");
            Assert::IsTrue   (mapping.pdl0[1].analog == ControlId { ControlKind::Axis, DefaultMapping::kAxisZ });
            Assert::IsTrue   (mapping.pdl1[1].analog == ControlId { ControlKind::Axis, DefaultMapping::kAxisRz });
            Assert::IsTrue   (mapping.pdl0[1].response == AxisResponse::Absolute);
            Assert::IsTrue   (mapping.pdl1[1].response == AxisResponse::Absolute);
        }


        //  An Xbox controller keeps its right stick whatever the form factor
        //  passed, since XInput's layout is fixed.
        TEST_METHOD (JoyportMapping_OnAnXboxController_KeepsItsRightStick)
        {
            ControlMapping  mapping = DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls());

            Assert::IsTrue (mapping.pdl0[1].analog == ControlId { ControlKind::Axis, XInputSampleDecoder::kRightStickX });
            Assert::IsTrue (mapping.pdl1[1].analog == ControlId { ControlKind::Axis, XInputSampleDecoder::kRightStickY });
        }


        //  The Joyport reads neither PB1 nor PB2, so nothing is put on them.
        TEST_METHOD (JoyportMapping_BindsNothingToPb1OrPb2)
        {
            ControlMapping  xbox  = DefaultMapping::MakeJoyport (Xbox(),  ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls());
            ControlMapping  stick = DefaultMapping::MakeJoyport (Stick(), ControllerFormFactor::Joystick, XInputSampleDecoder::ListControls());



            Assert::IsTrue (xbox.pb1.empty(),  L"B is not left on PB1");
            Assert::IsTrue (xbox.pb2.empty());
            Assert::IsTrue (stick.pb1.empty());
            Assert::IsTrue (stick.pb2.empty());
        }


        //  A trigger rests at one end of its travel, so on no kind of
        //  controller does one steer: an Xbox controller's triggers only fire.
        TEST_METHOD (JoyportMapping_NoTriggerEverSteers)
        {
            const ControllerFormFactor  formFactors[] = { ControllerFormFactor::Gamepad, ControllerFormFactor::Joystick, ControllerFormFactor::Wheel };
            const ControllerModelKey    models[]      = { Xbox(), Stick() };
            std::vector<ControlId>      controls      = XInputSampleDecoder::ListControls();
            size_t                      checked       = 0;

            controls.push_back ({ ControlKind::Axis, DefaultMapping::kAxisZ });
            controls.push_back ({ ControlKind::Axis, DefaultMapping::kAxisRz });

            for (const ControllerModelKey & model : models)
            {
                for (ControllerFormFactor formFactor : formFactors)
                {
                    ControlMapping                      mapping = DefaultMapping::MakeJoyport (model, formFactor, controls);
                    const std::vector<AxisBinding> *    axes[]  = { &mapping.pdl0, &mapping.pdl1, &mapping.pdl2, &mapping.pdl3 };

                    for (const std::vector<AxisBinding> * bindings : axes)
                    {
                        for (const AxisBinding & binding : *bindings)
                        {
                            Assert::IsTrue (binding.analog.kind   != ControlKind::Trigger || binding.kind != AxisBindingKind::Analog, L"no trigger on an axis");
                            Assert::IsTrue (binding.negative.kind != ControlKind::Trigger && binding.positive.kind != ControlKind::Trigger);
                            checked++;
                        }
                    }
                }
            }

            Assert::IsTrue (checked > 0, L"some bindings were checked");
        }


        //  A DirectInput joystick's higher axes can be pedals or a throttle
        //  resting hard over, so only its primary stick and D-pad steer.
        TEST_METHOD (JoyportMapping_OnADirectInputJoystickLeavesTheOtherAxesAlone)
        {
            std::vector<ControlId>  controls =
            {
                { ControlKind::Axis,      0 },
                { ControlKind::Axis,      1 },
                { ControlKind::Axis,      3 },
                { ControlKind::Axis,      4 },
                { ControlKind::Button,    0 },
                { ControlKind::Button,    9 },
                { ControlKind::DpadLeft,  0 },
                { ControlKind::DpadRight, 0 },
            };
            ControlMapping          mapping  = DefaultMapping::MakeJoyport (Stick(), ControllerFormFactor::Joystick, controls);
            ControlMapping          wheel    = DefaultMapping::MakeJoyport (Stick(), ControllerFormFactor::Wheel,    controls);

            Assert::AreEqual (size_t (2), mapping.pdl0.size(), L"the primary stick and the D-pad");
            Assert::AreEqual (size_t (1), mapping.pdl1.size(), L"no D-pad pair on Y, so the stick alone");
            Assert::AreEqual (size_t (2), mapping.pb0.size(),  L"every button fires");
            Assert::IsTrue   (wheel == mapping,                L"and a wheel steers the same way");
        }


        //  Prefs saved before the Joyport profile existed gain it, and a
        //  profile the user already called Joyport becomes it rather than
        //  standing beside a second one.
        TEST_METHOD (AnOlderModel_GainsItsJoyportProfile)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::vector<ControlId>    controls = XInputSampleDecoder::ListControls();
            std::string               xbox     = ControllerTokens::ModelToToken (Xbox());
            std::string               stick    = ControllerTokens::ModelToToken (Stick());
            JsonValue                 doc      = Parse (
                "{\"models\":{"
                  "\"" + xbox  + "\":{\"profiles\":[{\"name\":\"Default\",\"default\":true,\"mapping\":{}}]},"
                  "\"" + stick + "\":{\"profiles\":[{\"name\":\"Default\",\"default\":true,\"mapping\":{}},"
                                                   "{\"name\":\"joyport\",\"mapping\":{}}]}"
                "}}");

            store.FromJson (doc, rejected);

            Assert::IsTrue   (rejected.empty());

            const ControllerModelSettings &  older   = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);
            const ControllerModelSettings &  adopted = store.GetOrCreateModel (Stick(), ControllerFormFactor::Gamepad, controls);

            Assert::AreEqual (size_t (3), older.profiles.size(), L"the Joyport and Paddles profiles are added");
            Assert::IsTrue   (older.profiles[1].kind == ControllerProfileKind::Joyport);
            Assert::IsTrue   (older.profiles[1].mapping == DefaultMapping::MakeJoyport (Xbox(), ControllerFormFactor::Gamepad, controls));
            Assert::IsTrue   (older.profiles[2].kind == ControllerProfileKind::Paddles);
            Assert::IsTrue   (older.profiles[2].mapping == DefaultMapping::MakePaddles (Xbox(), ControllerFormFactor::Gamepad, controls));

            Assert::AreEqual (size_t (3), adopted.profiles.size(), L"no second Joyport profile");
            Assert::IsTrue   (adopted.profiles[1].kind == ControllerProfileKind::Joyport);
            Assert::IsTrue   (adopted.profiles[1].mapping == ControlMapping(), L"the user's mapping is kept");
            Assert::IsTrue   (adopted.profiles[1].mode == ProfileMode::Joyport, L"and it becomes a Joyport-mode profile");
            Assert::IsTrue   (older.profiles[1].mode == ProfileMode::Joyport);
        }


        //  Each kind's chosen profiles are saved apart, so none overwrites
        //  another, and the Joystick choices keep the key they were saved
        //  under before profiles had kinds.
        TEST_METHOD (ActiveProfiles_RoundTripForEachMode)
        {
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            std::vector<std::string>  rejected;
            std::string               unit = ControllerTokens::UnitToToken ({ Xbox(), "045e:02e0", ControllerUnitSource::XInputProduct });
            JsonValue                 written;
            const JsonValue         * joystick = nullptr;
            const JsonValue         * paddle   = nullptr;

            store.GetActiveProfiles (ProfileMode::Joystick)[unit] = "Flight";
            store.GetActiveProfiles (ProfileMode::Paddle)[unit]   = "Pong";
            store.GetActiveProfiles (ProfileMode::Joyport)[unit]  = "Default";

            written = store.ToJson (JsonValue());
            readBack.FromJson (written, rejected);

            Assert::IsTrue   (rejected.empty());
            Assert::AreEqual (std::string ("Flight"),  readBack.activeProfiles.at (unit));
            Assert::AreEqual (std::string ("Pong"),    readBack.paddleActiveProfiles.at (unit));
            Assert::AreEqual (std::string ("Default"), readBack.joyportActiveProfiles.at (unit));
            Assert::IsTrue   (written.HasObject ("activeProfiles", joystick) && joystick != nullptr,     L"the Joystick choices under their old key");
            Assert::IsTrue   (written.HasObject ("paddleActiveProfiles", paddle) && paddle != nullptr,   L"the Paddle choices under their own");
        }


        TEST_METHOD (DeleteProfile_RemovesOnlyThatProfile)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls());

            settings.AddProfile ("Flight", MakeFullMapping(), ProfileMode::Joystick);
            settings.AddProfile ("Pong", ControlMapping(), ProfileMode::Joystick);

            Assert::IsTrue   (settings.DeleteProfile ("FLIGHT") == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.FindProfile ("Flight") == nullptr);
            Assert::IsTrue   (settings.FindProfile ("Pong") != nullptr);
            Assert::AreEqual (size_t (4), settings.profiles.size());
        }


        TEST_METHOD (FindProfile_ByModelTokenAndName)
        {
            ControllerProfileStore  store;
            std::string             token = ControllerTokens::ModelToToken (Xbox());

            store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, XInputSampleDecoder::ListControls()).AddProfile ("Lode Runner", MakeFullMapping(), ProfileMode::Joystick);

            Assert::IsTrue (store.FindProfile (token, "lode runner") != nullptr, L"ignoring case");
            Assert::IsTrue (store.FindProfile (token, "Lode Runner")->mapping == MakeFullMapping());
            Assert::IsTrue (store.FindProfile (token, "Flight") == nullptr, L"a missing name is not found, so the caller plays Default");
            Assert::IsTrue (store.FindProfile (ControllerTokens::ModelToToken (Stick()), "Lode Runner") == nullptr, L"profiles belong to their model");
        }


        TEST_METHOD (NamedProfiles_RoundTripThroughTheGlobalPrefsFile)
        {
            InMemoryFileSystem        fs;
            GlobalUserPrefs           saved;
            GlobalUserPrefs           loaded;
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            std::vector<ControlId>    controls = XInputSampleDecoder::ListControls();
            std::vector<std::string>  rejected;
            std::string               token    = ControllerTokens::ModelToToken (Xbox());

            store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls).AddProfile ("Flight", MakeFullMapping(), ProfileMode::Joystick);
            store.CreateProfile (Xbox(), ControllerFormFactor::Gamepad, controls, "Pong", ProfileSource::PaddleMapping, ProfileMode::Paddle);

            saved.controllers = store.ToJson (saved.controllers);
            AssertSucceeded (saved.Save  (L"C:\\Casso", fs));
            AssertSucceeded (loaded.Load (L"C:\\Casso", fs));

            readBack.FromJson (loaded.controllers, rejected);

            Assert::IsTrue (rejected.empty());
            Assert::IsTrue (readBack.models.at (token) == store.models.at (token), L"every profile, its name, order, Default flag and mapping come back");
        }


        TEST_METHOD (AMissingDefaultProfile_IsRecreatedFromTheDefaultMapping)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::vector<ControlId>    controls = XInputSampleDecoder::ListControls();
            std::string               token    = ControllerTokens::ModelToToken (Xbox());
            JsonValue                 doc      = Parse (
                "{\"models\":{\"" + token + "\":{\"profiles\":["
                  "{\"name\":\"Default\",\"default\":true,\"mapping\":{\"pb0\":[{\"control\":\"lever:9\"}]}},"
                  "{\"name\":\"Flight\",\"mapping\":{}}"
                "]}}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (1), rejected.size(), L"the unreadable Default is reported");

            const ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls);
            const ControllerProfile       *  profile  = settings.FindDefaultProfile();

            Assert::IsNotNull (profile, L"and recreated");
            Assert::AreEqual (std::string ("Default"), profile->name);
            Assert::IsTrue   (profile->mapping == DefaultMapping::For (Xbox(), controls));
            Assert::IsTrue   (settings.FindProfile ("Flight") != nullptr, L"the readable profile is kept");
        }


        TEST_METHOD (AModelThatFailsValidation_IsRebuiltWithOnlyItsBuiltInProfilesAndReported)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            std::vector<ControlId>    controls = XInputSampleDecoder::ListControls();
            std::string               xbox     = ControllerTokens::ModelToToken (Xbox());
            std::string               stick    = ControllerTokens::ModelToToken (Stick());
            std::string               other    = ControllerTokens::ModelToToken ({ ControllerKind::DirectInput, 1, 2 });
            JsonValue                 doc      = Parse (
                "{\"models\":{"
                  "\"" + xbox  + "\":5,"
                  "\"" + stick + "\":{\"deadzone\":0.3,\"profiles\":\"nonsense\"},"
                  "\"" + other + "\":{\"profiles\":[{\"name\":\"Default\",\"default\":true,\"mapping\":{}}]}"
                "}}");

            store.FromJson (doc, rejected);

            Assert::AreEqual (size_t (2), rejected.size(), L"each unreadable model is reported");
            Assert::IsTrue   (store.models.at (other).FindDefaultProfile() != nullptr, L"other models still load");

            const ControllerModelSettings &  rebuilt = store.GetOrCreateModel (Stick(), ControllerFormFactor::Gamepad, controls);

            Assert::AreEqual (size_t (3), rebuilt.profiles.size(), L"the rebuilt model holds only its built-in profiles");
            Assert::IsTrue   (rebuilt.profiles[0].kind == ControllerProfileKind::Default);
            Assert::IsTrue   (rebuilt.profiles[1].kind == ControllerProfileKind::Joyport);
            Assert::IsTrue   (rebuilt.profiles[2].kind == ControllerProfileKind::Paddles);
            Assert::AreEqual (DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput), rebuilt.deadzone, 0.0001f);
            Assert::AreEqual (size_t (3), store.GetOrCreateModel (Xbox(), ControllerFormFactor::Gamepad, controls).profiles.size());
        }


        //
        //  Players and last holders
        //

        static ControllerUnitKey StickUnit (const char * unitId)
        {
            ControllerUnitKey  unit;
            HRESULT            hr   = ControllerTokens::UnitFromToken (StickToken (unitId), unit);

            AssertSucceeded (hr);
            return unit;
        }


        // The section's players read on their own, with what was reported.
        static ControllerProfileStore ReadSection (const std::string & text, std::vector<std::string> & rejected)
        {
            ControllerProfileStore  store;

            store.FromJson (Parse (text), rejected);
            return store;
        }


        TEST_METHOD (Players_RoundTripThroughTheGlobalPrefsFile)
        {
            InMemoryFileSystem        fs;
            GlobalUserPrefs           saved;
            GlobalUserPrefs           loaded;
            ControllerProfileStore    store;
            ControllerProfileStore    readBack;
            std::vector<std::string>  rejected;
            PlayerEntries             entries;

            entries[0].kind = PlayerEntryKind::Controller;
            entries[0].unit = StickUnit ("{A}");
            entries[0].mode = PlayerMode::Paddle;

            store.players        = entries;
            store.lastHolders[0] = StickUnit ("{A}");

            saved.controllers = store.ToJson (saved.controllers);
            AssertSucceeded (saved.Save  (L"C:\\Casso", fs));
            AssertSucceeded (loaded.Load (L"C:\\Casso", fs));

            readBack.FromJson (loaded.controllers, rejected);

            Assert::IsTrue  (rejected.empty());
            Assert::IsTrue  (readBack.players.has_value(), L"saved players mark the adoption done");
            Assert::IsTrue  (readBack.players.value() == entries, L"a pick, its mode, and an Automatic player's mode come back");
            Assert::IsTrue  (readBack.lastHolders[0] == StickUnit ("{A}"), L"the last holder comes back");
            Assert::IsFalse (readBack.lastHolders[1].has_value(), L"and a slot nobody held stays empty");

            entries         = PlayerEntries();
            entries[0].kind = PlayerEntryKind::ArrowKeys;
            entries[1].kind = PlayerEntryKind::Disabled;

            store.players     = entries;
            saved.controllers = store.ToJson (saved.controllers);
            AssertSucceeded (saved.Save  (L"C:\\Casso", fs));
            AssertSucceeded (loaded.Load (L"C:\\Casso", fs));

            readBack.FromJson (loaded.controllers, rejected);

            Assert::IsTrue (rejected.empty());
            Assert::IsTrue (readBack.players.value() == entries, L"the keys for Player 1 and Disabled for Player 2 come back");

            entries[0].kind = PlayerEntryKind::MousePaddle;
            entries[0].mode = PlayerMode::Paddle;
            store.players   = entries;
            readBack.FromJson (store.ToJson (JsonValue()), rejected);

            Assert::IsTrue (readBack.players.value() == entries, L"and so does the mouse");
        }


        //  Each player's mode is saved under `mode`. A player saved before
        //  players had modes carries the paddles its slot mapped to, under
        //  `maps`, and a single paddle there reads as Paddle mode; a player
        //  with neither, or with an unrecognized mode, is in Joystick mode. The keys play in Joystick mode and the mouse in Paddle mode,
        //  whatever the file holds.
        TEST_METHOD (Players_TheModeIsSavedAndALegacyTargetReadsAsIt)
        {
            std::vector<std::string>  rejected;
            ControllerProfileStore    store;
            JsonValue                 written;
            const JsonValue         * players = nullptr;
            std::string               mode;

            store.players                  = PlayerEntries();
            store.players.value()[1].mode  = PlayerMode::Paddle;
            written                        = store.ToJson (JsonValue());

            Assert::IsTrue   (written.HasArray ("players", players) && players != nullptr);

            if (players == nullptr)
            {
                return;
            }

            Assert::IsTrue   (players->GetArrayElement (0).HasString ("mode", mode), L"Joystick mode is written");
            Assert::AreEqual (std::string ("joystick"), mode);
            Assert::IsTrue   (players->GetArrayElement (1).HasString ("mode", mode), L"and Paddle mode");
            Assert::AreEqual (std::string ("paddle"), mode);

            store = ReadSection ("{\"players\":[{\"entry\":\"automatic\",\"maps\":\"joystick0\"},"
                                              "{\"entry\":\"automatic\",\"maps\":\"paddle2\"}]}", rejected);

            Assert::IsTrue (store.players.value()[0].mode == PlayerMode::Joystick, L"a joystick target reads as Joystick mode");
            Assert::IsTrue (store.players.value()[1].mode == PlayerMode::Paddle,   L"a single paddle as Paddle mode");

            store = ReadSection ("{\"players\":[{\"entry\":\"automatic\",\"mode\":\"sideways\"},"
                                              "{\"entry\":\"automatic\",\"mode\":\"paddle\",\"maps\":\"joystick1\"}]}", rejected);

            Assert::IsTrue (store.players.value()[0].mode == PlayerMode::Joystick, L"an unknown mode reads as Joystick");
            Assert::IsTrue (store.players.value()[1].mode == PlayerMode::Paddle,   L"and a mode outranks an old target");

            store = ReadSection ("{\"players\":[{\"entry\":\"keys\",\"mode\":\"paddle\"},{\"entry\":\"automatic\"}]}", rejected);

            Assert::IsTrue (store.players.value()[0].mode == PlayerMode::Joystick, L"the keys play in Joystick mode");

            store = ReadSection ("{\"players\":[{\"entry\":\"mouse\",\"mode\":\"joystick\"},{\"entry\":\"automatic\"}]}", rejected);

            Assert::IsTrue (store.players.value()[0].mode == PlayerMode::Paddle, L"and the mouse in Paddle mode");
        }


        //  The Joyport's jacks, Two paddles and Same as Player 1 are saved as
        //  modes too, and read back as they were. A Player 2 saved with no mode is Same as
        //  Player 1, its default; Player 1 on Same as Player 1 is Joystick; and
        //  two players saved in one jack leave it to Player 1, with Player 2
        //  following into the other.
        TEST_METHOD (Players_TheJoyportModesRoundTrip)
        {
            const std::pair<PlayerMode, const char *>  kTokens[] =
            {
                { PlayerMode::Joystick,      "joystick"      },
                { PlayerMode::JoyportLeft,   "joyportLeft"   },
                { PlayerMode::JoyportRight,  "joyportRight"  },
                { PlayerMode::Paddle,        "paddle"        },
                { PlayerMode::SameAsPlayer1, "sameAsPlayer1" },
                { PlayerMode::TwoPaddles,    "twoPaddles"    },
            };
            std::vector<std::string>                   rejected;
            ControllerProfileStore                     store;
            ControllerProfileStore                     read;
            JsonValue                                  written;
            const JsonValue                          * players  = nullptr;
            std::string                                mode;
            size_t                                     swept    = 0;



            for (const auto & token : kTokens)
            {
                store.players                 = PlayerEntries();
                store.players.value()[1].mode = token.first;
                written                       = store.ToJson (JsonValue());

                Assert::IsTrue   (written.HasArray ("players", players) && players != nullptr);
                Assert::IsTrue   (players->GetArrayElement (1).HasString ("mode", mode));
                Assert::AreEqual (std::string (token.second), mode);

                read = ReadSection (JsonWriter::Write (written), rejected);
                Assert::IsTrue (read.players.value()[1].mode == token.first, L"and reads back as it was");
                swept++;
            }

            Assert::AreEqual (static_cast<size_t> (PlayerMode::TwoPaddles) + 1, swept, L"every mode has its token");

            read = ReadSection ("{\"players\":[{\"entry\":\"automatic\"},{\"entry\":\"automatic\"}]}", rejected);
            Assert::IsTrue (read.players.value()[0].mode == PlayerMode::Joystick,      L"Player 1 with no mode is Joystick");
            Assert::IsTrue (read.players.value()[1].mode == PlayerMode::SameAsPlayer1, L"and Player 2 Same as Player 1");

            read = ReadSection ("{\"players\":[{\"entry\":\"automatic\",\"mode\":\"sameAsPlayer1\"},{\"entry\":\"automatic\"}]}", rejected);
            Assert::IsTrue (read.players.value()[0].mode == PlayerMode::Joystick, L"Player 1 has no one to follow");

            read = ReadSection ("{\"players\":[{\"entry\":\"automatic\",\"mode\":\"joyportLeft\"},{\"entry\":\"automatic\",\"mode\":\"joyportLeft\"}]}", rejected);
            Assert::IsTrue (read.players.value()[0].mode == PlayerMode::JoyportLeft,   L"one jack, two players: Player 1 keeps it");
            Assert::IsTrue (read.players.value()[1].mode == PlayerMode::SameAsPlayer1, L"and Player 2 follows into the other");

            read = ReadSection ("{\"players\":[{\"entry\":\"keys\",\"mode\":\"joyportRight\"},{\"entry\":\"automatic\"}]}", rejected);
            Assert::IsTrue (read.players.value()[0].mode == PlayerMode::JoyportRight, L"the keys play in a jack");
        }


        //  A player saved with a mode is recorded, which is what marks the old
        //  Joyport setting as read. Players saved before modes, or no players
        //  at all, leave it to be read.
        TEST_METHOD (Players_ASavedModeIsRecorded)
        {
            std::vector<std::string>  rejected;
            ControllerProfileStore    store;



            store = ReadSection ("{\"players\":[{\"entry\":\"automatic\"},{\"entry\":\"automatic\",\"mode\":\"paddle\"}]}", rejected);
            Assert::IsTrue (store.hasPlayerModes, L"a mode on either player");

            store = ReadSection ("{\"players\":[{\"entry\":\"automatic\",\"maps\":\"joystick0\"},{\"entry\":\"automatic\"}]}", rejected);
            Assert::IsFalse (store.hasPlayerModes, L"players saved before modes");

            store = ReadSection ("{\"models\":{}}", rejected);
            Assert::IsFalse (store.hasPlayerModes, L"no players");
        }

        TEST_METHOD (Players_AbsentLeavesTheAdoptionToRunAndWritesNothing)
        {
            ControllerProfileStore    store;
            std::vector<std::string>  rejected;
            JsonValue                 written;
            const JsonValue         * member  = nullptr;

            store = ReadSection ("{\"models\":{}}", rejected);

            Assert::IsFalse (store.players.has_value(), L"no players saved means the adoption has not run");
            Assert::IsTrue  (rejected.empty());

            written = store.ToJson (Parse ("{\"models\":{}}"));

            Assert::IsFalse (written.HasArray ("players", member),     L"nothing is written for players that were never set");
            Assert::IsFalse (written.HasArray ("lastHolders", member));
        }


        TEST_METHOD (Players_AnUnreadableEntryReadsAsAutomaticAndIsReported)
        {
            const char *  kCases[] =
            {
                "{\"players\":[{\"entry\":\"joystick\"},{\"entry\":\"automatic\"}]}",
                "{\"players\":[{\"entry\":\"automatic\"},{\"entry\":\"controller\",\"controller\":\"nonsense\"}]}",
                "{\"players\":[{\"entry\":\"controller\"},{\"entry\":\"automatic\"}]}",
                "{\"players\":[{\"maps\":\"paddle0\"},{\"entry\":\"automatic\"}]}",
                "{\"players\":[5,{\"entry\":\"automatic\"}]}",
                "{\"players\":[{\"entry\":\"automatic\"},{\"entry\":\"keys\"}]}",
                "{\"players\":[{\"entry\":\"automatic\"},{\"entry\":\"mouse\"}]}",
                "{\"players\":[{\"entry\":\"disabled\"},{\"entry\":\"automatic\"}]}",
            };

            for (const char * text : kCases)
            {
                std::vector<std::string>  rejected;
                ControllerProfileStore    store = ReadSection (text, rejected);
                std::wstring              label (text, text + strlen (text));

                Assert::IsTrue   (store.players.has_value(), label.c_str());
                Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) store.players.value()[0].kind, label.c_str());
                Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) store.players.value()[1].kind, label.c_str());
                Assert::AreEqual (size_t (1), rejected.size(), (L"reported once: " + label).c_str());
            }
        }


        TEST_METHOD (Players_AnUnreadableEntryKeepsTheModeItSet)
        {
            std::vector<std::string>  rejected;
            ControllerProfileStore    store = ReadSection ("{\"players\":[{\"entry\":\"joystick\",\"mode\":\"paddle\"},{\"entry\":\"automatic\"}]}", rejected);

            Assert::IsTrue (store.players.value()[0].mode == PlayerMode::Paddle, L"only the entry was unreadable");
        }


        TEST_METHOD (Players_NotAnArrayOfTwo_IsReportedAndReadAsFarAsItGoes)
        {
            std::vector<std::string>  rejected;
            ControllerProfileStore    store = ReadSection ("{\"players\":5}", rejected);

            Assert::IsTrue   (store.players.has_value(), L"present in any form, the adoption has run");
            Assert::IsTrue   (store.players.value() == PlayerEntries(), L"both players on Automatic");
            Assert::AreEqual (size_t (1), rejected.size());

            rejected.clear();
            store = ReadSection ("{\"players\":[{\"entry\":\"keys\"}]}", rejected);

            Assert::AreEqual ((int) PlayerEntryKind::ArrowKeys, (int) store.players.value()[0].kind, L"the one player there is read");
            Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) store.players.value()[1].kind, L"the missing one is Automatic");
            Assert::AreEqual (size_t (1), rejected.size(), L"and the length is reported");
        }


        TEST_METHOD (LastHolders_AnUnreadableEntryReadsAsNullAndIsReported)
        {
            std::vector<std::string>  rejected;
            std::string               stick = StickToken ("{A}");
            ControllerProfileStore    store = ReadSection ("{\"players\":[{\"entry\":\"automatic\"},{\"entry\":\"automatic\"}],"
                                                           "\"lastHolders\":[\"nonsense\",5]}", rejected);

            Assert::IsFalse  (store.lastHolders[0].has_value());
            Assert::IsFalse  (store.lastHolders[1].has_value());
            Assert::AreEqual (size_t (2), rejected.size(), L"each unreadable entry is reported");

            rejected.clear();
            store = ReadSection ("{\"lastHolders\":[null,\"" + stick + "\"]}", rejected);

            Assert::IsTrue  (rejected.empty(), L"null is a slot nobody held, not an error");
            Assert::IsFalse (store.lastHolders[0].has_value());
            Assert::IsTrue  (store.lastHolders[1] == StickUnit ("{A}"));
        }


        TEST_METHOD (Players_ARepeatedControllerIsNormalized)
        {
            std::vector<std::string>  rejected;
            std::string               stick = StickToken ("{A}");
            ControllerProfileStore    store = ReadSection (
                "{\"players\":[{\"entry\":\"controller\",\"controller\":\"" + stick + "\"},"
                              "{\"entry\":\"controller\",\"controller\":\"" + stick + "\"}]}", rejected);

            Assert::AreEqual ((int) PlayerEntryKind::Controller, (int) store.players.value()[0].kind);
            Assert::AreEqual ((int) PlayerEntryKind::Automatic,  (int) store.players.value()[1].kind, L"one controller cannot play for both");
        }


        TEST_METHOD (Players_UnknownMembersOfTheSectionStillSurvive)
        {
            ControllerProfileStore  store;
            JsonValue               written;
            int                     futureKey = 0;

            store.players = PlayerEntries();
            written       = store.ToJson (Parse ("{\"futureKey\":1}"));

            Assert::IsTrue (written.HasInt ("futureKey", futureKey), L"writing the players keeps what this build does not know");
        }
    };
}
