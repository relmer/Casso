#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "Seams/IByteStream.h"
#include "../EmuTests/TestMachine.h"
#include "../NetworkTests/FakeHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineSocketStream
//
//  The PC's end of a TCP connection an emulated W5100 accepted through a
//  FakeHostNetwork: what the test writes is the card's inbound data, and
//  what the card sends is read. A read with nothing waiting runs the machine
//  a frame at a time, its timeout counted in the machine's cycles.
//
////////////////////////////////////////////////////////////////////////////////

class MachineSocketStream : public IByteStream
{
public:
    static constexpr uint64_t  kStepCycles = 17'030;   // a frame

    MachineSocketStream (TestMachine & machine, std::shared_ptr<FakeSocketState> state) : m_machine (machine), m_state (std::move (state)) {}

    HRESULT  Write (const Byte * data, size_t count) override
    {
        m_state->inbound.insert (m_state->inbound.end(), data, data + count);
        return S_OK;
    }

    HRESULT  Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override
    {
        uint64_t  budget = (uint64_t) ((double) timeoutMs * CassoLinkHost::kCpuCyclesPerMs);
        uint64_t  spent  = 0;



        while (m_state->sent.empty() && !IsClosed() && spent < budget)
        {
            m_machine.RunCycles (kStepCycles);
            spent += kStepCycles;
        }

        outRead = std::min (capacity, m_state->sent.size());
        std::copy (m_state->sent.begin(), m_state->sent.begin() + (ptrdiff_t) outRead, buffer);
        m_state->sent.erase (m_state->sent.begin(), m_state->sent.begin() + (ptrdiff_t) outRead);
        return S_OK;
    }

    size_t  GetReadableCount() override          { return m_state->sent.size(); }
    bool    IsClosed         () const override   { return m_state->destroyed || m_state->shutdownSent; }
    void    Close            () override         { m_state->inboundEof = true; }

private:
    TestMachine                       & m_machine;
    std::shared_ptr<FakeSocketState>    m_state;
};
