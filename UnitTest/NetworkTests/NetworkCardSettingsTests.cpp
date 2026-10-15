#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettingsTests
//
//  The `network` object on the card's slot entry.
//
////////////////////////////////////////////////////////////////////////////////

namespace NetworkCardSettingsTests
{
    static bool Fails (HRESULT hr)
    {
        return FAILED (hr);
    }


    static JsonValue Parse (const char * text)
    {
        JsonValue        value;
        JsonParseError   error;

        AssertSucceeded (JsonParser::Parse (text, value, error));

        return value;
    }





    TEST_CLASS (NetworkCardSettingsTests)
    {
    public:

        TEST_METHOD (ReadsEveryKey)
        {
            NetworkCardSettings       settings;
            std::vector<std::string>  warnings;

            AssertSucceeded (NetworkCardSettings::FromJson (Parse (R"({
                "mac": "02:CA:55:12:34:56", "lanAccess": true,
                "portForwards": [ { "protocol": "tcp", "hostPort": 8080, "guestPort": 80 },
                                  { "protocol": "udp", "hostPort": 6502, "guestPort": 6502 } ] })"), settings, warnings));

            Assert::AreEqual (std::string ("02:CA:55:12:34:56"), settings.mac.Format());
            Assert::IsTrue (settings.lanAccess);
            Assert::AreEqual ((size_t) 2, settings.portForwards.size());
            Assert::IsTrue (settings.portForwards[1] == PortForward { PortForwardProtocol::Udp, 6502, 6502 });
            Assert::IsTrue (warnings.empty());
        }


        TEST_METHOD (MissingKeysTakeDefaults)
        {
            NetworkCardSettings       settings;
            std::vector<std::string>  warnings;

            AssertSucceeded (NetworkCardSettings::FromJson (Parse ("{}"), settings, warnings));

            Assert::IsTrue (settings == NetworkCardSettings {});
            Assert::IsFalse (settings.mac.IsSet());
            Assert::IsTrue (warnings.empty());
        }


        TEST_METHOD (BadValuesTakeDefaultsWithWarnings)
        {
            NetworkCardSettings       settings;
            std::vector<std::string>  warnings;

            AssertSucceeded (NetworkCardSettings::FromJson (Parse (R"({
                "mac": "01:00:5E:00:00:01",
                "portForwards": [ { "protocol": "icmp", "hostPort": 1, "guestPort": 1 },
                                  { "protocol": "tcp", "hostPort": 70000, "guestPort": 80 },
                                  { "protocol": "tcp", "hostPort": 8080, "guestPort": 80 },
                                  { "protocol": "tcp", "hostPort": 8080, "guestPort": 81 } ] })"), settings, warnings));

            Assert::IsFalse (settings.mac.IsSet(), L"multicast is not a card address");
            Assert::AreEqual ((size_t) 1, settings.portForwards.size(), L"only the first 8080 forward is valid");
            Assert::AreEqual ((size_t) 4, warnings.size());
        }


        TEST_METHOD (NonObjectFails)
        {
            NetworkCardSettings       settings;
            std::vector<std::string>  warnings;

            Assert::IsTrue (Fails (NetworkCardSettings::FromJson (Parse ("[]"), settings, warnings)));
        }


        TEST_METHOD (WritesWhatItReads)
        {
            NetworkCardSettings       original;
            NetworkCardSettings       reread;
            std::vector<std::string>  warnings;
            std::string               text;
            JsonWriter::Options       options;

            Assert::IsTrue (MacAddress::TryParse ("02:00:00:00:00:07", original.mac));
            original.lanAccess    = true;
            original.portForwards = { { PortForwardProtocol::Tcp, 2323, 23 } };

            options.fPretty = false;
            AssertSucceeded (JsonWriter::Write (original.ToJson(), options, text));
            AssertSucceeded (NetworkCardSettings::FromJson (Parse (text.c_str()), reread, warnings));

            Assert::IsTrue (reread == original);
        }


        TEST_METHOD (ValidatesPortsAndDuplicates)
        {
            std::vector<PortForward>  existing = { { PortForwardProtocol::Tcp, 8080, 80 } };

            Assert::IsTrue  (NetworkCardSettings::ValidatePortForward ({ PortForwardProtocol::Tcp, 8081, 80 }, existing).empty());
            Assert::IsTrue  (NetworkCardSettings::ValidatePortForward ({ PortForwardProtocol::Udp, 8080, 80 }, existing).empty());
            Assert::IsFalse (NetworkCardSettings::ValidatePortForward ({ PortForwardProtocol::Tcp, 8080, 81 }, existing).empty());
            Assert::IsFalse (NetworkCardSettings::ValidatePortForward ({ PortForwardProtocol::Tcp, 0, 81 }, existing).empty());
            Assert::IsFalse (NetworkCardSettings::ValidatePortForward ({ PortForwardProtocol::Tcp, 81, 0 }, existing).empty());
        }
    };
}
