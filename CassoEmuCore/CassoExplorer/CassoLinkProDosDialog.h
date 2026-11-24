#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosPanel
//
//  The explanation over a row of the ProDOS disk's label, its dropdown and
//  the Browse button.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkProDosPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    using MouseFn = std::function<void (const DxuiMouseEvent & ev)>;

    void  Init (DxuiLabel & note, DxuiLabel & label, DxuiComboBox & disks, DxuiButton & browse);

    //  Called with each mouse event before the controls are.
    void  SetOnMouse (MouseFn fn) { m_onMouse = std::move (fn); }

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kLabelWidthDip  = 100;
    static constexpr int  kBrowseWidthDip = 90;

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    DxuiLabel      * m_note   = nullptr;
    DxuiLabel      * m_label  = nullptr;
    DxuiComboBox   * m_disks  = nullptr;
    DxuiButton     * m_browse = nullptr;
    DxuiDpiScaler    m_scaler;
    MouseFn          m_onMouse;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDialog
//
//  Chooses the user's ProDOS disk that the CassoLink virtual drives disk is
//  made from: a dropdown of the disks used before, newest first, by file
//  name, and Browse for another. A tip over the dropdown, and over each row
//  of its open list, has the disk's full path. The confirm button ends the
//  dialog with the chosen disk.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkProDosDialog : public DxuiDialogWindow
{
public:
    using BrowseFn = std::function<bool (HWND owner, std::wstring & ioDisk)>;

    struct Request
    {
        std::wstring               title;
        std::wstring               note;
        std::wstring               confirm;
        bool                       destructive = false;     // a red confirm button, for one that erases a disk
        std::vector<std::wstring>  history;
        BrowseFn                   browse;
    };

    //  False when canceled or the dialog could not open; otherwise
    //  `outDisk` is the chosen disk.
    static bool  Choose (HWND owner, const IDxuiTheme * theme, const Request & request, std::wstring & outDisk);

protected:
    void  OnCreate      () override;
    void  OnDialogTick  () override;

private:
    static constexpr UINT  kTipTickMs = 50;

    void  ShowDisks     (int selected);
    void  ShowTip       (const DxuiMouseEvent & ev);
    void  ShowListTip   (int index);

    Request                     m_request;
    const IDxuiTheme          * m_theme     = nullptr;
    DxuiTooltip                 m_tip;
    bool                        m_listTip   = false;
    std::vector<std::wstring>   m_disks;
    std::wstring                m_chosen;
    bool                        m_confirmed = false;
    DxuiLabel                   m_note;
    DxuiLabel                   m_label;
    DxuiComboBox                m_combo;
    DxuiButton                  m_browse;
    DxuiButton                * m_ok        = nullptr;
    CassoLinkProDosPanel      * m_body      = nullptr;
};
