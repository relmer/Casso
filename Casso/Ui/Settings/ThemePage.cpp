#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "ThemePage.h"

#include "../Chrome/ChromeMetrics.h"
#include "../IDriveCommandSink.h"
#include "Core/DxuiFormLayout.h"
#include "Core/UnicodeSymbols.h"





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
    // the 100%-zoom window's aspect ratio (which depends on the bottom
    // inset -- full/compact drive bar when a controller is present, or the
    // thin joystick band alone when it isn't). Returns the scale factor used
    // so the caller can size sub-regions consistently.
    void ComputePreviewGeometry (const RECT  & availRect,
                                 int           driveBandDp,
                                 RECT        & outPrevRect,
                                 float       & outScale)
    {
        int    targetWdp    = s_kPrevFbWidthDp;
        int    targetHdp    = s_kPrevTitleBarDp + s_kPrevNavStripDp + s_kPrevFbHeightDp + driveBandDp;
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
                             const CassoTheme                    & theme,
                             bool                                   hasDisk,
                             const std::function<const uint32_t * (int &, int &)> & framebufferSource,
                             const std::function<std::wstring (int)>              & mountedPathSource,
                             const std::function<WriteProtectInfo (int)>          & writeProtectSource,
                             std::array<DriveWidget, 2>           & previewDrives,
                             JoystickToggleButton                 & previewButton)
    {
        RECT      prevRect = {};
        float     scale    = 0.0f;
        HRESULT   hr       = S_OK;
        auto      ScalePx  = [&scale] (int dp) -> int { return (int) ((float) dp * scale); };

        // Bottom inset: a full/compact drive bar when the machine has a Disk ][
        // controller, else just the joystick band (mirrors the live chrome's
        // Phase D reclaim). Drives both the aspect ratio and the painted band.
        int  driveBandDp = hasDisk
            ? (theme.compactDrives ? s_kPrevDriveBarCmptDp : s_kPrevDriveBarFullDp)
            : s_kPrevJoystickBandDp;



        ComputePreviewGeometry (availRect, driveBandDp, prevRect, scale);
        if (scale <= 0.0f)
        {
            return;
        }

        int  prevW       = prevRect.right  - prevRect.left;
        int  prevH       = prevRect.bottom - prevRect.top;
        int  titleH      = ScalePx (s_kPrevTitleBarDp);
        int  navH        = ScalePx (s_kPrevNavStripDp);
        int  driveBarH   = ScalePx (driveBandDp);
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
                uint32_t  argb = LerpArgb (theme.titleBarTop, theme.titleBarBottom, t);

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
                                                      theme.titleText,
                                                      captionDip,
                                                      DxuiTheme::kBodyFace,
                                                      DxuiTextRenderer::HAlign::Left,
                                                      DxuiTextRenderer::VAlign::Center));

            // Close (rightmost) -- drawn in its IDLE state (like min/max), NOT
            // the red hover fill, so the mockup matches a fresh app caption
            // rather than looking permanently hovered. Glyph is a real "x"
            // (multiplication sign) drawn as text since the painter is
            // axis-aligned only (two crossing FillRects read as a "+").
            painter.FillRect ((float) (btnRight - sysBtnW), (float) btnTop,
                              (float) sysBtnW, (float) btnH,
                              theme.sysButtonIdle);
            IGNORE_RETURN_VALUE (hr, text.DrawString (s_kpszMultiplyX,
                                                      (float) (btnRight - sysBtnW),
                                                      (float) btnTop,
                                                      (float) sysBtnW,
                                                      (float) btnH,
                                                      theme.titleText,
                                                      captionDip,
                                                      DxuiTheme::kBodyFace,
                                                      DxuiTextRenderer::HAlign::Center,
                                                      DxuiTextRenderer::VAlign::Center));

            int  btnMaxRight = btnRight - sysBtnW - sysBtnGap;
            painter.FillRect ((float) (btnMaxRight - sysBtnW), (float) btnTop,
                              (float) sysBtnW, (float) btnH, theme.sysButtonIdle);
            painter.OutlineRect ((float) (btnMaxRight - sysBtnW + ScalePx (12)),
                                 (float) (btnTop + ScalePx (10)),
                                 (float) (sysBtnW - ScalePx (24)),
                                 (float) (btnH - ScalePx (20)),
                                 1.0f, theme.titleText);

            int  btnMinRight = btnMaxRight - sysBtnW - sysBtnGap;
            painter.FillRect ((float) (btnMinRight - sysBtnW), (float) btnTop,
                              (float) sysBtnW, (float) btnH, theme.sysButtonIdle);
            painter.FillRect ((float) (btnMinRight - sysBtnW + ScalePx (12)),
                              (float) (btnTop + btnH / 2),
                              (float) (sysBtnW - ScalePx (24)), 1.0f,
                              theme.titleText);
        }

        // ----- Nav strip. -----
        {
            int    navTop = prevRect.top + titleH;
            float  navDip = (float) s_kPrevNavFontDp * scale;

            painter.FillRect ((float) prevRect.left, (float) navTop,
                              (float) prevW, (float) navH,
                              theme.navStrip);
            IGNORE_RETURN_VALUE (hr, text.DrawString (L"File   Edit   Machine   Disk   View   Help",
                                                      (float) (prevRect.left + ScalePx (12)),
                                                      (float) navTop,
                                                      (float) (prevW - ScalePx (24)),
                                                      (float) navH,
                                                      theme.navItemText,
                                                      navDip,
                                                      DxuiTheme::kBodyFace,
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
                              theme.navStrip);

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

            if (writeProtectSource)
            {
                mount0.writeProtect = writeProtectSource (0);
                mount1.writeProtect = writeProtectSource (1);
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

            // Only paint the drive widgets when the machine has a Disk ][
            // controller. With none, driveBandDp has already collapsed the
            // bar to the joystick band, so the preview shows just the band
            // fill + joystick button -- matching the live chrome's Phase D
            // reclaim. (The widgets were laid out above but go undrawn.)
            if (hasDisk)
            {
                previewDrives[0].Paint (painter, text, theme);
                previewDrives[1].Paint (painter, text, theme);
            }

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
//  ThemePage::ThemePage
//
//  Registers the label and dropdown into the panel's child list via
//  Adopt so they participate in the IDxuiControl tree (Bounds,
//  Visible, focus, parent pointers). The widgets remain ThemePage-
//  owned members; Adopt is non-owning. Layout positioning still
//  happens in Layout() below via the legacy SetRect calls because
//  the leaf widgets store their rect twice today (m_rect alongside
//  m_boundsDip) and DxuiPanel's layout-policy walk only writes the
//  latter -- closing that duality so DxuiFormLayout can drive
//  positioning end-to-end is Phase 14 work.
//
////////////////////////////////////////////////////////////////////////////////

ThemePage::ThemePage(std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_themeLabel);
    Adopt (m_themeDropdown);
    Adopt (m_applyNowButton);

    m_applyNowButton.SetLabel   (L"Apply now");
    m_applyNowButton.SetOnClick ([this] { if (m_onApplyThemeNow) { m_onApplyThemeNow (); } });
}




////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::SelectedThemeId
//
////////////////////////////////////////////////////////////////////////////////

std::string ThemePage::SelectedThemeId () const
{
    if (m_activeIndex < 0 || m_activeIndex >= (int) m_themeIds.size())
    {
        return std::string();
    }
    return m_themeIds[(size_t) m_activeIndex];
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
    UINT   dpi        = scaler.Dpi();
    int    pad        = scaler.Px (s_kPagePadDp);
    int    rowHeight  = scaler.Px (s_kRowHeightDp);
    int    labelWidth = scaler.Px (s_kLabelWidthDp);
    int    dropWidth  = scaler.Px (s_kDropdownWidthDp);
    int    x          = rect.left + pad;
    int    y          = rect.top  + pad;
    int    previewGap = scaler.Px (24);
    int    previewTop = y + rowHeight + previewGap;
    RECT   rowBounds  = { x, y, x + labelWidth + dropWidth, y + rowHeight };
    auto   form       = std::make_unique<DxuiFormLayout> ((float) labelWidth,
                                                          (float) rowHeight,
                                                          0.0f,
                                                          0.0f,
                                                          0.0f);



    m_themeLabel.SetText (L"Theme:");
    form->AddRow         (&m_themeLabel, &m_themeDropdown);
    SetLayout            (std::move (form));

    // Drive arrangement through the IDxuiControl tree. DxuiPanel::Layout
    // writes our own bounds via SetBounds(rect) and then asks the
    // policy to assign label / field rects -- replaces the bespoke
    // m_themeLabel.SetRect / m_themeDropdown.SetRect calls that used
    // to live here. Pass the single-row anchor rect so the form fills
    // exactly one row regardless of how tall the page itself is.
    DxuiPanel::Layout (rowBounds, scaler);

    // Mirror the full page footprint after the policy run so the
    // panel's stored bounds match the page area rather than just the
    // single-row form. The Adopt'd children's bounds (written by the
    // form's Arrange call above) are unaffected.
    DxuiPanel::SetBounds (rect);

    m_themeLabel.SetDpi    (dpi);
    m_themeDropdown.SetDpi (dpi);

    // FR-132: the form policy stretches the theme dropdown to fill the
    // row, leaving no room for a button. Re-fix the dropdown to its
    // intended width and dock the "Apply now" button immediately to its
    // right, so both stay near the left of the row (visible regardless of
    // how wide the settings window is).
    int   applyGap   = scaler.Px (8);
    int   applyWidth = scaler.Px (88);
    RECT  dropB      = m_themeDropdown.Bounds();

    dropB.right = dropB.left + dropWidth;
    m_themeDropdown.SetBounds (dropB);

    m_applyNowButton.Layout (MakeRect ((int) dropB.right + applyGap, (int) dropB.top,
                                       applyWidth, (int) (dropB.bottom - dropB.top)));
    m_applyNowButton.SetDpi (dpi);

    m_previewRect.left   = x;
    m_previewRect.top    = previewTop;
    m_previewRect.right  = std::max ((LONG) x, (LONG) (rect.right  - pad));
    m_previewRect.bottom = std::max ((LONG) previewTop, (LONG) (rect.bottom - pad));
    m_scaler             = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Bespoke input + focus shims (OnLButtonDown / OnLButtonUp /
//  OnMouseHover / OnKey / CollectFocusables / AnyDropdownOpen) used
//  to live here. SettingsPanel now dispatches via IDxuiControl::OnMouse /
//  OnKey through DxuiPanel auto fan-out and queries the dropdown
//  directly. The Paint(painter, text, theme) overload stays bespoke
//  because the theme preview window paints between the dropdown box
//  and its menu, which the inherited DxuiPanel walk cannot supply.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::Paint (IDxuiPainter & painterIf, IDxuiTextRenderer & textIf, const IDxuiTheme & theme)
{
    static NullDriveSink  s_kNullSink;

    // The host always paints through the concrete Dxui renderers; the
    // theme-preview window (mock chrome + framebuffer blit) needs their
    // concrete surface, so recover them from the interface references.
    DxuiPainter       & painter = static_cast<DxuiPainter &> (painterIf);
    DxuiTextRenderer  & text    = static_cast<DxuiTextRenderer &> (textIf);


    m_themeDropdown.SetTheme    (&theme);

    m_themeLabel.Paint          (painter, text);
    m_themeDropdown.PaintBase   (painter, text);
    m_applyNowButton.Paint      (painter, text, theme);

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
        CassoTheme  preview = CassoTheme::ForName (m_themeIds[(size_t) previewIndex]);

        if (!m_previewDrivesInitialized)
        {
            m_previewDrives[0].Initialize (6, 0, &s_kNullSink);
            m_previewDrives[1].Initialize (6, 1, &s_kNullSink);
            m_previewDrivesInitialized = true;
        }

        bool  hasDisk = m_hasDiskSource ? m_hasDiskSource () : true;

        PaintPreviewWindow (painter, text, m_previewRect, preview, hasDisk, m_framebufferSource, m_mountedPathSource, m_writeProtectSource, m_previewDrives, m_previewJoystickButton);
    }

    m_themeDropdown.PaintMenu   (painter, text);
}
