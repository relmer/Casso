#pragma once

#include "Pch.h"
#include "Seams/IProcessLauncher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeProcessLauncher
//
//  Records every launch instead of starting anything, and answers Exists from
//  a list the test fills.
//
////////////////////////////////////////////////////////////////////////////////

class FakeProcessLauncher : public IProcessLauncher
{
public:
    struct Launched
    {
        std::wstring  exePath;
        std::wstring  arguments;
    };

    std::vector<Launched>      launched;
    std::vector<std::wstring>  present;
    HRESULT                    nextResult = S_OK;

    HRESULT  Launch (const std::wstring & exePath, const std::wstring & arguments) override
    {
        launched.push_back (Launched { exePath, arguments });

        return nextResult;
    }

    bool  Exists (const std::wstring & exePath) override
    {
        for (const std::wstring & path : present)
        {
            if (_wcsicmp (path.c_str(), exePath.c_str()) == 0)
            {
                return true;
            }
        }

        return false;
    }
};
