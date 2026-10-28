#include "Pch.h"

#include "Ui/Settings/SerialPortsPage.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Core/JsonParser.h"
#include "../SerialTests/FakeHostSerialPorts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SerialPortsPageTestNs
{
    static constexpr const char *  kApple2cJson = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "Apple2c",
        "internalDevices": [ { "type": "keyboard" } ],
        "ports": [
            { "name": "disk",     "device": "" },
            { "name": "serial1",  "device": "" },
            { "name": "serial2",  "device": "serial-tcp", "host": "peer.example.org" },
            { "name": "joystick", "device": "" }
        ]
    })JSON";




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialPortsPageTests
    //
    //  Settings > Serial ports on a //c: each built-in port's attachment,
    //  staged as the user picks it.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialPortsPageTests)
    {
    public:
        static void Load (SettingsPanelState & state)
        {
            JsonValue       json;
            JsonParseError  error;
            HRESULT         hr    = S_OK;



            hr = JsonParser::Parse (kApple2cJson, json, error);
            Assert::IsTrue (SUCCEEDED (hr));

            hr = state.LoadFromMachine ("Apple2c", json, json);
            Assert::IsTrue (SUCCEEDED (hr));
        }


        TEST_METHOD (EachPortShowsItsStagedAttachment)
        {
            SettingsPanelState  state;
            SerialPortsPage     page;



            Load (state);
            page.SetState (&state);

            Assert::AreEqual (0, page.GetPort (SerialPortId::Port1).GetAttachDropdown().GetSelectedIndex(), L"port 1 has the loopback plug");
            Assert::AreEqual (std::string (kpszSerialTcp), page.GetPort (SerialPortId::Port2).GetChoice().device);
            Assert::AreEqual (std::string ("peer.example.org"), page.GetPort (SerialPortId::Port2).GetChoice().options.host);
        }


        TEST_METHOD (APickIsStagedForItsPort)
        {
            constexpr int        kModemChoice = 5;     // on a port: Loopback, Output file, Input file, COM port, TCP, Modem
            SettingsPanelState   state;
            SerialPortsPage      page;
            DxuiComboBox       * combo        = nullptr;



            Load (state);
            page.SetState (&state);

            combo = &page.GetPort (SerialPortId::Port1).GetAttachDropdown();
            combo->Open();

            while (combo->GetHighlightIndex() < kModemChoice)
            {
                combo->HandleKey (VK_DOWN);
            }

            combo->HandleKey (VK_RETURN);

            Assert::AreEqual (std::string (kpszSerialModem), state.GetSerialAttachment (SerialPortId::Port1).device);
            Assert::AreEqual (std::string (kpszSerialTcp),   state.GetSerialAttachment (SerialPortId::Port2).device, L"the other port is left alone");
            Assert::IsTrue (state.RequiresReset());
        }


        TEST_METHOD (TheComPortListComesFromThePc)
        {
            constexpr int         kComPortChoice = 3;
            SettingsPanelState    state;
            SerialPortsPage       page;
            FakeHostSerialPorts   ports;
            DxuiComboBox        * combo          = nullptr;



            ports.AddPort ("COM4");
            Load (state);
            page.SetSerialPorts (&ports);
            page.SetState (&state);

            combo = &page.GetPort (SerialPortId::Port2).GetAttachDropdown();
            combo->Open();

            while (combo->GetHighlightIndex() > kComPortChoice)
            {
                combo->HandleKey (VK_UP);
            }

            combo->HandleKey (VK_RETURN);

            Assert::AreEqual (std::string ("COM4"), state.GetSerialAttachment (SerialPortId::Port2).options.comPort, L"the PC's only port is chosen");
        }
    };
}
