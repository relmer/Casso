#include "Pch.h"

#include "Seams/Win32LinkAnnouncements.h"
#include "Seams/Win32NetworkAdapters.h"

#pragma comment(lib, "ws2_32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Win32LinkAnnouncements::~Win32LinkAnnouncements
//
////////////////////////////////////////////////////////////////////////////////

Win32LinkAnnouncements::~Win32LinkAnnouncements()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32LinkAnnouncements::Start
//
//  The PC's own addresses are listed once, here, to recognize an
//  announcement sent from this PC.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32LinkAnnouncements::Start (uint16_t port)
{
    HRESULT                      hr       = S_OK;
    HRESULT                      listed   = S_OK;
    WSADATA                      data     = {};
    sockaddr_in                  address  = {};
    BOOL                         reuse    = TRUE;
    DWORD                        wait     = s_kWaitMs;
    int                          result   = 0;
    std::vector<NetworkAdapter>  adapters;



    result = WSAStartup (MAKEWORD (2, 2), &data);
    CBREx (result == 0, HRESULT_FROM_WIN32 (result));

    m_winsock = true;

    m_socket = ::socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CWR (m_socket != INVALID_SOCKET);

    result = setsockopt (m_socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof (reuse));
    CWR (result == 0);

    result = setsockopt (m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &wait, sizeof (wait));
    CWR (result == 0);

    address.sin_family      = AF_INET;
    address.sin_port        = htons (port);
    address.sin_addr.s_addr = htonl (INADDR_ANY);

    result = ::bind (m_socket, (const sockaddr *) &address, sizeof (address));
    CWR (result == 0);

    listed = Win32NetworkAdapters::List (adapters);
    IGNORE_RETURN_VALUE (listed, S_OK);

    for (const NetworkAdapter & adapter : adapters)
    {
        m_local.push_back ((uint32_t) ((adapter.address[0] << 24) | (adapter.address[1] << 16) | (adapter.address[2] << 8) | adapter.address[3]));
    }

    m_stop   = false;
    m_thread = std::thread ([this]() { Run(); });

Error:
    if (FAILED (hr))
    {
        Stop();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32LinkAnnouncements::Stop
//
//  Closing the socket ends the receive under way.
//
////////////////////////////////////////////////////////////////////////////////

void Win32LinkAnnouncements::Stop()
{
    m_stop = true;

    if (m_socket != INVALID_SOCKET)
    {
        closesocket (m_socket);
        m_socket = INVALID_SOCKET;
    }

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
//  Win32LinkAnnouncements::GetAnnounced
//
////////////////////////////////////////////////////////////////////////////////

std::vector<LinkAnnouncement> Win32LinkAnnouncements::GetAnnounced() const
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_announced;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32LinkAnnouncements::Run
//
//  A receive that times out is made again, as is one that fails otherwise,
//  as a UDP socket's does after an ICMP error.
//
////////////////////////////////////////////////////////////////////////////////

void Win32LinkAnnouncements::Run()
{
    Byte              buffer[512] = {};
    sockaddr_in       from        = {};
    int               fromLength  = 0;
    int               received    = 0;
    LinkAnnouncement  announcement;



    while (!m_stop)
    {
        fromLength = sizeof (from);
        received   = ::recvfrom (m_socket, (char *) buffer, (int) sizeof (buffer), 0, (sockaddr *) &from, &fromLength);

        if (received > 0 && LinkAnnouncement::Parse (std::span<const Byte> (buffer, (size_t) received), announcement))
        {
            Record (announcement, from);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32LinkAnnouncements::Record
//
////////////////////////////////////////////////////////////////////////////////

void Win32LinkAnnouncements::Record (LinkAnnouncement announcement, const sockaddr_in & from)
{
    uint32_t                     sender = ntohl (from.sin_addr.s_addr);
    bool                         local  = (sender >> 24) == 127 || std::find (m_local.begin(), m_local.end(), sender) != m_local.end();
    std::lock_guard<std::mutex>  guard (m_lock);
    auto                         known  = std::find_if (m_announced.begin(), m_announced.end(), [&announcement] (const LinkAnnouncement & listed)
    {
        return listed.mac == announcement.mac;
    });



    if (local)
    {
        announcement.address = { 127, 0, 0, 1 };
    }

    if (known == m_announced.end())
    {
        m_announced.push_back (announcement);
        m_changes++;
    }
    else if (!(*known == announcement))
    {
        *known = announcement;
        m_changes++;
    }
}
