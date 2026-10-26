#include "Pch.h"

#include "Ui/Settings/SerialCardPage.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPageTests
//
//  The Serial card page against a staged slot entry.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SerialCardPageTests)
{
public:

    static constexpr const char *  kShipped = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "TestMachine",
        "internalDevices": [ { "type": "keyboard" } ],
        "slots": [ { "slot": 2, "device": "super-serial-card", "firmware": "SSC.rom", "enabled": true } ]
    })JSON";


    static JsonValue ParseOrFail (const char * text)
    {
        JsonValue        value;
        JsonParseError   error;
        HRESULT          hr = JsonParser::Parse (text, value, error);



        AssertSucceeded (hr);
        return value;
    }


    //  Restore defaults stages the switches as the card ships and nothing on
    //  the connector, and the drop-downs follow.
    TEST_METHOD (RestoreDefaultsStagesTheShippedSwitchesAndNoConnector)
    {
        SettingsPanelState  state;
        SerialCardPage      page;
        JsonValue           shipped  = ParseOrFail (kShipped);
        SscSwitches         defaults = SscSwitches::MakeDefault();
        int                 index    = -1;



        AssertSucceeded (state.LoadFromMachine ("X", shipped, shipped));
        AssertSucceeded (state.SetSerialCardSwitches ("on on on on off off on", "off off off off on on off"));
        AssertSucceeded (state.SetSerialCardAttachment ("serial-loopback", ""));

        page.SetState (&state);
        page.GetRestoreButton().Click();

        index = state.FindSerialCard();
        Assert::IsTrue (index >= 0);

        const HardwareEntry &  entry = state.GetHardware()[static_cast<size_t> (index)];

        Assert::AreEqual (defaults.GetBankText (SscBank::Sw1), entry.switchesSw1, L"SW1 as shipped");
        Assert::AreEqual (defaults.GetBankText (SscBank::Sw2), entry.switchesSw2, L"SW2 as shipped");
        Assert::IsTrue   (entry.ports.empty() || entry.ports.front().empty(), L"nothing on the connector");
        Assert::AreEqual (0, page.GetAttachDropdown().GetSelectedIndex(), L"the connector drop-down is at None");
        Assert::AreEqual (SscSettingsModel::GetChoiceIndex (SscSetting::Mode, defaults), page.GetRowDropdown (0).GetSelectedIndex(),
                          L"the mode drop-down follows");
    }
};
