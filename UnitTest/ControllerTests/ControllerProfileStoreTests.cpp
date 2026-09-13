#include "Pch.h"

#include "../UiTests/InMemoryFileSystem.h"

#include "Config/GlobalUserPrefs.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerTokens.h"
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
    };
}
