#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "Seams/IByteStream.h"
#include "Seams/InProcessByteStream.h"
#include "../EmuTests/TestMachine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SteppedByteStream
//
//  The PC's end of an in-process pair, for a CassoLinkHost on the test's
//  thread. A Read with nothing waiting runs the machine a frame at a time
//  until a byte arrives or the timeout passes, with the timeout counted in
//  the machine's cycles at the //c's clock. The host and the machine share
//  one thread and one clock, so the test is deterministic at any speed.
//
////////////////////////////////////////////////////////////////////////////////

class SteppedByteStream : public IByteStream
{
public:
    static constexpr uint64_t  kStepCycles = 17'030;   // a frame

    SteppedByteStream (TestMachine & machine, std::shared_ptr<InProcessByteStream> end) : m_machine (machine), m_end (std::move (end)) {}

    HRESULT  Write            (const Byte * data, size_t count) override { return m_end->Write (data, count); }
    size_t   GetReadableCount () override                               { return m_end->GetReadableCount(); }
    bool     IsClosed         () const override                         { return m_end->IsClosed(); }
    void     Close            () override                               { m_end->Close(); }

    //  The line closed once the machine has run `cycles` through this
    //  stream, as when the PC's end goes away at that point.
    void      CloseAfter (uint64_t cycles) { m_closeAfter = cycles; }

    //  The cycles the machine has run through this stream.
    uint64_t  GetCycles  () const          { return m_total; }

    //  A Read of zero runs one step, so the machine advances while a caller
    //  polls.
    HRESULT  Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override
    {
        uint64_t  budget = (uint64_t) ((double) timeoutMs * CassoLinkHost::kCpuCyclesPerMs);
        uint64_t  spent  = 0;



        do
        {
            if (m_end->GetReadableCount() > 0 || m_end->IsClosed())
            {
                break;
            }

            m_machine.RunCycles (kStepCycles);
            spent  += kStepCycles;
            m_total += kStepCycles;

            if (m_total >= m_closeAfter)
            {
                m_end->Close();
            }
        }
        while (spent < budget);

        return m_end->Read (buffer, capacity, 0, outRead);
    }

private:
    TestMachine                           & m_machine;
    std::shared_ptr<InProcessByteStream>    m_end;
    uint64_t                                m_total      = 0;
    uint64_t                                m_closeAfter = UINT64_MAX;
};
