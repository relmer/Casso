#pragma once

#include "Pch.h"


class DxuiLabel;
class DownloadBodyPanel;





////////////////////////////////////////////////////////////////////////////////
//
//  DownloadContentPanel
//
//  Stacks StartupDownloadDialog's intro label above its asset-row body.
//  Lays out in physical pixels (the hosted dialog passes a px content rect)
//  so the intro height scales with DPI. Does not own either child -- Adopt
//  only wires them into the panel tree.
//
//  The intro's height starts as an estimate from its line breaks, for the
//  dialog's first size; TryMeasureHeightPx replaces it with the wrapped
//  height the window's renderer measures, so a long intro is never clipped.
//
////////////////////////////////////////////////////////////////////////////////

class DownloadContentPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    // `introHeightDip` is the estimate, `introPadDip` the gap below the
    // intro, which it includes.
    void  Init   (DxuiLabel * intro, DownloadBodyPanel * body, int introHeightDip, int introPadDip);
    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    // The measured intro and its gap, above the body's rows at their
    // laid-out height.
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    DxuiLabel          *  m_intro          = nullptr;
    DownloadBodyPanel  *  m_body           = nullptr;
    int                   m_introHeightDip = 0;
    int                   m_introPadDip    = 0;
    int                   m_introHeightPx  = 0;   // measured; 0 until TryMeasureHeightPx
    DxuiDpiScaler         m_scaler;
};
