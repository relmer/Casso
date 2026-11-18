#include "Pch.h"

#include "Seams/Win32UdpStream.h"

#pragma comment(lib, "ws2_32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UdpStream::~Win32UdpStream
//
////////////////////////////////////////////////////////////////////////////////

Win32UdpStream::~Win32UdpStream()
{
    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UdpStream::Open
//
//  The socket is connected, so it sends to the address and port and takes
//  datagrams only from them. A port unreachable reply to a datagram sent,
//  as from an Apple that is booting, would otherwise fail the next receive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UdpStream::Open (const std::string & host, uint16_t port)
{
    HRESULT      hr       = S_OK;
    addrinfo     hints    = {};
    addrinfo   * found    = nullptr;
    std::string  service  = std::to_string (port);
    int          result   = 0;
    BOOL         reset    = FALSE;
    DWORD        returned = 0;



    hr = StartWinsock();
    CHR (hr);

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    result = getaddrinfo (host.c_str(), service.c_str(), &hints, &found);
    CBREx (result == 0, HRESULT_FROM_WIN32 (result));

    m_socket = ::socket (found->ai_family, found->ai_socktype, found->ai_protocol);
    CWR (m_socket != INVALID_SOCKET);

    result = WSAIoctl (m_socket, s_kUdpConnReset, &reset, sizeof (reset), nullptr, 0, &returned, nullptr, nullptr);
    CWR (result == 0);

    result = ::connect (m_socket, found->ai_addr, (int) found->ai_addrlen);
    CWR (result == 0);

    result = ::send (m_socket, "", 0, 0);
    CWR (result == 0);

    m_thread = std::thread ([this, receiving = m_socket]() { RunReceiver (receiving); });

Error:
    if (found != nullptr)
    {
        freeaddrinfo (found);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UdpStream::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UdpStream::Write (const Byte * data, size_t count)
{
    HRESULT  hr      = S_OK;
    SOCKET   sending = INVALID_SOCKET;
    size_t   sent    = 0;
    size_t   chunk   = 0;
    int      result  = 0;



    CBRAEx (data != nullptr || count == 0, E_INVALIDARG);

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        CBREx (!m_closed && m_socket != INVALID_SOCKET, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

        sending = m_socket;
    }

    while (sent < count)
    {
        if (sent > 0)
        {
            Sleep (s_kPaceMs);
        }

        chunk  = std::min (count - sent, kDatagramMax);
        result = ::send (sending, (const char *) data + sent, (int) chunk, 0);
        CWR (result == (int) chunk);

        sent += chunk;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UdpStream::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UdpStream::Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead)
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
//  Win32UdpStream::GetReadableCount
//
////////////////////////////////////////////////////////////////////////////////

size_t Win32UdpStream::GetReadableCount()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_received.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UdpStream::IsClosed
//
////////////////////////////////////////////////////////////////////////////////

bool Win32UdpStream::IsClosed() const
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UdpStream::Close
//
//  Closing the socket ends the blocked receive, so the thread returns and
//  is joined.
//
////////////////////////////////////////////////////////////////////////////////

void Win32UdpStream::Close()
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_closed = true;

        if (m_socket != INVALID_SOCKET)
        {
            closesocket (m_socket);
            m_socket = INVALID_SOCKET;
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
//  Win32UdpStream::StartWinsock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UdpStream::StartWinsock()
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
//  Win32UdpStream::RunReceiver
//
//  Each datagram's bytes are queued; a receive fails once Close closes the
//  socket, which ends the loop.
//
////////////////////////////////////////////////////////////////////////////////

void Win32UdpStream::RunReceiver (SOCKET receiving)
{
    std::array<Byte, 2048>  datagram = {};
    int                     result   = 0;



    for (result = ::recv (receiving, (char *) datagram.data(), (int) datagram.size(), 0); result != SOCKET_ERROR; result = ::recv (receiving, (char *) datagram.data(), (int) datagram.size(), 0))
    {
        {
            std::lock_guard<std::mutex>  guard (m_lock);

            m_received.insert (m_received.end(), datagram.begin(), datagram.begin() + result);
        }

        m_arrived.notify_all();
    }
}
