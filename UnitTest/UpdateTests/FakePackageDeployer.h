#pragma once

#include "Pch.h"

#include "Update/IPackageDeployer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakePackageDeployer
//
////////////////////////////////////////////////////////////////////////////////

class FakePackageDeployer : public IPackageDeployer
{
public:
    HRESULT       result  = S_OK;
    std::wstring  deployedPath;
    int           deploys = 0;

    HRESULT DeployBundle (const std::wstring & bundlePath) override
    {
        deploys++;
        deployedPath = bundlePath;
        return result;
    }
};

