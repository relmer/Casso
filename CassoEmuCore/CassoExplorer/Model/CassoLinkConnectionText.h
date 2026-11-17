#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectionText
//
//  The text Casso Explorer has for a connection: its state, its pending
//  operations, a failed operation, and the dial command for a Wi-Fi modem.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkConnectionText
{
public:
    //  The text for an operation that failed.
    static std::wstring  DescribeFailure (const LinkOperation & operation);

    //  The status bar's text for the pending operations: the first one's
    //  progress, and how many follow it. Empty for none.
    static std::wstring  DescribeJobs    (const std::vector<std::shared_ptr<LinkOperation>> & jobs);

    //  The text under a link's root in place of its drives.
    static std::wstring  DescribeState   (LinkState state, const CassoLinkSettings & settings, const std::wstring & pcName);

    //  A speed with its thousands separated, as 115,200.
    static std::wstring  FormatBaud      (DWORD baud);

    //  Whether a link's name already says its address's port, as a name
    //  made from the settings does, so a sentence need not repeat it.
    static bool          NamesItsPort    (const std::wstring & name, const std::wstring & address);

    //  The command a Wi-Fi modem dials for a link: ATDT, this PC's name with
    //  .local, and the link's TCP port.
    static std::wstring  DescribeDial    (const std::wstring & pcName, const CassoLinkSettings & settings);
};