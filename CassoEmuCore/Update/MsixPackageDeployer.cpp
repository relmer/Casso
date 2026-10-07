#include "Pch.h"

#include "Update/MsixPackageDeployer.h"





using Microsoft::WRL::Callback;
using Microsoft::WRL::Wrappers::Event;
using Microsoft::WRL::Wrappers::HStringReference;
using Microsoft::WRL::Wrappers::RoInitializeWrapper;

namespace AbiFoundation = ABI::Windows::Foundation;
namespace AbiDeployment = ABI::Windows::Management::Deployment;





////////////////////////////////////////////////////////////////////////////////
//
//  MsixPackageDeployer::DeployBundle
//
//  From a file:// URI to the bundle already downloaded and verified. Now is
//  PackageManager.AddPackageAsync with ForceTargetApplicationShutdown, after
//  registering Casso to restart with `restartArgs`; the registration is
//  withdrawn again if the deployment fails, so a later crash does not
//  relaunch Casso with --updated. WhenClosed defers the registration until
//  Casso is no longer running and registers no restart.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MsixPackageDeployer::DeployBundle (
    const std::wstring  & bundlePath,
    DeployTiming          timing,
    const std::wstring  & restartArgs)
{
    HRESULT                                  hr             = S_OK;
    HRESULT                                  hrUnregister   = S_OK;
    RoInitializeWrapper                      roInit         (RO_INIT_MULTITHREADED);
    ComPtr<IInspectable>                     inspectable;
    ComPtr<AbiDeployment::IPackageManager>   packageManager;
    ComPtr<AbiFoundation::IUriRuntimeClass>  uri;
    ComPtr<DeployOperation>                  operation;
    bool                                     isRegistered   = false;



    hr = roInit;
    CHR (hr);

    if (timing == DeployTiming::Now)
    {
        hr = RegisterApplicationRestart (restartArgs.c_str(), 0);
        CHR (hr);

        isRegistered = true;
    }

    hr = RoActivateInstance (HStringReference (RuntimeClass_Windows_Management_Deployment_PackageManager).Get(),
                             &inspectable);
    CHR (hr);

    hr = inspectable.As (&packageManager);
    CHRA (hr);

    hr = CreateUri (MakeFileUri (bundlePath), &uri);
    CHR (hr);

    if (timing == DeployTiming::WhenClosed)
    {
        hr = AddWhenClosed (inspectable.Get(), uri.Get(), &operation);
        CHR (hr);
    }
    else
    {
        hr = packageManager->AddPackageAsync (uri.Get(),
                                              nullptr,
                                              AbiDeployment::DeploymentOptions_ForceTargetApplicationShutdown,
                                              &operation);
        CHR (hr);
    }

    hr = WaitForDeploy (operation.Get());
    CHR (hr);

Error:
    if (FAILED (hr) && isRegistered)
    {
        hrUnregister = UnregisterApplicationRestart();
        IGNORE_RETURN_VALUE (hrUnregister, S_OK);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MsixPackageDeployer::MakeFileUri
//
//  "C:\a b\x.msixbundle" to "file:///C:/a%20b/x.msixbundle".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MsixPackageDeployer::MakeFileUri (const std::wstring & path)
{
    std::wstring  uri = L"file:///";



    for (wchar_t ch : path)
    {
        switch (ch)
        {
            case L'\\':
                uri += L'/';
                break;

            case L'%':
                uri += L"%25";
                break;

            case L' ':
                uri += L"%20";
                break;

            case L'#':
                uri += L"%23";
                break;

            default:
                uri += ch;
                break;
        }
    }

    return uri;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MsixPackageDeployer::CreateUri
//
//  A Windows.Foundation.Uri for `uriText`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MsixPackageDeployer::CreateUri (const std::wstring & uriText, AbiFoundation::IUriRuntimeClass ** ppUri)
{
    HRESULT                                         hr = S_OK;
    ComPtr<AbiFoundation::IUriRuntimeClassFactory>  factory;



    hr = RoGetActivationFactory (HStringReference (RuntimeClass_Windows_Foundation_Uri).Get(),
                                 IID_PPV_ARGS (&factory));
    CHR (hr);

    hr = factory->CreateUri (HStringReference (uriText.c_str(), (unsigned int) uriText.size()).Get(), ppUri);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MsixPackageDeployer::WaitForDeploy
//
//  Blocks until the deployment completes, for at most the timeout below.
//  A successful deployment of Casso's own package usually ends this process
//  before the wait returns. Otherwise the operation's error, or the
//  deployment result's extended error, is the result.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MsixPackageDeployer::WaitForDeploy (DeployOperation * operation)
{
    static constexpr DWORD                    kMsPerMinute    = 60000;
    static constexpr DWORD                    kTimeoutMinutes = 10;
    static constexpr DWORD                    kTimeoutMs      = kTimeoutMinutes * kMsPerMinute;
    HRESULT                                   hr              = S_OK;
    HRESULT                                   hrExtended      = S_OK;
    Event                                     completed;
    ComPtr<CompletedHandler>                  handler;
    ComPtr<AbiFoundation::IAsyncInfo>         asyncInfo;
    ComPtr<AbiDeployment::IDeploymentResult>  result;
    AbiFoundation::AsyncStatus                status          = AbiFoundation::AsyncStatus::Started;
    DWORD                                     wait            = 0;
    bool                                      isValid         = false;
    bool                                      hasHandler      = false;



    completed.Attach (CreateEventExW (nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));

    isValid = completed.IsValid();
    CWR (isValid);

    handler = Callback<CompletedHandler> (
        [&completed] (DeployOperation *, AbiFoundation::AsyncStatus) -> HRESULT
        {
            SetEvent (completed.Get());
            return S_OK;
        });
    hasHandler = handler != nullptr;
    CBREx (hasHandler, E_OUTOFMEMORY);

    hr = operation->put_Completed (handler.Get());
    CHR (hr);

    wait = WaitForSingleObject (completed.Get(), kTimeoutMs);
    CBREx (wait == WAIT_OBJECT_0, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

    hr = operation->QueryInterface (IID_PPV_ARGS (&asyncInfo));
    CHRA (hr);

    hr = asyncInfo->get_Status (&status);
    CHRA (hr);

    if (status != AbiFoundation::AsyncStatus::Completed)
    {
        hrExtended = E_FAIL;
        hr         = asyncInfo->get_ErrorCode (&hrExtended);
        CHRA (hr);

        hr = FAILED (hrExtended) ? hrExtended : E_FAIL;
        CHR (hr);
    }

    hr = operation->GetResults (&result);
    CHR (hr);

    hr = result->get_ExtendedErrorCode (&hrExtended);
    CHRA (hr);

    hr = hrExtended;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MsixPackageDeployer::AddWhenClosed
//
//  AddPackageByUriAsync with DeferRegistrationWhenPackagesAreInUse: the new
//  version is staged now and registered by Windows once Casso has exited,
//  without shutting the running copy down.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MsixPackageDeployer::AddWhenClosed (
    IInspectable                     * packageManager,
    AbiFoundation::IUriRuntimeClass  * uri,
    DeployOperation                 ** ppOperation)
{
    HRESULT                                    hr = S_OK;
    ComPtr<IInspectable>                       inspectable;
    ComPtr<AbiDeployment::IPackageManager9>    manager9;
    ComPtr<AbiDeployment::IAddPackageOptions>  options;



    hr = packageManager->QueryInterface (IID_PPV_ARGS (&manager9));
    CHR (hr);

    hr = RoActivateInstance (HStringReference (RuntimeClass_Windows_Management_Deployment_AddPackageOptions).Get(), &inspectable);
    CHR (hr);

    hr = inspectable.As (&options);
    CHRA (hr);

    hr = options->put_DeferRegistrationWhenPackagesAreInUse (TRUE);
    CHR (hr);

    hr = manager9->AddPackageByUriAsync (uri, options.Get(), ppOperation);
    CHR (hr);

Error:
    return hr;
}