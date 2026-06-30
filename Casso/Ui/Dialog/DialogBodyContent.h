#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"

#include "DialogDefinition.h"


class IDxuiControl;




////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContent
//
//  Dxui content panel that renders a DialogDefinition's body runs as a
//  vertical stack: normal runs become wrapped DxuiLabels, hyperlink runs
//  become DxuiButton(Link) controls whose click opens the URL. Lays itself
//  out in physical pixels (the hosted dialog passes a px content rect), so
//  the per-run heights scale with DPI. Used by the Dxui dialog path that
//  replaces the legacy DialogPrimitive for simple + hyperlink dialogs.
//
////////////////////////////////////////////////////////////////////////////////

class DialogBodyContent : public DxuiPanel
{
public:
    // Build one child widget per body run. `textArgb` is the color used
    // for normal (non-hyperlink) runs.
    void  SetRuns  (const std::vector<DialogTextRun> & runs, uint32_t textArgb);

    // Estimated stacked height in DIP, used by the caller to size the
    // hosting dialog.
    int   PreferredHeightDip () const;

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;


private:
    struct Item
    {
        IDxuiControl *  widget = nullptr;
        int             lines  = 1;
    };


    std::vector<Item>  m_items;
};
