#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"


class DxuiDropdown;
class DxuiLabel;
class DxuiListView;
class DxuiTextInput;





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDiskBodyPanel
//
//  Dxui content panel for the create-disk dialog: the current-folder path
//  label on top, the folder listing filling the middle, then a Format /
//  Contents options strip and a "Name:" label + text input strip along the
//  bottom. Lays out in physical pixels (the hosting dialog passes a px
//  content rect) so the fixed strip heights scale with DPI. Does not own
//  any child -- Init only wires them into the panel tree.
//
//  The list view expects widget-LOCAL mouse coordinates while the other
//  children hit-test absolute client points, so OnMouse serves the
//  dropdowns first (an open menu must see every click), translates for the
//  list, and falls through to the text input.
//
////////////////////////////////////////////////////////////////////////////////

class CreateDiskBodyPanel : public DxuiPanel
{
public:
    void  Init (DxuiLabel * pathLabel, DxuiListView * list,
                DxuiLabel * formatLabel, DxuiDropdown * format,
                DxuiLabel * contentsLabel, DxuiDropdown * contents,
                DxuiLabel * nameLabel, DxuiTextInput * nameInput);

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    bool  OnMouse (const DxuiMouseEvent & ev) override;

    LPCWSTR  CursorForPoint (POINT clientPx) const override;

private:
    static constexpr int  kPathHeightDip     = 22;
    static constexpr int  kPathGapDip        = 6;
    static constexpr int  kOptionsRowDip     = 30;
    static constexpr int  kOptionsGapDip     = 8;
    static constexpr int  kOptionLabelPadDip = 6;
    static constexpr int  kFormatLabelDip    = 56;
    static constexpr int  kFormatDropDip     = 110;
    static constexpr int  kContentsLabelDip  = 72;
    static constexpr int  kContentsDropDip   = 150;
    static constexpr int  kNameRowDip        = 30;
    static constexpr int  kNameGapDip        = 8;
    static constexpr int  kNameLabelDip      = 56;
    static constexpr int  kNameLabelPadDip   = 6;

    DxuiLabel     * m_pathLabel     = nullptr;
    DxuiListView  * m_list          = nullptr;
    DxuiLabel     * m_formatLabel   = nullptr;
    DxuiDropdown  * m_format        = nullptr;
    DxuiLabel     * m_contentsLabel = nullptr;
    DxuiDropdown  * m_contents      = nullptr;
    DxuiLabel     * m_nameLabel     = nullptr;
    DxuiTextInput * m_nameInput     = nullptr;
};
