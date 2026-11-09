#include "Pch.h"

#include "Seams/Win32TcpStream.h"

#pragma comment(lib, "ws2_32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::~Win32TcpStream
//
////////////////////////////////////////////////////////////////////////////////

Win32TcpStream::~Win32TcpStream()
{
    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::Listen
//
//  The port is taken exclusively, so a second program cannot bind it too and
//  take this stream's clients.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32TcpStream::Listen (const char * bindAddress, uint16_t port)
{
    HRESULT      hr        = S_OK;
    sockaddr_in  address   = {};
    int          result    = 0;
    BOOL         exclusive = TRUE;



    CBRAEx (bindAddress != nullptr, E_INVALIDARG);

    hr = StartWinsock();
    CHR (hr);

    m_listener = ::socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CWR (m_listener != INVALID_SOCKET);

    result = setsockopt (m_listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *) &exclusive, sizeof (exclusive));
    CWR (result == 0);

    address.sin_family = AF_INET;
    address.sin_port   = htons (port);

    result = inet_pton (AF_INET, bindAddress, &address.sin_addr);
    CBREx (result == 1, E_INVALIDARG);

    result = ::bind (m_listener, (const sockaddr *) &address, sizeof (address));
    CWR (result == 0);

    result = ::listen (m_listener, 1);
    CWR (result == 0);

    m_thread = std::thread ([this]() { RunListener(); });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::Connect
//
//  The socket connects without blocking and is polled a slice at a time, so
//  a stop ends a connect to an address that does not answer at once rather
//  than at the system's timeout. It blocks again once connected.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32TcpStream::Connect (const std::string & host, uint16_t port, std::stop_token stop)
{
    HRESULT       hr       = S_OK;
    addrinfo      hints    = {};
    addrinfo    * found    = nullptr;
    std::string   service  = std::to_string (port);
    SOCKET        client   = INVALID_SOCKET;
    int           result   = 0;
    u_long        nonBlock = 1;
    WSAPOLLFD     poll     = {};
    int           error    = 0;
    int           size     = sizeof (error);
    ULONGLONG     deadline = 0;
    bool          stopped  = false;
    bool          late     = false;



    hr = StartWinsock();
    CHR (hr);

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    result = getaddrinfo (host.c_str(), service.c_str(), &hints, &found);
    CBREx (result == 0, HRESULT_FROM_WIN32 (result));

    client = ::socket (found->ai_family, found->ai_socktype, found->ai_protocol);
    CWR (client != INVALID_SOCKET);

    result = ioctlsocket (client, FIONBIO, &nonBlock);
    CWR (result == 0);

    result = ::connect (client, found->ai_addr, (int) found->ai_addrlen);
    error  = (result == 0) ? 0 : WSAGetLastError();
    CBREx (result == 0 || error == WSAEWOULDBLOCK, HRESULT_FROM_WIN32 (error));

    deadline = GetTickCount64() + s_kConnectTimeoutMs;

    //  Writable means the connect ended; SO_ERROR is how it ended.
    while (result != 0)
    {
        stopped = stop.stop_requested();
        late    = GetTickCount64() >= deadline;

        CBREx (!stopped, HRESULT_FROM_WIN32 (ERROR_CANCELLED));
        CBREx (!late, HRESULT_FROM_WIN32 (WSAETIMEDOUT));

        poll   = { client, POLLWRNORM, 0 };
        result = WSAPoll (&poll, 1, s_kConnectSliceMs);
        CWR (result != SOCKET_ERROR);

        if (result > 0)
        {
            result = getsockopt (client, SOL_SOCKET, SO_ERROR, (char *) &error, &size);
            CWR (result == 0);
            CBREx (error == 0, HRESULT_FROM_WIN32 (error));
        }
        else
        {
            result = -1;
        }
    }

    nonBlock = 0;
    result   = ioctlsocket (client, FIONBIO, &nonBlock);
    CWR (result == 0);

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_client = client;
        client   = INVALID_SOCKET;
    }

    m_thread = std::thread ([this, connected = m_client]()
    {
        RunReceiver (connected);

        {
            std::lock_guard<std::mutex>  guard (m_lock);

            m_closed = true;
        }

        m_arrived.notify_all();
    });

Error:
    if (client != INVALID_SOCKET)
    {
        closesocket (client);
    }

    if (found != nullptr)
    {
        freeaddrinfo (found);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::Write
//
//  The socket is copied under the lock and written outside it, so a send
//  delayed by a full window never blocks the receiving thread. A stream set
//  to drop sends a byte at a time, each once the socket can take it, and
//  drops the rest when it cannot: a blocking send larger than the room left
//  would block.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32TcpStream::Write (const Byte * data, size_t count)
{
    HRESULT    hr     = S_OK;
    SOCKET     client = INVALID_SOCKET;
    size_t     sent   = 0;
    int        result = 0;
    WSAPOLLFD  poll   = {};



    CBRAEx (data != nullptr || count == 0, E_INVALIDARG);

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        CBREx (!m_closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

        client = m_client;
    }

    BAIL_OUT_IF (client == INVALID_SOCKET, S_OK);

    while (sent < count)
    {
        if (m_dropWhenFull)
        {
            poll   = { client, POLLWRNORM, 0 };
            result = WSAPoll (&poll, 1, 0);
            CWR (result != SOCKET_ERROR);
            BAIL_OUT_IF ((poll.revents & POLLWRNORM) == 0, S_OK);
        }

        result = ::send (client, (const char *) data + sent, m_dropWhenFull ? 1 : (int) (count - sent), 0);
        CWR (result > 0);

        sent += (size_t) result;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32TcpStream::Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead)
{
    HRESULT                       hr    = S_OK;
    std::unique_lock<std::mutex>  guard (m_lock);



    outRead = 0;

    CBRAEx (buffer != nullptr || capacity == 0, E_INVALIDARG);

    m_arrived.wait_for (guard, std::chrono::milliseconds (timeoutMs), [this]()
    {
        return !m_received.empty() || m_closed;
    });

    while (outRead < capacity && !m_received.empty())
    {
        buffer[outRead++] = m_received.front();
        m_received.pop_front();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::GetReadableCount
//
////////////////////////////////////////////////////////////////////////////////

size_t Win32TcpStream::GetReadableCount()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_received.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::IsClosed
//
////////////////////////////////////////////////////////////////////////////////

bool Win32TcpStream::IsClosed() const
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::Close
//
//  Closing the sockets ends the blocked accept and receive, so the thread
//  returns and is joined.
//
////////////////////////////////////////////////////////////////////////////////

void Win32TcpStream::Close()
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_closed = true;

        if (m_listener != INVALID_SOCKET)
        {
            closesocket (m_listener);
            m_listener = INVALID_SOCKET;
        }

        if (m_client != INVALID_SOCKET)
        {
            closesocket (m_client);
            m_client = INVALID_SOCKET;
        }
    }

    m_arrived.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    if (m_winsock)
    {
        WSACleanup();
        m_winsock = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::StartWinsock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32TcpStream::StartWinsock()
{
    HRESULT  hr     = S_OK;
    WSADATA  data   = {};
    int      result = 0;



    BAIL_OUT_IF (m_winsock, S_OK);

    result = WSAStartup (MAKEWORD (2, 2), &data);
    CBREx (result == 0, HRESULT_FROM_WIN32 (result));

    m_winsock = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::RunListener
//
//  One client at a time; after it disconnects, the next. Accept fails once Close
//  shuts the listening socket, which ends the loop.
//
////////////////////////////////////////////////////////////////////////////////

void Win32TcpStream::RunListener()
{
    SOCKET  client = INVALID_SOCKET;



    for (client = ::accept (m_listener, nullptr, nullptr); client != INVALID_SOCKET; client = ::accept (m_listener, nullptr, nullptr))
    {
        bool  closed = false;

        //  A client accepted as Close runs is closed here, or nothing
        //  would end its receive and Close's join would wait on it.
        {
            std::lock_guard<std::mutex>  guard (m_lock);

            closed = m_closed;

            if (closed)
            {
                closesocket (client);
            }
            else
            {
                m_client = client;
            }
        }

        if (closed)
        {
            break;
        }

        RunReceiver (client);
        DropClient();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::RunReceiver
//
////////////////////////////////////////////////////////////////////////////////

void Win32TcpStream::RunReceiver (SOCKET client)
{
    char  chunk[s_kReceiveChunk] = {};
    int   result                 = 0;



    for (result = ::recv (client, chunk, (int) sizeof (chunk), 0); result > 0; result = ::recv (client, chunk, (int) sizeof (chunk), 0))
    {
        {
            std::lock_guard<std::mutex>  guard (m_lock);

            m_received.insert (m_received.end(), (const Byte *) chunk, (const Byte *) chunk + result);
        }

        m_arrived.notify_all();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32TcpStream::DropClient
//
////////////////////////////////////////////////////////////////////////////////

void Win32TcpStream::DropClient()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    if (m_client != INVALID_SOCKET)
    {
        closesocket (m_client);
        m_client = INVALID_SOCKET;
    }
}
