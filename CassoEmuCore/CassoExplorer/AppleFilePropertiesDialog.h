#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/AppleFileProperties.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesPanel
//
//  The dialog's rows, one under another: a label beside one or more fields,
//  or a note in the field column. A row whose controls are all hidden takes
//  no room.
//
////////////////////////////////////////////////////////////////////////////////

class AppleFilePropertiesPanel : public DxuiPanel
{
public:
    struct Field
    {
        IDxuiControl  * control  = nullptr;
        int             widthDip = 0;   // 0 takes the rest of the row
    };

    struct Row
    {
        DxuiLabel           * label     = nullptr;
        std::vector<Field>    fields;
        bool                  gapAbove  = false;
        int                   heightDip = kRowHeightDip;
    };

    void  Init                 (std::vector<Row> rows);
    void  SetOnChildPressed    (std::function<void (IDxuiControl *)> fn) { m_onPressed = std::move (fn); }
    int   GetRequiredHeightDip () const;

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kLabelWidthDip = 110;
    static constexpr int  kRowHeightDip  = 30;
    static constexpr int  kRowGapDip     = 6;
    static constexpr int  kGroupGapDip   = 16;
    static constexpr int  kFieldGapDip   = 8;

private:
    bool  IsRowShown (const Row & row) const;

    std::vector<Row>                      m_rows;
    std::function<void (IDxuiControl *)>  m_onPressed;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesDialog
//
//  An Apple file's name, type, second field, access and dates, editable, and
//  its size. With several files, a field that differs reads (multiple) and
//  changes only when it is edited, the name is not offered, and the second
//  field is offered only when every file has the same type. On a disk that
//  can't be written, the fields are disabled and Close is the only button.
//
////////////////////////////////////////////////////////////////////////////////

class AppleFilePropertiesDialog : public DxuiDialogWindow
{
public:
    //  Writes the edits to the files, returning the reason it failed, or
    //  nothing when it succeeded, and the files as they are after the write.
    using ApplyFn = std::function<std::wstring (const AppleFileProperties::Edits & edits, std::vector<FileEntry> & inOutEntries)>;

    //  For a file on the PC: which rows it has, the file system whose name
    //  rule its name follows, and for an AppleSingle file, the box that
    //  renames it on the PC too, with whether a name is taken there.
    struct HostOptions
    {
        bool                                            showAccess  = true;
        bool                                            showDates   = true;
        VolumeKind                                      nameKind    = VolumeKind::Dos33;
        bool                                            offerRename = false;
        std::wstring                                    leafName;
        std::function<bool (const std::wstring & leaf)> isTaken;
    };

    static void  Show (HWND owner, const IDxuiTheme * theme, const std::vector<FileEntry> & entries, VolumeKind kind, bool readOnly, ApplyFn apply,
                       const HostOptions * host = nullptr);

    static constexpr const wchar_t *  s_kMultiple = L"(multiple)";

protected:
    void  OnCreate () override;

private:
    //  The fields' edits, checked; false with the reason when one can't be used.
    bool  CollectEdits  (AppleFileProperties::Edits & outEdits, std::wstring & outError) const;

    //  Applies the edits and takes the fields as they now are as unedited;
    //  false, after a message box, when the edits can't be used or written.
    bool  Apply         ();

    void  ResetEdited     ();
    void  FitToRows       ();
    void  UpdateTypeRow   ();
    void  UpdateRenameBox ();
    Byte  GetChosenType   (bool & outKnown) const;

    template <typename T>
    static bool  IsSame (const std::vector<FileEntry> & entries, T get) { return std::all_of (entries.begin(), entries.end(), [&] (const FileEntry & e) { return get (e) == get (entries[0]); }); }

    static constexpr int  s_kChromeDip   = 130;
    static constexpr int  s_kWidthDip    = 620;
    static constexpr int  s_kHexWidthDip = 70;

    const IDxuiTheme                              * m_theme      = nullptr;
    std::vector<FileEntry>                          m_entries;
    VolumeKind                                      m_kind       = VolumeKind::Dos33;
    bool                                            m_readOnly   = false;
    ApplyFn                                         m_apply;
    std::vector<AppleFileProperties::TypeChoice>    m_choices;
    bool                                            m_typeOffset = false;   // (multiple) is the combo's first row
    bool                                            m_sameType   = true;
    std::wstring                                    m_nameShown;
    bool                                            m_hasHost    = false;
    HostOptions                                     m_host;

    bool  m_typeEdited     = false;
    bool  m_auxEdited      = false;
    Byte  m_accessMask     = 0;
    bool  m_lockedEdited   = false;
    bool  m_createdEdited  = false;
    bool  m_modifiedEdited = false;

    DxuiLabel       m_nameLabel;
    DxuiTextInput   m_name;
    DxuiCheckbox    m_renameHost;
    DxuiLabel       m_typeLabel;
    DxuiComboBox    m_type;
    DxuiTextInput   m_typeHex;
    DxuiLabel       m_typeNote;
    DxuiLabel       m_auxLabel;
    DxuiTextInput   m_aux;
    DxuiLabel       m_accessLabel;
    DxuiCheckbox    m_locked  { L"Locked" };
    DxuiCheckbox    m_read    { L"Read" };
    DxuiCheckbox    m_write   { L"Write" };
    DxuiCheckbox    m_rename  { L"Rename" };
    DxuiCheckbox    m_destroy { L"Destroy" };
    DxuiCheckbox    m_backup  { L"Backup" };
    DxuiLabel                   m_createdLabel;
    DxuiTextInput               m_created;
    DxuiLabel                   m_modifiedLabel;
    DxuiTextInput               m_modified;
    DxuiLabel                   m_sizeLabel;
    DxuiLabel                   m_size;
    AppleFilePropertiesPanel  * m_body          = nullptr;
};
