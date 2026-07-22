#include "Pch.h"

#include "MonitorFrame.h"
#include "CassoBranding.h"
#include "Render/IDxuiPainter.h"




// Apple Monitor //c palette -- warm snow-white/platinum plastic lit from
// above. Hardcoded for now; a later pass keys the housing color off the
// machine + color mode (mono vs the AppleColor Composite shell).
static constexpr uint32_t  s_kShellHilite    = 0xFFECE8DE;   // top-lit platinum
static constexpr uint32_t  s_kShellBase      = 0xFFDAD5C8;   // main platinum
static constexpr uint32_t  s_kShellShadow    = 0xFFB6B0A1;   // lower / edge shade
static constexpr uint32_t  s_kGlassFrame     = 0xFF2A2926;   // dark bezel lip around the CRT
static constexpr uint32_t  s_kBevelShade     = 0xFF938E80;   // shaded platinum: recessed bevel wall
static constexpr uint32_t  s_kShadowArgb     = 0x66000000;   // soft contact shadow under the shell
static constexpr uint32_t  s_kLogoBorderArgb = 0xFF4A4A4A;   // dark gray outline around the chin logo

// Desk / wall backdrop filling the area around the (bounded) monitor. The
// window is usually far wider than a 4:3 monitor, so the surround must not be
// platinum or the shell would read as an edge-to-edge wall.
static constexpr uint32_t  s_kDeskTop        = 0xFF1B1A18;   // dark warm gray, top
static constexpr uint32_t  s_kDeskBot        = 0xFF27241F;   // slightly lifted at desk

// Power lamp: the //c case-switch strip's lit indicator (a slanted green lens),
// which is also the "/" power mark on the real //c monitor's bezel.
static constexpr uint32_t  s_kLampGreen      = 0xFF3CE070;   // lit green lens
static constexpr uint32_t  s_kLampGlow       = 0xFF2FBF5F;   // green bloom
static constexpr uint32_t  s_kLampEdge       = 0xFF1C6E3A;   // darker-green lens rim (not black)
static constexpr uint32_t  s_kLampSpecular   = 0x99EFFFF0;   // light catch near the top

static constexpr int       s_kBaseDpi          = 96;
static constexpr int       s_kScreenNativeHDp  = 384;   // native framebuffer height; anchors SceneScale
static constexpr int       s_kGlassLipDp       = 6;     // dark bezel lip thickness
static constexpr int       s_kLampHDp          = 25;    // power-lamp height     (matches the strip LED)
static constexpr int       s_kLampWDp          = 8;     // power-lamp body width (matches the strip LED)
static constexpr int       s_kLampGlowRings    = 6;     // nested glow rectangles

// The composited framebuffer is 560x384; the recess is sized to that exact
// aspect so the display fills the glass with no bars inside the bezel.
static constexpr float     s_kScreenAspect     = 560.0f / 384.0f;

// Bezel thickness, even on all four sides, as a fraction of the screen height.
// Chunky like the period //c shell -- the screen ends up ~75% of the housing
// width rather than a modern thin-bezel look.
static constexpr float     s_kBezFrac          = 0.24f;

// How much of the available center the monitor is allowed to fill. Wide
// windows are bound by height (s_kFitV); narrow ones by width (s_kFitH).
static constexpr float     s_kFitV             = 0.95f;
static constexpr float     s_kFitH             = 0.90f;

// Curvature. The housing corners round but its SIDES stay straight (no barrel).
// The glass opening keeps a slight convex bow with modestly rounded corners.
static constexpr float     s_kHousingCorner    = 0.070f;
static constexpr float     s_kHousingBarrel    = 0.000f;   // straight monitor sides
static constexpr float     s_kGlassCorner      = 0.040f;   // tight inner-bezel corners
static constexpr float     s_kGlassBarrel      = 0.020f;

// Shading + composition tuning.
static constexpr float     s_kBevelLipFrac     = 0.9f;    // bevel wall thickness vs the lip
static constexpr float     s_kGradientSplit    = 0.45f;   // hilite->base fraction of the top-lit gradient
static constexpr float     s_kBevelBlend       = 0.8f;    // how far the bevel wall shades toward s_kBevelShade
static constexpr float     s_kShadowWidthFrac  = 0.42f;   // contact-shadow half-width vs housing width
static constexpr float     s_kShadowHeightFrac = 0.035f;  // contact-shadow half-height vs housing height

