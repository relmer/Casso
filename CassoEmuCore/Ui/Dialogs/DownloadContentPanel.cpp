#include "Pch.h"

#include "DownloadContentPanel.h"

#include "DownloadBodyPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Init
//
////////////////////////////////////////////////////////////////////////////////

void DownloadContentPanel::Init (DxuiLabel * intro, DownloadBodyPanel * body, int introHeightDip, int introPadDip)
{
    m_intro          = intro;
    m_body           = body;
    m_introHeightDip = introHeightDip;
    m_introPadDip    = introPadDip;

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
    int  ih = (m_introHeightPx > 0) ? m_introHeightPx : scaler.ToPx (m_introHeightDip);



    SetBounds (boundsPx);

    m_scaler = scaler;

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





////////////////////////////////////////////////////////////////////////////////
//
//  TryMeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

bool DownloadContentPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    HRESULT  hr       = S_OK;
    float    fontDip  = 0.0f;
    float    measureW = 0.0f;
    float    measureH = 0.0f;
    RECT     body     = {};



    CBR (m_intro != nullptr && m_body != nullptr && widthPx > 0);

    fontDip = (m_intro->GetFontSizeDip() > 0.0f) ? m_intro->GetFontSizeDip() : theme.BodyFont().sizeDip;

    hr = text.MeasureStringWrapped (m_intro->GetText().c_str(), m_scaler.ToPxf (fontDip), DxuiTheme::kBodyFace,
                                    (float) widthPx, measureW, measureH);
    CHR (hr);

    body            = m_body->GetBounds();
    m_introHeightPx = (int) std::ceil (measureH) + m_scaler.ToPx (m_introPadDip);
    heightPx        = m_introHeightPx + (body.bottom - body.top);

Error:
    return SUCCEEDED (hr);
}
