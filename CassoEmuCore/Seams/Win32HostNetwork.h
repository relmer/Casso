#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostNetwork
//
//  IHostNetwork over Winsock 2. Winsock is started in the constructor and
//  cleaned up in the destructor, so every socket this creates must be
//  destroyed first; the shell owns this object for the life of the process.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostNetwork : public IHostNetwork
{
public:

    Win32HostNetwork ();
    ~Win32HostNetwork () override;

    Win32HostNetwork (const Win32HostNetwork &)             = delete;
    Win32HostNetwork & operator= (const Win32HostNetwork &) = delete;

    HRESULT  CreateSocket (HostSocketKind kind, std::unique_ptr<IHostSocket> & outSocket) override;
    HRESULT  GetDnsServer (uint32_t & outIpv4)                                            override;
    HRESULT  LookUpHost   (const std::string & host, std::unique_ptr<IHostNameLookup> & outLookup) override;

private:

    bool  m_started = false;
};