// Chin brand + power lamp placement.
static constexpr float     s_kMinBrandBandPx    = 6.0f;   // don't draw brand below this band height
static constexpr float     s_kLogoHFrac         = 0.62f;  // logo height vs the bottom-bezel band
static constexpr float     s_kLogoGridAspect    = 36.0f / 54.0f;   // cassowary silhouette grid W:H
static constexpr float     s_kLampBandFrac      = 0.7f;   // clamp the lamp to this fraction of the band
static constexpr float     s_kLampSlantTan      = 0.176f; // ~10 degrees, matches the strip
static constexpr float     s_kLampRimFrac       = 0.03f;  // lens rim thickness vs lamp height
static constexpr float     s_kLampGlowAlpha     = 60.0f;  // peak glow alpha (0..255) at the innermost ring
static constexpr float     s_kLampGlowExpand    = 0.32f;  // outermost glow-ring expansion vs lamp height
static constexpr float     s_kLampSpecularFrac  = 0.4f;   // specular size vs lamp box
static constexpr float     s_kLampSpecularInset = 1.5f;   // specular offset from top-left, as a multiple of the rim

// Degenerate-layout guards, in pixels.
static constexpr float     s_kMinLipPx          = 2.0f;




////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFrame
//
////////////////////////////////////////////////////////////////////////////////

MonitorFrame::MonitorFrame()
{
    m_focusable = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFrame::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    float  cw          = (float) (boundsDip.right  - boundsDip.left);
    float  ch          = (float) (boundsDip.bottom - boundsDip.top);
    float  screenH     = 0.0f;
    float  screenW     = 0.0f;
    float  bez         = 0.0f;
    float  housingW    = 0.0f;
    float  housingH    = 0.0f;
    float  maxHousingW = 0.0f;
    float  hx          = 0.0f;
    float  hy          = 0.0f;



    m_hidden     = false;
    m_dpi        = (scaler.Dpi() == 0) ? (UINT) s_kBaseDpi : scaler.Dpi();
    m_centerRect = boundsDip;

    // Size the glass to the display aspect, then wrap it in an even bezel to
    // get the housing. Fit the housing to the available height (the usual
    // wide-window bind); if that overflows the width (a tall / narrow window),
    // rescale to the width instead. The monitor is centered in the available
    // area with the desk behind it, sitting flat -- no stand.
    screenH  = (ch * s_kFitV) / (1.0f + 2.0f * s_kBezFrac);
    screenW  = screenH * s_kScreenAspect;
    bez      = screenH * s_kBezFrac;
    housingW = screenW + 2.0f * bez;

    maxHousingW = cw * s_kFitH;
    if (housingW > maxHousingW)
    {
        float  k = maxHousingW / housingW;

        screenW  *= k;
        screenH  *= k;
        bez      *= k;
        housingW *= k;
    }

    housingH = screenH + 2.0f * bez;
    hx       = (float) boundsDip.left + (cw - housingW) * 0.5f;
    hy       = (float) boundsDip.top  + (ch - housingH) * 0.5f;

    m_housingRect.left   = (int) hx;
    m_housingRect.top    = (int) hy;
    m_housingRect.right  = (int) (hx + housingW);
    m_housingRect.bottom = (int) (hy + housingH);

    m_screenRect.left   = (int) (hx + bez);
    m_screenRect.top    = (int) (hy + bez);
    m_screenRect.right  = (int) (m_screenRect.left + screenW);
    m_screenRect.bottom = (int) (m_screenRect.top  + screenH);

    m_sceneScale = screenH / (float) MulDiv (s_kScreenNativeHDp, (int) m_dpi, s_kBaseDpi);

    SetBounds (boundsDip);
}




////////////////////////////////////////////////////////////////////////////////
//
//  CenterSizeForScreenPx
//
//  Inverse of the Layout fit: given a target screen (recess) size, return the
//  center size that Layout would inset back down to it. The +2px on width keeps
//  the width from binding first (which would rescale the screen smaller).
//
////////////////////////////////////////////////////////////////////////////////

