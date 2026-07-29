#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"


class DxuiLabel;
class DownloadBodyPanel;




////////////////////////////////////////////////////////////////////////////////
//
//  DownloadContentPanel
//
//  Stacks StartupDownloadDialog's intro label above its asset-row body.
//  Lays out in physical pixels (the hosted dialog passes a px content rect)
//  so the fixed intro height scales with DPI. Does not own either child --
//  Adopt only wires them into the panel tree.
//
////////////////////////////////////////////////////////////////////////////////

class DownloadContentPanel : public DxuiPanel
{
public:
    void  Init   (DxuiLabel * intro, DownloadBodyPanel * body, int introHeightDip);
    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

private:
    DxuiLabel          *  m_intro          = nullptr;
    DownloadBodyPanel  *  m_body           = nullptr;
    int                   m_introHeightDip = 0;
};
