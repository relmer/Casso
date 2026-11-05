#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ICassoLauncher
//
//  Starting Casso for the MCP server's `launch` tool, and the clock it waits
//  by. A seam so a test can substitute a process that never starts, or a
//  channel that opens on the third look, without running Casso.
//
////////////////////////////////////////////////////////////////////////////////

class ICassoLauncher
{
public:
    virtual ~ICassoLauncher() = default;

    //  Casso.exe from CassoCli's folder with these arguments: its process id,
    //  or 0 with `error` set when it didn't start.
    virtual uint32_t  Launch    (const std::wstring & arguments, std::string & error) = 0;

    virtual bool      IsRunning (uint32_t processId) = 0;
    virtual void      Wait      (DWORD milliseconds) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32CassoLauncher
//
////////////////////////////////////////////////////////////////////////////////

class Win32CassoLauncher : public ICassoLauncher
{
public:
    uint32_t  Launch    (const std::wstring & arguments, std::string & error) override;
    bool      IsRunning (uint32_t processId) override;
    void      Wait      (DWORD milliseconds) override { Sleep (milliseconds); }

    //  The Casso.exe beside the running executable.
    static std::wstring  GetCassoPath ();
};