SIZE MonitorFrame::CenterSizeForScreenPx (int screenWpx, int screenHpx)
{
    float  sh       = (float) screenHpx;
    float  sw       = (float) screenWpx;
    float  bez      = sh * s_kBezFrac;
    float  housingW = sw + 2.0f * bez;
    float  housingH = sh + 2.0f * bez;
    float  ch       = housingH / s_kFitV;
    float  cw       = housingW / s_kFitH + 2.0f;



    return SIZE{ (int) std::ceil (cw), (int) std::ceil (ch) };
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFrame::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme)
{
    float  cl  = (float) m_centerRect.left;
    float  ct  = (float) m_centerRect.top;
    float  cr  = (float) m_centerRect.right;
    float  cb  = (float) m_centerRect.bottom;
    float  hl  = (float) m_housingRect.left;
    float  ht  = (float) m_housingRect.top;
    float  hr  = (float) m_housingRect.right;
    float  hb  = (float) m_housingRect.bottom;
    float  sl  = (float) m_screenRect.left;
    float  st  = (float) m_screenRect.top;
    float  sr     = (float) m_screenRect.right;
    float  sb     = (float) m_screenRect.bottom;
    float  lip    = std::max (s_kMinLipPx, (float) MulDiv (s_kGlassLipDp, (int) m_dpi, s_kBaseDpi));
    float  bevelW = std::max (s_kMinLipPx, lip * s_kBevelLipFrac);

    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    if (m_hidden)
    {
        return;
    }

    // Desk / wall backdrop, painted as four bands AROUND the screen recess --
    // never over it. Chrome paints on top of the composited emulator frame, so
    // filling the recess would hide the display that must show through the
    // hole. The housing then paints over whichever bands it covers.
    {
        float     chH   = std::max (1.0f, cb - ct);
        uint32_t  deskT = LerpArgb (s_kDeskTop, s_kDeskBot, (st - ct) / chH);
        uint32_t  deskB = LerpArgb (s_kDeskTop, s_kDeskBot, (sb - ct) / chH);

        painter.FillGradientRect (cl, ct, cr - cl, st - ct, s_kDeskTop, deskT);   // above recess
        painter.FillGradientRect (cl, sb, cr - cl, cb - sb, deskB, s_kDeskBot);   // below recess
        painter.FillGradientRect (cl, st, sl - cl, sb - st, deskT, deskB);        // left of recess
        painter.FillGradientRect (sr, st, cr - sr, sb - st, deskT, deskB);        // right of recess
    }

    // Soft contact shadow so the monitor sits on the desk rather than floating.
    painter.FillEllipseApprox ((hl + hr) * 0.5f, hb,
                               (hr - hl) * s_kShadowWidthFrac, (hb - ht) * s_kShadowHeightFrac,
                               s_kShadowArgb);

    // The dark glass opening: a rounded, gently barrelled rectangle just outside
    // the display recess. The platinum wraps its convex outline and the display
    // composites into the recess inside it. A slightly larger "bevel" outline
    // nests between the flat platinum and the glass, shaded darker so the screen
    // reads as recessed below the shell surface.
    {
        float  boL     = sl - lip;
        float  boT     = st - lip;
        float  boR     = sr + lip;
        float  boB     = sb + lip;
        float  voL     = boL - bevelW;
        float  voT     = boT - bevelW;
        float  voR     = boR + bevelW;
        float  voB     = boB + bevelW;
        float  hRad    = std::min (hr - hl, hb - ht) * s_kHousingCorner;
        float  hBarrel = (hr - hl) * s_kHousingBarrel;
        float  bRad    = std::min (boR - boL, boB - boT) * s_kGlassCorner;
        float  bBarrel = (boR - boL) * s_kGlassBarrel;
        float  vRad    = bRad + bevelW;
        float  vBarrel = bBarrel;
        int    y0      = (int) ht;
        int    y1      = (int) hb;

        // Scanline-fill the platinum shell with its curved silhouette, leaving
        // the glass opening (and, within it, the transparent display recess) as
        // holes. Per-scanline vertical gradient: top-lit platinum easing to an
        // edge shade.
        for (int yy = y0; yy < y1; ++yy)
        {
            float     y   = (float) yy + 0.5f;
            float     yf  = (float) yy;
            float     hIn = EdgeInset (y, ht, hb, hRad, hBarrel);
            float     hxl = hl + hIn;
            float     hxr = hr - hIn;
            float     t   = (y - ht) / std::max (1.0f, hb - ht);
            uint32_t  col = (t < s_kGradientSplit)
                                ? LerpArgb (s_kShellHilite, s_kShellBase,  t / s_kGradientSplit)
                                : LerpArgb (s_kShellBase,   s_kShellShadow,
                                            (t - s_kGradientSplit) / (1.0f - s_kGradientSplit));

            if (y < voT || y >= voB)
            {
                FillSpan (painter, hxl, hxr, yf, col);
                continue;
            }

            float     vIn      = EdgeInset (y, voT, voB, vRad, vBarrel);
            float     vxl      = voL + vIn;
            float     vxr      = voR - vIn;
            uint32_t  bevelCol = LerpArgb (col, s_kBevelShade, s_kBevelBlend);

            // Flat platinum outside the bevel outline.
            FillSpan (painter, hxl, vxl, yf, col);
            FillSpan (painter, vxr, hxr, yf, col);

            if (y < boT || y >= boB)
            {
                // Bevel band above / below the glass (the recess top/bottom walls).
                FillSpan (painter, vxl, vxr, yf, bevelCol);
                continue;
            }

            float  bIn  = EdgeInset (y, boT, boB, bRad, bBarrel);
            float  bxl  = boL + bIn;
            float  bxr  = boR - bIn;

            // Bevel wall (shaded platinum) between the flat shell and glass.
            FillSpan (painter, vxl, bxl, yf, bevelCol);
            FillSpan (painter, bxr, vxr, yf, bevelCol);

            // Dark bezel between the glass outline and the display recess. Over
            // the display columns it is drawn only in the top/bottom lip bands,
            // clamped to the rounded glass silhouette so it never pokes past the
            // corners; the recess itself stays transparent.
            FillSpan (painter, bxl, sl,  yf, s_kGlassFrame);
            FillSpan (painter, sr,  bxr, yf, s_kGlassFrame);

            if (y < st || y >= sb)
            {
                FillSpan (painter, std::max (sl, bxl), std::min (sr, bxr), yf, s_kGlassFrame);
            }
        }
    }

    // Brand + power lamp on the bottom bezel strip (the platinum below the
    // glass, now the same thickness as the other three sides). The rainbow
    // cassowary -- Casso's period-Apple analog of the rainbow logo -- sits
    // lower-left; the power lamp sits lower-right, balancing it.
    {
        float  bandTop = sb + lip + bevelW;
        float  bandH   = hb - bandTop;

        if (bandH <= s_kMinBrandBandPx)
        {
            return;
        }

        // Rainbow cassowary, lower-left: aligned under the screen's left edge,
        // matching the Apple logo's placement on the real bezel.
        float  logoH = bandH * s_kLogoHFrac;
        float  logoW = logoH * s_kLogoGridAspect;
        float  logoX = sl;
        float  logoY = bandTop + (bandH - logoH) * 0.5f;

        CassoBranding::DrawCassowaryRainbow (painter, logoX, logoY, logoW, logoH, s_kLogoBorderArgb);

        // Power lamp, lower-right: the slanted green lamp from the //c case-
        // switch strip -- the "/" power mark under the screen's right edge. The
        // strip's LED dimensions at SceneScale, so the lamp zooms with the
        // monitor it is mounted on. Clamped to the band on tiny layouts,
        // preserving the LED proportion.
        float  lampH    = (float) MulDiv (s_kLampHDp, (int) m_dpi, s_kBaseDpi) * m_sceneScale;
        float  lampBody = 0.0f;
        float  lampBox  = 0.0f;
        float  lampX    = 0.0f;
        float  lampY    = 0.0f;

        lampH    = std::min (lampH, bandH * s_kLampBandFrac);
        lampBody = lampH * ((float) s_kLampWDp / (float) s_kLampHDp);
        lampBox  = lampBody + lampH * s_kLampSlantTan;
        lampX    = sr - lampBox;
        lampY    = bandTop + (bandH - lampH) * 0.5f;

        PaintPowerLamp (painter, lampX, lampY, lampBox, lampH);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFrame::LerpArgb
//
//  Linear blend of two 0xAARRGGBB colors.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t MonitorFrame::LerpArgb (uint32_t a, uint32_t b, float t)
{
    auto  lane = [&] (int shift) -> uint32_t
    {
        float  av = (float) ((a >> shift) & 0xFFu);
        float  bv = (float) ((b >> shift) & 0xFFu);

        return (uint32_t) (av + (bv - av) * t + 0.5f);
    };



    t = std::clamp (t, 0.0f, 1.0f);

    return (lane (24) << 24) | (lane (16) << 16) | (lane (8) << 8) | lane (0);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFrame::EdgeInset
//
//  Inset of a rounded-rect-with-slight-barrel edge from the straight side at
//  scanline y. Positive pulls in (rounded corner); the barrel term pushes out
//  (bulge), strongest at the vertical midpoint, so the faces read convex.
//
////////////////////////////////////////////////////////////////////////////////

float MonitorFrame::EdgeInset (float y, float top, float bottom, float radius, float barrel)
{
    float  roundIn = 0.0f;
    float  dTop    = y - top;
    float  dBot    = bottom - y;
    float  h       = bottom - top;
    float  ny      = 0.0f;



    if (dTop < radius)
    {
        float  d = radius - dTop;

        roundIn = radius - std::sqrt (std::max (0.0f, radius * radius - d * d));
    }
    else if (dBot < radius)
    {
        float  d = radius - dBot;

        roundIn = radius - std::sqrt (std::max (0.0f, radius * radius - d * d));
    }

    ny = (h > 0.0f) ? (((y - top) / h) * 2.0f - 1.0f) : 0.0f;   // -1..1

    return roundIn - barrel * (1.0f - ny * ny);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFrame::FillSpan
//
//  Fills a 1px-tall horizontal span [xLeft, xRight) at y -- but only when the
//  span is non-empty, folding the many "draw this segment if it has width"
//  guards in the scanline loop into one place.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFrame::FillSpan (IDxuiPainter & painter, float xLeft, float xRight, float y, uint32_t argb)
{
    if (xRight > xLeft)
    {
        painter.FillRect (xLeft, y, xRight - xLeft, 1.0f, argb);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFrame::ShearFillQuad
//
//  A slanted (sheared) rectangle: the quad leans by `tan` about `refBottom`,
//  the top edge shifted right of the bottom -- the lamp/switch idiom.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFrame::ShearFillQuad (
    IDxuiPainter & painter,
    float          xLeft,
    float          yTop,
    float          w,
    float          h,
    float          tan,
    float          refBottom,
    uint32_t       argb)
{
    float  st = (refBottom - yTop)       * tan;   // top-edge shift
    float  sb = (refBottom - (yTop + h)) * tan;   // bottom-edge shift



    painter.FillConvexQuad (xLeft + st,     yTop,
                            xLeft + w + st, yTop,
                            xLeft + w + sb, yTop + h,
                            xLeft + sb,     yTop + h,
                            argb);
}




////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFrame::PaintPowerLamp
//
//  The //c case-switch strip's lit indicator: a slanted green lens with a
//  rectangular glow and a specular catch, also the "/" power mark on the real
//  //c monitor's bezel. Kept separate from the strip's own lamp (the two
//  diverge -- a green lens rim here for the light platinum vs a near-black rim
//  on the strip's dark band). Always lit; the monitor is powered whenever it is
//  on screen. `bboxW` includes the slant overhang; the lamp body is
//  `bboxW - h*tan` wide.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFrame::PaintPowerLamp (IDxuiPainter & painter, float x, float y, float bboxW, float h)
{
    float  refB  = y + h;
    float  dx    = h * s_kLampSlantTan;
    float  bodyW = bboxW - dx;
    float  rim   = std::max (1.0f, h * s_kLampRimFrac);



    // Green glow halo: nested slanted rects on a quadratic alpha falloff, so the
    // lamp reads as a little light spilling onto the platinum -- kept subtle (a
    // tight, faint bloom, not a big aura).
    for (int ring = s_kLampGlowRings; ring >= 1; --ring)
    {
        float     t = (float) ring / (float) (s_kLampGlowRings + 1);
        uint32_t  a = (uint32_t) (s_kLampGlowAlpha * (1.0f - t) * (1.0f - t) + 0.5f);
        float     e = (h * s_kLampGlowExpand) * t;

        ShearFillQuad (painter, x - e, y - e, bodyW + 2.0f * e, h + 2.0f * e,
                       s_kLampSlantTan, refB, (a << 24) | (s_kLampGlow & 0x00FFFFFF));
    }

    // A thin darker-green lens rim (a lit lens, not a black-framed lamp) and the
    // bright green body, with a specular catch near the top.
    ShearFillQuad (painter, x,       y,       bodyW,              h,              s_kLampSlantTan, refB, s_kLampEdge);
    ShearFillQuad (painter, x + rim, y + rim, bodyW - 2.0f * rim, h - 2.0f * rim, s_kLampSlantTan, refB, s_kLampGreen);
    ShearFillQuad (painter, x + rim * s_kLampSpecularInset, y + rim * s_kLampSpecularInset,
                   bodyW * s_kLampSpecularFrac, h * s_kLampSpecularFrac, s_kLampSlantTan, refB, s_kLampSpecular);
}
