#include "Pch.h"

#include "Devices/Printer/PrinterJob.h"
#include "Devices/Printer/PrinterByteRing.h"




static constexpr uint32_t   s_kDrainChunk = 4096;




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterJob::Reset ()
{
    m_interpreter.Reset ();
    m_raster.Clear ();
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
    Byte     buffer[s_kDrainChunk];
    size_t   total = 0;

    for (;;)
    {
        uint32_t   got = m_ring.Drain (buffer, s_kDrainChunk);

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
