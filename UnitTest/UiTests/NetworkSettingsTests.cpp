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
//  The Uthernet II in the Settings sheet's model: enabling it (adding its
//  slot entry when the saved slot list predates it), choosing its slot, its
//  MAC address, LAN access and port forwards, and what a save writes.
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

    // A user file saved before the card existed: its slot list replaces the
    // shipped one and has no card entry.
    static constexpr const char *  s_kpszOldUser = R"JSON({
        "name": "Apple //e",
        "slots": [
            { "slot": 1, "device": "parallel-printer", "capabilityFlag": "optional" },
            { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
        ]
    })JSON";

    static constexpr const char *  s_kpszFullSlots = R"JSON({
        "name": "Full",
        "slots": [
            { "slot": 1, "device": "parallel-printer" },
            { "slot": 2, "device": "parallel-printer" },
            { "slot": 3, "device": "parallel-printer" },
            { "slot": 4, "device": "mockingboard" },
            { "slot": 5, "device": "parallel-printer" },
            { "slot": 6, "device": "disk-ii" },
            { "slot": 7, "device": "parallel-printer" }
        ]
    })JSON";

    static constexpr const char *  s_kpszNoSlots = R"JSON({
        "name": "No slots"
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


        TEST_METHOD (MovingTheCardResetsTheMachineInsteadOfReconfiguring)
        {
            SettingsPanelState  state;
            NetworkSink         sink;
            JsonValue           saved;

            Load (state, s_kpszEnabled);

            state.SetNetworkLanAccess (true);
            AssertSucceeded (state.SetNetworkCardSlot (4));

            Assert::IsTrue (state.RequiresReset());

            AssertSucceeded (state.Apply (sink, saved));

            Assert::AreEqual (0, sink.applied);
            Assert::AreEqual (1, sink.resets);
        }

        TEST_METHOD (ShippedEntryIsFoundDisabled)
        {
            SettingsPanelState  state;

            Load (state, s_kpszShipped);

            Assert::IsNotNull (state.GetNetworkCard());
            Assert::IsFalse (state.GetNetworkCard()->enabled);
            Assert::AreEqual (3, state.GetNetworkCard()->slot);
        }


        TEST_METHOD (EnablingGeneratesALocalUnicastMacAndRequiresAReset)
        {
            SettingsPanelState  state;

            Load (state, s_kpszShipped);
            AssertSucceeded (state.SetNetworkCardEnabled (true));

            const MacAddress  & mac = state.GetNetworkCard()->network.mac;

            Assert::IsTrue (mac.IsSet());
            Assert::IsTrue (mac.IsUnicast());
            Assert::AreEqual ((Byte) 0x02, (Byte) (mac.GetOctets()[0] & 0x02), L"locally administered");
            Assert::IsTrue (state.IsDirty());
            Assert::IsTrue (state.RequiresReset());
        }


        TEST_METHOD (EnablingAddsTheEntryWhenTheSavedListPredatesIt)
        {
            SettingsPanelState  state;
            std::string         saved;

            Load (state, s_kpszOldUser);
            Assert::IsNull (state.GetNetworkCard());

            AssertSucceeded (state.SetNetworkCardEnabled (true));

            Assert::IsNotNull (state.GetNetworkCard());
            Assert::AreEqual (3, state.GetNetworkCard()->slot);
            Assert::IsTrue (state.RequiresReset());

            saved = Write (state.BuildCurrentJson());
            Assert::IsTrue (saved.find ("\"device\":\"uthernet2\"") != std::string::npos, TextEncoding::NarrowToWide (saved).c_str());
            Assert::IsTrue (saved.find ("\"network\"") != std::string::npos);
        }


        TEST_METHOD (EnablingWithEverySlotTakenFails)
        {
            SettingsPanelState  state;

            Load (state, s_kpszFullSlots);

            Assert::IsTrue (Fails (state.SetNetworkCardEnabled (true)));
            Assert::IsNull (state.GetNetworkCard());
        }


        TEST_METHOD (TheCardIsNotOfferedWithoutASlotList)
        {
            SettingsPanelState  state;

            Load (state, s_kpszNoSlots);

            Assert::IsFalse (state.GetMachineInfo().supportsSlots);
            Assert::IsTrue (Fails (state.SetNetworkCardEnabled (true)));
        }


        TEST_METHOD (SlotMovesOnlyToAFreeSlot)
        {
            SettingsPanelState  state;
            std::vector<int>    free;

            Load (state, s_kpszShipped);
            AssertSucceeded (state.SetNetworkCardEnabled (true));

            Assert::IsTrue (Fails (state.SetNetworkCardSlot (6)), L"slot 6 has the Disk II");
            Assert::IsTrue (Fails (state.SetNetworkCardSlot (0)));
            Assert::IsTrue (Fails (state.SetNetworkCardSlot (8)));

            AssertSucceeded (state.SetNetworkCardSlot (5));
            Assert::AreEqual (5, state.GetNetworkCard()->slot);
            Assert::AreEqual (std::string ("Slot 5: Uthernet II (Ethernet)"), state.GetNetworkCard()->displayName);

            free = state.GetFreeSlots ((size_t) (state.GetNetworkCard() - state.GetHardware().data()));
            Assert::IsTrue (free == std::vector<int> { 2, 3, 4, 5, 7 });
        }


        TEST_METHOD (InvalidMacKeepsTheStoredAddress)
        {
            SettingsPanelState  state;
            std::string         error;
            MacAddress          before;

            Load (state, s_kpszShipped);
            AssertSucceeded (state.SetNetworkCardEnabled (true));
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
            AssertSucceeded (state.SetNetworkCardEnabled (true));

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

            Load (state, s_kpszOldUser);
            AssertSucceeded (state.SetNetworkCardEnabled (true));
            AssertSucceeded (state.AddPortForward ({ PortForwardProtocol::Tcp, 2323, 23 }, error));

            state.Cancel();

            Assert::IsNull (state.GetNetworkCard());
            Assert::IsFalse (state.IsDirty());
        }


        TEST_METHOD (SavedSettingsLoadBack)
        {
            SettingsPanelState  first;
            SettingsPanelState  second;
            std::string         error;
            std::string         saved;

            Load (first, s_kpszShipped);
            AssertSucceeded (first.SetNetworkCardEnabled (true));
            AssertSucceeded (first.SetNetworkCardMac ("02:CA:55:00:00:42", error));
            AssertSucceeded (first.AddPortForward ({ PortForwardProtocol::Tcp, 2323, 23 }, error));

            saved = Write (first.BuildCurrentJson());
            AssertSucceeded (second.LoadFromMachine ("Custom", Parse (s_kpszShipped), Parse (saved.c_str())));

            Assert::IsTrue (second.GetNetworkCard()->enabled);
            Assert::IsTrue (second.GetNetworkCard()->network == first.GetNetworkCard()->network);
        }
    };
}
