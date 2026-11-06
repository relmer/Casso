#pragma once

#include "Pch.h"
#include "DxuiStripFlyout.h"
#include "IDxuiImageStripSource.h"



class DxuiDpiScaler;
class DxuiHwndSource;
class DxuiImageStrip;
class IDxuiTextRenderer;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripMarks
//
//  An image strip's marks on a strip lying down: a thin line across the
//  pictures at each mark's offset, and a badge on the pictures' bottom edge
//  with the mark's glyph or branch and its count. Marks closer together than
//  a badge's width share one badge. A mark with rows opens the flyout once
//  the pointer rests on its badge.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiStripMarks
{
public:
    //  A mark's badge as the last paint placed it: its rectangle, where its
    //  line crosses the strip, its tip (every mark merged into it, one to a
    //  line), the source's key for its first mark, and whether it opens rows.
    struct Badge
    {
        RECT          rect    = {};
        float         along   = 0.0f;
        std::wstring  tip;
        uint64_t      id      = 0;
        bool          hasRows = false;
    };

    void  Paint          (IDxuiImageStripSource & source, const DxuiImageStrip & strip, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  ClearBadges    () { m_badges.clear(); }
    void  Track          (IDxuiImageStripSource * source, DxuiHwndSource * popupHost, int x, int y);
    void  Untrack        (IDxuiImageStripSource * source);
    bool  TryGetBadgeAt  (int x, int y, size_t & outIndex) const;

    const std::vector<Badge> &  GetBadges () const { return m_badges; }
    DxuiStripFlyout          &  GetFlyout ()       { return m_flyout; }
    const DxuiStripFlyout    &  GetFlyout () const { return m_flyout; }

private:
    static constexpr float    kLineDip   = 1.0f;    // a mark's line across the pictures
    static constexpr float    kBadgeDip  = 14.0f;   // a mark's badge, square for a glyph alone
    static constexpr float    kGlyphDip  = 10.0f;   // the glyph in it
    static constexpr wchar_t  kGlyphFace[] = L"Segoe MDL2 Assets";

    void  Join           (const DxuiImageStrip & strip, float badgePx);
    void  PaintBranchArt (IDxuiTextRenderer & text, float x, float y, float sizePx, uint32_t ink);

    std::vector<DxuiStripMark>  m_marks;                // the source's marks, read each paint
    std::vector<Badge>          m_badges;               // where the last paint placed them
    std::vector<uint32_t>       m_branchArt;            // a branch mark's pixels, redrawn when the size or ink changes
    int                         m_branchArtSize = 0;
    uint32_t                    m_branchArtInk  = 0;
    DxuiStripFlyout             m_flyout;
};
