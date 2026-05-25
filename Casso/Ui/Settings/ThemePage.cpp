#include "Pch.h"

#include "ThemePage.h"





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

    // Mock-window preview dimensions in dp -- a miniature of the live
    // Casso window using the selected theme's actual palette. Keeping
    // these in dp keeps the preview crisp under DPI scaling without
    // bitmap blur.
    constexpr int  s_kPrevTitleBarDp     = 22;
    constexpr int  s_kPrevNavStripDp     = 20;
    constexpr int  s_kPrevDriveBarFullDp = 80;
    constexpr int  s_kPrevDriveBarCompactDp = 28;
    constexpr int  s_kPrevSysButtonWDp   = 22;
    constexpr int  s_kPrevSysButtonGapDp = 2;
    constexpr int  s_kPrevCaptionFontDp  = 11;
    constexpr int  s_kPrevNavFontDp      = 11;
    constexpr int  s_kPrevDriveFontDp    = 10;
    constexpr int  s_kPrevDriveGapDp     = 12;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    void PaintPreviewWindow (DxUiPainter        & painter,
                             DwriteTextRenderer & text,
                             const RECT         & rect,
                             const ChromeTheme  & theme,
                             const DpiScaler    & scaler,
                             const std::function<const uint32_t * (int &, int &)> & framebufferSource)
    {
        int       windowW    = rect.right  - rect.left;
        int       windowH    = rect.bottom - rect.top;
        int       titleH     = scaler.Px (s_kPrevTitleBarDp);
        int       navH       = scaler.Px (s_kPrevNavStripDp);
        int       driveH     = scaler.Px (theme.compactDrives ? s_kPrevDriveBarCompactDp
                                                              : s_kPrevDriveBarFullDp);
        int       sysButtonW = scaler.Px (s_kPrevSysButtonWDp);
        int       sysButtonGap = scaler.Px (s_kPrevSysButtonGapDp);
        float     captionDip = (float) s_kPrevCaptionFontDp * (float) scaler.Dpi() / 96.0f;
        float     navDip     = (float) s_kPrevNavFontDp     * (float) scaler.Dpi() / 96.0f;
        float     driveDip   = (float) s_kPrevDriveFontDp   * (float) scaler.Dpi() / 96.0f;
        HRESULT   hr         = S_OK;



        if (windowW <= 0 || windowH <= 0)
        {
            return;
        }

        // Outer drop-shadow / border so the preview reads as a window
        // on the panel background rather than a flat region.
        painter.OutlineRect ((float) rect.left, (float) rect.top,
                             (float) windowW, (float) windowH, 1.0f, 0xFF101010);

        // ----- Title bar gradient (top -> bottom). -----
        {
            int  bandStep = (titleH > 0) ? titleH : 1;
            int  i        = 0;

            for (i = 0; i < bandStep; i++)
            {
                float    t      = (float) i / (float) bandStep;
                uint8_t  aTop   = (uint8_t) ((theme.titleBarTopArgb    >> 24) & 0xFF);
                uint8_t  rTop   = (uint8_t) ((theme.titleBarTopArgb    >> 16) & 0xFF);
                uint8_t  gTop   = (uint8_t) ((theme.titleBarTopArgb    >>  8) & 0xFF);
                uint8_t  bTop   = (uint8_t) ((theme.titleBarTopArgb         ) & 0xFF);
                uint8_t  aBot   = (uint8_t) ((theme.titleBarBottomArgb >> 24) & 0xFF);
                uint8_t  rBot   = (uint8_t) ((theme.titleBarBottomArgb >> 16) & 0xFF);
                uint8_t  gBot   = (uint8_t) ((theme.titleBarBottomArgb >>  8) & 0xFF);
                uint8_t  bBot   = (uint8_t) ((theme.titleBarBottomArgb      ) & 0xFF);
                uint8_t  a      = (uint8_t) (aTop + (int) ((aBot - (int) aTop) * t));
                uint8_t  r      = (uint8_t) (rTop + (int) ((rBot - (int) rTop) * t));
                uint8_t  g      = (uint8_t) (gTop + (int) ((gBot - (int) gTop) * t));
                uint8_t  b      = (uint8_t) (bTop + (int) ((bBot - (int) bTop) * t));
                uint32_t argb   = ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;

                painter.FillRect ((float) rect.left, (float) (rect.top + i),
                                  (float) windowW, 1.0f, argb);
            }
        }

        // Caption text.
        IGNORE_RETURN_VALUE (hr, text.DrawString (L"Casso",
                                                  (float) (rect.left + scaler.Px (8)),
                                                  (float) rect.top,
                                                  (float) (windowW - 3 * (sysButtonW + sysButtonGap) - scaler.Px (16)),
                                                  (float) titleH,
                                                  theme.titleTextArgb,
                                                  captionDip,
                                                  L"Segoe UI",
                                                  DwriteTextRenderer::HAlign::Left,
                                                  DwriteTextRenderer::VAlign::Center));

        // System buttons: idle-coloured placeholders for min/max, red
        // for close. The idle colour is normally transparent so it
        // reads against the title-bar gradient; that's fine here too.
        {
            int  btnRight = rect.right - sysButtonGap;
            int  btnTop   = rect.top;
            int  btnH     = titleH;

            // Close (rightmost).
            painter.FillRect ((float) (btnRight - sysButtonW), (float) btnTop,
                              (float) sysButtonW, (float) btnH,
                              theme.sysButtonCloseHoverArgb);
            // X glyph
            {
                float  cx = (float) (btnRight - sysButtonW / 2);
                float  cy = (float) (btnTop + btnH / 2);
                float  r  = (float) scaler.Px (4);

                painter.FillRect (cx - r, cy - 0.5f, r * 2.0f, 1.0f, theme.sysButtonCloseHoverGlyphArgb);
                painter.FillRect (cx - 0.5f, cy - r, 1.0f, r * 2.0f, theme.sysButtonCloseHoverGlyphArgb);
            }

            int  btnMaxRight = btnRight - sysButtonW - sysButtonGap;
            painter.FillRect ((float) (btnMaxRight - sysButtonW), (float) btnTop,
                              (float) sysButtonW, (float) btnH, 0x0FFFFFFF);
            painter.OutlineRect ((float) (btnMaxRight - sysButtonW + scaler.Px (6)),
                                 (float) (btnTop + scaler.Px (6)),
                                 (float) (sysButtonW - scaler.Px (12)),
                                 (float) (btnH - scaler.Px (12)),
                                 1.0f, theme.titleTextArgb);

            int  btnMinRight = btnMaxRight - sysButtonW - sysButtonGap;
            painter.FillRect ((float) (btnMinRight - sysButtonW), (float) btnTop,
                              (float) sysButtonW, (float) btnH, 0x0FFFFFFF);
            painter.FillRect ((float) (btnMinRight - sysButtonW + scaler.Px (6)),
                              (float) (btnTop + btnH / 2),
                              (float) (sysButtonW - scaler.Px (12)), 1.0f,
                              theme.titleTextArgb);
        }

        // ----- Nav strip. -----
        {
            int     navTop   = rect.top + titleH;
            wchar_t menuList[] = L"File    Machine    View    Help";

            painter.FillRect ((float) rect.left, (float) navTop,
                              (float) windowW, (float) navH,
                              theme.navStripArgb);
            IGNORE_RETURN_VALUE (hr, text.DrawString (menuList,
                                                      (float) (rect.left + scaler.Px (10)),
                                                      (float) navTop,
                                                      (float) (windowW - scaler.Px (20)),
                                                      (float) navH,
                                                      theme.navItemTextArgb,
                                                      navDip,
                                                      L"Segoe UI",
                                                      DwriteTextRenderer::HAlign::Left,
                                                      DwriteTextRenderer::VAlign::Center));
        }

        // ----- Screen area (everything between nav strip and drive bar). -----
        {
            int       screenTop = rect.top + titleH + navH;
            int       screenH   = std::max (0, windowH - titleH - navH - driveH);
            uint32_t  screenBg  = 0xFF000000;

            painter.FillRect ((float) rect.left, (float) screenTop,
                              (float) windowW, (float) screenH, screenBg);

            // Live emulator framebuffer: D2D bitmap upload + scaled
            // linear blit. The chrome composition pass is sandwiched
            // between the D3D emulator blit and Present, so the
            // framebuffer pointer the source returns is always one
            // frame fresh.
            if (screenH > 0 && framebufferSource)
            {
                int               fbW = 0;
                int               fbH = 0;
                const uint32_t *  fbPixels = framebufferSource (fbW, fbH);

                if (fbPixels != nullptr && fbW > 0 && fbH > 0)
                {
                    // Letterbox the framebuffer inside the screen area
                    // so its native aspect ratio is preserved (Apple II
                    // is 560:384). Without this the preview stretches
                    // and the emulator content looks distorted.
                    float  srcAspect = (float) fbW / (float) fbH;
                    float  dstAspect = (float) windowW / (float) screenH;
                    float  drawW     = (float) windowW;
                    float  drawH     = (float) screenH;
                    float  drawX     = (float) rect.left;
                    float  drawY     = (float) screenTop;

                    if (dstAspect > srcAspect)
                    {
                        drawW = drawH * srcAspect;
                        drawX += ((float) windowW - drawW) * 0.5f;
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

        // ----- Drive bar. -----
        {
            int  driveTop = rect.bottom - driveH;

            painter.FillRect ((float) rect.left, (float) driveTop,
                              (float) windowW, (float) driveH,
                              theme.navStripArgb);

            if (theme.compactDrives)
            {
                // Two flat compact cards centred horizontally.
                int  cardW = scaler.Px (90);
                int  cardH = std::max (scaler.Px (18), driveH - scaler.Px (8));
                int  gap   = scaler.Px (s_kPrevDriveGapDp);
                int  total = cardW * 2 + gap;
                int  cardX = rect.left + (windowW - total) / 2;
                int  cardY = driveTop + (driveH - cardH) / 2;
                int  d     = 0;

                for (d = 0; d < 2; d++)
                {
                    int  x = cardX + d * (cardW + gap);
                    int  ledR = scaler.Px (3);
                    int  ledX = x + cardW - scaler.Px (10);
                    int  ledY = cardY + cardH / 2 - ledR;

                    painter.FillRect    ((float) x, (float) cardY, (float) cardW, (float) cardH, theme.driveBodyArgb);
                    painter.OutlineRect ((float) x, (float) cardY, (float) cardW, (float) cardH, 1.0f, theme.driveBezelArgb);

                    wchar_t  drv[16];
                    swprintf_s (drv, L"Drive %d", d + 1);
                    IGNORE_RETURN_VALUE (hr, text.DrawString (drv,
                                                              (float) (x + scaler.Px (8)),
                                                              (float) cardY,
                                                              (float) (cardW - scaler.Px (24)),
                                                              (float) cardH,
                                                              theme.driveLabelArgb,
                                                              driveDip,
                                                              L"Segoe UI",
                                                              DwriteTextRenderer::HAlign::Left,
                                                              DwriteTextRenderer::VAlign::Center));

                    // LED: idle dot with a halo ring suggesting the
                    // active palette. Using ledActive keeps the
                    // preview lively even though no disk is mounted.
                    painter.FillRect ((float) (ledX - ledR), (float) ledY,
                                      (float) (ledR * 2), (float) (ledR * 2),
                                      theme.ledActiveArgb);
                }
            }
            else
            {
                // Simplified skeuomorphic mini-drives: rectangular
                // beige cases with a darker faceplate band, slot, and
                // an LED on each. Not pixel-perfect to the real
                // widget, but unmistakably "the realistic ones".
                int  cardW = scaler.Px (110);
                int  cardH = std::max (scaler.Px (40), driveH - scaler.Px (10));
                int  gap   = scaler.Px (s_kPrevDriveGapDp);
                int  total = cardW * 2 + gap;
                int  cardX = rect.left + (windowW - total) / 2;
                int  cardY = rect.bottom - driveH + (driveH - cardH) / 2;
                int  d     = 0;

                for (d = 0; d < 2; d++)
                {
                    int  x       = cardX + d * (cardW + gap);
                    int  faceH   = cardH / 2;
                    int  faceY   = cardY + cardH - faceH;
                    int  slotInset = scaler.Px (10);
                    int  slotH   = scaler.Px (3);
                    int  slotY   = faceY + faceH / 2 - slotH / 2;
                    int  ledR    = scaler.Px (3);
                    int  ledX    = x + cardW - scaler.Px (14);
                    int  ledY    = faceY + faceH - scaler.Px (10);

                    // Beige case top.
                    painter.FillRect ((float) x, (float) cardY,
                                      (float) cardW, (float) (cardH - faceH), 0xFFCCB68B);
                    // Darker faceplate.
                    painter.FillRect ((float) x, (float) faceY,
                                      (float) cardW, (float) faceH, theme.driveBodyArgb);
                    painter.OutlineRect ((float) x, (float) cardY,
                                         (float) cardW, (float) cardH, 1.0f, 0xFF000000);
                    // Slot.
                    painter.FillRect ((float) (x + slotInset), (float) slotY,
                                      (float) (cardW - slotInset * 2), (float) slotH,
                                      theme.driveBezelArgb);
                    // "DRIVE N" label.
                    {
                        wchar_t  drv[16];
                        swprintf_s (drv, L"DRIVE %d", d + 1);
                        IGNORE_RETURN_VALUE (hr, text.DrawString (drv,
                                                                  (float) (x + scaler.Px (6)),
                                                                  (float) (faceY + scaler.Px (2)),
                                                                  (float) (cardW - scaler.Px (12)),
                                                                  driveDip + 2.0f,
                                                                  theme.driveLabelArgb,
                                                                  driveDip,
                                                                  L"Segoe UI",
                                                                  DwriteTextRenderer::HAlign::Left,
                                                                  DwriteTextRenderer::VAlign::Top));
                    }
                    // LED.
                    painter.FillRect ((float) (ledX - ledR), (float) ledY,
                                      (float) (ledR * 2), (float) (ledR * 2),
                                      theme.ledActiveArgb);
                }
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

void ThemePage::Layout (const RECT & rect, const DpiScaler & scaler)
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

void ThemePage::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    m_themeLabel.Paint          (painter, text);
    m_themeDropdown.PaintBase   (painter, text);

    // Live preview of the dropdown's current selection. Reads the
    // staged theme name (not the active chrome theme) so the user can
    // see what they'll get before committing OK.
    if (m_previewRect.right > m_previewRect.left &&
        m_previewRect.bottom > m_previewRect.top &&
        m_activeIndex >= 0 && m_activeIndex < (int) m_themeIds.size())
    {
        ChromeTheme  preview = ChromeTheme::ForName (m_themeIds[(size_t) m_activeIndex]);

        PaintPreviewWindow (painter, text, m_previewRect, preview, m_scaler, m_framebufferSource);
    }

    m_themeDropdown.PaintMenu   (painter, text);
}
