#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IInstallEnvironment
//
//  What the install-type decision reads from the running process: whether
//  it has package identity (it was installed from the MSIX), and the path of
//  its executable.
//
////////////////////////////////////////////////////////////////////////////////

class IInstallEnvironment
{
public:
    virtual ~IInstallEnvironment() = default;

    virtual bool    HasPackageIdentity() = 0;
    virtual HRESULT GetExecutablePath  (std::wstring & outPath) = 0;
};
