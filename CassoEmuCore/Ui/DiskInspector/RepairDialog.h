#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody
//
//  The confirmation's body: a line on the copy, a check box for each
//  change, and a line under them with the reason the checked changes can't
//  be made, laid out in physical pixels as the other dialog bodies are.
//
////////////////////////////////////////////////////////////////////////////////

class RepairDialogBody : public DxuiPanel
{
public:
    static constexpr int  kPadDip   = 12;
    static constexpr int  kLineDip  = 22;
    static constexpr int  kCheckDip = 28;

    void  Init    (DxuiLabel & intro, vector<std::unique_ptr<DxuiCheckbox>> & checks, DxuiLabel & status, int introLines);
    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    //  The body's height for a count of changes and of the intro's lines.
    static int  GetHeightDip (size_t changes, int introLines);

private:
    DxuiLabel                                * m_intro  = nullptr;
    vector<std::unique_ptr<DxuiCheckbox>>    * m_checks = nullptr;
    DxuiLabel                                * m_status = nullptr;
    int                                        m_lines  = 2;
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
    static constexpr int  kWidthDip  = 560;
    static constexpr int  kChromeDip = 108;

    //  The analysis is the caller's and outlives the dialog. The changes
    //  whose keys are in `chosen` start checked, or every change when it's
    //  empty.
    void  Configure (const IDxuiTheme * theme, const std::wstring & fileName, const DiskAnalysis & analysis, const std::set<std::wstring> & chosen);

    bool                            IsConfirmed () const { return m_isConfirmed; }
    const RepairPlan              & GetPlan     () const { return m_plan; }
    const RepairPlan              & GetFullPlan () const { return m_fullPlan; }
    const std::set<std::wstring>  & GetChosen   () const { return m_chosen; }

    //  A change checked or cleared, as a click on its box does.
    void  SetChosen (const std::wstring & key, bool isChosen);

    //  The dialog's size for the plan's changes.
    static SIZE  GetSizeDip (const RepairPlan & plan);

    //  The intro, with a caution after it for a volume that is only Suspect,
    //  whose findings can be normal for the disk; the intro's lines; and the
    //  line under the changes: the reason the plan can't be made, or nothing.
    static std::wstring  FormatIntro    (const std::wstring & fileName);
    static std::wstring  FormatIntro    (const std::wstring & fileName, const RepairPlan & plan);
    static int           GetIntroLines  (const RepairPlan & plan);
    static std::wstring  FormatStatus   (const RepairPlan & plan);

    //  The image file analyzed as saved, the confirmation with the chosen
    //  changes checked, and the copy written beside the image. S_OK with the
    //  copy's path, which is empty when the user cancels; a failure with a
    //  note for the status bar when repair isn't available or the copy isn't
    //  written.
    static HRESULT  Run (HWND                            owner,
                         const IDxuiTheme              * theme,
                         IDiskFileIo                   & io,
                         const std::string             & imagePath,
                         const std::set<std::wstring>  & chosen,
                         std::string                   & outCopyPath,
                         std::wstring                  & outNote);

protected:
    void  OnCreate () override;

private:
    void  Replan ();

    const IDxuiTheme                       * m_theme       = nullptr;   // non-owning
    const DiskAnalysis                     * m_analysis    = nullptr;   // non-owning
    std::wstring                             m_fileName;
    RepairPlan                               m_fullPlan;
    RepairPlan                               m_plan;
    std::set<std::wstring>                   m_chosen;
    bool                                     m_isConfirmed = false;
    DxuiLabel                                m_intro;
    DxuiLabel                                m_status;
    vector<std::unique_ptr<DxuiCheckbox>>    m_checks;
    DxuiButton                             * m_repair      = nullptr;
};
