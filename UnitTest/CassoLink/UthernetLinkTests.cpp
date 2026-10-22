#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSettings.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Seams/Win32NetworkAdapters.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetLinkTests
//
//  The link over an Uthernet II: the transport's stored name, the W5100
//  builds of the shim and the program and where they fit, the network the
//  host writes into the shim's header, and that network as it is made from
//  a link's address and the PC's adapters.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UthernetLinkTests)
{
public:

    static NetworkAdapter  MakeAdapter (std::array<Byte, 4> address, std::array<Byte, 4> mask, std::array<Byte, 4> gateway)
    {
        NetworkAdapter  adapter;

        adapter.address = address;
        adapter.mask    = mask;
        adapter.gateway = gateway;

        return adapter;
    }

    static CassoLinkSettings  MakeLink (const std::string & address)
    {
        CassoLinkSettings  settings;

        settings.transport = LinkTransport::Uthernet;
        settings.address   = address;

        return settings;
    }




    TEST_METHOD (EveryTransport_RoundTripsByItsStoredName)
    {
        LinkTransport  parsed = LinkTransport::Serial;

        for (LinkTransport transport : { LinkTransport::Serial, LinkTransport::Tcp, LinkTransport::WifiModem, LinkTransport::Emulated, LinkTransport::Uthernet })
        {
            Assert::IsTrue (CassoLinkSettings::TryParseTransport (CassoLinkSettings::GetTransportName (transport), parsed));
            Assert::IsTrue (parsed == transport);
        }

        Assert::AreEqual ("Uthernet", CassoLinkSettings::GetTransportName (LinkTransport::Uthernet));
    }



    TEST_METHOD (OnlyAnUthernetLink_UsesTheW5100)
    {
        Assert::IsTrue (CassoLinkSession::GetLinkChip (LinkTransport::Uthernet) == LinkChip::W5100);

        for (LinkTransport transport : { LinkTransport::Serial, LinkTransport::Tcp, LinkTransport::WifiModem, LinkTransport::Emulated })
        {
            Assert::IsTrue (CassoLinkSession::GetLinkChip (transport) == LinkChip::Acia);
        }
    }



    TEST_METHOD (EachShimAndProgram_FitsWhereItLoads)
    {
        std::vector<Byte>  acia;
        std::vector<Byte>  w5100;
        std::vector<Byte>  program;
        std::string        errors;

        AssertSucceeded (LinkProgram::AssembleShim (acia, errors, LinkChip::Acia));
        Assert::IsTrue (acia.size() <= LinkProgram::kShimLimit, std::format (L"the 6551 shim has {} bytes", acia.size()).c_str());

        AssertSucceeded (LinkProgram::AssembleShim (w5100, errors, LinkChip::W5100));
        Assert::IsTrue (errors.empty(), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (w5100.size() <= LinkProgram::kW5100ShimLimit, std::format (L"the W5100 shim has {} bytes", w5100.size()).c_str());
        Assert::IsTrue (w5100.size() > acia.size(), L"the W5100 build has its own driver");

        AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::W5100));
        Assert::IsTrue (LinkProgram::kProgramAddress + program.size() <= LinkProgram::kW5100ZeroPageSave, L"below the W5100 shim's copy of the zero page");
        Assert::AreEqual<int> (LinkProgram::kProgramVersion & 0xFF, program[3], L"the version the shim draws, at $0803");
    }



    TEST_METHOD (BuildShim_WritesTheNetworkAndSlotIntoAW5100Header)
    {
        constexpr Byte     kSlot  = 5;
        LinkShimOptions    options;
        std::vector<Byte>  assembled;
        std::vector<Byte>  shim;
        std::string        errors;

        options.chip            = LinkChip::W5100;
        options.slot            = kSlot;
        options.network.address = { 192, 168, 1, 50 };
        options.network.mask    = { 255, 255, 0, 0 };
        options.network.gateway = { 192, 168, 0, 1 };
        options.network.mac     = { 0x02, 0x43, 0x41, 0x53, 0x53, 0x32 };
        options.network.port    = 0x1966;

        AssertSucceeded (LinkProgram::AssembleShim (assembled, errors, LinkChip::W5100));
        AssertSucceeded (LinkProgram::BuildShim (options, shim));

        Assert::AreEqual (assembled.size(), shim.size());
        Assert::IsTrue (std::equal (options.network.address.begin(), options.network.address.end(), shim.begin() + LinkProgram::kNetAddressOffset));
        Assert::IsTrue (std::equal (options.network.mask.begin(),    options.network.mask.end(),    shim.begin() + LinkProgram::kNetMaskOffset));
        Assert::IsTrue (std::equal (options.network.gateway.begin(), options.network.gateway.end(), shim.begin() + LinkProgram::kNetGatewayOffset));
        Assert::IsTrue (std::equal (options.network.mac.begin(),     options.network.mac.end(),     shim.begin() + LinkProgram::kNetMacOffset));
        Assert::AreEqual<int> (0x19,  shim[LinkProgram::kNetPortOffset],     L"the port, high byte first");
        Assert::AreEqual<int> (0x66,  shim[LinkProgram::kNetPortOffset + 1]);
        Assert::AreEqual<int> (kSlot, shim[LinkProgram::kSlotOffset]);
        Assert::AreEqual<int> (assembled[LinkProgram::kSpeedOffset], shim[LinkProgram::kSpeedOffset], L"no 6551 speed for a W5100");
    }



    TEST_METHOD (AnAddressOnAnAdaptersSubnet_TakesItsMaskAndGateway)
    {
        std::vector<NetworkAdapter>  adapters = { MakeAdapter ({ 10, 0, 0, 7 },     { 255, 0, 0, 0 },     { 10, 0, 0, 254 }),
                                                  MakeAdapter ({ 192, 168, 1, 20 }, { 255, 255, 255, 0 }, { 192, 168, 1, 1 }) };
        LinkNetworkConfig            network;
        std::array<Byte, 4>          address  = { 192, 168, 1, 50 };
        std::array<Byte, 4>          mask     = { 255, 255, 255, 0 };
        std::array<Byte, 4>          gateway  = { 192, 168, 1, 1 };
        std::array<Byte, 6>          mac      = { 0x02, 'C', 'A', 'S', 'S', 50 };

        AssertSucceeded (CassoLinkConnections::MakeNetworkConfig (MakeLink ("192.168.1.50:6400"), adapters, network));

        Assert::IsTrue (network.address == address);
        Assert::IsTrue (network.mask    == mask);
        Assert::IsTrue (network.gateway == gateway);
        Assert::IsTrue (network.mac     == mac, L"locally administered, ending in the address's last byte");
        Assert::AreEqual<int> (6400, network.port);
    }



    TEST_METHOD (AnAddressOnNoAdaptersSubnet_TakesAClassCMaskAndTheSubnetsFirstHost)
    {
        std::vector<NetworkAdapter>  adapters = { MakeAdapter ({ 10, 0, 0, 7 }, { 255, 0, 0, 0 }, { 10, 0, 0, 254 }) };
        LinkNetworkConfig            network;
        std::array<Byte, 4>          mask     = { 255, 255, 255, 0 };
        std::array<Byte, 4>          gateway  = { 172, 16, 5, 1 };

        AssertSucceeded (CassoLinkConnections::MakeNetworkConfig (MakeLink ("172.16.5.9:6502"), adapters, network));

        Assert::IsTrue (network.mask    == mask);
        Assert::IsTrue (network.gateway == gateway);
        Assert::AreEqual<int> (6502, network.port);
    }



    TEST_METHOD (AHostName_OrABadPort_IsNotANetwork)
    {
        LinkNetworkConfig  network;
        HRESULT            hostName = CassoLinkConnections::MakeNetworkConfig (MakeLink ("apple.local:6502"), {}, network);
        HRESULT            tooHigh  = CassoLinkConnections::MakeNetworkConfig (MakeLink ("10.0.0.2:70000"), {}, network);
        HRESULT            worded   = CassoLinkConnections::MakeNetworkConfig (MakeLink ("10.0.0.2:port"), {}, network);

        Assert::IsTrue (FAILED (hostName), L"the chip takes an address, not a name");
        Assert::IsTrue (FAILED (tooHigh),  L"a port above 65535");
        Assert::IsTrue (FAILED (worded));
    }



    TEST_METHOD (AnotherTransportsShim_IsThe6551s)
    {
        CassoLinkSettings  settings;
        LinkShimOptions    options;

        settings.transport = LinkTransport::WifiModem;
        settings.address   = "0.0.0.0:6400";
        settings.baud      = 19200;

        options = CassoLinkConnections::MakeShimOptions (L"CASSO", settings);

        Assert::IsTrue (options.chip == LinkChip::Acia);
        Assert::AreEqual ((DWORD) 19200, options.baud);
        Assert::AreEqual (std::string ("ATDT CASSO.local:6400"), options.dial);
    }
};
