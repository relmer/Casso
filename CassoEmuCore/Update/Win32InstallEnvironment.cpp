#include "Pch.h"

#include "Update/Win32InstallEnvironment.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InstallEnvironment::HasPackageIdentity
//
//  A process started from an installed package has a package full name;
//  every other process gets APPMODEL_ERROR_NO_PACKAGE. The length query is
//  enough to tell which.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32InstallEnvironment::HasPackageIdentity()
{
    UINT32  length = 0;
    LONG    status = GetCurrentPackageFullName (&length, nullptr);



    return status != APPMODEL_ERROR_NO_PACKAGE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InstallEnvironment::GetExecutablePath
//
//  The running executable's full path, growing the buffer until it fits.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32InstallEnvironment::GetExecutablePath (std::wstring & outPath)
{
    static constexpr DWORD  kInitialChars = MAX_PATH;
    static constexpr DWORD  kMaxChars     = 32768;
    HRESULT                 hr            = S_OK;
    DWORD                   capacity      = kInitialChars;
    DWORD                   length        = 0;



    outPath.clear();

    while (true)
    {
        outPath.resize (capacity);

        length = GetModuleFileNameW (nullptr, outPath.data(), capacity);
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
