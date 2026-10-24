#include "Pch.h"

#include "Seams/Win32NetworkChanges.h"

#pragma comment(lib, "iphlpapi.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NetworkChanges::Win32NetworkChanges
//
//  A registration that fails has a null handle; the count then changes
//  only for the other notification.
//
////////////////////////////////////////////////////////////////////////////////

Win32NetworkChanges::Win32NetworkChanges()
{
    DWORD  result = NO_ERROR;



    result = NotifyUnicastIpAddressChange (AF_UNSPEC, &Win32NetworkChanges::OnAddress, this, FALSE, &m_address);
    m_address = (result == NO_ERROR) ? m_address : nullptr;

    result = NotifyIpInterfaceChange (AF_UNSPEC, &Win32NetworkChanges::OnInterface, this, FALSE, &m_interface);
    m_interface = (result == NO_ERROR) ? m_interface : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NetworkChanges::~Win32NetworkChanges
//
//  CancelMibChangeNotify2 returns once no callback is running.
//
////////////////////////////////////////////////////////////////////////////////

Win32NetworkChanges::~Win32NetworkChanges()
{
    if (m_address != nullptr)
    {
        CancelMibChangeNotify2 (m_address);
    }

    if (m_interface != nullptr)
    {
        CancelMibChangeNotify2 (m_interface);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NetworkChanges::OnAddress
//
////////////////////////////////////////////////////////////////////////////////

void NETIOAPI_API_ Win32NetworkChanges::OnAddress (PVOID context, PMIB_UNICASTIPADDRESS_ROW row, MIB_NOTIFICATION_TYPE type)
{
    UNREFERENCED_PARAMETER (row);
    UNREFERENCED_PARAMETER (type);

    static_cast<Win32NetworkChanges *> (context)->m_changes++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NetworkChanges::OnInterface
//
////////////////////////////////////////////////////////////////////////////////

void NETIOAPI_API_ Win32NetworkChanges::OnInterface (PVOID context, PMIB_IPINTERFACE_ROW row, MIB_NOTIFICATION_TYPE type)
{
    UNREFERENCED_PARAMETER (row);
    UNREFERENCED_PARAMETER (type);

    static_cast<Win32NetworkChanges *> (context)->m_changes++;
}
