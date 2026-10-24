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
    HRESULT  Read               (FirewallSnapshot & outSnapshot) override;
    HRESULT  ReadActiveProfiles (FirewallProfile & outActive) override;

    //  This process as the firewall's rules identify it.
    static HRESULT  GetThisProgram (FirewallProgram & outProgram);

    //  The same, or an empty program when it cannot be read, whose verdicts
    //  then match only the rules for every program.
    static FirewallProgram  ReadThisProgram ();

    //  One rule, with outInbound false for an outbound rule or one for a
    //  protocol other than TCP or UDP; outRule is then incomplete.
    static HRESULT  ReadRule       (INetFwRule * rule, bool & outInbound, FirewallRule & outRule);

private:
    static std::wstring  TakeString (BSTR text);
};
