#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiFieldError.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiTextInput.h"
#include "Widgets/DxuiTooltip.h"
#include "Widgets/DxuiButton.h"
#include "Window/DxuiDialogWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerNewDiskChoices
//
//  The choices the new-disk and format dialogs offer, the runner request a
//  set of them becomes, and the rules each field's value must meet. Kept apart
//  from the dialog so the mapping and the rules are testable without a window.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerNewDiskChoices
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

    //  The container, and the format where the extension implies one, that a
    //  typed extension asks for: .po a ProDOS-order ProDOS disk, .dsk and .do
    //  a DOS-order DOS 3.3 disk, .nib a nibble DOS 3.3 disk, .woz a WOZ image
    //  of either. False, both left alone, for any other extension or none;
    //  `outFormat` is -1 where the extension implies no format.
    static bool          GetChoicesForExtension (const std::wstring & name, int & outContainer, int & outFormat);

    //  Why a field's value cannot be used, or empty when it can. The file name
    //  is also refused when `exists` reports the name, extension added, as
    //  taken.
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
//  CassoExplorerNewDiskPanel
//
//  The form's rows, laid out one under another, each a label beside its field.
//  A field's error takes the room its message needs under that field and
//  pushes the rows below it down. The name and container rows are hidden when
//  formatting an image that already exists.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerNewDiskPanel : public DxuiPanel
{
public:
    struct Children
    {
        DxuiLabel       * nameLabel      = nullptr;
        DxuiTextInput   * name           = nullptr;
        DxuiFieldError  * nameError      = nullptr;
        DxuiLabel       * containerLabel = nullptr;
        DxuiComboBox    * container      = nullptr;
        DxuiLabel       * formatLabel    = nullptr;
        DxuiComboBox    * format         = nullptr;
        DxuiLabel       * volumeLabel    = nullptr;
        DxuiTextInput   * volume         = nullptr;
        DxuiFieldError  * volumeError    = nullptr;
        DxuiCheckbox    * bootable       = nullptr;

        //  Format mode's first row: the image being formatted, not editable.
        DxuiLabel       * fileLabel      = nullptr;
        DxuiLabel       * fileName       = nullptr;
    };

    using TipFn = std::function<std::wstring (IDxuiControl * field)>;

    void  Init (const Children & children, bool showNameRows);
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn) { m_onPressed = std::move (fn); }

    //  The tooltip a hovered field shows, and the text for each field; a field
    //  with no text shows none.
    void  SetTooltip (DxuiTooltip * tooltip, TipFn tipFor) { m_tooltip = tooltip; m_tipFor = std::move (tipFor); }

    //  What the panel measures its errors with. Without both, an error is taken
    //  to need a single line.
    void  SetMeasure (IDxuiTextRenderer * text, const IDxuiTheme * theme) { m_text = text; m_theme = theme; }

    //  The height the rows need at the panel's width, errors included.
    int   GetRequiredHeightPx () const { return ArrangeRows (m_boundsDip, m_scaler, false); }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kRowHeightDip      = 30;
    static constexpr int  kRowGapDip         = 8;
    static constexpr int  kLabelWidthDip     = 110;
    static constexpr int  kFallbackErrorDip  = 20;
    static constexpr int  kErrorTopDip       = 4;    // between a box and the message under it

private:
    //  Places the rows down from the top of `bounds`, or only measures them,
    //  and returns how tall they come to.
    int   ArrangeRows      (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const;
    int   GetErrorHeightPx (const DxuiFieldError & error, int widthPx, const DxuiDpiScaler & scaler) const;
    void  UpdateTooltip    (int x, int y);

    Children                               m_kids;
    bool                                   m_showNameRows = true;
    std::function<void (IDxuiControl *)>   m_onPressed;
    DxuiTooltip                          * m_tooltip      = nullptr;
    TipFn                                  m_tipFor;
    IDxuiTextRenderer                    * m_text         = nullptr;
    const IDxuiTheme                     * m_theme        = nullptr;
    DxuiDpiScaler                          m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerNewDiskDialog
//
//  Collects what a new disk image or a format needs, checking each field as it
//  changes and confirming only when every field passes. In format mode there
//  is no file to name: the image's own name shows, not editable, above the
//  format, volume and bootable rows.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerNewDiskDialog : public DxuiDialogWindow
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

    //  Runs the dialog modally over `owner`, the format list starting at
    //  `formatIndex`.
    static Outcome  Ask (HWND owner, const IDxuiTheme * theme, bool formatMode, const ExistsFn & exists = {},
                         int formatIndex = CassoExplorerNewDiskChoices::kFormatDos33, const std::wstring & target = {});

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    //  Checks every field, shows each one's error under it, and fits the window
    //  to the rows the errors leave.
    void  Revalidate   ();

    //  Sets the container and format to what the name's extension asks for,
    //  when the extension typed has just changed to one that asks.
    void  FollowTypedExtension (const std::wstring & name);

    //  Grows or shrinks the window by the difference between the room the rows
    //  have and the room they need.
    void  FitToContent ();
    void  FitBeforeShowing ();

    const IDxuiTheme    * m_theme          = nullptr;
    bool                  m_formatMode     = false;
    int                   m_formatIndex    = CassoExplorerNewDiskChoices::kFormatDos33;
    std::wstring          m_target;
    bool                  m_fitted         = false;
    ExistsFn              m_exists;
    std::wstring          m_typedExtension;
    DxuiButton          * m_ok             = nullptr;
    DxuiFieldValidator    m_validator;
    DxuiTooltip           m_tooltip;
    Outcome               m_outcome;
    DxuiLabel             m_nameLabel;
    DxuiTextInput         m_name;
    DxuiFieldError        m_nameError;
    DxuiLabel             m_containerLabel;
    DxuiComboBox          m_container;
    DxuiLabel             m_formatLabel;
    DxuiComboBox          m_format;
    DxuiLabel             m_volumeLabel;
    DxuiLabel             m_fileLabel;
    DxuiLabel             m_fileName;
    DxuiTextInput         m_volume;
    DxuiFieldError        m_volumeError;
    DxuiCheckbox             m_bootable { L"Bootable (copies the stock system files)" };
    CassoExplorerNewDiskPanel    * m_body       = nullptr;
};
