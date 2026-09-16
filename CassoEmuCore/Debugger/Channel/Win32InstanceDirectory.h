#pragma once

#include "Debugger/Channel/IInstanceDirectory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InstanceDirectory
//
//  Running instances found by listing `\\.\pipe\` for `Casso.Debug.<pid>`, and
//  reached by opening that pipe. An instance whose pipe refuses the open --
//  another user's, since the pipe admits only the user that created it -- is
//  simply not connected, which is what leaves it out of the list.
//
////////////////////////////////////////////////////////////////////////////////

class Win32InstanceDirectory : public IInstanceDirectory
{
public:
    static constexpr const wchar_t *  kPipePrefix = L"Casso.Debug.";

    std::vector<uint32_t>            ListProcessIds () override;
    std::unique_ptr<IChannelClient>  Connect        (uint32_t processId) override;

    //  The process id a pipe name carries, when it is one of ours.
    static bool  TryParsePipeName (const std::wstring & name, uint32_t & processId);
};
