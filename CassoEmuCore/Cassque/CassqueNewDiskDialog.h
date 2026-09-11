#pragma once

#include "Pch.h"

#include "Cassque/Model/DiskOperations.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiTextInput.h"
#include "Window/DxuiDialogWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices
//
//  The choices the new-disk and format dialogs offer, and the runner request
//  a set of them becomes. Kept apart from the dialog so the mapping is
//  testable without a window.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueNewDiskChoices
{
public:
    struct Choice
    {
        const wchar_t  * label;
        const char     * value;
    };

    static constexpr Choice  kFormats[] =
    {
        { L"DOS 3.3",     "dos33"  },
        { L"ProDOS",      "prodos" },
        { L"Unformatted", "none"   },
    };

    static constexpr Choice  kContainers[] =
    {
        { L"WOZ (.woz)",           "woz" },
        { L"DOS order (.dsk)",     "dsk" },
        { L"ProDOS order (.po)",   "po"  },
        { L"Nibble (.nib)",        "nib" },
    };

    static std::vector<std::wstring>  GetFormatLabels();
    static std::vector<std::wstring>  GetContainerLabels();

    //  The request for the chosen indices. An index out of range takes the
    //  first choice; an empty volume leaves the runner's default.
    static DiskOperations::NewDiskRequest  MakeRequest (int formatIndex, int containerIndex, const std::wstring & volume, bool bootable);

    //  The file name with the container's extension, added when it has none.
    static std::wstring  ApplyExtension (const std::wstring & name, int containerIndex);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskPanel
//
//  The form's rows, laid out one under another, each a label beside its
//  field. The name and container rows are hidden when formatting an image
//  that already exists.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueNewDiskPanel : public DxuiPanel
{
public:
    struct Children
    {
        DxuiLabel      * nameLabel      = nullptr;
        DxuiTextInput  * name           = nullptr;
        DxuiLabel      * containerLabel = nullptr;
        DxuiComboBox   * container      = nullptr;
        DxuiLabel      * formatLabel    = nullptr;
        DxuiComboBox   * format         = nullptr;
        DxuiLabel      * volumeLabel    = nullptr;
        DxuiTextInput  * volume         = nullptr;
        DxuiCheckbox   * bootable       = nullptr;
    };

    void  Init (const Children & children, bool showNameRows);
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn) { m_onPressed = std::move (fn); }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kRowHeightDip   = 30;
    static constexpr int  kRowGapDip      = 8;
    static constexpr int  kLabelWidthDip  = 110;

private:
    Children                               m_kids;
    bool                                   m_showNameRows = true;
    std::function<void (IDxuiControl *)>   m_onPressed;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskDialog
//
//  Collects what a new disk image or a format needs. In format mode there is
//  no file to name, so only the format, volume and bootable rows show.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueNewDiskDialog : public DxuiDialogWindow
{
public:
    struct Outcome
    {
        bool                            confirmed = false;
        std::wstring                    fileName;
        DiskOperations::NewDiskRequest  request;
    };

    //  Runs the dialog modally over `owner`.
    static Outcome  Ask (HWND owner, const IDxuiTheme * theme, bool formatMode);

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme       * m_theme      = nullptr;
    bool                     m_formatMode = false;
    Outcome                  m_outcome;
    DxuiLabel                m_nameLabel;
    DxuiTextInput            m_name;
    DxuiLabel                m_containerLabel;
    DxuiComboBox             m_container;
    DxuiLabel                m_formatLabel;
    DxuiComboBox             m_format;
    DxuiLabel                m_volumeLabel;
    DxuiTextInput            m_volume;
    DxuiCheckbox             m_bootable { L"Bootable (copies the stock system files)" };
    CassqueNewDiskPanel    * m_body       = nullptr;
};
