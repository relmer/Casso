#pragma once

#include "Pch.h"
#include "Window/DxuiDialogWindow.h"

#include "../FileBrowseModel.h"
#include "../CreateDiskBodyPanel.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiDropdown.h"
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
//  The Image-type dropdown (WOZ / DSK / PO) drives the Format choices
//  (DOS 3.3 / ProDOS / unformatted): only legal pairings are ever listed,
//  so an illegal combination cannot be selected. The name field's
//  extension follows the image type, as does the listing's extension
//  filter. The Bootable toggle installs the chosen OS from the downloaded
//  master; clicking any control moves keyboard focus there.
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

    using AvailableFn = std::function<bool (BlankDiskContents)>;
    using DownloadFn  = std::function<HRESULT (BlankDiskContents)>;

    //  Wire the pre-configured model (caller-owned; folder set, extension
    //  filter + mounted paths applied), the theme used for the dialog's own
    //  message popups, and the boot-payload callbacks: `payloadAvailable`
    //  answers whether the OS master for a contents choice is cached, and
    //  `downloadPayload` fetches it on the user's explicit click (FR-017).
    //  Call before Create.
    void  Configure (FileBrowseModel * model, const IDxuiTheme * theme, UINT dpi,
                     AvailableFn payloadAvailable, DownloadFn downloadPayload);

    const Result &  Outcome () const { return m_result; }

protected:
    void  OnCreate () override;

private:
    void  RefreshFromModel   ();
    void  RefreshListing     ();
    void  OnCreateClicked    ();
    void  OnFormatChanged    (int index);
    void  OnContentsChanged  (int index);
    void  OnDownloadClicked  ();
    void  OnRowActivated     (int row);
    void  RebuildContentsChoices ();
    void  UpdateBootableRow  ();

    static std::wstring    FormatSize       (const FileBrowseEntry & entry);
    static std::wstring    FormatModified   (int64_t modifiedUnix);
    static const wchar_t * FormatExtension  (DiskFormat format);
    static std::wstring    ContentsCaption  (BlankDiskContents contents);
    static std::wstring    ReplaceExtension (const std::wstring & name, const wchar_t * ext);

    FileBrowseModel     * m_model = nullptr;   // non-owning
    const IDxuiTheme    * m_theme = nullptr;   // non-owning
    UINT                  m_dpi   = 96;
    Result                m_result;

    DiskFormat                      m_format   = DiskFormat::Woz;
    BlankDiskContents               m_contents = BlankDiskContents::Dos33;
    std::vector<BlankDiskContents>  m_contentsChoices;
    AvailableFn                     m_payloadAvailable;
    DownloadFn                      m_downloadPayload;

    CreateDiskBodyPanel * m_body      = nullptr;   // owned by the child tree
    DxuiLabel             m_pathLabel;
    DxuiListView          m_list;
    DxuiLabel             m_formatLabel;
    DxuiDropdown          m_formatDropdown;
    DxuiLabel             m_contentsLabel;
    DxuiDropdown          m_contentsDropdown;
    DxuiCheckbox          m_bootableCheck;
    DxuiButton            m_downloadButton;
    DxuiLabel             m_bootHint;
    DxuiLabel             m_nameLabel;
    DxuiTextInput         m_nameInput;
};
