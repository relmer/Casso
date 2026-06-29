#include "Pch.h"
#include "Theme/DxuiTheme.h"
#include "DriveWidget.h"
#include "../IDriveCommandSink.h"
#include "../../UnicodeSymbols.h"





namespace
{
    constexpr int     s_kBaseDpi           = 96;
    constexpr int     s_kBodyWidthPx       = 220;
    constexpr int     s_kBodyHeightPx      = 160;
    constexpr int     s_kFaceplateHeightPx = 104;
    constexpr int     s_kCaseBackInsetPx   = 30;
    constexpr int     s_kLabelPadPx        = 10;
    constexpr float   s_kLabelFontDip      = 13.0f;
    constexpr float   s_kInUseFontDip      = 10.0f;
    constexpr int     s_kSlotInsetPx       = 22;
    constexpr int     s_kSlotHeightPx      = 6;
    constexpr int     s_kSlotCenterYPx     = 50;
    constexpr int     s_kDoorWidthPx       = 72;
    constexpr int     s_kDoorHeightPx      = 44;
    constexpr int     s_kDoorTravelPx      = 32;
    constexpr int     s_kNotchWidthPx      = 28;
    constexpr int     s_kNotchHeightPx     = 8;
    constexpr int     s_kLedCenterYPx      = 84;
    constexpr int     s_kInUseGapPx        = 4;
    constexpr int     s_kInUseWidthPx      = 56;
    constexpr int     s_kRidgeCountPx      = 2;
    constexpr int     s_kVentCountPx       = 9;        // matches real Disk II side-vent count
    constexpr int     s_kVentSlotHeightPx  = 1;        // each vent is 1 px tall (scaled by DPI)
    constexpr int     s_kVentSlotGapPx     = 2;        // vertical gap between vents
    constexpr int     s_kCassowaryWidthPx  = 28;
    constexpr int     s_kCassowaryHeightPx = 42;
    constexpr int     s_kCassowaryMarginPx = 6;
    constexpr int     s_kLabelStripHeightPx = 18;
    constexpr int     s_kLabelStripGapPx    = 2;
    constexpr float   s_kBasenameFontDip    = 11.0f;
    constexpr const wchar_t * s_kFontFamily      = DxuiTheme::kBodyFace;

    // Marquee timing for an overflowing basename label. The hold delay is
    // both the lead-in before a freshly mounted disk first scrolls and the
    // pause between replays while the pointer lingers over the widget.
    constexpr int64_t s_kMarqueeHoldMs         = 2000;
    constexpr float   s_kMarqueeSpeedDipPerSec = 45.0f;
    constexpr float   s_kMarqueeGapDip         = 25.0f;

    // Compact paint-path dimensions. The compact widget is a flat
    // rounded card with "DRIVE N" on the left and the status LED on
    // the right -- no 3D case top, no door, no cassowary. Total
    // height is sized so the drive bar can shrink the bottom inset
    // dramatically when the active theme requests compact drives.
    constexpr int     s_kCompactBodyWidthPx  = 140;
    constexpr int     s_kCompactBodyHeightPx = 40;
    constexpr int     s_kCompactPadPx        = 10;
    constexpr int     s_kCompactCornerPx     = 4;
    constexpr float   s_kCompactFontDip      = 12.0f;


    bool RectContains (const RECT & rect, int x, int y)
    {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }


    int Scale (int value, UINT dpi)
    {
        UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;



        return MulDiv (value, (int) effectiveDpi, s_kBaseDpi);
    }


    float Clamp01 (float v)
    {
        if (v < 0.0f) { return 0.0f; }
        if (v > 1.0f) { return 1.0f; }
        return v;
    }


    // Fills a trapezoid with parallel horizontal front and back edges
    // by stacking 1-px horizontal scanlines whose widths interpolate
    // linearly from front to back. Used for the receding case top.
    void FillTrapezoidApprox (IDxuiPainter & painter,
                              float frontLeft,  float frontRight,
                              float backLeft,   float backRight,
                              float frontY,     float backY,
                              uint32_t argb)
    {
        int    height = (int) (frontY - backY);
        int    i      = 0;
        float  denom  = (float) ((height > 1) ? (height - 1) : 1);

        if (height <= 0)
        {
            return;
        }

        for (i = 0; i < height; i++)
        {
            float  t     = (float) i / denom;
            float  left  = frontLeft  + (backLeft  - frontLeft)  * t;
            float  right = frontRight + (backRight - frontRight) * t;
            float  y     = frontY - 1.0f - (float) i;

            painter.FillRect (left, y, right - left, 1.0f, argb);
        }
    }


    // Draws a horizontal ridge line on the case top at fractional depth
    // (0=front, 1=back), respecting the trapezoid's perspective taper.
    void DrawCaseRidge (DxuiPainter & painter,
                        float frontLeft, float frontRight,
                        float backLeft,  float backRight,
                        float frontY,    float backY,
                        float depthT,
                        uint32_t argb)
    {
        float  y     = frontY + (backY - frontY) * depthT;
        float  left  = frontLeft  + (backLeft  - frontLeft)  * depthT;
        float  right = frontRight + (backRight - frontRight) * depthT;

        painter.FillRect (left + 2.0f, y, right - left - 4.0f, 1.0f, argb);
    }


