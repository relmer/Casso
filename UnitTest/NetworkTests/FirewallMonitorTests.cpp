#include "Pch.h"

#include "Net/FirewallMonitor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallMonitorTests
//
//  When the monitor reads the firewall again, over a fake firewall and a
//  fake count of network changes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FirewallMonitorTests)
{
public:

    class FakeFirewall : public IHostFirewall
    {
    public:
        HRESULT  Read (FirewallSnapshot & outSnapshot) override
        {
            reads++;
            outSnapshot        = snapshot;
            outSnapshot.active = active;
            return result;
        }

        HRESULT  ReadActiveProfiles (FirewallProfile & outActive) override
        {
            outActive = active;
            return S_OK;
        }

        FirewallSnapshot  snapshot;
        FirewallProfile   active = FirewallProfile::Public;
        HRESULT           result = S_OK;
        int               reads  = 0;
    };


    class FakeChanges : public INetworkChanges
    {
    public:
        uint32_t  GetChangeCount() const override { return count; }

        uint32_t  count = 0;
    };




    TEST_METHOD (TheFirstUpdate_ReadsAndLaterOnesDoNot)
    {
        FakeFirewall     firewall;
        FakeChanges      changes;
        FirewallMonitor  monitor (firewall, &changes, FirewallProgram());

        Assert::IsTrue   (monitor.Update());
        Assert::IsFalse  (monitor.Update());
        Assert::AreEqual (1, firewall.reads);
    }


    TEST_METHOD (ANetworkChange_ReadsAgain)
    {
        FakeFirewall     firewall;
        FakeChanges      changes;
        FirewallMonitor  monitor (firewall, &changes, FirewallProgram());

        monitor.Update();
        changes.count++;

        Assert::IsTrue   (monitor.Update());
        Assert::AreEqual (2, firewall.reads);
    }


    //  Marking a network private changes no address, only the profile.
    TEST_METHOD (ACategoryChange_ReadsAgain)
    {
        FakeFirewall     firewall;
        FakeChanges      changes;
        FirewallMonitor  monitor (firewall, &changes, FirewallProgram());

        monitor.Update();
        firewall.active = FirewallProfile::Private;

        Assert::IsTrue (monitor.Update());
        Assert::IsTrue (monitor.GetSnapshot().active == FirewallProfile::Private);
    }


    TEST_METHOD (Invalidate_ReadsAgain)
    {
        FakeFirewall     firewall;
        FirewallMonitor  monitor (firewall, nullptr, FirewallProgram());
        uint32_t         version = 0;

        monitor.Update();
        version = monitor.GetVersion();
        monitor.Invalidate();

        Assert::IsTrue      (monitor.Update());
        Assert::AreNotEqual (version, monitor.GetVersion());
    }


    //  A failed read reports every port admitted, so no warning is drawn.
    TEST_METHOD (AFailedRead_AdmitsEverything_AndIsNotRetriedEveryPoll)
    {
        FakeFirewall     firewall;
        FirewallMonitor  monitor (firewall, nullptr, FirewallProgram());

        firewall.result = E_ACCESSDENIED;

        Assert::IsTrue   (monitor.Update());
        Assert::IsTrue   (monitor.Check (HostSocketKind::Udp, 6510).IsAdmitted());
        Assert::IsFalse  (monitor.Update());
        Assert::AreEqual (1, firewall.reads);
    }


    TEST_METHOD (TheVerdict_IsFromTheSnapshotRead)
    {
        FakeFirewall     firewall;
        FirewallMonitor  monitor (firewall, nullptr, FirewallProgram());

        Assert::IsTrue (monitor.Check (HostSocketKind::Udp, 6510).IsAdmitted(), L"nothing is known before the first read");

        monitor.Update();
        Assert::IsTrue (monitor.Check (HostSocketKind::Udp, 6510).admission == FirewallAdmission::NoRule);
    }
};
