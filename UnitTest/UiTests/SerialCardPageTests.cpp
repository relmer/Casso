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


    //  A connector set outside the list, such as another TCP port, is listed
    //  in words, not by its device name.
    TEST_METHOD (AConnectorOutsideTheList_IsListedInWords)
    {
        std::vector<std::wstring>  labels;
        std::vector<std::string>   devices;



        SerialCardPage::BuildAttachments (2, {}, "tcp:6600", labels, devices);

        Assert::AreEqual (std::string ("tcp:6600"), devices.back());
        Assert::AreEqual (std::wstring (L"TCP port 6600 on this PC"), labels.back());

        SerialCardPage::BuildAttachments (2, {}, "imagewriter", labels, devices);
        Assert::AreEqual (std::wstring (L"ImageWriter II"), labels.back());
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


    //  After the fixed attachments, a TCP port for the card's slot and each
    //  of the PC's COM ports, as a //c's port offers.
    TEST_METHOD (TheConnectorOffersATcpPortForItsSlot_AndTheComPorts)
    {
        SettingsPanelState  state;
        SerialCardPage      page;
        JsonValue           shipped = ParseOrFail (kShipped);
        int                 index   = -1;



        AssertSucceeded (state.LoadFromMachine ("X", shipped, shipped));

        page.SetComPorts ({ L"COM3" });
        page.SetState (&state);
        page.Rebuild();

        Assert::AreEqual (std::string ("tcp:6502"), page.GetAttachDevices()[SerialCardPage::kTcpChoice]);
        Assert::AreEqual (std::string ("com:COM3"), page.GetAttachDevices()[SerialCardPage::kTcpChoice + 1]);

        page.GetAttachDropdown().SetSelected (SerialCardPage::kTcpChoice);
        AssertSucceeded (state.SetSerialCardAttachment (page.GetAttachDevices()[SerialCardPage::kTcpChoice], ""));
        page.Rebuild();

        index = state.FindSerialCard();
        Assert::AreEqual (std::string ("tcp:6502"), state.GetHardware()[static_cast<size_t> (index)].ports.front());
        Assert::AreEqual (SerialCardPage::kTcpChoice, page.GetAttachDropdown().GetSelectedIndex(), L"the staged TCP port is selected");
    }
};
