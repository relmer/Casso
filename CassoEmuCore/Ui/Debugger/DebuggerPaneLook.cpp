#include "Pch.h"

#include "Ui/Debugger/DebuggerPaneLook.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Ui/Debugger/Panes/DiskHeadView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetColorPalette
//
//  Every color the legend shows, as the panes draw them in the theme in
//  force.
//
////////////////////////////////////////////////////////////////////////////////

ColorLegend::Palette DebuggerPaneLook::GetColorPalette() const
{
    ColorLegend::Palette  palette;



    palette.text       = GetTextColors();
    palette.background = (m_theme != nullptr) ? m_theme->ContentBackground()  : 0xFF000000;
    palette.muted      = (m_theme != nullptr) ? m_theme->ForegroundMuted()    : 0xFF808080;
    palette.disabled   = (m_theme != nullptr) ? m_theme->ForegroundDisabled() : 0xFF808080;
    palette.accent     = (m_theme != nullptr) ? m_theme->Accent()             : 0xFF3C8CE6;
    palette.flash      = (m_theme != nullptr) ? DiskHeadView::GetFlashColor (*m_theme) : 0xFFFFFFFF;
    palette.pcMarker   = GetPcMarkerArgb();
    palette.breakpoint = GetBreakpointArgb();
    palette.meterEmpty = DebuggerStatusText::kEmptyArgb;
    palette.meterFull  = DebuggerStatusText::kFullArgb;

    return palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::IsDark
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerPaneLook::IsDark() const
{
    uint32_t  bg    = (m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000;
    uint32_t  luma  = ((bg >> 16) & 0xFF) * 299 + ((bg >> 8) & 0xFF) * 587 + (bg & 0xFF) * 114;



    return luma < 128 * 1000;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetBreakpointArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerPaneLook::GetBreakpointArgb() const
{
    //  A warm red that holds its own against a blue ground, as Visual Studio
    //  Code's breakpoint red does; a pure red goes purple beside blue.
    return IsDark() ? 0xFFF4524D : 0xFFD1242F;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetBreakpointIcon
//
//  The breakpoint's dot, filled when enabled and a ring when not. Rebuilt
//  when the theme changes the color.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerPaneLook::GetBreakpointIcon (bool enabled) const
{
    uint32_t  argb = GetBreakpointArgb();



    if (argb != m_breakpointIconArgb || m_breakpointIcons[0] == nullptr)
    {
        m_breakpointIconArgb = argb;
        m_breakpointIcons[0] = GutterGlyph::MakeDot (argb, false);
        m_breakpointIcons[1] = GutterGlyph::MakeDot (argb, true);
    }

    return m_breakpointIcons[enabled ? 1 : 0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetHoverBreakpointIcon
//
//  The gray dot under the pointer in the breakpoint column, in the theme's
//  muted text color, where a click would set a breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerPaneLook::GetHoverBreakpointIcon() const
{
    uint32_t  argb = (m_theme != nullptr) ? m_theme->ForegroundMuted() : 0xFF808080;



    if (argb != m_hoverBreakpointArgb || m_hoverBreakpointIcon == nullptr)
    {
        m_hoverBreakpointArgb = argb;
        m_hoverBreakpointIcon = GutterGlyph::MakeDot (argb, true);
    }

    return m_hoverBreakpointIcon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetPcIcon
//
//  The PC's arrow in the glyph margin, over the line's dot when it has one,
//  as Visual Studio draws its arrow over a breakpoint. A dot not seen before
//  gets its own image, and a few are kept: the enabled and disabled dots,
//  the gray one under the pointer, and the arrow alone; one more means a
//  dot from an earlier theme, so the old ones are dropped.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerPaneLook::GetPcIcon (const std::shared_ptr<const DxuiIconImage> & dot) const
{
    constexpr size_t                      kMaxPcIcons = 8;
    uint32_t                              argb        = GetPcMarkerArgb();
    std::shared_ptr<const DxuiIconImage>  icon;



    if (argb != m_pcIconArgb)
    {
        m_pcIconArgb = argb;
        m_pcIcons.clear();
    }

    for (const MarkedIcon & each : m_pcIcons)
    {
        icon = (each.dot == dot) ? each.icon : icon;
    }

    if (icon == nullptr)
    {
        if (m_pcIcons.size() >= kMaxPcIcons)
        {
            m_pcIcons.clear();
        }

        icon = GutterGlyph::MakeMarked (dot, GutterGlyph::MakeArrow (argb));

        m_pcIcons.push_back ({ dot, icon });
    }

    return icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetBookmarkMark
//
//  A bookmark's outline in the theme's accent, drawn over the line's dot
//  when it has one. A few are cached, as the PC's are.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerPaneLook::GetBookmarkMark (const std::shared_ptr<const DxuiIconImage> & dot) const
{
    constexpr size_t                      kMaxIcons = 8;
    uint32_t                              argb      = (m_theme != nullptr) ? m_theme->Accent() : 0xFF3C8CE6;
    std::shared_ptr<const DxuiIconImage>  icon;
    std::shared_ptr<const DxuiIconImage>  outline;



    if (argb != m_bookmarkIconArgb)
    {
        m_bookmarkIconArgb = argb;
        m_bookmarkIcons.clear();
    }

    for (const MarkedIcon & each : m_bookmarkIcons)
    {
        icon = (each.dot == dot) ? each.icon : icon;
    }

    if (icon == nullptr)
    {
        if (m_bookmarkIcons.size() >= kMaxIcons)
        {
            m_bookmarkIcons.clear();
        }

        outline = GutterGlyph::MakeBookmark (argb);
        icon    = (dot != nullptr) ? std::shared_ptr<const DxuiIconImage> (GutterGlyph::MakeOver (*dot, *outline)) : outline;

        m_bookmarkIcons.push_back ({ dot, icon });
    }

    return icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetPcMarkerArgb
//
//  Visual Studio's yellow, which reads on either background.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerPaneLook::GetPcMarkerArgb() const
{
    return DisassemblyCells::GetPcMarkerArgb ((m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetPcTabMarkArgb
//
//  The PC's yellow, darkened or lightened until its contrast with the band
//  behind the tabs and with a tab's fill is 3:1, a glyph's minimum; the code
//  views' yellow, picked against their fill, was faint on a light theme's
//  tabs.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerPaneLook::GetPcTabMarkArgb() const
{
    constexpr float  kGlyphContrast = 3.0f;



    if (m_theme == nullptr)
    {
        return GetPcMarkerArgb();
    }

    return DebuggerTextColors::GetReadable (GetPcMarkerArgb(), { m_theme->PaneBand(), m_theme->ContentBackground(), m_theme->Background() }, kGlyphContrast);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetTextColors
//
//  Every text color the panes draw, made readable against the theme's
//  content background, with every color of the theme's own: its ROM, I/O and
//  memory map colors among them.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerTextColors::Set DebuggerPaneLook::GetTextColors() const
{
    if (m_theme != nullptr)
    {
        return m_textColors.GetFor (*m_theme);
    }

    return DebuggerTextColors::Make (0xFF000000, 0xFFFFFFFF, 0xFFC0C0C0, 0u);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook::GetHeatPalette
//
//  Code in the disassembly's instruction color, reads in the annotation
//  green and writes in the changed red, from the theme's cold gray, over the
//  page the panes are drawn on. The PC is outlined in the disassembly's PC
//  yellow and a breakpoint in its breakpoint red, and a symbol tints toward
//  the symbol color; the rest of the overlays take the theme's heat colors.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::Palette DebuggerPaneLook::GetHeatPalette() const
{
    DebuggerTextColors::Set  colors  = GetTextColors();
    HeatMapView::Palette     palette;



    palette.background    = (m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000;
    palette.cold          = colors.heatCold;
    palette.execute       = colors.syntax.mnemonic;
    palette.read          = colors.annotation;
    palette.write         = colors.changed;
    palette.pc            = GetPcMarkerArgb();
    palette.stack         = colors.heatStack;
    palette.breakpoint    = GetBreakpointArgb();
    palette.readWatch     = colors.heatReadWatch;
    palette.writeWatch    = colors.heatWriteWatch;
    palette.symbol        = colors.syntax.symbol;
    palette.selfModifying = colors.heatSelfModifying;
    palette.unwritten     = colors.heatUnwritten;
    palette.changed       = colors.heatChanged;
    palette.found         = (m_theme != nullptr) ? m_theme->Accent() : 0xFFFFFFFF;

    return palette;
}
