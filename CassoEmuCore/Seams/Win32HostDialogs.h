#pragma once

#include "Pch.h"

#include "Seams/IHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostDialogs
//
//  IHostDialogs over the common item dialog: IFileOpenDialog for opening and
//  for folders (FOS_PICKFOLDERS is the modern SHBrowseForFolder), and
//  IFileSaveDialog for saving.
//
////////////////////////////////////////////////////////////////////////////////

class Win32HostDialogs : public IHostDialogs
{
public:

    HRESULT  PickFileToOpen (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath,
                             bool & outPicked) override;
    HRESULT  PickFileToSave (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath,
                             bool & outPicked) override;
    HRESULT  PickFolder     (HWND owner, std::filesystem::path & outFolder, bool & outPicked) override;

private:

    //  The part every picker shares: configure from the spec, show, and read
    //  the chosen item back as a file-system path. A cancel succeeds with
    //  nothing picked and the path untouched.
    static HRESULT  Configure (IFileDialog * dialog, const FileDialogSpec & spec);
    static HRESULT  ShowAndRead (IFileDialog * dialog, HWND owner, std::filesystem::path & outPath,
                                 bool & outPicked);
};
