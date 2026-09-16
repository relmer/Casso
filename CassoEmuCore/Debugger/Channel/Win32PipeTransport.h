#pragma once

#include "Debugger/Channel/IPipeTransport.h"
#include "Debugger/Channel/PipeSecurity.h"

class INamedPipeApi;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport
//
//  The debug channel over a named pipe, `\\.\pipe\Casso.Debug.<pid>`.
//
//  ONE LISTENING INSTANCE AT A TIME. A named pipe accepts one client per
//  instance, so the transport keeps one instance waiting for a client and,
//  when a client arrives, hands that instance to the connection and opens the
//  next. The first instance is created with FILE_FLAG_FIRST_PIPE_INSTANCE, so a
//  process that took the name first makes Listen fail rather than leaving Casso
//  talking through someone else's pipe; later instances cannot carry that flag
//  and must not.
//
//  NOTHING HERE BLOCKS BUT A WRITE. Accepting and reading are overlapped and
//  polled, which is what IPipeTransport's pull model needs: the server pumps
//  from the CPU thread once a frame and must never stall a frame waiting for a
//  client. A write waits a bounded time for a client to take the bytes, and a
//  client that will not is dropped rather than allowed to hold the machine.
//
//  BUFFERING IS BOUNDED. A client that sends more than a request line may hold
//  without a newline gets the oversize line passed up, so the protocol can
//  answer "line too long", and the rest of that line is discarded as it
//  arrives rather than kept.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PipeTransport : public IPipeTransport
{
public:
    //  How long a write waits for a client to take its bytes before the client
    //  is dropped.
    static constexpr DWORD  kWriteTimeoutMs = 2000;

    //  The size of one overlapped read, and of the pipe's own buffers.
    static constexpr DWORD  kReadChunkBytes  = 4096;
    static constexpr DWORD  kPipeBufferBytes = 65536;

    Win32PipeTransport (INamedPipeApi & api, uint32_t processId, std::vector<BYTE> userSid);
    ~Win32PipeTransport() override;

    Win32PipeTransport             (const Win32PipeTransport &) = delete;
    Win32PipeTransport & operator= (const Win32PipeTransport &) = delete;

    static std::wstring  GetPipeName (uint32_t processId);

    // IPipeTransport
    HRESULT  Listen      () override;
    bool     TryAccept   (ChannelConnectionId & connection) override;
    bool     TryReadLine (ChannelConnectionId & connection, std::string & line) override;
    void     WriteLine   (ChannelConnectionId connection, const std::string & line) override;
    void     Disconnect  (ChannelConnectionId connection) override;
    void     Close       () override;

    std::vector<ChannelConnectionId>  GetConnections () const override;

private:
    struct Listener
    {
        HANDLE      pipe        = INVALID_HANDLE_VALUE;
        HANDLE      event       = nullptr;
        OVERLAPPED  overlapped  = {};
        bool        isPending   = false;
        bool        isConnected = false;
    };

    struct Connection
    {
        ChannelConnectionId                id             = 0;
        HANDLE                             pipe           = INVALID_HANDLE_VALUE;
        HANDLE                             readEvent      = nullptr;
        HANDLE                             writeEvent     = nullptr;
        OVERLAPPED                         readOverlapped = {};
        bool                               isReadPending  = false;
        bool                               isBroken       = false;
        bool                               isDiscarding   = false;
        std::array<char, kReadChunkBytes>  readBuffer     = {};
        std::string                        partial;
        std::deque<std::string>            lines;
    };

    HRESULT  OpenListener       (bool isFirst);
    void     ReleaseListener    ();
    void     StartRead          (Connection & connection);
    void     PollRead           (Connection & connection);
    void     Append             (Connection & connection, const char * bytes, DWORD count);
    void     ReleaseConnection  (Connection & connection);
    void     RemoveBroken       ();

    INamedPipeApi                               & m_api;
    std::wstring                                  m_name;
    std::vector<BYTE>                             m_userSid;
    PipeSecurityDescriptor                        m_security;
    bool                                          m_isSecurityBuilt = false;
    bool                                          m_isListening     = false;
    Listener                                      m_listener;
    std::vector<std::unique_ptr<Connection>>      m_connections;
    ChannelConnectionId                           m_nextId          = 1;
};
