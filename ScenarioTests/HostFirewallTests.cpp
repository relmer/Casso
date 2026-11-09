#include "Pch.h"

#include "EhmTestHelper.h"
#include "Seams/Win32HostFirewall.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostFirewallScenarioTests
//
//  Win32HostFirewall against this PC's Windows Firewall, read only.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostFirewallScenarioTests)
{
public:

    //  Every Windows install has inbound rules, and a verdict for this
    //  process is one of the four.
    TEST_METHOD (ThisPcsFirewall_IsRead)
    {
        HRESULT            hrCom    = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
        Win32HostFirewall  firewall;
        FirewallSnapshot   snapshot;
        FirewallProgram    program;
        FirewallVerdict    verdict;

        AssertSucceeded (firewall.Read (snapshot));
        AssertSucceeded (Win32HostFirewall::GetThisProgram (program));

        Assert::IsFalse (snapshot.rules.empty());
        Assert::IsFalse (program.path.empty());

        verdict = FirewallCheck::Evaluate (snapshot, program, HostSocketKind::Udp, 6510);
        Logger::WriteMessage (std::format (L"active {}, enabled {}, rules {}, verdict {}\n", (uint32_t) snapshot.active, (uint32_t) snapshot.enabled,
                                           snapshot.rules.size(), (int) verdict.admission).c_str());

        if (SUCCEEDED (hrCom))
        {
            CoUninitialize();
        }
    }
};
