#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IPackageDeployer
//
//  Installs a downloaded .msixbundle over the running package. On success
//  Windows shuts the running Casso down and restarts it on the new version,
//  so a successful call may never return.
//
////////////////////////////////////////////////////////////////////////////////

class IPackageDeployer
{
public:
    virtual ~IPackageDeployer() = default;

    virtual HRESULT DeployBundle (const std::wstring & bundlePath) = 0;
};
