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
//  updated for a deleted image.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerDeletion
{
public:
    explicit ExplorerDeletion (ExplorerContext & context);

    void     DeleteSelected             ();
    void     DeleteSelectionPermanently ();
    HRESULT  RecycleHostFiles           (const std::vector<std::wstring> & paths);

private:
    void  DeleteHostSelection ();
    void  DeleteFromVolume    ();

    ExplorerContext  & m_context;
};