    // Cassowary head + neck silhouette, baked from
    // Resources/Branding/Cassowary.png at 36x54 resolution. Stored as
    // a per-row 36-bit bitmask (bit 0 = leftmost column). Bitmask
    // format preserves per-row concavities -- in particular the gap
    // between the underside of the beak and the top of the neck --
    // that a single (start, end) span per row would erroneously fill.
    void DrawCassowaryRainbow (IDxuiPainter & painter,
                               float left, float top,
                               float width, float height)
    {
        constexpr int       kGridW             = 36;
        constexpr int       kGridH             = 54;
        constexpr int       kStripeCount       = 6;
        static const uint64_t s_kSilhouette[kGridH] = {
            0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL,
            0x000000FE00ULL, 0x000001FF80ULL, 0x000003FFC0ULL, 0x000007FFE0ULL, 0x00000FFFC0ULL,
            0x00000FFFC0ULL, 0x00001FFF80ULL, 0x00001FFF00ULL, 0x00003FFF00ULL, 0x00003FFF00ULL,
            0x00007FFE00ULL, 0x00007FFE00ULL, 0x0000FFFE00ULL, 0x0000FFFC00ULL, 0x0000FFFC00ULL,
            0x0000FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL,
            0x0003FFFE00ULL, 0x0003FFFE00ULL, 0x0003FFFF00ULL, 0x0003FFFF80ULL, 0x0003FFFFC0ULL,
            0x0003FFFFC0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x000FFFFFF0ULL,
            0x001FFFFFF0ULL, 0x003FFFFFF0ULL, 0x007FFFFFF0ULL, 0x00FFFFFFF0ULL, 0x01FFFFFFF8ULL,
            0x01FFFFFFF8ULL, 0x03F007FFF8ULL, 0x038000FFF8ULL, 0x0200007FF8ULL, 0x0000007FF8ULL,
            0x0000007FF8ULL, 0x0000007FF8ULL, 0x000000FFF8ULL, 0x000000FFF8ULL, 0x000001FFF8ULL,
            0x000001FFF8ULL, 0x000001FFF8ULL, 0x000001FFF0ULL, 0x000003FFF0ULL
        };
        static const uint32_t s_kStripeColors[kStripeCount] = {
            0xFF61BB46,  // green
            0xFFFDB827,  // yellow
            0xFFF5821F,  // orange
            0xFFE03A3E,  // red
            0xFF963D97,  // purple
            0xFF009DDC,  // blue
        };

        // Find the first / last non-empty row so the rainbow bands
        // span the actual silhouette extents rather than the full
        // grid (which would waste stripes on the empty rows above).
        int  firstRow = kGridH;
        int  lastRow  = -1;
        int  i        = 0;

        for (i = 0; i < kGridH; i++)
        {
            if (s_kSilhouette[i] != 0)
            {
                if (i < firstRow) { firstRow = i; }
                if (i > lastRow)  { lastRow  = i; }
            }
        }
        if (lastRow < firstRow)
        {
            return;
        }

        int    silhouetteH = lastRow - firstRow + 1;
        float  rowH        = height / (float) kGridH;
        float  colW        = width  / (float) kGridW;

        for (int row = firstRow; row <= lastRow; row++)
        {
            uint64_t  bits   = s_kSilhouette[row];
            int       stripe = ((row - firstRow) * kStripeCount) / silhouetteH;
            uint32_t  argb   = s_kStripeColors[stripe];
            int       col    = 0;

            // Walk this row and emit one FillRect per contiguous bit
            // run, so per-row concavities render as actual daylight
            // rather than being span-filled.
            while (col < kGridW)
            {
                if ((bits & (1ULL << col)) == 0)
                {
                    col++;
                    continue;
                }

                int  runStart = col;

                while (col < kGridW && (bits & (1ULL << col)) != 0)
                {
                    col++;
                }

                float  x = left + (float) runStart * colW;
                float  y = top  + (float) row      * rowH;
                float  w = (float) (col - runStart) * colW;

                painter.FillRect (x, y, w, rowH + 0.5f, argb);
            }
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::Initialize (int slot, int drive, IDriveCommandSink * pSink)
{
    m_slot  = slot;
    m_drive = drive;
    m_sink  = pSink;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidget
//
////////////////////////////////////////////////////////////////////////////////

DriveWidget::DriveWidget ()
{
    m_focusable = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  IDxuiControl override. Uses boundsDip.left / boundsDip.top as the
//  anchor; ignores boundsDip.right / bottom because the widget has an
//  intrinsic size derived from the scale constants. Calls SetBounds
//  with the computed OuterRect so panel hit-testing sees the actual
//  occupied region.
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   x           = boundsDip.left;
    int   y           = boundsDip.top;
    UINT  dpi         = scaler.Dpi();
    int   bodyW       = Scale (s_kBodyWidthPx, dpi);
    int   bodyH       = Scale (s_kBodyHeightPx, dpi);
    int   faceH       = Scale (s_kFaceplateHeightPx, dpi);
    int   slotInset   = Scale (s_kSlotInsetPx, dpi);
    int   slotH       = Scale (s_kSlotHeightPx, dpi);
    int   slotCY      = Scale (s_kSlotCenterYPx, dpi);
    int   doorW       = Scale (s_kDoorWidthPx, dpi);
    int   doorH       = Scale (s_kDoorHeightPx, dpi);



    m_dpi = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;

    if (m_compact)
    {
        // Compact card: faceRect == bodyRect (no receding case top);
        // the LED sits on the right edge with the label filling the
        // remainder. Slot/eject rects collapse to zero since the
        // compact widget has no door affordance.
        int  cBodyW = Scale (s_kCompactBodyWidthPx,  dpi);
        int  cBodyH = Scale (s_kCompactBodyHeightPx, dpi);
        int  pad    = Scale (s_kCompactPadPx,        dpi);

        m_bodyRect.left   = x;
        m_bodyRect.top    = y;
        m_bodyRect.right  = x + cBodyW;
        m_bodyRect.bottom = y + cBodyH;

        m_faceRect  = m_bodyRect;
        m_slotRect  = {};
        m_ejectRect = {};

        m_labelRect.left   = m_bodyRect.left;
        m_labelRect.top    = m_bodyRect.bottom + Scale (s_kLabelStripGapPx, dpi);
        m_labelRect.right  = m_bodyRect.right;
        m_labelRect.bottom = m_labelRect.top + Scale (s_kLabelStripHeightPx, dpi);

        m_led.PositionAt (m_bodyRect.right - pad - Scale (10, dpi),
                          m_bodyRect.top   + cBodyH / 2 - Scale (3, dpi),
                          dpi);
        SetBounds (OuterRect());
        return;
    }

    m_bodyRect.left   = x;
    m_bodyRect.top    = y;
    m_bodyRect.right  = x + bodyW;
    m_bodyRect.bottom = y + bodyH;

    // Faceplate occupies the BOTTOM portion of the widget; the receding
    // case top is painted above it for fake 3D perspective.
    m_faceRect.left   = x;
    m_faceRect.top    = y + bodyH - faceH;
    m_faceRect.right  = x + bodyW;
    m_faceRect.bottom = y + bodyH;

    m_slotRect.left   = m_faceRect.left  + slotInset;
    m_slotRect.top    = m_faceRect.top   + slotCY - slotH / 2;
    m_slotRect.right  = m_faceRect.right - slotInset;
    m_slotRect.bottom = m_slotRect.top + slotH;

    m_ejectRect.left   = m_faceRect.left + (bodyW - doorW) / 2;
    m_ejectRect.top    = m_faceRect.top + slotCY - doorH / 2;
    m_ejectRect.right  = m_ejectRect.left + doorW;
    m_ejectRect.bottom = m_ejectRect.top + doorH;

    m_labelRect.left   = m_bodyRect.left;
    m_labelRect.top    = m_bodyRect.bottom + Scale (s_kLabelStripGapPx, dpi);
    m_labelRect.right  = m_bodyRect.right;
    m_labelRect.bottom = m_labelRect.top + Scale (s_kLabelStripHeightPx, dpi);

    m_led.PositionAt (m_faceRect.left + Scale (s_kLabelPadPx + s_kInUseWidthPx + s_kInUseGapPx, dpi),
                      m_faceRect.top + Scale (s_kLedCenterYPx, dpi) - Scale (3, dpi),
                      dpi);
    SetBounds (OuterRect());
}




////////////////////////////////////////////////////////////////////////////////
//
//  SyncFromState
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::SyncFromState (const DriveWidgetState & state)
{
    bool  motorOn = state.motorOn.load (std::memory_order_relaxed);
    bool  active  = motorOn || state.diskActive.load (std::memory_order_relaxed);



    m_state.mountedImagePath      = state.mountedImagePath;
    m_state.doorState             = state.doorState;
    m_state.animationStartTimeMs  = state.animationStartTimeMs;
    m_state.lastSyncEventId       = state.lastSyncEventId;
    m_state.motorOn.store (motorOn, std::memory_order_relaxed);
    m_state.diskActive.store (active, std::memory_order_relaxed);

    m_led.SetState (active ? LedState::Active : LedState::Idle);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & dxuiTheme)
{
    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);
    const CassoTheme & theme = static_cast<const CassoTheme &> (dxuiTheme);

    HRESULT  hr           = S_OK;
    int      bodyW        = m_bodyRect.right - m_bodyRect.left;
    int      faceW        = m_faceRect.right - m_faceRect.left;
    int      faceH        = m_faceRect.bottom - m_faceRect.top;
    int      slotW        = m_slotRect.right - m_slotRect.left;
    int      slotH        = m_slotRect.bottom - m_slotRect.top;
    int      doorH        = m_ejectRect.bottom - m_ejectRect.top;
    UINT     dpi          = (m_dpi == 0) ? (UINT) s_kBaseDpi : m_dpi;
    int      notchW       = Scale (s_kNotchWidthPx, dpi);
    int      notchH       = Scale (s_kNotchHeightPx, dpi);
    int      labelPad     = Scale (s_kLabelPadPx, dpi);
    int      inUseW       = Scale (s_kInUseWidthPx, dpi);
    int      caseBackInset = Scale (s_kCaseBackInsetPx, dpi);
    float    labelFontDip = s_kLabelFontDip * (float) dpi / (float) s_kBaseDpi;
    float    inUseFontDip = s_kInUseFontDip * (float) dpi / (float) s_kBaseDpi;
    float    doorOffset   = 0.0f;
    wchar_t  label[32]    = {};



    if (m_focused)
    {
        int   ring = Scale (2, dpi);
        RECT  o    = OuterRect();

        painter.OutlineRect ((float) (o.left  - ring),
                             (float) (o.top   - ring),
                             (float) (o.right  - o.left + ring * 2),
                             (float) (o.bottom - o.top  + ring * 2),
                             (float) std::max (1, Scale (1, dpi)),
                             theme.link);
    }

    if (m_compact)
    {
        int      bodyWcompact  = m_bodyRect.right  - m_bodyRect.left;
        int      bodyHcompact  = m_bodyRect.bottom - m_bodyRect.top;
        int      pad           = Scale (s_kCompactPadPx, dpi);
        float    fontDip       = s_kCompactFontDip * (float) dpi / (float) s_kBaseDpi;
        uint32_t bodyFill      = theme.driveBody;
        uint32_t bezelEdge     = theme.driveBezel;
        uint32_t labelArgb     = theme.driveLabel;

        // Flat card body with a single bezel-coloured outline.
        painter.FillRect    ((float) m_bodyRect.left, (float) m_bodyRect.top,
                             (float) bodyWcompact, (float) bodyHcompact, bodyFill);
        painter.OutlineRect ((float) m_bodyRect.left, (float) m_bodyRect.top,
                             (float) bodyWcompact, (float) bodyHcompact, 1.0f, bezelEdge);

        swprintf_s (label, L"Drive %d", m_drive + 1);
        IGNORE_RETURN_VALUE (hr, text.DrawString (label,
                                                  (float) (m_bodyRect.left + pad),
                                                  (float) m_bodyRect.top,
                                                  (float) (bodyWcompact - 2 * pad - Scale (16, dpi)),
                                                  (float) bodyHcompact,
                                                  labelArgb,
                                                  fontDip,
                                                  s_kFontFamily,
                                                  DxuiTextRenderer::HAlign::Left,
                                                  DxuiTextRenderer::VAlign::Center));

        UNREFERENCED_PARAMETER (bodyW);
        UNREFERENCED_PARAMETER (faceW);
        UNREFERENCED_PARAMETER (faceH);
        UNREFERENCED_PARAMETER (slotW);
        UNREFERENCED_PARAMETER (slotH);
        UNREFERENCED_PARAMETER (doorH);
        UNREFERENCED_PARAMETER (notchW);
        UNREFERENCED_PARAMETER (notchH);
        UNREFERENCED_PARAMETER (labelPad);
        UNREFERENCED_PARAMETER (inUseW);
        UNREFERENCED_PARAMETER (caseBackInset);
        UNREFERENCED_PARAMETER (labelFontDip);
        UNREFERENCED_PARAMETER (inUseFontDip);
        UNREFERENCED_PARAMETER (doorOffset);

        m_led.Paint (painter, text, theme);
        PaintBasenameLabel (text, theme, dpi);
        return;
    }



    // Receding case top: trapezoid spanning the space above the
    // faceplate, narrowing toward the back to suggest perspective.
    // Camera is slightly above and in front of the drive.
    {
        float  frontLeft  = (float) m_bodyRect.left;
        float  frontRight = (float) m_bodyRect.right;
        float  backLeft   = (float) (m_bodyRect.left  + caseBackInset + m_perspectiveSkewPx);
        float  backRight  = (float) (m_bodyRect.right - caseBackInset + m_perspectiveSkewPx);
        float  frontY     = (float) m_faceRect.top;
        float  backY      = (float) m_bodyRect.top;
        uint32_t caseColor   = 0xFFCCB68B;
        uint32_t caseHilite  = 0xFFE6D3AC;
        uint32_t caseShade   = 0xFF8E7A55;
        uint32_t backEdge    = 0xFF5E4F36;

        FillTrapezoidApprox (painter, frontLeft, frontRight, backLeft, backRight,
                             frontY, backY, caseColor);

        // Back-edge dark line (rear of case top).
        painter.FillRect (backLeft, backY, backRight - backLeft, 1.0f, backEdge);

        // Front-edge highlight (where case top meets faceplate top).
        painter.FillRect (frontLeft, frontY - 1.0f, frontRight - frontLeft, 1.0f, caseHilite);

        // Diagonal side highlights -- approximate by drawing a thin
        // line along each slanted edge using small stair-step rects.
        {
            int    edgeH = (int) (frontY - backY);
            int    i     = 0;
            float  denom = (float) ((edgeH > 1) ? (edgeH - 1) : 1);

            for (i = 0; i < edgeH; i++)
            {
                float  t       = (float) i / denom;
                float  leftEdge  = frontLeft  + (backLeft  - frontLeft)  * t;
                float  rightEdge = frontRight + (backRight - frontRight) * t;
                float  y         = frontY - 1.0f - (float) i;

                painter.FillRect (leftEdge,           y, 1.0f, 1.0f, caseShade);
                painter.FillRect (rightEdge - 1.0f,   y, 1.0f, 1.0f, caseShade);
            }
        }

        // Two indented lid panels on the case top, matching the real
        // Disk II's stamped panel design. Each panel is itself a
        // trapezoid that follows the case-top's perspective slant
        // (drawn scanline by scanline so the left/right edges taper
        // toward the back exactly like the case top).
        float  panelInsetTop;
        float  panelInsetBottom;
        {
            float    edgeH        = frontY - backY;
            float    midGapH      = edgeH * 0.08f;
            float    panelH       = (edgeH - midGapH) * 0.5f;
            float    topMargin    = edgeH * 0.10f;
            float    bottomMargin = edgeH * 0.08f;
            float    rearY1       = backY + topMargin;
            float    rearY2       = rearY1 + panelH - topMargin;
            float    frontY1      = rearY2 + midGapH;
            float    frontY2      = frontY - bottomMargin;
            uint32_t panelFill    = 0xFFC0AA82;
            uint32_t panelShadow  = 0xFF8E7A55;
            uint32_t panelHilite  = 0xFFD8C49B;
            float    sideMargin   = 12.0f;

            auto LerpEdges = [&] (float yPos)
            {
                // depth fraction at this y (0 = front, 1 = back)
                float  t          = (yPos - frontY) / (backY - frontY);
                float  leftEdge   = frontLeft  + (backLeft  - frontLeft)  * t;
                float  rightEdge  = frontRight + (backRight - frontRight) * t;
                return std::pair<float,float> (leftEdge + sideMargin, rightEdge - sideMargin);
            };

            // Trapezoidal panel: edges follow case-top perspective.
            // Linearly interpolate between (yTop edges) and (yBot edges)
            // per scanline so the panel narrows toward the back.
            auto DrawPanel = [&] (float yTop, float yBot)
            {
                auto   topEdges    = LerpEdges (yTop);
                auto   bottomEdges = LerpEdges (yBot);
                int    rows        = (int) (yBot - yTop);
                int    i           = 0;
                float  denom       = (float) ((rows > 1) ? (rows - 1) : 1);

                for (i = 0; i < rows; i++)
                {
                    float  t          = (float) i / denom;
                    // Note: i=0 is at yBot (front), i=rows-1 is at yTop (back).
                    float  l          = bottomEdges.first  + (topEdges.first  - bottomEdges.first)  * t;
                    float  r          = bottomEdges.second + (topEdges.second - bottomEdges.second) * t;
                    float  y          = yBot - 1.0f - (float) i;
                    uint32_t fill     = panelFill;

                    if (i == rows - 1)
                    {
                        fill = panelShadow;
                    }
                    else if (i == 0)
                    {
                        fill = panelHilite;
                    }
                    painter.FillRect (l, y, r - l, 1.0f, fill);
                    // Left edge shadow, right edge highlight, follow slant.
                    painter.FillRect (l,        y, 1.0f, 1.0f, panelShadow);
                    painter.FillRect (r - 1.0f, y, 1.0f, 1.0f, panelHilite);
                }
            };

            DrawPanel (rearY1, rearY2);
            DrawPanel (frontY1, frontY2);

            // Cache the rear panel's y-range so the vent slits below
            // can align with it.
            panelInsetTop    = rearY1;
            panelInsetBottom = rearY2;
        }

        // Nine vent slots on each side of the case top, aligned with
        // the rear lid-panel y-range. Matches the real Disk II which
        // has vents on both side faces of the case.
        {
            int      ventCount  = s_kVentCountPx;
            float    ventTop    = panelInsetTop;
            float    ventBottom = panelInsetBottom;
            float    span       = ventBottom - ventTop;
            float    spacing    = span / (float) (ventCount + 1);
            float    slitH      = (float) Scale (s_kVentSlotHeightPx, dpi);
            float    slitInset  = 6.0f;
            uint32_t ventArgb   = 0xFF4A3F2A;
            int      v          = 0;

            for (v = 0; v < ventCount; v++)
            {
                float  y         = ventTop + spacing * (float) (v + 1);
                float  depthT    = (y - frontY) / (backY - frontY);
                float  leftAtY   = frontLeft  + (backLeft  - frontLeft)  * depthT;
                float  rightAtY  = frontRight + (backRight - frontRight) * depthT;
                float  slitLen   = (rightAtY - leftAtY) * 0.10f;
                painter.FillRect (leftAtY  + slitInset,           y, slitLen, slitH, ventArgb);
                painter.FillRect (rightAtY - slitInset - slitLen, y, slitLen, slitH, ventArgb);
            }
        }
    }

    // Black faceplate, inset from the body's left/right edges so the
    // beige case wraps around the faceplate on all four sides
    // (matches the real Disk II's recessed front panel).
    {
        int    faceInsetX = Scale (4, dpi);
        int    faceInsetB = Scale (3, dpi);
        float  ffx = (float) (m_faceRect.left  + faceInsetX);
        float  ffy = (float) m_faceRect.top;
        float  ffw = (float) (faceW - 2 * faceInsetX);
        float  ffh = (float) (faceH - faceInsetB);
        // Beige body shows around the faceplate (top edge already
        // butts against the case-top trapezoid; bottom + sides need
        // explicit fill).
        painter.FillRect ((float) m_faceRect.left, (float) m_faceRect.top,
                          (float) faceW, (float) faceH, 0xFFCCB68B);
        painter.FillRect (ffx, ffy, ffw, ffh, theme.driveBody);
        // Corner chamfer on the inset faceplate.
        painter.FillRect (ffx,             ffy,             1.0f, 1.0f, 0xFFCCB68B);
        painter.FillRect (ffx + ffw - 1,   ffy,             1.0f, 1.0f, 0xFFCCB68B);
        painter.FillRect (ffx,             ffy + ffh - 1,   1.0f, 1.0f, 0xFFCCB68B);
        painter.FillRect (ffx + ffw - 1,   ffy + ffh - 1,   1.0f, 1.0f, 0xFFCCB68B);
    }

    // Slot.
    painter.FillRect ((float) m_slotRect.left, (float) m_slotRect.top, (float) slotW, (float) slotH, theme.driveBezel);

    // Finger-pull recess behind the door: a darker rectangle that the
    // user grabs the disk through. Drawn before the door so it's
    // visually revealed as the door tilts open. Shared inset values
    // used here AND by the door geometry below so the door visually
    // fits inside the recess (same width, slightly shorter so a
    // strip of recess shows above the hinge when the door is closed).
    int    recessInsetX   = Scale (4, dpi);
    int    recessInsetTop = Scale (3, dpi);
    int    recessInsetBot = Scale (2, dpi);
    float  recessLeft     = (float) (m_ejectRect.left  + recessInsetX);
    float  recessRight    = (float) (m_ejectRect.right - recessInsetX);
    float  recessTop      = (float) (m_ejectRect.top + recessInsetTop);
    float  recessBottom   = (float) (m_ejectRect.bottom - recessInsetBot);
    {
        uint32_t recessArgb = 0xFF050505;
        uint32_t shadowArgb = 0xFF000000;

        painter.FillRect (recessLeft, recessTop, recessRight - recessLeft, recessBottom - recessTop, recessArgb);
        // Inset shadow on the top + left edges so the recess reads as
        // sunken below the faceplate surface.
        painter.FillRect (recessLeft, recessTop, recessRight - recessLeft, 1.0f, shadowArgb);
        painter.FillRect (recessLeft, recessTop, 1.0f, recessBottom - recessTop, shadowArgb);
    }

    // Door tab vertical position.
    {
        int64_t  nowMs    = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                std::chrono::steady_clock::now().time_since_epoch()).count();
        int64_t  elapsed  = nowMs - m_state.animationStartTimeMs;
        float    progress = Clamp01 ((float) elapsed / (float) DriveWidgetState::kDoorAnimationMs);

        if (m_state.doorState == DriveWidgetState::Door::Open)
        {
            doorOffset = 1.0f;
        }
        else if (m_state.doorState == DriveWidgetState::Door::Opening)
        {
            doorOffset = progress;
        }
        else if (m_state.doorState == DriveWidgetState::Door::Closing)
        {
            doorOffset = 1.0f - progress;
        }
    }

    {
        // Cantilever rotation: door is hinged a few px inside the
        // slot bezel and tilts up + back as it opens, retracting
        // mostly inside the case. Real Disk II behavior: the door
        // pivots from a point inside the drive face (not flush with
        // the slot top), and tucks 75% of its length inside as it
        // opens, leaving only a small flap sticking out of the slot.
        // Door horizontally matches the recess (insetX from the
        // bezel edges) so the visible dark area stays the same
        // width whether the door is open or closed.
        // Max tilt clamped to 75 deg so the far edge doesn't
        // overshoot the case top.
        constexpr float  kPi                  = 3.14159265f;
        constexpr float  kMaxAngleRad         = 75.0f * kPi / 180.0f;
        constexpr float  kOpenVisibleFraction = 0.25f;     // 75% retracted
        constexpr int    kHingeOffsetDp       = 4;          // pivot sits this far below the recess top
        constexpr int    kFingerNotchDp       = 8;          // bottom strip of recess that stays visible when closed

        float    hingeY     = recessTop + (float) Scale (kHingeOffsetDp, dpi);
        float    hingeL     = recessLeft;
        float    hingeR     = recessRight;
        float    doorBottomY = recessBottom - (float) Scale (kFingerNotchDp, dpi);
        float    doorHf     = doorBottomY - hingeY;
        float    angle      = doorOffset * kMaxAngleRad;
        float    cosA       = cosf (angle);
        float    sinA       = sinf (angle);
        float    visLen     = doorHf * (1.0f - (1.0f - kOpenVisibleFraction) * doorOffset);
        float    depthBack  = visLen * sinA;
        float    visibleH   = visLen * cosA;
        float    caseDepthY = (float) (m_faceRect.top - m_bodyRect.top);
        float    caseFrontW;
        float    perDepthTaper;       // case-side taper magnitude per unit depth
        float    fracL;
        float    fracR;
        float    dxBackL;
        float    dxBackR;
        float    farL;
        float    farR;
        float    farY;
        uint8_t  shade;
        uint32_t doorArgb;
        uint32_t edgeArgb   = 0xFF000000;
        uint32_t hiliteArgb = 0xFF5A5A5A;
        float    yTop;
        float    yBot;
        int      rows;
        int      i          = 0;

        if (caseDepthY < 1.0f)
        {
            caseDepthY = 1.0f;
        }
        caseFrontW      = (float) (m_bodyRect.right - m_bodyRect.left);
        if (caseFrontW < 1.0f)
        {
            caseFrontW = 1.0f;
        }
        perDepthTaper = (float) caseBackInset / caseDepthY;

        // Per-edge perspective: each side of the door's far edge
        // applies the case-top trapezoid's own per-unit-depth taper
        // at that horizontal position. A vertical line at front-x
        // shifts by caseTaper * (1 - 2*fracX) per unit of depth,
        // where fracX is the line's horizontal fraction across the
        // case width (0 = leftmost, 1 = rightmost). Door edges
        // converge inward at the back, matching the case-side
        // inward taper at the same horizontal positions.
        //
        // The case-top trapezoid also shifts laterally by the
        // camera-skew amount, but we deliberately don't apply that
        // to the door: per-drive lateral shift would make the door
        // tilt left/right depending on which drive it's on, and the
        // resulting "door leans the wrong way" reads as more wrong
        // than the lost camera-lateral consistency reads as right.
        // Real 3D rendering would resolve this properly.
        fracL   = (hingeL - (float) m_bodyRect.left) / caseFrontW;
        fracR   = (hingeR - (float) m_bodyRect.left) / caseFrontW;
        dxBackL = depthBack * perDepthTaper * (1.0f - 2.0f * fracL);
        dxBackR = depthBack * perDepthTaper * (1.0f - 2.0f * fracR);

        farL    = hingeL + dxBackL;
        farR    = hingeR + dxBackR;
        farY    = hingeY + visibleH - depthBack;

        // Front face (closed) is the darkest; underside (visible as
        // the door tilts) lerps toward a slightly lighter grey so the
        // tilt reads visually.
        shade     = (uint8_t) (0x1F + (uint32_t) (sinA * (float) (0x4A - 0x1F)));
        doorArgb  = 0xFF000000u | ((uint32_t) shade << 16) | ((uint32_t) shade << 8) | (uint32_t) shade;

        yTop      = std::min (hingeY, farY);
        yBot      = std::max (hingeY, farY);
        rows      = (int) (yBot - yTop);
        if (rows < 1)
        {
            rows = 1;
        }

        // Fill the door parallelogram scanline by scanline.
        for (i = 0; i < rows; i++)
        {
            float  y     = yTop + (float) i;
            float  t;
            float  lx;
            float  rx;

            if (farY >= hingeY)
            {
                t = (y - hingeY) / std::max (farY - hingeY, 1.0f);
            }
            else
            {
                t = (hingeY - y) / std::max (hingeY - farY, 1.0f);
            }
            lx = hingeL + t * (farL - hingeL);
            rx = hingeR + t * (farR - hingeR);
            painter.FillRect (lx, y, rx - lx, 1.0f, doorArgb);
        }

        // Hinge edge highlight (thin lighter line at the top edge so
        // the cantilever pivot reads).
        painter.FillRect (hingeL, hingeY, hingeR - hingeL, 1.0f, hiliteArgb);

        // Far edge (top of door when closed -> rear of door when open):
        // dark outline along whichever screen-y it currently occupies.
        painter.FillRect (farL, farY, farR - farL, 1.0f, edgeArgb);
    }

    // "DRIVE N" upper-left of faceplate. Mounted-disk basename is
    // painted in m_labelRect (below the body) after the case + face
    // rendering completes, so it's the same code path in both
    // skeuomorphic and compact modes.
    swprintf_s (label, L"DRIVE %d", m_drive + 1);
    IGNORE_RETURN_VALUE (hr, text.DrawString (label,
                                              (float) (m_faceRect.left + labelPad),
                                              (float) (m_faceRect.top + labelPad - 2),
                                              (float) (faceW - 2 * labelPad),
                                              labelFontDip + 4.0f,
                                              theme.driveLabel,
                                              labelFontDip,
                                              s_kFontFamily));

    // "IN USE >" label bottom-left of faceplate, LED to its right.
    swprintf_s (label, L"IN USE %s", s_kpszTriangleRight);
    IGNORE_RETURN_VALUE (hr, text.DrawString (label,
                                              (float) (m_faceRect.left + labelPad),
                                              (float) (m_led.GetLayout().coreRect.top - 3),
                                              (float) inUseW,
                                              inUseFontDip + 4.0f,
                                              theme.driveLabel,
                                              inUseFontDip,
                                              s_kFontFamily));

    UNREFERENCED_PARAMETER (bodyW);
    m_led.Paint (painter, text, theme);

    // Cassowary rainbow logo, bottom-right of faceplate (where the
    // Apple logo lives on the real Disk II). Silhouette is left-facing
    // so the bird "watches" the drive slot.
    {
        int    iconW   = Scale (s_kCassowaryWidthPx,  dpi);
        int    iconH   = Scale (s_kCassowaryHeightPx, dpi);
        int    marginX = Scale (s_kCassowaryMarginPx, dpi);
        int    marginY = Scale (s_kCassowaryMarginPx, dpi);
        float  iconX   = (float) (m_faceRect.right  - iconW - marginX);
        float  iconY   = (float) (m_faceRect.bottom - iconH - marginY);

        DrawCassowaryRainbow (painter, iconX, iconY, (float) iconW, (float) iconH);
    }

    PaintBasenameLabel (text, theme, dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintBasenameLabel
//
//  Paints the mounted disk's basename inside m_labelRect (below the
//  drive icon body) in both compact and skeuomorphic paint paths.
//  Hidden when no disk is mounted; ellipsis-truncated to the label
//  strip width via the pure TruncateToWidth algorithm.
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidget::PaintBasenameLabel (
    IDxuiTextRenderer & text,
    const CassoTheme & theme,
    UINT                dpi)
{
    HRESULT                hr             = S_OK;
    int64_t                nowMs          = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                                std::chrono::steady_clock::now().time_since_epoch()).count();
    float                  basenameDip    = s_kBasenameFontDip * (float) dpi / (float) s_kBaseDpi;
    float                  labelLeft      = (float) m_labelRect.left;
    float                  labelTop       = (float) m_labelRect.top;
    float                  labelW         = (float) (m_labelRect.right  - m_labelRect.left);
    float                  labelH         = (float) (m_labelRect.bottom - m_labelRect.top);
    float                  speedPxPerSec  = s_kMarqueeSpeedDipPerSec * (float) dpi / (float) s_kBaseDpi;
    float                  gap            = s_kMarqueeGapDip * (float) dpi / (float) s_kBaseDpi;
    std::filesystem::path  imagePath;
    std::wstring           basename;
    float                  textW          = 0.0f;
    float                  textH          = 0.0f;
    float                  offset         = 0.0f;
    float                  drawX          = 0.0f;
    bool                   clipped        = false;



    if (m_state.mountedImagePath.empty())
    {
        m_marqueePath.clear();
        return;
    }

    imagePath = std::filesystem::path (m_state.mountedImagePath);
    basename  = imagePath.filename().wstring();

    // On a fresh mount, schedule the first scroll after a readable lead-in
    // delay (m_marqueeStartMs is the moment scroll motion begins). A hover
    // enter instead sets it to "now" (UpdateMarqueeHover) for an immediate
    // scroll.
    if (m_state.mountedImagePath != m_marqueePath)
    {
        m_marqueePath    = m_state.mountedImagePath;
        m_marqueeStartMs = nowMs + s_kMarqueeHoldMs;
    }

    hr = text.MeasureString (basename.c_str(), basenameDip, s_kFontFamily, textW, textH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    // Confine all drawing to the drive's label strip so a long name never
    // spills past the drive's left / right bounds or wraps below the strip.
    hr      = text.PushClipRect (labelLeft, labelTop, labelW, labelH);
    clipped = SUCCEEDED (hr);

    if (textW <= labelW)
    {
        // Fits: static and centered.
        IGNORE_RETURN_VALUE (hr, text.DrawString (basename.c_str(),
                                                  labelLeft,
                                                  labelTop,
                                                  labelW,
                                                  labelH,
                                                  theme.driveLabel,
                                                  basenameDip,
                                                  s_kFontFamily,
                                                  DxuiTextRenderer::HAlign::Center,
                                                  DxuiTextRenderer::VAlign::Center));
    }
    else
    {
        int64_t  scrollMs  = 0;
        int64_t  scrollEnd = 0;
        float    period    = textW + gap;
        auto     drawCopy  = [&] (float x)
        {
            HRESULT  dhr = text.DrawString (basename.c_str(),
                                            x,
                                            labelTop,
                                            textW + 1.0f,
                                            labelH,
                                            theme.driveLabel,
                                            basenameDip,
                                            s_kFontFamily,
                                            DxuiTextRenderer::HAlign::Left,
                                            DxuiTextRenderer::VAlign::Center);
            IGNORE_RETURN_VALUE (dhr, S_OK);
        };

        // m_marqueeStartMs is when scroll motion begins (it sits in the
        // future during a mount's lead-in delay; a hover enter sets it to
        // "now"). One scroll travels a full name-plus-gap period; at the
        // period's end the second copy's head sits where the first began,
        // so resting at offset 0 is seamless.
        scrollMs  = (speedPxPerSec > 0.0f)
                        ? (int64_t) (period / speedPxPerSec * 1000.0f)
                        : 0;
        scrollEnd = m_marqueeStartMs + scrollMs;

        if (scrollMs <= 0 || nowMs < m_marqueeStartMs)
        {
            offset = 0.0f;                                        // pre-scroll, at head
        }
        else if (nowMs < scrollEnd)
        {
            offset = period * (float) (nowMs - m_marqueeStartMs) / (float) scrollMs;
        }
        else
        {
            offset = 0.0f;                                        // finished, rest at head

            // While the pointer lingers over the widget, replay the scroll
            // after the inter-scroll delay (the lead-in delay only applies
            // here, between repeats -- the initial hover scroll is instant).
            if (m_marqueeHovered && (nowMs - scrollEnd) >= s_kMarqueeHoldMs)
            {
                m_marqueeStartMs = nowMs;
            }
        }

        drawX = labelLeft - offset;

        // Two copies a period apart: as the first scrolls off the left, the
        // gap then the second copy's head follow it in from the right. The
        // clip rect trims whatever falls outside the drive bounds.
        drawCopy (drawX);
        drawCopy (drawX + period);
    }

    if (clipped)
    {
        IGNORE_RETURN_VALUE (hr, text.PopClipRect());
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

DriveWidgetRegion DriveWidget::HitTest (int x, int y) const
{
    if (RectContains (m_ejectRect, x, y))
    {
        return DriveWidgetRegion::Eject;
    }

    if (RectContains (m_bodyRect, x, y))
    {
        return DriveWidgetRegion::Body;
    }

    return DriveWidgetRegion::None;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnDrop
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DriveWidget::OnDrop (const std::wstring & path)
{
    HRESULT  hr = S_OK;



    CBRA (m_sink);

    hr = m_sink->Mount (m_slot, m_drive, path);
    CHR (hr);

Error:
    return hr;
}
