#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HardwareTreeTests
//
//  The pure-data shaping of the hardware tree. We exercise
//  `SettingsPanelState::ExtractHardware` against fixture JSONs and
//  assert that the produced rows carry the right (kind, slot,
//  capability, lockReason, enabled) for the SettingsPanel renderer
//  to consume. The chrome surface is NOT loaded.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    JsonValue ParseOrFail (const std::string & text)
    {
        JsonValue        v;
        JsonParseError   e;
        HRESULT          hr = JsonParser::Parse (text, v, e);
        Assert::IsTrue (SUCCEEDED (hr));
        return v;
    }
}





TEST_CLASS (HardwareTreeTests)
{
public:

    TEST_METHOD (Extract_DefaultCapabilities_PerFR015)
    {
        // FR-015 default: internal devices -> Required, slots -> Optional.
        const char * j = R"JSON({
            "internalDevices": [ { "type": "keyboard" } ],
            "slots":           [ { "slot": 6, "device": "disk-ii" } ]
        })JSON";

        std::vector<HardwareEntry>  out;
        Assert::IsTrue (SUCCEEDED (
            SettingsPanelState::ExtractHardware (ParseOrFail (j), out)));

        Assert::AreEqual<size_t> (2u, out.size());

        Assert::IsTrue (out[0].kind       == HardwareEntryKind::InternalDevice);
        Assert::IsTrue (out[0].capability == CapabilityFlag::Required);
        Assert::AreEqual (std::string ("keyboard"), out[0].type);

        Assert::IsTrue (out[1].kind       == HardwareEntryKind::Slot);
        Assert::IsTrue (out[1].capability == CapabilityFlag::Optional);
        Assert::AreEqual (6, out[1].slot);
    }


    TEST_METHOD (Extract_RequiredFlag_NotInteractive)
    {
        const char * j = R"JSON({
            "slots": [
                { "slot": 6, "device": "disk-ii", "capabilityFlag": "required" }
            ]
        })JSON";

        std::vector<HardwareEntry>  out;
        SettingsPanelState::ExtractHardware (ParseOrFail (j), out);

        Assert::IsTrue (out[0].capability == CapabilityFlag::Required,
                        L"required slot overrides default optional");
    }


    TEST_METHOD (Extract_PlatformLocked_CarriesLockReason)
    {
        const char * j = R"JSON({
            "internalDevices": [
                { "type": "80col-card",
                  "capabilityFlag": "platform-locked",
                  "lockReason": "integrated on Apple //c" }
            ]
        })JSON";

        std::vector<HardwareEntry>  out;
        SettingsPanelState::ExtractHardware (ParseOrFail (j), out);

        Assert::IsTrue (out[0].capability == CapabilityFlag::PlatformLocked);
        Assert::AreEqual (std::string ("integrated on Apple //c"), out[0].lockReason);
    }


    TEST_METHOD (Extract_EnabledFlag_RoundTrips)
    {
        const char * j = R"JSON({
            "slots": [
                { "slot": 4, "device": "mockingboard", "enabled": false }
            ]
        })JSON";

        std::vector<HardwareEntry>  out;
        SettingsPanelState::ExtractHardware (ParseOrFail (j), out);

        Assert::IsFalse (out[0].enabled, L"explicit enabled:false respected");
    }


    TEST_METHOD (Extract_UnknownCapability_FallsBackToKindDefault)
    {
        const char * j = R"JSON({
            "internalDevices": [
                { "type": "keyboard", "capabilityFlag": "bogus" }
            ],
            "slots": [
                { "slot": 6, "device": "disk-ii", "capabilityFlag": "bogus" }
            ]
        })JSON";

        std::vector<HardwareEntry> out;
        SettingsPanelState::ExtractHardware (ParseOrFail (j), out);

        Assert::AreEqual<size_t> (2u, out.size());
        Assert::IsTrue (out[0].capability == CapabilityFlag::Required);
        Assert::IsTrue (out[1].capability == CapabilityFlag::Optional);
    }


    TEST_METHOD (Extract_PreservesInternalThenSlotOrdering)
    {
        // Verifies the renderer's traversal order -- internal devices
        // first (nested at the top of the tree), then slots. This is
        // what the C++ side relies on to label rows.
        const char * j = R"JSON({
            "internalDevices": [
                { "type": "kbd" },
                { "type": "spk" }
            ],
            "slots": [
                { "slot": 2, "device": "ssc" },
                { "slot": 6, "device": "disk-ii" }
            ]
        })JSON";

        std::vector<HardwareEntry>  out;
        SettingsPanelState::ExtractHardware (ParseOrFail (j), out);

        Assert::AreEqual<size_t> (4u, out.size());
        Assert::IsTrue   (out[0].kind == HardwareEntryKind::InternalDevice);
        Assert::IsTrue   (out[1].kind == HardwareEntryKind::InternalDevice);
        Assert::IsTrue   (out[2].kind == HardwareEntryKind::Slot);
        Assert::IsTrue   (out[3].kind == HardwareEntryKind::Slot);
        Assert::AreEqual (2, out[2].slot);
        Assert::AreEqual (6, out[3].slot);

        // jsonIndex preserves the source-array position so BuildJson
        // can write back to the right entry.
        Assert::AreEqual (0, out[0].jsonIndex);
        Assert::AreEqual (1, out[1].jsonIndex);
        Assert::AreEqual (0, out[2].jsonIndex);
        Assert::AreEqual (1, out[3].jsonIndex);
    }


    TEST_METHOD (Extract_SlotDisplayName_FormattedWithSlotNumberAndDevice)
    {
        const char * j = R"JSON({
            "slots": [ { "slot": 6, "device": "disk-ii" } ]
        })JSON";

        std::vector<HardwareEntry>  out;
        SettingsPanelState::ExtractHardware (ParseOrFail (j), out);

        Assert::AreEqual (std::string ("Slot 6: Disk ]["), out[0].displayName);
    }
};
