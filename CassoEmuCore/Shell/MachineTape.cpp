#include "Pch.h"

#include "Shell/MachineTape.h"
#include "Shell/MachineParts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineTape::MachineTape
//
////////////////////////////////////////////////////////////////////////////////

MachineTape::MachineTape (MachineParts & parts) :
    m_parts       (parts),
    m_deck        (std::make_unique<TapeDeck>()),
    m_diagnostics (*m_deck, parts.refs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineTape::Stop
//
//  A reset stops the recorder: after a reset mid-load, a real one would run
//  on into a guest that no longer reads it.
//
////////////////////////////////////////////////////////////////////////////////

void MachineTape::Stop()
{
    uint64_t  now = (m_parts.cpu != nullptr) ? *m_parts.cpu->GetBusCyclePtr() : 0;



    m_deck->Stop (now);
    m_resetCount.fetch_add (1, std::memory_order_release);
}
