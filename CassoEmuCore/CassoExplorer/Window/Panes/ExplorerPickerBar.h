#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerBar
//
//  A picker's bar along the bottom of the window: the receiving drive and
//  the images listed for it, the filter entries as a choice when there are
//  several, the create command when the request has one, and the confirm
//  and cancel buttons. In a window that is not a picker it has no controls
//  and its band has no height.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerBar
{
public:
    explicit ExplorerPickerBar (ExplorerContext & context);

    static constexpr int  kBandDip       = 52;
    static constexpr int  kButtonDip     = 32;
    static constexpr int  kEdgePadDip    = 12;
    static constexpr int  kGapDip        = 8;
    static constexpr int  kFilterMaxDip  = 320;
    static constexpr int  kNameLabelDip  = 84;
    static constexpr int  kNameMinDip    = 160;

    //  The band's height in the window: kBandDip in a picker, 0 otherwise.
    int   GetBandDip () const;

    void  Create     ();
    void  Layout     (const RECT & band);

    //  The confirm button follows the selection, and the filter button the
    //  active entry.
    void  Update     ();

    bool  IsConfirmEnabled () const;

    //  The save form's file name box: its text, and the mouse, keys and
    //  focus the window routes to it. Enter in the box confirms.
    std::wstring  GetNameText        () const;
    void          SetNameText        (const std::wstring & text);
    bool          HandleNameMouse    (const DxuiMouseEvent & ev);
    bool          HandleNameKey      (const DxuiKeyEvent & ev);
    void          HandleFocusChanged (bool isFocused);
    bool          IsCaretOn          () const;

private:
    void  ShowFilterMenu ();

    ExplorerContext  & m_context;
    DxuiLabel        * m_header           = nullptr;
    DxuiLabel        * m_nameLabel        = nullptr;
    DxuiTextInput    * m_name             = nullptr;
    DxuiButton       * m_filter           = nullptr;
    DxuiButton       * m_create           = nullptr;
    DxuiButton       * m_confirm          = nullptr;
    DxuiButton       * m_cancel           = nullptr;
    std::wstring       m_copiedFrom;                 // the selected file whose name the box was last given
    bool               m_isNameConfigured = false;
};
