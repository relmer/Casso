#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeScc8530
//
//  A IIgs's 8530 and its speed register, at $C036 to $C03B: the speed
//  register at $C036, channel B's command and data registers at $C038 and
//  $C03A, and channel A's at $C039 and $C03B. Channel A is the printer port,
//  port 1, and channel B the modem port, port 2.
//
//  A write to a command register with no register selected selects the
//  register in its low three bits, plus 8 for the point high command;
//  the next access writes or reads that register, and register 0 is selected
//  again. Register 0 has a byte received in bit 0 and the transmit buffer
//  empty in bit 2, with DCD and CTS on. Register 1 has all sent in bit 0,
//  since each byte is sent as it is written.
//
//  The 8530's clock is a 3.6864 MHz crystal at RTxC, so a channel has a rate
//  only with register 11's crystal bit set. From the baud rate generator,
//  with register 14's enable bit set and RTxC as its source, the rate is the
//  clock divided by twice the time constant plus 2; straight from RTxC, it
//  is the clock. Either is then divided by register 4's clock mode.
//
//  The line has a rate, SetLineBaud: a byte is sent or received only when
//  the channel's rate is the line's, its transmitter or receiver is on, and
//  it has 8 bits. With kAnyBaud, the line follows the channel, which needs
//  a clock. A byte sent or received otherwise is counted as garbled.
//  A channel attached to a byte stream sends to it and receives from it.
//
////////////////////////////////////////////////////////////////////////////////

class FakeScc8530 : public MemoryDevice
{
public:
    static constexpr Word      kSpeedRegister = 0xC036;
    static constexpr Word      kBCommand      = 0xC038;
    static constexpr Word      kACommand      = 0xC039;
    static constexpr Word      kBData         = 0xC03A;
    static constexpr Word      kAData         = 0xC03B;
    static constexpr uint32_t  kClock         = 3'686'400;
    static constexpr int       kA             = 0;
    static constexpr int       kB             = 1;
    static constexpr uint32_t  kAnyBaud       = 0;

    Word  GetStart () const override { return kSpeedRegister; }
    Word  GetEnd   () const override { return kAData; }
    void  Reset    () override       {}

    Byte  Read (Word address) override
    {
        Byte  value = 0;



        if (address == kSpeedRegister)
        {
            value = m_speed;
        }
        else if (address == kACommand || address == kBCommand)
        {
            value = ReadCommand (address == kACommand ? kA : kB);
        }
        else if (address == kAData || address == kBData)
        {
            value = ReadData (address == kAData ? kA : kB);
        }

        return value;
    }

    void  Write (Word address, Byte value) override
    {
        if (address == kSpeedRegister)
        {
            m_speed = value;
            m_speedWrites.push_back (value);
        }
        else if (address == kACommand || address == kBCommand)
        {
            WriteCommand (address == kACommand ? kA : kB, value);
        }
        else if (address == kAData || address == kBData)
        {
            WriteData (address == kAData ? kA : kB, value);
        }
    }

    void  SetLineBaud (uint32_t baud)                                   { m_lineBaud = baud; }
    void  SetSpeed    (Byte value)                                      { m_speed = value; }
    void  Attach      (int channel, std::shared_ptr<IByteStream> stream) { m_channels[channel].stream = std::move (stream); }

    //  The bytes for the channel to receive, at the line's rate.
    void  Receive (int channel, std::initializer_list<Byte> bytes)
    {
        m_channels[channel].arriving.insert (m_channels[channel].arriving.end(), bytes.begin(), bytes.end());
    }

    const std::vector<Byte> &  GetSent        (int channel) const       { return m_channels[channel].sent; }
    int                        GetGarbled     (int channel) const       { return m_channels[channel].garbled; }
    Byte                       GetRegister    (int channel, int r) const { return m_channels[channel].wr[r]; }
    Byte                       GetSpeed       () const                  { return m_speed; }
    const std::vector<Byte> &  GetSpeedWrites () const                  { return m_speedWrites; }

