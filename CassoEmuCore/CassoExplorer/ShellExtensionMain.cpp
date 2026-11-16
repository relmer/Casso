#include "Pch.h"

#include "CassoExplorer/RawCopyDropHandler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DllGetClassObject
//
//  CassoShellExtension.dll's class factory, exported by its .def file.
//  CassoShellExtension.vcxproj doesn't compile any code, as
//  CassoExplorer.vcxproj doesn't; this file is linked into the DLL from
//  CassoEmuCore.lib.
//
////////////////////////////////////////////////////////////////////////////////

_Check_return_
STDAPI DllGetClassObject (_In_ REFCLSID clsid, _In_ REFIID riid, _Outptr_ LPVOID FAR * ppv)
{
    HRESULT  hr = S_OK;



    CBREx (ppv != nullptr, E_POINTER);

    *ppv = nullptr;
    CBREx (clsid == RawCopyDropHandler::kClsid, CLASS_E_CLASSNOTAVAILABLE);

    hr = RawCopyDropHandler::CreateClassFactory (riid, ppv);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DllCanUnloadNow
//
//  File Explorer unloads the DLL once no handler or factory is alive, so a
//  rebuild can replace it without restarting File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

__control_entrypoint (DllExport)
STDAPI DllCanUnloadNow()
{
    return (RawCopyDropHandler::GetObjectCount() == 0) ? S_OK : S_FALSE;
}
