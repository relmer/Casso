#pragma once

#include "Pch.h"

#include "Update/IPackageDeployer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MsixPackageDeployer
//
//  IPackageDeployer over the Windows Runtime PackageManager, called through
//  the ABI headers with WRL. Registers Casso for restart first, so Windows
//  relaunches it on the new version after shutting it down. Runs on a
//  worker thread, which it joins to the multithreaded apartment.
//
////////////////////////////////////////////////////////////////////////////////

class MsixPackageDeployer : public IPackageDeployer
{
public:
    static constexpr LPCWSTR  kpszRestartArgs = L"--updated";

    HRESULT              DeployBundle  (const std::wstring & bundlePath) override;

    static std::wstring  MakeFileUri   (const std::wstring & path);

private:
    using DeployResult     = ABI::Windows::Management::Deployment::DeploymentResult;
    using DeployProgress   = ABI::Windows::Management::Deployment::DeploymentProgress;
    using DeployOperation  = ABI::Windows::Foundation::IAsyncOperationWithProgress<DeployResult *, DeployProgress>;
    using CompletedHandler = ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<DeployResult *, DeployProgress>;

    static HRESULT       CreateUri     (const std::wstring & uriText, ABI::Windows::Foundation::IUriRuntimeClass ** ppUri);
    static HRESULT       WaitForDeploy (DeployOperation * operation);
};
