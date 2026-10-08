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
    std::wstring  restartArgs;
    DeployTiming  timing  = DeployTiming::Now;
    int           deploys = 0;

    HRESULT DeployBundle (const std::wstring & bundlePath, DeployTiming deployTiming, const std::wstring & args) override
    {
        deploys++;
        deployedPath = bundlePath;
        timing       = deployTiming;
        restartArgs  = args;
        return result;
    }
};
