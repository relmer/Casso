#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IProcessProbe
//
//  Whether a process is running, by its id and the time it started, so a
//  process id the system has given to a new process is not taken for the
//  old one. Win32ProcessProbe reads it from the system; tests substitute a
//  table.
//
////////////////////////////////////////////////////////////////////////////////

class IProcessProbe
{
public:
    virtual ~IProcessProbe() = default;

    //  The process's start time as a FILETIME, or false when no process has
    //  that id.
    virtual bool  TryGetStartTime (DWORD pid, uint64_t & outStarted) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ProcessProbe
//
////////////////////////////////////////////////////////////////////////////////

class Win32ProcessProbe : public IProcessProbe
{
public:
    bool  TryGetStartTime (DWORD pid, uint64_t & outStarted) override;
};
