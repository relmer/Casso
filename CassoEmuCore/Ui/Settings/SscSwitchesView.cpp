#include "Pch.h"

#include "Ui/Settings/SscSwitchesView.h"
#include "Ui/Settings/SscSettingsModel.h"





// The banks are drawn in their own colors rather than the theme's: they are a
// picture of the blue DIP switch packages on the card, with white sliders and
// white markings, and those are what make them recognizable on any theme.
static constexpr uint32_t  s_kBodyEdgeColor   = 0xFF1A3A6E;
static constexpr uint32_t  s_kBodyColor       = 0xFF2B5CA8;
static constexpr uint32_t  s_kSlotColor       = 0xFF0F1E38;
static constexpr uint32_t  s_kSliderColor     = 0xFFF2F2EE;
static constexpr uint32_t  s_kSliderHotColor  = 0xFFFFFFFF;
static constexpr uint32_t  s_kSliderEdgeColor = 0xFFB8C4D8;
static constexpr uint32_t  s_kMarkingColor    = 0xFFFFFFFF;

static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

// Geometry in DIPs. A bank's slots sit one pitch apart between its side pads;
// ON is printed in the band above them and the switch numbers in the band
// below, as on the package.
static constexpr float  s_kCaptionDip    = 18.0f;
static constexpr float  s_kSidePadDip    = 10.0f;
static constexpr float  s_kTopBandDip    = 16.0f;
static constexpr float  s_kBottomBandDip = 16.0f;
static constexpr float  s_kPitchDip      = 18.0f;
static constexpr float  s_kSlotWidthDip  = 10.0f;
static constexpr float  s_kSlotHeightDip = 28.0f;
static constexpr float  s_kSliderInset   = 1.5f;
static constexpr float  s_kBankGapDip    = 24.0f;
static constexpr float  s_kEdgeDip       = 1.5f;
static constexpr float  s_kCornerDip     = 3.0f;
static constexpr float  s_kMarkFontDip   = 10.0f;
static constexpr float  s_kCaptionFontDip = 12.0f;

static constexpr int    s_kSwitchesPerBank = SscSwitches::kSwitchesPerBank;
static constexpr float  s_kBodyWidthDip    = s_kSidePadDip * 2.0f + s_kPitchDip * (s_kSwitchesPerBank - 1) + s_kSlotWidthDip;
static constexpr float  s_kBodyHeightDip   = s_kTopBandDip + s_kSlotHeightDip + s_kBottomBandDip;

