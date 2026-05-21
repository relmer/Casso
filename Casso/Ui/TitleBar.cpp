#include "Pch.h"

#include "TitleBar.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Inline RML / RCSS markup
//
//  Hard-coded for P4. P5 will let themes override via Resources/.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char * kTitleBarRml = R"RML(
<rml>
    <head>
        <title>Casso Title Bar</title>
        <style>
            body
            {
                width: 100%;
                height: 32px;
                background-color: #1f1f1f;
                color: #f0f0f0;
                font-family: sans-serif;
                font-size: 13dp;
                display: block;
            }
            .titlebar
            {
                width: 100%;
                height: 32px;
                display: block;
            }
            .app-title
            {
                display: inline-block;
                padding: 7dp 14dp;
                color: #f0f0f0;
            }
            .sysbtn-strip
            {
                position: absolute;
                top: 0px;
                right: 0px;
                height: 32px;
            }
            .sysbtn
            {
                display: inline-block;
                width: 46dp;
                height: 32px;
                text-align: center;
                color: #f0f0f0;
                vertical-align: middle;
            }
            .sysbtn:hover { background-color: #2d2d2d; }
            .sysbtn.close:hover { background-color: #c42b1c; }
        </style>
    </head>
    <body>
        <div class="titlebar">
            <span class="app-title">Casso</span>
            <div class="sysbtn-strip">
                <span class="sysbtn min">&#x2013;</span>
                <span class="sysbtn max">&#x25A1;</span>
                <span class="sysbtn close">&#x2715;</span>
            </div>
        </div>
    </body>
</rml>
)RML";
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::DefaultTitleHeight
//
////////////////////////////////////////////////////////////////////////////////

int  TitleBarLayout::DefaultTitleHeight (UINT dpi)
{
    int  cy   = 0;
    int  padd = 0;


    cy   = GetSystemMetricsForDpi (SM_CYCAPTION,      dpi);
    padd = GetSystemMetricsForDpi (SM_CXPADDEDBORDER, dpi);

    if (cy <= 0)
    {
        return 32;
    }

    return cy + padd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::DefaultButtonWidth
//
//  Win11 uses a 46-DIP-wide system button. Scale by DPI/96.
//
////////////////////////////////////////////////////////////////////////////////

int  TitleBarLayout::DefaultButtonWidth (UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? 96 : dpi;

    return MulDiv (46, (int) effectiveDpi, 96);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::Compute
//
////////////////////////////////////////////////////////////////////////////////

TitleBarLayoutOutput TitleBarLayout::Compute (const TitleBarLayoutInput & in)
{
    TitleBarLayoutOutput  out         = {};
    int                   buttonStripL = 0;
    int                   buttonStripR = 0;
    int                   minLeft      = 0;
    int                   maxLeft      = 0;
    int                   closeLeft    = 0;
    int                   buttonWidth  = 0;


    buttonWidth = in.buttonWidth > 0 ? in.buttonWidth : 46;

    // Title-bar rect spans full client width across the top.
    out.titleBar = { 0, 0, in.clientWidth, in.titleHeight };

    // No buttons if either dimension collapses.
    if (in.clientWidth <= 0 || in.titleHeight <= 0)
    {
        out.dragRegion = out.titleBar;
        return out;
    }

    // Buttons stack right-to-left as close | max | min so when the
    // window is wider than 3 * buttonWidth we get the usual ordering.
    closeLeft    = in.clientWidth - buttonWidth;
    maxLeft      = closeLeft       - buttonWidth;
    minLeft      = maxLeft         - buttonWidth;
    buttonStripL = minLeft;
    buttonStripR = in.clientWidth;

    if (minLeft < 0)
    {
        // Window narrower than the strip — collapse all buttons to
        // zero-width on the left so hit-test stops returning them.
        out.minButton   = {};
        out.maxButton   = {};
        out.closeButton = {};
        out.dragRegion  = out.titleBar;
        return out;
    }

    out.minButton   = { minLeft,   0, minLeft   + buttonWidth, in.titleHeight };
    out.maxButton   = { maxLeft,   0, maxLeft   + buttonWidth, in.titleHeight };
    out.closeButton = { closeLeft, 0, closeLeft + buttonWidth, in.titleHeight };

    // Drag region = title strip minus the button strip on the right.
    out.dragRegion = { 0, 0, buttonStripL, in.titleHeight };

    UNREFERENCED_PARAMETER (buttonStripR);
    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar
//
////////////////////////////////////////////////////////////////////////////////

TitleBar::TitleBar()
{
    // Reasonable 96-DPI defaults so tests that read GetButtonRect()
    // before UpdateGeometry() get sensible output.
    UpdateGeometry (1024, 96);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~TitleBar
//
////////////////////////////////////////////////////////////////////////////////

TitleBar::~TitleBar()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar::Show
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TitleBar::Show (Rml::Context * context)
{
    HRESULT  hr = S_OK;


    CPR (context);

    if (m_doc != nullptr)
    {
        Hide();
    }

    m_context = context;
    m_doc     = m_context->LoadDocumentFromMemory (kTitleBarRml,
                                                    "title_bar.rml");
    CPR (m_doc);

    m_doc->Show();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar::Hide
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::Hide()
{
    if (m_doc != nullptr && m_context != nullptr)
    {
        m_context->UnloadDocument (m_doc);
    }

    m_doc     = nullptr;
    m_context = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar::UpdateGeometry
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::UpdateGeometry (int clientWidth, UINT dpi)
{
    TitleBarLayoutInput  in = {};


    in.clientWidth = clientWidth;
    in.titleHeight = TitleBarLayout::DefaultTitleHeight (dpi);
    in.buttonWidth = TitleBarLayout::DefaultButtonWidth (dpi);

    m_layout = TitleBarLayout::Compute (in);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar::GetButtonRect
//
////////////////////////////////////////////////////////////////////////////////

RECT TitleBar::GetButtonRect (SystemButton which) const
{
    switch (which)
    {
        case SystemButton::Minimize: return m_layout.minButton;
        case SystemButton::Maximize: return m_layout.maxButton;
        case SystemButton::Close:    return m_layout.closeButton;
    }

    return RECT {};
}
