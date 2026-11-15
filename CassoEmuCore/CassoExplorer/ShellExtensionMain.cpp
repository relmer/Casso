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

STDAPI DllGetClassObject (REFCLSID clsid, REFIID riid, void ** ppv)
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

STDAPI DllCanUnloadNow()
{
    return (RawCopyDropHandler::GetObjectCount() == 0) ? S_OK : S_FALSE;
}
