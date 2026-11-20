#include "Pch.h"

#include "Update/ModuleInstallEnvironment.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleInstallEnvironment::HasPackageIdentity
//
//  Package identity belongs to the process, so the answer is the one
//  Win32InstallEnvironment gives.
//
////////////////////////////////////////////////////////////////////////////////

bool ModuleInstallEnvironment::HasPackageIdentity()
{
    UINT32  length = 0;
    LONG    status = GetCurrentPackageFullName (&length, nullptr);



    return status != APPMODEL_ERROR_NO_PACKAGE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleInstallEnvironment::GetExecutablePath
//
//  The full path of the module that contains this function, found from an
//  address inside it. The reference count is not changed, so the module needs
//  no release. The buffer grows until the path fits.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ModuleInstallEnvironment::GetExecutablePath (std::wstring & outPath)
{
    static constexpr DWORD  kInitialChars = MAX_PATH;
    static constexpr DWORD  kMaxChars     = 32768;
    static constexpr DWORD  kFlags        = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    static const wchar_t    kchAnchor     = 0;
    HRESULT                 hr            = S_OK;
    HMODULE                 module        = nullptr;
    DWORD                   capacity      = kInitialChars;
    DWORD                   length        = 0;
    BOOL                    found         = FALSE;



    outPath.clear();

    found = GetModuleHandleExW (kFlags, &kchAnchor, &module);
    CWR (found);

    while (true)
    {
        outPath.resize (capacity);

        length = GetModuleFileNameW (module, outPath.data(), capacity);
        CWR (length != 0);

        if (length < capacity)
        {
            break;
        }

        CBREx (capacity < kMaxChars, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));
        capacity *= 2;
    }

    outPath.resize (length);

Error:
    if (FAILED (hr))
    {
        outPath.clear();
    }

    return hr;
}
