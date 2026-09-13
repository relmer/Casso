#include "Pch.h"

#include "../UiTests/InMemoryFileSystem.h"

#include "Config/GlobalUserPrefs.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"
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
    };
}
