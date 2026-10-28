#include "Pch.h"

#include "Seams/Win32HostNameLookup.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNameLookup::Win32HostNameLookup
//
////////////////////////////////////////////////////////////////////////////////

Win32HostNameLookup::Win32HostNameLookup()
{
    m_overlapped.hEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNameLookup::~Win32HostNameLookup
//
////////////////////////////////////////////////////////////////////////////////

Win32HostNameLookup::~Win32HostNameLookup()
{
    if (m_isRunning)
    {
        GetAddrInfoExCancel (&m_cancel);
        WaitForSingleObject (m_overlapped.hEvent, INFINITE);
    }

    if (m_results != nullptr)
    {
        FreeAddrInfoExW (m_results);
        m_results = nullptr;
    }

    if (m_overlapped.hEvent != nullptr)
    {
        CloseHandle (m_overlapped.hEvent);
        m_overlapped.hEvent = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNameLookup::Start
//
//  A dotted IPv4 address is the result at once. A name is looked up for IPv4
//  addresses only, which is what the serial attachments connect to.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostNameLookup::Start (const std::string & host)
{
    HRESULT  hr        = S_OK;
    IN_ADDR  address   = {};
    int      converted = 0;



    CWRA (m_overlapped.hEvent);

    converted = inet_pton (AF_INET, host.c_str(), &address);

    if (converted == 1)
    {
        m_ipv4 = ntohl (address.s_addr);
        Finish (S_OK);
    }
    else
    {
        StartLookup (host);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNameLookup::StartLookup
//
//  A lookup that fails at once is finished with its error.
//
////////////////////////////////////////////////////////////////////////////////

void Win32HostNameLookup::StartLookup (const std::string & host)
{
    ADDRINFOEXW  hints  = {};
    INT          result = 0;



    m_host            = TextEncoding::NarrowToWide (host);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    result = GetAddrInfoExW (m_host.c_str(), nullptr, NS_ALL, nullptr, &hints, &m_results, nullptr, &m_overlapped, nullptr, &m_cancel);

    if (result == WSA_IO_PENDING)
    {
        m_isRunning = true;
    }
    else
    {
        Finish ((result == NO_ERROR) ? S_OK : HRESULT_FROM_WIN32 (result));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNameLookup::TryGetResult
//
////////////////////////////////////////////////////////////////////////////////

bool Win32HostNameLookup::TryGetResult (
    HRESULT   & outHr,
    uint32_t  & outIpv4)
{
    DWORD  waited = 0;
    INT    result = 0;



    if (m_isRunning)
    {
        waited = WaitForSingleObject (m_overlapped.hEvent, 0);

        if (waited == WAIT_OBJECT_0)
        {
            m_isRunning = false;
            result      = GetAddrInfoExOverlappedResult (&m_overlapped);
            Finish ((result == NO_ERROR) ? S_OK : HRESULT_FROM_WIN32 (result));
        }
    }

    if (m_isDone)
    {
        outHr   = m_hr;
        outIpv4 = m_ipv4;
    }

    return m_isDone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNameLookup::Finish
//
//  The first IPv4 address among the results, when the lookup has any.
//
////////////////////////////////////////////////////////////////////////////////

void Win32HostNameLookup::Finish (HRESULT hr)
{
    const ADDRINFOEXW  * each = nullptr;



    m_isDone = true;
    m_hr     = hr;

    for (each = m_results; SUCCEEDED (hr) && each != nullptr; each = each->ai_next)
    {
        if (each->ai_family == AF_INET && each->ai_addr != nullptr)
        {
            m_ipv4 = ntohl (((const sockaddr_in *) each->ai_addr)->sin_addr.s_addr);
            break;
        }
    }

    if (SUCCEEDED (hr) && m_ipv4 == 0)
    {
        m_hr = HRESULT_FROM_WIN32 (WSAHOST_NOT_FOUND);
    }
}
