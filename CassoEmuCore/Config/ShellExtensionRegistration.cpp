#include "Pch.h"

#include "Config/ShellExtensionRegistration.h"
#include "CassoExplorer/RawCopyDropHandler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::Register
//
//  The class first, then each folder type's handler entry, so File Explorer
//  never finds an entry for a class that isn't there yet.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellExtensionRegistration::Register (IUserClasses & classes, const std::wstring & dllPath)
{
    HRESULT  hr = S_OK;



    hr = classes.SetValue (GetClassKey(), L"", L"Casso Explorer raw copy");
    CHR (hr);

    hr = classes.SetValue (GetClassKey() + L"\\InprocServer32", L"", dllPath);
    CHR (hr);

    hr = classes.SetValue (GetClassKey() + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    CHR (hr);

    for (const wchar_t * targetType : kTargetTypes)
    {
        hr = classes.SetValue (GetHandlerKey (targetType), L"", GetClsidText());
        CHR (hr);
    }

Error:
    classes.NotifyChanged();
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::Unregister
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellExtensionRegistration::Unregister (IUserClasses & classes)
{
    HRESULT  hr = S_OK;



    for (const wchar_t * targetType : kTargetTypes)
    {
        hr = classes.DeleteKey (GetHandlerKey (targetType));
        CHR (hr);
    }

    hr = classes.DeleteKey (GetClassKey());
    CHR (hr);

Error:
    classes.NotifyChanged();
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::IsRegistered
//
//  Registered for this DLL: a class whose InprocServer32 path is another
//  copy, such as one an earlier build left in another folder, doesn't count.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellExtensionRegistration::IsRegistered (const IUserClasses & classes, const std::wstring & dllPath)
{
    std::wstring  value;
    bool          registered = classes.GetValue (GetClassKey() + L"\\InprocServer32", L"", value) && _wcsicmp (value.c_str(), dllPath.c_str()) == 0;



    for (const wchar_t * targetType : kTargetTypes)
    {
        registered = registered && classes.GetValue (GetHandlerKey (targetType), L"", value) && value == GetClsidText();
    }

    return registered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::GetClassKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellExtensionRegistration::GetClassKey()
{
    return L"CLSID\\" + GetClsidText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::GetClsidText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellExtensionRegistration::GetClsidText()
{
    wchar_t  clsid[64] = {};
    int      written   = StringFromGUID2 (RawCopyDropHandler::kClsid, clsid, ARRAYSIZE (clsid));



    IGNORE_RETURN_VALUE (written, 39);

    return clsid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::GetHandlerKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellExtensionRegistration::GetHandlerKey (const wchar_t * targetType)
{
    return std::wstring (targetType) + L"\\shellex\\DragDropHandlers\\" + kHandlerName;
}
