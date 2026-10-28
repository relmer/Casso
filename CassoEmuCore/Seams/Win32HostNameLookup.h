#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNameLookup
//
//  IHostNameLookup over an asynchronous GetAddrInfoExW, signaled through an
//  event in its OVERLAPPED. A dotted IPv4 address completes at once, without
//  a lookup. Destroying a lookup that is running cancels it and waits for the
//  cancellation, so the OVERLAPPED outlives the system's use of it.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostNameLookup : public IHostNameLookup
{
public:

    Win32HostNameLookup ();
    ~Win32HostNameLookup () override;

    Win32HostNameLookup (const Win32HostNameLookup &)             = delete;
    Win32HostNameLookup & operator= (const Win32HostNameLookup &) = delete;

    HRESULT  Start        (const std::string & host);
    bool     TryGetResult (HRESULT & outHr, uint32_t & outIpv4) override;

private:

    void  StartLookup (const std::string & host);
    void  Finish      (HRESULT hr);

    OVERLAPPED     m_overlapped = {};
    HANDLE         m_cancel     = nullptr;
    PADDRINFOEXW   m_results    = nullptr;
    std::wstring   m_host;
    bool           m_isRunning  = false;
    bool           m_isDone     = false;
    HRESULT        m_hr         = S_OK;
    uint32_t       m_ipv4       = 0;
};
