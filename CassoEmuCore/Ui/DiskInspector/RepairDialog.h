#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody
//
//  The confirmation's body: a line on the copy, a line for each change, and
//  "Keep sectors not in any file marked as used", laid out in physical pixels as
//  the other dialog bodies are.
//
////////////////////////////////////////////////////////////////////////////////

class RepairDialogBody : public DxuiPanel
{
public:
    static constexpr int  kPadDip   = 12;
    static constexpr int  kLineDip  = 22;
    static constexpr int  kCheckDip = 32;

    void  Init    (DxuiLabel & intro, DxuiLabel & changes, DxuiCheckbox & noFree);
    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    //  The body's height for a count of change lines.
    static int  GetHeightDip (size_t lines);

private:
    DxuiLabel     * m_intro   = nullptr;
    DxuiLabel     * m_changes = nullptr;
    DxuiCheckbox  * m_noFree  = nullptr;
    size_t          m_lines   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog
//
//  Repair's confirmation: the changes, a line each, made again when
//  "Keep sectors not in any file marked as used" is turned on or off, and Repair
//  or Cancel. The plan comes from the caller's planner, which reads the
//  image file as saved.
//
////////////////////////////////////////////////////////////////////////////////

class RepairDialog : public DxuiDialogWindow
{
public:
    using Planner = std::function<RepairPlan (bool isKeepingUnowned)>;

    static constexpr int  kWidthDip  = 560;
    static constexpr int  kChromeDip = 108;

    void  Configure (const IDxuiTheme * theme, const std::wstring & fileName, Planner planner);

    bool                IsConfirmed      () const { return m_isConfirmed; }
    bool                IsKeepingUnowned () const { return m_noFree.IsChecked(); }
    const RepairPlan  & GetPlan          () const { return m_plan; }

    //  The dialog's size for the plan's changes.
    static SIZE  GetSizeDip (const RepairPlan & plan);

    //  The intro and the change lines as they appear in the dialog.
    static std::wstring  FormatIntro   (const std::wstring & fileName);
    static std::wstring  FormatChanges (const RepairPlan & plan);

protected:
    void  OnCreate () override;

private:
    void  Replan ();

    const IDxuiTheme  * m_theme       = nullptr;   // non-owning
    std::wstring        m_fileName;
    Planner             m_planner;
    RepairPlan          m_plan;
    bool                m_isConfirmed = false;
    DxuiLabel           m_intro;
    DxuiLabel           m_changes;
    DxuiCheckbox        m_noFree      { L"Keep sectors not in any file marked as used" };
};
