#include "Pch.h"

#include "Net/FirewallCheck.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallCheckTests
//
//  The four admissions from snapshots written here, in the form
//  Win32HostFirewall reads from Windows Firewall.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FirewallCheckTests)
{
public:

    static constexpr uint16_t  kPort = 6510;

    static FirewallProgram MakeProgram()
    {
        FirewallProgram  program;

        program.path = L"C:\\Casso\\CassoExplorer.exe";

        return program;
    }


    static FirewallRule MakeRule (bool allow, FirewallProfile profiles)
    {
        FirewallRule  rule;

        rule.name        = allow ? L"Allow" : L"Block";
        rule.application = L"c:\\casso\\cassoexplorer.exe";
        rule.anyProtocol = false;
        rule.protocol    = HostSocketKind::Udp;
        rule.localPorts  = L"6510";
        rule.profiles    = profiles;
        rule.allow       = allow;

        return rule;
    }


    static FirewallSnapshot MakeSnapshot (FirewallProfile active, std::vector<FirewallRule> rules)
    {
        FirewallSnapshot  snapshot;

        snapshot.active = active;
        snapshot.rules  = std::move (rules);

        return snapshot;
    }


    static FirewallVerdict Check (const FirewallSnapshot & snapshot)
    {
        return FirewallCheck::Evaluate (snapshot, MakeProgram(), HostSocketKind::Udp, kPort);
    }




    //  The path is compared without regard to case, as Windows compares it.
    TEST_METHOD (AnAllowRule_OnThePresentProfile_Admits)
    {
        FirewallVerdict  verdict = Check (MakeSnapshot (FirewallProfile::Public, { MakeRule (true, FirewallProfile::All) }));

        Assert::IsTrue (verdict.IsAdmitted());
    }


    TEST_METHOD (ABlockRule_WinsOverAnAllowRule)
    {
        FirewallRule     block   = MakeRule (false, FirewallProfile::Public);
        FirewallVerdict  verdict;

        block.anyProtocol = true;
        block.localPorts.clear();

        verdict = Check (MakeSnapshot (FirewallProfile::Public, { MakeRule (true, FirewallProfile::All), block }));

        Assert::IsTrue   (verdict.admission == FirewallAdmission::Refused);
        Assert::AreEqual (std::wstring (L"Block"), verdict.rule);
        Assert::IsTrue   (verdict.profile == FirewallProfile::Public);
    }


    TEST_METHOD (NoRule_IsNoRule)
    {
        FirewallVerdict  verdict = Check (MakeSnapshot (FirewallProfile::Public, {}));

        Assert::IsTrue (verdict.admission == FirewallAdmission::NoRule);
        Assert::IsTrue (verdict.profile == FirewallProfile::Public);
    }


    TEST_METHOD (AnAllowRule_OnAnotherProfileOnly_IsOtherProfile)
    {
        FirewallVerdict  verdict = Check (MakeSnapshot (FirewallProfile::Public, { MakeRule (true, FirewallProfile::Private) }));

        Assert::IsTrue (verdict.admission == FirewallAdmission::OtherProfile);
    }


    //  On two networks at once, the worse of the two is the verdict.
    TEST_METHOD (TwoProfiles_GiveTheWorseVerdict)
    {
        FirewallVerdict  verdict = Check (MakeSnapshot (FirewallProfile::Private | FirewallProfile::Public, { MakeRule (true, FirewallProfile::Private) }));

        Assert::IsTrue (verdict.admission == FirewallAdmission::OtherProfile);
        Assert::IsTrue (verdict.profile == FirewallProfile::Public);
    }


    //  Blocked on both, the public network is the one reported, since both
    //  remedies apply there.
    TEST_METHOD (TwoProfilesBlocked_ReportThePublicOne)
    {
        FirewallVerdict  verdict = Check (MakeSnapshot (FirewallProfile::Private | FirewallProfile::Public, { MakeRule (false, FirewallProfile::All) }));

        Assert::IsTrue (verdict.admission == FirewallAdmission::Refused);
        Assert::IsTrue (verdict.profile == FirewallProfile::Public);
    }


    TEST_METHOD (ABlockRule_ForAnotherProgramOrPort_DoesNotApply)
    {
        FirewallRule  otherProgram = MakeRule (false, FirewallProfile::All);
        FirewallRule  otherPort    = MakeRule (false, FirewallProfile::All);
        FirewallRule  otherKind    = MakeRule (false, FirewallProfile::All);

        otherProgram.application = L"C:\\Other\\Other.exe";
        otherPort.localPorts     = L"6511";
        otherKind.protocol       = HostSocketKind::Tcp;

        Assert::IsTrue (Check (MakeSnapshot (FirewallProfile::Public, { MakeRule (true, FirewallProfile::All), otherProgram, otherPort, otherKind })).IsAdmitted());
    }


    TEST_METHOD (ADisabledRule_DoesNotApply)
    {
        FirewallRule  block = MakeRule (false, FirewallProfile::All);

        block.enabled = false;

        Assert::IsTrue (Check (MakeSnapshot (FirewallProfile::Public, { MakeRule (true, FirewallProfile::All), block })).IsAdmitted());
    }


    TEST_METHOD (AFirewallThatIsOff_AdmitsEverything)
    {
        FirewallSnapshot  snapshot = MakeSnapshot (FirewallProfile::Public, {});

        snapshot.enabled = FirewallProfile::Domain | FirewallProfile::Private;

        Assert::IsTrue (Check (snapshot).IsAdmitted());
    }


    //  A package's rules have its SID and no path.
    TEST_METHOD (APackageRule_AppliesToItsPackageOnly)
    {
        FirewallRule     rule    = MakeRule (true, FirewallProfile::All);
        FirewallProgram  program = MakeProgram();

        rule.application.clear();
        rule.package    = L"S-1-15-2-1";
        program.package = L"S-1-15-2-1";

        Assert::IsTrue (FirewallCheck::Evaluate (MakeSnapshot (FirewallProfile::Public, { rule }), program, HostSocketKind::Udp, kPort).IsAdmitted());

        program.package = L"S-1-15-2-2";
        Assert::IsTrue (FirewallCheck::Evaluate (MakeSnapshot (FirewallProfile::Public, { rule }), program, HostSocketKind::Udp, kPort).admission == FirewallAdmission::NoRule);
    }


    //  Store apps' rules have an owner and no path, and apply to app
    //  containers only.
    TEST_METHOD (AStoreAppsRule_AppliesToNoCassoProgram)
    {
        FirewallRule  rule = MakeRule (true, FirewallProfile::All);

        rule.application.clear();
        rule.owner       = L"S-1-5-21-1-2-3-1002";
        rule.anyProtocol = true;
        rule.localPorts.clear();

        Assert::IsTrue (Check (MakeSnapshot (FirewallProfile::Public, { rule })).admission == FirewallAdmission::NoRule);
    }


    TEST_METHOD (APortList_HasPortsRangesAndEvery)
    {
        Assert::IsTrue  (FirewallCheck::MatchesPort (L"",                  6510));
        Assert::IsTrue  (FirewallCheck::MatchesPort (L"*",                 6510));
        Assert::IsTrue  (FirewallCheck::MatchesPort (L"6510",              6510));
        Assert::IsTrue  (FirewallCheck::MatchesPort (L"80,6500-6520,6600", 6510));
        Assert::IsFalse (FirewallCheck::MatchesPort (L"80,6500-6509,6600", 6510));
        Assert::IsFalse (FirewallCheck::MatchesPort (L"RPC",               6510));
        Assert::IsFalse (FirewallCheck::MatchesPort (L"6510-",             6510));
    }
};
