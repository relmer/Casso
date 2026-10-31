#pragma once

#include "Pch.h"

#include "Update/IInstallEnvironment.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InstallEnvironment
//
//  IInstallEnvironment for the running process.
//
////////////////////////////////////////////////////////////////////////////////

class Win32InstallEnvironment : public IInstallEnvironment
{
public:
    bool    HasPackageIdentity() override;
    HRESULT GetExecutablePath  (std::wstring & outPath) override;
};
