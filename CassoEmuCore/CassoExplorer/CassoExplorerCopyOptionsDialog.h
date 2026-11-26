#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "CassoExplorer/Model/CopyOptionRows.h"
#include "CassoExplorer/Model/HostFileNaming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel
//
//  Each row's label to the left of its dropdown, and the check boxes below
//  their rows, indented to the dropdowns.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerCopyOptionsPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    //  A label and its dropdown, a check box or a note under the dropdowns,
    //  or a label, a value and a button at the right. `groupGaps` indicates
    //  how many group spaces are above it.
    struct Line
    {
        DxuiLabel     * label     = nullptr;
        DxuiComboBox  * box       = nullptr;
        DxuiCheckbox  * check     = nullptr;
        DxuiLabel     * note      = nullptr;
        DxuiLabel     * value     = nullptr;
        DxuiButton    * button    = nullptr;
        int             groupGaps = 0;
    };

    using MouseFn = std::function<void (const DxuiMouseEvent & ev)>;

    void  Init (const std::vector<Line> & lines);

    //  Called with each mouse event before the controls are.
    void  SetOnMouse (MouseFn fn) { m_onMouse = std::move (fn); }

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kNoteHeightDip   = 20;
    static constexpr int  kButtonWidthDip  = 88;
    static constexpr int  kLabelWidthDip   = 170;

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    std::vector<Line>  m_lines;
    DxuiDpiScaler      m_scaler;
    MouseFn            m_onMouse;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog
//
//  Copy to folder with options: a row for each kind of file in the
//  selection that a copy has options for, each a dropdown of its choices.
//  The 65816's register widths are check boxes below the code row, enabled
//  when the 65816 is chosen. Copy ends the dialog with the choices and the
//  destination folder, and is enabled once the folder exists.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerCopyOptionsDialog : public DxuiDialogWindow
{
public:
    //  The naming style a copy writes files under, and which files take it.
    struct Naming
    {
        HostFileNaming::Style      style = HostFileNaming::Style::Descriptive;
        ActionsTypes::NamingScope  scope = ActionsTypes::NamingScope::Unconverted;
    };

    //  The first selected file's name in a naming style.
    using NameFn = std::function<std::wstring (HostFileNaming::Style style)>;

    //  Shows the folder picker; false when canceled.
    using PickFn = std::function<bool (HWND owner, std::wstring & ioFolder)>;

    //  False when canceled or the dialog could not open; otherwise
    //  `ioOptions`, `ioNaming` and `ioFolder` have the choices.
    static bool  Choose (HWND                                       owner,
                         const IDxuiTheme                         * theme,
                         const std::vector<CopyOptionRows::Row>   & rows,
                         DecodeOptions                            & ioOptions,
                         Naming                                   & ioNaming,
                         std::wstring                             & ioFolder,
                         const NameFn                             & nameFor,
                         const PickFn                             & pick);

    static const wchar_t                    * GetRowLabel   (CopyOptionRows::Row row);
    static const std::vector<std::wstring>  & GetRowChoices (CopyOptionRows::Row row);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    static constexpr int   kRowCountMax = 7;
    static constexpr UINT  kTipTickMs   = 50;

    void  UpdateChecks      ();
    void  UpdateNamePreview ();
    void  AddNamingRows     (std::vector<CassoExplorerCopyOptionsPanel::Line> & ioLines);
    void  AddDestinationRow (std::vector<CassoExplorerCopyOptionsPanel::Line> & ioLines);
    void  UpdateCopyEnabled ();
    void  ShowTip           (const DxuiMouseEvent & ev);

    std::vector<CopyOptionRows::Row>                     m_rows;
    DecodeOptions                                        m_options;
    bool                                                 m_confirmed = false;
    std::array<DxuiLabel,    kRowCountMax>               m_labels;
    std::array<DxuiComboBox, kRowCountMax>               m_boxes;
    Naming                                               m_naming;
    DxuiLabel                                            m_styleLabel;
    DxuiComboBox                                         m_styleBox;
    DxuiLabel                                            m_scopeLabel;
    DxuiComboBox                                         m_scopeBox;
    DxuiLabel                                            m_namePreview;
    NameFn                                               m_nameFor;
    std::wstring                                         m_folder;
    PickFn                                               m_pick;
    DxuiLabel                                            m_folderLabel;
    DxuiLabel                                            m_folderPath;
    DxuiButton                                           m_browse;
    DxuiTooltip                                          m_tip;
    const IDxuiTheme                                   * m_tipTheme  = nullptr;
    DxuiButton                                         * m_copy      = nullptr;
    DxuiCheckbox                                         m_longRegisters;
    DxuiCheckbox                                         m_followFlags;
    CassoExplorerCopyOptionsPanel                      * m_body      = nullptr;
};
