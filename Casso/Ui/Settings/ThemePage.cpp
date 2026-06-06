#include "Pch.h"

#include "ThemePage.h"

#include "../Chrome/ChromeMetrics.h"
#include "../IDriveCommandSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int  s_kRowHeightDp     = 28;
    constexpr int  s_kLabelWidthDp    = 140;
    constexpr int  s_kDropdownWidthDp = 220;
    constexpr int  s_kPagePadDp       = 16;

    // 100%-zoom window dimensions in dp. The preview is rendered at
    // the SAME aspect ratio as the live emulator window would be at
    // integer scale 1x, so the user sees a true miniature of what
    // their window will look like after picking OK.
    constexpr int  s_kPrevFbWidthDp       = ChromeMetrics::kFramebufferWidthPx;   // 560
    constexpr int  s_kPrevFbHeightDp      = ChromeMetrics::kFramebufferHeightPx;  // 384
    constexpr int  s_kPrevTitleBarDp      = 32;
    constexpr int  s_kPrevNavStripDp      = 32;
    constexpr int  s_kPrevDriveBarFullDp  = 225;
    constexpr int  s_kPrevDriveBarCmptDp  = 105;
    // Joystick-mode button band height (dp) at the top of the preview
    // drive bar -- mirrors s_kJoystickButtonBandDp in EmulatorShell.cpp.
    constexpr int  s_kPrevJoystickBandDp  = 43;
    constexpr int  s_kPrevSysButtonWDp    = 46;
    constexpr int  s_kPrevSysButtonGapDp  = 1;
    constexpr int  s_kPrevCaptionFontDp   = 14;
    constexpr int  s_kPrevNavFontDp       = 13;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    // Linear interpolation between two ARGB endpoints, premultiplied
    // per-channel. Used for the title-bar gradient bands.
    uint32_t LerpArgb (uint32_t a, uint32_t b, float t)
    {
        uint8_t  aA = (uint8_t) ((a >> 24) & 0xFF);
        uint8_t  rA = (uint8_t) ((a >> 16) & 0xFF);
        uint8_t  gA = (uint8_t) ((a >>  8) & 0xFF);
        uint8_t  bA = (uint8_t) ( a        & 0xFF);
        uint8_t  aB = (uint8_t) ((b >> 24) & 0xFF);
        uint8_t  rB = (uint8_t) ((b >> 16) & 0xFF);
        uint8_t  gB = (uint8_t) ((b >>  8) & 0xFF);
        uint8_t  bB = (uint8_t) ( b        & 0xFF);
        uint8_t  aOut = (uint8_t) (aA + (int) ((aB - (int) aA) * t));
        uint8_t  rOut = (uint8_t) (rA + (int) ((rB - (int) rA) * t));
        uint8_t  gOut = (uint8_t) (gA + (int) ((gB - (int) gA) * t));
        uint8_t  bOut = (uint8_t) (bA + (int) ((bB - (int) bA) * t));

        return ((uint32_t) aOut << 24) | ((uint32_t) rOut << 16) |
               ((uint32_t) gOut <<  8) |  (uint32_t) bOut;
    }


    // Pure-paint sink for the preview-only DriveWidgets: ignores every
    // command. DriveWidget requires an IDriveCommandSink* in Initialize
    // but we only ever Layout + Paint, never dispatch input from the
    // preview, so the sink never receives calls.
    class NullDriveSink : public IDriveCommandSink
    {
    public:
        HRESULT  Mount (int /*slot*/, int /*drive*/, const std::wstring & /*path*/) override { return S_OK; }
        void     Eject (int /*slot*/, int /*drive*/)                                override { }
    };


    // Computes the actual preview rect inside availRect that matches
    // the 100%-zoom window's aspect ratio (which depends on whether
    // the staged theme uses compact drives, since that changes the
    // bottom inset). Returns the scale factor used so the caller can
    // size sub-regions consistently.
    void ComputePreviewGeometry (const RECT  & availRect,
                                 bool          compactDrives,
                                 RECT        & outPrevRect,
                                 float       & outScale)
    {
        int    driveDp      = compactDrives ? s_kPrevDriveBarCmptDp : s_kPrevDriveBarFullDp;
        int    targetWdp    = s_kPrevFbWidthDp;
        int    targetHdp    = s_kPrevTitleBarDp + s_kPrevNavStripDp + s_kPrevFbHeightDp + driveDp;
        int    availW       = std::max (0, (int) (availRect.right  - availRect.left));
        int    availH       = std::max (0, (int) (availRect.bottom - availRect.top));
        float  targetAspect = (float) targetWdp / (float) targetHdp;
        float  availAspect  = (availH > 0) ? ((float) availW / (float) availH) : 0.0f;
        int    prevW        = 0;
        int    prevH        = 0;

        if (availW <= 0 || availH <= 0)
        {
            outPrevRect = {};
            outScale    = 0.0f;
            return;
        }

        if (availAspect > targetAspect)
        {
            // Height-limited: preview is as tall as available, width
            // shrinks to preserve aspect.
            prevH = availH;
            prevW = (int) ((float) prevH * targetAspect);
        }
        else
        {
            prevW = availW;
            prevH = (int) ((float) prevW / targetAspect);
        }

        outPrevRect.left   = availRect.left + (availW - prevW) / 2;
        outPrevRect.top    = availRect.top  + (availH - prevH) / 2;
        outPrevRect.right  = outPrevRect.left + prevW;
        outPrevRect.bottom = outPrevRect.top  + prevH;
        outScale           = (float) prevW / (float) targetWdp;
    }


    void PaintPreviewWindow (DxuiPainter                          & painter,
                             DxuiTextRenderer                   & text,
                             const RECT                           & availRect,
                             const ChromeTheme                    & theme,
                             const std::function<const uint32_t * (int &, int &)> & framebufferSource,
                             const std::function<std::wstring (int)>              & mountedPathSource,
                             std::array<DriveWidget, 2>           & previewDrives,
                             JoystickToggleButton                 & previewButton)
    {
        RECT      prevRect = {};
        float     scale    = 0.0f;
        HRESULT   hr       = S_OK;
        auto      ScalePx  = [&scale] (int dp) -> int { return (int) ((float) dp * scale); };



        ComputePreviewGeometry (availRect, theme.compactDrives, prevRect, scale);
        if (scale <= 0.0f)
        {
            return;
        }

        int  prevW       = prevRect.right  - prevRect.left;
        int  prevH       = prevRect.bottom - prevRect.top;
        int  titleH      = ScalePx (s_kPrevTitleBarDp);
        int  navH        = ScalePx (s_kPrevNavStripDp);
        int  driveBarH   = ScalePx (theme.compactDrives ? s_kPrevDriveBarCmptDp : s_kPrevDriveBarFullDp);
        int  screenH     = std::max (0, prevH - titleH - navH - driveBarH);
        UINT effectiveDpi = (UINT) std::max (24, (int) (96.0f * scale));

        // Outer 1px frame so the preview reads as a discrete window
        // on the panel background.
        painter.OutlineRect ((float) prevRect.left, (float) prevRect.top,
                             (float) prevW, (float) prevH, 1.0f, 0xFF101010);

        // ----- Title bar gradient. -----
        {
            int  bandSteps = std::max (1, titleH);
            int  i         = 0;

            for (i = 0; i < bandSteps; i++)
            {
                float     t    = (float) i / (float) bandSteps;
                uint32_t  argb = LerpArgb (theme.titleBarTopArgb, theme.titleBarBottomArgb, t);

                painter.FillRect ((float) prevRect.left, (float) (prevRect.top + i),
                                  (float) prevW, 1.0f, argb);
            }
        }

        // Caption + system buttons.
        {
            int    sysBtnW      = ScalePx (s_kPrevSysButtonWDp);
            int    sysBtnGap    = std::max (0, ScalePx (s_kPrevSysButtonGapDp));
            float  captionDip   = (float) s_kPrevCaptionFontDp * scale;
            int    btnRight     = prevRect.right;
            int    btnTop       = prevRect.top;
            int    btnH         = titleH;

            IGNORE_RETURN_VALUE (hr, text.DrawString (L"Casso emulator",
                                                      (float) (prevRect.left + ScalePx (12)),
                                                      (float) btnTop,
                                                      (float) (prevW - 3 * (sysBtnW + sysBtnGap) - ScalePx (24)),
                                                      (float) btnH,
                                                      theme.titleTextArgb,
                                                      captionDip,
                                                      L"Segoe UI",
                                                      DxuiTextRenderer::HAlign::Left,
                                                      DxuiTextRenderer::VAlign::Center));

            // Close (rightmost) -- always red.
            painter.FillRect ((float) (btnRight - sysBtnW), (float) btnTop,
                              (float) sysBtnW, (float) btnH,
                              theme.sysButtonCloseHoverArgb);
            {
                float  cx = (float) (btnRight - sysBtnW / 2);
                float  cy = (float) (btnTop + btnH / 2);
                float  r  = (float) ScalePx (5);

                painter.FillRect (cx - r, cy - 0.5f, r * 2.0f, 1.0f, theme.sysButtonCloseHoverGlyphArgb);
                painter.FillRect (cx - 0.5f, cy - r, 1.0f, r * 2.0f, theme.sysButtonCloseHoverGlyphArgb);
            }

            int  btnMaxRight = btnRight - sysBtnW - sysBtnGap;
            painter.FillRect ((float) (btnMaxRight - sysBtnW), (float) btnTop,
                              (float) sysBtnW, (float) btnH, theme.sysButtonIdleArgb);
            painter.OutlineRect ((float) (btnMaxRight - sysBtnW + ScalePx (12)),
                                 (float) (btnTop + ScalePx (10)),
                                 (float) (sysBtnW - ScalePx (24)),
                                 (float) (btnH - ScalePx (20)),
                                 1.0f, theme.titleTextArgb);

            int  btnMinRight = btnMaxRight - sysBtnW - sysBtnGap;
            painter.FillRect ((float) (btnMinRight - sysBtnW), (float) btnTop,
                              (float) sysBtnW, (float) btnH, theme.sysButtonIdleArgb);
            painter.FillRect ((float) (btnMinRight - sysBtnW + ScalePx (12)),
                              (float) (btnTop + btnH / 2),
                              (float) (sysBtnW - ScalePx (24)), 1.0f,
                              theme.titleTextArgb);
        }

        // ----- Nav strip. -----
        {
            int    navTop = prevRect.top + titleH;
            float  navDip = (float) s_kPrevNavFontDp * scale;

            painter.FillRect ((float) prevRect.left, (float) navTop,
                              (float) prevW, (float) navH,
                              theme.navStripArgb);
            IGNORE_RETURN_VALUE (hr, text.DrawString (L"File   Machine   View   Help",
                                                      (float) (prevRect.left + ScalePx (12)),
                                                      (float) navTop,
                                                      (float) (prevW - ScalePx (24)),
                                                      (float) navH,
                                                      theme.navItemTextArgb,
                                                      navDip,
                                                      L"Segoe UI",
                                                      DxuiTextRenderer::HAlign::Left,
                                                      DxuiTextRenderer::VAlign::Center));
        }

        // ----- Screen area: live emulator framebuffer, aspect-fit. -----
        {
            int  screenTop = prevRect.top + titleH + navH;

            painter.FillRect ((float) prevRect.left, (float) screenTop,
                              (float) prevW, (float) screenH, 0xFF000000);

            if (screenH > 0 && framebufferSource)
            {
                int               fbW = 0;
                int               fbH = 0;
                const uint32_t *  fbPixels = framebufferSource (fbW, fbH);

                if (fbPixels != nullptr && fbW > 0 && fbH > 0)
                {
                    float  srcAspect = (float) fbW / (float) fbH;
                    float  dstAspect = (float) prevW / (float) screenH;
                    float  drawW     = (float) prevW;
                    float  drawH     = (float) screenH;
                    float  drawX     = (float) prevRect.left;
                    float  drawY     = (float) screenTop;

                    if (dstAspect > srcAspect)
                    {
                        drawW = drawH * srcAspect;
                        drawX += ((float) prevW - drawW) * 0.5f;
                    }
                    else
                    {
                        drawH = drawW / srcAspect;
                        drawY += ((float) screenH - drawH) * 0.5f;
                    }

                    HRESULT  hrFb = text.DrawFramebuffer (fbPixels, fbW, fbH,
                                                          drawX, drawY, drawW, drawH);
                    IGNORE_RETURN_VALUE (hrFb, S_OK);
                }
            }
        }

        // ----- Drive bar: real DriveWidget instances at preview scale. -----
        {
            int       driveTop  = prevRect.bottom - driveBarH;
            int       gap       = std::max (1, ScalePx (16));
            ChromeVisualState  visual = {};

            painter.FillRect ((float) prevRect.left, (float) driveTop,
                              (float) prevW, (float) driveBarH,
                              theme.navStripArgb);

            // Layout each preview drive: probe widget[0] for its
            // intrinsic size at the effective DPI, then space the
            // pair horizontally just like
            // LayoutDriveWidgetsInCommandBar does for the live chrome.
            previewDrives[0].SetCompact (theme.compactDrives);
            previewDrives[1].SetCompact (theme.compactDrives);

            // Preview the actual mounted disk paths so the basename
            // label strip reflects the live drive state. Falls back to
            // empty (no label) if the host hasn't wired a source or the
            // drive is empty.
            DriveWidgetState  mount0;
            DriveWidgetState  mount1;

            if (mountedPathSource)
            {
                mount0.mountedImagePath = mountedPathSource (0);
                mount1.mountedImagePath = mountedPathSource (1);
            }

            mount0.doorState = mount0.mountedImagePath.empty()
                ? DriveWidgetState::Door::Open
                : DriveWidgetState::Door::Closed;
            mount1.doorState = mount1.mountedImagePath.empty()
                ? DriveWidgetState::Door::Open
                : DriveWidgetState::Door::Closed;

            previewDrives[0].SyncFromState (mount0);
            previewDrives[1].SyncFromState (mount1);

            DxuiDpiScaler  previewScaler;
            RECT           previewAnchor = { 0, 0, 0, 0 };

            previewScaler.SetDpi (effectiveDpi);
            previewDrives[0].Layout (previewAnchor, previewScaler);

            RECT  probe   = previewDrives[0].OuterRect();
            int   widgetW = probe.right  - probe.left;
            int   widgetH = probe.bottom - probe.top;
            int   totalW  = widgetW * 2 + gap;
            int   startX  = prevRect.left + std::max (0, (prevW - totalW) / 2);
            int   labelGapPx = std::max (1, ScalePx (2));
            int   widgetY = prevRect.bottom - widgetH - labelGapPx;
            int   d       = 0;

            for (d = 0; d < 2; d++)
            {
                int   widgetX       = startX + d * (widgetW + gap);
                int   widgetCenterX = widgetX + widgetW / 2;
                int   vanishingX    = prevRect.left + prevW / 2;
                int   skewPx        = MulDiv (vanishingX - widgetCenterX, 27, 100);
                RECT  widgetAnchor  = { widgetX, widgetY, widgetX, widgetY };

                previewDrives[(size_t) d].SetPerspectiveSkewPx (skewPx);
                previewDrives[(size_t) d].Layout (widgetAnchor, previewScaler);
            }

            visual.dpi        = effectiveDpi;
            visual.nowMs      = 0;
            visual.frameIndex = 0;
            previewDrives[0].Paint (painter, text, theme);
            previewDrives[1].Paint (painter, text, theme);

            // Joystick-mode toggle button -- preview as "on" so the lit
            // blue LED reads in the band above the drives, matching the
            // live chrome's resting state when the user toggled it on.
            {
                int   bandHeight = std::max (1, ScalePx (s_kPrevJoystickBandDp));
                int   bandTop    = driveTop;
                int   centerX    = prevRect.left + prevW / 2;
                int   centerY    = bandTop + bandHeight / 2;
                RECT  anchor     = { centerX, centerY, centerX, centerY };

                previewButton.SetTextRenderer (&text);
                previewButton.SetOn           (true);
                previewButton.Layout          (anchor, previewScaler);
                previewButton.Paint           (painter, text, theme);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::SetThemes
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::SetThemes (std::vector<std::string>  themeIds,
                           std::vector<std::wstring> displayNames,
                           int                       activeIndex)
{
    m_themeIds    = std::move (themeIds);
    m_activeIndex = activeIndex;

    if (displayNames.size() != m_themeIds.size())
    {
        // Caller mismatch -- fall back to ids as labels so the panel
        // is still functional even if a friendly name table is missing.
        displayNames.clear();
        for (const std::string & id : m_themeIds)
        {
            displayNames.emplace_back (id.begin(), id.end());
        }
    }

    m_themeDropdown.SetItems    (displayNames);
    m_themeDropdown.SetSelected (m_activeIndex);
    m_themeDropdown.SetSelect ([this] (int idx)
    {
        if (idx < 0 || idx >= (int) m_themeIds.size())
        {
            return;
        }
        m_activeIndex = idx;
        if (m_onThemeSelected)
        {
            m_onThemeSelected (m_themeIds[(size_t) idx]);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi        = scaler.Dpi();
    int   pad        = scaler.Px (s_kPagePadDp);
    int   rowHeight  = scaler.Px (s_kRowHeightDp);
    int   labelWidth = scaler.Px (s_kLabelWidthDp);
    int   dropWidth  = scaler.Px (s_kDropdownWidthDp);
    int   x          = rect.left + pad;
    int   y          = rect.top  + pad;
    int   controlsX  = x + labelWidth;
    int   previewGap = scaler.Px (24);
    int   previewTop = y + rowHeight + previewGap;



    m_themeLabel.SetRect    (MakeRect (x, y, labelWidth, rowHeight));
    m_themeLabel.SetText    (L"Theme:");
    m_themeDropdown.SetRect (MakeRect (controlsX, y, dropWidth, rowHeight));

    m_themeLabel.SetDpi    (dpi);
    m_themeDropdown.SetDpi (dpi);

    m_previewRect.left   = x;
    m_previewRect.top    = previewTop;
    m_previewRect.right  = std::max ((LONG) x, (LONG) (rect.right  - pad));
    m_previewRect.bottom = std::max ((LONG) previewTop, (LONG) (rect.bottom - pad));
    m_scaler             = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::OnLButtonDown (int x, int y)
{
    if (m_themeDropdown.OnLButtonDown (x, y)) { return; }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::OnLButtonUp (int x, int y)
{
    (void) m_themeDropdown.OnLButtonUp (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::OnMouseHover (int x, int y)
{
    m_themeDropdown.SetMouseHover (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool ThemePage::OnKey (WPARAM vk)
{
    if (m_themeDropdown.HandleKey (vk)) { return true; }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::CollectFocusables
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::CollectFocusables (std::vector<std::function<void (bool)>> & out)
{
    out.push_back ([this] (bool f) { m_themeDropdown.SetFocused (f); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::Paint (DxuiPainter & painter, DxuiTextRenderer & text) const
{
    static NullDriveSink  s_kNullSink;

    m_themeLabel.Paint          (painter, text);
    m_themeDropdown.PaintBase   (painter, text);

    // Live preview tracks the dropdown's effective hovered/highlighted
    // item while open (so mouse hover and arrow-key nav both update
    // the mock window immediately), and falls back to the committed
    // selection when closed. Matches the monitor dropdown live channel.
    int  previewIndex = m_activeIndex;

    if (m_themeDropdown.IsOpen())
    {
        int  highlighted = m_themeDropdown.HighlightIndex();

        if (highlighted >= 0 && highlighted < (int) m_themeIds.size())
        {
            previewIndex = highlighted;
        }
    }

    if (m_previewRect.right > m_previewRect.left &&
        m_previewRect.bottom > m_previewRect.top &&
        previewIndex >= 0 && previewIndex < (int) m_themeIds.size())
    {
        ChromeTheme  preview = ChromeTheme::ForName (m_themeIds[(size_t) previewIndex]);

        if (!m_previewDrivesInitialized)
        {
            m_previewDrives[0].Initialize (6, 0, &s_kNullSink);
            m_previewDrives[1].Initialize (6, 1, &s_kNullSink);
            m_previewDrivesInitialized = true;
        }

        PaintPreviewWindow (painter, text, m_previewRect, preview, m_framebufferSource, m_mountedPathSource, m_previewDrives, m_previewJoystickButton);
    }

    m_themeDropdown.PaintMenu   (painter, text);
}
