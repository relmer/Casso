#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Picker/PickerPrefs.h"
#include "CassoExplorer/Picker/PickerRequest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PickerPrefs::GetFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PickerPrefs::GetFilePath (const std::wstring & baseDir, const std::wstring & viewKey)
{
    std::wstring  path = baseDir;
    std::wstring  key;



    for (wchar_t ch : viewKey)
    {
        if (iswalnum (ch) || ch == L'-' || ch == L'_')
        {
            key += ch;
        }
    }

    if (key.empty())
    {
        key = kDefaultKey;
    }

    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
    {
        path += L'\\';
    }

    return path + kFilePrefix + L"-" + key + L".json";
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerPrefs::Scrub
//
//  A drive's picker always starts on the Casso node's list, so its location
//  is not saved.
//
////////////////////////////////////////////////////////////////////////////////

void PickerPrefs::Scrub (const PickerRequest & request, CassoExplorerPrefs & prefs)
{
    prefs.links.clear();

    if (request.IsForDrive())
    {
        prefs.tabs.clear();
    }
    else if (prefs.tabs.size() > 1)
    {
        prefs.tabs.resize (1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerPrefs::Load
//
////////////////////////////////////////////////////////////////////////////////

void PickerPrefs::Load (const std::wstring & baseDir, IFileSystem & fs, const PickerRequest & request, const std::string & theme, CassoExplorerPrefs & outPrefs)
{
    std::wstring  path    = GetFilePath (baseDir, request.viewKey);
    bool          isSaved = fs.Exists (path);
    HRESULT       hr      = outPrefs.LoadFile (path, fs);



    if (FAILED (hr))
    {
        outPrefs = CassoExplorerPrefs();
        isSaved  = false;
    }

    //  A picker opens as a list. The preview pane is one click away, and
    //  the choice is saved with the view.
    if (!isSaved)
    {
        outPrefs.previewVisible = false;
        outPrefs.autoPreview    = false;
    }

    Scrub (request, outPrefs);

    if (CassoExplorerPrefs::IsKnownTheme (theme))
    {
        outPrefs.theme = theme;
    }

    //  The Casso node is open in the tree, where its folders and the stock
    //  disks are.
    if (request.IsForDrive())
    {
        outPrefs.treeCassoOpen = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerPrefs::Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PickerPrefs::Save (const std::wstring & baseDir, IFileSystem & fs, const PickerRequest & request, const CassoExplorerPrefs & prefs)
{
    CassoExplorerPrefs  saved = prefs;



    Scrub (request, saved);

    return saved.SaveFile (GetFilePath (baseDir, request.viewKey), fs);
}
