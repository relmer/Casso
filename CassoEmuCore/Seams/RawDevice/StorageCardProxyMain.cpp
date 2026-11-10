#include "Pch.h"

#include "Seams/RawDevice/Win32StorageCardProxy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  wCassoStorageCardProxyMain
//
//  CassoStorageCardProxy's process entry point. The process exit code is
//  the HRESULT the proxy ends with, which Casso Explorer reports when the
//  proxy exits before it connects.
//
////////////////////////////////////////////////////////////////////////////////

extern "C" int WINAPI wCassoStorageCardProxyMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    UNREFERENCED_PARAMETER (hInstance);
    UNREFERENCED_PARAMETER (hPrevInstance);
    UNREFERENCED_PARAMETER (lpCmdLine);
    UNREFERENCED_PARAMETER (nCmdShow);

    return (int) Win32StorageCardProxy::Run (GetCommandLineW());
}