#include "Pch.h"

#include "FakeHostNameLookup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNameLookup::TryGetResult
//
////////////////////////////////////////////////////////////////////////////////

bool FakeHostNameLookup::TryGetResult (
    HRESULT   & outHr,
    uint32_t  & outIpv4)
{
    if (m_state->isDone)
    {
        outHr   = m_state->hr;
        outIpv4 = m_state->ipv4;
    }

    return m_state->isDone;
}
