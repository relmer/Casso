#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/ChooseDetailsModel.h"


class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsPanel
//
//  The dialog's content, as File Explorer lays out Choose Details: the
//  prompt across the top, and the list at the left with its buttons beside
//  it. Mouse points are translated to the list's coordinates for the list.
//
////////////////////////////////////////////////////////////////////////////////

class ChooseDetailsPanel : public DxuiPanel
{
public:
    void     Init              (DxuiLabel & prompt, DxuiListView & list, std::vector<DxuiButton *> buttons);
    void     SetOnChildPressed (std::function<void (IDxuiControl *)> onPressed) { m_onPressed = std::move (onPressed); }
    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

    static constexpr int  kPromptDip      = 24;
    static constexpr int  kGapDip         = 12;
    static constexpr int  kButtonWidthDip = 104;
    static constexpr int  kButtonDip      = 32;

private:
    DxuiLabel                              * m_prompt  = nullptr;
    DxuiListView                           * m_list    = nullptr;
    std::vector<DxuiButton *>                m_buttons;
    std::function<void (IDxuiControl *)>     m_onPressed;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsDialog
//
//  File Explorer's Choose Details: every column available to the list,
//  checked when visible, with buttons to move the selected entry up or down
//  and to check or clear every selected entry. A column added is fitted to
//  its values, so the dialog has no width field.
//
////////////////////////////////////////////////////////////////////////////////

class ChooseDetailsDialog : public DxuiDialogWindow
{
public:
    //  Runs the dialog over `owner` on `model`; true when OK ended it, with
    //  the model as the user left it.
    static bool  Run (HWND owner, const IDxuiTheme * theme, ChooseDetailsModel & model);

protected:
    void  OnCreate() override;

private:
    void  FillList      ();
    void  SelectEntry   (size_t entry);
    void  UpdateButtons ();
    void  SetChecked    (bool checked);
    void  KeepFocus     (DxuiButton & button);

    ChooseDetailsModel  * m_model    = nullptr;
    const IDxuiTheme    * m_theme    = nullptr;
    int                   m_selected = -1;
    DxuiLabel             m_prompt;
    DxuiListView          m_list;
    DxuiButton             m_up       { L"Move up" };
    DxuiButton             m_down     { L"Move down" };
    DxuiButton             m_show     { L"Show" };
    DxuiButton             m_hide     { L"Hide" };
    ChooseDetailsPanel   * m_body     = nullptr;
};
