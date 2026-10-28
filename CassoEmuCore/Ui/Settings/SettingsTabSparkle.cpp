#include "Pch.h"

#include "Ui/Settings/SettingsTabSparkle.h"

#include "Ui/Settings/SettingsSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsTabSparkle::Start
//
////////////////////////////////////////////////////////////////////////////////

void SettingsTabSparkle::Start (int pageIndex)
{
    if (pageIndex >= 0 && DxuiSystemSettings::Instance().AreAnimationsEnabled())
    {
        m_startMs[pageIndex] = (int64_t) GetTickCount64();
        m_sheet.UpdateTickInterval();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsTabSparkle::GetProgress
//
//  The current sweep's progress over a page's tab, 0 to 1, or nothing
//  between sweeps, once the tab has been opened, or while it is hidden.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<float> SettingsTabSparkle::GetProgress (int pageIndex) const
{
    std::optional<float>  progress;
    int64_t               phase   = 0;
    auto                  start   = m_startMs.find (pageIndex);
    bool                  running = start != m_startMs.end() && m_sheet.IsPageVisible (pageIndex) &&
                                    m_sheet.GetActiveIndex() != pageIndex;



    if (running)
    {
        phase = ((int64_t) GetTickCount64() - start->second) % kPeriodMs;

        if (phase < DxuiShimmer::kSweepMs)
        {
            progress = (float) phase / (float) DxuiShimmer::kSweepMs;
        }
    }

    return progress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsTabSparkle::IsAnySweeping
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsTabSparkle::IsAnySweeping() const
{
    bool  sweeping = false;



    for (const auto & [pageIndex, startMs] : m_startMs)
    {
        sweeping = sweeping || GetProgress (pageIndex).has_value();
    }

    return sweeping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsTabSparkle::Update
//
//  Opening a sparkling tab ends its sparkle for good, as does hiding it
//  again by turning its card off.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsTabSparkle::Update()
{
    std::erase_if (m_startMs, [this] (const auto & entry)
    {
        return !m_sheet.IsPageVisible (entry.first) || m_sheet.GetActiveIndex() == entry.first;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsTabSparkle::PaintAll
//
//  Each sparkling tab's sweep over its label.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsTabSparkle::PaintAll (IDxuiTextRenderer & text)
{
    std::optional<float>  progress;



    for (const auto & [pageIndex, startMs] : m_startMs)
    {
        progress = GetProgress (pageIndex);

        if (progress.has_value())
        {
            Paint (text, pageIndex, *progress);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsTabSparkle::Paint
//
//  DxuiShimmer over a page's tab label, drawn where and as the tab strip
//  draws it; the glints span only the label text's width.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsTabSparkle::Paint (IDxuiTextRenderer & text, int pageIndex, float progress)
{
    constexpr float  kGlintDip = 2.5f;



    HRESULT                    hr      = S_OK;
    const DxuiTabStrip::Tab *  tab     = m_sheet.FindTabOfPage (pageIndex);
    DxuiDpiScaler              scaler;
    float                      fontPx  = 0.0f;
    float                      padX    = 0.0f;
    float                      x       = 0.0f;
    float                      y       = 0.0f;
    float                      w       = 0.0f;
    float                      h       = 0.0f;
    float                      labelW  = 0.0f;
    float                      labelH  = 0.0f;



    BAIL_OUT_IF (tab == nullptr, S_OK);

    scaler.SetDpi (m_sheet.GetDpi());
    fontPx = scaler.ToPxf (DxuiTabStrip::kLabelFontDip);
    padX   = scaler.ToPxf (DxuiTabStrip::kLabelPadXDip);
    x      = (float) tab->rect.left + padX;
    y      = (float) tab->rect.top;
    w      = (float) (tab->rect.right - tab->rect.left) - padX * 2.0f;
    h      = (float) (tab->rect.bottom - tab->rect.top);

    hr = text.MeasureString (tab->label.c_str(), fontPx, DxuiTheme::kBodyFace, labelW, labelH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    labelW = std::min (labelW, w);

    DxuiShimmer::Paint (text, x, y, w, h, x + (w - labelW) * 0.5f, labelW, fontPx, scaler.ToPxf (kGlintDip), progress, m_glints,
                        [&] (uint32_t argb)
                        {
                            HRESULT  drawHr = S_OK;

                            drawHr = text.DrawString (tab->label.c_str(), x, y, w, h, argb, fontPx, DxuiTheme::kBodyFace,
                                                      DxuiTextHAlign::Center, DxuiTextVAlign::Center);
                            IGNORE_RETURN_VALUE (drawHr, S_OK);
                        });

Error:
    return;
}
