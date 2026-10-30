#include "Pch.h"

#include "Config/FileAssociations.h"
#include "CassoExplorer/Model/CatalogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::Register
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::Register (IUserClasses & classes, const std::wstring & cassoExe, const std::wstring & explorerExe)
{
    HRESULT  hr = S_OK;



    for (const wchar_t * extension : kExtensions)
    {
        hr = WriteProgId (classes, GetProgId (extension), CatalogModel::GetDiskImageTypeName (extension), L"Casso", cassoExe,
                          L"\"" + cassoExe + L"\" --disk1 \"%1\"");
        CHR (hr);
    }

    //  The single type an earlier build registered goes.
    hr = classes.DeleteKey (kCassoProgId);
    CHR (hr);

    hr = classes.SetValue (std::wstring (L"Applications\\") + kExplorerApp, L"FriendlyAppName", L"Casso Explorer");
    CHR (hr);

    hr = classes.SetValue (std::wstring (L"Applications\\") + kExplorerApp + L"\\shell\\open\\command", L"", L"\"" + explorerExe + L"\" \"%1\"");
    CHR (hr);

    for (const wchar_t * extension : kExtensions)
    {
        hr = TakeType (classes, extension);
        CHR (hr);
    }

Error:
    classes.NotifyChanged();
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::WriteProgId
//
//  A type's name, its icon from Casso's own resources, and its Open command,
//  with the program's name for Open with.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::WriteProgId (IUserClasses & classes, const std::wstring & progId, const std::wstring & typeName,
                                       const std::wstring & appName, const std::wstring & iconExe, const std::wstring & command)
{
    HRESULT  hr = S_OK;



    hr = classes.SetValue (progId, L"", typeName);
    CHR (hr);

    hr = classes.SetValue (progId + L"\\DefaultIcon", L"", L"\"" + iconExe + L"\",-" + std::to_wstring (kDiskIconId));
    CHR (hr);

    hr = classes.SetValue (progId + L"\\shell\\open", L"FriendlyAppName", appName);
    CHR (hr);

    hr = classes.SetValue (progId + L"\\shell\\open\\command", L"", command);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::TakeType
//
//  The program the type had is kept beside it, once, so registering twice
//  does not lose it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::TakeType (IUserClasses & classes, const std::wstring & extension)
{
    HRESULT       hr       = S_OK;
    std::wstring  previous;
    bool          had      = classes.GetValue (extension, L"", previous);



    if (had && !IsCassoType (extension, previous))
    {
        hr = classes.SetValue (extension, kPreviousName, previous);
        CHR (hr);
    }

    hr = classes.SetValue (extension, L"", GetProgId (extension));
    CHR (hr);

    hr = classes.DeleteValue (extension + L"\\OpenWithProgids", kCassoProgId);
    CHR (hr);

    hr = classes.SetValue (extension + L"\\OpenWithProgids", GetProgId (extension), L"");
    CHR (hr);

    hr = classes.SetValue (extension + L"\\OpenWithList\\" + kExplorerApp, L"", L"");
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::Unregister
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::Unregister (IUserClasses & classes, IUserClasses & fileExts)
{
    HRESULT  hr = S_OK;



    for (const wchar_t * extension : kExtensions)
    {
        hr = ReturnType (classes, extension);
        CHR (hr);

        hr = ClearRecord (fileExts, extension);
        CHR (hr);
    }

    for (const wchar_t * extension : kExtensions)
    {
        hr = classes.DeleteKey (GetProgId (extension));
        CHR (hr);
    }

    hr = classes.DeleteKey (kCassoProgId);
    CHR (hr);

    hr = classes.DeleteKey (kExplorerProgId);
    CHR (hr);

    hr = classes.DeleteKey (std::wstring (L"Applications\\") + kExplorerApp);
    CHR (hr);

Error:
    classes.NotifyChanged();
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::ReturnType
//
//  The type goes back to the program it had, or to none, and a key Register
//  made that is left empty goes too.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::ReturnType (IUserClasses & classes, const std::wstring & extension)
{
    HRESULT       hr       = S_OK;
    std::wstring  current;
    std::wstring  previous;
    std::wstring  withKey  = extension + L"\\OpenWithProgids";
    std::wstring  listKey  = extension + L"\\OpenWithList";



    if (classes.GetValue (extension, L"", current) && IsCassoType (extension, current))
    {
        hr = classes.GetValue (extension, kPreviousName, previous) ? classes.SetValue (extension, L"", previous)
                                                                   : classes.DeleteValue (extension, L"");
        CHR (hr);
    }

    hr = classes.DeleteValue (extension, kPreviousName);
    CHR (hr);

    hr = classes.DeleteValue (withKey, kCassoProgId);
    CHR (hr);

    hr = classes.DeleteValue (withKey, GetProgId (extension));
    CHR (hr);

    hr = classes.DeleteValue (withKey, kExplorerProgId);
    CHR (hr);

    if (classes.IsEmpty (withKey))
    {
        hr = classes.DeleteKey (withKey);
        CHR (hr);
    }

    hr = classes.DeleteKey (listKey + L"\\" + kExplorerApp);
    CHR (hr);

    if (classes.IsEmpty (listKey))
    {
        hr = classes.DeleteKey (listKey);
        CHR (hr);
    }

    if (classes.IsEmpty (extension))
    {
        hr = classes.DeleteKey (extension);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::ClearRecord
//
//  File Explorer lists each type an extension had under its OpenWithProgids
//  record; Casso's entries go, and the key goes when they were its only
//  values. The programs the user picked in Open with are the user's own.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::ClearRecord (IUserClasses & fileExts, const std::wstring & extension)
{
    HRESULT       hr      = S_OK;
    std::wstring  withKey = extension + L"\\OpenWithProgids";



    hr = fileExts.DeleteValue (withKey, kCassoProgId);
    CHR (hr);

    hr = fileExts.DeleteValue (withKey, GetProgId (extension));
    CHR (hr);

    hr = fileExts.DeleteValue (withKey, kExplorerProgId);
    CHR (hr);

    if (fileExts.IsEmpty (withKey))
    {
        hr = fileExts.DeleteKey (withKey);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::IsRegistered
//
//  Every type is Casso's, and its icon and both commands are these
//  programs', so a registration a copy elsewhere made does not count.
//
////////////////////////////////////////////////////////////////////////////////

bool FileAssociations::IsRegistered (const IUserClasses & classes, const std::wstring & cassoExe, const std::wstring & explorerExe)
{
    std::wstring  value;



    if (!classes.GetValue (std::wstring (L"Applications\\") + kExplorerApp + L"\\shell\\open\\command", L"", value) || value != L"\"" + explorerExe + L"\" \"%1\"")
    {
        return false;
    }

    for (const wchar_t * extension : kExtensions)
    {
        std::wstring  progId = GetProgId (extension);

        if (!classes.GetValue (extension, L"", value) || value != progId)
        {
            return false;
        }

        if (!classes.GetValue (progId + L"\\shell\\open\\command", L"", value) || value != L"\"" + cassoExe + L"\" --disk1 \"%1\"")
        {
            return false;
        }

        if (!classes.GetValue (progId + L"\\DefaultIcon", L"", value) || value != L"\"" + cassoExe + L"\",-" + std::to_wstring (kDiskIconId))
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::IsCassoType
//
//  The extension's type, or the single type an earlier build registered.
//
////////////////////////////////////////////////////////////////////////////////

bool FileAssociations::IsCassoType (const std::wstring & extension, const std::wstring & progId)
{
    return progId == GetProgId (extension) || progId == kCassoProgId;
}
