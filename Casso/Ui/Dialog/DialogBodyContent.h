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

    // Optional centered top icon (premultiplied BGRA), drawn above the
    // run stack at `displaySizeDip` square. Pass empty pixels for none.
    void  SetIcon  (std::vector<uint32_t> bgraPremul, int srcW, int srcH, int displaySizeDip);

    // Estimated stacked height in DIP, used by the caller to size the
    // hosting dialog.
    int   PreferredHeightDip () const;

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;


private:
    struct Item
    {
        IDxuiControl *  widget = nullptr;
        int             lines  = 1;
    };


    std::vector<Item>      m_items;
    std::vector<uint32_t>  m_iconPixels;
    int                    m_iconSrcW    = 0;
    int                    m_iconSrcH    = 0;
    int                    m_iconSizeDip = 0;
    RECT                   m_iconRectPx  = {};
};
