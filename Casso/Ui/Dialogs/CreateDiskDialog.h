#pragma once

#include "Pch.h"
#include "Window/DxuiDialogWindow.h"

#include "../FileBrowseModel.h"
#include "../CreateDiskBodyPanel.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDiskDialog
//
//  The themed create-a-blank-disk dialog: a save-dialog-style body —
//  current-folder listing, file-name input — over a FileBrowseModel that
//  owns all listing and validation logic. The dialog returns the confirmed
//  BlankDiskSpec + target path; the caller builds, writes, and mounts.
//
//  The Create button validates through the model: an invalid name or a
//  target that is currently mounted in a drive is refused with a message; an
//  existing file asks for explicit overwrite confirmation; only a clean
//  verdict (or a confirmed overwrite) ends the modal with a confirmed
//  Result. Cancel / Escape / the close box leave Result unconfirmed.
//
//  Format / contents / bootable controls and in-dialog folder navigation are
//  not built yet; the spec returned today is the default configuration.
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

    //  Wire the pre-configured model (caller-owned; folder set, extension
    //  filter + mounted paths applied) and the theme used for the dialog's
    //  own message popups. Call before Create.
    void  Configure (FileBrowseModel * model, const IDxuiTheme * theme, UINT dpi);

    const Result &  Outcome () const { return m_result; }

protected:
    void  OnCreate () override;

private:
    void  RefreshFromModel ();
    void  OnCreateClicked  ();

    static std::wstring  FormatSize     (const FileBrowseEntry & entry);
    static std::wstring  FormatModified (int64_t modifiedUnix);

    FileBrowseModel     * m_model = nullptr;   // non-owning
    const IDxuiTheme    * m_theme = nullptr;   // non-owning
    UINT                  m_dpi   = 96;
    Result                m_result;

    CreateDiskBodyPanel * m_body      = nullptr;   // owned by the child tree
    DxuiLabel             m_pathLabel;
    DxuiListView          m_list;
    DxuiLabel             m_nameLabel;
    DxuiTextInput         m_nameInput;
};
