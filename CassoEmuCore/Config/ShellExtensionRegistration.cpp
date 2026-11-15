#include "Pch.h"

#include "Config/ShellExtensionRegistration.h"
#include "CassoExplorer/RawCopyDropHandler.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "Seams/Win32UserClasses.h"
#include "Update/AuthenticodeVerifier.h"
#include "Update/InstallTypeDetector.h"
#include "Update/Win32InstallEnvironment.h"





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
//  ShellExtensionRegistration::RegisterForInstall
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellExtensionRegistration::RegisterForInstall (IUserClasses & classes, IInstallEnvironment & environment, ISignatureVerifier & verifier,
                                                        const std::wstring & dllPath)
{
    HRESULT      hr   = S_OK;
    InstallType  type = InstallType::Unknown;



    hr = InstallTypeDetector::Detect (environment, verifier, type);
    CHR (hr);

    if (type == InstallType::Zip && !IsRegistered (classes, dllPath))
    {
        hr = Register (classes, dllPath);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::StartInstallRegistration
//
//  The thread creates everything it touches, so it can run past the window
//  that started it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellExtensionRegistration::StartInstallRegistration()
{
    std::thread ([]()
    {
        Win32UserClasses         classes;
        Win32InstallEnvironment  environment;
        AuthenticodeVerifier     verifier;
        wchar_t                  module[MAX_PATH] = {};
        DWORD                    length           = GetModuleFileNameW (nullptr, module, MAX_PATH);
        HRESULT                  hr               = S_OK;



        if (length > 0 && length < MAX_PATH)
        {
            hr = RegisterForInstall (classes, environment, verifier,
                                     LaunchCommand::GetSiblingPath (std::filesystem::path (module).parent_path().wstring(), kDllName));
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }).detach();
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