    //  The channel's rate, from its registers; 0 with no clock.
    uint32_t  GetBaud (int channel) const
    {
        static constexpr uint32_t  kModeDivider[] = { 1, 16, 32, 64 };

        const Channel &  c       = m_channels[channel];
        bool             crystal = (c.wr[11] & 0x80) != 0;
        int              txClock = (c.wr[11] >> 3) & 0x03;
        uint32_t         divider = kModeDivider[(c.wr[4] >> 6) & 0x03];
        uint32_t         tc      = (uint32_t) c.wr[12] | ((uint32_t) c.wr[13] << 8);
        uint32_t         baud    = 0;



        if (crystal && txClock == 0)
        {
            baud = kClock / divider;
        }
        else if (crystal && txClock == 2 && (c.wr[14] & 0x03) == 0x01)
        {
            baud = kClock / (2 * (tc + 2)) / divider;
        }

        return baud;
    }

private:
    struct Channel
    {
        std::array<Byte, 16>          wr       = {};
        int                           selected = 0;
        std::deque<Byte>              arriving;
        std::vector<Byte>             sent;
        int                           garbled  = 0;
        std::shared_ptr<IByteStream>  stream;
    };

    bool  IsOnLine (int channel) const
    {
        uint32_t  baud = GetBaud (channel);



        return baud != 0 && (m_lineBaud == kAnyBaud || baud == m_lineBaud);
    }

    void  Pump (int channel)
    {
        Channel &  c        = m_channels[channel];
        Byte       byte     = 0;
        size_t     received = 0;
        HRESULT    hr       = S_OK;
        bool       reading  = c.stream != nullptr;



        while (reading && c.stream->GetReadableCount() > 0)
        {
            hr      = c.stream->Read (&byte, 1, 0, received);
            reading = SUCCEEDED (hr) && received == 1;

            if (reading)
            {
                c.arriving.push_back (byte);
            }
        }
    }

    Byte  ReadCommand (int channel)
    {
        Channel &  c        = m_channels[channel];
        bool       receiver = (c.wr[3] & 0xC1) == 0xC1;
        Byte       value    = 0;



        if (c.selected == 0)
        {
            Pump (channel);

            while (!c.arriving.empty() && !(receiver && IsOnLine (channel)))
            {
                c.arriving.pop_front();
                c.garbled++;
            }

            value = (Byte) (0x2C | (c.arriving.empty() ? 0x00 : 0x01));
        }
        else if (c.selected == 1)
        {
            value = 0x01;
        }

        c.selected = 0;
        return value;
    }

    void  WriteCommand (int channel, Byte value)
    {
        Channel &  c = m_channels[channel];



        if (c.selected == 0)
        {
            c.selected = (value & 0x07) | (((value >> 3) & 0x07) == 1 ? 8 : 0);
            c.wr[0]    = value;
        }
        else
        {
            c.wr[c.selected] = value;
            c.selected       = 0;
        }
    }

    Byte  ReadData (int channel)
    {
        Channel &  c     = m_channels[channel];
        Byte       value = 0;



        if (!c.arriving.empty())
        {
            value = c.arriving.front();
            c.arriving.pop_front();
        }

        return value;
    }

    void  WriteData (int channel, Byte value)
    {
        Channel &  c           = m_channels[channel];
        bool       transmitter = (c.wr[5] & 0x68) == 0x68;



        if (transmitter && IsOnLine (channel))
        {
            c.sent.push_back (value);

            if (c.stream)
            {
                c.stream->Write (&value, 1);
            }
        }
        else
        {
            c.garbled++;
        }
    }

    std::array<Channel, 2>  m_channels;
    uint32_t                m_lineBaud = 0;
    Byte                    m_speed    = 0x80;
    std::vector<Byte>       m_speedWrites;
};
