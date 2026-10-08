#pragma once

#include "Pch.h"

#include "Update/IInstallEnvironment.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeInstallEnvironment
//
////////////////////////////////////////////////////////////////////////////////

class FakeInstallEnvironment : public IInstallEnvironment
{
public:
    bool          isPackaged = false;
    HRESULT       pathResult = S_OK;
    std::wstring  exePath    = L"C:\\Casso\\Casso.exe";
    int           pathCalls  = 0;

    bool HasPackageIdentity() override
    {
        return isPackaged;
    }

    HRESULT GetExecutablePath (std::wstring & outPath) override
    {
        pathCalls++;
        outPath = exePath;
        return pathResult;
    }
};
