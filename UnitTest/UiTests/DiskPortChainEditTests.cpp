#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainEditTests
//
//  The //c Plus's disk port chain as the Disk menu edits it: a drive plugged
//  in goes where the firmware looks for its kind, and a drive unplugged takes
//  its disk with it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskPortChainEditTests)
{
public:

    TEST_METHOD (AddsEachKindWhereTheFirmwareLooksForIt)
    {
        SettingsPanelState        st;
        SettingsPanelState        reloaded;
        JsonValue                 v        = Parse (kFixtureJson);
        JsonValue                 saved;
        std::vector<std::string>  expected = { "apple35-drive", "apple35-drive", "unidisk35", "disk-iic", "disk-iic" };



        st.LoadFromMachine ("TestCcPlus", v, v);
        Assert::IsTrue (st.GetDiskPortChain().empty(), L"the port starts with nothing on it");

        st.AddDiskPortDrive ("disk-iic");
        st.AddDiskPortDrive ("apple35-drive");
        st.AddDiskPortDrive ("unidisk35");
        st.AddDiskPortDrive ("disk-iic");
        st.AddDiskPortDrive ("apple35-drive");

        saved = st.BuildCurrentJson();
        reloaded.LoadFromMachine ("TestCcPlus", v, saved);

        Assert::IsTrue (reloaded.GetDiskPortChain() == expected, L"Apple 3.5 Drives first, 5.25-inch drives last, saved");
    }


    TEST_METHOD (RemovesTheNthDriveOfItsKind)
    {
        SettingsPanelState  st;
        JsonValue           v    = Parse (R"JSON({
            "$cassoMachineVersion": 1,
            "ports": [
                { "name": "disk", "device": "", "chain": [
                    { "device": "apple35-drive", "image": "a.po" },
                    { "device": "apple35-drive", "image": "b.po" },
                    { "device": "disk-iic" }
                ] }
            ]
        })JSON");
        std::string         text;



        st.LoadFromMachine ("TestCcPlus", v, v);

        st.RemoveDiskPortDrive ("apple35-drive", 0);

        Assert::IsTrue (st.GetDiskPortChain() == std::vector<std::string> { "apple35-drive", "disk-iic" });

        text = Write (st.BuildCurrentJson());
        Assert::IsTrue  (text.find ("b.po") != std::string::npos, L"the other drive keeps its disk");
        Assert::IsFalse (text.find ("a.po") != std::string::npos, L"the removed drive's disk goes with it");

        st.RemoveDiskPortDrive ("apple35-drive", 0);
        st.RemoveDiskPortDrive ("disk-iic",      0);
        st.RemoveDiskPortDrive ("disk-iic",      0);

        text = Write (st.BuildCurrentJson());
        Assert::IsTrue (st.GetDiskPortChain().empty());
        Assert::IsTrue (text.find ("\"chain\"") == std::string::npos, L"an empty chain is left out");
    }


private:

    static constexpr const char *  kFixtureJson = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "TestCcPlus",
        "cpu": "65C02",
        "timing": { "clockSpeed": 1023000 },
        "ram": [ { "address": "0x0000", "size": "0xC000" } ],
        "systemRom": { "address": "0xC000", "file": "x.rom", "romBankSize": "0x4000" },
        "internalDevices": [ { "type": "keyboard" } ],
        "ports": [
            { "name": "internal", "device": "apple35-drive" },
            { "name": "disk",     "device": "" },
            { "name": "joystick", "device": "" }
        ]
    })JSON";


    static JsonValue Parse (const std::string & text)
    {
        JsonValue       v;
        JsonParseError  e;
        HRESULT         hr = JsonParser::Parse (text, v, e);



        AssertSucceeded (hr, L"test fixture JSON must parse");

        return v;
    }


    static std::string Write (const JsonValue & value)
    {
        std::string          text;
        JsonWriter::Options  opts;



        opts.fPretty = false;
        AssertSucceeded (JsonWriter::Write (value, opts, text), L"emitted JSON must serialize");

        return text;
    }
};
