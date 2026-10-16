#include "Pch.h"

#include "CassoExplorer/CassoExplorerLinkDialog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialogTests
//
//  The link dialog's rules and the settings its choices become.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerLinkDialogTests)
{
public:

    TEST_METHOD (Name_MustBeNewAndUsable)
    {
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateName (L"Apple II", { L"Other" }).empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateName (L"", {}).empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateName (L"   ", {}).empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateName (L"apple ii", { L"Apple II" }).empty(), L"names differ in more than case");
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateName (L"A|B", {}).empty(), L"the id's separator");
    }



    TEST_METHOD (Port_IsAComPortOrBlank)
    {
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidatePort (L"COM3").empty());
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidatePort (L"com12").empty());
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidatePort (L"").empty(), L"blank searches");
        Assert::IsFalse (CassoExplorerLinkChoices::ValidatePort (L"COM").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidatePort (L"COM0").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidatePort (L"LPT1").empty());
    }



    TEST_METHOD (Address_IsAHostAndAPort)
    {
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateAddress (L"127.0.0.1:6502").empty());
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateAddress (L"apple.local:23").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L"127.0.0.1").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L":6502").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L"host:0").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L"host:70000").empty());
    }



    TEST_METHOD (MakeSettings_KeepsOnlyTheTransportsField)
    {
        AppleLinkSettings  serial   = CassoExplorerLinkChoices::MakeSettings (L"S", CassoExplorerLinkChoices::GetTransportIndex (LinkTransport::Serial), L"COM3", L"a:1",
                                                                              CassoExplorerLinkChoices::GetSpeedIndex (19200), CassoExplorerLinkChoices::GetSpeedIndex (9600), true);
        AppleLinkSettings  emulated = CassoExplorerLinkChoices::MakeSettings (L"E", CassoExplorerLinkChoices::GetTransportIndex (LinkTransport::Emulated), L"COM3", L"127.0.0.1:6502",
                                                                              0, 0, false);

        Assert::IsTrue   (serial.transport == LinkTransport::Serial);
        Assert::AreEqual (std::string ("COM3"), serial.port);
        Assert::IsTrue   (serial.address.empty());
        Assert::AreEqual ((DWORD) 19200, serial.baud);
        Assert::AreEqual ((DWORD) 9600,  serial.bootstrapBaud);
        Assert::IsTrue   (serial.hardwareHandshake);

        Assert::IsTrue   (emulated.transport == LinkTransport::Emulated);
        Assert::IsTrue   (emulated.port.empty());
        Assert::AreEqual (std::string ("127.0.0.1:6502"), emulated.address);
        Assert::AreEqual ((DWORD) 115200, emulated.baud);
    }


    TEST_METHOD (EachTransport_HasOnlyItsOwnRows)
    {
        using C = CassoExplorerLinkChoices;

        Assert::IsTrue (C::UsesPort (LinkTransport::Serial) && C::UsesSpeed (LinkTransport::Serial) && C::UsesBootstrap (LinkTransport::Serial) && C::UsesHandshake (LinkTransport::Serial));
        Assert::IsFalse (C::UsesAddress (LinkTransport::Serial));

        for (LinkTransport tcp : { LinkTransport::Emulated, LinkTransport::Tcp })
        {
            Assert::IsTrue  (C::UsesAddress (tcp));
            Assert::IsFalse (C::UsesPort (tcp) || C::UsesSpeed (tcp) || C::UsesBootstrap (tcp) || C::UsesHandshake (tcp), L"no wires: no speed or handshake");
        }

        Assert::IsTrue  (C::UsesAddress (LinkTransport::WifiModem) && C::UsesSpeed (LinkTransport::WifiModem) && C::UsesHandshake (LinkTransport::WifiModem));
        Assert::IsFalse (C::UsesPort (LinkTransport::WifiModem) || C::UsesBootstrap (LinkTransport::WifiModem));

        Assert::AreEqual (std::wstring (L"127.0.0.1:6502"), C::GetDefaultAddress (LinkTransport::Emulated));
        Assert::AreEqual (std::wstring (L"TCP port:"),      std::wstring (C::GetAddressLabel (LinkTransport::WifiModem)));
    }



    TEST_METHOD (MakeSettings_GivesHiddenRowsTheirDefaults)
    {
        using C = CassoExplorerLinkChoices;

        AppleLinkSettings  emulated = C::MakeSettings (L"//c", C::GetTransportIndex (LinkTransport::Emulated), L"COM3", L"127.0.0.1:6502", C::GetSpeedIndex (9600), C::GetSpeedIndex (1200), true);
        AppleLinkSettings  modem    = C::MakeSettings (L"Modem", C::GetTransportIndex (LinkTransport::WifiModem), L"", L"6400", C::GetSpeedIndex (19200), C::GetSpeedIndex (1200), true);

        Assert::AreEqual (std::string(),               emulated.port);
        Assert::AreEqual ((DWORD) 115200,              emulated.baud,          L"the link's speed");
        Assert::AreEqual ((DWORD) 9600,                emulated.bootstrapBaud);
        Assert::IsFalse  (emulated.hardwareHandshake);

        Assert::AreEqual (std::string ("0.0.0.0:6400"), modem.address, L"a Wi-Fi modem connects from the network");
        Assert::AreEqual ((DWORD) 19200,                modem.baud);
        Assert::AreEqual ((DWORD) 9600,                 modem.bootstrapBaud, L"no bootstrap speed over a modem");
        Assert::IsTrue   (modem.hardwareHandshake);
        Assert::AreEqual (std::wstring (L"6400"),       C::GetAddressText (modem), L"the row is the TCP port alone");

        Assert::IsTrue  (C::ValidateAddressFor (LinkTransport::WifiModem, L"6400").empty());
        Assert::IsFalse (C::ValidateAddressFor (LinkTransport::WifiModem, L"70000").empty());
        Assert::IsFalse (C::ValidateAddressFor (LinkTransport::Emulated,  L"6400").empty(), L"an emulated machine's row is host and port");
    }
};