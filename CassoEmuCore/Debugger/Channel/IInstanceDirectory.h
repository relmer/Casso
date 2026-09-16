#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  IChannelClient
//
//  The client end of one running Casso's debug channel: whole lines out, whole
//  lines in.
//
////////////////////////////////////////////////////////////////////////////////

class IChannelClient
{
public:
    virtual ~IChannelClient() = default;

    //  False once the channel has closed.
    virtual bool  WriteLine (const std::string & line) = 0;

    //  The next complete line, waiting at most `timeoutMs`. False on a timeout
    //  and on a closed channel; IsClosed says which.
    virtual bool  ReadLine  (std::string & line, DWORD timeoutMs) = 0;

    virtual bool  IsClosed  () const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IInstanceDirectory
//
//  Which Casso processes have a debug channel open, and a way to reach one.
//
//  A seam for the same reason the pipe API is one: what `debug --list` does
//  with an instance that refuses it, or that connects and never answers, is
//  the behavior worth testing, and neither can be arranged against real pipes.
//
////////////////////////////////////////////////////////////////////////////////

class IInstanceDirectory
{
public:
    virtual ~IInstanceDirectory() = default;

    //  The process ids with a channel named for them.
    virtual std::vector<uint32_t>            ListProcessIds () = 0;

    //  A connection to that process's channel, or null when it refused one --
    //  another user's instance, or one that exited since it was listed.
    virtual std::unique_ptr<IChannelClient>  Connect        (uint32_t processId) = 0;
};
