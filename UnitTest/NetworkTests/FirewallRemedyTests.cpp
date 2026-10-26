#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Net/FirewallRemedy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedyTests
//
//  The elevated helper's plan: which ports and profiles it admits, its
//  command-line form, and which rules it changes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FirewallRemedyTests)
{
public:

    static constexpr const wchar_t *  kProgram = L"C:\\Casso\\CassoExplorer.exe";

    static FirewallProgram MakeProgram()
    {
        FirewallProgram  program;

        program.path = kProgram;

        return program;
    }


    //  The rule the prompt writes when the user cancels it: the whole
    //  program, every port, for one protocol.
    static FirewallRule MakeCanceledPromptRule (HostSocketKind protocol)
    {
        FirewallRule  rule;

        rule.name        = L"cassoexplorer.exe";
        rule.application = kProgram;
        rule.anyProtocol = false;
        rule.protocol    = protocol;
        rule.localPorts  = L"*";
        rule.profiles    = FirewallProfile::Private | FirewallProfile::Public;
        rule.allow       = false;

        return rule;
    }


    static FirewallPlan MakePlan (const FirewallSnapshot & snapshot, bool markPrivate)
    {
        return FirewallRemedy::MakePlan (snapshot, MakeProgram(), L"Casso Explorer",
                                         { { HostSocketKind::Udp, 6510 }, { HostSocketKind::Tcp, 6400 } }, markPrivate);
    }




    TEST_METHOD (APortRefusedOnPublic_IsAdmittedOnPublic)
    {
        FirewallSnapshot  snapshot;
        FirewallPlan      plan;

        snapshot.active = FirewallProfile::Public;
        snapshot.rules  = { MakeCanceledPromptRule (HostSocketKind::Udp) };

        plan = MakePlan (snapshot, false);

        Assert::AreEqual ((uint32_t) FirewallProfile::Public, (uint32_t) plan.profiles);
        Assert::AreEqual ((size_t) 2, plan.ports.size(), L"the TCP port has no rule at all");
        Assert::IsFalse  (plan.markPrivate);
    }


    TEST_METHOD (MarkingPrivate_AdmitsOnPrivateInstead)
    {
        FirewallSnapshot  snapshot;
        FirewallPlan      plan;

        snapshot.active = FirewallProfile::Public;

        plan = MakePlan (snapshot, true);

        Assert::AreEqual ((uint32_t) FirewallProfile::Private, (uint32_t) plan.profiles);
        Assert::IsTrue (plan.markPrivate);
    }


    //  With no public network, there is nothing to mark private.
    TEST_METHOD (MarkingPrivate_OnAPrivateNetwork_MarksNothing)
    {
        FirewallSnapshot  snapshot;

        snapshot.active = FirewallProfile::Private;

        Assert::IsFalse (MakePlan (snapshot, true).markPrivate);
    }


    TEST_METHOD (AnAdmittedPort_IsLeftOut)
    {
        FirewallSnapshot  snapshot;
        FirewallRule      allow;
        FirewallPlan      plan;

        allow.application = kProgram;
        allow.anyProtocol = false;
        allow.protocol    = HostSocketKind::Tcp;
        allow.localPorts  = L"6400";

        snapshot.active = FirewallProfile::Public;
        snapshot.rules  = { allow };

        plan = MakePlan (snapshot, false);

        Assert::AreEqual ((size_t) 1, plan.ports.size());
        Assert::IsTrue   (plan.ports[0] == FirewallPort { HostSocketKind::Udp, 6510 });
    }


    TEST_METHOD (TheCanceledPromptsRule_IsBlockingForItsProtocolOnly)
    {
        FirewallSnapshot  snapshot;
        FirewallPlan      plan;

        snapshot.active = FirewallProfile::Public;
        plan            = MakePlan (snapshot, false);

        Assert::IsTrue  (FirewallRemedy::IsBlocking (MakeCanceledPromptRule (HostSocketKind::Udp), plan, { HostSocketKind::Udp, 6510 }));
        Assert::IsFalse (FirewallRemedy::IsBlocking (MakeCanceledPromptRule (HostSocketKind::Tcp), plan, { HostSocketKind::Udp, 6510 }));
    }


    TEST_METHOD (TheRuleNamed_ForAPort_IsThePlansRule)
    {
        FirewallSnapshot  snapshot;
        FirewallPlan      plan;
        FirewallRule      rule;

        snapshot.active = FirewallProfile::Public;
        plan            = MakePlan (snapshot, false);

        rule.name        = FirewallRemedy::MakeRuleName (plan, { HostSocketKind::Udp, 6510 });
        rule.application = L"c:\\casso\\cassoexplorer.exe";

        Assert::AreEqual (std::wstring (L"Casso Explorer, UDP 6510"), rule.name);
        Assert::IsTrue   (FirewallRemedy::IsPlanRule (rule, plan, { HostSocketKind::Udp, 6510 }));
        Assert::IsFalse  (FirewallRemedy::IsPlanRule (rule, plan, { HostSocketKind::Tcp, 6400 }));
    }


    TEST_METHOD (APlan_RoundTripsThroughItsArguments)
    {
        FirewallSnapshot  snapshot;
        FirewallPlan      plan;
        FirewallPlan      parsed;

        snapshot.active = FirewallProfile::Public;
        plan            = MakePlan (snapshot, true);

        AssertSucceeded (FirewallRemedy::ParseArguments (FirewallRemedy::FormatArguments (plan), parsed));
        Assert::IsTrue  (parsed == plan);
    }


    //  The helper runs elevated, so nothing it cannot read is run.
    TEST_METHOD (ArgumentsThatAreNotAPlan_FailAsData)
    {
        FirewallPlan  plan;

        for (const wchar_t * text : { L"", L"2|4|0|Casso|C:\\Casso.exe|U6510", L"1|8|0|Casso|C:\\Casso.exe|U6510", L"1|4|0|Casso|C:\\Casso.exe|X6510",
                                      L"1|4|0|Casso|C:\\Casso.exe|U0", L"1|4|0|Casso|C:\\Casso.exe|U70000", L"1|4|0||C:\\Casso.exe|U6510" })
        {
            Assert::IsTrue (FirewallRemedy::ParseArguments (text, plan) == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), text);
        }
    }
};
