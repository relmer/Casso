#pragma once

#include "Pch.h"

#include "Net/FirewallCheck.h"
#include "Seams/IHostFirewall.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall
//
//  IHostFirewall over INetFwPolicy2, on a thread with COM initialized. A
//  rule for a protocol other than TCP or UDP is left out, since no Casso
//  socket uses one.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostFirewall : public IHostFirewall
{
public:
    HRESULT  Read (FirewallSnapshot & outSnapshot) override;

    //  This process as the firewall's rules identify it.
    static HRESULT  GetThisProgram (FirewallProgram & outProgram);

private:
    static HRESULT       ReadRule   (INetFwRule * rule, bool & outInbound, FirewallRule & outRule);
    static std::wstring  TakeString (BSTR text);
};
