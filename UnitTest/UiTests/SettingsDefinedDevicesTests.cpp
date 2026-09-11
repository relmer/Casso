#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsDefinedDevicesTests
//
//  The Hardware tab reads a machine's internal devices, and a shipped
//  machine's document no longer lists them: what devices a machine has is
//  stated in code, so the file describes only what an owner configures.
//
//  Without the injection these test, the tab would come up empty for every
//  shipped machine -- a regression nothing else would catch, because the
//  document parses perfectly well and the emulator runs fine.
//
//  A machine with no definition is the other half: its document is the only
//  description of it, so nothing may overwrite it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SettingsDefinedDevicesTests)
{
public:

    TEST_METHOD (AShippedMachineGetsItsDevicesFromItsDefinition)
    {
        SettingsPanelState  state;
        JsonValue           doc = ParseOrFail (DocumentWithoutDevices());
        HRESULT             hr  = S_OK;

        hr = state.LoadFromMachine ("Apple2e", doc, doc);
        AssertSucceeded (hr, L"a document without a device list must still load");

        Assert::IsTrue (state.GetHardware().size() > 0,
                        L"the Hardware tab must not come up empty for a shipped machine");
    }


    TEST_METHOD (TheDevicesAreTheOnesTheMachineActuallyHas)
    {
        SettingsPanelState  state;
        JsonValue           doc   = ParseOrFail (DocumentWithoutDevices());
        bool                found = false;
        HRESULT             hr    = S_OK;

        hr = state.LoadFromMachine ("Apple2e", doc, doc);
        AssertSucceeded (hr);

        for (const HardwareEntry & entry : state.GetHardware())
        {
            if (entry.type == "apple2e-family-mmu")
            {
                found = true;
                break;
            }
        }

        Assert::IsTrue (found, L"a //e has an MMU, and the tab must say so");
    }


    TEST_METHOD (AMachineWithNoDefinitionKeepsItsOwnDeviceList)
    {
        SettingsPanelState  state;
        JsonValue           doc   = ParseOrFail (DocumentWithOwnDevices());
        bool                found = false;
        HRESULT             hr    = S_OK;

        //  Someone's own machine, copied from a shipped one and renamed. Its
        //  document is the only description of it there is.
        hr = state.LoadFromMachine ("MyOwnMachine", doc, doc);
        AssertSucceeded (hr);

        for (const HardwareEntry & entry : state.GetHardware())
        {
            if (entry.type == "apple2-family-speaker")
            {
                found = true;
                break;
            }
        }

        Assert::IsTrue (found, L"an undefined machine's own devices must survive");
    }


private:

    static JsonValue ParseOrFail (const std::string & text)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT         hr = JsonParser::Parse (text, value, error);

        AssertSucceeded (hr, L"test fixture JSON must parse");

        return (value);
    }


    //
    //  A shipped machine's document as it now ships: slots and ROMs, and not a
    //  word about which devices the machine has.
    //
    static std::string DocumentWithoutDevices()
    {
        return R"({
            "$cassoMachineVersion": 12,
            "name": "Apple //e",
            "timing": { "videoStandard": "ntsc", "clockSpeed": 1023000, "cyclesPerScanline": 65 },
            "systemRom": { "address": "0xC000", "file": "Apple2e.rom" },
            "slots": [
                { "slot": 6, "device": "disk-ii" }
            ]
        })";
    }


    static std::string DocumentWithOwnDevices()
    {
        return R"({
            "$cassoMachineVersion": 1,
            "name": "My Own Machine",
            "cpu": "6502",
            "timing": { "videoStandard": "ntsc", "clockSpeed": 1023000, "cyclesPerScanline": 65 },
            "systemRom": { "address": "0xD000", "file": "Apple2.rom" },
            "internalDevices": [
                { "type": "apple2-family-speaker" }
            ]
        })";
    }
};
