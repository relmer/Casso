#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TelnetCodec
//
//  The Telnet layer between a serial attachment's data and a TCP connection.
//  Data sent has $FF doubled and CR followed by NUL. Data received has a
//  doubled $FF undoubled, the NUL after a CR dropped, and every Telnet
//  command removed: replies to option negotiation go to outReplies, and
//  subnegotiation and the other commands are dropped.
//
//  Echo and suppress-go-ahead from the remote end, and suppress-go-ahead
//  from this end, are agreed to; every other option is declined. A reply is
//  sent only for a request to change an option's state, so negotiation
//  cannot loop. A sequence may be split across reads.
//
////////////////////////////////////////////////////////////////////////////////

class TelnetCodec
{
public:
    static constexpr Byte  kIac                   = 255;
    static constexpr Byte  kDont                  = 254;
    static constexpr Byte  kDo                    = 253;
    static constexpr Byte  kWont                  = 252;
    static constexpr Byte  kWill                  = 251;
    static constexpr Byte  kSb                    = 250;
    static constexpr Byte  kSe                    = 240;
    static constexpr Byte  kOptionEcho            = 1;
    static constexpr Byte  kOptionSuppressGoAhead = 3;

    void  Encode (std::span<const Byte> data, std::vector<Byte> & outWire) const;
    void  Decode (std::span<const Byte> wire, std::vector<Byte> & outData, std::vector<Byte> & outReplies);
    void  Reset  ();

private:
    enum class State
    {
        Data,
        Command,
        Option,
        Subnegotiation,
        SubnegotiationCommand,
    };

    static constexpr size_t  kOptionCount = 256;

    void  DecodeByte    (Byte value, std::vector<Byte> & outData, std::vector<Byte> & outReplies);
    void  AnswerOption  (Byte command, Byte option, std::vector<Byte> & outReplies);

    State                           m_state      = State::Data;
    Byte                            m_command    = 0;
    bool                            m_isAfterCr  = false;
    std::array<bool, kOptionCount>  m_isRemoteOn = {};      // options the remote end performs, as agreed
    std::array<bool, kOptionCount>  m_isLocalOn  = {};      // options this end performs, as agreed
};
