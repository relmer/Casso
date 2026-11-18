#pragma once

#include "Pch.h"

#include "Devices/Tape/TapeDeck.h"
#include "Machines/Apple2/Common/CassetteDiagnostics.h"

struct MachineParts;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineTape
//
//  The recorder plugged into the cassette jacks, and its panel. The deck is
//  the owner's, not the machine's, so it outlives every rebuild; each new
//  cassette port is connected to it.
//
////////////////////////////////////////////////////////////////////////////////

class MachineTape
{
public:
    explicit MachineTape (MachineParts & parts);

    MachineTape (const MachineTape &)             = delete;
    MachineTape & operator= (const MachineTape &) = delete;

    TapeDeck        & GetDeck()       noexcept { return *m_deck; }
    const TapeDeck  & GetDeck() const noexcept { return *m_deck; }

    //  Stops the recorder at the current bus cycle; the tape stays inserted.
    //  A reset, a power cycle and a machine switch each do this.
    void  Stop();

    //  How many times a reset, power cycle or rebuild has stopped the tape,
    //  so the UI can detect one and release the recorder's keys.
    uint32_t  GetResetCount() const { return m_resetCount.load (std::memory_order_acquire); }

    const CassetteDiagnostics  & GetDiagnostics() const noexcept { return m_diagnostics; }

private:
    MachineParts               & m_parts;
    std::unique_ptr<TapeDeck>    m_deck;
    CassetteDiagnostics          m_diagnostics;
    std::atomic<uint32_t>        m_resetCount { 0 };
};
