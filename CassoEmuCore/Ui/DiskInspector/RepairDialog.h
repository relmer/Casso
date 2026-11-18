#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody
//
//  The confirmation's body: the intro, then for each change a check box
//  with the problem it sets right and a line under it with the repair, a
//  gap between one change and the next, and a line at the bottom with the
//  reason the checked changes can't be made, laid out in physical pixels as
//  the other dialog bodies are.
//
////////////////////////////////////////////////////////////////////////////////

class RepairDialogBody : public DxuiPanel
{
public:
    static constexpr int  kPadDip    = 12;
    static constexpr int  kLineDip   = 22;
    static constexpr int  kCheckDip  = 26;
    static constexpr int  kIndentDip = 26;     // the repair's line, under the check box's label
    static constexpr int  kGapDip    = 10;

    void  Init    (DxuiLabel & intro, vector<std::unique_ptr<DxuiCheckbox>> & checks, vector<std::unique_ptr<DxuiLabel>> & repairs, DxuiLabel & status,
                   int introLines);
    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    //  The body's height for a count of changes and of the intro's lines.
    static int  GetHeightDip (size_t changes, int introLines);

private:
    DxuiLabel                                * m_intro   = nullptr;
    vector<std::unique_ptr<DxuiCheckbox>>    * m_checks  = nullptr;
    vector<std::unique_ptr<DxuiLabel>>       * m_repairs = nullptr;
    DxuiLabel                                * m_status  = nullptr;
    int                                        m_lines   = 3;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog
//
//  Repair's confirmation: every change the volume can have, each with a
//  check box, and Repair or Cancel. The checked changes are planned again
//  as each box is turned on or off, and Repair is unavailable while they
//  can't be made, with the reason under them. Repair writes one copy with
//  every checked change.
//
////////////////////////////////////////////////////////////////////////////////

class RepairDialog : public DxuiDialogWindow
{
public:
    static constexpr int  kWidthDip  = 600;
    static constexpr int  kChromeDip = 108;

    using PlanFn = std::function<RepairPlan (const std::set<std::wstring> & chosen)>;

    //  The analysis is the caller's and outlives the dialog. The changes
    //  whose keys are in `chosen` start checked, or every change when it's
    //  empty.
    void  Configure (const IDxuiTheme * theme, const std::wstring & fileName, const DiskAnalysis & analysis, const std::set<std::wstring> & chosen);

    //  The same for any volume: the intro and its lines, the plan with every
    //  change, and the plan of the checked changes.
    void  Configure (const IDxuiTheme * theme, const std::wstring & intro, int introLines, const RepairPlan & fullPlan, PlanFn planChosen, const std::set<std::wstring> & chosen);

    //  The dialog created over `owner` and run until it closes.
    HRESULT  ShowModal (HWND owner, const IDxuiTheme * theme);

    bool                            IsConfirmed () const { return m_isConfirmed; }
    const RepairPlan              & GetPlan     () const { return m_plan; }
    const RepairPlan              & GetFullPlan () const { return m_fullPlan; }
    const std::set<std::wstring>  & GetChosen   () const { return m_chosen; }

    //  A change checked or cleared, as a click on its box does.
    void  SetChosen (const std::wstring & key, bool isChosen);

    //  The dialog's size for the plan's changes.
    static SIZE  GetSizeDip (const RepairPlan & plan);

    //  A file's name, in quotes when it has a space; the intro, with a
    //  caution after it for a volume that is only Suspect,
    //  whose findings can be normal for the disk; the intro's lines; a
    //  change's repair line; and the line under the changes: the reason the
    //  plan can't be made, or nothing.
    static std::wstring  FormatName     (const std::wstring & fileName);
    static std::wstring  FormatIntro    (const std::wstring & fileName);
    static std::wstring  FormatIntro    (const std::wstring & fileName, const RepairPlan & plan);
    static int           GetIntroLines  (const RepairPlan & plan);
    static std::wstring  FormatRepair   (const RepairChange & change);
    static std::wstring  FormatStatus   (const RepairPlan & plan);

