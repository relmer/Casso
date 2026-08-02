#include "Pch.h"

#include "DownloadContentPanel.h"

#include "DownloadBodyPanel.h"
#include "Widgets/DxuiLabel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Init
//
////////////////////////////////////////////////////////////////////////////////

void DownloadContentPanel::Init (DxuiLabel * intro, DownloadBodyPanel * body, int introHeightDip)
{
    m_intro          = intro;
    m_body           = body;
    m_introHeightDip = introHeightDip;

    Adopt (*intro);
    Adopt (*body);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void DownloadContentPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  ih = scaler.Px (m_introHeightDip);



    SetBounds (boundsPx);

    if (m_intro != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + ih };

        m_intro->Layout (r, scaler);
    }

    if (m_body != nullptr)
    {
        RECT  r = { boundsPx.left, boundsPx.top + ih, boundsPx.right, boundsPx.bottom };

        m_body->Layout (r, scaler);
    }
}
