#pragma once

#include "Pch.h"

#include "Seams/IByteStreamFactory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSearch
//
//  Finds the COM port an CassoLink is on, for a serial link with no port
//  set. Each listed port is opened in turn at each of the given
//  speeds, the bootstrap speed and then the link's; a port another program
//  has open fails to open and is skipped. Idle is sent, and the port is the
//  link's when Casso's shim or program replies Ready within the timeout.
//  Each port is closed before it is opened again or the next is opened, and
//  the search stops at the first that replies.
//
//  It runs only when a link with no port starts, never in the background.
//  Another device on a port receives only the Idle packet.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkSearch
{
public:
    static constexpr DWORD  kReplyTimeoutMs = 1000;

    //  ERROR_NOT_FOUND when no port replies, and ERROR_CANCELLED once `stop`
    //  is requested, checked before each port and speed.
    static HRESULT  Find (IByteStreamFactory & factory, const ComSettings & settings, std::span<const DWORD> speeds,
                          DWORD replyTimeoutMs, std::wstring & outPort, std::stop_token stop = {});

private:
    static HRESULT  Probe (IByteStream & stream, DWORD replyTimeoutMs, bool & outReplied);
};
