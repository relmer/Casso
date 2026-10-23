#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkSettingsTests
//
//  The Uthernet II in the Settings sheet's model: turning it on in the
//  Machine tab, its MAC address, LAN access and port forwards, what a save
//  writes, and what applies to the running card.
//
////////////////////////////////////////////////////////////////////////////////

namespace NetworkSettingsTests
{
    static constexpr const char *  s_kpszShipped = R"JSON({
        "name": "Apple //e",
        "slots": [
            { "slot": 1, "device": "parallel-printer", "capabilityFlag": "optional" },
            { "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": false },
            { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
        ]
    })JSON";



    static constexpr const char *  s_kpszEnabled = R"JSON({
        "name": "Apple //e",
        "slots": [
            { "slot": 1, "device": "parallel-printer", "capabilityFlag": "optional" },
            { "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": true },
            { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
        ]
    })JSON";


    // The settings Apply sends for the card, and the resets it queues.
    class NetworkSink : public ISettingsApplySink
    {
    public:
        void  ApplySpeedMode              (SettingsSpeedMode)            override {}
        void  ApplyColorMode              (SettingsColorMode)            override {}
        void  ApplyFloppySound            (bool)                         override {}
        void  ApplyMechanism              (const std::string &)          override {}
        void  ApplyFastTapeLoading        (bool)                         override {}
        void  ApplyTapeVolume             (float)                        override {}
        void  ApplyTapeAutoStop           (bool)                         override {}
        void  ApplyTapeIdleStop           (bool)                         override {}
        void  ApplyTapeEightBit           (bool)                         override {}
        void  ApplyDriveVolumes           (float, float, float)          override {}
        void  ApplyDrivePan               (float, float)                 override {}
        void  ApplyWriteProtect           (int, bool)                    override {}
        void  ApplyExternalDriveConnected (bool)                         override {}
        void  ApplyMouseConnected         (bool)                         override {}
        void  ApplySerialPortDevice       (int, const std::string &, const std::string &) override {}
        void  ApplyParallelPrinter        (int, bool)                    override {}
        void  ApplySerialCardSwitches     (const std::string &, const std::string &) override {}
        void  ApplyNetworkCard            (const NetworkCardSettings & settings) override { last = settings; ++applied; }
        void  QueueMachineReset           ()                             override { ++resets; }

        NetworkCardSettings  last;
        int                  applied = 0;
        int                  resets  = 0;
    };

    static bool Fails (HRESULT hr)
    {
        return FAILED (hr);
    }


    static JsonValue Parse (const char * text)
    {
        JsonValue        value;
        JsonParseError   error;

        AssertSucceeded (JsonParser::Parse (text, value, error), L"fixture JSON must parse");

        return value;
    }


    static std::string Write (const JsonValue & value)
    {
        std::string          text;
        JsonWriter::Options  options;

        options.fPretty = false;
        AssertSucceeded (JsonWriter::Write (value, options, text));

        return text;
    }


    static void Load (SettingsPanelState & state, const char * merged)
    {
        AssertSucceeded (state.LoadFromMachine ("Custom", Parse (s_kpszShipped), Parse (merged)));
    }


    // Turns the card on as the Machine tab's tree does.
    static void TurnCardOn (SettingsPanelState & state, bool enabled = true)
    {
        const HardwareEntry  * card = state.GetNetworkCard();

        Assert::IsNotNull (card);
        AssertSucceeded (state.SetHardwareEnabled ((size_t) (card - state.GetHardware().data()), enabled));
    }





    TEST_CLASS (NetworkSettingsTests)
    {
    public:

        TEST_METHOD (LanAccessAndForwardsApplyToTheRunningCard)
        {
            SettingsPanelState  state;
            NetworkSink         sink;
            JsonValue           saved;
            std::string         error;
            PortForward         forward;

            Load (state, s_kpszEnabled);

            forward.hostPort  = 8080;
            forward.guestPort = 80;

            state.SetNetworkLanAccess (true);
            AssertSucceeded (state.AddPortForward (forward, error));

            Assert::IsFalse (state.RequiresReset());

            AssertSucceeded (state.Apply (sink, saved));

            Assert::AreEqual (1, sink.applied);
            Assert::AreEqual (0, sink.resets);
            Assert::IsTrue (sink.last.lanAccess);
            Assert::AreEqual ((size_t) 1, sink.last.portForwards.size());
        }


        TEST_METHOD (AnUnchangedCardIsNotReconfigured)
        {
            SettingsPanelState  state;
            NetworkSink         sink;
            JsonValue           saved;

            Load (state, s_kpszEnabled);

            AssertSucceeded (state.Apply (sink, saved));

            Assert::AreEqual (0, sink.applied);
        }


        TEST_METHOD (ShippedEntryIsFoundDisabled)
        {
            SettingsPanelState  state;

            Load (state, s_kpszShipped);

            Assert::IsNotNull (state.GetNetworkCard());
            Assert::IsFalse (state.GetNetworkCard()->enabled);
            Assert::AreEqual (3, state.GetNetworkCard()->slot);
        }


        TEST_METHOD (TurningTheCardOnGeneratesALocalUnicastMacAndRequiresAReset)
        {
            SettingsPanelState  state;

            Load (state, s_kpszShipped);
            Assert::IsFalse (state.HasEnabledNetworkCard());

            TurnCardOn (state);

            const MacAddress  & mac = state.GetNetworkCard()->network.mac;

            Assert::IsTrue (state.HasEnabledNetworkCard());
            Assert::IsTrue (mac.IsSet());
            Assert::IsTrue (mac.IsUnicast());
            Assert::AreEqual ((Byte) 0x02, (Byte) (mac.GetOctets()[0] & 0x02), L"locally administered");
            Assert::IsTrue (state.IsDirty());
            Assert::IsTrue (state.RequiresReset());
        }


        TEST_METHOD (TurningTheCardOffAndOnKeepsItsMac)
        {
            SettingsPanelState  state;
            MacAddress          first;

            Load (state, s_kpszShipped);
            TurnCardOn (state);
            first = state.GetNetworkCard()->network.mac;

            TurnCardOn (state, false);
            Assert::IsFalse (state.HasEnabledNetworkCard());

            TurnCardOn (state);
            Assert::IsTrue (state.GetNetworkCard()->network.mac == first);
        }


        TEST_METHOD (InvalidMacKeepsTheStoredAddress)
        {
            SettingsPanelState  state;
            std::string         error;
            MacAddress          before;

            Load (state, s_kpszShipped);
            TurnCardOn (state);
            before = state.GetNetworkCard()->network.mac;

            for (const char * bad : { "02:CA:55:12:34", "02:CA:55:12:34:ZZ", "01:00:5E:00:00:01", "02:CA-55:12:34:56" })
            {
                Assert::IsTrue (Fails (state.SetNetworkCardMac (bad, error)));
                Assert::IsTrue (error.starts_with ("Error: invalid MAC address\n"));
                Assert::IsTrue (state.GetNetworkCard()->network.mac == before);
            }

            AssertSucceeded (state.SetNetworkCardMac ("02-ca-55-12-34-56", error));
            Assert::AreEqual (std::string ("02:CA:55:12:34:56"), state.GetNetworkCard()->network.mac.Format());
            Assert::IsTrue (error.empty());
        }


        TEST_METHOD (LanAccessAndForwardsAreStagedAndSaved)
        {
            SettingsPanelState  state;
            std::string         error;
            std::string         saved;

            Load (state, s_kpszShipped);
            TurnCardOn (state);

            state.SetNetworkLanAccess (true);
            AssertSucceeded (state.AddPortForward ({ PortForwardProtocol::Tcp, 8080, 80 }, error));
            AssertSucceeded (state.AddPortForward ({ PortForwardProtocol::Udp, 8080, 6502 }, error), L"same port, other protocol");

            Assert::IsTrue (Fails (state.AddPortForward ({ PortForwardProtocol::Tcp, 8080, 23 }, error)));
            Assert::IsTrue (error.starts_with ("Error: invalid port forward\n"));
            Assert::IsTrue (Fails (state.AddPortForward ({ PortForwardProtocol::Tcp, 0, 23 }, error)));

            saved = Write (state.BuildCurrentJson());
            Assert::IsTrue (saved.find ("\"lanAccess\":true") != std::string::npos);
            Assert::IsTrue (saved.find ("\"protocol\":\"udp\",\"hostPort\":8080,\"guestPort\":6502") != std::string::npos,
                            TextEncoding::NarrowToWide (saved).c_str());

            AssertSucceeded (state.RemovePortForward (0));
            Assert::AreEqual ((size_t) 1, state.GetNetworkCard()->network.portForwards.size());
            Assert::IsTrue (Fails (state.RemovePortForward (5)));
        }


        TEST_METHOD (UntouchedCardWritesNoNetworkObject)
        {
            SettingsPanelState  state;
            std::string         saved;

            Load (state, s_kpszShipped);

            saved = Write (state.BuildCurrentJson());
            Assert::IsTrue (saved.find ("\"network\"") == std::string::npos, TextEncoding::NarrowToWide (saved).c_str());
            Assert::IsFalse (state.IsDirty());
        }


        TEST_METHOD (CancelRestoresTheCard)
        {
            SettingsPanelState  state;
            std::string         error;

            Load (state, s_kpszShipped);
            TurnCardOn (state);
            AssertSucceeded (state.AddPortForward ({ PortForwardProtocol::Tcp, 2323, 23 }, error));

            state.Cancel();

            Assert::IsFalse (state.HasEnabledNetworkCard());
            Assert::IsTrue (state.GetNetworkCard()->network.portForwards.empty());
            Assert::IsFalse (state.IsDirty());
        }


        TEST_METHOD (SavedSettingsLoadBack)
        {
            SettingsPanelState  first;
            SettingsPanelState  second;
            std::string         error;
            std::string         saved;

            Load (first, s_kpszShipped);
            TurnCardOn (first);
            AssertSucceeded (first.SetNetworkCardMac ("02:CA:55:00:00:42", error));
            AssertSucceeded (first.AddPortForward ({ PortForwardProtocol::Tcp, 2323, 23 }, error));

            saved = Write (first.BuildCurrentJson());
            AssertSucceeded (second.LoadFromMachine ("Custom", Parse (s_kpszShipped), Parse (saved.c_str())));

            Assert::IsTrue (second.GetNetworkCard()->enabled);
            Assert::IsTrue (second.GetNetworkCard()->network == first.GetNetworkCard()->network);
        }
    };
}
