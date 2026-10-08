#include "Pch.h"

#include "Config/FileAssociations.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::Register
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::Register (IUserClasses & classes, const std::wstring & cassoExe, const std::wstring & explorerExe)
{
    HRESULT  hr = S_OK;



    hr = WriteProgId (classes, kCassoProgId, L"Casso", cassoExe, L"\"" + cassoExe + L"\" --disk1 \"%1\"");
    CHR (hr);

    hr = WriteProgId (classes, kExplorerProgId, L"Casso Explorer", cassoExe, L"\"" + explorerExe + L"\" \"%1\"");
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

HRESULT FileAssociations::WriteProgId (IUserClasses & classes, const std::wstring & progId, const std::wstring & appName,
                                       const std::wstring & iconExe, const std::wstring & command)
{
    HRESULT  hr = S_OK;



    hr = classes.SetValue (progId, L"", L"Apple II disk image");
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



    if (had && previous != kCassoProgId)
    {
        hr = classes.SetValue (extension, kPreviousName, previous);
        CHR (hr);
    }

    hr = classes.SetValue (extension, L"", kCassoProgId);
    CHR (hr);

    hr = classes.SetValue (extension + L"\\OpenWithProgids", kCassoProgId, L"");
    CHR (hr);

    hr = classes.SetValue (extension + L"\\OpenWithProgids", kExplorerProgId, L"");
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations::Unregister
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileAssociations::Unregister (IUserClasses & classes)
{
    HRESULT  hr = S_OK;



    for (const wchar_t * extension : kExtensions)
    {
        hr = ReturnType (classes, extension);
        CHR (hr);
    }

    hr = classes.DeleteKey (kCassoProgId);
    CHR (hr);

    hr = classes.DeleteKey (kExplorerProgId);
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



    if (classes.GetValue (extension, L"", current) && current == kCassoProgId)
    {
        hr = classes.GetValue (extension, kPreviousName, previous) ? classes.SetValue (extension, L"", previous)
                                                                   : classes.DeleteValue (extension, L"");
        CHR (hr);
    }

    hr = classes.DeleteValue (extension, kPreviousName);
    CHR (hr);

    hr = classes.DeleteValue (withKey, kCassoProgId);
    CHR (hr);

    hr = classes.DeleteValue (withKey, kExplorerProgId);
    CHR (hr);

    if (classes.IsEmpty (withKey))
    {
        hr = classes.DeleteKey (withKey);
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
//  FileAssociations::IsRegistered
//
//  Every type is Casso's.
//
////////////////////////////////////////////////////////////////////////////////

bool FileAssociations::IsRegistered (const IUserClasses & classes)
{
    for (const wchar_t * extension : kExtensions)
    {
        std::wstring  current;

        if (!classes.GetValue (extension, L"", current) || current != kCassoProgId)
        {
            return false;
        }
    }

    return true;
}
