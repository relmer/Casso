#include "Pch.h"

#include "Ui/Debugger/ReplayProgressDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ReplayProgressPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ReplayProgressPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr     = S_OK;
    RECT     bounds = GetBounds();
    float    left   = (float) bounds.left;
    float    width  = (float) (bounds.right - bounds.left);
    float    textH  = m_scaler.ToPxf ((float) kTextDip);
    float    barTop = (float) bounds.top + textH + m_scaler.ToPxf ((float) kGapDip);
    float    barH   = m_scaler.ToPxf ((float) kBarDip);
    float    corner = barH * 0.5f;



    if (!IsVisible())
    {
        return;
    }

    hr = text.DrawString (m_text.c_str(), left, (float) bounds.top, width, textH,
                          theme.Foreground(), m_scaler.ToPxf (theme.BodyFont().sizeDip), theme.BodyFont().face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.FillRoundedRect    (left, barTop, width, barH, corner, theme.BackgroundElevated());
    painter.FillRoundedRect    (left, barTop, width * std::clamp (m_fraction, 0.0f, 1.0f), barH, corner, theme.Accent());
    painter.OutlineRoundedRect (left, barTop, width, barH, corner, std::max (1.0f, m_scaler.ToPxf (1.0f)), theme.Border());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressDialog::IsDue
//
////////////////////////////////////////////////////////////////////////////////

bool ReplayProgressDialog::IsDue (const ReplayProgress & progress)
{
    return progress.isReplaying && progress.elapsedMs >= kShowAfterMs && progress.fraction >= 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressDialog::GetText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ReplayProgressDialog::GetText (const ReplayProgress & progress, bool isStopping)
{
    static constexpr float  kPercent = 100.0f;



    if (isStopping)
    {
        return L"Stopping the search...";
    }

    return std::format (L"Searching history for a breakpoint: {}%", std::lround (std::clamp (progress.fraction, 0.0f, 1.0f) * kPercent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void ReplayProgressDialog::OnCreate()
{
    m_panel      = CreateDialogContent<ReplayProgressPanel>();
    m_stopButton = AddDialogButton (L"Stop", IDCANCEL);

    m_stopButton->SetOnClick ([this]()
    {
        if (m_stop && !m_isStopping)
        {
            m_stop();
        }

        m_isStopping = true;
        Refresh();
    });

    SetDialogTickIntervalMs (kTickMs);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void ReplayProgressDialog::OnDialogTick()
{
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressDialog::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void ReplayProgressDialog::Refresh()
{
    ReplayProgress  progress = m_progress ? m_progress() : ReplayProgress();



    m_panel->SetProgress (GetText (progress, m_isStopping), progress.fraction);
    m_stopButton->SetEnabled (!m_isStopping);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgressDialog::Open
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ReplayProgressDialog> ReplayProgressDialog::Open (HWND owner, const IDxuiTheme * theme, ProgressFn progress, StopFn stop)
{
    HRESULT                                hr     = S_OK;
    std::unique_ptr<ReplayProgressDialog>  dialog = std::make_unique<ReplayProgressDialog>();
    DxuiWindow::CreateParams               params;



    dialog->m_progress = std::move (progress);
    dialog->m_stop     = std::move (stop);

    params.title                    = L"Reverse continue";
    params.hInstance                = (owner != nullptr) ? (HINSTANCE) GetWindowLongPtrW (owner, GWLP_HINSTANCE) : GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kWidthDip, kHeightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog->Create (params);
    CHRA (hr);

    dialog->SetTheme (theme);
    dialog->ShowModelessDialog (IDCANCEL);

Error:
    return SUCCEEDED (hr) ? std::move (dialog) : nullptr;
}
