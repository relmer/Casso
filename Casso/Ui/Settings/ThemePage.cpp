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

RECT ThemePage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };
    return rc;
}


// Linear interpolation between two ARGB endpoints, premultiplied
// per-channel. Used for the title-bar gradient bands.
uint32_t ThemePage::LerpArgb (uint32_t a, uint32_t b, float t)
{
    uint8_t  aA   = (uint8_t) ((a >> 24) & 0xFF);
    uint8_t  rA   = (uint8_t) ((a >> 16) & 0xFF);
    uint8_t  gA   = (uint8_t) ((a >>  8) & 0xFF);
    uint8_t  bA   = (uint8_t) ( a        & 0xFF);
    uint8_t  aB   = (uint8_t) ((b >> 24) & 0xFF);
    uint8_t  rB   = (uint8_t) ((b >> 16) & 0xFF);
    uint8_t  gB   = (uint8_t) ((b >>  8) & 0xFF);
    uint8_t  bB   = (uint8_t) ( b        & 0xFF);
    uint8_t  aOut = (uint8_t) (aA + (int) ((aB - (int) aA) * t));
    uint8_t  rOut = (uint8_t) (rA + (int) ((rB - (int) rA) * t));
    uint8_t  gOut = (uint8_t) (gA + (int) ((gB - (int) gA) * t));
    uint8_t  bOut = (uint8_t) (bA + (int) ((bB - (int) bA) * t));



    return ((uint32_t) aOut << 24) | ((uint32_t) rOut << 16) |
           ((uint32_t) gOut <<  8) |  (uint32_t) bOut;
}

