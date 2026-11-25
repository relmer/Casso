#include "Pch.h"

#include "CassoExplorer/Actions/LinkCompletionGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkCompletionGuard::Guard
//
//  Wraps a completion. If this guard is gone when the job finishes, the
//  wrapper runs nothing and returns an empty string.
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkConnections::Completion LinkCompletionGuard::Guard (CassoLinkConnections::Completion inner) const
{
    return [alive = std::weak_ptr<int> (m_alive), inner = std::move (inner)] (const LinkOperation & operation) -> std::wstring
    {
        return alive.expired() ? std::wstring() : inner (operation);
    };
}