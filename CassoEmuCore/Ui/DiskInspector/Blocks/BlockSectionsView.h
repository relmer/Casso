#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockLevels.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockSectionsView
//
//  A level of block mode drawn as sections: each heading, then its rows
//  with the label in one column and the value in the next. A row stored in
//  blocks has its value in the theme's link color, and a click on it steps
//  to its block. The wheel scrolls the sections.
//
////////////////////////////////////////////////////////////////////////////////

class BlockSectionsView : public IDxuiControl
{
public:
    static constexpr float  kRowDip     = 22.0f;
    static constexpr float  kFontDip    = 12.0f;
    static constexpr float  kHeadingDip = 13.0f;
    static constexpr float  kLabelDip   = 190.0f;
    static constexpr float  kPadDip     = 8.0f;
    static constexpr int    kWheelRows  = 3;

    using FollowFn = std::function<void (uint32_t block)>;

    void  SetSections (vector<BlockSection> sections);
    void  SetOnFollow (FollowFn fn) { m_onFollow = std::move (fn); }

    const vector<BlockSection> &  GetSections () const { return m_sections; }

    //  The row's label and value at a point, for a test; false off the rows.
    bool  TryGetRowAt (POINT pointPx, BlockField & outRow) const;

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override { SetBounds (boundsPx); m_scaler = scaler; }
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    std::wstring        GetAccessibleName () const override { return L"Details"; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::ListView; }

private:
    //  A line: a section's heading, or one of its rows.
    struct Line
    {
        const BlockSection  * section = nullptr;
        const BlockField    * row     = nullptr;
    };

    int  FindLineAt    (POINT pointPx) const;
    int  GetLinesShown () const;

    vector<BlockSection>  m_sections;
    vector<Line>          m_lines;
    DxuiDpiScaler         m_scaler;
    FollowFn              m_onFollow;
    int                   m_top   = 0;
    int                   m_hover = -1;
};