void ThemePage::ComputePreviewGeometry (const RECT  & availRect,
                                        int           driveBandDp,
                                        RECT        & outPrevRect,
                                        float       & outScale)
{
    int    targetWdp    = kPrevFbWidthDp;
    int    targetHdp    = kPrevTitleBarDp + kPrevNavStripDp + kPrevFbHeightDp + driveBandDp;
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


void ThemePage::PaintPreviewWindow (DxuiPainter                          & painter,
                                   DxuiTextRenderer                     & text,
                                   const RECT                           & availRect,
                                   const CassoTheme                     & theme,
                                   bool                                   hasDisk,
                                   const std::function<const uint32_t * (int &, int &)> & framebufferSource,
                                   const std::function<std::wstring (int)>              & mountedPathSource,
                                   const std::function<WriteProtectInfo (int)>          & writeProtectSource,
                                   std::array<DriveWidget, 2>           & previewDrives,
                                   JoystickToggleButton                 & previewButton)
{
    RECT     prevRect     = {};
    float    scale        = 0.0f;
    HRESULT  hr           = S_OK;
    int      prevW        = 0;
    int      prevH        = 0;
    int      titleH       = 0;
    int      navH         = 0;
    int      driveBarH    = 0;
    int      screenH      = 0;
    UINT     effectiveDpi = 0;
    auto      ScalePx  = [&scale] (int dp) -> int { return (int) ((float) dp * scale); };

    // Bottom inset: a full/compact drive bar when the machine has a Disk ][
    // controller, else just the joystick band (mirrors the live chrome's
    // Phase D reclaim). Drives both the aspect ratio and the painted band.
    int  driveBandDp = hasDisk
        ? (theme.compactDrives ? kPrevDriveBarCmptDp : kPrevDriveBarFullDp)
        : kPrevJoystickBandDp;



    ComputePreviewGeometry (availRect, driveBandDp, prevRect, scale);
    if (scale <= 0.0f)
    {
        return;
    }

    prevW = prevRect.right  - prevRect.left;
    prevH = prevRect.bottom - prevRect.top;
    titleH = ScalePx (kPrevTitleBarDp);
    navH = ScalePx (kPrevNavStripDp);
    driveBarH = ScalePx (driveBandDp);
    screenH = std::max (0, prevH - titleH - navH - driveBarH);
    effectiveDpi = (UINT) std::max (24, (int) (96.0f * scale));

    // Outer 1px frame so the preview reads as a discrete window
    // on the panel background.
    painter.OutlineRect ((float) prevRect.left, (float) prevRect.top,
                         (float) prevW, (float) prevH, 1.0f, 0xFF101010);

    // Title bar gradient.
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
        int    sysBtnW     = ScalePx (kPrevSysButtonWDp);
        int    sysBtnGap   = std::max (0, ScalePx (kPrevSysButtonGapDp));
        float  captionDip  = (float) kPrevCaptionFontDp * scale;
        int    btnRight    = prevRect.right;
        int    btnTop      = prevRect.top;
        int    btnH        = titleH;
        int    btnMaxRight = 0;
        int    btnMinRight = 0;

        hr = text.DrawString (L"Casso emulator",
                              (float) (prevRect.left + ScalePx (12)),
                              (float) btnTop,
                              (float) (prevW - 3 * (sysBtnW + sysBtnGap) - ScalePx (24)),
                              (float) btnH,
                              theme.titleText,
                              captionDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextRenderer::HAlign::Left,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);

        // Close (rightmost) -- drawn in its IDLE state (like min/max), NOT
        // the red hover fill, so the mockup matches a fresh app caption
        // rather than looking permanently hovered. Glyph is a real "x"
        // (multiplication sign) drawn as text since the painter is
        // axis-aligned only (two crossing FillRects read as a "+").
        painter.FillRect ((float) (btnRight - sysBtnW), (float) btnTop,
                          (float) sysBtnW, (float) btnH,
                          theme.sysButtonIdle);
        hr = text.DrawString (s_kpszMultiplyX,
                              (float) (btnRight - sysBtnW),
                              (float) btnTop,
                              (float) sysBtnW,
                              (float) btnH,
                              theme.titleText,
                              captionDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextRenderer::HAlign::Center,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);

        btnMaxRight = btnRight - sysBtnW - sysBtnGap;
        painter.FillRect ((float) (btnMaxRight - sysBtnW), (float) btnTop,
                          (float) sysBtnW, (float) btnH, theme.sysButtonIdle);
        painter.OutlineRect ((float) (btnMaxRight - sysBtnW + ScalePx (12)),
                             (float) (btnTop + ScalePx (10)),
                             (float) (sysBtnW - ScalePx (24)),
                             (float) (btnH - ScalePx (20)),
                             1.0f, theme.titleText);

        btnMinRight = btnMaxRight - sysBtnW - sysBtnGap;
        painter.FillRect ((float) (btnMinRight - sysBtnW), (float) btnTop,
                          (float) sysBtnW, (float) btnH, theme.sysButtonIdle);
        painter.FillRect ((float) (btnMinRight - sysBtnW + ScalePx (12)),
                          (float) (btnTop + btnH / 2),
                          (float) (sysBtnW - ScalePx (24)), 1.0f,
                          theme.titleText);
    }

    // Nav strip.
    {
        int    navTop = prevRect.top + titleH;
        float  navDip = (float) kPrevNavFontDp * scale;

        painter.FillRect ((float) prevRect.left, (float) navTop,
                          (float) prevW, (float) navH,
                          theme.navStrip);
        hr = text.DrawString (L"File   Edit   Machine   Disk   View   Help",
                              (float) (prevRect.left + ScalePx (12)),
                              (float) navTop,
                              (float) (prevW - ScalePx (24)),
                              (float) navH,
                              theme.navItemText,
                              navDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextRenderer::HAlign::Left,
                              DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    // Screen area: live emulator framebuffer, aspect-fit.
    {
        int  screenTop = prevRect.top + titleH + navH;

        painter.FillRect ((float) prevRect.left, (float) screenTop,
                          (float) prevW, (float) screenH, 0xFF000000);

        if (screenH > 0 && framebufferSource)
        {
            int               fbW      = 0;
            int               fbH      = 0;
            const uint32_t  * fbPixels = framebufferSource (fbW, fbH);

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

    // Drive bar: real DriveWidget instances at preview scale.
    {
        int                driveTop      = prevRect.bottom - driveBarH;
        int                gap           = std::max (1, ScalePx (16));
        ChromeVisualState  visual        = {};
        DriveWidgetState   mount0;
        DriveWidgetState   mount1;
        DxuiDpiScaler      previewScaler;
        RECT               previewAnchor = {};
        RECT               probe         = {};
        int                widgetW       = 0;
        int                widgetH       = 0;
        int                totalW        = 0;
        int                startX        = 0;
        int                labelGapPx    = 0;
        int                widgetY       = 0;
        int                d             = 0;

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

        previewAnchor = { 0, 0, 0, 0 };

        previewScaler.SetDpi (effectiveDpi);
        previewDrives[0].Layout (previewAnchor, previewScaler);

        probe = previewDrives[0].OuterRect();
        widgetW = probe.right  - probe.left;
        widgetH = probe.bottom - probe.top;
        totalW = widgetW * 2 + gap;
        startX = prevRect.left + std::max (0, (prevW - totalW) / 2);
        labelGapPx = std::max (1, ScalePx (2));
        widgetY = prevRect.bottom - widgetH - labelGapPx;

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
            int   bandHeight = std::max (1, ScalePx (kPrevJoystickBandDp));
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
    Adopt (m_monitorFrameCheckbox);

    m_applyNowButton.SetLabel   (L"Apply now");
    m_applyNowButton.SetOnClick ([this] { if (m_onApplyThemeNow) { m_onApplyThemeNow(); } });

    // Skeuo desk-scene opt-in. Applies live (the change is visible on the
    // real chrome behind the sheet immediately); enabled only while a
    // skeuomorphic theme is selected.
    m_monitorFrameCheckbox.SetLabel (L"CRT monitor desk scene (uses more screen space)");
    m_monitorFrameCheckbox.SetSingleLineLabel (true);
    m_monitorFrameCheckbox.SetOnChange ([this] (bool checked)
    {
        if (m_onMonitorFrameToggled) { m_onMonitorFrameToggled (checked); }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::UpdateMonitorCheckboxEnabled
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::UpdateMonitorCheckboxEnabled()
{
    std::string  selected = SelectedThemeId();
    bool         isSkeuo  = !selected.empty()
                            && !CassoTheme::ForName (selected).compactDrives;

    m_monitorFrameCheckbox.SetEnabled (isSkeuo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::SelectedThemeId
//
////////////////////////////////////////////////////////////////////////////////

std::string ThemePage::SelectedThemeId() const
{
    bool  hasSelection = (m_activeIndex >= 0 && m_activeIndex < (int) m_themeIds.size());



    // Empty means "no theme selected", which the caller treats as leave the
    // current theme alone.
    return hasSelection ? m_themeIds[(size_t) m_activeIndex] : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::SetThemes
//
//  Populates the theme dropdown from parallel id and display-name lists.
//
//  Ids and display names are kept SEPARATE because the id is what persists in
//  prefs and what the theme manager is addressed by, while the display name is
//  user-facing and may be localized or renamed without invalidating anyone's
//  saved setting.
//
//  A caller mismatch between the two lists falls back to using the ids as
//  labels rather than failing or truncating. The page stays functional with
//  slightly uglier text, which is a far better outcome than a theme picker
//  that cannot pick a theme.
//
//  The selection callback re-validates its index. It fires from the dropdown,
//  which is driven by user input and by SetSelected, and the id list can be
//  replaced underneath it.
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
        UpdateMonitorCheckboxEnabled();
        if (m_onThemeSelected)
        {
            m_onThemeSelected (m_themeIds[(size_t) idx]);
        }
    });

    UpdateMonitorCheckboxEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage::Layout
//
//  Arranges the theme row, the desk-scene checkbox, and the preview area.
//
//  Arrangement is driven through the IDxuiControl tree rather than by setting
//  each child's rect directly: a form layout policy assigns the label and
//  field rects, so the page participates in the standard layout contract
//  instead of hand-placing controls.
//
//  Two rects are therefore written in sequence, and the order is deliberate.
//  The form is run against a SINGLE-ROW anchor so it fills exactly one row
//  regardless of page height, then the panel's own bounds are re-set to the
//  full page footprint. The adopted children keep the rects the form's arrange
//  pass gave them; only the panel's stored bounds change.
//
//  The dropdown is then re-fixed to its intended width, because the form
//  policy stretches the field to fill the row and would leave no space for the
//  Apply button. Docking the button immediately to its right keeps both near
//  the left edge, visible no matter how wide the settings window is (FR-132).
//
//  The checkbox row spans label + dropdown + gap + button so its caption
//  cannot truncate against a narrower control.
//
//  The preview rect is clamped so a window narrower or shorter than the
//  padding yields an empty rect rather than an inverted one.
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi        = scaler.Dpi();
    int   pad        = scaler.Px (kPagePadDp);
    int   rowHeight  = scaler.Px (kRowHeightDp);
    int   labelWidth = scaler.Px (kLabelWidthDp);
    int   dropWidth  = scaler.Px (kDropdownWidthDp);
    int   x          = rect.left + pad;
    int   y          = rect.top  + pad;
    int   rowGap     = scaler.Px (8);
    int   previewGap = scaler.Px (24);
    int   previewTop = y + 2 * rowHeight + rowGap + previewGap;
    RECT  rowBounds  = { x, y, x + labelWidth + dropWidth, y + rowHeight };
    int   applyGap   = 0;
    int   applyWidth = 0;
    RECT  dropB      = {};
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
    applyGap = scaler.Px (8);
    applyWidth = scaler.Px (88);
    dropB = m_themeDropdown.Bounds();

    dropB.right = dropB.left + dropWidth;
    m_themeDropdown.SetBounds (dropB);

    m_applyNowButton.Layout (MakeRect ((int) dropB.right + applyGap, (int) dropB.top,
                                       applyWidth, (int) (dropB.bottom - dropB.top)));
    m_applyNowButton.SetDpi (dpi);

    // Desk-scene opt-in row directly under the theme row, spanning the
    // label + dropdown + button width so the caption never truncates.
    {
        RECT  cbRect = MakeRect (x, y + rowHeight + rowGap,
                                 labelWidth + dropWidth + applyGap + applyWidth,
                                 rowHeight);

        m_monitorFrameCheckbox.Layout (cbRect, scaler);
        m_monitorFrameCheckbox.SetDpi (dpi);
    }

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
//  Draws the controls and the live theme preview.
//
//  The dropdown is painted in TWO parts around the preview -- PaintBase before,
//  PaintMenu last -- so an open dropdown appears above the preview window
//  rather than behind it. Painting it as one call would put the menu underneath.
//
//  The preview tracks the dropdown's HIGHLIGHTED item while it is open, not
//  the committed selection, so both mouse hover and arrow-key navigation
//  update the mock window immediately. That is what makes the picker browsable
//  -- a user sees each theme before choosing it -- and it falls back to the
//  committed value once the dropdown closes.
//
//  The interface references are downcast to the concrete Dxui renderers
//  because the preview needs their actual surface to blit a mock chrome and
//  framebuffer. That is safe here: the host always paints through those
//  concrete types.
//
//  The preview drives are initialized lazily and reused, so the mock chrome
//  has real drive widgets to render without building them on every frame.
//
//  A degenerate preview rect is skipped, so a settings window too small to
//  show the preview simply omits it.
//
////////////////////////////////////////////////////////////////////////////////

void ThemePage::Paint (IDxuiPainter & painterIf, IDxuiTextRenderer & textIf, const IDxuiTheme & theme)
{
    static NullDriveCommandSink  s_kNullSink;
    int                          previewIndex = 0;



    // The host always paints through the concrete Dxui renderers; the
    // theme-preview window (mock chrome + framebuffer blit) needs their
    // concrete surface, so recover them from the interface references.
    DxuiPainter       & painter = static_cast<DxuiPainter &> (painterIf);
    DxuiTextRenderer  & text    = static_cast<DxuiTextRenderer &> (textIf);


    m_themeDropdown.SetTheme    (&theme);

    m_themeLabel.Paint            (painter, text);
    m_themeDropdown.PaintBase     (painter, text);
    m_applyNowButton.Paint        (painter, text, theme);
    m_monitorFrameCheckbox.Paint  (painter, text, theme);

    // Live preview tracks the dropdown's effective hovered/highlighted
    // item while open (so mouse hover and arrow-key nav both update
    // the mock window immediately), and falls back to the committed
    // selection when closed. Matches the monitor dropdown live channel.
    previewIndex = m_activeIndex;

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
        bool        hasDisk = false;

        if (!m_previewDrivesInitialized)
        {
            m_previewDrives[0].Initialize (6, 0, &s_kNullSink);
            m_previewDrives[1].Initialize (6, 1, &s_kNullSink);
            m_previewDrivesInitialized = true;
        }

        hasDisk = m_hasDiskSource ? m_hasDiskSource() : true;

        PaintPreviewWindow (painter, text, m_previewRect, preview, hasDisk, m_framebufferSource, m_mountedPathSource, m_writeProtectSource, m_previewDrives, m_previewJoystickButton);
    }

    m_themeDropdown.PaintMenu   (painter, text);
}
