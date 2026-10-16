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
};
