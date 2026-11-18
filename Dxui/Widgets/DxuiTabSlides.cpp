#include "Pch.h"

#include "Widgets/DxuiTabSlides.h"
#include "Theme/DxuiTheme.h"
#include "DxuiTabStrip.h"
#include "DxuiTabDragModel.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiAnimation.h"
#include "Core/DxuiSystemSettings.h"
#include "Core/DxuiTextElide.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Render/DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabSlides::DxuiTabSlides
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabSlides::DxuiTabSlides (DxuiTabStrip & strip)
    : m_strip (strip)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetIncomingDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabSlides::SetIncomingDrag (int x, int widthPx)
{
    m_incoming      = true;
    m_incomingX     = x;
    m_incomingWidth = widthPx;

    UpdateDragGap();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearIncomingDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabSlides::ClearIncomingDrag()
{
    m_incoming = false;

    UpdateDragGap();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDragGap
//
//  Where the gap is, for the tab dragged along the strip or the one dragged
//  in, and the slides that open it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabSlides::UpdateDragGap()
{
    std::vector<int>  widths;
    int               origin = m_strip.m_tabs.empty() ? 0 : (int) m_strip.m_tabs.front().rect.left;
    int               width  = 0;



    for (const DxuiTabStrip::Tab & tab : m_strip.m_tabs)
    {
        widths.push_back ((int) (tab.rect.right - tab.rect.left));
    }

    m_gap = -1;

    if (m_strip.m_dragging && !m_strip.m_tornOff && m_strip.m_pressed >= 0 && m_strip.m_pressed < (int) widths.size())
    {
        width = widths[(size_t) m_strip.m_pressed];
        m_gap = DxuiTabDragModel::GetGapIndex (widths, m_strip.m_pressed, origin, m_strip.GetContentX (m_strip.m_dragX) - m_strip.m_grab.x, width);
    }
    else if (!m_strip.m_dragging && m_incoming)
    {
        width = m_incomingWidth;
        m_gap = DxuiTabDragModel::GetGapIndex (widths, -1, origin, m_strip.GetContentX (m_incomingX) - width / 2, width);
    }

    RetargetSlides();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RetargetSlides
//
//  Each tab, and the + button after them, slides from where it is drawn now
//  to where the drag puts it. With no drag, that is where the host laid it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabSlides::RetargetSlides()
{
    std::vector<int>  widths;
    std::vector<int>  lefts;
    int               origin  = m_strip.m_tabs.empty() ? 0 : (int) m_strip.m_tabs.front().rect.left;
    int               end     = m_strip.m_tabs.empty() ? 0 : (int) m_strip.m_tabs.back().rect.right;
    int               dragged = m_strip.m_dragging ? m_strip.m_pressed : -1;
    int64_t           now     = GetNowMs();
    size_t            i       = 0;



    if (m_slides.size() != m_strip.m_tabs.size() + 1)
    {
        SnapSlides();
    }

    for (const DxuiTabStrip::Tab & tab : m_strip.m_tabs)
    {
        widths.push_back ((int) (tab.rect.right - tab.rect.left));
        lefts.push_back  ((int) tab.rect.left);
    }

    if (m_strip.m_dragging || m_incoming)
    {
        DxuiTabDragModel::GetLefts (widths, dragged, origin, m_gap,
                                    m_strip.m_dragging ? ((dragged >= 0) ? widths[(size_t) dragged] : 0) : m_incomingWidth,
                                    lefts, end);
    }

    lefts.push_back (end);

    for (i = 0; i < m_slides.size(); i++)
    {
        Slide &  slide = m_slides[i];
        float    to    = (float) lefts[i];

        if (slide.to != to)
        {
            slide.from    = m_animate ? GetSlideLeft (i) : to;
            slide.to      = to;
            slide.startMs = now;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SnapSlides
//
//  Every tab and the + button straight to where the host laid them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabSlides::SnapSlides()
{
    size_t  i = 0;



    m_slides.assign (m_strip.m_tabs.size() + 1, Slide());

    for (i = 0; i < m_slides.size(); i++)
    {
        float  left = (i < m_strip.m_tabs.size()) ? (float) m_strip.m_tabs[i].rect.left : (m_strip.m_tabs.empty() ? 0.0f : (float) m_strip.m_tabs.back().rect.right);

        m_slides[i].from = left;
        m_slides[i].to   = left;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiTabSlides::GetNowMs() const
{
    return m_clock ? m_clock() : (int64_t) GetTickCount64();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSlideLeft
//
//  Where item `item` is drawn now: a tab by its index, or the + button after
//  the last of them, easing out toward its place.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTabSlides::GetSlideLeft (size_t item) const
{
    Slide    slide;
    float    left    = 0.0f;
    int64_t  elapsed = 0;



    if (item >= m_slides.size())
    {
        return (item < m_strip.m_tabs.size()) ? (float) m_strip.m_tabs[item].rect.left : (m_strip.m_tabs.empty() ? 0.0f : (float) m_strip.m_tabs.back().rect.right);
    }

    slide   = m_slides[item];
    elapsed = GetNowMs() - slide.startMs;
    left    = slide.to;

    if (m_animate && slide.from != slide.to && elapsed >= 0 && elapsed < DxuiTabStrip::s_kSlideMs)
    {
        left = slide.from + (slide.to - slide.from) * DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut, (float) elapsed / (float) DxuiTabStrip::s_kSlideMs);
    }

    return left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSliding
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabSlides::IsSliding() const
{
    int64_t  now     = GetNowMs();
    bool     sliding = false;



    for (const Slide & slide : m_slides)
    {
        sliding = sliding || (m_animate && slide.from != slide.to && now - slide.startMs < DxuiTabStrip::s_kSlideMs);
    }

    return sliding;
}
