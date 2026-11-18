#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"
#include "SettingsRecordingSink.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelSerialTests
//
//  The Settings model's serial connectors: the COM port, TCP and modem
//  options of the card's connector and the //c's two ports, read from the
//  machine's file, written back, and the reset a change to one of them
//  requires.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SettingsPanelSerialTests)
{
public:

    using RecordingSink = SettingsRecordingSink;


    JsonValue ParseOrFail (const std::string & text)
    {
        JsonValue        v;
        JsonParseError   e;
        HRESULT          hr = JsonParser::Parse (text, v, e);
        AssertSucceeded (hr, L"test fixture JSON must parse");
        return v;
    }


    std::string WriteCompactOrFail (const JsonValue & value)
    {
        std::string          text;
        JsonWriter::Options  opts;

        opts.fPretty = false;
        AssertSucceeded (JsonWriter::Write (value, opts, text),
                         L"emitted JSON must serialize");

        return text;
    }


    static constexpr const char *  kSerialCardJson = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "TestMachine",
        "internalDevices": [ { "type": "keyboard" } ],
        "slots": [
            { "slot": 2, "device": "super-serial-card", "firmware": "SSC.rom", "enabled": true,
              "switches": { "sw1": "off off off on on on on", "sw2": "on on off on off off off" },
              "ports": [ { "device": "serial-file-out", "path": "C:\\out.txt" } ] }
        ]
    })JSON";



    static constexpr const char *  kSerialTcpJson = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "TestMachine",
        "internalDevices": [ { "type": "keyboard" } ],
        "slots": [
            { "slot": 2, "device": "super-serial-card", "firmware": "SSC.rom", "enabled": true,
              "ports": [ { "device": "serial-tcp", "host": "peer.example.org", "port": 2323, "telnet": true } ] }
        ]
    })JSON";


    TEST_METHOD (SerialCard_AttachmentOptionsAreReadAndWrittenBack)
    {
        SettingsPanelState      st;
        RecordingSink           sink;
        JsonValue               v       = ParseOrFail (kSerialTcpJson);
        JsonValue               outJson;
        std::string             text;
        SerialAttachmentChoice  choice;



        AssertSucceeded (st.LoadFromMachine ("X", v, v));

        choice = st.GetSerialAttachment (SerialPortId::Card);
        Assert::AreEqual (std::string (kpszSerialTcp),      choice.device);
        Assert::AreEqual (std::string ("peer.example.org"), choice.options.host);
        Assert::AreEqual (2323,                             choice.options.tcpPort);
        Assert::IsTrue   (choice.options.IsTelnetOn (false));

        choice.device                 = kpszSerialModem;
        choice.options.modemListenPort = 6400;
        choice.options.telnet.reset();
        AssertSucceeded (st.SetSerialAttachment (SerialPortId::Card, choice));
        Assert::IsTrue (st.RequiresReset(), L"an attachment is opened when the machine is built");

        st.Apply (sink, outJson);
        text = WriteCompactOrFail (outJson);

        Assert::IsTrue  (text.find ("\"serial-modem\"")   != std::string::npos, TextEncoding::NarrowToWide (text).c_str());
        Assert::IsTrue  (text.find ("\"listenPort\":6400") != std::string::npos, L"the modem's listen port is saved");
        Assert::IsFalse (text.find ("\"telnet\"")         != std::string::npos, L"an unset Telnet option is not written");
    }


    TEST_METHOD (SerialCard_AnOptionChangeAloneRequiresAReset)
    {
        SettingsPanelState      st;
        JsonValue               v      = ParseOrFail (kSerialTcpJson);
        SerialAttachmentChoice  choice;



        AssertSucceeded (st.LoadFromMachine ("X", v, v));

        choice              = st.GetSerialAttachment (SerialPortId::Card);
        choice.options.host = "other.example.org";
        AssertSucceeded (st.SetSerialAttachment (SerialPortId::Card, choice));

        Assert::IsTrue (st.RequiresReset());
    }


    static constexpr const char *  kApple2cPortsJson = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "Apple2c",
        "internalDevices": [ { "type": "keyboard" } ],
        "ports": [
            { "name": "disk",     "device": "disk-iic-drive" },
            { "name": "serial1",  "device": "serial-com-port", "comPort": "COM3" },
            { "name": "serial2",  "device": "" },
            { "name": "joystick", "device": "" }
        ]
    })JSON";


    TEST_METHOD (Apple2cPorts_AttachmentsAreReadAndWrittenBack)
    {
        SettingsPanelState      st;
        RecordingSink           sink;
        JsonValue               v       = ParseOrFail (kApple2cPortsJson);
        JsonValue               outJson;
        std::string             text;
        SerialAttachmentChoice  choice;



        AssertSucceeded (st.LoadFromMachine ("Apple2c", v, v));

        Assert::IsTrue  (st.HasBuiltInSerialPorts());
        Assert::IsFalse (st.HasEnabledSerialCard());
        Assert::AreEqual (std::string ("COM3"), st.GetSerialAttachment (SerialPortId::Port1).options.comPort);
        Assert::AreEqual (std::string(),       st.GetSerialAttachment (SerialPortId::Port2).device);

        choice.device                  = kpszSerialModem;
        choice.options.modemListenPort = 6400;
        AssertSucceeded (st.SetSerialAttachment (SerialPortId::Port2, choice));
        Assert::IsTrue (st.RequiresReset());

        st.Apply (sink, outJson);
        text = WriteCompactOrFail (outJson);

        Assert::IsTrue (text.find ("\"serial-modem\"")    != std::string::npos, TextEncoding::NarrowToWide (text).c_str());
        Assert::IsTrue (text.find ("\"comPort\":\"COM3\"") != std::string::npos, L"port 1's COM port is kept");
        Assert::IsTrue (text.find ("\"disk-iic-drive\"")  != std::string::npos, L"the other ports are kept");
    }


    TEST_METHOD (Apple2cPorts_AMachineWithoutThemHasNone)
    {
        SettingsPanelState  st;
        JsonValue           v  = ParseOrFail (kSerialCardJson);
        HRESULT             hr = S_OK;



        AssertSucceeded (st.LoadFromMachine ("X", v, v));

        Assert::IsFalse (st.HasBuiltInSerialPorts());

        hr = st.SetSerialAttachment (SerialPortId::Port1, {});
        Assert::IsTrue (FAILED (hr));
    }
};
