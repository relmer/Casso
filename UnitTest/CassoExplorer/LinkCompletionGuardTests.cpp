#include "Pch.h"

#include "CassoExplorer/Actions/LinkCompletionGuard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkCompletionGuardTests
//
//  A completion wrapped by a guard runs while the guard is alive, and does
//  nothing once the guard has been destroyed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkCompletionGuardTests)
{
public:

    TEST_METHOD (Guard_RunsTheCompletion_WhileTheGuardIsAlive)
    {
        LinkCompletionGuard               guard;
        LinkOperation                     operation;
        int                               calls   = 0;
        CassoLinkConnections::Completion  wrapped = guard.Guard ([&calls] (const LinkOperation &) -> std::wstring { calls++; return L"done"; });

        Assert::AreEqual (std::wstring (L"done"), wrapped (operation));
        Assert::AreEqual (1, calls);
    }




    TEST_METHOD (Guard_DoesNothing_OnceTheGuardIsGone)
    {
        auto                              guard   = std::make_unique<LinkCompletionGuard>();
        LinkOperation                     operation;
        int                               calls   = 0;
        CassoLinkConnections::Completion  wrapped = guard->Guard ([&calls] (const LinkOperation &) -> std::wstring { calls++; return L"done"; });

        guard.reset();

        Assert::AreEqual (std::wstring(), wrapped (operation));
        Assert::AreEqual (0, calls);
    }
};