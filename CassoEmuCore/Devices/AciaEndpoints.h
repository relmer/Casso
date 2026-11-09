#pragma once

#include "Pch.h"

#include "IAciaEndpoint.h"
#include "Seams/IByteStream.h"

class Acia6551;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaLoopbackEndpoint
//
//  A loopback plug: every transmitted byte is queued back to the ACIA's
//  receiver, RTS is wired to CTS, and DTR to DSR and DCD. Used for comms
//  self-test: the guest reads what it sends. The ACIA is caller-owned.
//
////////////////////////////////////////////////////////////////////////////////

class AciaLoopbackEndpoint : public IAciaEndpoint
{
public:
    explicit    AciaLoopbackEndpoint (Acia6551 * acia) : m_acia (acia) {}

    void        OnByteTransmitted     (Byte value) override;
    void        OnModemControlChanged (bool dtr, bool rts) override;

private:
    Acia6551 *    m_acia = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AciaFileEndpoint
//
//  Appends every transmitted byte to a host file — the v1 serial-printing sink
//  (spec 015 routes guest print output here). Transmit-only.
//
////////////////////////////////////////////////////////////////////////////////

class AciaFileEndpoint : public IAciaEndpoint
{
public:
    HRESULT     Open   (const string & path);
    void        Close  ();
    bool        IsOpen () const { return m_file.is_open (); }

    void        OnByteTransmitted (Byte value) override;

private:
    ofstream    m_file;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AciaPrinterEndpoint
//
//  The ImageWriter II on a serial cable. Every transmitted byte goes into the
//  ring the printer drains. Its handshake line drops when the ring nears
//  full and rises again once the drain has made room; the line is wired to
//  CTS, DSR and DCD together, since cables and drivers differ in which input
//  they watch, and CTS false stops the transmitter on every one of them.
//
//  The ring and the ACIA are caller-owned.
//
////////////////////////////////////////////////////////////////////////////////

class AciaPrinterEndpoint : public IAciaEndpoint
{
public:
    //  Free bytes at or below which the line drops, and at or above which it
    //  rises again.
    static constexpr uint32_t  kBusyFreeBytes  = 1024;
    static constexpr uint32_t  kReadyFreeBytes = 4096;

    AciaPrinterEndpoint (Acia6551 * acia, class PrinterByteRing * ring) : m_acia (acia), m_ring (ring) {}

    void        OnByteTransmitted (Byte value) override;

    //  Advance by the cycles the CPU has run, checking the ring now and then
    //  to raise the line once there is room.
    void        Tick              (uint32_t cpuCycles);

    bool        IsReady           () const { return m_ready; }

private:
    //  Cycles between checks of a busy ring: about a millisecond.
    static constexpr int64_t  kCheckCycles = 1000;

    void        Update            ();

    Acia6551                * m_acia      = nullptr;
    class PrinterByteRing   * m_ring      = nullptr;
    bool                      m_ready     = true;
    int64_t                   m_checkWait = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpoint
//
//  Joins the ACIA to a byte stream. The ACIA paces the line: it passes each
//  byte here as its shift ends, and lands the bytes queued from the stream
//  one character time apart, at the speed in its control register, so bytes
//  arrive with the timing of a real serial cable. The clock is the machine's
//  cycles, so the timing is correct under a debugger's pause and at any
//  emulation speed.
//
//  RECEIVE IS A LINE, NOT A QUEUE the guest can see. A byte lands one
//  character time after the last, whether or not the guest read the one
//  before, which then overruns, as on a real //c.
//
//  The stream is shared, so a test or the link can own its other end. The
//  ACIA is caller-owned.
//
////////////////////////////////////////////////////////////////////////////////

class AciaBridgeEndpoint : public IAciaEndpoint
{
public:
    using ByteLogFn = std::function<void (bool fromGuest, Byte value, uint64_t cycle)>;

    AciaBridgeEndpoint (Acia6551 * acia, std::shared_ptr<IByteStream> stream) : m_acia (acia), m_stream (std::move (stream)) {}

    void        OnByteTransmitted (Byte value) override;

    //  Advance the line by the cycles the CPU has run, moving what the
    //  stream received to the ACIA once per character time.
    void        Tick              (uint32_t cpuCycles);

    //  Every byte that crosses the line, in either direction, with the count
    //  of cycles the line has run when it crossed, for captures: a sent byte
    //  as its shift ends, a received one as it is queued for the ACIA.
    void        SetByteLog        (ByteLogFn log)   { m_log = std::move (log); }

    IByteStream *  GetStream      () const          { return m_stream.get(); }

private:
    //  Bytes queued in the ACIA at most: enough that its queue never empties
    //  between two checks of the stream.
    static constexpr size_t  kQueueCap = 16;

    size_t      GetQueueRoom () const;

    Acia6551                     * m_acia     = nullptr;
    std::shared_ptr<IByteStream>   m_stream;
    ByteLogFn                      m_log;
    uint64_t                       m_cycle    = 0;
    int64_t                        m_pollWait = 0;   // cycles until the stream is next checked
};
