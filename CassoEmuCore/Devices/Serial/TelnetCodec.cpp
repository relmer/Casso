#include "Pch.h"

#include "Devices/Serial/TelnetCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TelnetCodec::Encode
//
//  Binary mode is never agreed, so a CR goes out as CR NUL, as an NVT sends
//  a bare carriage return.
//
////////////////////////////////////////////////////////////////////////////////

void TelnetCodec::Encode (
    std::span<const Byte>    data,
    std::vector<Byte>      & outWire) const
{
    constexpr Byte  kCr  = '\r';
    constexpr Byte  kNul = 0;



    for (Byte value : data)
    {
        outWire.push_back (value);

        if (value == kIac)
        {
            outWire.push_back (kIac);
        }
        else if (value == kCr)
        {
            outWire.push_back (kNul);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TelnetCodec::Decode
//
////////////////////////////////////////////////////////////////////////////////

void TelnetCodec::Decode (
    std::span<const Byte>    wire,
    std::vector<Byte>      & outData,
    std::vector<Byte>      & outReplies)
{
    for (Byte value : wire)
    {
        DecodeByte (value, outData, outReplies);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TelnetCodec::Reset
//
//  A new connection: no option agreed and no sequence under way.
//
////////////////////////////////////////////////////////////////////////////////

void TelnetCodec::Reset()
{
    m_state     = State::Data;
    m_command   = 0;
    m_isAfterCr = false;
    m_isRemoteOn.fill (false);
    m_isLocalOn.fill (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TelnetCodec::DecodeByte
//
//  One byte through the state machine: data, IAC and the command after it,
//  the option after WILL, WONT, DO or DONT, and a subnegotiation, which ends
//  at IAC SE.
//
////////////////////////////////////////////////////////////////////////////////

void TelnetCodec::DecodeByte (
    Byte                  value,
    std::vector<Byte>   & outData,
    std::vector<Byte>   & outReplies)
{
    constexpr Byte  kCr        = '\r';
    constexpr Byte  kNul       = 0;
    bool            wasAfterCr = m_isAfterCr;



    m_isAfterCr = false;

    switch (m_state)
    {
    case State::Data:
        if (value == kIac)
        {
            m_state = State::Command;
        }
        else if (!(value == kNul && wasAfterCr))
        {
            outData.push_back (value);
            m_isAfterCr = value == kCr;
        }

        break;

    case State::Command:
        m_command = value;
        m_state   = (value >= kWill && value <= kDont) ? State::Option
                  : (value == kSb)                     ? State::Subnegotiation
                  :                                      State::Data;

        if (value == kIac)
        {
            outData.push_back (kIac);
        }

        break;

    case State::Option:
        AnswerOption (m_command, value, outReplies);
        m_state = State::Data;
        break;

    case State::Subnegotiation:
        m_state = (value == kIac) ? State::SubnegotiationCommand : State::Subnegotiation;
        break;

    case State::SubnegotiationCommand:
        m_state = (value == kSe) ? State::Data : State::Subnegotiation;
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TelnetCodec::AnswerOption
//
//  WILL and WONT are about what the remote end performs, DO and DONT about
//  what this end performs. A request to turn an option on is agreed to or
//  declined; a request to turn off one that is on is agreed to; a request
//  for the state an option already has is not acknowledged.
//
////////////////////////////////////////////////////////////////////////////////

void TelnetCodec::AnswerOption (
    Byte                  command,
    Byte                  option,
    std::vector<Byte>   & outReplies)
{
    bool    isRemote  = command == kWill || command == kWont;
    bool    isOnWish  = command == kWill || command == kDo;
    bool  & isOn      = isRemote ? m_isRemoteOn[option] : m_isLocalOn[option];
    bool    isAllowed = isRemote ? (option == kOptionEcho || option == kOptionSuppressGoAhead)
                                 : (option == kOptionSuppressGoAhead);
    Byte    yes       = isRemote ? kDo   : kWill;
    Byte    no        = isRemote ? kDont : kWont;



    if (isOnWish == isOn)
    {
        return;
    }

    if (isOnWish && !isAllowed)
    {
        outReplies.insert (outReplies.end(), { kIac, no, option });
        return;
    }

    isOn = isOnWish;
    outReplies.insert (outReplies.end(), { kIac, isOnWish ? yes : no, option });
}
