#include "Pch.h"

#include "MonitorFrame.h"
#include "CassoBranding.h"
#include "Render/IDxuiPainter.h"




namespace
{
    // Apple Monitor //c palette -- warm snow-white/platinum plastic lit from
    // above. Hardcoded for now; a later pass keys the housing colour off the
    // machine + colour mode (mono vs the AppleColor Composite shell).
    constexpr uint32_t s_kShellHilite = 0xFFECE8DE;   // top-lit platinum
    constexpr uint32_t s_kShellBase   = 0xFFDAD5C8;   // main platinum
    constexpr uint32_t s_kShellShadow = 0xFFB6B0A1;   // lower / edge shade
    constexpr uint32_t s_kGlassFrame  = 0xFF2A2926;   // dark bezel lip around the CRT
    constexpr uint32_t s_kBevelShade  = 0xFF938E80;   // shaded platinum: recessed bevel wall

    // Desk / wall backdrop filling the area around the (bounded) monitor. The
    // window is usually far wider than a 4:3 monitor, so the surround must not
    // be platinum or the shell would read as an edge-to-edge wall.
    constexpr uint32_t s_kDeskTop     = 0xFF1B1A18;   // dark warm grey, top
    constexpr uint32_t s_kDeskBot     = 0xFF27241F;   // slightly lifted at desk

    constexpr int      s_kBaseDpi     = 96;

    // The composited framebuffer is 560x384; the recess is sized to that exact
    // aspect so the display fills the glass with no bars inside the bezel. The
    // native height also anchors SceneScale: recess == Px(384) tall <=> the
    // monitor is at its natural 100%-zoom size.
    constexpr int      s_kScreenNativeHDp = 384;
    constexpr float    s_kScreenAspect    = 560.0f / 384.0f;

    // Bezel thickness, even on all four sides. Expressed as a fraction of the
    // screen height and applied in pixels so top/bottom/left/right match
    // exactly (the brand + power LED sit on the bottom bezel strip). Chunky,
    // like the period //c shell -- the screen ends up ~75% of the housing
    // width rather than a modern thin-bezel look.
    constexpr float    s_kBezFrac     = 0.24f;

    // How much of the available center the monitor is allowed to fill. Wide
    // windows are bound by height (s_kFitV); narrow ones by width (s_kFitH).
    constexpr float    s_kFitV        = 0.95f;
    constexpr float    s_kFitH        = 0.90f;

    // Curvature. The housing corners round but its SIDES stay straight (no
    // barrel). The glass opening keeps a slight convex bow, wrapping the CRT
    // face, with modestly rounded corners. Radii are a fraction of the shorter
    // span, barrels a fraction of the wider span.
    constexpr float    s_kHousingCorner = 0.070f;
    constexpr float    s_kHousingBarrel = 0.000f;   // straight monitor sides
    constexpr float    s_kGlassCorner   = 0.040f;   // tight inner-bezel corners
    constexpr float    s_kGlassBarrel   = 0.020f;


    // ---- helpers --------------------------------------------------------

    // Linear blend of two 0xAARRGGBB colours.
    uint32_t LerpArgb (uint32_t a, uint32_t b, float t)
    {
        t = std::clamp (t, 0.0f, 1.0f);

        auto lane = [&] (int shift) -> uint32_t
        {
            float av = (float) ((a >> shift) & 0xFFu);
            float bv = (float) ((b >> shift) & 0xFFu);
            return (uint32_t) (av + (bv - av) * t + 0.5f);
        };

        return (lane (24) << 24) | (lane (16) << 16) | (lane (8) << 8) | lane (0);
    }

    // Inset of a rounded-rect-with-slight-barrel edge from the straight side at
    // scanline y. Positive pulls in (rounded corner); the barrel term pushes
    // out (bulge), strongest at the vertical midpoint, so the faces read convex.
    float EdgeInset (float y, float top, float bottom, float radius, float barrel)
    {
        float roundIn = 0.0f;
        float dTop     = y - top;
        float dBot     = bottom - y;

        if (dTop < radius)
        {
            float t = radius - dTop;
            roundIn = radius - std::sqrt (std::max (0.0f, radius * radius - t * t));
        }
        else if (dBot < radius)
        {
            float t = radius - dBot;
            roundIn = radius - std::sqrt (std::max (0.0f, radius * radius - t * t));
        }

        float h  = bottom - top;
        float ny = (h > 0.0f) ? (((y - top) / h) * 2.0f - 1.0f) : 0.0f;   // -1..1

        return roundIn - barrel * (1.0f - ny * ny);
    }


    // A slanted (sheared) rectangle: the quad leans by `tan` about `refBottom`,
    // top edge shifted right of the bottom -- the lamp/switch idiom.
    void ShearFillQuad (IDxuiPainter & p, float xL, float yTop, float w, float h,
                        float tan, float refBottom, uint32_t argb)
    {
        float st = (refBottom - yTop)         * tan;   // top-edge shift
        float sb = (refBottom - (yTop + h))   * tan;   // bottom-edge shift

        p.FillConvexQuad (xL + st,     yTop,
                          xL + w + st, yTop,
                          xL + w + sb, yTop + h,
                          xL + sb,     yTop + h,
                          argb);
    }


