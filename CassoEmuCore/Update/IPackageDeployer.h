#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeployTiming
//
//  When a deployed bundle replaces the running package:
//
//    Now         Windows shuts the running Casso down and restarts it on the
//                new version, with `restartArgs`
//    WhenClosed  registration waits until Casso is no longer running; the
//                running copy is left alone
//
////////////////////////////////////////////////////////////////////////////////

enum class DeployTiming
{
    Now,
    WhenClosed,
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPackageDeployer
//
//  Installs a downloaded .msixbundle over the running package. Deployed Now,
//  a successful call may never return.
//
////////////////////////////////////////////////////////////////////////////////

class IPackageDeployer
{
public:
    virtual ~IPackageDeployer() = default;

    virtual HRESULT DeployBundle (const std::wstring  & bundlePath,
                                  DeployTiming          timing,
                                  const std::wstring  & restartArgs) = 0;
};
