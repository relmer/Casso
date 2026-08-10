#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"


class DxuiButton;
class DxuiCheckbox;
class DxuiDropdown;
class DxuiLabel;
class DxuiListView;
class DxuiTextInput;





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDiskBodyPanel
//
//  Dxui content panel for the create-disk dialog: the current-folder path
//  label on top, the folder listing filling the middle, then three fixed
//  strips along the bottom -- Format / Contents options, the Bootable
//  toggle with its download affordance and hint, and the "Name:" label +
//  text input. Lays out in physical pixels (the hosting dialog passes a px
//  content rect) so the fixed strip heights scale with DPI. Does not own
//  any child -- Init only wires them into the panel tree.
//
//  The list view expects widget-LOCAL mouse coordinates while the other
//  children hit-test absolute client points, so OnMouse serves the
//  dropdowns first (an open menu must see every click), translates for the
//  list, and falls through to the remaining strip widgets.
//
////////////////////////////////////////////////////////////////////////////////

class CreateDiskBodyPanel : public DxuiPanel
{
public:
    //  Every child the dialog composes into this panel, none owned here.
    struct Children
    {
        DxuiLabel     * pathLabel     = nullptr;
        DxuiListView  * list          = nullptr;
        DxuiLabel     * formatLabel   = nullptr;
        DxuiDropdown  * format        = nullptr;
        DxuiLabel     * contentsLabel = nullptr;
        DxuiDropdown  * contents      = nullptr;
        DxuiCheckbox  * bootable      = nullptr;
        DxuiButton    * download      = nullptr;
        DxuiLabel     * bootHint      = nullptr;
        DxuiLabel     * nameLabel     = nullptr;
        DxuiTextInput * nameInput     = nullptr;
    };

    void  Init (const Children & children);

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

    //  Re-runs the last Layout so visibility changes (the download button
    //  appearing / vanishing) re-flow the bootable strip immediately.
    void  Relayout ();

    //  Reports the interactive child a mouse press landed on, so the
    //  hosting dialog can move keyboard focus there (click-to-focus).
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn)
    {
        m_onChildPressed = std::move (fn);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override;

    LPCWSTR  CursorForPoint (POINT clientPx) const override;

private:
    static constexpr int  kPathHeightDip     = 22;
    static constexpr int  kPathGapDip        = 6;
    static constexpr int  kOptionsRowDip     = 30;
    static constexpr int  kOptionsGapDip     = 8;
    static constexpr int  kOptionLabelPadDip = 6;
    static constexpr int  kFormatLabelDip    = 85;    // "Image type:"
    static constexpr int  kFormatDropDip     = 110;
    static constexpr int  kContentsLabelDip  = 60;    // "Format:"
    static constexpr int  kContentsDropDip   = 150;
    static constexpr int  kBootRowDip        = 30;
    static constexpr int  kBootCheckDip      = 110;
    static constexpr int  kBootButtonDip     = 170;
    static constexpr int  kNameRowDip        = 30;
    static constexpr int  kNameGapDip        = 8;
    static constexpr int  kNameLabelDip      = 56;
    static constexpr int  kNameLabelPadDip   = 6;

    Children  m_kids;

    std::function<void (IDxuiControl *)>  m_onChildPressed;
    DxuiDpiScaler                         m_lastScaler;
};
