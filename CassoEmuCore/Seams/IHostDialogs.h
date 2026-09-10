#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileDialogSpec
//
//  What a file picker is told before it opens. Everything is optional; an
//  empty field is a thing the dialog decides for itself.
//
////////////////////////////////////////////////////////////////////////////////

struct FileDialogFilter
{
    std::wstring  name;      // "Disk images"
    std::wstring  pattern;   // "*.dsk;*.woz"
};


struct FileDialogSpec
{
    std::vector<FileDialogFilter>  filters;
    std::wstring                   defaultExtension;   // "png", without the dot
    std::wstring                   defaultFileName;    // seeds the name field
    std::filesystem::path          initialFolder;      // best effort: the dialog may open elsewhere
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostDialogs
//
//  The operating system's file and folder pickers, as a question with three
//  answers: a path, a cancel, or a failure.
//
//  A success with `outPicked` set carries a path. A success without it is
//  the user backing out, which every caller treats as keeping what it had --
//  a normal thing to do, not an error, and the reason it is not one here. A
//  failure is a failure to report.
//
//  The pickers are modal and belong to the window that owns them, which is
//  why an owner is passed rather than remembered: the shell has several
//  windows a picker can hang from.
//
////////////////////////////////////////////////////////////////////////////////

class IHostDialogs
{
public:

    virtual ~IHostDialogs () = default;

    virtual HRESULT  PickFileToOpen (HWND                     owner,
                                     const FileDialogSpec &   spec,
                                     std::filesystem::path &  outPath,
                                     bool &                   outPicked)  = 0;

    virtual HRESULT  PickFileToSave (HWND                     owner,
                                     const FileDialogSpec &   spec,
                                     std::filesystem::path &  outPath,
                                     bool &                   outPicked)  = 0;

    virtual HRESULT  PickFolder     (HWND                     owner,
                                     std::filesystem::path &  outFolder,
                                     bool &                   outPicked)  = 0;
};
