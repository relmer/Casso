#include "Pch.h"

#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialogTests
//
//  The connection dialog's rules and the settings its choices become.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerLinkDialogTests)
{
public:

    TEST_METHOD (Address_IsAHostAndAPort_AndEmptyHasNoError)
    {
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateAddress (L"127.0.0.1:6502").empty());
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateAddress (L"apple.local:23").empty());
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateAddress (L"").empty(), L"an empty address has no error displayed");
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L"127.0.0.1").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L":6502").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L"host:0").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddress (L"host:70000").empty());
    }



    TEST_METHOD (UthernetAddress_IsAnIpv4Address)
    {
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateAddressFor (LinkTransport::Uthernet, L"192.168.1.50:6502").empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateAddressFor (LinkTransport::Uthernet, L"apple.local:6502").empty());
        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateAddressFor (LinkTransport::Tcp, L"apple.local:6502").empty());
    }



    TEST_METHOD (PortChoices_AreAnyOrTcpThenThePortsFound)
    {
        std::vector<std::wstring>  ports = { L"COM3", L"COM4" };
        CassoLinkSettings          link;



        Assert::IsTrue (CassoExplorerLinkChoices::GetPortLabels (LinkTransport::Serial, ports) == std::vector<std::wstring> ({ L"Any", L"COM3", L"COM4" }));
        Assert::IsTrue (CassoExplorerLinkChoices::GetPortLabels (LinkTransport::AdtProServer, ports) == std::vector<std::wstring> ({ L"TCP address", L"COM3", L"COM4" }));

        link.port = "com4";
        Assert::AreEqual (2, CassoExplorerLinkChoices::GetPortIndex (link, ports));

        link.port = "COM9";
        Assert::AreEqual (0, CassoExplorerLinkChoices::GetPortIndex (link, ports), L"a port not found now is the first choice");
    }



    TEST_METHOD (MakeSettings_KeepsOnlyTheTransportsFields_AndMakesTheName)
    {
        std::vector<std::wstring>  ports  = { L"COM3" };
        int                        speed  = CassoExplorerLinkChoices::GetSpeedIndex (19200);
        int                        boot   = CassoExplorerLinkChoices::GetSpeedIndex (4800);
        CassoLinkSettings          serial = CassoExplorerLinkChoices::MakeSettings (CassoExplorerLinkChoices::GetTransportIndex (LinkTransport::Serial), ports, 1, L"a:1",
                                                                                     speed, boot, true, L"C:\\x");
        CassoLinkSettings          any    = CassoExplorerLinkChoices::MakeSettings (CassoExplorerLinkChoices::GetTransportIndex (LinkTransport::Serial), ports, 0, L"",
                                                                                     0, 0, false, L"");
        CassoLinkSettings          tcp    = CassoExplorerLinkChoices::MakeSettings (CassoExplorerLinkChoices::GetTransportIndex (LinkTransport::Tcp), ports, 1, L"192.168.1.5:6502",
                                                                                     speed, boot, true, L"");
        CassoLinkSettings          modem  = CassoExplorerLinkChoices::MakeSettings (CassoExplorerLinkChoices::GetTransportIndex (LinkTransport::WifiModem), ports, 0, L"x:1",
                                                                                     0, 0, false, L"");



        Assert::AreEqual (std::string ("COM3"), serial.name);
        Assert::AreEqual (std::string ("COM3"), serial.port);
        Assert::AreEqual (std::string(), serial.address);
        Assert::AreEqual ((DWORD) 19200, serial.baud);
        Assert::AreEqual ((DWORD) 4800, serial.bootstrapBaud);
        Assert::IsTrue   (serial.hardwareHandshake);
        Assert::IsTrue   (serial.folder.empty());

        Assert::AreEqual (std::string ("Serial (any port)"), any.name);

        Assert::AreEqual (std::string ("TCP 192.168.1.5:6502"), tcp.name);
        Assert::AreEqual (std::string(), tcp.port);
        Assert::AreEqual (CassoLinkSettings::kDefaultBaud, tcp.baud, L"TCP has no speed");
        Assert::IsFalse  (tcp.hardwareHandshake);

        Assert::AreEqual (std::string ("Wi-Fi modem on port 6400"), modem.name);
        Assert::AreEqual (std::string ("0.0.0.0:6400"), modem.address, L"a Wi-Fi modem listens on 6400 on every address");
    }



    TEST_METHOD (AdtProServer_IsOnAComPortOrATcpAddress_WithItsFolder)
    {
        std::vector<std::wstring>  ports  = { L"COM3" };
        int                        server = CassoExplorerLinkChoices::GetTransportIndex (LinkTransport::AdtProServer);
        CassoLinkSettings          onCom  = CassoExplorerLinkChoices::MakeSettings (server, ports, 1, L"x:1", 0, 0, true, L"C:\\Disks");
        CassoLinkSettings          onTcp  = CassoExplorerLinkChoices::MakeSettings (server, ports, 0, L"127.0.0.1:6502", 0, 0, true, L"C:\\Disks");
        CassoLinkSettings          bare   = CassoExplorerLinkChoices::MakeSettings (server, ports, 0, L"127.0.0.1:6502", 0, 0, false, L"");



        Assert::AreEqual (std::string ("ADTPro server on COM3"), onCom.name);
        Assert::AreEqual (std::wstring (L"C:\\Disks"), onCom.folder);
        Assert::IsTrue   (onCom.hardwareHandshake);
        Assert::IsTrue   (onCom.IsComPort());

        Assert::AreEqual (std::string ("ADTPro server at 127.0.0.1:6502"), onTcp.name);
        Assert::IsFalse  (onTcp.hardwareHandshake, L"TCP has no handshake");
        Assert::IsFalse  (onTcp.IsComPort());

        Assert::IsTrue   (CassoExplorerLinkChoices::IsComplete (onTcp));
        Assert::IsFalse  (CassoExplorerLinkChoices::IsComplete (bare), L"a server with no folder is incomplete");
    }



    TEST_METHOD (Rows_FollowTheTransport)
    {
        Assert::IsTrue  (CassoExplorerLinkChoices::UsesBootstrap (LinkTransport::Serial));
        Assert::IsFalse (CassoExplorerLinkChoices::UsesBootstrap (LinkTransport::AdtProServer));
        Assert::IsTrue  (CassoExplorerLinkChoices::UsesSpeed     (LinkTransport::WifiModem, 0));
        Assert::IsFalse (CassoExplorerLinkChoices::UsesSpeed     (LinkTransport::Tcp, 0));
        Assert::IsTrue  (CassoExplorerLinkChoices::UsesAddress   (LinkTransport::AdtProServer, 0));
        Assert::IsFalse (CassoExplorerLinkChoices::UsesAddress   (LinkTransport::AdtProServer, 1));
        Assert::IsFalse (CassoExplorerLinkChoices::UsesAddress   (LinkTransport::WifiModem, 0), L"the modem's port is fixed");
        Assert::IsTrue  (CassoExplorerLinkChoices::UsesFolder    (LinkTransport::AdtProServer));
    }



    TEST_METHOD (IsComplete_NeedsAnAddressWhereThereIsOne)
    {
        CassoLinkSettings  tcp;



        tcp.transport = LinkTransport::Tcp;
        Assert::IsFalse (CassoExplorerLinkChoices::IsComplete (tcp), L"Add is disabled with no error displayed");

        tcp.address = "192.168.1.5:6502";
        Assert::IsTrue (CassoExplorerLinkChoices::IsComplete (tcp));
    }



    TEST_METHOD (IsComplete_AnUthernetWithNoAddressIsASearch)
    {
        CassoLinkSettings  uthernet;



        uthernet.transport = LinkTransport::Uthernet;
        Assert::IsTrue (CassoExplorerLinkChoices::IsComplete (uthernet));
        Assert::IsTrue (uthernet.IsSearch());

        uthernet.address = "192.168.1.50:6502";
        Assert::IsFalse (uthernet.IsSearch());
    }



    TEST_METHOD (Duplicate_IsAConnectionAnotherHasTheNameOf)
    {
        CassoLinkSettings  serial;



        serial.transport = LinkTransport::Serial;
        serial.name      = CassoLinkSettings::MakeName (serial);

        Assert::IsTrue  (CassoExplorerLinkChoices::ValidateDuplicate (serial, { L"COM3" }).empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateDuplicate (serial, { L"Serial (any port)" }).empty());
        Assert::IsFalse (CassoExplorerLinkChoices::ValidateDuplicate (serial, { L"serial (ANY port)" }).empty(), L"a match in all but case is a duplicate");
    }
};
