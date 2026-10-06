#pragma once

#include "Ui/Debugger/ColorLegend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendPanel
//
//  The legend's list, filling the dialog's content area. A list takes points
//  relative to itself, so the panel moves each mouse event there.
//
////////////////////////////////////////////////////////////////////////////////

class ColorLegendPanel : public DxuiPanel
{
public:
    void  Init    (DxuiListView & list);
    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

private:
    DxuiListView  * m_list = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendDialog
//
//  Help > Colors in the debugger: every color that means something, grouped
//  by the pane it is seen in, each as a swatch beside its meaning. It stays
//  open beside the window, which hands it the theme's colors whenever the
//  theme changes, so its swatches always match the panes.
//
////////////////////////////////////////////////////////////////////////////////

class ColorLegendDialog : public DxuiDialogWindow
{
public:
    //  The list's rows: a heading for each pane, then a swatch and a meaning
    //  for each of its colors.
    static std::vector<std::vector<DxuiListView::Cell>>  MakeRows (const ColorLegend::Palette & palette, const IDxuiTheme & theme);

    //  Shows the legend beside `owner`, making it the first time; it stays
    //  open while the debugger is used.
    HRESULT  Open      (HWND owner, const IDxuiTheme * theme, const ColorLegend::Palette & palette);

    //  The colors of a theme the window has changed to.
    void     SetColors (const IDxuiTheme * theme, const ColorLegend::Palette & palette);

    bool     IsShown   () const { return IsCreated() && m_isShown; }

    static constexpr int  kWidthDip        = 480;
    static constexpr int  kHeightDip       = 600;
    static constexpr int  kSwatchColumnDip = 36;

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme               * m_theme   = nullptr;
    ColorLegend::Palette             m_palette;
    DxuiListView                     m_list;
    ColorLegendPanel               * m_body    = nullptr;
    bool                             m_isShown = false;
};