static_assert (s_kBodyWidthDip * 2.0f + s_kBankGapDip <= (float) SscSwitchesView::kWidthDip,  "the banks fit the view's width");
static_assert (s_kCaptionDip + s_kBodyHeightDip     <= (float) SscSwitchesView::kHeightDip, "the banks fit the view's height");





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void SscSwitchesView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    SetBounds (boundsPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildArt
//
//  SW1 on the left, SW2 to its right; within a bank, switch 1 at the left.
//
////////////////////////////////////////////////////////////////////////////////

SscSwitchArt SscSwitchesView::BuildArt (float left, float top, float pxPerDip)
{
    SscSwitchArt  art;
    float         bodyTop = top + s_kCaptionDip * pxPerDip;



    art.cornerRadius  = s_kCornerDip      * pxPerDip;
    art.markFontPx    = s_kMarkFontDip    * pxPerDip;
    art.captionFontPx = s_kCaptionFontDip * pxPerDip;

    for (int b = 0; b < 2; ++b)
    {
        SscSwitchArtBank &  bank     = art.banks[static_cast<size_t> (b)];
        float               bodyLeft = left + (s_kBodyWidthDip + s_kBankGapDip) * pxPerDip * (float) b;

        bank.bank    = (b == 0) ? SscBank::Sw1 : SscBank::Sw2;
        bank.caption = { bodyLeft, top, s_kBodyWidthDip * pxPerDip, s_kCaptionDip * pxPerDip };
        bank.body    = { bodyLeft, bodyTop, s_kBodyWidthDip * pxPerDip, s_kBodyHeightDip * pxPerDip };
        bank.onMark  = { bodyLeft + s_kSidePadDip * pxPerDip, bodyTop, s_kPitchDip * 2.0f * pxPerDip, s_kTopBandDip * pxPerDip };

        for (int n = 1; n <= s_kSwitchesPerBank; ++n)
        {
            SscSwitchArtSwitch  sw;
            float               slotLeft = bodyLeft + (s_kSidePadDip + s_kPitchDip * (float) (n - 1)) * pxPerDip;
            float               slotTop  = bodyTop  + s_kTopBandDip * pxPerDip;
            float               labelW   = s_kPitchDip * pxPerDip;

            sw.bank   = bank.bank;
            sw.number = n;
            sw.slot   = { slotLeft, slotTop, s_kSlotWidthDip * pxPerDip, s_kSlotHeightDip * pxPerDip };
            sw.slider = { slotLeft + s_kSliderInset * pxPerDip,
                          slotTop  + s_kSliderInset * pxPerDip,
                          (s_kSlotWidthDip - s_kSliderInset * 2.0f) * pxPerDip,
                          (s_kSlotHeightDip * 0.5f - s_kSliderInset) * pxPerDip };
            sw.label  = { slotLeft + (s_kSlotWidthDip * pxPerDip - labelW) * 0.5f,
                          slotTop + s_kSlotHeightDip * pxPerDip,
                          labelW,
                          s_kBottomBandDip * pxPerDip };

            art.switches.push_back (sw);
        }
    }

    return art;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildArt (this view)
//
////////////////////////////////////////////////////////////////////////////////

SscSwitchArt SscSwitchesView::BuildArt() const
{
    RECT  bounds = GetBounds();



    return BuildArt ((float) bounds.left, (float) bounds.top, m_scaler.ToPxf (1.0f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
//  A switch's whole slot is its target, so a click on either half flips it.
//
////////////////////////////////////////////////////////////////////////////////

bool SscSwitchesView::HitTest (const SscSwitchArt & art, float x, float y, SscBank & bank, int & number)
{
    bool  found = false;



    for (const SscSwitchArtSwitch & sw : art.switches)
    {
        if (!found && sw.slot.Contains (x, y))
        {
            bank   = sw.bank;
            number = sw.number;
            found  = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Each bank: its caption, the body with a darker edge, ON along the top band,
//  and per switch the dark slot, the slider in the half the switch is set to,
//  and the number beneath.
//
////////////////////////////////////////////////////////////////////////////////

void SscSwitchesView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    SscSwitchArt  art;
    float         edge  = 0.0f;
    HRESULT       hr    = S_OK;
    size_t        i     = 0;



    if (!IsVisible())
    {
        return;
    }

    art  = BuildArt();
    edge = m_scaler.ToPxf (s_kEdgeDip);

    for (const SscSwitchArtBank & bank : art.banks)
    {
        hr = text.DrawString (bank.bank == SscBank::Sw1 ? L"SW1" : L"SW2",
                              bank.caption.x, bank.caption.y, bank.caption.w, bank.caption.h,
                              theme.Foreground(), art.captionFontPx, s_kpszFont,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        painter.FillRoundedRect (bank.body.x, bank.body.y, bank.body.w, bank.body.h, art.cornerRadius, s_kBodyEdgeColor);
        painter.FillRoundedRect (bank.body.x + edge, bank.body.y + edge, bank.body.w - edge * 2.0f, bank.body.h - edge * 2.0f,
                                 art.cornerRadius, s_kBodyColor);

        hr = text.DrawString (L"ON", bank.onMark.x, bank.onMark.y, bank.onMark.w, bank.onMark.h,
                              s_kMarkingColor, art.markFontPx, s_kpszFont,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Bold, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (i = 0; i < art.switches.size(); ++i)
    {
        const SscSwitchArtSwitch &  sw       = art.switches[i];
        bool                        isOn     = m_switches.IsOn (sw.bank, sw.number);
        float                       sliderY  = isOn ? sw.slider.y : sw.slot.y + sw.slot.h - sw.slider.h - (sw.slider.y - sw.slot.y);
        bool                        isHot    = static_cast<int> (i) == m_hover;

        painter.FillRect (sw.slot.x, sw.slot.y, sw.slot.w, sw.slot.h, s_kSlotColor);
        painter.FillRect (sw.slider.x, sliderY, sw.slider.w, sw.slider.h, s_kSliderEdgeColor);
        painter.FillRect (sw.slider.x, sliderY, sw.slider.w, sw.slider.h - edge, isHot ? s_kSliderHotColor : s_kSliderColor);

        hr = text.DrawString (std::to_wstring (sw.number).c_str(), sw.label.x, sw.label.y, sw.label.w, sw.label.h,
                              s_kMarkingColor, art.markFontPx, s_kpszFont,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  A press on a switch flips it through the page; moving over one lights its
//  slider.
//
////////////////////////////////////////////////////////////////////////////////

bool SscSwitchesView::OnMouse (const DxuiMouseEvent & ev)
{
    SscSwitchArt  art      = BuildArt();
    SscBank       bank     = SscBank::Sw1;
    int           number   = 0;
    bool          isOver   = HitTest (art, (float) ev.positionDip.x, (float) ev.positionDip.y, bank, number);
    int           hover    = isOver ? (bank == SscBank::Sw1 ? 0 : s_kSwitchesPerBank) + number - 1 : -1;
    bool          handled  = false;



    if (ev.kind == DxuiMouseEventKind::Leave)
    {
        hover = -1;
    }

    handled = hover != m_hover;
    m_hover = hover;

    if (isOver && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        if (m_onToggle)
        {
            m_onToggle (bank, number);
        }

        handled = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAccessibleName
//
//  Both banks' positions, as the text readout listed them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SscSwitchesView::GetAccessibleName() const
{
    return SscSettingsModel::GetReadout (m_switches, SscBank::Sw1) + L", " +
           SscSettingsModel::GetReadout (m_switches, SscBank::Sw2);
}