    //  The image file analyzed as saved, the confirmation with the chosen
    //  changes checked, and the copy written beside the image. S_OK with the
    //  copy's path and the plan written, the path empty when the user
    //  cancels; a failure with a note for the status bar when repair isn't
    //  available or the copy isn't written.
    static HRESULT  Run (HWND                            owner,
                         const IDxuiTheme              * theme,
                         IDiskFileIo                   & io,
                         const std::string             & imagePath,
                         const std::set<std::wstring>  & chosen,
                         std::string                   & outCopyPath,
                         RepairPlan                    & outPlan,
                         std::wstring                  & outNote);

protected:
    void  OnCreate () override;

private:
    void  Replan ();

    const IDxuiTheme                       * m_theme       = nullptr;   // non-owning
    PlanFn                                   m_planChosen;
    std::wstring                             m_introText;
    int                                      m_introLines  = 3;
    RepairPlan                               m_fullPlan;
    RepairPlan                               m_plan;
    std::set<std::wstring>                   m_chosen;
    bool                                     m_isConfirmed = false;
    DxuiLabel                                m_intro;
    DxuiLabel                                m_status;
    vector<std::unique_ptr<DxuiCheckbox>>    m_checks;
    vector<std::unique_ptr<DxuiLabel>>       m_repairs;
    DxuiButton                             * m_repair      = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody
//
//  The report's body after a repair: a heading, a line for each fix with a
//  check mark in a column at the left, so a fix that wraps lines up after the
//  mark, and a line at the bottom. Each line is measured as it draws, both
//  for the dialog's width and for its height.
//
////////////////////////////////////////////////////////////////////////////////

class RepairReportBody : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    static constexpr int  kLineDip   = 18;
    static constexpr int  kMarkDip   = 26;     // the check marks' column
    static constexpr int  kGapDip    = 10;     // under the heading and over the bottom line
    static constexpr int  kFixGapDip = 4;
    static constexpr int  kMinDip    = 300;

    void  Init (const std::wstring & heading, const vector<std::wstring> & fixes, const std::wstring & footer);

    //  The height with each line on one line, for the dialog's first size.
    int   GetPreferredHeightDip () const;

    //  The width the widest line takes unwrapped, from kMinDip up to `maxPx`.
    bool  TryMeasureWidthPx  (IDxuiTextRenderer & text, const IDxuiTheme & theme, int maxPx, int & widthPx);
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;
    void  Layout             (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    size_t             GetRowCount () const               { return m_rows.size(); }
    const DxuiLabel  * GetMark     (size_t row) const     { return m_rows[row].mark; }
    const DxuiLabel  & GetLabel    (size_t row) const     { return *m_rows[row].label; }

private:
    struct Row
    {
        DxuiLabel  * mark     = nullptr;   // a fix's check mark; null on the heading and the bottom line
        DxuiLabel  * label    = nullptr;
        int          heightPx = 0;         // measured; 0 until TryMeasureHeightPx
    };

    int    GetGapAfterDip (size_t row) const;
    float  GetFontPx      (const DxuiLabel & label, const IDxuiTheme & theme) const;

    vector<Row>    m_rows;
    DxuiDpiScaler  m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog
//
//  The report after a repair, titled Repair complete, with the fixes applied
//  and only OK. The dialog is as wide as its widest line, up to kMaxWidthDip,
//  where longer lines wrap.
//
////////////////////////////////////////////////////////////////////////////////

class RepairReportDialog : public DxuiDialogWindow
{
public:
    static constexpr int  kMaxWidthDip = 560;
    static constexpr int  kChromeDip   = 108;

    void  Configure (const RepairPlan & plan, const std::wstring & footer);

    //  The window's width fitted to the body's widest line on each layout,
    //  once the window's frame is applied, as the base grows its height.
    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    RepairReportBody  * GetBody () const { return m_body; }
    bool                IsOk    () const { return m_isOk; }

    //  The heading and the bottom line: the copy loaded on OK where the host
    //  can open it, or the copy's name.
    static std::wstring  FormatHeading ();
    static std::wstring  FormatFooter  (bool canOpen, const std::wstring & copyName);

    //  Runs the report as a modal dialog; true when it's closed with OK.
    static bool  Run (HWND owner, const IDxuiTheme * theme, const RepairPlan & plan, const std::wstring & footer);

protected:
    void  OnCreate () override;

private:
    void  FitWidth (const DxuiDpiScaler & scaler);

    vector<std::wstring>   m_fixes;
    std::wstring           m_footer;
    RepairReportBody     * m_body      = nullptr;
    bool                   m_isOk      = false;
    bool                   m_isFitting = false;
};
