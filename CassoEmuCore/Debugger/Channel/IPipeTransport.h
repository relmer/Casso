#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelConnectionId
//
//  Distinct per connection and never reused, so a reply cannot be delivered
//  to whoever inherited a closed client's slot.
//
////////////////////////////////////////////////////////////////////////////////

using ChannelConnectionId = int64_t;





////////////////////////////////////////////////////////////////////////////////
//
//  IPipeTransport
//
//  The bytes under the debug channel: accepting clients, reading a line from
//  one, writing a line to one, and closing.
//
//  PULLED RATHER THAN PUSHED. Every read is a question the server asks when
//  it is ready for an answer, which is what lets commands run one at a time
//  in arrival order without a lock: the server pumps this from one thread and
//  there is no callback arriving from another. A test double then drives a
//  whole multi-client conversation deterministically, with no threads and no
//  pipe, which is why the server's behavior is testable at all.
//
//  Lines carry no terminator. Framing is the transport's business: the real
//  one appends LF on write and splits on it when reading, and a line split
//  across two reads or two lines in one read are its problem, not the
//  server's.
//
//  A write to a connection that has gone is not an error. A client can
//  disappear between the server deciding to answer and the answer being
//  written, and nothing useful is left to do about it.
//
////////////////////////////////////////////////////////////////////////////////

class IPipeTransport
{
public:
    virtual ~IPipeTransport() = default;

    //  Starts accepting. Fails when the name is already taken, which is the
    //  case worth reporting: another process holds this instance's pipe.
    virtual HRESULT  Listen  () = 0;

    //  The next client that arrived, if one has. Never blocks.
    virtual bool     TryAccept (ChannelConnectionId & connection) = 0;

    //  The next complete line from any client, with the client it came from.
    //  Never blocks, and returns false when nothing is waiting.
    virtual bool     TryReadLine (ChannelConnectionId & connection, std::string & line) = 0;

    virtual void     WriteLine   (ChannelConnectionId connection, const std::string & line) = 0;
    virtual void     Disconnect  (ChannelConnectionId connection) = 0;

    //  Stops accepting and drops every client. The server sends `closing`
    //  before calling this.
    virtual void     Close       () = 0;

    virtual std::vector<ChannelConnectionId>  GetConnections () const = 0;
};
