#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeLookupState
//
//  One fake lookup's result, shared between the IHostNameLookup the code
//  under test polls and the FakeHostNetwork that completes it.
//
////////////////////////////////////////////////////////////////////////////////

struct FakeLookupState
{
    std::string  host;
    bool         isDone  = false;
    HRESULT      hr      = S_OK;
    uint32_t     ipv4    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNameLookup
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostNameLookup : public IHostNameLookup
{
public:

    explicit FakeHostNameLookup (std::shared_ptr<FakeLookupState> state) : m_state (std::move (state)) {}

    bool  TryGetResult (HRESULT & outHr, uint32_t & outIpv4) override;

private:

    std::shared_ptr<FakeLookupState>  m_state;
};
