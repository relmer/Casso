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
//  The Format dropdown (DOS 3.3 / ProDOS / Unformatted) is the primary
//  choice; it drives which image types (WOZ / DSK / PO) are offered, so an
//  illegal pairing cannot be selected. The name field's extension follows
//  the image type, as does the listing's extension filter. The Make-
//  bootable checkbox carries its own explanation in its label and installs
//  the chosen OS from the downloaded master; clicking any control moves
//  keyboard focus there.
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
    //  answers whether the OS master for a format choice is cached, and
    //  `downloadPayload` fetches it on the user's explicit click (FR-017).
    //  Call before Create. DPI is not taken here: it flows to every child
    //  through the panel tree's layout pass.
    void  Configure (FileBrowseModel * model, const IDxuiTheme * theme,
                     AvailableFn payloadAvailable, DownloadFn downloadPayload);

    const Result &  Outcome () const { return m_result; }

protected:
    void  OnCreate () override;

private:
    void  RefreshFromModel     ();
    void  RefreshListing       ();
    void  OnCreateClicked      ();
    void  OnFormatChanged      (int index);
    void  OnImageTypeChanged   (int index);
    void  OnDownloadClicked    ();
    void  OnRowActivated       (int row);
    void  RebuildImageTypeChoices ();
    void  ApplyImageTypeExtension ();
    void  UpdateBootableRow    ();

    static std::wstring    FormatSize       (const FileBrowseEntry & entry);
    static std::wstring    FormatModified   (int64_t modifiedUnix);
    static std::wstring    ReplaceExtension (const std::wstring & name, const wchar_t * ext);


    FileBrowseModel     * m_model = nullptr;   // non-owning
    const IDxuiTheme    * m_theme = nullptr;   // non-owning
    Result                m_result;

    DiskFormat               m_imageType = DiskFormat::Woz;
    BlankDiskContents        m_contents  = BlankDiskContents::Dos33;
    std::vector<DiskFormat>  m_imageTypeChoices;
    AvailableFn              m_payloadAvailable;
    DownloadFn               m_downloadPayload;

    CreateDiskBodyPanel * m_body      = nullptr;   // owned by the child tree
    DxuiLabel             m_pathLabel;
    DxuiListView          m_list;
    DxuiLabel             m_formatLabel;
    DxuiDropdown          m_formatDropdown;      // DOS 3.3 / ProDOS / Unformatted
    DxuiLabel             m_imageTypeLabel;
    DxuiDropdown          m_imageTypeDropdown;   // WOZ / DSK / PO
    DxuiCheckbox          m_bootableCheck;
    DxuiButton            m_downloadButton;
    DxuiLabel             m_nameLabel;
    DxuiTextInput         m_nameInput;
};
