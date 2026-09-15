#pragma once

#include "Pch.h"

#include "Cassque/Model/DiskOperations.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiTextInput.h"
#include "Widgets/DxuiTooltip.h"
#include "Widgets/DxuiButton.h"
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

    //  Why a field's value cannot be used, or empty when it can. Checked as the
    //  user types, so the dialog never closes on a value the runner would
    //  refuse. The file name is also refused when `exists` reports the name,
    //  extension added, as taken.
    static std::wstring  ValidateFileName  (const std::wstring & name, int containerIndex, const std::function<bool (const std::wstring &)> & exists);
    static std::wstring  ValidateVolume    (int formatIndex, const std::wstring & volume);

    //  The most characters the volume field takes for a format: a DOS 3.3
    //  volume number's three digits, or a ProDOS name's fifteen characters.
    static size_t        GetVolumeMaxLength (int formatIndex);

    static constexpr int     kFormatDos33           = 0;   // indices into kFormats
    static constexpr int     kFormatProDos          = 1;
    static constexpr int     kFormatNone            = 2;
    static constexpr size_t  kMaxFileNameLength     = 200;
    static constexpr size_t  kMaxDos33VolumeLength  = 3;
    static constexpr size_t  kMaxProDosVolumeLength = 15;
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
        DxuiLabel      * nameError      = nullptr;
        DxuiLabel      * containerLabel = nullptr;
        DxuiComboBox   * container      = nullptr;
        DxuiLabel      * formatLabel    = nullptr;
        DxuiComboBox   * format         = nullptr;
        DxuiLabel      * volumeLabel    = nullptr;
        DxuiTextInput  * volume         = nullptr;
        DxuiLabel      * volumeError    = nullptr;
        DxuiCheckbox   * bootable       = nullptr;
    };

    using TipFn = std::function<std::wstring (IDxuiControl * field)>;

    void  Init (const Children & children, bool showNameRows);
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn) { m_onPressed = std::move (fn); }

    //  The tooltip a hovered field shows, and the text for each field; a field
    //  with no text shows none.
    void  SetTooltip (DxuiTooltip * tooltip, TipFn tipFor) { m_tooltip = tooltip; m_tipFor = std::move (tipFor); }

    //  Which fields carry an error, marked beside the message under them.
    void  SetErrorMarks (bool name, bool volume) { m_nameBad = name; m_volumeBad = volume; }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kRowHeightDip   = 30;
    static constexpr int  kRowGapDip      = 8;
    static constexpr int  kLabelWidthDip  = 110;
    static constexpr int  kErrorLineDip   = 20;   // the line under a field that holds its error
    static constexpr int  kErrorMarkDip   = 14;   // the error mark's diameter
    static constexpr int  kErrorGapDip    = 6;    // between the mark and the message

private:
    void  UpdateTooltip  (int x, int y);
    void  PaintErrorMark (IDxuiPainter & painter, const RECT & line, const IDxuiTheme & theme) const;

    Children                               m_kids;
    bool                                   m_showNameRows    = true;
    std::function<void (IDxuiControl *)>   m_onPressed;
    DxuiTooltip                          * m_tooltip         = nullptr;
    TipFn                                  m_tipFor;
    bool                                   m_nameBad         = false;
    bool                                   m_volumeBad       = false;
    RECT                                   m_nameErrorLine   = {};
    RECT                                   m_volumeErrorLine = {};
    DxuiDpiScaler                          m_scaler;
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

    //  Whether a file name is taken in the folder the image will go in, asked
    //  as the name is typed. None skips the check.
    using ExistsFn = std::function<bool (const std::wstring & fileName)>;

    //  Runs the dialog modally over `owner`.
    static Outcome  Ask (HWND owner, const IDxuiTheme * theme, bool formatMode, const ExistsFn & exists = {});

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    //  Checks every field, shows each one's error under it, and enables the
    //  confirming button only when none has one.
    void  Revalidate ();

    const IDxuiTheme       * m_theme      = nullptr;
    bool                     m_formatMode = false;
    ExistsFn                 m_exists;
    DxuiButton             * m_ok         = nullptr;
    DxuiTooltip              m_tooltip;
    Outcome                  m_outcome;
    DxuiLabel                m_nameLabel;
    DxuiTextInput            m_name;
    DxuiLabel                m_nameError;
    DxuiLabel                m_containerLabel;
    DxuiComboBox             m_container;
    DxuiLabel                m_formatLabel;
    DxuiComboBox             m_format;
    DxuiLabel                m_volumeLabel;
    DxuiTextInput            m_volume;
    DxuiLabel                m_volumeError;
    DxuiCheckbox             m_bootable { L"Bootable (copies the stock system files)" };
    CassqueNewDiskPanel    * m_body       = nullptr;
};
