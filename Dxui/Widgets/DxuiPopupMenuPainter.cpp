#include "Pch.h"

#include "Widgets/DxuiPopupMenuPainter.h"
#include "Widgets/DxuiCheckbox.h"
#include "Core/DxuiIconImage.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"
#include "DxuiPopupMenu.h"
#include "DxuiMenuBar.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiSystemSettings.h"
#include "Render/DxuiStroke.h"
#include "Render/DxuiVectorIcon.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPainter::DxuiPopupMenuPainter
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuPainter::DxuiPopupMenuPainter (DxuiPopupMenu & menu)
    : m_menu (menu)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPainter::ResolvePalette
//
//  The application's override when one was supplied, else the theme.
//  Disabled text is always the theme's; no override carries one.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPopupMenuPalette DxuiPopupMenuPainter::ResolvePalette() const
{
    DxuiPopupMenuPalette  pal;



    pal.bg       = m_menu.m_colorsSet ? m_menu.m_colors.bg      : m_menu.m_theme->BackgroundElevated();
    pal.hover    = m_menu.m_colorsSet ? m_menu.m_colors.hover   : m_menu.m_theme->MenuHover();
    pal.text     = m_menu.m_colorsSet ? m_menu.m_colors.text    : m_menu.m_theme->Foreground();
    pal.accel    = m_menu.m_colorsSet ? m_menu.m_colors.accel   : m_menu.m_theme->ForegroundMuted();
    pal.border   = m_menu.m_colorsSet ? m_menu.m_colors.border  : m_menu.m_theme->Border();
    pal.divider  = m_menu.m_colorsSet ? m_menu.m_colors.divider : m_menu.m_theme->MenuSeparator();
    pal.disabled = m_menu.m_theme->ForegroundDisabled();

    return pal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPainter::PaintBody
//
//  Shared menu renderer. Background, border, then each row at the supplied
//  origin. The in-window Paint passes the panel-absolute bounds; the popup
//  render hook passes (0,0).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenuPainter::PaintBody (IDxuiPainter & painter, IDxuiTextRenderer & text, int originLeft, int originTop) const
{
    DxuiPopupMenuPalette  pal;
    float                 fontDip = m_menu.m_metrics.fontPx;
    float                 left    = (float) originLeft;
    float                 top     = (float) originTop;
    float                 width   = (float) (m_menu.m_boundsDip.right  - m_menu.m_boundsDip.left);
    float                 height  = (float) (m_menu.m_boundsDip.bottom - m_menu.m_boundsDip.top);



    if (m_menu.m_theme == nullptr)
    {
        return;
    }

    pal = ResolvePalette();

    // Rounded at the overlay radius. A hosted menu's card is also drawn by
    // its popup host at this same radius and rect; a square fill here would
    // paint the host's rounded corners back to square.
    //
    //  The border is a ring: the card filled in its color, then the inside
    //  in the background. A dip of border lands on whole pixels as Explorer
    //  lays it out: rounded down on the left, up on the other three sides,
    //  so 1 px and 2 px at 150%. A command bar's drop-down has a dark border,
    //  the background at 60%, where a context menu's is light.
    {
        float     radius  = m_menu.m_scaler.ToPxf (DxuiTheme::kOverlayCornerRadiusDip);
        float     edgePx  = m_menu.m_scaler.ToPxf ((float) DxuiPopupMenu::kBorderDip);
        float     leftPx  = std::floor (edgePx);
        float     otherPx = std::ceil  (edgePx);
        uint32_t  border  = m_menu.m_compactRows ? DxuiColor::Mix (pal.bg, 0xFF000000u, DxuiPopupMenu::s_kCompactBorderDarken) : pal.border;

        //  Over acrylic the card is translucent, so the border is drawn as a
        //  ring over it rather than filled beneath it.
        if (m_menu.m_acrylic)
        {
            float  opacity = DxuiColor::ComputeRelativeLuminance (pal.bg) < 0.5f ? DxuiPopupMenu::s_kAcrylicDarkOpacity : DxuiPopupMenu::s_kAcrylicLightOpacity;

            painter.FillRoundedRect    (left, top, width, height, radius, DxuiColor::ScaleAlpha (pal.bg, opacity));
            painter.OutlineRoundedRect (left, top, width, height, radius, otherPx, border);
        }
        else
        {
            painter.FillRoundedRect (left, top, width, height, radius, border);
            painter.FillRoundedRect (left + leftPx, top + otherPx, width - leftPx - otherPx, height - otherPx * 2.0f, radius - otherPx, pal.bg);
        }
    }

    //  Only the rows in view. Scrolling moves by whole rows, so none is cut.
    for (int i = m_menu.m_layout.m_scrollRow; i < (int) m_menu.m_rows.size(); i++)
    {
        if (m_menu.m_layout.m_viewportPx > 0 && m_menu.m_layout.GetRowTopPx (i + 1) - m_menu.m_layout.GetScrollPx() > m_menu.m_layout.m_viewportPx)
        {
            break;
        }

        PaintRow (painter, text, pal, i, left, top, width, fontDip);
    }

    //  A list longer than it shows says so with a thumb down its right edge,
    //  sized and placed by how much of the list is in view.
    if (m_menu.m_layout.m_viewportPx > 0)
    {
        float  track    = (float) m_menu.m_layout.m_viewportPx;
        float  content  = (float) m_menu.m_layout.GetContentHeightPx();
        float  thumbW   = m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kThumbWidthDip);
        float  thumbH   = (std::max) (thumbW * 4.0f, track * track / content);
        int    maxRow   = m_menu.m_layout.GetMaxScrollRow();
        float  fraction = (maxRow > 0) ? (float) m_menu.m_layout.m_scrollRow / (float) maxRow : 0.0f;

        painter.FillRoundedRect (left + width - thumbW * 2.0f, top + (track - thumbH) * fraction,
                                 thumbW, thumbH, thumbW * 0.5f, pal.disabled);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPainter::PaintRow
//
//  One row, reading everything from the command AT PAINT TIME: label,
//  checked, enabled, accelerator. A separator is a divider line inset from
//  both edges at the row's vertical midpoint.
//
//  The label is clipped to the label column and the accelerator is drawn LEFT
//  aligned at the accelerator column's origin, so accelerators of unequal
//  length share a left edge and a label that outgrows its column is elided
//  rather than painted over the column beside it. A submenu arrow is the one
//  thing right aligned, against the right pad reserved for it.
//
//  Text sits at the row's vertical center, computed from the metrics' line
//  height so a row that grows with the system menu font stays centered.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenuPainter::PaintRow (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const DxuiPopupMenuPalette      & pal,
    int                  index,
    float                left,
    float                top,
    float                width,
    float                fontDip) const
{
    HRESULT                    hr        = S_OK;
    const DxuiPopupMenuItem &  row       = m_menu.m_rows[(size_t) index];
    int                        rowTopPx  = m_menu.m_layout.GetRowTopPx (index);
    int                        rowH      = m_menu.m_layout.GetRowHeightPx (index);
    int                        pad       = m_menu.m_metrics.leftPadPx;
    int                        gutter    = m_menu.m_metrics.checkGutterPx;
    int                        inset     = m_menu.m_metrics.separatorInsetPx;
    int                        labelLeft = m_menu.m_layout.m_labelLeftPx;
    int                        padTop    = (rowH - m_menu.m_metrics.lineHeightPx) / 2;
    float                      y         = top + (float) (rowTopPx - m_menu.m_layout.GetScrollPx());
    float                      labelW    = (float) (m_menu.m_layout.m_accelLeftPx - labelLeft);
    float                      accelW    = width - (float) m_menu.m_layout.m_accelLeftPx - (float) m_menu.m_metrics.rightPadPx;
    std::wstring               stripped;
    int                        mnIdx     = -1;
    wchar_t                    mnCh      = 0;
    bool                       enabled   = false;
    uint32_t                   labelArgb = 0;
    uint32_t                   accelArgb = 0;



    if (row.kind == DxuiPopupMenuItem::Kind::Separator)
    {
        //  A dip, in whole pixels: two at 150%, as Explorer draws it.
        float  thick = std::ceil (m_menu.m_scaler.ToPxf (1.0f));

        painter.FillRect (left + (float) inset,
                          std::ceil (y + ((float) rowH - thick) * 0.5f),
                          width - (float) inset - (float) inset,
                          thick,
                          pal.divider);
        return;
    }

    if (row.kind == DxuiPopupMenuItem::Kind::IconRow)
    {
        PaintIconRow (painter, text, pal, index, left, y);
        return;
    }

    if (row.command == nullptr)
    {
        return;
    }

    //  A header is a title, not a choice: secondary ink, no hover, no check,
    //  and no underline cue, since nothing under it answers a keystroke.
    if (row.kind == DxuiPopupMenuItem::Kind::Header)
    {
        hr = text.DrawString (row.command->GetLabelText().c_str(),
                              left + (float) labelLeft,
                              y + (float) padTop,
                              labelW,
                              (float) rowH,
                              pal.accel,
                              fontDip,
                              DxuiTheme::GetUiFace());
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    enabled   = row.command->IsEnabled();
    labelArgb = enabled ? pal.text  : pal.disabled;
    accelArgb = enabled ? pal.accel : pal.disabled;

    DxuiPopupMenu::ParseMnemonic (row.command->GetLabelText(), stripped, mnIdx, mnCh);

    if (index == m_menu.m_hover)
    {
        float  insetX = m_menu.m_scaler.ToPxf ((float) DxuiPopupMenu::kHoverInsetXDip);
        float  insetY = m_menu.m_scaler.ToPxf ((float) DxuiPopupMenu::kHoverInsetYDip);

        painter.FillRoundedRect (left  + insetX,
                                 y     + insetY,
                                 width - insetX - insetX,
                                 (float) rowH - insetY - insetY,
                                 m_menu.m_scaler.ToPxf (DxuiPopupMenu::kHoverRadiusDip),
                                 pal.hover);
    }

    hr = text.DrawString (stripped.c_str(),
                          left + (float) labelLeft,
                          y + (float) padTop,
                          labelW,
                          (float) rowH,
                          labelArgb,
                          fontDip,
                          DxuiTheme::GetUiFace());
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  A check box row draws its box in the gutter, checked or empty.
    if (row.command->checkBox)
    {
        const IDxuiTheme &  theme = *m_menu.m_theme;
        float               size  = std::floor (m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kCheckBoxDip));
        uint32_t            fill  = theme.Accent();

        DxuiCheckbox::PaintBox (painter, text,
                                std::round (left + (float) pad + ((float) gutter - size) * 0.5f),
                                std::round (y + ((float) rowH - size) * 0.5f),
                                size, std::ceil (m_menu.m_scaler.ToPxf (1.0f)), row.command->IsChecked(),
                                pal.bg, (labelArgb & 0x00FFFFFFu) | 0x99000000u, fill, DxuiTheme::GetContrastingText (fill));
    }
    else if (row.command->IsChecked() && row.command->radio)
    {
        //  Explorer's radio mark is a dot, not the bullet glyph: 7 px across
        //  at 150%, on the row's middle, in the secondary text color.
        painter.FillCircle (left + m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kRadioCenterXDip),
                            std::floor (y + (float) rowH * 0.5f),
                            m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kRadioRadiusDip),
                            (labelArgb & 0x00FFFFFFu) | ((uint32_t) (((labelArgb >> 24) * DxuiPopupMenu::s_kRadioAlpha) / 255u) << 24));
    }
    else if (row.command->IsChecked())
    {
        hr = text.DrawString (s_kpszCheckMark,
                              left + (float) pad,
                              y + (float) padTop,
                              (float) gutter,
                              (float) rowH,
                              labelArgb,
                              fontDip,
                              DxuiTheme::GetUiFace(),
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Top);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  An SVG, when the host has one, is drawn in place of the vector icon.
    if (row.command->vectorIcon != nullptr && row.command->menuSvg == nullptr)
    {
        float  iconDip  = (float) m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kRowIconDip);
        float  iconLeft = m_menu.m_layout.m_hasGutter ? left + (float) (pad + gutter + m_menu.m_metrics.gutterGapPx + m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kGutterIconShiftDip)) : left + (float) m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kRowIconLeftDip);

        hr = text.FillVectorIcon (*row.command->vectorIcon, iconLeft, y + ((float) rowH - iconDip) * 0.5f, iconDip, labelArgb, labelArgb);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else if (row.command->menuGlyph != nullptr || row.command->menuSvg != nullptr || row.command->vectorIcon != nullptr || row.command->menuImage != nullptr)
    {
        float  iconDip  = (float) m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kRowIconDip);
        float  iconLeft = m_menu.m_layout.m_hasGutter ? left + (float) (pad + gutter + m_menu.m_metrics.gutterGapPx + m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kGutterIconShiftDip)) : left + (float) m_menu.m_scaler.ToPx (DxuiPopupMenu::s_kRowIconLeftDip);
        float  alpha    = text.GetGlobalAlpha();

        hr = E_NOTIMPL;

        if (row.command->menuImage != nullptr && !row.command->menuImage->bgraPremul.empty())
        {
            const DxuiIconImage &  image = *row.command->menuImage;

            text.SetGlobalAlpha (enabled ? alpha : alpha * DxuiPopupMenu::s_kDisabledIconAlpha);
            hr = text.DrawIconBitmap (image.bgraPremul.data(), image.width, image.height,
                                      iconLeft, std::floor (y + ((float) rowH - iconDip) * 0.5f), iconDip, iconDip);
            text.SetGlobalAlpha (alpha);
        }

        if (FAILED (hr) && row.command->menuSvg != nullptr)
        {
            text.SetGlobalAlpha (enabled ? alpha : alpha * DxuiPopupMenu::s_kDisabledIconAlpha);
            //  On a whole pixel, as Explorer's are, where an odd row height
            //  would leave its strokes smeared across two.
            hr = text.DrawSvgIcon (*row.command->menuSvg, iconLeft, std::floor (y + ((float) rowH - iconDip) * 0.5f), iconDip);
            text.SetGlobalAlpha (alpha);
        }

        if (FAILED (hr) && row.command->vectorIcon != nullptr)
        {
            hr = text.FillVectorIcon (*row.command->vectorIcon, iconLeft, y + ((float) rowH - iconDip) * 0.5f, iconDip, labelArgb, labelArgb);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
        else if (FAILED (hr) && row.command->menuGlyph != nullptr)
        {
            hr = text.DrawString (row.command->menuGlyph, iconLeft, y, iconDip, (float) rowH, labelArgb, iconDip, DxuiPopupMenu::s_kIconFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (m_menu.m_showCues && mnIdx >= 0 && !stripped.empty() && enabled)
    {
        PaintUnderline (painter, text, stripped, mnIdx,
                        left + (float) labelLeft, y + (float) padTop, fontDip, labelArgb);
    }

    //  Explorer ends a submenu row in a thin chevron, not a filled triangle:
    //  two strokes meeting at a point, at the right of the accelerator column.
    if (row.kind == DxuiPopupMenuItem::Kind::Submenu)
    {
        float  right = left + (float) m_menu.m_layout.m_accelLeftPx + accelW + (float) m_menu.m_metrics.rightPadPx - (float) pad;
        float  half  = m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kSubmenuChevronHalfDip);
        float  depth = m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kSubmenuChevronDepthDip);
        float  thick = (std::max) (1.0f, m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kSubmenuChevronStrokeDip));
        float  tipX  = right - m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kSubmenuChevronInsetDip);
        float  midY  = y + (float) rowH * 0.5f;

        //  Centered on the row itself, not on the text line's offset within it,
        //  and drawn as filled quads, whose edges are smoothed where a stroked
        //  line's are not.
        DxuiStroke::Segment (painter, tipX - depth, midY - half, tipX, midY, thick, accelArgb);
        DxuiStroke::Segment (painter, tipX - depth, midY + half, tipX, midY, thick, accelArgb);
    }
    else if (!row.command->accelerator.empty())
    {
        hr = text.DrawString (row.command->accelerator.c_str(),
                              left + (float) m_menu.m_layout.m_accelLeftPx,
                              y + (float) padTop,
                              accelW,
                              (float) rowH,
                              accelArgb,
                              fontDip,
                              DxuiTheme::GetUiFace(),
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Top);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPainter::PaintIconRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenuPainter::PaintIconRow (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiPopupMenuPalette & pal,
                                  int index, float left, float top) const
{
    const DxuiPopupMenuItem &  row    = m_menu.m_rows[(size_t) index];
    float                      size   = (float) m_menu.m_layout.GetIconButtonPx();
    float                      rowH   = (float) m_menu.m_layout.GetRowHeightPx (index);
    float                      rule   = (float) m_menu.m_layout.GetIconDividerPx();
    float                      insetX = m_menu.m_scaler.ToPxf ((float) DxuiPopupMenu::kHoverInsetYDip);
    float                      x      = left + (float) m_menu.m_layout.GetIconRowLeftPx();
    HRESULT                    hr     = S_OK;
    size_t                     i      = 0;
    std::wstring               label;



    for (i = 0; i < row.children.size(); i++, x += size + rule)
    {
        const DxuiCommand  * cmd     = row.children[i].command.get();
        bool                 enabled = cmd != nullptr && cmd->IsEnabled();

        if (cmd == nullptr || cmd->glyph == nullptr)
        {
            continue;
        }

        if (index == m_menu.m_hover && (int) i == m_menu.m_iconHover && enabled)
        {
            painter.FillRoundedRect (x + insetX, top + insetX, size - insetX * 2.0f, rowH - insetX * 2.0f,
                                     m_menu.m_scaler.ToPxf (DxuiPopupMenu::kHoverRadiusDip), pal.hover);
        }

        //  Explorer's: the icon over a small label, and a thin rule between
        //  one button and the next.
        if (i > 0)
        {
            painter.FillRect (x - rule, std::floor (top + m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconRuleTopDip)), rule, m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconRuleDip), pal.divider);
        }

        hr = E_NOTIMPL;

        if (cmd->menuSvg != nullptr)
        {
            float  iconPx = m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconGlyphDip);
            float  alpha  = text.GetGlobalAlpha();

            text.SetGlobalAlpha (enabled ? alpha : alpha * DxuiPopupMenu::s_kDisabledIconAlpha);
            hr = text.DrawSvgIcon (*cmd->menuSvg, std::floor (x + (size - iconPx) * 0.5f), std::floor (top + m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconTopDip)), iconPx);
            text.SetGlobalAlpha (alpha);
        }

        if (FAILED (hr))
        {
            hr = text.DrawString (cmd->glyph, x, top + m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconTopDip), size, m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconGlyphDip),
                                  enabled ? pal.text : pal.disabled,
                                  m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconGlyphDip),
                                  DxuiPopupMenu::s_kIconFace,
                                  DxuiTextHAlign::Center,
                                  DxuiTextVAlign::Center);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        //  The row's own label, less its mnemonic mark and the dots that say a
        //  dialog follows: a button under an icon is a single word.
        label = cmd->GetLabelText();
        label.erase (std::remove (label.begin(), label.end(), L'&'), label.end());

        while (!label.empty() && (label.back() == L'.' || label.back() == L'\x2026'))
        {
            label.pop_back();
        }

        hr = text.DrawString (label.c_str(), x, top + m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconLabelTopDip), size, m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconLabelBoxDip),
                              enabled ? pal.text : pal.disabled,
                              m_menu.m_scaler.ToPxf (DxuiPopupMenu::s_kIconLabelDip),
                              DxuiTheme::GetUiFace(),
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPainter::PaintUnderline
//
//  The mnemonic cue: a one-pixel rule under the marked letter. Its left edge
//  is the width of the text before the letter and its length is the width
//  of the prefix-plus-letter less the prefix, both measured rather than
//  estimated, so it sits under the glyph the parser marked.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPopupMenuPainter::PaintUnderline (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const std::wstring  & stripped,
    int                   mnIdx,
    float                 labelX,
    float                 labelY,
    float                 fontDip,
    uint32_t              ink) const
{
    HRESULT       hr       = S_OK;
    float         prefixW  = 0.0f;
    float         withChW  = 0.0f;
    float         fullH    = 0.0f;
    float         ignoredH = 0.0f;
    std::wstring  prefix   = stripped.substr (0, (size_t) mnIdx);
    std::wstring  prefixCh = stripped.substr (0, (size_t) mnIdx + 1);



    if (!prefix.empty())
    {
        hr = text.MeasureString (prefix.c_str(), fontDip, DxuiTheme::GetUiFace(), prefixW, fullH);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        std::wstring  oneCh (1, stripped[(size_t) mnIdx]);

        hr = text.MeasureString (oneCh.c_str(), fontDip, DxuiTheme::GetUiFace(), prefixW, fullH);
        IGNORE_RETURN_VALUE (hr, S_OK);
        prefixW = 0.0f;
    }

    hr = text.MeasureString (prefixCh.c_str(), fontDip, DxuiTheme::GetUiFace(), withChW, ignoredH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.FillRect (labelX + prefixW, labelY + fullH, withChW - prefixW, DxuiPopupMenu::kUnderlineThicknessDip, ink);
}
