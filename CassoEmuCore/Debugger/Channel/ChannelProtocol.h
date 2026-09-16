#pragma once

#include "Core/JsonValue.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelRequestType
//
////////////////////////////////////////////////////////////////////////////////

enum class ChannelRequestType
{
    Hello,
    Command,
    Pause,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelRequest
//
//  One record a client sent. `mode` and `budget` are absent unless the client
//  gave them, which is the difference between "use the session's" and "use
//  this one for this line".
//
////////////////////////////////////////////////////////////////////////////////

struct ChannelRequest
{
    ChannelRequestType          type     = ChannelRequestType::Command;
    int64_t                     id       = 0;
    std::string                 line;
    std::optional<CommandMode>  mode;
    std::optional<uint64_t>     budget;

    // hello only, and both optional: a client need not name itself.
    std::string                 client;
    int                         protocol = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelHello
//
//  What the server answers a hello with, and what `debug --list` reads to
//  describe an instance. A drive with no disk is an absent entry rather than
//  an empty string, because "no disk" and "a disk whose path is empty" are
//  not the same thing.
//
////////////////////////////////////////////////////////////////////////////////

struct ChannelHello
{
    int64_t                                   id       = 0;
    int                                       protocol = 0;
    uint32_t                                  pid      = 0;
    std::string                               title;
    std::string                               machine;
    std::vector<std::optional<std::string>>   disks;
    CommandMode                               mode     = CommandMode::AppleWin;
    bool                                      isPaused = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChannelProtocol
//
//  The debug channel's records, as data in and data out. It knows nothing of
//  pipes, threads or sessions, so the whole contract is testable without any
//  of them.
//
//  REPLIES AND NOTIFICATIONS ARE ReplyJson's, not written again here. Batch
//  mode's `--json` prints the same records, and the contract promises they are
//  the same records; two writers would be two chances to disagree. What this
//  adds is the half ReplyJson has no reason to know about: reading a client's
//  request, the handshake, and the error record for a line that never became
//  a request at all.
//
////////////////////////////////////////////////////////////////////////////////

class ChannelProtocol
{
public:
    //  Raised only when an existing field changes meaning or is removed.
    //  Added fields, data kinds, notification types and stop reasons do not
    //  change it, which is what lets the server answer a client that asked
    //  for a higher version rather than refusing it.
    static constexpr int     kProtocolVersion = 1;

    //  A longer line is refused and the connection stays open, so one
    //  oversized request cannot end a session.
    static constexpr size_t  kMaxLineBytes    = 1024 * 1024;

    static bool         TryParseRequest (const std::string & line, ChannelRequest & request, ReplyError & error);
    static std::string  WriteHello      (const ChannelHello & hello);
    static std::string  WriteError      (const std::string & label, const std::string & detail);

    static const char * GetRequestTypeName (ChannelRequestType type);

private:
    using Members = std::vector<std::pair<std::string, JsonValue>>;

    static std::string  WriteLine  (Members && members);
    static JsonValue    MakeDisks  (const std::vector<std::optional<std::string>> & disks);
    static bool         TryGetMode (const std::string & name, CommandMode & mode);
    static void         SetError   (ReplyError & error, const std::string & label, const std::string & detail);
};
