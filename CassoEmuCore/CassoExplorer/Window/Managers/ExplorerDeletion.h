#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDeletion
//
//  Every way Casso Explorer deletes files: Delete on the selection, on the PC
//  to the Recycle Bin and inside a disk image or on an Apple drive after a
//  confirmation, with an undo for each; deleting for good, as a file inside
//  a zip is; and recycling a move's sources. The Full Send manifest is
//  updated for a deleted image, and its thumbnail removed. Clear thumbnails
//  removes the thumbnails of every image.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerDeletion
{
public:
    explicit ExplorerDeletion (ExplorerContext & context);

    void     DeleteSelected             ();
    void     DeleteSelectionPermanently ();
    HRESULT  RecycleHostFiles           (const std::vector<std::wstring> & paths);
    void     ClearThumbnails            (HWND owner);

private:
    void                      DeleteHostSelection ();
    void                      DeleteFromVolume    ();
    bool                      ConfirmA2PicoDrives (const std::vector<std::wstring> & paths);
    std::vector<std::string>  GetThumbnailKeys    (const std::vector<std::wstring> & paths);
    void                      RemoveThumbnails    (const std::vector<std::wstring> & paths, const std::vector<std::string> & keys);

    ExplorerContext  & m_context;
};