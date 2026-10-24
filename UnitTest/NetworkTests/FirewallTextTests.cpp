#include "Pch.h"

#include "Net/FirewallText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallTextTests
//
//  The words for each of the three causes, and none for an admitted port.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FirewallTextTests)
{
public:

    static std::wstring Describe (FirewallAdmission admission, FirewallProfile profile, const wchar_t * rule = L"")
    {
        FirewallVerdict  verdict;

        verdict.admission = admission;
        verdict.profile   = profile;
        verdict.rule      = rule;

        return FirewallText::DescribeCause (verdict, L"Casso Explorer", { HostSocketKind::Udp, 6510 });
    }




    TEST_METHOD (ABlockRule_IsNamedWithTheNetwork)
    {
        Assert::AreEqual (std::wstring (L"Windows Firewall blocks UDP port 6510 for Casso Explorer on this public network (rule \"cassoexplorer.exe\")."),
                          Describe (FirewallAdmission::Refused, FirewallProfile::Public, L"cassoexplorer.exe"));
    }


    TEST_METHOD (NoRule_SaysTheFirewallBlocksThePort)
    {
        Assert::AreEqual (std::wstring (L"Windows Firewall has no rule allowing UDP port 6510 for Casso Explorer on this public network, so it blocks the port."),
                          Describe (FirewallAdmission::NoRule, FirewallProfile::Public));
    }


    TEST_METHOD (ARuleForOtherNetworks_SaysWhichNetworkThisIs)
    {
        Assert::AreEqual (std::wstring (L"Windows Firewall allows UDP port 6510 for Casso Explorer on other kinds of network, but not on this private one."),
                          Describe (FirewallAdmission::OtherProfile, FirewallProfile::Private));
    }


    TEST_METHOD (AnAdmittedPort_HasNoText)
    {
        Assert::IsTrue (Describe (FirewallAdmission::Admitted, FirewallProfile::Public).empty());
    }
};
