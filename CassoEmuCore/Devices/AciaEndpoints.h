#pragma once

#include "Pch.h"

#include "IAciaEndpoint.h"
#include "Seams/IByteStream.h"

class Acia6551;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaLoopbackEndpoint
//
//  Feeds every transmitted byte straight back into the ACIA's receiver. Used
//  for comms self-test: each byte the guest sends is received back. The ACIA is caller-owned.
//
////////////////////////////////////////////////////////////////////////////////

class AciaLoopbackEndpoint : public IAciaEndpoint
{
public:
    explicit    AciaLoopbackEndpoint (Acia6551 * acia) : m_acia (acia) {}

    void        OnByteTransmitted (Byte value) override;

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
//  AciaBridgeEndpoint
//
//  Joins the ACIA to a byte stream at the line's own speed, so bytes arrive
//  with the timing of a real serial cable. The speed is read from the
//  ACIA's control register for each character, and the clock is the
//  machine's cycles, so the timing is correct under a debugger's pause and at
//  any emulation speed.
//
//  RECEIVE IS A LINE, NOT A QUEUE. A byte is due one character time after
//  the last; it arrives whether or not the guest read the one before, which
//  then overruns, as on a real //c. A guest that reads each byte in time receives every byte.
//
//  Transmit is a shift register behind the transmit data register: a byte
//  written while one shifts remains in the register with TDRE clear, and is
//  sent when the first one finishes.
//
//  The stream is shared, so a test or the link can own its other end. The
//  ACIA is caller-owned.
//
////////////////////////////////////////////////////////////////////////////////

class AciaBridgeEndpoint : public IAciaEndpoint
{
public:
    using ByteLogFn = std::function<void (bool fromGuest, Byte value)>;

    //  An NTSC Apple II's CPU clock.
    static constexpr uint32_t  kCpuHz = 1020484;

    AciaBridgeEndpoint (Acia6551 * acia, std::shared_ptr<IByteStream> stream) : m_acia (acia), m_stream (std::move (stream)) {}

    void        OnByteTransmitted (Byte value) override;
    bool        PacesTransmit     () const override { return true; }

    //  Advance the line by the cycles the CPU has run.
    void        Tick              (uint32_t cpuCycles);

    //  Every byte that crosses the line, in either direction, for captures.
    void        SetByteLog        (ByteLogFn log)   { m_log = std::move (log); }

    IByteStream *  GetStream      () const          { return m_stream.get(); }

    //  The 6551's rate for its control register: the low four bits select a
    //  rate from the //c's 1.8432 MHz crystal, and zero is its 16x external
    //  clock, 115,200 baud.
    static uint32_t  GetBaudRate           (Byte control);

    //  CPU cycles to send one character: a start bit, the data bits, a parity
    //  bit when the command register enables one, and the stop bits.
    static uint32_t  GetCyclesPerCharacter (Byte control, Byte command);

private:
    void        DeliverReceived ();
    void        FinishShifting  ();
    uint32_t    GetCharacterCycles () const;

    static constexpr uint32_t  s_kBaudRates[16] =
    {
        115200, 50, 75, 110, 135, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600, 19200,
    };

    Acia6551                     * m_acia      = nullptr;
    std::shared_ptr<IByteStream>   m_stream;
    ByteLogFn                      m_log;
    int64_t                        m_rxWait    = 0;
    int64_t                        m_txWait    = 0;
    bool                           m_shifting  = false;
    Byte                           m_shiftByte = 0;
    bool                           m_tdrFull   = false;
    Byte                           m_tdr       = 0;
};
