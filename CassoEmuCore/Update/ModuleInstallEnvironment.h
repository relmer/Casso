#pragma once

#include "Pch.h"

#include "Update/IInstallEnvironment.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleInstallEnvironment
//
//  IInstallEnvironment over the module that contains CassoEmuCore. For Casso,
//  CassoExplorer, CassoCli and the storage card broker that module is the
//  executable. Inside the test host it is the test assembly in the build output
//  folder, so a test run resolves to the checkout that built it rather than to
//  the host's install folder.
//
////////////////////////////////////////////////////////////////////////////////

class ModuleInstallEnvironment : public IInstallEnvironment
{
public:
    bool    HasPackageIdentity () override;
    HRESULT GetExecutablePath  (std::wstring & outPath) override;
};
