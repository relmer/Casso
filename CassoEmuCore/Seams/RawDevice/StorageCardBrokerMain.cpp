#include "Pch.h"

#include "Seams/RawDevice/Win32StorageCardBroker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  wCassoStorageCardBrokerMain
//
//  CassoStorageCardBroker's process entry point. The process exit code is
//  the HRESULT the broker ends with, which Casso Explorer reports when the
//  broker exits before it connects.
//
////////////////////////////////////////////////////////////////////////////////

extern "C" int WINAPI wCassoStorageCardBrokerMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    UNREFERENCED_PARAMETER (hInstance);
    UNREFERENCED_PARAMETER (hPrevInstance);
    UNREFERENCED_PARAMETER (lpCmdLine);
    UNREFERENCED_PARAMETER (nCmdShow);

    return (int) Win32StorageCardBroker::Run (GetCommandLineW());
}