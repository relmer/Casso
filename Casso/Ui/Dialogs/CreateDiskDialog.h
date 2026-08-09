#pragma once

#include "Pch.h"
#include "Window/DxuiDialogWindow.h"

#include "../FileBrowseModel.h"
#include "Devices/Disk/BlankDiskBuilder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDiskDialog
//
//  The themed create-a-blank-disk dialog (spec 017 R-010, contract
//  shell-integration §3): a save-dialog-style body — current-folder listing,
//  file-name input, format / contents dropdowns, bootable toggle — over a
//  FileBrowseModel that owns all navigation and validation logic. The dialog
//  returns the confirmed BlankDiskSpec + target path; the caller
//  (WindowCommandManager) builds, writes, and mounts.
//
//  Built across T009 (US1: defaults + name + verdicts), T015 (US2: format /
//  contents / bootable), and T027 (US3: full navigation + keyboard pass).
//
////////////////////////////////////////////////////////////////////////////////

class CreateDiskDialog : public DxuiDialogWindow
{
public:
    //  Everything the create flow needs from a confirmed dialog.
    struct Result
    {
        BlankDiskSpec  spec;
        std::wstring   targetPath;
        bool           confirmed = false;
    };

    //  Wire the model (caller-owned) before Create; read Outcome after the
    //  modal ends.
    void            BindModel (FileBrowseModel * model) { m_model = model; }
    const Result &  Outcome   () const                  { return m_result; }

private:
    FileBrowseModel * m_model = nullptr;   // non-owning
    Result            m_result;
};
