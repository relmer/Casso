#include "Pch.h"

#include "Devices/Printer/PrinterJob.h"
#include "Devices/Printer/PrinterByteRing.h"




static constexpr uint32_t   s_kDrainChunk = 4096;





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterJob::Reset()
{
    m_interpreter.Reset();
    m_raster.Clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Drain
//
//  Empties the ring in chunks, feeding each through the interpreter. Runs the
//  byte observer (if any) first so a diagnostics/capture tap sees the exact
//  bytes the interpreter will act on.
//
////////////////////////////////////////////////////////////////////////////////

size_t PrinterJob::Drain (vector<PrinterEvent> & outEvents)
{
    return Drain (outEvents, (std::numeric_limits<size_t>::max) ());
}


uint32_t PrinterJob::Pending() const
{
    return m_ring.ApproxSize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Drain
//
////////////////////////////////////////////////////////////////////////////////

size_t PrinterJob::Drain (vector<PrinterEvent> & outEvents, size_t maxBytes)
{
    Byte     buffer[s_kDrainChunk];
    size_t   total = 0;

    while (total < maxBytes)
    {
        uint32_t   want = (uint32_t) (std::min) (maxBytes - total, (size_t) s_kDrainChunk);
        uint32_t   got  = m_ring.Drain (buffer, want);

        if (got == 0)
        {
            break;
        }

        if (m_observer)
        {
            m_observer (buffer, got);
        }

        m_interpreter.Consume (buffer, got, m_raster, outEvents);
        total += got;
    }

    return total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormFeed
//
//  Feeds a single $0C through the interpreter, exactly as if the guest had
//  sent it -- the paper advances to the top of the next page through the
//  same code path, so page boundaries and events stay consistent.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterJob::FormFeed (vector<PrinterEvent> & outEvents)
{
    Byte   ff = 0x0C;



    m_interpreter.Consume (&ff, 1, m_raster, outEvents);
}
