#include "Pch.h"

#include "Seams/Win32HostSocket.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Win32HostSocket
//
////////////////////////////////////////////////////////////////////////////////

Win32HostSocket::Win32HostSocket (SOCKET handle)
    : m_handle (handle)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::~Win32HostSocket
//
////////////////////////////////////////////////////////////////////////////////

Win32HostSocket::~Win32HostSocket()
{
    if (m_handle != INVALID_SOCKET)
    {
        ::closesocket (m_handle);
        m_handle = INVALID_SOCKET;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Create (
    HostSocketKind                  kind,
    std::unique_ptr<IHostSocket>  & outSocket)
{
    HRESULT  hr       = S_OK;
    SOCKET   handle   = INVALID_SOCKET;
    int      type     = (kind == HostSocketKind::Tcp) ? SOCK_STREAM : SOCK_DGRAM;
    int      protocol = (kind == HostSocketKind::Tcp) ? IPPROTO_TCP : IPPROTO_UDP;



    handle = ::socket (AF_INET, type, protocol);
    CBREx (handle != INVALID_SOCKET, GetLastSocketError());

    hr = PrepareSocket (handle, kind);
    CHR (hr);

    outSocket = std::make_unique<Win32HostSocket> (handle);
    handle    = INVALID_SOCKET;

Error:
    if (handle != INVALID_SOCKET)
    {
        ::closesocket (handle);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::PrepareSocket
//
//  Non-blocking for every socket. A UDP socket also stops reporting an
//  ICMP port-unreachable from an earlier send as a reset on its next
//  receive, which Windows does by default and which would read as the
//  socket failing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::PrepareSocket (
    SOCKET          handle,
    HostSocketKind  kind)
{
    // mstcpip.h's value; that header redefines ASSERT over Ehm's.
    constexpr DWORD  kSioUdpConnReset = _WSAIOW (IOC_VENDOR, 12);
    HRESULT          hr               = S_OK;
    u_long           nonBlocking      = 1;
    BOOL             reportReset      = FALSE;
    DWORD            returned         = 0;
    int              result           = 0;



    result = ::ioctlsocket (handle, FIONBIO, &nonBlocking);
    CBREx (result == 0, GetLastSocketError());

    if (kind == HostSocketKind::Udp)
    {
        result = WSAIoctl (handle,
                           kSioUdpConnReset,
                           &reportReset,
                           sizeof (reportReset),
                           nullptr,
                           0,
                           &returned,
                           nullptr,
                           nullptr);
        CBREx (result == 0, GetLastSocketError());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Bind
//
//  Exclusive use of the port, so binding a port another program has bound
//  fails here rather than silently sharing it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Bind (
    const HostEndpoint  & local,
    bool                  allowBroadcast)
{
    HRESULT      hr        = S_OK;
    sockaddr_in  address   = EndpointToAddress (local);
    BOOL         exclusive = TRUE;
    BOOL         broadcast = allowBroadcast ? TRUE : FALSE;
    int          result    = 0;



    result = ::setsockopt (m_handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *) &exclusive, sizeof (exclusive));
    CBREx (result == 0, GetLastSocketError());

    if (allowBroadcast)
    {
        result = ::setsockopt (m_handle, SOL_SOCKET, SO_BROADCAST, (const char *) &broadcast, sizeof (broadcast));
        CBREx (result == 0, GetLastSocketError());
    }

    result = ::bind (m_handle, (const sockaddr *) &address, sizeof (address));
    CBREx (result == 0, GetLastSocketError());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::GetLocal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::GetLocal (HostEndpoint & outLocal) const
{
    HRESULT      hr      = S_OK;
    sockaddr_in  address = {};
    int          length  = sizeof (address);
    int          result  = 0;



    result = ::getsockname (m_handle, (sockaddr *) &address, &length);
    CBREx (result == 0, GetLastSocketError());

    outLocal = AddressToEndpoint (address);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Listen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Listen()
{
    HRESULT  hr     = S_OK;
    int      result = 0;



    result = ::listen (m_handle, SOMAXCONN);
    CBREx (result == 0, GetLastSocketError());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Accept
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Accept (
    std::unique_ptr<IHostSocket>  & outAccepted,
    HostEndpoint                  & outPeer,
    bool                          & outWouldBlock)
{
    HRESULT      hr       = S_OK;
    sockaddr_in  address  = {};
    int          length   = sizeof (address);
    SOCKET       accepted = INVALID_SOCKET;
    int          wsaError = 0;



    outWouldBlock = false;

    accepted = ::accept (m_handle, (sockaddr *) &address, &length);

    if (accepted == INVALID_SOCKET)
    {
        wsaError      = WSAGetLastError();
        outWouldBlock = IsWouldBlock (wsaError);
        BAIL_OUT_IF (outWouldBlock, S_OK);
        hr = HRESULT_FROM_WIN32 (wsaError);
        CHR (hr);
    }

    hr = PrepareSocket (accepted, HostSocketKind::Tcp);
    CHR (hr);

    outAccepted = std::make_unique<Win32HostSocket> (accepted);
    outPeer     = AddressToEndpoint (address);
    accepted    = INVALID_SOCKET;

Error:
    if (accepted != INVALID_SOCKET)
    {
        ::closesocket (accepted);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Connect
//
//  A non-blocking connect reports WSAEWOULDBLOCK while the handshake runs;
//  that is the normal start, and Poll reports how it ends.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Connect (const HostEndpoint & remote)
{
    HRESULT      hr       = S_OK;
    sockaddr_in  address  = EndpointToAddress (remote);
    int          result   = 0;
    int          wsaError = 0;
    bool         pending  = false;



    result = ::connect (m_handle, (const sockaddr *) &address, sizeof (address));

    if (result != 0)
    {
        wsaError = WSAGetLastError();
        pending  = IsWouldBlock (wsaError);
        CBREx (pending, HRESULT_FROM_WIN32 (wsaError));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Send
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Send (
    std::span<const Byte>   bytes,
    size_t                & outSent)
{
    HRESULT  hr       = S_OK;
    int      result   = 0;
    int      wsaError = 0;
    bool     full     = false;
    bool     isEmpty  = false;



    outSent = 0;

    isEmpty = bytes.empty();
    BAIL_OUT_IF (isEmpty, S_OK);

    result = ::send (m_handle, (const char *) bytes.data(), (int) bytes.size(), 0);

    if (result == SOCKET_ERROR)
    {
        wsaError = WSAGetLastError();
        full     = IsWouldBlock (wsaError);
        BAIL_OUT_IF (full, S_OK);
        hr = HRESULT_FROM_WIN32 (wsaError);
        CHR (hr);
    }

    outSent = (size_t) result;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Receive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Receive (
    std::span<Byte>   buffer,
    size_t          & outReceived,
    bool            & outEof)
{
    HRESULT  hr       = S_OK;
    int      result   = 0;
    int      wsaError = 0;
    bool     empty    = false;
    bool     isEmpty  = false;



    outReceived = 0;
    outEof      = false;

    isEmpty = buffer.empty();
    BAIL_OUT_IF (isEmpty, S_OK);

    result = ::recv (m_handle, (char *) buffer.data(), (int) buffer.size(), 0);

    if (result == SOCKET_ERROR)
    {
        wsaError = WSAGetLastError();
        empty    = IsWouldBlock (wsaError);
        BAIL_OUT_IF (empty, S_OK);
        hr = HRESULT_FROM_WIN32 (wsaError);
        CHR (hr);
    }

    outReceived = (size_t) result;
    outEof      = (result == 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::SendTo
//
//  A datagram the send buffer cannot take right now is dropped, which is
//  what UDP permits and what a guest stack already copes with.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::SendTo (
    const HostEndpoint     & remote,
    std::span<const Byte>    bytes)
{
    HRESULT      hr       = S_OK;
    sockaddr_in  address  = EndpointToAddress (remote);
    int          result   = 0;
    int          wsaError = 0;
    bool         full     = false;



    result = ::sendto (m_handle,
                     (const char *) bytes.data(),
                     (int) bytes.size(),
                     0,
                     (const sockaddr *) &address,
                     sizeof (address));

    if (result == SOCKET_ERROR)
    {
        wsaError = WSAGetLastError();
        full     = IsWouldBlock (wsaError);
        BAIL_OUT_IF (full, S_OK);
        hr = HRESULT_FROM_WIN32 (wsaError);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::ReceiveFrom
//
//  A datagram larger than the buffer arrives truncated with WSAEMSGSIZE;
//  the truncated part is delivered and the rest is discarded, as UDP allows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::ReceiveFrom (
    std::span<Byte>   buffer,
    size_t          & outReceived,
    HostEndpoint    & outFrom)
{
    HRESULT      hr       = S_OK;
    sockaddr_in  address  = {};
    int          length   = sizeof (address);
    int          result   = 0;
    int          wsaError = 0;
    bool         empty    = false;



    outReceived = 0;

    result = ::recvfrom (m_handle, (char *) buffer.data(), (int) buffer.size(), 0, (sockaddr *) &address, &length);

    if (result == SOCKET_ERROR)
    {
        wsaError = WSAGetLastError();
        empty    = IsWouldBlock (wsaError);
        BAIL_OUT_IF (empty, S_OK);

        if (wsaError == WSAEMSGSIZE)
        {
            result = (int) buffer.size();
        }
        else
        {
            hr = HRESULT_FROM_WIN32 (wsaError);
        CHR (hr);
        }
    }

    outReceived = (size_t) result;
    outFrom     = AddressToEndpoint (address);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::ShutdownSend
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::ShutdownSend()
{
    HRESULT  hr     = S_OK;
    int      result = 0;



    result = ::shutdown (m_handle, SD_SEND);
    CBREx (result == 0, GetLastSocketError());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::Poll
//
//  POLLHUP without POLLERR is an orderly close, which reads as readable so
//  the next Receive reports the end of the stream. POLLERR is a connect that
//  ended in an error, or a reset.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::Poll (HostSocketReadiness & outReadiness)
{
    HRESULT   hr     = S_OK;
    WSAPOLLFD fd     = {};
    int       result = 0;



    outReadiness = {};

    fd.fd     = m_handle;
    fd.events = POLLRDNORM | POLLWRNORM;

    result = WSAPoll (&fd, 1, 0);
    CBREx (result != SOCKET_ERROR, GetLastSocketError());

    outReadiness.readable = (fd.revents & (POLLRDNORM | POLLHUP)) != 0;
    outReadiness.writable = (fd.revents & POLLWRNORM) != 0;
    outReadiness.failed   = (fd.revents & (POLLERR | POLLNVAL)) != 0;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::EndpointToAddress
//
////////////////////////////////////////////////////////////////////////////////

sockaddr_in Win32HostSocket::EndpointToAddress (const HostEndpoint & endpoint)
{
    sockaddr_in  address = {};



    address.sin_family      = AF_INET;
    address.sin_port        = htons (endpoint.port);
    address.sin_addr.s_addr = htonl (endpoint.ipv4);

    return address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::AddressToEndpoint
//
////////////////////////////////////////////////////////////////////////////////

HostEndpoint Win32HostSocket::AddressToEndpoint (const sockaddr_in & address)
{
    HostEndpoint  endpoint;



    endpoint.ipv4 = ntohl (address.sin_addr.s_addr);
    endpoint.port = ntohs (address.sin_port);

    return endpoint;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::GetLastSocketError
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostSocket::GetLastSocketError()
{
    return HRESULT_FROM_WIN32 (WSAGetLastError());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostSocket::IsWouldBlock
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostSocket::IsWouldBlock (int wsaError)
{
    return wsaError == WSAEWOULDBLOCK || wsaError == WSAEINPROGRESS;
}