    // Power lamp: the //c case-switch strip's lit indicator (Apple2cSwitchBar::
    // PaintLed) -- a slanted green rectangular lamp with a rectangular glow and
    // a specular catch, which is also the "/" power mark on the real //c
    // monitor's bezel. Kept separate from that strip's lamp (now on master):
    // the two diverge -- this one has a green lens rim for the light platinum
    // bezel, the strip uses a near-black rim on its dark band -- so a shared,
    // parameterized helper is a possible follow-up, not a drop-in. Always lit:
    // the monitor is powered whenever it is on screen. `bboxW` includes the
    // slant overhang; the lamp body is `bboxW - h*tan` wide.
    void PaintPowerLamp (IDxuiPainter & p, float x, float y, float bboxW, float h)
    {
        constexpr float    kTan   = 0.176f;         // ~10 degrees, matches the strip
        constexpr uint32_t kGreen = 0xFF3CE070;     // lit green lens
        constexpr uint32_t kGlow  = 0xFF2FBF5F;     // green bloom
        constexpr uint32_t kEdge  = 0xFF1C6E3A;     // darker-green lens rim (NOT black)
        constexpr int      kRings = 6;

        float  refB  = y + h;
        float  dx    = h * kTan;
        float  bodyW = bboxW - dx;
        float  rim   = std::max (1.0f, h * 0.03f);

        // Green glow halo: nested slanted rects on a quadratic alpha falloff, so
        // the lamp reads as a little light spilling onto the platinum -- kept
        // subtle (a tight, faint bloom, not a big aura).
        for (int ring = kRings; ring >= 1; --ring)
        {
            float     t = (float) ring / (float) (kRings + 1);
            uint32_t  a = (uint32_t) (60.0f * (1.0f - t) * (1.0f - t) + 0.5f);
            float     e = (h * 0.32f) * t;
            ShearFillQuad (p, x - e, y - e, bodyW + 2.0f * e, h + 2.0f * e,
                           kTan, refB, (a << 24) | (kGlow & 0x00FFFFFF));
        }

        // A thin darker-green lens rim (a lit lens, not a black-framed lamp) and
        // the bright green body, with a specular catch near the top.
        ShearFillQuad (p, x,              y,              bodyW,               h,              kTan, refB, kEdge);
        ShearFillQuad (p, x + rim,        y + rim,        bodyW - 2.0f * rim,  h - 2.0f * rim, kTan, refB, kGreen);
        ShearFillQuad (p, x + rim * 1.5f, y + rim * 1.5f, bodyW * 0.4f,        h * 0.4f,       kTan, refB, 0x99EFFFF0);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFrame
//
////////////////////////////////////////////////////////////////////////////////

MonitorFrame::MonitorFrame ()
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
    float  cw = (float) (boundsDip.right  - boundsDip.left);
    float  ch = (float) (boundsDip.bottom - boundsDip.top);



    m_hidden     = false;
    m_dpi        = (scaler.Dpi() == 0) ? (UINT) s_kBaseDpi : scaler.Dpi();
    m_centerRect = boundsDip;

    // Size the glass to the display aspect, then wrap it in an even bezel to
    // get the housing. Fit the housing to the available height (the usual
    // wide-window bind); if that overflows the width (a tall / narrow window),
    // rescale to the width instead. The monitor is centered in the available
    // area with the desk behind it, sitting flat -- no stand.
    float  screenH  = (ch * s_kFitV) / (1.0f + 2.0f * s_kBezFrac);
    float  screenW  = screenH * s_kScreenAspect;
    float  bez      = screenH * s_kBezFrac;
    float  housingW = screenW + 2.0f * bez;

    float  maxHousingW = cw * s_kFitH;
    if (housingW > maxHousingW)
    {
        float  k = maxHousingW / housingW;
        screenW  *= k;
        screenH  *= k;
        bez      *= k;
        housingW *= k;
    }
    float  housingH = screenH + 2.0f * bez;

    float  hx = (float) boundsDip.left + (cw - housingW) * 0.5f;
    float  hy = (float) boundsDip.top  + (ch - housingH) * 0.5f;

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

    float  ch = housingH / s_kFitV;
    float  cw = housingW / s_kFitH + 2.0f;

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
    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    if (m_hidden)
    {
        return;
    }

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
    float  sr  = (float) m_screenRect.right;
    float  sb  = (float) m_screenRect.bottom;
    float  lip = std::max (2.0f, (float) MulDiv (6, (int) m_dpi, s_kBaseDpi));



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
    painter.FillEllipseApprox ((hl + hr) * 0.5f, hb, (hr - hl) * 0.42f, (hb - ht) * 0.035f, 0x66000000);

    // The dark glass opening: a rounded, gently barrelled rectangle just
    // outside the display recess. The platinum wraps its convex outline and
    // the display composites into the recess inside it. A slightly larger
    // "bevel" outline nests between the flat platinum and the glass, shaded
    // darker so the screen reads as recessed below the shell surface.
    float  bevelW  = std::max (2.0f, lip * 0.9f);
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

    // Scanline-fill the platinum shell with its curved silhouette, leaving the
    // glass opening (and, within it, the transparent display recess) as holes.
    // Per-scanline vertical gradient: top-lit platinum easing to an edge shade.
    int  y0 = (int) ht;
    int  y1 = (int) hb;

    for (int yy = y0; yy < y1; ++yy)
    {
        float     y     = (float) yy + 0.5f;
        float     hIn   = EdgeInset (y, ht, hb, hRad, hBarrel);
        float     hxl   = hl + hIn;
        float     hxr   = hr - hIn;
        float     t     = (y - ht) / std::max (1.0f, hb - ht);
        uint32_t  col   = (t < 0.45f)
                              ? LerpArgb (s_kShellHilite, s_kShellBase,  t / 0.45f)
                              : LerpArgb (s_kShellBase,   s_kShellShadow, (t - 0.45f) / 0.55f);

        if (y >= voT && y < voB)
        {
            float  vIn = EdgeInset (y, voT, voB, vRad, vBarrel);
            float  vxl = voL + vIn;
            float  vxr = voR - vIn;
            uint32_t  bevelCol = LerpArgb (col, s_kBevelShade, 0.8f);

            // Flat platinum outside the bevel outline.
            if (vxl > hxl) painter.FillRect (hxl, (float) yy, vxl - hxl, 1.0f, col);
            if (hxr > vxr) painter.FillRect (vxr, (float) yy, hxr - vxr, 1.0f, col);

            if (y >= boT && y < boB)
            {
                float  bIn  = EdgeInset (y, boT, boB, bRad, bBarrel);
                float  bxl  = boL + bIn;
                float  bxr  = boR - bIn;
                float  segL = std::max (sl, bxl);
                float  segR = std::min (sr, bxr);

                // Bevel wall (shaded platinum) between the flat shell and glass.
                if (bxl > vxl) painter.FillRect (vxl, (float) yy, bxl - vxl, 1.0f, bevelCol);
                if (vxr > bxr) painter.FillRect (bxr, (float) yy, vxr - bxr, 1.0f, bevelCol);

                // Dark bezel between the glass outline and the display recess.
                // Over the display columns it is drawn only in the top/bottom
                // lip bands, clamped to the rounded glass silhouette so it never
                // pokes past the corners; the recess stays transparent.
                if (sl > bxl) painter.FillRect (bxl, (float) yy, sl - bxl, 1.0f, s_kGlassFrame);
                if (bxr > sr) painter.FillRect (sr,  (float) yy, bxr - sr, 1.0f, s_kGlassFrame);
                if ((y < st || y >= sb) && segR > segL) painter.FillRect (segL, (float) yy, segR - segL, 1.0f, s_kGlassFrame);
            }
            else
            {
                // Bevel band above / below the glass (the recess's top and
                // bottom walls).
                if (vxr > vxl) painter.FillRect (vxl, (float) yy, vxr - vxl, 1.0f, bevelCol);
            }
        }
        else
        {
            painter.FillRect (hxl, (float) yy, hxr - hxl, 1.0f, col);
        }
    }

    // Brand + power LED on the bottom bezel strip (the platinum below the
    // glass, now the same thickness as the other three sides). The rainbow
    // cassowary -- Casso's period-Apple analog of the rainbow logo -- sits
    // lower-left; the power LED sits lower-right, balancing it.
    {
        float  bandTop = sb + lip + bevelW;
        float  bandH   = hb - bandTop;

        if (bandH > 6.0f)
        {
            // Rainbow cassowary, lower-left: aligned under the screen's left
            // edge, matching the Apple logo's placement on the real bezel.
            float  logoH = bandH * 0.62f;
            float  logoW = logoH * (36.0f / 54.0f);
            float  logoX = sl;
            float  logoY = bandTop + (bandH - logoH) * 0.5f;

            DrawCassowaryRainbow (painter, logoX, logoY, logoW, logoH, 0xFF4A4A4A);

            // Power lamp, lower-right: the slanted green lamp from the //c
            // case-switch strip -- the "/" power mark under the screen's right
            // edge. The strip's LED dimensions (8 x 25 dp) at SceneScale, so
            // the lamp zooms with the monitor it is mounted on. Clamped to the
            // band on tiny layouts, preserving the 8:25 proportion.
            float  lampH = (float) MulDiv (25, (int) m_dpi, s_kBaseDpi) * m_sceneScale;

            lampH = std::min (lampH, bandH * 0.7f);

            float  lampBody = lampH * (8.0f / 25.0f);
            float  lampBox  = lampBody + lampH * 0.176f;
            float  lampX    = sr - lampBox;
            float  lampY    = bandTop + (bandH - lampH) * 0.5f;

            PaintPowerLamp (painter, lampX, lampY, lampBox, lampH);
        }
    }
}