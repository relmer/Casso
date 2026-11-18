#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CopyOptionRows.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsPanel
//
//  Each row's label to the left of its dropdown, and the check boxes below
//  the last row, indented to the dropdowns.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerCopyOptionsPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    struct Line
    {
        DxuiLabel     * label = nullptr;
        DxuiComboBox  * box   = nullptr;
    };

    void  Init (const std::vector<Line> & lines, const std::vector<DxuiCheckbox *> & checks);

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    std::vector<Line>            m_lines;
    std::vector<DxuiCheckbox *>  m_checks;
    DxuiDpiScaler                m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCopyOptionsDialog
//
//  Copy to folder with options: a row for each kind of file in the
//  selection that a copy has options for, each a dropdown of its choices.
//  The 65816's register widths are check boxes below the code row, enabled
//  when the 65816 is chosen. Copy ends the dialog with the choices, and the
//  folder is picked after it.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerCopyOptionsDialog : public DxuiDialogWindow
{
public:
    //  False when canceled or the dialog could not open; otherwise
    //  `ioOptions` has the choices.
    static bool  Choose (HWND                                       owner,
                         const IDxuiTheme                         * theme,
                         const std::vector<CopyOptionRows::Row>   & rows,
                         DecodeOptions                            & ioOptions);

    static const wchar_t                    * GetRowLabel   (CopyOptionRows::Row row);
    static const std::vector<std::wstring>  & GetRowChoices (CopyOptionRows::Row row);

protected:
    void  OnCreate () override;

private:
    static constexpr int  kRowCountMax = 6;

    void  UpdateChecks ();

    std::vector<CopyOptionRows::Row>                     m_rows;
    DecodeOptions                                        m_options;
    bool                                                 m_confirmed = false;
    std::array<DxuiLabel,    kRowCountMax>               m_labels;
    std::array<DxuiComboBox, kRowCountMax>               m_boxes;
    DxuiCheckbox                                         m_longRegisters;
    DxuiCheckbox                                         m_followFlags;
    CassoExplorerCopyOptionsPanel                      * m_body      = nullptr;
};
