#pragma once

#include "Core/JsonValue.h"

class IChannelClient;
struct ChannelClient;





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannelResult
//
//  One command's outcome on the channel: the reply's status, text and typed
//  data, and for a run the stop that ended it.
//
////////////////////////////////////////////////////////////////////////////////

struct McpChannelResult
{
    std::string               status;
    std::vector<std::string>  text;
    std::optional<JsonValue>  data;
    std::string               errorLabel;
    std::string               errorDetail;
    std::optional<JsonValue>  stop;
    bool                      isClosed   = false;
    bool                      isTimedOut = false;
    bool                      wasPaused  = false;

    bool  IsOk () const { return status == "ok" && !isClosed && !isTimedOut; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  McpChannel
//
//  The MCP server's connection to one Casso's debug channel. Execute sends a
//  command line and waits for its reply; a run the line starts is waited for
//  until its stop, and paused when it outlasts the timeout, as
//  `CassoCli debug --attach` does, so no run goes on unwatched.
//
////////////////////////////////////////////////////////////////////////////////

class McpChannel
{
public:
    //  How long a paused run has to report its stop.
    static constexpr DWORD  kPauseWaitMs = 5000;

    explicit McpChannel (std::unique_ptr<IChannelClient> client);
    ~McpChannel();

    McpChannel (const McpChannel &)             = delete;
    McpChannel & operator= (const McpChannel &) = delete;

    //  The hello record Casso sent in reply, or false when none arrived.
    //  `client` sets the file access of the commands sent after it.
    bool  Hello    (DWORD timeoutMs, const ChannelClient & client, std::string & record);

    //  `budget` is the cycle limit of a run the line starts; `overwrite`
    //  lets a write the line makes replace a file that exists.
    void  Execute  (const std::string & line, const std::string & mode, uint64_t budget, bool overwrite, DWORD timeoutMs, McpChannelResult & result);

    //  A line that starts a run with no budget, its reply waited for and its
    //  stop not: the machine runs on as it does after the debugger's Go.
    void  Start    (const std::string & line, const std::string & mode, DWORD timeoutMs, McpChannelResult & result);

    bool  IsClosed () const;

    //  Whether the machine is paused, and where, by the hello record and
    //  the stopped and resumed notifications read since; Drain reads those
    //  that have arrived without waiting.
    void  Drain    ();
    bool  IsPaused () const { return m_isPaused; }
    int   GetPc    () const { return m_pc; }

private:
    using IsWantedFn = std::function<bool (const JsonValue & record, const std::string & type)>;

    enum class Wait
    {
        Received,
        TimedOut,
        Closed,
    };

    Wait  ReadUntil   (DWORD timeoutMs, const IsWantedFn & isWanted, JsonValue & found);
    void  ReadReply   (const JsonValue & reply, McpChannelResult & result);
    void  PauseAfter  (int64_t causeId, McpChannelResult & result);

    void  Note        (const JsonValue & record, const std::string & type);

    std::unique_ptr<IChannelClient>  m_client;
    int64_t                          m_nextId   = 1;
    bool                             m_isPaused = false;
    int                              m_pc       = 0;
};
