#include "Pch.h"

#include "Seams/Win32HostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Configure
//
//  The start folder is best effort: one that cannot be made into a shell
//  item just means the dialog opens wherever the shell last left it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostDialogs::Configure (IFileDialog * dialog, const FileDialogSpec & spec)
{
    HRESULT                         hr       = S_OK;
    HRESULT                         hrFolder = S_OK;
    std::vector<COMDLG_FILTERSPEC>  filters;
    ComPtr<IShellItem>              folderItem;



    for (const FileDialogFilter & filter : spec.filters)
    {
        filters.push_back ({ filter.name.c_str(), filter.pattern.c_str() });
    }

    if (!filters.empty())
    {
        hr = dialog->SetFileTypes ((UINT) filters.size(), filters.data());
        CHR (hr);
    }

    if (!spec.defaultExtension.empty())
    {
        hr = dialog->SetDefaultExtension (spec.defaultExtension.c_str());
        CHR (hr);
    }

    if (!spec.initialFolder.empty())
    {
        hrFolder = SHCreateItemFromParsingName (spec.initialFolder.c_str(), nullptr, IID_PPV_ARGS (&folderItem));

        if (SUCCEEDED (hrFolder))
        {
            hrFolder = dialog->SetFolder (folderItem.Get());
        }

        IGNORE_RETURN_VALUE (hrFolder, S_OK);
    }

    if (!spec.defaultFileName.empty())
    {
        hr = dialog->SetFileName (spec.defaultFileName.c_str());
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowAndRead
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostDialogs::ShowAndRead (IFileDialog * dialog, HWND owner, std::filesystem::path & outPath,
                                       bool & outPicked)
{
    HRESULT             hr        = S_OK;
    ComPtr<IShellItem>  item;
    PWSTR               path      = nullptr;
    bool                cancelled = false;



    outPicked = false;

    hr        = dialog->Show (owner);
    cancelled = (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED));

    //  Backing out is an answer, not a failure.
    BAIL_OUT_IF (cancelled, S_OK);
    CHR (hr);

    hr = dialog->GetResult (&item);
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_FILESYSPATH, &path);
    CHR (hr);

    outPath   = std::filesystem::path (path);
    outPicked = true;

Error:
    if (path != nullptr)
    {
        CoTaskMemFree (path);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickFileToOpen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostDialogs::PickFileToOpen (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath,
                                          bool & outPicked)
{
    HRESULT                  hr = S_OK;
    ComPtr<IFileOpenDialog>  dialog;



    outPicked = false;

    hr = CoCreateInstance (CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = Configure (dialog.Get(), spec);
    CHR (hr);

    hr = ShowAndRead (dialog.Get(), owner, outPath, outPicked);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickFileToSave
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostDialogs::PickFileToSave (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath,
                                          bool & outPicked)
{
    HRESULT                  hr = S_OK;
    ComPtr<IFileSaveDialog>  dialog;



    outPicked = false;

    hr = CoCreateInstance (CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = Configure (dialog.Get(), spec);
    CHR (hr);

    hr = ShowAndRead (dialog.Get(), owner, outPath, outPicked);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickFolder
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostDialogs::PickFolder (HWND owner, std::filesystem::path & outFolder, bool & outPicked)
{
    HRESULT                  hr      = S_OK;
    ComPtr<IFileOpenDialog>  dialog;
    DWORD                    options = 0;



    outPicked = false;

    hr = CoCreateInstance (CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&dialog));
    CHR (hr);

    hr = dialog->GetOptions (&options);
    CHR (hr);

    hr = dialog->SetOptions (options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    CHR (hr);

    hr = ShowAndRead (dialog.Get(), owner, outFolder, outPicked);

Error:
    return hr;
}
