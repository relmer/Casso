#pragma once

#include "Pch.h"

#include "Seams/INetworkChanges.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NetworkChanges
//
//  INetworkChanges over NotifyUnicastIpAddressChange and
//  NotifyIpInterfaceChange. The callbacks run on a thread from Windows' pool,
//  and only increment the count. With the notifications unavailable, the count
//  never changes.
//
////////////////////////////////////////////////////////////////////////////////

class Win32NetworkChanges : public INetworkChanges
{
public:
    Win32NetworkChanges ();
    ~Win32NetworkChanges () override;

    uint32_t  GetChangeCount () const override { return m_changes; }

private:
    static void NETIOAPI_API_  OnAddress   (PVOID context, PMIB_UNICASTIPADDRESS_ROW row, MIB_NOTIFICATION_TYPE type);
    static void NETIOAPI_API_  OnInterface (PVOID context, PMIB_IPINTERFACE_ROW row, MIB_NOTIFICATION_TYPE type);

    HANDLE                 m_address   = nullptr;
    HANDLE                 m_interface = nullptr;
    std::atomic<uint32_t>  m_changes   { 0 };
};
