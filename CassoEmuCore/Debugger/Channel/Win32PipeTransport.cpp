#include "Pch.h"

#include "Debugger/Channel/Win32PipeTransport.h"

#include "Debugger/Channel/ChannelProtocol.h"
#include "Debugger/Channel/INamedPipeApi.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::Win32PipeTransport
//
////////////////////////////////////////////////////////////////////////////////

Win32PipeTransport::Win32PipeTransport (INamedPipeApi & api, uint32_t processId, std::vector<BYTE> userSid) :
    m_api     (api),
    m_name    (GetPipeName (processId)),
    m_userSid (std::move (userSid))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::~Win32PipeTransport
//
////////////////////////////////////////////////////////////////////////////////

Win32PipeTransport::~Win32PipeTransport()
{
    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::GetPipeName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32PipeTransport::GetPipeName (uint32_t processId)
{
    return std::format (L"\\\\.\\pipe\\Casso.Debug.{}", processId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::Listen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PipeTransport::Listen()
{
    HRESULT  hr     = S_OK;
    bool     hasSid = !m_userSid.empty();



    CBRAEx (!m_isListening, E_UNEXPECTED);

    if (!m_isSecurityBuilt)
    {
        CBRAEx (hasSid, E_INVALIDARG);

        hr = m_security.BuildForUser ((PSID) m_userSid.data());
        CHR (hr);

        m_isSecurityBuilt = true;
    }

    hr = OpenListener (true);
    CHR (hr);

    m_isListening = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::OpenListener
//
//  An instance waiting for a client. Everything acquired along the way is
//  released on every failure, so a failed attempt leaves no handle behind.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PipeTransport::OpenListener (bool isFirst)
{
    HRESULT  hr        = S_OK;
    DWORD    openMode  = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (isFirst ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
    DWORD    pipeMode  = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    BOOL     connected = FALSE;
    DWORD    error     = ERROR_SUCCESS;



    m_listener = Listener();

    m_listener.event = m_api.CreateEventHandle (TRUE, FALSE);
    CBREx (m_listener.event != nullptr, HRESULT_FROM_WIN32 (m_api.GetLastErrorCode()));

    m_listener.pipe = m_api.CreatePipeInstance (m_name, openMode, pipeMode, PIPE_UNLIMITED_INSTANCES,
                                                kPipeBufferBytes, kPipeBufferBytes, m_security.GetAttributes());
    CBREx (m_listener.pipe != INVALID_HANDLE_VALUE, HRESULT_FROM_WIN32 (m_api.GetLastErrorCode()));

    m_listener.overlapped.hEvent = m_listener.event;

    connected = m_api.ConnectPipe (m_listener.pipe, &m_listener.overlapped);

    if (connected)
    {
        m_listener.isConnected = true;
    }
    else
    {
        error = m_api.GetLastErrorCode();

        if (error == ERROR_IO_PENDING)
        {
            m_listener.isPending = true;
        }
        else if (error == ERROR_PIPE_CONNECTED)
        {
            m_listener.isConnected = true;
        }
        else
        {
            hr = HRESULT_FROM_WIN32 (error);
        }
    }

Error:
    if (FAILED (hr))
    {
        ReleaseListener();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::ReleaseListener
//
//  A pending connect is cancelled and waited out before its handles close, so
//  the kernel never completes an operation against an event already freed.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::ReleaseListener()
{
    DWORD  transferred = 0;



    if (m_listener.pipe != INVALID_HANDLE_VALUE)
    {
        if (m_listener.isPending)
        {
            m_api.CancelIo (m_listener.pipe, &m_listener.overlapped);
            m_api.GetResult (m_listener.pipe, &m_listener.overlapped, &transferred, TRUE);
        }

        m_api.CloseHandleOf (m_listener.pipe);
    }

    if (m_listener.event != nullptr)
    {
        m_api.CloseHandleOf (m_listener.event);
    }

    m_listener = Listener();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::TryAccept
//
//  A connected listener becomes a connection and the next listener is opened.
//  If opening it fails, the transport stays listening and tries again on the
//  next call: one failed instance must not end the channel for every client
//  still to come.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PipeTransport::TryAccept (ChannelConnectionId & connection)
{
    DWORD                          transferred = 0;
    std::unique_ptr<Connection>    accepted;
    HRESULT                        hr          = S_OK;



    if (!m_isListening)
    {
        return false;
    }

    if (m_listener.pipe == INVALID_HANDLE_VALUE)
    {
        hr = OpenListener (false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        return false;
    }

    if (m_listener.isPending)
    {
        if (m_api.WaitForHandle (m_listener.event, 0) != WAIT_OBJECT_0)
        {
            return false;
        }

        m_listener.isPending = false;

        if (!m_api.GetResult (m_listener.pipe, &m_listener.overlapped, &transferred, FALSE))
        {
            ReleaseListener();
            return false;
        }

        m_listener.isConnected = true;
    }

    if (!m_listener.isConnected)
    {
        return false;
    }

    accepted             = std::make_unique<Connection>();
    accepted->pipe       = m_listener.pipe;
    accepted->readEvent  = m_listener.event;
    accepted->writeEvent = m_api.CreateEventHandle (TRUE, FALSE);

    //  The instance now belongs to the connection, whatever happens next.
    m_listener = Listener();

    if (accepted->writeEvent == nullptr)
    {
        ReleaseConnection (*accepted);
        hr = OpenListener (false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        return false;
    }

    accepted->id = m_nextId++;
    connection   = accepted->id;

    StartRead (*accepted);
    m_connections.push_back (std::move (accepted));

    hr = OpenListener (false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::StartRead
//
//  Reads until a read goes pending. A read that completes at once is taken and
//  the next one started, bounded so a client flooding the pipe cannot keep one
//  pump from returning.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::StartRead (Connection & connection)
{
    static constexpr int    kMaxImmediateReads = 16;
    DWORD                   read               = 0;
    DWORD                   error              = ERROR_SUCCESS;



    for (int attempt = 0; attempt < kMaxImmediateReads && !connection.isBroken; attempt++)
    {
        connection.readOverlapped        = OVERLAPPED();
        connection.readOverlapped.hEvent = connection.readEvent;

        if (m_api.ReadPipe (connection.pipe, connection.readBuffer.data(), kReadChunkBytes, &read, &connection.readOverlapped))
        {
            Append (connection, connection.readBuffer.data(), read);
            continue;
        }

        error = m_api.GetLastErrorCode();

        if (error == ERROR_IO_PENDING)
        {
            connection.isReadPending = true;
        }
        else
        {
            connection.isBroken = true;
        }

        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::PollRead
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::PollRead (Connection & connection)
{
    DWORD  read = 0;



    if (connection.isBroken)
    {
        return;
    }

    //  StartRead stops after a bounded number of reads that completed at once,
    //  so a flood cannot hold one pump. Such a connection has no read pending,
    //  and the next one has to be started here or it would never be read again.
    if (!connection.isReadPending)
    {
        StartRead (connection);
        return;
    }

    if (m_api.WaitForHandle (connection.readEvent, 0) != WAIT_OBJECT_0)
    {
        return;
    }

    connection.isReadPending = false;

    if (!m_api.GetResult (connection.pipe, &connection.readOverlapped, &read, FALSE))
    {
        connection.isBroken = true;
        return;
    }

    Append (connection, connection.readBuffer.data(), read);
    StartRead (connection);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::Append
//
//  Splits on LF. A partial line longer than a request may be is passed up
//  one byte over the limit, which is exactly enough for the protocol to report
//  it, and the rest of that line is dropped as it arrives.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::Append (Connection & connection, const char * bytes, DWORD count)
{
    for (DWORD i = 0; i < count; i++)
    {
        char  c = bytes[i];

        if (connection.isDiscarding)
        {
            connection.isDiscarding = (c != '\n');
            continue;
        }

        if (c == '\n')
        {
            connection.lines.push_back (std::move (connection.partial));
            connection.partial.clear();
            continue;
        }

        connection.partial.push_back (c);

        if (connection.partial.size() > ChannelProtocol::kMaxLineBytes)
        {
            connection.lines.push_back (std::move (connection.partial));
            connection.partial.clear();
            connection.isDiscarding = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::TryReadLine
//
//  A line a client finished before it left is still delivered: a client may
//  send its last request and hang up, and that request is owed an answer as
//  much as any other. The connection is removed once its lines are gone.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PipeTransport::TryReadLine (ChannelConnectionId & connection, std::string & line)
{
    for (auto & each : m_connections)
    {
        PollRead (*each);
    }

    for (auto & each : m_connections)
    {
        if (!each->lines.empty())
        {
            connection = each->id;
            line       = std::move (each->lines.front());
            each->lines.pop_front();
            return true;
        }
    }

    RemoveBroken();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::WriteLine
//
//  A connection that has gone, or that fails or stalls a write, is marked
//  broken and removed on the next read. Nothing is reported: IPipeTransport's
//  contract is that writing to a departed client is not an error.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::WriteLine (ChannelConnectionId connection, const std::string & line)
{
    Connection   * target      = nullptr;
    std::string    data        = line + "\n";
    size_t         offset      = 0;
    DWORD          written     = 0;
    OVERLAPPED     overlapped  = {};
    DWORD          error       = ERROR_SUCCESS;



    for (auto & each : m_connections)
    {
        if (each->id == connection)
        {
            target = each.get();
            break;
        }
    }

    if (target == nullptr || target->isBroken)
    {
        return;
    }

    while (offset < data.size())
    {
        overlapped        = OVERLAPPED();
        overlapped.hEvent = target->writeEvent;
        written           = 0;

        if (!m_api.WritePipe (target->pipe, data.data() + offset, (DWORD) (data.size() - offset), &written, &overlapped))
        {
            error = m_api.GetLastErrorCode();

            if (error != ERROR_IO_PENDING)
            {
                target->isBroken = true;
                return;
            }

            if (m_api.WaitForHandle (target->writeEvent, kWriteTimeoutMs) != WAIT_OBJECT_0)
            {
                m_api.CancelIo (target->pipe, &overlapped);
                m_api.GetResult (target->pipe, &overlapped, &written, TRUE);
                target->isBroken = true;
                return;
            }

            if (!m_api.GetResult (target->pipe, &overlapped, &written, FALSE))
            {
                target->isBroken = true;
                return;
            }
        }

        if (written == 0)
        {
            target->isBroken = true;
            return;
        }

        offset += written;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::ReleaseConnection
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::ReleaseConnection (Connection & connection)
{
    DWORD  transferred = 0;



    if (connection.pipe != INVALID_HANDLE_VALUE)
    {
        if (connection.isReadPending)
        {
            m_api.CancelIo (connection.pipe, &connection.readOverlapped);
            m_api.GetResult (connection.pipe, &connection.readOverlapped, &transferred, TRUE);
            connection.isReadPending = false;
        }

        m_api.DisconnectPipe (connection.pipe);
        m_api.CloseHandleOf (connection.pipe);
        connection.pipe = INVALID_HANDLE_VALUE;
    }

    if (connection.readEvent != nullptr)
    {
        m_api.CloseHandleOf (connection.readEvent);
        connection.readEvent = nullptr;
    }

    if (connection.writeEvent != nullptr)
    {
        m_api.CloseHandleOf (connection.writeEvent);
        connection.writeEvent = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::RemoveBroken
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::RemoveBroken()
{
    for (auto it = m_connections.begin(); it != m_connections.end(); )
    {
        if ((*it)->isBroken && (*it)->lines.empty())
        {
            ReleaseConnection (**it);
            it = m_connections.erase (it);
        }
        else
        {
            ++it;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::Disconnect
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::Disconnect (ChannelConnectionId connection)
{
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        if ((*it)->id == connection)
        {
            ReleaseConnection (**it);
            m_connections.erase (it);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::Close
//
////////////////////////////////////////////////////////////////////////////////

void Win32PipeTransport::Close()
{
    for (auto & each : m_connections)
    {
        ReleaseConnection (*each);
    }

    m_connections.clear();
    ReleaseListener();
    m_isListening = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeTransport::GetConnections
//
//  A broken connection is not listed: a notification sent to it would go
//  nowhere, and the server should not count it as a client.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ChannelConnectionId> Win32PipeTransport::GetConnections() const
{
    std::vector<ChannelConnectionId>  ids;



    for (const auto & each : m_connections)
    {
        if (!each->isBroken)
        {
            ids.push_back (each->id);
        }
    }

    return ids;
}
