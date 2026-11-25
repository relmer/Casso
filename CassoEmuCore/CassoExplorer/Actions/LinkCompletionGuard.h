#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkCompletionGuard
//
//  Completions that do nothing once the guard is gone. Jobs are in the
//  shell's connections, which every window shares, and can finish after the
//  window that queued them, and the guard's owner, have closed.
//
////////////////////////////////////////////////////////////////////////////////

class LinkCompletionGuard
{
public:
    CassoLinkConnections::Completion  Guard (CassoLinkConnections::Completion inner) const;

private:
    std::shared_ptr<int>  m_alive = std::make_shared<int> (0);
};