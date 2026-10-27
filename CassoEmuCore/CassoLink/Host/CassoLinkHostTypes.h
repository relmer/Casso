#pragma once

#include "Pch.h"


class IByteStream;
class ICassoLinkFolder;
struct CassoLinkClient;
struct CassoLinkStatus;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHostTypes
//
//  The types CassoLinkHost and its parts share. CassoLinkHost derives from it, so
//  each is also CassoLinkHost::<type>.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkHostTypes
{
    //  The four bytes Read3 sends ahead of a block: minute, hour, then the
    //  ProDOS date, low byte first.
    using ClockFn = std::function<std::array<Byte, 4> ()>;

    //  Which client the host serves. A request from the other is skipped as
    //  noise, so an ADTPro-compatible server connection serves only ADTPro's
    //  client and every other connection only Casso's.
    enum class Clients
    {
        Both,
        Casso,
        AdtPro,
    };

    static constexpr DWORD   kReplyTimeoutMs     = 3000;

    static constexpr DWORD   kOperationTimeoutMs = 30000;

    //  Where each of ADTPro's requests is reported, as a line of text, once
    //  it is served.
    using ActivityFn = std::function<void (const std::string & text)>;
};
