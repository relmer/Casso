#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IProcessLauncher
//
//  Starting another program, and nothing else about it.
//
//  A SEAM BECAUSE A TEST MAY NOT START A PROCESS. What gets launched, with
//  which arguments and when the executable is missing are all decided above
//  this call and asserted against a fake that records it.
//
////////////////////////////////////////////////////////////////////////////////

class IProcessLauncher
{
public:
    virtual ~IProcessLauncher () = default;

    //  Starts `exePath` with `arguments` as its command line, not waiting for
    //  it. The arguments are passed as written; quoting is the caller's.
    virtual HRESULT  Launch (const std::wstring & exePath, const std::wstring & arguments) = 0;

    //  Whether a file is there to launch.
    virtual bool     Exists (const std::wstring & exePath) = 0;
};
