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
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellExtensionRegistration::Register (IUserClasses & classes, const std::wstring & dllPath)
{
    return RegisterClass (classes, dllPath, RawCopyDropHandler::kClsid, kHandlerName, L"Casso Explorer raw copy");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::RegisterDeveloper
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellExtensionRegistration::RegisterDeveloper (IUserClasses & classes, const std::wstring & dllPath)
{
    return RegisterClass (classes, dllPath, RawCopyDropHandler::kDevClsid, kDevHandlerName, L"Casso Explorer raw copy (developer build)");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::RegisterClass
//
//  The class first, then each folder type's handler entry, so File Explorer
//  never finds an entry for a class that isn't there yet.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellExtensionRegistration::RegisterClass (IUserClasses & classes, const std::wstring & dllPath, REFCLSID clsid, const wchar_t * handlerName, const wchar_t * description)
{
    HRESULT  hr = S_OK;



    hr = classes.SetValue (GetClassKey (clsid), L"", description);
    CHR (hr);

    hr = classes.SetValue (GetClassKey (clsid) + L"\\InprocServer32", L"", dllPath);
    CHR (hr);

    hr = classes.SetValue (GetClassKey (clsid) + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    CHR (hr);

    for (const wchar_t * targetType : kTargetTypes)
    {
        hr = classes.SetValue (GetHandlerKey (targetType, handlerName), L"", GetClsidText (clsid));
        CHR (hr);
    }

Error:
    classes.NotifyChanged();
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::RegisterDeveloperCopy
//
//  Each copy has a new name, so a copy File Explorer has loaded is never
//  overwritten. A copy that can't be deleted is loaded in File Explorer;
//  a later registration deletes it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellExtensionRegistration::RegisterDeveloperCopy (std::wstring & outCopy)
{
    HRESULT           hr               = S_OK;
    Win32UserClasses  classes;
    wchar_t           module[MAX_PATH] = {};
    DWORD             length           = GetModuleFileNameW (nullptr, module, MAX_PATH);
    PWSTR             localAppData     = nullptr;
    std::error_code   error;
    fs::path          folder;
    fs::path          copy;
    SYSTEMTIME        now              = {};



    outCopy.clear();
    CBREx (length > 0 && length < MAX_PATH, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    hr = SHGetKnownFolderPath (FOLDERID_LocalAppData, 0, nullptr, &localAppData);
    CHR (hr);

    GetLocalTime (&now);
    folder = fs::path (localAppData) / kDevCopyFolder;
    copy   = folder / std::format (L"CassoShellExtension-{:04}{:02}{:02}-{:02}{:02}{:02}.dll", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

    fs::create_directories (folder, error);
    CBREx (!error, HRESULT_FROM_WIN32 (error.value()));

    fs::copy_file (LaunchCommand::GetSiblingPath (fs::path (module).parent_path().wstring(), kDllName), copy, fs::copy_options::overwrite_existing, error);
    CBREx (!error, HRESULT_FROM_WIN32 (error.value()));

    hr = RegisterDeveloper (classes, copy.wstring());
    CHR (hr);

    for (const fs::directory_entry & entry : fs::directory_iterator (folder, error))
    {
        if (entry.path() != copy)
        {
            std::error_code  ignored;

            fs::remove (entry.path(), ignored);
        }
    }

    outCopy = copy.wstring();

Error:
    CoTaskMemFree (localAppData);
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

std::wstring ShellExtensionRegistration::GetClassKey (REFCLSID clsid)
{
    return L"CLSID\\" + GetClsidText (clsid);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::GetClsidText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellExtensionRegistration::GetClsidText (REFCLSID clsid)
{
    wchar_t  text[64] = {};
    int      written  = StringFromGUID2 (clsid, text, ARRAYSIZE (text));



    IGNORE_RETURN_VALUE (written, 39);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration::GetHandlerKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellExtensionRegistration::GetHandlerKey (const wchar_t * targetType, const wchar_t * handlerName)
{
    return std::wstring (targetType) + L"\\shellex\\DragDropHandlers\\" + handlerName;
}
