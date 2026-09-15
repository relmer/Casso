#include "Pch.h"

#include "../UiTests/InMemoryFileSystem.h"

#include "Config/GlobalUserPrefs.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"
#include "Controllers/XInputSampleDecoder.h"
#include "Core/JsonParser.h"

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


        TEST_METHOD (DefaultProfileAndDeadzone_RoundTripThroughTheGlobalPrefsFile)
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
            settings.profiles.push_back ({ "Default", true, MakeFullMapping() });
            store.models[token] = settings;

            saved.controllers = store.ToJson (saved.controllers);
            AssertSucceeded (saved.Save  (L"C:\\Casso", fs));
            AssertSucceeded (loaded.Load (L"C:\\Casso", fs));

            readBack.FromJson (loaded.controllers, rejected);

            Assert::IsTrue   (rejected.empty());
            Assert::AreEqual (0.3f, readBack.models.at (token).deadzone, 0.0001f, L"the model's deadzone comes back");
            Assert::IsTrue   (readBack.models.at (token).profiles[0].mapping == MakeFullMapping(),
                L"every binding comes back: rate response and speed, inversion, a D-pad pair, thresholds, a direction and PB2");
            Assert::IsTrue   (readBack.models.at (token).FindDefaultProfile() != nullptr);
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
            settings.profiles.push_back ({ "Lode Runner", false, ControlMapping() });
            store.models[ControllerTokens::ModelToToken (Stick())] = settings;

            store.GetDefaultSettings (Stick(), controls, mapping, deadzone);

            Assert::IsTrue   (mapping == DefaultMapping::For (Stick(), controls), L"with no Default profile, the default mapping is the Default");
            Assert::AreEqual (0.4f, deadzone, 0.0001f, L"but the model's own deadzone still applies");
        }


        TEST_METHOD (AModelNeverEdited_PlaysWithTheBuiltInDefaults)
        {
            ControllerProfileStore  store;
            ControlMapping          mapping;
            float                   deadzone = 0.0f;
            std::vector<ControlId>  controls = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 } };

            store.GetDefaultSettings (Stick(), controls, mapping, deadzone);

            Assert::IsTrue   (mapping == DefaultMapping::For (Stick(), controls));
            Assert::AreEqual (DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput), deadzone, 0.0001f);
        }


        static ControllerModelKey Xbox()
        {
            return { ControllerKind::XInput, 0, 0 };
        }


        TEST_METHOD (DefaultProfile_CanNeitherBeDeletedNorRenamed)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), XInputSampleDecoder::ListControls());

            Assert::IsTrue (settings.DeleteProfile ("Default") == ProfileEditResult::IsDefaultProfile);
            Assert::IsTrue (settings.RenameProfile ("default", "Main") == ProfileEditResult::IsDefaultProfile);
            Assert::AreEqual (size_t (1), settings.profiles.size());
            Assert::AreEqual (std::string ("Default"), settings.profiles[0].name);
            Assert::IsTrue (settings.DeleteProfile ("Missing") == ProfileEditResult::NotFound);
        }


        TEST_METHOD (ProfileNames_AreTrimmedAndOneToFortyCharacters)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), XInputSampleDecoder::ListControls());
            std::string                forty (ControllerModelSettings::kMaxProfileNameLength, 'a');
            std::string                fortyOne (ControllerModelSettings::kMaxProfileNameLength + 1, 'b');

            Assert::IsTrue   (settings.AddProfile ("", ControlMapping()) == ProfileEditResult::EmptyName);
            Assert::IsTrue   (settings.AddProfile (" \t ", ControlMapping()) == ProfileEditResult::EmptyName, L"whitespace alone is empty");
            Assert::IsTrue   (settings.AddProfile (fortyOne, ControlMapping()) == ProfileEditResult::NameTooLong);
            Assert::IsTrue   (settings.AddProfile ("  " + forty + "  ", ControlMapping()) == ProfileEditResult::Ok, L"length is measured after trimming");
            Assert::IsTrue   (settings.AddProfile ("  Flight  ", ControlMapping()) == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.FindProfile ("Flight") != nullptr, L"and the name is stored trimmed");
            Assert::AreEqual (std::string ("Flight"), settings.FindProfile ("flight")->name);
            Assert::IsTrue   (settings.RenameProfile ("Flight", "   ") == ProfileEditResult::EmptyName);
            Assert::IsTrue   (settings.RenameProfile ("Flight", fortyOne) == ProfileEditResult::NameTooLong);
        }


        TEST_METHOD (ProfileNames_AreUniqueIgnoringCase)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), XInputSampleDecoder::ListControls());

            Assert::IsTrue   (settings.AddProfile ("Flight", ControlMapping()) == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.AddProfile ("Lode Runner", ControlMapping()) == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.AddProfile ("FLIGHT", ControlMapping()) == ProfileEditResult::DuplicateName);
            Assert::IsTrue   (settings.AddProfile ("default", ControlMapping()) == ProfileEditResult::DuplicateName, L"never a second Default");
            Assert::IsTrue   (settings.RenameProfile ("Lode Runner", " flight ") == ProfileEditResult::DuplicateName);
            Assert::IsTrue   (settings.RenameProfile ("Flight", "FLIGHT") == ProfileEditResult::Ok, L"a profile may change the case of its own name");
            Assert::AreEqual (std::string ("FLIGHT"), settings.FindProfile ("flight")->name);
            Assert::AreEqual (size_t (3), settings.profiles.size());
        }


        TEST_METHOD (CreateProfile_FromTheDefaultACopyOrPaddles)
        {
            ControllerProfileStore         store;
            std::vector<ControlId>         controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings      & settings = store.GetOrCreateModel (Xbox(), controls);
            int                            defaults = 0;

            settings.AddProfile ("Flight", MakeFullMapping());

            Assert::IsTrue (store.CreateProfile (Xbox(), controls, "Plain", ProfileSource::DefaultMapping) == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), controls, "Flight 2", ProfileSource::CopyOfProfile, "flight") == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), controls, "Pong", ProfileSource::Paddles) == ProfileEditResult::Ok);
            Assert::IsTrue (store.CreateProfile (Xbox(), controls, "Ghost", ProfileSource::CopyOfProfile, "Missing") == ProfileEditResult::NotFound);
            Assert::IsTrue (store.CreateProfile (Xbox(), controls, "PONG", ProfileSource::Paddles) == ProfileEditResult::DuplicateName);

            Assert::IsTrue (settings.FindProfile ("Plain")->mapping == DefaultMapping::For (Xbox(), controls));
            Assert::IsTrue (settings.FindProfile ("Flight 2")->mapping == MakeFullMapping(), L"a copy carries the source's mapping");
            Assert::IsTrue (settings.FindProfile ("Pong")->mapping == DefaultMapping::MakePaddles (Xbox(), controls));
            Assert::IsTrue (settings.FindProfile ("Ghost") == nullptr);

            for (const ControllerProfile & profile : settings.profiles)
            {
                defaults += profile.isDefault ? 1 : 0;
            }

            Assert::AreEqual (1, defaults, L"creating profiles never adds a second Default");
        }


        TEST_METHOD (ResetProfile_RestoresTheDefaultMapping)
        {
            ControllerProfileStore         store;
            std::vector<ControlId>         controls = XInputSampleDecoder::ListControls();
            ControllerModelSettings      & settings = store.GetOrCreateModel (Xbox(), controls);

            settings.AddProfile ("Flight", MakeFullMapping());
            settings.profiles[0].mapping = MakeFullMapping();

            Assert::IsTrue (store.ResetProfile (Xbox(), controls, "Flight") == ProfileEditResult::Ok);
            Assert::IsTrue (store.ResetProfile (Xbox(), controls, "Default") == ProfileEditResult::Ok, L"the Default can be reset");
            Assert::IsTrue (store.ResetProfile (Xbox(), controls, "Missing") == ProfileEditResult::NotFound);
            Assert::IsTrue (settings.FindProfile ("Flight")->mapping == DefaultMapping::For (Xbox(), controls));
            Assert::IsTrue (settings.FindDefaultProfile()->mapping == DefaultMapping::For (Xbox(), controls));
        }


        TEST_METHOD (DeleteProfile_RemovesOnlyThatProfile)
        {
            ControllerProfileStore     store;
            ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), XInputSampleDecoder::ListControls());

            settings.AddProfile ("Flight", MakeFullMapping());
            settings.AddProfile ("Pong", ControlMapping());

            Assert::IsTrue   (settings.DeleteProfile ("FLIGHT") == ProfileEditResult::Ok);
            Assert::IsTrue   (settings.FindProfile ("Flight") == nullptr);
            Assert::IsTrue   (settings.FindProfile ("Pong") != nullptr);
            Assert::AreEqual (size_t (2), settings.profiles.size());
        }


        TEST_METHOD (FindProfile_ByModelTokenAndName)
        {
            ControllerProfileStore  store;
            std::string             token = ControllerTokens::ModelToToken (Xbox());

            store.GetOrCreateModel (Xbox(), XInputSampleDecoder::ListControls()).AddProfile ("Lode Runner", MakeFullMapping());

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

            store.GetOrCreateModel (Xbox(), controls).AddProfile ("Flight", MakeFullMapping());
            store.CreateProfile (Xbox(), controls, "Pong", ProfileSource::Paddles);

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

            const ControllerModelSettings &  settings = store.GetOrCreateModel (Xbox(), controls);
            const ControllerProfile       *  profile  = settings.FindDefaultProfile();

            Assert::IsTrue   (profile != nullptr, L"and recreated");
            Assert::AreEqual (std::string ("Default"), profile->name);
            Assert::IsTrue   (profile->mapping == DefaultMapping::For (Xbox(), controls));
            Assert::IsTrue   (settings.FindProfile ("Flight") != nullptr, L"the readable profile is kept");
        }


        TEST_METHOD (AModelThatFailsValidation_IsRebuiltWithOnlyItsDefaultAndReported)
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

            const ControllerModelSettings &  rebuilt = store.GetOrCreateModel (Stick(), controls);

            Assert::AreEqual (size_t (1), rebuilt.profiles.size(), L"the rebuilt model holds only its Default");
            Assert::IsTrue   (rebuilt.profiles[0].isDefault);
            Assert::AreEqual (DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput), rebuilt.deadzone, 0.0001f);
            Assert::AreEqual (size_t (1), store.GetOrCreateModel (Xbox(), controls).profiles.size());
        }
    };
}
