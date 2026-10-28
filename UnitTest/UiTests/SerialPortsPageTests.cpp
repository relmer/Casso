#include "Pch.h"

#include "Ui/Settings/SerialPortsPage.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Core/JsonParser.h"
#include "Core/TextEncoding.h"
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

            Assert::AreEqual (0, page.GetPort (SerialPortId::Port1).GetAttachDropdown().GetSelectedIndex(), L"port 1 has nothing on it");
            Assert::AreEqual (std::string (kpszSerialTcp), page.GetPort (SerialPortId::Port2).GetChoice().device);
            Assert::AreEqual (std::string ("peer.example.org"), page.GetPort (SerialPortId::Port2).GetChoice().options.host);
        }


        TEST_METHOD (APickIsStagedForItsPort)
        {
            constexpr int        kModemChoice = 6;     // None, Loopback, Output file, Input file, COM port, TCP, Modem
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


        //  Every attachment on either port lays out with its visible controls
        //  inside the page and none over another, port 2 below port 1.
        TEST_METHOD (EveryAttachmentLaysOutWithoutOverlap)
        {
            constexpr int        kWidth  = 720;
            constexpr int        kHeight = 600;
            SettingsPanelState   state;
            SerialPortsPage      page;
            DxuiDpiScaler        scaler;
            RECT                 rect    = { 0, 0, kWidth, kHeight };
            size_t               choice  = 0;



            Load (state);
            page.SetState (&state);

            for (choice = 0; choice < page.GetPort (SerialPortId::Port1).GetDevices().size(); choice++)
            {
                std::string                  device      = page.GetPort (SerialPortId::Port1).GetDevices()[choice];
                std::vector<IDxuiControl *>  shown;
                LONG                         port1Bottom = 0;

                state.SetSerialAttachment (SerialPortId::Port1, { device, "C:\\file.txt", {} });
                state.SetSerialAttachment (SerialPortId::Port2, { device, "C:\\file.txt", {} });
                page.Rebuild();
                page.Layout (rect, scaler);

                for (SerialPortId port : { SerialPortId::Port1, SerialPortId::Port2 })
                {
                    SerialAttachmentView &  view = page.GetPort (port);

                    for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { &view.GetAttachDropdown(), &view.GetComPortDropdown(), &view.GetConnectDropdown(),
                                                                                         &view.GetHostInput(), &view.GetPortInput(), &view.GetListenPortInput(),
                                                                                         &view.GetTelnetCheckbox(), &view.GetBrowseButton() })
                    {
                        if (control->IsVisible())
                        {
                            shown.push_back (control);
                            port1Bottom = (port == SerialPortId::Port1) ? std::max (port1Bottom, control->GetBounds().bottom) : port1Bottom;
                        }
                    }
                }

                for (size_t i = 0; i < shown.size(); i++)
                {
                    RECT  a = shown[i]->GetBounds();

                    Assert::IsTrue (a.left >= 0 && a.right <= kWidth && a.top >= 0 && a.bottom <= kHeight && a.right > a.left && a.bottom > a.top,
                                    TextEncoding::NarrowToWide ("inside the page for " + device).c_str());

                    for (size_t j = i + 1; j < shown.size(); j++)
                    {
                        RECT  b       = shown[j]->GetBounds();
                        bool  isApart = a.right <= b.left || b.right <= a.left || a.bottom <= b.top || b.bottom <= a.top;

                        Assert::IsTrue (isApart, TextEncoding::NarrowToWide ("no overlap for " + device).c_str());
                    }
                }

                Assert::IsTrue (page.GetPort (SerialPortId::Port2).GetAttachDropdown().GetBounds().top >= port1Bottom, L"port 2 below port 1");
            }
        }


        TEST_METHOD (TheComPortListComesFromThePc)
        {
            constexpr int         kComPortChoice = 4;
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
